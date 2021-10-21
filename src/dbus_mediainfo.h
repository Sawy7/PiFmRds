// Forward declaration
typedef struct _GVariant GVariant;
typedef struct _GDBusProxy GDBusProxy;
typedef char gchar;
typedef void* gpointer;
typedef struct _GObject GObject;
typedef struct _GParamSpec GParamSpec;

#define METADATA_TEXT_SIZE 65

void *dbus_main(void *userdata);

int create_dbus();
int get_metadata();
int get_playback_status(const char *player);
int add_signal(const char *player);
void export_metadata(GVariant *metadata);
void on_signal (GDBusProxy *proxy, gchar *sender_name, gchar *signal_name, GVariant *parameters, gpointer user_data);
void on_name_owner_notify (GObject *object, GParamSpec *pspec, gpointer user_data);