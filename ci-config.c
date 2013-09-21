#include "ci-config.h"
#include "ci-service.h"
#include <stdio.h>

struct {
    gchar *hostname;
    guint16 port;
    gint retry_interval;
    gchar *pidfile;
    gchar *config_file;

    gboolean print_version;
    gboolean list_services;
    gboolean daemonize;
} ci_config;

gboolean ci_config_option_add_command(const gchar *option_name,
                                      const gchar *value,
                                      gpointer data,
                                      GError **error)
{
    if (!ci_service_add_service(NULL, value, TRUE)) {
        g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                "Command `%s` is invalid.", value);
        return FALSE;
    }

    return TRUE;
}

gchar *ci_config_get_config_file(void)
{
    if (ci_config.config_file != NULL &&
            g_file_test(ci_config.config_file, G_FILE_TEST_IS_REGULAR))
        return g_strdup(ci_config.config_file);

    gchar *filename;
    filename = g_build_filename(g_get_user_config_dir(), "ciservicerc", NULL);
    if (g_file_test(filename, G_FILE_TEST_IS_REGULAR))
        return filename;
    g_free(filename);

#ifdef SYSCONFIGDIR
    filename = g_build_filename(SYSCONFIGDIR, "ciservicerc", NULL);
    if (g_file_test(filename, G_FILE_TEST_IS_REGULAR))
        return filename;
    g_free(filename);
#endif

    const gchar * const *cfgdirs = g_get_system_config_dirs();
    if (cfgdirs == NULL)
        return NULL;
    guint i;

    for (i = 0; cfgdirs[i] != NULL; ++i) {
        filename = g_build_filename(cfgdirs[i], "ciservicerc", NULL);
        if (g_file_test(filename, G_FILE_TEST_IS_REGULAR))
            return filename;
        g_free(filename);
    }

    return NULL;
}

gboolean ci_config_load_file(void)
{
    gchar *cfgfile = ci_config_get_config_file();
    if (cfgfile == NULL)
        return FALSE;

    GKeyFile *keyfile = g_key_file_new();
    if (!g_key_file_load_from_file(keyfile, cfgfile, 0, NULL)) {
        g_free(cfgfile);
        return FALSE;
    }

    /* check if these are already set via command line; if not read them from file */
    if (ci_config.pidfile == NULL)
        ci_config.pidfile = g_key_file_get_string(keyfile, "General", "pidfile", NULL);
    if (ci_config.hostname == NULL)
        ci_config.hostname = g_key_file_get_string(keyfile, "Server", "host", NULL);
    if (ci_config.port == 0)
        ci_config.port = g_key_file_get_integer(keyfile, "Server", "port", NULL);
    if (ci_config.retry_interval < 0 && g_key_file_has_key(keyfile, "Server", "retry-interval", NULL))
        ci_config.retry_interval = g_key_file_get_integer(keyfile, "Server", "retry-interval", NULL);

    /* get services */
    gchar **services = g_key_file_get_keys(keyfile, "Services", NULL, NULL);

    guint i;
    gchar *cmd;
    if (services != NULL) {
        for (i = 0; services[i] != NULL; ++i) {
            cmd = g_key_file_get_string(keyfile, "Services", services[i], NULL);
            ci_service_add_service(services[i], cmd, FALSE);
            g_free(cmd);
        }
        g_strfreev(services);
    }

    g_key_file_free(keyfile);
    return TRUE;
}

gboolean ci_config_load(int *argc, char ***argv)
{
    ci_config.retry_interval = -1;
    GOptionEntry cmdline_options[] = {
        { "version", 'v', 0, G_OPTION_ARG_NONE, &ci_config.print_version,
            "Print version and exit.", NULL },
        { "list", 'l', 0, G_OPTION_ARG_NONE, &ci_config.list_services,
            "List available services and exit.", NULL },
        { "daemonize", 'd', 0, G_OPTION_ARG_NONE, &ci_config.daemonize,
            "Start in background.", NULL },
        { "host", 'h', 0, G_OPTION_ARG_STRING, &ci_config.hostname,
            "Hostname of the server.", NULL },
        { "port", 'p', 0, G_OPTION_ARG_INT, &ci_config.port,
            "Port of the server.", NULL },
        { "retry-interval", 'r', 0, G_OPTION_ARG_INT, &ci_config.retry_interval,
            "Retry interval in seconds if connection is lost.", NULL },
        { "command", 'c', 0, G_OPTION_ARG_CALLBACK, ci_config_option_add_command,
            "Command to execute.", NULL },
        { "file", 'f', 0, G_OPTION_ARG_STRING, &ci_config.config_file,
            "Alternative configuration file.", NULL },
        { NULL }
    };

    GError *error = NULL;

    GOptionContext *context = g_option_context_new("- run callerinfo services");
    g_option_context_add_main_entries(context, cmdline_options, NULL);

    if (!g_option_context_parse(context, argc, argv, &error)) {
        fprintf(stderr, "%s\n", error->message);
        g_error_free(error);
        return FALSE;
    }

    /* read config file */
    ci_config_load_file();
    /* add services from argv[1], argv[2], … if available in conf file */
    gint i;
    CIService *service;
    for (i = 1; i < *argc; ++i) {
        service = ci_service_get((*argv)[i]);
        if (service == NULL)
            fprintf(stderr, "Service `%s' not found.\n", (*argv)[i]);
        else
            ci_service_set_active(service, TRUE);
    }

    /* otherwise we have no way to determine if this interval was set by command line or keyfile */
    if (ci_config.retry_interval < 0)
        ci_config.retry_interval = 10;

    return TRUE;
}

void ci_config_cleanup(void)
{
    g_free(ci_config.hostname);
    g_free(ci_config.pidfile);
    g_free(ci_config.config_file);
}

gboolean ci_config_get(const gchar *key, gpointer val)
{
    if (key == NULL || val == NULL)
        return FALSE;

    if (g_strcmp0(key, "hostname") == 0)
        *((gchar **)val) = g_strdup(ci_config.hostname);
    else if (g_strcmp0(key, "port") == 0)
        *((guint *)val) = ci_config.port;
    else if (g_strcmp0(key, "daemonize") == 0)
        *((gboolean *)val) = ci_config.daemonize;
    else if (g_strcmp0(key, "print-version") == 0)
        *((gboolean *)val) = ci_config.print_version;
    else if (g_strcmp0(key, "list-services") == 0)
        *((gboolean *)val) = ci_config.list_services;
    else if (g_strcmp0(key, "pidfile") == 0)
        *((gchar **)val) = g_strdup(ci_config.pidfile);
    else if (g_strcmp0(key, "retry-interval") == 0)
        *((gint *)val) = ci_config.retry_interval;
    else
        return FALSE;

    return TRUE;
}
