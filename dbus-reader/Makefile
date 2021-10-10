CFLAGS = `pkg-config --cflags dbus-1 dbus-glib-1 glib-2.0 gio-2.0`
LIBS = `pkg-config --libs dbus-1 dbus-glib-1 glib-2.0 gio-2.0`

all: dbus-reader

dbus-reader: dbus-reader.c dbus-reader.h
	gcc \
	-pthread \
	-I/usr/include/dbus-1.0 \
	-I/usr/lib/arm-linux-gnueabihf/dbus-1.0/include \
	-I/usr/include/libmount \
	-I/usr/include/blkid \
	-I/usr/include/uuid \
	-I/usr/include/glib-2.0 \
	-I/usr/lib/arm-linux-gnueabihf/glib-2.0/include \
	$^ -o $@ \
	-ldbus-glib-1 \
	-ldbus-1 \
	-lgio-2.0 \
	-lgobject-2.0 \
	-lglib-2.0


deps:
	sudo apt-get -y install dbus libdbus-1-dev libdbus-glib-1-2 libdbus-glib-1-dev

clean:
	rm dbus-reader