/*
 * main.c -- config file parsing and main loop for browser-switchboard
 *
 * Copyright (C) 2009 Steven Luo
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
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <regex.h>
#include <dbus/dbus-glib.h>

#include "browser-switchboard.h"
#include "launcher.h"
#include "dbus-server-bindings.h"

#define DEFAULT_HOMEDIR "/home/user"
#define CONFIGFILE_LOC "/.config/browser-switchboard"
#define MAXLINE 1024

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
	char *homedir, *configfile;
	size_t len;
	char buf[MAXLINE];
	char *key, *value;
	char *default_browser = NULL;
	FILE *fp;
	regex_t re_ignore, re_config1, re_config2;
	regmatch_t substrs[3];

	set_config_defaults(&ctx);

	if (!(homedir = getenv("HOME")))
		homedir = DEFAULT_HOMEDIR;
	len = strlen(homedir) + strlen(CONFIGFILE_LOC) + 1;
	if (!(configfile = calloc(len, sizeof(char))))
		goto out_noopen;
	snprintf(configfile, len, "%s%s", homedir, CONFIGFILE_LOC);

	if (!(fp = fopen(configfile, "r")))
		goto out_noopen;

	/* compile regex matching blank lines or comments */
	if (regcomp(&re_ignore, "^[[:space:]]*(#|$)", REG_EXTENDED|REG_NOSUB))
		goto out_nore;
	/* compile regex matching foo = "bar", with arbitrary whitespace at
	   beginning and end of line and surrounding the = */
	if (regcomp(&re_config1,
		    "^[[:space:]]*([^=[:space:]]+)[[:space:]]*=[[:space:]]*\"(.*)\"[[:space:]]*$",
		    REG_EXTENDED)) {
		regfree(&re_ignore);
		goto out_nore;
	}
	/* compile regex matching foo = bar, with arbitrary whitespace at
	   beginning of line and surrounding the = */
	if (regcomp(&re_config2,
		    "^[[:space:]]*([^=[:space:]]+)[[:space:]]*=[[:space:]]*(.*)$",
		    REG_EXTENDED|REG_NEWLINE)) {
		regfree(&re_ignore);
		regfree(&re_config1);
		goto out_nore;
	}

	/* Read in the config file one line at a time and parse it
	   XXX doesn't deal with lines longer than MAXLINE */
	while (fgets(buf, MAXLINE, fp)) {
		/* skip blank lines and comments */
		if (!regexec(&re_ignore, buf, 0, NULL, 0))
			continue;

		/* Find the substrings corresponding to the key and value
		   If the line doesn't match our idea of a config file entry,
		   skip it */
		if (regexec(&re_config1, buf, 3, substrs, 0) &&
		    regexec(&re_config2, buf, 3, substrs, 0))
			continue;
		if (substrs[1].rm_so == -1 || substrs[2].rm_so == -1)
			continue;

		/* copy the config value into a new string */
		len = substrs[2].rm_eo - substrs[2].rm_so;
		if (!(value = calloc(len+1, sizeof(char))))
			goto out;
		strncpy(value, buf+substrs[2].rm_so, len);
		/* calloc() zeroes the memory, so string is automatically
		   null terminated */

		/* make key point to a null-terminated string holding the 
		   config key */
		key = buf + substrs[1].rm_so;
		buf[substrs[1].rm_eo] = '\0';

		if (!strcmp(key, "continuous_mode")) {
			ctx.continuous_mode = atoi(value);
			free(value);
		} else if (!strcmp(key, "default_browser")) {
			if (!default_browser)
				default_browser = value;
		} else if (!strcmp(key, "other_browser_cmd")) {
			if (!ctx.other_browser_cmd)
				ctx.other_browser_cmd = value;
		} else {
			/* Don't need this line's contents */
			free(value);
		}
		value = NULL;
	}

	printf("continuous_mode: %d\n", ctx.continuous_mode);
	printf("default_browser: '%s'\n", default_browser?default_browser:"NULL");
	printf("other_browser_cmd: '%s'\n", ctx.other_browser_cmd?ctx.other_browser_cmd:"NULL");

out:
	regfree(&re_ignore);
	regfree(&re_config1);
	regfree(&re_config2);
out_nore:
	fclose(fp);
out_noopen:
	update_default_browser(&ctx, default_browser);
	free(configfile);
	free(default_browser);
	return;
}

int main() {
	OssoBrowser *obj;
	GMainLoop *mainloop;
	GError *error = NULL;

	read_config(0);

	if (ctx.continuous_mode) {
		struct sigaction act;
		act.sa_flags = SA_RESTART;

		act.sa_handler = waitforzombies;
		if (sigaction(SIGCHLD, &act, NULL) == -1) {
			printf("Installing signal handler failed\n");
			return 1;
		}

		act.sa_handler = read_config;
		if (sigaction(SIGHUP, &act, NULL) == -1) {
			printf("Installing signal handler failed\n");
			return 1;
		}
	}

	g_type_init();

	dbus_g_object_type_install_info(OSSO_BROWSER_TYPE,
			&dbus_glib_osso_browser_object_info);

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

	dbus_request_osso_browser_name(&ctx);

	obj = g_object_new(OSSO_BROWSER_TYPE, NULL);
	dbus_g_connection_register_g_object(ctx.session_bus,
			"/com/nokia/osso_browser", G_OBJECT(obj));
	dbus_g_connection_register_g_object(ctx.session_bus,
			"/com/nokia/osso_browser/request", G_OBJECT(obj));

	mainloop = g_main_loop_new(NULL, FALSE);
	printf("Starting main loop\n");
	g_main_loop_run(mainloop);
	printf("Main loop completed\n");

	return 0;
}
