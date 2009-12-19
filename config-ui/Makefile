CC = gcc
CFLAGS = -Wall -Os $(EXTRA_CFLAGS)
CFLAGS_PLUGIN = -fPIC
CPPFLAGS = -I../ `pkg-config --cflags gtk+-2.0`
CPPFLAGS_HILDON = -DHILDON `pkg-config --cflags hildon-1`
CPPFLAGS_PLUGIN = $(CPPFLAGS_HILDON) -DHILDON_CP_APPLET \
	`pkg-config --cflags libosso` `pkg-config --cflags hildon-control-panel`
LDFLAGS = `pkg-config --libs gtk+-2.0`
LDFLAGS_HILDON = `pkg-config --libs hildon-1`
LDFLAGS_PLUGIN = -shared $(LDFLAGS_HILDON) \
	`pkg-config --libs libosso` `pkg-config --libs hildon-control-panel`
PREFIX = /usr

other_obj = ../configfile.o

APP = browser-switchboard-cp
app_obj = $(APP).app.o $(other_obj)
HILDON_APP = $(APP)-hildon
happ_obj = $(APP).happ.o $(other_obj)
PLUGIN = lib$(APP).so
plugin_obj = $(APP).plugin.o ../configfile.plugin.o

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

strip: strip-plugin
strip-plugin: plugin
	strip $(PLUGIN)

install: install-plugin
install-plugin: plugin
	mkdir -p $(DESTDIR)$(PREFIX)/lib/hildon-control-panel
	mkdir -p $(DESTDIR)$(PREFIX)/share/applications/hildon-control-panel
	install -c -m 0755 $(PLUGIN) $(DESTDIR)$(PREFIX)/lib/hildon-control-panel
	install -c -m 0644 $(APP).desktop $(DESTDIR)$(PREFIX)/share/applications/hildon-control-panel

clean:
	rm -f $(APP) $(HILDON_APP) $(PLUGIN) *.o ../configfile.o

.PHONY: strip strip-plugin install install-plugin plugin app hildon-app
