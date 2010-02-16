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
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dbus/dbus-glib.h>

#include "browser-switchboard.h"
#include "launcher.h"
#include "dbus-server-bindings.h"
#include "configfile.h"

struct swb_context ctx;

static void set_config_defaults(struct swb_context *ctx) {
	if (!ctx)
		return;
	free(ctx->other_browser_cmd);
	ctx->continuous_mode = 0;
	ctx->default_browser_launcher = NULL;
	ctx->other_browser_cmd = NULL;
}

static void waitforzombies(int signalnum) {
	while (waitpid(-1, NULL, WNOHANG) > 0)
		printf("Waited for a zombie\n");
}

static void read_config(int signalnum) {
	FILE *fp;
	int continuous_mode_seen = 0;
	struct swb_config_line line;
	char *default_browser = NULL;

	set_config_defaults(&ctx);

	if (!(fp = open_config_file()))
		goto out_noopen;

	/* Parse the config file
	   TODO: should we handle errors differently than EOF? */
	if (!parse_config_file_begin())
		goto out;
	while (!parse_config_file_line(fp, &line)) {
		if (line.parsed) {
			if (!strcmp(line.key, "continuous_mode")) {
				if (!continuous_mode_seen) {
					ctx.continuous_mode = atoi(line.value);
					continuous_mode_seen = 1;
				}
				free(line.value);
			} else if (!strcmp(line.key, "default_browser")) {
				if (!default_browser)
					default_browser = line.value;
			} else if (!strcmp(line.key, "other_browser_cmd")) {
				if (!ctx.other_browser_cmd)
					ctx.other_browser_cmd = line.value;
			} else {
				/* Don't need this line's contents */
				free(line.value);
			}
		}
		free(line.key);
	}
	parse_config_file_end();

	printf("continuous_mode: %d\n", ctx.continuous_mode);
	printf("default_browser: '%s'\n", default_browser?default_browser:"NULL");
	printf("other_browser_cmd: '%s'\n", ctx.other_browser_cmd?ctx.other_browser_cmd:"NULL");

out:
	fclose(fp);
out_noopen:
	update_default_browser(&ctx, default_browser);
	free(default_browser);
	return;
}

int main() {
	OssoBrowser *obj_osso_browser, *obj_osso_browser_req;
	GMainLoop *mainloop;
	GError *error = NULL;
	int reqname_result;

	read_config(0);

	if (ctx.continuous_mode) {
		/* Install signal handlers */
		struct sigaction act;
		act.sa_flags = SA_RESTART;
		sigemptyset(&(act.sa_mask));

		/* SIGCHLD -- clean up after zombies */
		act.sa_handler = waitforzombies;
		if (sigaction(SIGCHLD, &act, NULL) == -1) {
			printf("Installing signal handler failed\n");
			return 1;
		}

		/* SIGHUP -- reread config file */
		act.sa_handler = read_config;
		if (sigaction(SIGHUP, &act, NULL) == -1) {
			printf("Installing signal handler failed\n");
			return 1;
		}
	}

	g_type_init();

	dbus_g_object_type_install_info(OSSO_BROWSER_TYPE,
			&dbus_glib_osso_browser_object_info);

	/* Get a connection to the D-Bus session bus */
	ctx.session_bus = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
	if (!ctx.session_bus) {
		printf("Couldn't get a D-Bus bus connection\n");
		return 1;
	}
	ctx.dbus_proxy = dbus_g_proxy_new_for_name(ctx.session_bus,
			"org.freedesktop.DBus", "/org/freedesktop/DBus",
			"org.freedesktop.DBus");
	if (!ctx.dbus_proxy) {
		printf("Couldn't get an org.freedesktop.DBus proxy\n");
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
		printf("Couldn't acquire browser-switchboard lock: %s\n",
		       error->message);
		return 1;
	}
	if (reqname_result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {  
		printf("Another browser-switchboard already running\n");
		return 1;
	}

	dbus_request_osso_browser_name(&ctx);

	/* Register ourselves to handle the osso_browser D-Bus methods */
	obj_osso_browser = g_object_new(OSSO_BROWSER_TYPE, NULL);
	obj_osso_browser_req = g_object_new(OSSO_BROWSER_TYPE, NULL);
	dbus_g_connection_register_g_object(ctx.session_bus,
			"/com/nokia/osso_browser", G_OBJECT(obj_osso_browser));
	dbus_g_connection_register_g_object(ctx.session_bus,
			"/com/nokia/osso_browser/request",
			G_OBJECT(obj_osso_browser_req));

	mainloop = g_main_loop_new(NULL, FALSE);
	printf("Starting main loop\n");
	g_main_loop_run(mainloop);
	printf("Main loop completed\n");

	return 0;
}
