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
#include <sys/stat.h>
#include <fcntl.h>
#include <dbus/dbus-glib.h>

#ifdef FREMANTLE
#include <dbus/dbus.h>
#include <signal.h>
#endif

#include "browser-switchboard.h"
#include "launcher.h"
#include "dbus-server-bindings.h"

#define LAUNCH_DEFAULT_BROWSER launch_microb

#ifdef FREMANTLE
static int microb_started = 0;
static int kill_microb = 0;

/* Check to see whether MicroB is ready to handle D-Bus requests yet
   See the comments in launch_microb to understand how this works. */
static DBusHandlerResult check_microb_started(DBusConnection *connection,
				     DBusMessage *message,
				     void *user_data) {
	DBusError error;
	char *name, *old, *new;

	printf("Checking to see if MicroB is ready\n");
	dbus_error_init(&error);
	if (!dbus_message_get_args(message, &error,
				   DBUS_TYPE_STRING, &name,
				   DBUS_TYPE_STRING, &old,
				   DBUS_TYPE_STRING, &new,
				   DBUS_TYPE_INVALID)) {
		printf("%s\n", error.message);
		dbus_error_free(&error);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}
	/* If old is an empty string, then the name has been acquired, and
	   MicroB should be ready to handle our request */
	if (strlen(old) == 0) {
		printf("MicroB ready\n");
		microb_started = 1;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/* Check to see whether the last MicroB window has closed
   See the comments in launch_microb to understand how this works. */
static DBusHandlerResult check_microb_finished(DBusConnection *connection,
				     DBusMessage *message,
				     void *user_data) {
	DBusError error;
	char *name, *old, *new;

	printf("Checking to see if we should kill MicroB\n");
	/* Check to make sure that the Mozilla.MicroB name is being released,
	   not acquired -- if it's being acquired, we might be seeing an event
	   at MicroB startup, in which case killing the browser isn't
	   appropriate */
	dbus_error_init(&error);
	if (!dbus_message_get_args(message, &error,
				   DBUS_TYPE_STRING, &name,
				   DBUS_TYPE_STRING, &old,
				   DBUS_TYPE_STRING, &new,
				   DBUS_TYPE_INVALID)) {
		printf("%s\n", error.message);
		dbus_error_free(&error);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}
	/* If old isn't an empty string, the name is being released, and
	   we should now kill MicroB */
	if (strlen(old) > 0)
		kill_microb = 1;

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
#endif

/* Close stdin/stdout/stderr and replace with /dev/null */
static int close_stdio(void) {
	int fd;

	if ((fd = open("/dev/null", O_RDWR)) == -1)
		return -1;

	if (dup2(fd, 0) == -1 || dup2(fd, 1) == -1 || dup2(fd, 2) == -1)
		return -1;

	close(fd);
	return 0;
}

static void launch_tear(struct swb_context *ctx, char *uri) {
	int status;
	static DBusGProxy *tear_proxy = NULL;
	GError *error = NULL;
	pid_t pid;

	if (!uri)
		uri = "new_window";

	printf("launch_tear with uri '%s'\n", uri);

	/* We should be able to just call the D-Bus service to open Tear ...
	   but if Tear's not open, that cuases D-Bus to start Tear and then
	   pass it the OpenAddress call, which results in two browser windows.
	   Properly fixing this probably requires Tear to provide a D-Bus
	   method that opens an address in an existing window, but for now work
	   around by just invoking Tear with exec() if it's not running. */
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
			close_stdio();
		}
		execl("/usr/bin/tear", "/usr/bin/tear", uri, (char *)NULL);
	}
}

void launch_microb(struct swb_context *ctx, char *uri) {
	int kill_browserd = 0;
	int status;
	pid_t pid;
#ifdef FREMANTLE
	DBusConnection *raw_connection;
	DBusError dbus_error;
	DBusHandleMessageFunction filter_func;
	DBusGProxy *g_proxy;
	GError *gerror = NULL;
#endif

	if (!uri)
		uri = "new_window";

	printf("launch_microb with uri '%s'\n", uri);

	/* Launch browserd if it's not running */
	status = system("pidof browserd > /dev/null");
	if (WIFEXITED(status) && WEXITSTATUS(status)) {
		kill_browserd = 1;
#ifdef FREMANTLE
		system("/usr/sbin/browserd -d -b > /dev/null 2>&1");
#else
		system("/usr/sbin/browserd -d > /dev/null 2>&1");
#endif
	}

	/* Release the osso_browser D-Bus name so that MicroB can take it */
	dbus_release_osso_browser_name(ctx);

	if ((pid = fork()) == -1) {
		perror("fork");
		exit(1);
	}
#ifdef FREMANTLE
	if (pid > 0) {
		/* Parent process */
		/* Wait for our child to start the browser UI process and
		   for it to acquire the com.nokia.osso_browser D-Bus name,
		   then make the appropriate method call to open the browser
		   window.

		   Ideas for how to do this monitoring derived from the
		   dbus-monitor code (tools/dbus-monitor.c in the D-Bus
		   codebase). */
		microb_started = 0;
		dbus_error_init(&dbus_error);

		raw_connection = dbus_bus_get_private(DBUS_BUS_SESSION,
						      &dbus_error);
		if (!raw_connection) {
			fprintf(stderr,
				"Failed to open connection to session bus: %s\n",
				dbus_error.message);
			dbus_error_free(&dbus_error);
			exit(1);
		}

		dbus_bus_add_match(raw_connection,
				   "type='signal',interface='org.freedesktop.DBus',member='NameOwnerChanged',arg0='com.nokia.osso_browser'",
				   &dbus_error);
		if (dbus_error_is_set(&dbus_error)) {
			fprintf(stderr,
				"Failed to set up watch for browser UI start: %s\n",
				dbus_error.message);
			dbus_error_free(&dbus_error);
			exit(1);
		}
		filter_func = check_microb_started;
		if (!dbus_connection_add_filter(raw_connection,
						filter_func, NULL, NULL)) {
			fprintf(stderr, "Failed to set up watch filter!\n");
			exit(1);
		}
		printf("Waiting for MicroB to start\n");
		while (!microb_started &&
		       dbus_connection_read_write_dispatch(raw_connection,
							   -1));
		dbus_connection_remove_filter(raw_connection,
					      filter_func, NULL);
		dbus_bus_remove_match(raw_connection,
				      "type='signal',interface='org.freedesktop.DBus',member='NameOwnerChanged',arg0='com.nokia.osso_browser'",
				      &dbus_error);
		if (dbus_error_is_set(&dbus_error)) {
			fprintf(stderr,
				"Failed to remove watch for browser UI start: %s\n",
				dbus_error.message);
			dbus_error_free(&dbus_error);
			exit(1);
		}

		/* Browser UI's started, send it the request for a new window
		   via D-Bus */
		g_proxy = dbus_g_proxy_new_for_name(ctx->session_bus,
				"com.nokia.osso_browser",
				"/com/nokia/osso_browser/request",
				"com.nokia.osso_browser");
		if (!g_proxy) {
			printf("Couldn't get a com.nokia.osso_browser proxy\n");
			exit(1);
		}
		if (!strcmp(uri, "new_window")) {
#if 0 /* Since we can't detect when the bookmark window closes, we'd have a
	 corner case where, if the user just closes the bookmark window
	 without opening any browser windows, we don't kill off MicroB or
	 resume handling com.nokia.osso_browser */
			if (!dbus_g_proxy_call(g_proxy, "top_application",
					       &gerror, G_TYPE_INVALID,
					       G_TYPE_INVALID)) {
				printf("Opening window failed: %s\n",
				       gerror->message);
				exit(1);
			}
#endif
			if (!dbus_g_proxy_call(g_proxy, "load_url",
					       &gerror,
					       G_TYPE_STRING, "about:blank",
					       G_TYPE_INVALID,
					       G_TYPE_INVALID)) {
				printf("Opening window failed: %s\n",
				       gerror->message);
				exit(1);
			}
		} else {
			if (!dbus_g_proxy_call(g_proxy, "load_url",
					       &gerror,
					       G_TYPE_STRING, uri,
					       G_TYPE_INVALID,
					       G_TYPE_INVALID)) {
				printf("Opening window failed: %s\n",
				       gerror->message);
				exit(1);
			}
		}
		g_object_unref(g_proxy);

		/* Workaround: the browser process we started is going to want
		   to hang around forever, hogging the com.nokia.osso_browser
		   D-Bus interface while at it.  To fix this, we notice that
		   when the last browser window closes, the browser UI restarts
		   its attached browserd process, which causes an observable
		   change in the ownership of the Mozilla.MicroB D-Bus name.
		   Watch for this change and kill off the browser UI process
		   when it happens.

		   This has the problem of not being able to detect whether
		   the bookmark window is open and/or in use, but it's the best
		   that I can think of.  Better suggestions would be greatly
		   appreciated. */
		kill_microb = 0;
		dbus_bus_add_match(raw_connection,
				   "type='signal',interface='org.freedesktop.DBus',member='NameOwnerChanged',arg0='Mozilla.MicroB'",
				   &dbus_error);
		if (dbus_error_is_set(&dbus_error)) {
			fprintf(stderr,
				"Failed to set up watch for browserd restart: %s\n",
				dbus_error.message);
			dbus_error_free(&dbus_error);
			exit(1);
		}
		/* Maemo 5 PR1.1 seems to have changed the name browserd takes
		   to com.nokia.microb-engine; look for this too */
		dbus_bus_add_match(raw_connection,
				   "type='signal',interface='org.freedesktop.DBus',member='NameOwnerChanged',arg0='com.nokia.microb-engine'",
				   &dbus_error);
		if (dbus_error_is_set(&dbus_error)) {
			fprintf(stderr,
				"Failed to set up watch for browserd restart: %s\n",
				dbus_error.message);
			dbus_error_free(&dbus_error);
			exit(1);
		}
		filter_func = check_microb_finished;
		if (!dbus_connection_add_filter(raw_connection,
						filter_func, NULL, NULL)) {
			fprintf(stderr, "Failed to set up watch filter!\n");
			exit(1);
		}
		while (!kill_microb &&
		       dbus_connection_read_write_dispatch(raw_connection,
							   -1));
		dbus_connection_remove_filter(raw_connection,
					      filter_func, NULL);
		dbus_bus_remove_match(raw_connection,
				   "type='signal',interface='org.freedesktop.DBus',member='NameOwnerChanged',arg0='Mozilla.MicroB'",
				   &dbus_error);
		if (dbus_error_is_set(&dbus_error))
			/* Don't really care -- about to disconnect from the
			   bus anyhow */
			dbus_error_free(&dbus_error);
		dbus_bus_remove_match(raw_connection,
				   "type='signal',interface='org.freedesktop.DBus',member='NameOwnerChanged',arg0='com.nokia.microb-engine'",
				   &dbus_error);
		if (dbus_error_is_set(&dbus_error))
			dbus_error_free(&dbus_error);
		dbus_connection_close(raw_connection);
		dbus_connection_unref(raw_connection);

		/* Kill off browser UI
		   XXX: Hope we don't cause data loss here! */
		printf("Killing MicroB\n");
		kill(pid, SIGTERM);
	} else {
		/* Child process */
		close_stdio();

		/* exec maemo-invoker directly instead of relying on the
		   /usr/bin/browser symlink, since /usr/bin/browser may have
		   been replaced with a shell script calling us via D-Bus */
		/* Launch the browser in the background -- our parent will
		   wait for it to claim the D-Bus name and then display the
		   window using D-Bus */
		execl("/usr/bin/maemo-invoker", "browser", (char *)NULL);
	}
#else /* !FREMANTLE */
	if (pid > 0) {
		/* Parent process */
		waitpid(pid, &status, 0);
	} else {
		/* Child process */
		close_stdio();

		/* exec maemo-invoker directly instead of relying on the
		   /usr/bin/browser symlink, since /usr/bin/browser may have
		   been replaced with a shell script calling us via D-Bus */
		if (!strcmp(uri, "new_window")) {
			execl("/usr/bin/maemo-invoker",
			      "browser", (char *)NULL);
		} else {
			execl("/usr/bin/maemo-invoker",
			      "browser", "--url", uri, (char *)NULL);
		}
	}
#endif /* FREMANTLE */

	/* Kill off browserd if we started it */
	if (kill_browserd)
		system("kill `pidof browserd`");

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

	printf("launch_other_browser with uri '%s'\n", uri);

	if ((urilen = strlen(uri)) > 0) {
		/* Quote the URI to prevent the shell from interpreting it */
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
			memmove(quote+3, quote+1, strlen(quote));
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
		close_stdio();
	}
	execl("/bin/sh", "/bin/sh", "-c", command, (char *)NULL);
}

/* Use launch_other_browser as the default browser launcher, with the string
   passed in as the other_browser_cmd
   Resulting other_browser_cmd is always safe to free(), even if a pointer
   to a string constant is passed in */
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
		/* No default_browser configured -- use built-in default */
		ctx->default_browser_launcher = LAUNCH_DEFAULT_BROWSER;
		return;
	}

	if (!strcmp(default_browser, "tear"))
		ctx->default_browser_launcher = launch_tear;
	else if (!strcmp(default_browser, "microb"))
		ctx->default_browser_launcher = launch_microb;
	else if (!strcmp(default_browser, "fennec"))
		/* Cheat and reuse launch_other_browser, since we don't appear
		   to need to do anything special */
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
