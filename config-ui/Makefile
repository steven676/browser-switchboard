CC = gcc
CFLAGS = -Wall -Os -mcpu=arm1136jf-s -mthumb
CFLAGS_PLUGIN = -fPIC
CPPFLAGS = `pkg-config --cflags gtk+-2.0`
CPPFLAGS_HILDON = -DHILDON `pkg-config --cflags hildon-1`
CPPFLAGS_PLUGIN = $(CPPFLAGS_HILDON) -DHILDON_CP_APPLET \
	`pkg-config --cflags dbus-1` `pkg-config --cflags hildon-control-panel`
LDFLAGS = `pkg-config --libs gtk+-2.0`
LDFLAGS_HILDON = `pkg-config --libs hildon-1`
LDFLAGS_PLUGIN = -shared $(LDFLAGS_HILDON) \
	`pkg-config --libs dbus-1` `pkg-config --libs hildon-control-panel`
PREFIX = /usr/local

APP = browser-switchboard-cp
app_obj = $(APP).app.o
HILDON_APP = $(APP)-hildon
happ_obj = $(APP).happ.o
PLUGIN = lib$(APP).so
plugin_obj = $(APP).plugin.o

all: plugin
plugin: $(PLUGIN)
app: $(APP)
hildon-app: $(HILDON_APP)

$(APP): $(app_obj)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(APP) $(app_obj)

%.app.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(HILDON_APP): $(happ_obj)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LDFLAGS_HILDON) \
		-o $(HILDON_APP) $(happ_obj)

%.happ.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(CPPFLAGS_HILDON) -c -o $@ $<

$(PLUGIN): $(plugin_obj)
	$(CC) $(CFLAGS) $(CFLAGS_PLUGIN) $(LDFLAGS) $(LDFLAGS_PLUGIN) \
		-o $(PLUGIN) $(plugin_obj)

%.plugin.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_PLUGIN) $(CPPFLAGS) $(CPPFLAGS_PLUGIN) \
		-c -o $@ $<

strip: $(PLUGIN)
	strip $(PLUGIN)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/share/hildon-control-panel
	install -c -m 0755 $(PLUGIN) $(DESTDIR)$(PREFIX)/share/hildon-control-panel

clean:
	rm -f $(APP) $(HILDON_APP) $(PLUGIN) *.o

.PHONY: strip install plugin app hildon-app
