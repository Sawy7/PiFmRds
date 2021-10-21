#include "dbus-reader.h"

// Code snippets from:
// https://github.com/versi786/dbus-audio-controller/blob/master/main.cpp
// https://gist.github.com/grawity/5cfba06c70addcbcabfe

static GMainLoop *loop = NULL;
char metadata_text[65] = "NO MEDIA";

int GetPlaybackStatus(const char *player)
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
    
    // printf("PlaybackStatus: %s\n", ((char*)g_variant_get_data(reply)));

    g_object_unref(proxy);
    g_variant_unref(reply);
    g_variant_unref(parameters);
    g_variant_unref(parameters_array[0]);
    g_variant_unref(parameters_array[1]);

    return playbackReturn;
}

GError *tmp_error;
GDBusProxy *get_names_proxy;
GDBusProxy *get_props_proxy;

int NewDBus()
{
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
        fprintf(stderr, "Could not connect to DBus.\n");
        return -1;
    }

    return 0;
}

int GetMetaCombined()
{
    // Call method (listing all players)
    GVariant *reply = g_dbus_proxy_call_sync(get_names_proxy, "ListNames", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &tmp_error);

    // Check for errors
    if (tmp_error != NULL) {
        g_object_unref(get_names_proxy);
        fprintf(stderr, "DBus method call failed.\n");
        return -2;
    }

    // Getting array of players
    GVariant *reply_child = g_variant_get_child_value(reply, 0);
    gsize reply_count;
    const gchar **names = g_variant_get_strv(reply_child, &reply_count);
   
    for (gsize i = 0; i < reply_count; i += 1) {
        if (g_str_has_prefix(names[i], "org.mpris.MediaPlayer2.")) {
            if (GetPlaybackStatus(names[i]) == 0)
            {
                printf("%s is playing.\n", names[i]);
                // Hook into this player
                AddSignal(names[i]);
            }
        }
    }

    g_variant_unref(reply);
    g_variant_unref(reply_child);
    g_free(names);
}

int AddSignal(const char *player)
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

void on_signal (GDBusProxy *proxy,
           gchar      *sender_name,
           gchar      *signal_name,
           GVariant   *parameters,
           gpointer    user_data)
{
    int exit_loop = 0;
    gchar *parameters_str;
    GVariant *test;

    

    GVariant *child = g_variant_get_child_value(parameters, 1);
    if (g_variant_lookup(child, "PlaybackStatus", "s", &parameters_str))
    {
        if (strcmp((char*)parameters_str, "Paused") == 0)
        {
            exit_loop = 1;
        }

        g_free (parameters_str);
    }
    else if (g_variant_lookup(child, "Metadata", "a{sv}", &parameters_str))
    {
        GVariant *metadata = g_variant_lookup_value(child, "Metadata", G_VARIANT_TYPE_DICTIONARY);
        
        // parameters_str = g_variant_print(metadata, TRUE);
        // g_print ("%s\n", parameters_str);

        // gchar **artists = NULL, *title = NULL;
        // g_variant_lookup(metadata, "xesam:artist", "^a&s", &artists);
        // g_variant_lookup(metadata, "xesam:title", "s", &title);

        // if (artists)
        // {
        //     g_printf("Artists: %s\n", g_strjoinv(", ", artists));
        // }
        // if (title)
        // {
        //     g_printf("Title: %s\n", title);
        // }

        export_metadata(metadata);

        g_variant_unref(metadata);
        // g_free(artists);
        // g_free(title);
    }
    
    g_variant_unref(child);

    if (exit_loop)
    {
        printf("Exiting loop\n");
        g_main_loop_quit(loop);
    }
    
}

void export_metadata(GVariant *metadata)
{
    gchar **artists = NULL, *title = NULL;
    g_variant_lookup(metadata, "xesam:artist", "^a&s", &artists);
    g_variant_lookup(metadata, "xesam:title", "s", &title);

    snprintf(metadata_text, sizeof(metadata_text), "%s - %s", g_strjoinv(", ", artists), title);

    g_free(artists);
    g_free(title);
}

void on_name_owner_notify (GObject    *object,
                      GParamSpec *pspec,
                      gpointer    user_data)
{
    printf("Exiting loop\n");
    g_main_loop_quit(loop);
}

void *thread_main(void *userdata)
{
    NewDBus();
    while (1)
    {
        GetMetaCombined();
        sleep(3);
    }
}

int main()
{
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, thread_main, NULL);

    while (1)
    {
        printf("Main thread: %s\n", metadata_text);
        sleep(5);
    }
    
    return 0;
}