/*
    PiFmRds - FM/RDS transmitter for the Raspberry Pi
    Copyright (C) 2021 Jan Němec
    
    See https://github.com/ChristopheJacquet/PiFmRds
    
    rds_wav.c is a test program that writes a RDS baseband signal to a WAV
    file. It requires libsndfile.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    
    dbus_mediainfo.c: handles automatic radiotext by getting music metadata from dbus.
*/

#include "dbus_mediainfo.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <gio/gio.h>
#include <glib.h>

GError *tmp_error;
GDBusProxy *get_names_proxy;
GDBusProxy *get_props_proxy;
static GMainLoop *loop = NULL;
char *metadata_text = NULL;
int end_thread = 0;

void *dbus_main(void *userdata)
{
    metadata_text = (char*)userdata;
    
    // Block termination from this thread
    sigset_t mask;
	sigemptyset(&mask);
    for (int i = 0; i < 64; i++)
    {
        sigaddset(&mask, i);
    }
    pthread_sigmask(SIG_BLOCK, &mask, NULL);
    
    if (create_dbus() != 0)
    {
        fprintf(stderr, "Error: Could not connect to DBus.\n");
        return NULL;
    }
    
    while (1)
    {
        if (get_metadata() != 0)
        {
            fprintf(stderr, "Error: Could not get metadata.\n");
            return NULL;
        }
        if (end_thread)
        {
            break;
        }
        sleep(3);
    }

    return NULL;
}

int create_dbus()
{
    seteuid(getuid()); // This needs to run as normal user
    tmp_error = NULL;

    // Connect to DBus
    get_names_proxy = g_dbus_proxy_new_for_bus_sync( G_BUS_TYPE_SESSION,
                                                     G_DBUS_PROXY_FLAGS_NONE,
                                                     NULL,
                                                     "org.freedesktop.DBus",
                                                     "/org/freedesktop/DBus",
                                                     "org.freedesktop.DBus",
                                                     NULL,
                                                     &tmp_error);

    // Check for errors
    if (tmp_error != NULL) {
        return -1;
    }

    seteuid(0);
    return 0;
}

int get_metadata()
{
    // Call method (listing all players)
    GVariant *reply = g_dbus_proxy_call_sync(get_names_proxy, "ListNames", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &tmp_error);

    // Check for errors
    if (tmp_error != NULL) {
        g_object_unref(get_names_proxy);
        return -1;
    }

    // Getting array of players
    GVariant *reply_child = g_variant_get_child_value(reply, 0);
    gsize reply_count;
    const gchar **names = g_variant_get_strv(reply_child, &reply_count);
   
    for (gsize i = 0; i < reply_count; i += 1) {
        if (g_str_has_prefix(names[i], "org.mpris.MediaPlayer2.")) {
            if (get_playback_status(names[i]) == 0)
            {
                // printf("%s is playing.\n", names[i]);
                // Hook into this player
                add_signal(names[i]);
                break;
            }
        }
    }

    g_variant_unref(reply);
    g_variant_unref(reply_child);
    g_free(names);

    return 0;
}

int get_playback_status(const char *player)
{
    GError *tmp_error = NULL;

    GDBusProxy *proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
                                                      G_DBUS_PROXY_FLAGS_NONE,
                                                      NULL,
                                                      player,
                                                      "/org/mpris/MediaPlayer2",
                                                      "org.freedesktop.DBus.Properties",
                                                      NULL,
                                                      &tmp_error);

    if (tmp_error != NULL) {
        if (tmp_error->domain == G_IO_ERROR &&
            tmp_error->code == G_IO_ERROR_NOT_FOUND) {
            return -1;
        }
        return -2;
    }

    GVariant *parameters_array[2];

    parameters_array[0] = g_variant_new("s", "org.mpris.MediaPlayer2.Player");
    g_variant_ref(parameters_array[0]);
    parameters_array[1] = g_variant_new("s", "PlaybackStatus");
    g_variant_ref(parameters_array[1]);
    GVariant *parameters = g_variant_new_tuple(parameters_array, 2);
    g_variant_ref(parameters);
    
    GVariant *reply = g_dbus_proxy_call_sync(
        proxy, "Get", parameters, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &tmp_error);

    if (tmp_error != NULL) {
        g_object_unref(proxy);
        return -3;
    }

    int playbackReturn = -1;
    if (strcmp((char*)g_variant_get_data(reply), "Playing") == 0)
    {
        playbackReturn = 0;
    }
    
    g_object_unref(proxy);
    g_variant_unref(reply);
    g_variant_unref(parameters);
    g_variant_unref(parameters_array[0]);
    g_variant_unref(parameters_array[1]);

    return playbackReturn;
}

int add_signal(const char *player)
{
    loop = g_main_loop_new (NULL, FALSE);

    get_props_proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
                                                      G_DBUS_PROXY_FLAGS_NONE,
                                                      NULL,
                                                      player,
                                                      "/org/mpris/MediaPlayer2",
                                                      "org.freedesktop.DBus.Properties",
                                                      NULL,
                                                      &tmp_error);

    if (tmp_error != NULL) {
        if (tmp_error->domain == G_IO_ERROR &&
            tmp_error->code == G_IO_ERROR_NOT_FOUND) {
            return -1;
        }
        return -2;
    }

    GVariant *parameters;
    GVariant *parameters_array[2];

    parameters_array[0] = g_variant_new("s", "org.mpris.MediaPlayer2.Player");
    parameters_array[1] = g_variant_new("s", "Metadata");
    parameters = g_variant_new_tuple(parameters_array, 2);

    GVariant *reply = g_dbus_proxy_call_sync(
        get_props_proxy, "Get", parameters, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &tmp_error);

    if (tmp_error != NULL) {
        g_object_unref(get_props_proxy);
        return -2;
    }

    GVariant *props;

    g_variant_get(reply, "(v)", &props);
    
    export_metadata(props);

    g_signal_connect(get_props_proxy,
                    "g-signal",
                    G_CALLBACK (on_signal),
                    NULL);

    g_signal_connect(get_props_proxy,
                    "notify::g-name-owner",
                    G_CALLBACK (on_name_owner_notify),
                    NULL);

    g_main_loop_run(loop);

    g_object_unref(get_props_proxy);
    g_variant_unref(reply);
    
    return 0;
}

void export_metadata(GVariant *metadata)
{
    gchar **artists = NULL, *title = NULL;
    g_variant_lookup(metadata, "xesam:artist", "^a&s", &artists);
    g_variant_lookup(metadata, "xesam:title", "s", &title);

    // gchar *value_str = g_variant_print(metadata, TRUE);
    // printf("md: %s\n", (char*)value_str);

    // Song must have well defined ID3 (or equivalent) metadata
    if (artists && title)
    {
        snprintf(metadata_text, METADATA_TEXT_SIZE, "%s - %s", g_strjoinv(", ", artists), title);
    }
    else
    {
        snprintf(metadata_text, METADATA_TEXT_SIZE, "NO METADATA");
        // This only works in signal, so it's ok
        g_main_loop_quit(loop);
    }

    g_free(artists);
    g_free(title);
}

void on_signal (GDBusProxy *proxy, gchar *sender_name, gchar *signal_name, GVariant *parameters, gpointer user_data)
{
    int exit_loop = 0;
    gchar *parameters_str;

    GVariant *child = g_variant_get_child_value(parameters, 1);

    // gchar *value_str = g_variant_print(child, TRUE);
    // printf("md: %s\n", (char*)value_str);

    if (g_variant_lookup(child, "PlaybackStatus", "s", &parameters_str))
    {
        if (strcmp((char*)parameters_str, "Paused") == 0 || strcmp((char*)parameters_str, "Stopped") == 0)
        {
            exit_loop = 1;
        }

        g_free (parameters_str);
    }
    else if (g_variant_lookup(child, "Metadata", "a{sv}", &parameters_str))
    {
        GVariant *metadata = g_variant_lookup_value(child, "Metadata", G_VARIANT_TYPE_DICTIONARY);
        export_metadata(metadata);
        
        g_variant_unref(metadata);
    }
    
    g_variant_unref(child);

    if (exit_loop)
    {
        snprintf(metadata_text, METADATA_TEXT_SIZE, "NO MEDIA");
        g_main_loop_quit(loop);
    }
}

void on_name_owner_notify (GObject *object, GParamSpec *pspec, gpointer user_data)
{
    snprintf(metadata_text, METADATA_TEXT_SIZE, "NO MEDIA");
    g_main_loop_quit(loop);
}

void quit_dbus_thread()
{
    end_thread = 1;
    g_main_loop_quit(loop);
}