#ifndef __CI_CLIENT_H__
#define __CI_CLIENT_H__

#include <glib.h>
#include <cinet.h>

typedef void (*CIMsgCallback)(CINetMsg *msg);

void ci_client_init(CIMsgCallback callback);
void ci_client_connect(void);
void ci_client_disconnect(void);
void ci_client_stop(void);
void ci_client_cleanup(void);

#endif
