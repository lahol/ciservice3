#ifndef __DAEMON_H__
#define __DAEMON_H__

#include <unistd.h>

pid_t start_daemon(char *pidfile);
int stop_daemon(void);

#endif
