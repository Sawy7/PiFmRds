CC = gcc
STD_CFLAGS = -Wall -std=gnu99 -c -g

# Enable ARM-specific options only on ARM, and compilation of the app only on ARM
UNAME := $(shell uname -m)

# Determine Raspberry Pi version (if 2 or greater)
RPI_VERSION := $(shell cat /proc/device-tree/model | grep -a -o "Raspberry\sPi\s[0-9]\|Raspberry\sPi\sZero\s[0-9]" | grep -o "[0-9]")

# Determine the hardware platform and set proper compilation flags
ifeq ($(UNAME), armv6l)
	ARCH_CFLAGS = -march=armv6 -O3 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp -ffast-math
	TARGET = 1
else ifeq ($(shell expr $(RPI_VERSION) \> 1), 1)
	ifeq ($(UNAME), armv7l)
		ARCH_CFLAGS = -march=armv7-a -O3 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp -ffast-math
	else ifeq ($(UNAME), aarch64)
		ARCH_CFLAGS = -march=armv8-a -O2 -pipe -fstack-protector-strong -fno-plt -ffast-math
	endif
	ifeq ($(shell expr $(RPI_VERSION) \>= 4), 1)
		TARGET = 4
	else
		TARGET = 2
	endif
else
	ARCH_CFLAGS = -O3
	TARGET = other
endif
CFLAGS = $(STD_CFLAGS) $(ARCH_CFLAGS) -DRASPI=$(TARGET)

ifneq ($(TARGET), other)

app: rds.o waveforms.o pi_fm_rds.o fm_mpx.o control_pipe.o mailbox.o pulse_module.o dbus_mediainfo.o
	$(CC) \
	-I/usr/include/dbus-1.0 \
	-I/usr/lib/arm-linux-gnueabihf/dbus-1.0/include \
	-I/usr/include/libmount \
	-I/usr/include/blkid \
	-I/usr/include/uuid \
	-I/usr/include/glib-2.0 \
	-I/usr/lib/arm-linux-gnueabihf/glib-2.0/include \
	-o pi_fm_rds $^ \
	-ldbus-glib-1 \
	-ldbus-1 \
	-lgio-2.0 \
	-lgobject-2.0 \
	-lglib-2.0 \
	-lm -lsndfile -lpulse -lpthread
	sudo chown root pi_fm_rds
	sudo chmod +s pi_fm_rds

endif


rds_wav: rds.o waveforms.o rds_wav.o fm_mpx.o
	$(CC) -o rds_wav $^ -lm -lsndfile

rds.o: rds.c waveforms.h
	$(CC) $(CFLAGS) $<

control_pipe.o: control_pipe.c control_pipe.h rds.h
	$(CC) $(CFLAGS) $<

waveforms.o: waveforms.c waveforms.h
	$(CC) $(CFLAGS) $<

mailbox.o: mailbox.c mailbox.h
	$(CC) $(CFLAGS) $<

pi_fm_rds.o: pi_fm_rds.c control_pipe.h fm_mpx.h rds.h mailbox.h
	$(CC) $(CFLAGS) $<

rds_wav.o: rds_wav.c
	$(CC) $(CFLAGS) $<

fm_mpx.o: fm_mpx.c fm_mpx.h
	$(CC) $(CFLAGS) $<

pulse_module.o: pulse_module.c pulse_module.h
	$(CC) $(CFLAGS) $<

dbus_mediainfo.o: dbus_mediainfo.c dbus_mediainfo.h
	$(CC) $(CFLAGS) \
	-I/usr/include/dbus-1.0 \
	-I/usr/lib/arm-linux-gnueabihf/dbus-1.0/include \
	-I/usr/include/libmount \
	-I/usr/include/blkid \
	-I/usr/include/uuid \
	-I/usr/include/glib-2.0 \
	-I/usr/lib/arm-linux-gnueabihf/glib-2.0/include \
	$< \
	-ldbus-glib-1 \
	-ldbus-1 \
	-lgio-2.0 \
	-lgobject-2.0 \
	-lglib-2.0

deps:
	sudo apt update
	sudo apt -y upgrade
	sudo apt -y install git wget libpulse-dev libsndfile1-dev libdbus-1-dev libdbus-glib-1-2 libdbus-glib-1-dev debhelper

makedeb: app
	cd ../deb-package/debhelper; \
	./build.sh

install: makedeb
	sudo dpkg -i ../deb-package/pifmrds_1.0.0_armhf.deb
	sudo apt --fix-broken install -y

clean:
	rm -f *.o pi_fm_rds
