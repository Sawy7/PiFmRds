#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gprintf.h>

#define MPRIS_PREFIX "org.mpris.MediaPlayer2."

int GetPlayer();
int GetMetadata(const char *player);
int GetPlaybackStatus(const char *player);