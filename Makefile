CC = gcc
CFLAGS = -Wall -Os -mcpu=arm1136jf-s -mthumb
CPPFLAGS = `pkg-config --cflags dbus-glib-1`
LDFLAGS = `pkg-config --libs dbus-glib-1`
PREFIX = /usr/local

APP = browser-switchboard
obj = main.o launcher.o dbus-server-bindings.o

all: $(APP)

$(APP): dbus-server-glue.h $(obj)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(APP) $(obj)

dbus-server-glue.h:
	dbus-binding-tool --mode=glib-server --prefix="osso_browser" \
	    dbus-server-glue.xml > dbus-server-glue.h

strip: $(APP)
	strip $(APP)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -c -m 0755 browser-switchboard $(DESTDIR)$(PREFIX)/bin

clean:
	rm -f $(APP) *.o dbus-server-glue.h

.PHONY: strip install
