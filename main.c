/*
 * main.c -- config file parsing and main loop for browser-switchboard
 *
 * Copyright (C) 2009-2010 Steven Luo
 * Derived from a Python implementation by Jason Simpson and Steven Luo
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <glib.h>
#include <dbus/dbus-glib.h>

#include "browser-switchboard.h"
#include "launcher.h"
#include "dbus-server-bindings.h"
#include "config.h"
#include "log.h"

struct swb_context ctx;

static void read_config(void);

/* Wait for zombies on SIGCHLD */
static void waitforzombies(int signalnum) {
	while (waitpid(-1, NULL, WNOHANG) > 0);
}

/* Handle other signals in event loop by writing signal number to pipe */
int eventpipe[2];
static void handle_signal(int signalnum) {
	write(eventpipe[1], &signalnum, sizeof signalnum);
}

/* Callbacks for polling the event pipe in the GLib event loop */
static GPollFD fdevents_pfd;
/* Called before entering the poll() */
static gboolean fdevents_prepare(GSource *source, gint *timeout) {
	/* No timeout for poll() */
	*timeout = -1;
	return FALSE;
}
/* Check to see whether a handled event has happened */
static gboolean fdevents_check(GSource *source) {
	return !!(fdevents_pfd.revents & G_IO_IN);
}
/* Read the event from the pipe and handle it */
static gboolean fdevents_dispatch(GSource *source,
				  GSourceFunc callback, gpointer user_data) {
	int eventnum;

	if (read(eventpipe[0], &eventnum, sizeof eventnum) == (sizeof eventnum)) {
		/* Handle the event passed to us */
		switch (eventnum) {
		  /* SIGHUP received -- reread config file */
		  case SIGHUP:
			read_config();
			break;
		  default:
			return FALSE;
		}
		return TRUE;
	} else {
		return FALSE;
	}
}
static GSourceFuncs fdevents_funcs = {
	.prepare = fdevents_prepare,
	.check = fdevents_check,
	.dispatch = fdevents_dispatch,
	.finalize = NULL,
};


static void read_config(void) {
	struct swb_config cfg;

	swb_config_init(&cfg);

	swb_config_load(&cfg);

	log_config(cfg.logging);
#ifdef FREMANTLE
	/* continuous mode is required on Fremantle */
	ctx.continuous_mode = 1;
	if (!cfg.continuous_mode)
		log_msg("continuous_mode = 0 operation no longer supported, ignoring config setting\n");
#else
	ctx.continuous_mode = cfg.continuous_mode;
#endif
	free(ctx.other_browser_cmd);
	if (cfg.other_browser_cmd) {
		if (!(ctx.other_browser_cmd = strdup(cfg.other_browser_cmd))) {
			log_perror(errno, "Failed to set other_browser_cmd");
			exit(1);
		}
	} else
		ctx.other_browser_cmd = NULL;
	update_default_browser(&ctx, cfg.default_browser);
#ifdef FREMANTLE
	ctx.autostart_microb = cfg.autostart_microb;
#endif

	log_msg("continuous_mode: %d\n", cfg.continuous_mode);
	log_msg("default_browser: '%s'\n", cfg.default_browser);
	log_msg("other_browser_cmd: '%s'\n",
		cfg.other_browser_cmd?cfg.other_browser_cmd:"NULL");
	log_msg("logging: '%s'\n", cfg.logging);

	swb_config_free(&cfg);
	return;
}

int main() {
	OssoBrowser *obj_osso_browser, *obj_osso_browser_sys;
        OssoBrowser *obj_osso_browser_req, *obj_osso_browser_sys_req;
	OssoBrowser *obj_osso_browser_root, *obj_osso_browser_sys_root;
	GMainLoop *mainloop;
	GError *error = NULL;
	int reqname_result;
	GSource *fdevents;

	read_config();

	if (ctx.continuous_mode) {
		/* Install signal handlers */
		struct sigaction act;
		act.sa_flags = SA_RESTART;
		sigemptyset(&(act.sa_mask));

		/* SIGCHLD -- clean up after zombies */
		act.sa_handler = waitforzombies;
		if (sigaction(SIGCHLD, &act, NULL) == -1) {
			log_msg("Installing signal handler failed\n");
			return 1;
		}

		/* All other signals are handled via the GLib main loop */
		if (pipe(eventpipe) == -1) {
			log_perror(errno, "Creating event pipe failed");
			return 1;
		}
		act.sa_handler = handle_signal;
		if (sigaction(SIGHUP, &act, NULL) == -1) {
			log_msg("Installing signal handler failed\n");
			return 1;
		}
	}

	g_type_init();

	dbus_g_object_type_install_info(OSSO_BROWSER_TYPE,
			&dbus_glib_osso_browser_object_info);

	/* Get a connection to the D-Bus session bus */
	ctx.session_bus = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
	if (!ctx.session_bus) {
		log_msg("Couldn't get a D-Bus bus connection\n");
		return 1;
	}
	ctx.dbus_proxy = dbus_g_proxy_new_for_name(ctx.session_bus,
			"org.freedesktop.DBus", "/org/freedesktop/DBus",
			"org.freedesktop.DBus");
	if (!ctx.dbus_proxy) {
		log_msg("Couldn't get an org.freedesktop.DBus proxy\n");
		return 1;
	}

	/* Get the org.maemo.garage.browser-switchboard name from D-Bus, as
	   a form of locking to ensure that not more than one
	   browser-switchboard process is active at any time.  With
	   DBUS_NAME_FLAG_DO_NOT_QUEUE set and DBUS_NAME_FLAG_REPLACE_EXISTING
	   not set, getting the name succeeds if and only if no other
	   process owns the name. */
	if (!dbus_g_proxy_call(ctx.dbus_proxy, "RequestName", &error,
			       G_TYPE_STRING, "org.maemo.garage.browser-switchboard",
			       G_TYPE_UINT, DBUS_NAME_FLAG_DO_NOT_QUEUE,
			       G_TYPE_INVALID,
			       G_TYPE_UINT, &reqname_result,
			       G_TYPE_INVALID)) {
		log_msg("Couldn't acquire browser-switchboard lock: %s\n",
			error->message);
		return 1;
	}
	if (reqname_result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		log_msg("Another browser-switchboard already running\n");
		return 1;
	}

	/* Get a connection to the D-Bus system bus */
	ctx.system_bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
	if (!ctx.system_bus) {
		log_msg("Couldn't get a D-Bus system bus connection\n");
		return 1;
	}
	ctx.dbus_system_proxy = dbus_g_proxy_new_for_name(ctx.system_bus,
			"org.freedesktop.DBus", "/org/freedesktop/DBus",
			"org.freedesktop.DBus");
	if (!ctx.dbus_system_proxy) {
		log_msg("Couldn't get an org.freedesktop.DBus proxy\n");
		return 1;
	}

	dbus_request_osso_browser_name(&ctx);

	/* Register ourselves to handle the osso_browser D-Bus methods */
	obj_osso_browser = g_object_new(OSSO_BROWSER_TYPE, NULL);
	obj_osso_browser_req = g_object_new(OSSO_BROWSER_TYPE, NULL);
	obj_osso_browser_root = g_object_new(OSSO_BROWSER_TYPE, NULL);
	obj_osso_browser_sys = g_object_new(OSSO_BROWSER_TYPE, NULL);
	obj_osso_browser_sys_req = g_object_new(OSSO_BROWSER_TYPE, NULL);
	obj_osso_browser_sys_root = g_object_new(OSSO_BROWSER_TYPE, NULL);
	dbus_g_connection_register_g_object(ctx.session_bus,
			"/com/nokia/osso_browser", G_OBJECT(obj_osso_browser));
	dbus_g_connection_register_g_object(ctx.session_bus,
			"/com/nokia/osso_browser/request",
			G_OBJECT(obj_osso_browser_req));
	dbus_g_connection_register_g_object(ctx.session_bus,
			"/", G_OBJECT(obj_osso_browser_root));
	dbus_g_connection_register_g_object(ctx.system_bus,
			"/com/nokia/osso_browser",
			G_OBJECT(obj_osso_browser_sys));
	dbus_g_connection_register_g_object(ctx.system_bus,
			"/com/nokia/osso_browser/request",
			G_OBJECT(obj_osso_browser_sys_req));
	dbus_g_connection_register_g_object(ctx.system_bus,
			"/", G_OBJECT(obj_osso_browser_sys_root));

	mainloop = g_main_loop_new(NULL, FALSE);

	/* Hook up event pipe to the main loop */
	if (ctx.continuous_mode) {
		fdevents = g_source_new(&fdevents_funcs, sizeof(GSource));
		g_source_set_priority(fdevents, G_PRIORITY_HIGH);
		fdevents_pfd.fd = eventpipe[0];
		fdevents_pfd.events = G_IO_IN;
		fdevents_pfd.revents = 0;
		g_source_add_poll(fdevents, &fdevents_pfd);
		g_source_attach(fdevents, NULL);
	}

	log_msg("Starting main loop\n");
	g_main_loop_run(mainloop);
	log_msg("Main loop completed\n");

	return 0;
}
