#include "ci-service.h"
#include <string.h>
#include <stdio.h>

struct CIService {
    gchar *identifier;
    gchar *command;
    gboolean active;
};

GList *ci_services = NULL;

void ci_service_run(const gchar *command);
gboolean ci_service_regex_eval_cb(const GMatchInfo *info, GString *res, gpointer data);

gboolean ci_service_check_command(const gchar *commandline)
{
    if (commandline == NULL || commandline[0] == 0)
        return FALSE;

    gint ac;
    gchar **av = NULL;
    gchar *cmd = NULL;

    if (!g_shell_parse_argv(commandline, &ac, &av, NULL) ||
            (cmd = g_find_program_in_path(av[0])) == NULL) {
        g_strfreev(av);
        g_free(cmd);
        return FALSE;
    }

    g_strfreev(av);
    g_free(cmd);
    return TRUE;
}

gboolean ci_service_add_service(const gchar *identifier, const gchar *commandline, gboolean active)
{
    if (!ci_service_check_command(commandline))
        return FALSE;

    struct CIService *service = g_malloc0(sizeof(struct CIService));
    service->identifier = g_strdup(identifier);
    service->command = g_strdup(commandline);
    service->active = active;

    ci_services = g_list_append(ci_services, service);
    return TRUE;
}

GList *ci_service_list_services(void)
{
    return g_list_copy(ci_services);
}

gint ci_service_compare_identifier(struct CIService *service, const gchar *identifier)
{
    if (service == NULL)
        return -1;
    return g_strcmp0(service->identifier, identifier);
}

CIService *ci_service_get(const gchar *identifier)
{
    GList *result = g_list_find_custom(ci_services, identifier,
            (GCompareFunc)ci_service_compare_identifier);
    if (result != NULL)
        return (struct CIService *)result->data;

    return NULL;
}

const gchar *ci_service_get_identifier(CIService *service)
{
    if (service == NULL)
        return NULL;
    return service->identifier;
}

const gchar *ci_service_get_commandline(CIService *service)
{
    if (service == NULL)
        return NULL;
    return service->command;
}

gboolean ci_service_get_active(CIService *service)
{
    if (service == NULL)
        return FALSE;
    return service->active;
}

void ci_service_set_active(CIService *service, gboolean active)
{
    if (service != NULL)
        service->active = active;
}

gboolean ci_service_regex_eval_cb(const GMatchInfo *info, GString *res, gpointer data)
{
    gchar *match;
    gchar *r;

    match = g_match_info_fetch(info, 0);
    r = g_hash_table_lookup((GHashTable *)data, match);

    gchar *quoted = NULL;
    
    if (r != NULL)
        quoted = g_shell_quote(r);
    else
        quoted = g_shell_quote("");
    g_string_append(res, quoted);
    g_free(quoted);

    g_free(match);

    return FALSE;
}

void ci_service_run_commands(CICallInfo *callinfo)
{
    if (ci_services == NULL || callinfo == NULL)
        return;

    gchar buffer[64];
    GHashTable *hashtable = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(hashtable, "${number}", callinfo->number);
    g_hash_table_insert(hashtable, "${areacode}", callinfo->areacode);
    g_hash_table_insert(hashtable, "${area}", callinfo->area);
    g_hash_table_insert(hashtable, "${name}", callinfo->name);
    snprintf(buffer, 32, "%s %s", callinfo->date, callinfo->time);
    g_hash_table_insert(hashtable, "${time}", callinfo->time);
    g_hash_table_insert(hashtable, "${msn}", callinfo->msn);
    g_hash_table_insert(hashtable, "${alias}", callinfo->alias);
    g_hash_table_insert(hashtable, "${completenumber}", callinfo->completenumber);

    GRegex *regex = g_regex_new("\\${number}|\\${areacode}|\\${area}|\\${name}|\\${time}|\\${msn}|\\${alias}|\\${completenumber}",
            G_REGEX_RAW, 0, NULL);

    GList *tmp;
    gchar *cmd;
    for (tmp = ci_services; tmp != NULL; tmp = g_list_next(tmp)) {
        if (((struct CIService *)tmp->data)->active) {
            cmd = g_regex_replace_eval(regex, ((struct CIService *)tmp->data)->command,
                    -1, 0, 0, ci_service_regex_eval_cb, hashtable, NULL);
            ci_service_run(cmd);
            g_free(cmd);
        }
    }

    g_hash_table_destroy(hashtable);
}

void ci_service_free(struct CIService *service)
{
    if (service != NULL) {
        g_free(service->command);
        g_free(service->identifier);
        g_free(service);
    }
}

void ci_service_cleanup(void)
{
    g_list_free_full(ci_services, (GDestroyNotify)ci_service_free);
}

void ci_service_run(const gchar *command)
{
    gint argc = 0;
    gchar **argv = NULL;

    if (!g_shell_parse_argv(command, &argc, &argv, NULL))
        return;
    g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
            NULL, NULL, NULL, NULL);
    g_strfreev(argv);
}
