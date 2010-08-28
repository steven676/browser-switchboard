CC = gcc
CFLAGS = -Wall -Os $(EXTRA_CFLAGS)
CFLAGS_PLUGIN = -fPIC
CPPFLAGS = -I../ `pkg-config --cflags gtk+-2.0` $(EXTRA_CPPFLAGS)
CPPFLAGS_HILDON = -DHILDON `pkg-config --cflags hildon-1`
CPPFLAGS_PLUGIN = $(CPPFLAGS_HILDON) -DHILDON_CP_APPLET \
	`pkg-config --cflags libosso` `pkg-config --cflags hildon-control-panel`
LDFLAGS = -Wl,--as-needed `pkg-config --libs gtk+-2.0` $(EXTRA_LDFLAGS)
LDFLAGS_HILDON = `pkg-config --libs hildon-1`
LDFLAGS_PLUGIN = -shared $(LDFLAGS_HILDON) \
	`pkg-config --libs libosso` `pkg-config --libs hildon-control-panel`
PREFIX = /usr

other_obj = ../configfile.o ../config.o save-config.o

APP = browser-switchboard-cp
UTIL = browser-switchboard-config
app_obj = $(APP).app.o $(other_obj)
util_obj = $(UTIL).o $(other_obj)
HILDON_APP = $(APP)-hildon
happ_obj = $(APP).happ.o $(other_obj)
PLUGIN = lib$(APP).so
plugin_obj = $(APP).plugin.o ../configfile.plugin.o ../config.plugin.o save-config.plugin.o

all:
	@echo 'Usage:'
	@echo '    make app -- build standalone GTK+ application'
	@echo '    make util -- build command-line configuration utility'
	@echo '    make diablo-hildon-app -- build standalone Diablo Hildon application'
	@echo '    make diablo-plugin -- build Diablo hildon-control-panel plugin'
	@echo '    make fremantle-util -- build command-line configuration utility for Fremantle'
	@echo '    make fremantle-hildon-app -- build standalone Fremantle Hildon application'
	@echo '    make fremantle-plugin -- build Fremantle hildon-control-panel plugin'
app: $(APP)
util: $(UTIL)
diablo-hildon-app: $(HILDON_APP)
diablo-plugin: $(PLUGIN)
fremantle-util:
	@$(MAKE) EXTRA_CPPFLAGS='-DFREMANTLE $(EXTRA_CPPFLAGS)' $(UTIL)
fremantle-hildon-app:
	@$(MAKE) EXTRA_CPPFLAGS='-DFREMANTLE $(EXTRA_CPPFLAGS)' $(HILDON_APP)
fremantle-plugin:
	@$(MAKE) EXTRA_CPPFLAGS='-DFREMANTLE $(EXTRA_CPPFLAGS)' $(PLUGIN)

$(APP): $(app_obj)
	$(CC) $(CFLAGS) -o $(APP) $(app_obj) $(LDFLAGS)

%.app.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(UTIL): $(util_obj)
	$(CC) $(CFLAGS) -o $(UTIL) $(util_obj) $(LDFLAGS)

$(HILDON_APP): $(happ_obj)
	$(CC) $(CFLAGS) -o $(HILDON_APP) $(happ_obj) \
		$(LDFLAGS) $(LDFLAGS_HILDON)

%.happ.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(CPPFLAGS_HILDON) -c -o $@ $<

$(PLUGIN): $(plugin_obj)
	$(CC) $(CFLAGS) $(CFLAGS_PLUGIN) -o $(PLUGIN) $(plugin_obj) \
		$(LDFLAGS) $(LDFLAGS_PLUGIN)


%.plugin.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_PLUGIN) $(CPPFLAGS) $(CPPFLAGS_PLUGIN) \
		-c -o $@ $<

strip: strip-plugin strip-util
strip-plugin: $(PLUGIN)
	strip $(PLUGIN)
strip-util: $(UTIL)
	strip ($UTIL)

install: install-plugin install-util
install-plugin: $(PLUGIN)
	mkdir -p $(DESTDIR)$(PREFIX)/lib/hildon-control-panel
	mkdir -p $(DESTDIR)$(PREFIX)/share/applications/hildon-control-panel
	install -c -m 0755 $(PLUGIN) $(DESTDIR)$(PREFIX)/lib/hildon-control-panel
	install -c -m 0644 $(APP).desktop $(DESTDIR)$(PREFIX)/share/applications/hildon-control-panel
install-util: $(UTIL)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -c -m 0755 $(UTIL) $(DESTDIR)$(PREFIX)/bin

clean:
	rm -f $(APP) $(UTIL) $(HILDON_APP) $(PLUGIN) $(app_obj) $(util_obj) $(happ_obj) $(plugin_obj)

.PHONY: strip strip-plugin strip-util install install-plugin install-util app util diablo-hildon-app diablo-plugin fremantle-hildon-app fremantle-plugin
