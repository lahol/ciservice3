#include <glib.h>
#include <glib-object.h>
#include <glib-unix.h>
#include <cinetmsgs.h>
#include "ci-config.h"
#include <ci-client.h>
#include "ci-service.h"
#include "daemon.h"
#include <stdio.h>

CIClient *ci_client = NULL;

gboolean ci_main_handle_signal(GMainLoop *mainloop)
{
    g_main_loop_quit(mainloop);
    return FALSE;
}

void ci_main_cleanup(gboolean full)
{
    ci_config_cleanup();
    ci_service_cleanup();

    /* the following is only needed in the daemon */
    if (!full)
        return;

    if (ci_client != NULL) {
        ci_client_disconnect(ci_client);
        ci_client_shutdown(ci_client);
    }

    stop_daemon();
}

void ci_main_list_services(void)
{
    GList *services = ci_service_list_services();
    GList *tmp;
    const gchar *id;
    for (tmp = services; tmp != NULL; tmp = g_list_next(tmp)) {
        id = ci_service_get_identifier((CIService *)tmp->data);
        fprintf(stdout, "%s: %s\n", id ? id : "<cmdline>",
                ci_service_get_active((CIService *)tmp->data) ? "active" : "sleeping");
    }

    g_list_free(services);
}

void ci_main_print_version(void)
{
    fprintf(stdout, "%s - %s\n", APPNAME, VERSION);
}

void ci_main_handle_message(CINetMsg *msg)
{
    if (msg == NULL)
        return;
    if (msg->msgtype == CI_NET_MSG_EVENT_RING &&
            ((CINetMsgMultipart*)msg)->stage == MultipartStageComplete) {
        ci_service_run_commands(&((CINetMsgEventRing*)msg)->callinfo);
    }
}

int main(int argc, char **argv)
{
#if !GLIB_CHECK_VERSION(2,36,0)
    g_type_init();
#endif
    if (!ci_config_load(&argc, &argv))
        return 1;

    gboolean flag;
    pid_t daemon_pid;
    gchar *pidfile = NULL;

    if (ci_config_get("print-version", &flag) && flag) {
        ci_main_print_version();
        goto done;
    }
    if (ci_config_get("list-services", &flag) && flag) {
        ci_main_list_services();
        goto done;
    }
    if (ci_config_get("daemonize", &flag) && flag &&
            ci_config_get("pidfile", &pidfile)) {
        daemon_pid = start_daemon(argv[0], pidfile);
        g_free(pidfile);

        /* TODO: if daemon is already running, just add services via ipc-socket */
        if (daemon_pid == -1) {
            fprintf(stderr, "Could not start daemon.\n");
            ci_main_cleanup(TRUE);
            return 1;
        }
        if (daemon_pid) {
            fprintf(stderr, "Starting as daemon. Process id is %d\n", daemon_pid);
            ci_main_cleanup(FALSE);
            return 0;
        }
    }

    gchar *host = NULL;
    guint port;
    gint retry_interval;

    ci_config_get("hostname", &host);
    ci_config_get("port", &port);
    ci_config_get("retry-interval", &retry_interval);

    ci_client = ci_client_new_full(host, port, ci_main_handle_message, NULL);
    ci_client_set_retry_interval(ci_client, retry_interval);

    ci_client_connect(ci_client);

    GMainLoop *mainloop = g_main_loop_new(NULL, FALSE);

    g_unix_signal_add(SIGINT, (GSourceFunc)ci_main_handle_signal, mainloop);
    g_unix_signal_add(SIGTERM, (GSourceFunc)ci_main_handle_signal, mainloop);

    g_main_loop_run(mainloop);

done:
    ci_main_cleanup(TRUE);

    return 0;
}
