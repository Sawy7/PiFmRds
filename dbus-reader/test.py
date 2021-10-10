import dbus

bus = dbus.SessionBus()
vlc_media_player_obj = bus.get_object("org.mpris.MediaPlayer2.vlc", "/org/mpris/MediaPlayer2")
props_iface = dbus.Interface(vlc_media_player_obj, 'org.freedesktop.DBus.Properties')
pb_stat = props_iface.Get('org.mpris.MediaPlayer2.Player', 'PlaybackStatus')

print(pb_stat)