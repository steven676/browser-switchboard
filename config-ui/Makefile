CC = gcc
CFLAGS = -Wall -Os -mcpu=arm1136jf-s -mthumb
CFLAGS_LIB = -fPIC
CPPFLAGS = `pkg-config --cflags gtk+-2.0`
CPPFLAGS_LIB = -DHILDON -DHILDON_CP_APPLET
LDFLAGS = `pkg-config --libs gtk+-2.0`
LDFLAGS_LIB = -shared
PREFIX = /usr/local

APP = browser-switchboard-cp
app_obj = $(APP).app.o
LIB = lib$(APP).so
lib_obj = $(APP).lib.o

all: $(LIB)
lib: $(LIB)
app: $(APP)

$(APP): $(app_obj)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(APP) $(app_obj)

%.app.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(LIB): $(lib_obj)
	$(CC) $(CFLAGS) $(CFLAGS_LIB) $(LDFLAGS) $(LDFLAGS_LIB) -o $(LIB) $(lib_obj)

%.lib.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_LIB) $(CPPFLAGS) $(CPPFLAGS_LIB) -c -o $@ $<

strip: $(LIB)
	strip $(LIB)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/share/hildon-control-panel
	install -c -m 0755 $(LIB) $(DESTDIR)$(PREFIX)/share/hildon-control-panel

clean:
	rm -f $(APP) $(LIB) *.o

.PHONY: strip install lib app
