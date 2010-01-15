CC = gcc
CFLAGS = -Wall -Os $(EXTRA_CFLAGS)
CPPFLAGS = `pkg-config --cflags dbus-glib-1` $(EXTRA_CPPFLAGS)
LDFLAGS = `pkg-config --libs dbus-glib-1` $(EXTRA_LDFLAGS)
PREFIX = /usr

APP = browser-switchboard
obj = main.o launcher.o dbus-server-bindings.o configfile.o

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
	mkdir -p $(DESTDIR)$(PREFIX)/share/dbus-1/services
	install -c -m 0755 browser-switchboard $(DESTDIR)$(PREFIX)/bin
	install -c -m 0644 com.nokia.osso_browser.service $(DESTDIR)$(PREFIX)/share/dbus-1/services
	install -c -m 0755 browser $(DESTDIR)$(PREFIX)/bin

clean:
	rm -f $(APP) *.o dbus-server-glue.h

.PHONY: strip install
