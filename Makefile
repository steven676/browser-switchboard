CC = gcc
CFLAGS = -Wall -Os $(EXTRA_CFLAGS)
CPPFLAGS = `pkg-config --cflags dbus-glib-1` $(EXTRA_CPPFLAGS)
LDFLAGS = -Wl,--as-needed `pkg-config --libs dbus-glib-1` $(EXTRA_LDFLAGS)
PREFIX = /usr

APP = browser-switchboard
obj = main.o launcher.o dbus-server-bindings.o config.o configfile.o log.o

all:
	@echo 'Usage:'
	@echo '    make diablo -- build for Diablo'
	@echo '    make fremantle -- build for Fremantle'
diablo: $(APP)
fremantle:
	@$(MAKE) \
	    EXTRA_CPPFLAGS='-DFREMANTLE `pkg-config --cflags dbus-1` $(EXTRA_CPPFLAGS)' \
	    EXTRA_LDFLAGS='`pkg-config --libs dbus-1` $(EXTRA_LDFLAGS)' $(APP)


$(APP): dbus-server-glue.h $(obj)
	$(CC) $(CFLAGS) -o $(APP) $(obj) $(LDFLAGS)

dbus-server-glue.h:
	dbus-binding-tool --mode=glib-server --prefix="osso_browser" \
	    dbus-server-glue.xml > dbus-server-glue.h

strip: $(APP)
	strip $(APP)

install: $(APP)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/share/dbus-1/services
	mkdir -p $(DESTDIR)$(PREFIX)/share/applications/hildon
	install -c -m 0755 $(APP) $(DESTDIR)$(PREFIX)/bin
	install -c -m 0644 com.nokia.osso_browser.service $(DESTDIR)$(PREFIX)/share/dbus-1/services
	install -c -m 0755 browser $(DESTDIR)$(PREFIX)/bin
	install -c -m 0755 microb $(DESTDIR)$(PREFIX)/bin
	install -c -m 0644 microb.desktop $(DESTDIR)$(PREFIX)/share/applications/hildon

install-xsession-script:
	mkdir -p $(DESTDIR)/etc/X11/Xsession.post
	install -c -m 0755 xsession-post.sh $(DESTDIR)/etc/X11/Xsession.post/35browser-switchboard

clean:
	rm -f $(APP) $(obj) dbus-server-glue.h

.PHONY: strip install install-xsession-script diablo fremantle
