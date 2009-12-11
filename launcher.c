/*
 * launcher.c -- functions for launching web browsers for browser-switchboard
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
#include <sys/types.h>
#include <sys/wait.h>
#include <dbus/dbus-glib.h>

#include "browser-switchboard.h"
#include "launcher.h"
#include "dbus-server-bindings.h"

#define LAUNCH_DEFAULT_BROWSER launch_tear

static void launch_tear(struct swb_context *ctx, char *uri) {
	int status;
	static DBusGProxy *tear_proxy = NULL;
	GError *error = NULL;
	pid_t pid;

	if (!uri)
		uri = "new_window";

	printf("launch_tear with uri '%s'\n", uri);

	status = system("pidof tear > /dev/null");
	if (WIFEXITED(status) && !WEXITSTATUS(status)) {
		if (!tear_proxy)
			tear_proxy = dbus_g_proxy_new_for_name(ctx->session_bus,
				       	"com.nokia.tear", "/com/nokia/tear",
					"com.nokia.Tear");
		dbus_g_proxy_call(tear_proxy, "OpenAddress", &error,
				  G_TYPE_STRING, uri, G_TYPE_INVALID);
		if (!ctx->continuous_mode)
			exit(0);
	} else {
		if (ctx->continuous_mode) {
			if ((pid = fork()) != 0) {
				/* Parent process or error in fork() */
				printf("child: %d\n", (int)pid);
				return;
			}
			/* Child process */
			setsid();
		}
		execl("/usr/bin/tear", "/usr/bin/tear", uri, (char *)NULL);
	}
}

void launch_microb(struct swb_context *ctx, char *uri) {
	int kill_browserd = 0;
	int status;
	pid_t pid;

	if (!uri)
		uri = "new_window";

	status = system("pidof /usr/sbin/browserd > /dev/null");
	if (WIFEXITED(status) && WEXITSTATUS(status)) {
		kill_browserd = 1;
		system("/usr/sbin/browserd -d");
	}

	dbus_release_osso_browser_name(ctx);

	if ((pid = fork()) == -1) {
		perror("fork");
		exit(1);
	}
	if (pid > 0) {
		/* Parent process */
		waitpid(pid, &status, 0);
	} else {
		/* Child process */
		if (!strcmp(uri, "new_window")) {
			execl("/usr/bin/maemo-invoker",
			      "browser", (char *)NULL);
		} else {
			execl("/usr/bin/maemo-invoker",
			      "browser", "--url", uri, (char *)NULL);
		}
	}

	if (kill_browserd)
		system("kill `pidof /usr/sbin/browserd`");

	if (!ctx || !ctx->continuous_mode) 
		exit(0);

	dbus_request_osso_browser_name(ctx);
}

static void launch_other_browser(struct swb_context *ctx, char *uri) {
	char *command;
	char *quoted_uri, *quote;

	size_t cmdlen, urilen;
	size_t quoted_uri_size;
	size_t offset;

	if (!uri || !strcmp(uri, "new_window"))
		uri = "";
	urilen = strlen(uri);
	if (urilen > 0) {
		/* Quote the URI */
		/* urilen+3 = length of URI + 2x \' + \0 */
		if (!(quoted_uri = calloc(urilen+3, sizeof(char))))
			exit(1);
		snprintf(quoted_uri, urilen+3, "'%s'", uri);

		/* If there are any 's in the original URI, URL-escape them
		   (replace them with %27) */
		quoted_uri_size = urilen + 3;
		quote = quoted_uri + 1;
		while ((quote = strchr(quote, '\'')) &&
		       (offset = quote-quoted_uri) < strlen(quoted_uri)-1) {
			/* Check to make sure we don't shrink the memory area
			   as a result of integer overflow */
			if (quoted_uri_size+2 <= quoted_uri_size)
				exit(1);

			/* Grow the memory area;
			   2 = strlen("%27")-strlen("'") */
			if (!(quoted_uri = realloc(quoted_uri,
						   quoted_uri_size+2)))
				exit(1);
			quoted_uri_size = quoted_uri_size + 2;

			/* Recalculate the location of the ' character --
			   realloc() may have moved the string in memory */
			quote = quoted_uri + offset;

			/* Move the string after the ', including the \0,
			   over two chars */
			memmove(quote+3, quote+1, strlen(quote)+1);
			memcpy(quote, "%27", 3);
			quote = quote + 3;
		}
		urilen = strlen(quoted_uri);
	} else
		quoted_uri = uri;

	cmdlen = strlen(ctx->other_browser_cmd);

	/* cmdlen+urilen+1 is normally two bytes longer than we need (uri will
	   replace "%s"), but is needed in the case other_browser_cmd has no %s
	   and urilen < 2 */
	if (!(command = calloc(cmdlen+urilen+1, sizeof(char))))
		exit(1);
	snprintf(command, cmdlen+urilen+1, ctx->other_browser_cmd, quoted_uri);
	printf("command: '%s'\n", command);

	if (ctx->continuous_mode) {
		if (fork() != 0) {
			/* Parent process or error in fork() */
			if (urilen > 0)
				free(quoted_uri);
			free(command);	
			return;
		}
		/* Child process */
		setsid();
	}
	execl("/bin/sh", "/bin/sh", "-c", command, (char *)NULL);
}

static void use_other_browser_cmd(struct swb_context *ctx, char *cmd) {
	size_t len = strlen(cmd);

	free(ctx->other_browser_cmd);
	ctx->other_browser_cmd = calloc(len+1, sizeof(char));
	if (!ctx->other_browser_cmd) {
		printf("malloc failed!\n");
		ctx->default_browser_launcher = LAUNCH_DEFAULT_BROWSER;
	} else {
		ctx->other_browser_cmd = strncpy(ctx->other_browser_cmd,
						 cmd, len+1);
		ctx->default_browser_launcher = launch_other_browser;
	}
}

void update_default_browser(struct swb_context *ctx, char *default_browser) {
	if (!ctx)
		return;

	if (!default_browser) {
		ctx->default_browser_launcher = LAUNCH_DEFAULT_BROWSER;
		return;
	}

	if (!strcmp(default_browser, "tear"))
		ctx->default_browser_launcher = launch_tear;
	else if (!strcmp(default_browser, "microb"))
		ctx->default_browser_launcher = launch_microb;
	else if (!strcmp(default_browser, "fennec"))
		use_other_browser_cmd(ctx, "fennec %s");
	else if (!strcmp(default_browser, "midori"))
		use_other_browser_cmd(ctx, "midori %s");
	else if (!strcmp(default_browser, "other")) {
		if (ctx->other_browser_cmd)
			ctx->default_browser_launcher = launch_other_browser;
		else {
			printf("default_browser is 'other', but no other_browser_cmd set -- using default\n");
			ctx->default_browser_launcher = LAUNCH_DEFAULT_BROWSER;
		}
	} else {
		printf("Unknown default_browser %s, using default", default_browser);
		ctx->default_browser_launcher = LAUNCH_DEFAULT_BROWSER;
	}
}

void launch_browser(struct swb_context *ctx, char *uri) {
	if (ctx && ctx->default_browser_launcher)
		ctx->default_browser_launcher(ctx, uri);
}
