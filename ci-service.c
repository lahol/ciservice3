#include "ci-service.h"
#include <string.h>
#include <stdio.h>

struct CIService {
    gchar *identifier;
    gchar *command;
    gint userid;
    gboolean active;
};

GList *ci_services = NULL;
GRegex *ci_service_regex = NULL;

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

CIService *ci_service_add_service(const gchar *identifier, const gchar *commandline, gboolean active)
{
    if (!ci_service_check_command(commandline))
        return NULL;

    struct CIService *service = g_malloc0(sizeof(struct CIService));
    service->identifier = g_strdup(identifier);
    service->command = g_strdup(commandline);
    service->active = active;
    service->userid = -1;

    ci_services = g_list_append(ci_services, service);

    return service;
}

GList *ci_service_list_services(void)
{
    return g_list_copy(ci_services);
}

gint ci_service_compare_identifier(struct CIService *service, const gchar *identifier)
{
    g_return_val_if_fail(service != NULL, -1);

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
    g_return_val_if_fail(service != NULL, NULL);

    return service->identifier;
}

const gchar *ci_service_get_commandline(CIService *service)
{
    g_return_val_if_fail(service != NULL, NULL);

    return service->command;
}

gboolean ci_service_get_active(CIService *service)
{
    g_return_val_if_fail(service != NULL, FALSE);

    return service->active;
}

void ci_service_set_active(CIService *service, gboolean active)
{
    g_return_if_fail(service != NULL);

    service->active = active;
}

void ci_service_set_userid(CIService *service, gint userid)
{
    g_return_if_fail(service != NULL);

    service->userid = userid;
}

gint ci_service_get_userid(CIService *service)
{
    g_return_val_if_fail(service != NULL, -1);

    return service->userid;
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

struct _CIServiceQuery {
    CIService *service;
    GHashTable *hashtable;
};

void ci_service_query_caller_complete_cb(const gchar *name, struct _CIServiceQuery *querydata)
{
    g_return_if_fail(querydata != NULL);
    if (!querydata->service || !querydata->hashtable)
        goto done;

    gchar *name_bkup = g_strdup(g_hash_table_lookup(querydata->hashtable, "${name}"));
    if (name && name[0]) {
        g_hash_table_replace(querydata->hashtable, "${name}", g_strdup(name));
    }

    gchar *cmd = g_regex_replace_eval(ci_service_regex, querydata->service->command,
            -1, 0, 0, ci_service_regex_eval_cb, querydata->hashtable, NULL);
    ci_service_run(cmd);
    g_free(cmd);

    if (name && name[0]) {
        g_hash_table_replace(querydata->hashtable, "${name}", name_bkup);
    }
done:
    if (querydata->hashtable)
        g_hash_table_unref(querydata->hashtable);
    g_free(querydata);
}

void ci_service_run_commands(CICallInfo *callinfo, CIServiceQueryCallerCallback query_caller_cb, gpointer userdata)
{
    if (ci_services == NULL || callinfo == NULL)
        return;

    gchar buffer[64];
    GHashTable *hashtable = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
    g_hash_table_insert(hashtable, "${number}", g_strdup(callinfo->number));
    g_hash_table_insert(hashtable, "${areacode}", g_strdup(callinfo->areacode));
    g_hash_table_insert(hashtable, "${area}", g_strdup(callinfo->area));
    g_hash_table_insert(hashtable, "${name}", g_strdup(callinfo->name));
    snprintf(buffer, 32, "%s %s", callinfo->date, callinfo->time);
    g_hash_table_insert(hashtable, "${time}", g_strdup(callinfo->time));
    g_hash_table_insert(hashtable, "${msn}", g_strdup(callinfo->msn));
    g_hash_table_insert(hashtable, "${alias}", g_strdup(callinfo->alias));
    g_hash_table_insert(hashtable, "${completenumber}", g_strdup(callinfo->completenumber));

    if (G_UNLIKELY(ci_service_regex == NULL)) {
        ci_service_regex =
            g_regex_new("\\${number}|\\${areacode}|\\${area}|\\${name}|\\${time}|\\${msn}|\\${alias}|\\${completenumber}",
                        G_REGEX_RAW, 0, NULL);
    }

    GList *tmp;
    struct _CIServiceQuery *querydata;
    for (tmp = ci_services; tmp != NULL; tmp = g_list_next(tmp)) {
        if (((struct CIService *)tmp->data)->active) {
            querydata = g_malloc0(sizeof(struct _CIServiceQuery));
            querydata->service = (struct CIService *)tmp->data;
            querydata->hashtable = hashtable;
            g_hash_table_ref(hashtable);
            if (((struct CIService *)tmp->data)->userid != -1 &&
                    query_caller_cb) {
                query_caller_cb(callinfo->completenumber, ((struct CIService *)tmp->data)->userid, userdata,
                        (CIServiceQueryCompleteCallback)ci_service_query_caller_complete_cb, querydata);
            }
            else {
                ci_service_query_caller_complete_cb(NULL, querydata);
            }
        }
    }

    g_hash_table_unref(hashtable);
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
    if (ci_service_regex)
        g_regex_unref(ci_service_regex);
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
