#ifndef __CI_SERVICE_H__
#define __CI_SERVICE_H__

#include <glib.h>
#include <cinetmsgs.h>

typedef struct CIService CIService;

gboolean ci_service_add_service(const gchar *identifier, const gchar *commandline, gboolean active);
void ci_service_run_commands(CICallInfo *callinfo);
GList *ci_service_list_services(void); /* [element-type: CIService *][transfer-container] */

CIService *ci_service_get(const gchar *identifier);
const gchar *ci_service_get_identifier(CIService *service);
const gchar *ci_service_get_commandline(CIService *service);
gboolean ci_service_get_active(CIService *service);

void ci_service_set_active(CIService *service, gboolean active);

void ci_service_cleanup(void);

#endif
