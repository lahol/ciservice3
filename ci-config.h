#ifndef __CI_CONFIG_H__
#define __CI_CONFIG_H__

#include <glib.h>

gboolean ci_config_load(int *argc, char ***argv);
void ci_config_cleanup(void);

gboolean ci_config_get(const gchar *key, gpointer val);

#endif
