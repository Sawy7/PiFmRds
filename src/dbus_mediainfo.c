#include "dbus_mediainfo.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gprintf.h>

int get_mediainfo(char *mediainfo, int bytes)
{
    seteuid(getuid());
    snprintf(mediainfo, bytes/sizeof(char), "NO MEDIA PLAYING");

    GError *tmp_error = NULL;

    // Connect to DBus
    GDBusProxy *proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
                                                      G_DBUS_PROXY_FLAGS_NONE,
                                                      NULL,
                                                      "org.freedesktop.DBus",
                                                      "/org/freedesktop/DBus",
                                                      "org.freedesktop.DBus",
                                                      NULL,
                                                      &tmp_error);

    // Check for errors
    if (tmp_error != NULL) {
        fprintf(stderr, "Could not connect to DBus.\n");
        return -1;
    }

    // Call method (listing all players)
    GVariant *reply = g_dbus_proxy_call_sync(proxy, "ListNames", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &tmp_error);

    // Check for errors
    if (tmp_error != NULL) {
        g_object_unref(proxy);
        fprintf(stderr, "DBus method call failed.\n");
        return -2;
    }

    // Getting array of players
    GVariant *reply_child = g_variant_get_child_value(reply, 0);
    gsize reply_count;
    const gchar **names = g_variant_get_strv(reply_child, &reply_count);

    for (gsize i = 0; i < reply_count; i += 1) {
        if (g_str_has_prefix(names[i], "org.mpris.MediaPlayer2.")) {
            if (get_playback_status(names[i]) == 1)
            {
                if (get_metadata(names[i], mediainfo, bytes/sizeof(char)) != 0)
                {
                    fprintf(stderr, "Error: Could not get mediainfo.\n");
                    return -3;
                }
                break;
            }
        }
    }
    
    g_object_unref(proxy);
    g_variant_unref(reply);
    g_variant_unref(reply_child);
    g_free(names);
    g_free(tmp_error);

    seteuid(0);
    return 0;
}

int get_playback_status(const char *player)
{
    GError *tmp_error = NULL;
    int found = 0;

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

    GVariant *parameters;
    GVariant *parameters_array[2];

    parameters_array[0] = g_variant_new("s", "org.mpris.MediaPlayer2.Player");
    g_variant_ref(parameters_array[0]);
    parameters_array[1] = g_variant_new("s", "PlaybackStatus");
    g_variant_ref(parameters_array[1]);
    parameters = g_variant_new_tuple(parameters_array, 2);
    g_variant_ref(parameters);
    
    GVariant *reply = g_dbus_proxy_call_sync(
        proxy, "Get", parameters, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &tmp_error);

    if (tmp_error != NULL) {
        g_object_unref(proxy);
        return -2;
    }

    // printf("PlaybackStatus: %s\n", ((char*)g_variant_get_data(reply)));
    if (strcmp((char*)g_variant_get_data(reply), "Playing") == 0)
    {
        found = 1;
    }
    

    g_object_unref(proxy);
    g_variant_unref(reply);
    g_variant_unref(parameters);
    g_variant_unref(parameters_array[0]);
    g_variant_unref(parameters_array[1]);
    g_free(tmp_error);

    return found;
}

int get_metadata(const char *player, char *metadata, int bytes)
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

    GVariant *parameters;
    GVariant *parameters_array[2];

    parameters_array[0] = g_variant_new("s", "org.mpris.MediaPlayer2.Player");
    g_variant_ref(parameters_array[0]);
    parameters_array[1] = g_variant_new("s", "Metadata");
    g_variant_ref(parameters_array[1]);
    parameters = g_variant_new_tuple(parameters_array, 2);
    g_variant_ref(parameters);
    
    GVariant *reply = g_dbus_proxy_call_sync(
        proxy, "Get", parameters, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &tmp_error);

    if (tmp_error != NULL) {
        g_object_unref(proxy);
        return -2;
    }

    GVariant *props;

    g_variant_get(reply, "(v)", &props);
    gchar **artists = NULL, *artist = NULL, *title = NULL;
    g_variant_lookup(props, "xesam:artist", "^a&s", &artists);
	g_variant_lookup(props, "xesam:title", "s", &title);

    // TODO: Make it work with multiple artists
    if (artists && title)
    {
        artist = g_strjoinv(", ", artists);
        snprintf(metadata, bytes/sizeof(char), "%s - %s", (char*)artist, (char*)title);
    }

    // if (artist)
    // {
    //     g_printf("Artists: %s\n", g_strjoinv(", ", artists));
    // }
    // if (title)
    // {
    //     g_printf("Title: %s\n", title);
    // }

    g_object_unref(proxy);
    g_variant_unref(reply);
    g_variant_unref(props);
    g_variant_unref(parameters);
    g_variant_unref(parameters_array[0]);
    g_variant_unref(parameters_array[1]);
    g_free(artist);
    g_free(artists);
	g_free(title);
    g_free(tmp_error);
    
    return 0;
}