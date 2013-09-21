#include "ci-client.h"
#include "ci-config.h"
#include <gio/gio.h>
#include <memory.h>

struct {
    GSocketClient *client;
    GSocketConnection *connection;
    gchar *host;
    guint16 port;
    CIMsgCallback msgcallback;

    gint state;
    GCancellable *cancel;
    guint timer_source_id;
    guint stream_source_id;
} ci_client;

enum {
    CIClientStateUnknown = 0,
    CIClientStateInitialized,
    CIClientStateConnecting,
    CIClientStateConnected,
    CIClientStateDisconnected
};

CINetMsg *ci_client_recv_message(GSocket *socket, gboolean *conn_err);
void ci_client_send_message(GSocketConnection *conn, gchar *data, gsize len);
gboolean ci_client_incoming_data(GSocket *socket, GIOCondition cond, GSocketConnection *conn);
void ci_client_connected_func(GSocketClient *source, GAsyncResult *result, gpointer userdata);
void ci_client_stop_timer(void);
gboolean ci_client_try_connect_func(gpointer userdata);
void ci_client_handle_connection_lost(void);

void ci_client_handle_message(CINetMsg *msg);

void ci_client_init(CIMsgCallback callback)
{
    ci_config_get("hostname", &ci_client.host);
    ci_config_get("port", &ci_client.port);
    ci_client.msgcallback = callback;
    ci_client.client = g_socket_client_new();
    ci_client.connection = NULL;

    ci_client.state = CIClientStateInitialized;

    ci_client_connect();
}

void ci_client_connect(void)
{
    if (ci_client.state == CIClientStateConnecting ||
            ci_client.state == CIClientStateConnected)
        ci_client_stop();

    ci_client.cancel = g_cancellable_new();

    g_socket_client_connect_to_host_async(ci_client.client,
            ci_client.host, ci_client.port, ci_client.cancel,
            (GAsyncReadyCallback)ci_client_connected_func, NULL);

    ci_client.state = CIClientStateConnecting;
}

void ci_client_disconnect(void)
{
    ci_client_stop_timer();
    ci_client_stop();
}

void ci_client_stop(void)
{
    gchar *msgdata = NULL;
    gsize len;

    if (ci_client.state == CIClientStateConnecting && ci_client.cancel) {
        g_cancellable_cancel(ci_client.cancel);
        g_object_unref(ci_client.cancel);
        ci_client.cancel = NULL;
    }
    else if (ci_client.state == CIClientStateConnected) {
        if (cinet_message_new_for_data(&msgdata, &len, CI_NET_MSG_LEAVE, NULL, NULL) == 0) {
            ci_client_send_message(ci_client.connection, msgdata, len);
            g_free(msgdata);
        }

        if (ci_client.stream_source_id) {
            g_source_remove(ci_client.stream_source_id);
            ci_client.stream_source_id = 0;
        }
        g_io_stream_close(G_IO_STREAM(ci_client.connection), NULL, NULL);
        g_object_unref(ci_client.connection);
        ci_client.connection = NULL;
    }

    ci_client.state = CIClientStateInitialized;
}

void ci_client_cleanup(void)
{
    if (ci_client.state == CIClientStateInitialized) {
        ci_client_stop_timer();
        g_free(ci_client.host);
        ci_client.state = CIClientStateUnknown;
    }
}

void ci_client_handle_message(CINetMsg *msg)
{
    if (msg == NULL)
        return;

    if (msg->msgtype == CI_NET_MSG_LEAVE)
        ci_client.state = CIClientStateInitialized;
    
    if (ci_client.msgcallback)
        ci_client.msgcallback(msg);
}

void ci_client_send_message(GSocketConnection *conn, gchar *data, gsize len)
{
    GSocket *socket = g_socket_connection_get_socket(conn);
    if (socket == NULL)
        return;
    if (!g_socket_is_connected(socket))
        return;

    gssize bytes = 0;
    gssize rc;
    while (bytes < len) {
        rc = g_socket_send(socket, &data[bytes], len-bytes, NULL, NULL);
        if (rc < 0)
            return;
        bytes += rc;
    }
}

CINetMsg *ci_client_recv_message(GSocket *socket, gboolean *conn_err)
{
    CINetMsg *msg = NULL;
    gssize bytes;
    gssize rc;
    gchar *msgdata;
    CINetMsgHeader header;
    gchar buf[32];
    if (conn_err) *conn_err = FALSE;

    if (socket == NULL)
        return NULL;
    
    bytes = g_socket_receive(socket, buf, CINET_HEADER_LENGTH, NULL, NULL);
    if (bytes <= 0)
        goto connection_error;

    if (cinet_msg_read_header(&header, buf, bytes) < CINET_HEADER_LENGTH)
        return NULL;

    msgdata = g_malloc(CINET_HEADER_LENGTH + header.msglen);
    memcpy(msgdata, buf, bytes);
    bytes = 0;
    while (bytes < header.msglen) {
        rc = g_socket_receive(socket, &msgdata[CINET_HEADER_LENGTH + bytes],
                header.msglen - bytes, NULL, NULL);
        if (rc <= 0)
            goto connection_error;
        bytes += rc;
    }

    if (cinet_msg_read_msg(&msg, msgdata, CINET_HEADER_LENGTH + header.msglen) != 0) {
        g_free(msgdata);
        return NULL;
    }
    g_free(msgdata);

    return msg;

connection_error:
    if (conn_err)
        *conn_err = TRUE;
    return NULL;
}

gboolean ci_client_incoming_data(GSocket *socket, GIOCondition cond, GSocketConnection *conn)
{
    CINetMsg *msg;
    gboolean conn_err = FALSE;

    if (cond == G_IO_IN) {
        msg = ci_client_recv_message(socket, &conn_err);
        if (msg == NULL) {
            if (conn_err) {
                ci_client_handle_connection_lost();
                return FALSE;
            }
            return TRUE;
        }

        ci_client_handle_message(msg);
        cinet_msg_free(msg);
        return TRUE;
    }
    
    if ((cond & G_IO_ERR) || (cond & G_IO_HUP)) {
        ci_client_handle_connection_lost();
        return FALSE;
    }

    return TRUE;
}

void ci_client_connected_func(GSocketClient *source, GAsyncResult *result, gpointer userdata)
{
    if (ci_client.cancel && g_cancellable_is_cancelled(ci_client.cancel)) {
        g_object_unref(ci_client.cancel);
        ci_client.cancel = NULL;
        return;
    }

    GError *err = NULL;
    ci_client.connection = g_socket_client_connect_to_host_finish(source, result, &err);
    if (ci_client.connection == NULL) {
        /* log error */
        g_error_free(err);
        ci_client.state = CIClientStateInitialized;
        ci_client_handle_connection_lost();
        return;
    }

    ci_client_stop_timer();
    g_tcp_connection_set_graceful_disconnect(G_TCP_CONNECTION(ci_client.connection), TRUE);

    GSource *sock_source = NULL;
    GSocket *client_sock = g_socket_connection_get_socket(ci_client.connection);

    sock_source = g_socket_create_source(G_SOCKET(client_sock),
            G_IO_IN | G_IO_HUP | G_IO_ERR, NULL);
    if (sock_source != NULL) {
        g_source_set_callback(sock_source, (GSourceFunc)ci_client_incoming_data,
                ci_client.connection, NULL);
        g_source_attach(sock_source, NULL);
        ci_client.stream_source_id = g_source_get_id(sock_source);
    }

    ci_client.state = CIClientStateConnected;

    gchar *msgdata = NULL;
    gsize len = 0;

    if (cinet_message_new_for_data(&msgdata, &len, CI_NET_MSG_VERSION,
               "major", 3, "minor", 0, "patch", 0, NULL, NULL) == 0) {
        ci_client_send_message(ci_client.connection, msgdata, len);
        g_free(msgdata);
    }

    g_object_unref(ci_client.cancel);
    ci_client.cancel = NULL;
}

void ci_client_stop_timer(void)
{
    if (ci_client.timer_source_id != 0) {
        g_source_remove(ci_client.timer_source_id);
        ci_client.timer_source_id = 0;
    }
}

gboolean ci_client_try_connect_func(gpointer userdata)
{
    switch (ci_client.state) {
        case CIClientStateConnecting:
            return TRUE;
        case CIClientStateConnected:
            ci_client.timer_source_id = 0;
            return FALSE;
        case CIClientStateDisconnected:
        case CIClientStateInitialized:
            ci_client_connect();
            return TRUE;
        default:
            return TRUE;
    }
}

void ci_client_handle_connection_lost(void)
{
    if (ci_client.timer_source_id)
        return;

    ci_client_stop();

    gint retry_interval = 0;
    ci_config_get("retry-interval", &retry_interval);

    if (retry_interval > 0)
        ci_client.timer_source_id = g_timeout_add_seconds(retry_interval,
                (GSourceFunc)ci_client_try_connect_func, NULL);
}
