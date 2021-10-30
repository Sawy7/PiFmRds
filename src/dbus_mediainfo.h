// Forward declaration
typedef struct _GVariant GVariant;
typedef struct _GDBusProxy GDBusProxy;
typedef char gchar;
typedef void* gpointer;
typedef struct _GObject GObject;
typedef struct _GParamSpec GParamSpec;
typedef struct _GMainLoop GMainLoop;

#define METADATA_TEXT_SIZE 64

void *dbus_main(void *userdata);

int create_dbus();
int get_metadata();
int get_playback_status(const char *player);
int add_signal(const char *player);
void export_metadata(GVariant *metadata);
void on_signal (GDBusProxy *proxy, gchar *sender_name, gchar *signal_name, GVariant *parameters, gpointer user_data);
void on_name_owner_notify (GObject *object, GParamSpec *pspec, gpointer user_data);
void quit_dbus_thread();

// Note: tried alternating RT for potential song names, but:
// 1) Text on the receiver is all kinds of broken
// 2) Most songs fit into 64 chars just fine
// 3) Radio stations (like the big ones) do it like this anyway