#ifndef __CI_SERVICE_H__
#define __CI_SERVICE_H__

#include <glib.h>
#include <cinetmsgs.h>

typedef struct CIService CIService;

CIService *ci_service_add_service(const gchar *identifier, const gchar *commandline, gboolean active);

CIService *ci_service_get(const gchar *identifier);
const gchar *ci_service_get_identifier(CIService *service);

const gchar *ci_service_get_commandline(CIService *service);

void ci_service_set_active(CIService *service, gboolean active);
gboolean ci_service_get_active(CIService *service);

void ci_service_set_userid(CIService *service, gint userid);
gint ci_service_get_userid(CIService *service);

/* name, data */
typedef void (*CIServiceQueryCompleteCallback)(const gchar *, gpointer);
/* completenumber, userid, userdata, cb, servicedata */
typedef void (*CIServiceQueryCallerCallback)(const gchar *, gint, gpointer,
                                CIServiceQueryCompleteCallback, gpointer);
void ci_service_run_commands(CICallInfo *callinfo, CIServiceQueryCallerCallback query_caller_cb, gpointer userdata);

GList *ci_service_list_services(void); /* [element-type: CIService *][transfer-container] */

void ci_service_cleanup(void);

#endif
