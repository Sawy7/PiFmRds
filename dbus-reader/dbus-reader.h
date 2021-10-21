#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gprintf.h>

#define MPRIS_PREFIX "org.mpris.MediaPlayer2."

int GetPlaybackStatus(const char *player);
int NewDBus();
int GetMetaCombined();
int AddSignal(const char *player);
void OnChange();
void on_signal (GDBusProxy *proxy,
           gchar      *sender_name,
           gchar      *signal_name,
           GVariant   *parameters,
           gpointer    user_data);
void on_name_owner_notify (GObject    *object,
                      GParamSpec *pspec,
                      gpointer    user_data);