/*
 * launcher.c -- functions for launching web browsers for browser-switchboard
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dbus/dbus-glib.h>

#ifdef FREMANTLE
#include <dbus/dbus.h>
#include <errno.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/inotify.h>

#define DEFAULT_HOMEDIR "/home/user"
#define MICROB_PROFILE_DIR "/.mozilla/microb"
#define MICROB_LOCKFILE "lock"
#endif

#include "browser-switchboard.h"
#include "launcher.h"
#include "dbus-server-bindings.h"

#define LAUNCH_DEFAULT_BROWSER launch_microb

#ifdef FREMANTLE
static int microb_started = 0;

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

/* Get a browserd PID from the corresponding Mozilla profile lockfile */
static pid_t get_browserd_pid(const char *lockfile) {
	char buf[256], *tmp;

	/* The lockfile is a symlink pointing to "[ipaddr]:+[pid]", so read in
	   the target of the symlink and parse it that way */
	memset(buf, '\0', 256);
	if (readlink(lockfile, buf, 255) == -1)
		return -errno;
	if (!(tmp = strstr(buf, ":+")))
		return 0;
	tmp += 2; /* Skip over the ":+" */

	return atoi(tmp);
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
		if (!tear_proxy) {
			if (!(tear_proxy = dbus_g_proxy_new_for_name(
						ctx->session_bus,
				       		"com.nokia.tear",
						"/com/nokia/tear",
						"com.nokia.Tear"))) {
				printf("Failed to create proxy for com.nokia.Tear D-Bus interface\n");
				exit(1);
			}
		}

		if (!dbus_g_proxy_call(tear_proxy, "OpenAddress", &error,
				       G_TYPE_STRING, uri, G_TYPE_INVALID,
				       G_TYPE_INVALID)) {
			printf("Opening window failed: %s\n", error->message);
			exit(1);
		}
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
	char *homedir, *microb_profile_dir, *microb_lockfile;
	size_t len;
	int fd, inot_wd;
	DBusConnection *raw_connection;
	DBusError dbus_error;
	DBusHandleMessageFunction filter_func;
	DBusGProxy *g_proxy;
	GError *gerror = NULL;
	int bytes_read;
	char buf[256], *pos;
	struct inotify_event *event;
	pid_t browserd_pid, waited_pid;
	struct sigaction act, oldact;
	int ignore_sigstop;
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
	/* Put together the path to the MicroB browserd lockfile */
	if (!(homedir = getenv("HOME")))
		homedir = DEFAULT_HOMEDIR;
	len = strlen(homedir) + strlen(MICROB_PROFILE_DIR) + 1;
	if (!(microb_profile_dir = calloc(len, sizeof(char)))) {
		printf("calloc() failed\n");
		exit(1);
	}
	snprintf(microb_profile_dir, len, "%s%s",
		 homedir, MICROB_PROFILE_DIR);
	len = strlen(homedir) + strlen(MICROB_PROFILE_DIR) +
	      strlen("/") + strlen(MICROB_LOCKFILE) + 1;
	if (!(microb_lockfile = calloc(len, sizeof(char)))) {
		printf("calloc() failed\n");
		exit(1);
	}
	snprintf(microb_lockfile, len, "%s%s/%s",
		 homedir, MICROB_PROFILE_DIR, MICROB_LOCKFILE);

	/* Watch for the creation of a MicroB browserd lockfile
	   NB: The watch has to be set up here, before the browser
	   is launched, to make sure there's no race between browserd
	   starting and us creating the watch */
	if ((fd = inotify_init()) == -1) {
		perror("inotify_init");
		exit(1);
	}
	if ((inot_wd = inotify_add_watch(fd, microb_profile_dir,
					 IN_CREATE)) == -1) {
		perror("inotify_add_watch");
		exit(1);
	}
	free(microb_profile_dir);

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
		if (dbus_error_is_set(&dbus_error))
			/* Don't really care -- about to disconnect from the
			   bus anyhow */
			dbus_error_free(&dbus_error);
		dbus_connection_close(raw_connection);
		dbus_connection_unref(raw_connection);

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
		   its attached browserd process.  Get the browserd process's
		   PID and use ptrace() to watch for process termination.

		   This has the problem of not being able to detect whether
		   the bookmark window is open and/or in use, but it's the best
		   that I can think of.  Better suggestions would be greatly
		   appreciated. */

		/* Wait for the MicroB browserd lockfile to be created */
		printf("Waiting for browserd lockfile to be created\n");
		memset(buf, '\0', 256);
		/* read() blocks until there are events to be read */
		while ((bytes_read = read(fd, buf, 255)) > 0) {
			pos = buf;
			/* Loop until we see the event we're looking for
			   or until all the events are processed */
			while (pos && (pos-buf) < bytes_read) {
				event = (struct inotify_event *)pos;
				len = sizeof(struct inotify_event)
				      + event->len;
				if (!strcmp(MICROB_LOCKFILE,
					    event->name)) {
					/* Lockfile created */
					pos = NULL;
					break;
				} else if ((pos-buf) + len < bytes_read)
					/* More events to process */
					pos += len;
				else
					/* All events processed */
					pos = buf + bytes_read;
			}
			if (!pos)
				/* Event found, stop looking */
				break;
			memset(buf, '\0', 256);
                }
		inotify_rm_watch(fd, inot_wd);
		close(fd);

		/* Get the PID of the browserd from the lockfile */
		if ((browserd_pid = get_browserd_pid(microb_lockfile)) <= 0) {
			if (browserd_pid == 0)
				printf("Profile lockfile link lacks PID\n");
			else
				printf("readlink() on lockfile failed: %s\n",
				       strerror(-browserd_pid));
			exit(1);
		}
		free(microb_lockfile);

		/* Wait for the browserd to close */
		printf("Waiting for MicroB (browserd pid %d) to finish\n",
		       browserd_pid);
		/* Clear any existing SIGCHLD handler to prevent interference
		   with our wait() */
		act.sa_handler = SIG_DFL;
		act.sa_flags = 0;
		sigemptyset(&(act.sa_mask));
		if (sigaction(SIGCHLD, &act, &oldact) == -1) {
			perror("clearing SIGCHLD handler failed");
			exit(1);
		}

		/* Trace the browserd to get a close notification */
		ignore_sigstop = 1;
		if (ptrace(PTRACE_ATTACH, browserd_pid, NULL, NULL) == -1) {
			perror("PTRACE_ATTACH");
			exit(1);
		}
		ptrace(PTRACE_CONT, browserd_pid, NULL, NULL);
		while ((waited_pid = wait(&status)) > 0) {
			if (waited_pid != browserd_pid)
				/* Not interested in other processes */
				continue;
			if (WIFEXITED(status) || WIFSIGNALED(status))
				/* browserd exited */
				break;
			else if (WIFSTOPPED(status)) {
				/* browserd was sent a signal
				   We're responsible for making sure this
				   signal gets delivered */
				if (ignore_sigstop &&
				    WSTOPSIG(status) == SIGSTOP) {
					/* Ignore the first SIGSTOP received
					   This is raised for some reason
					   immediately after we start tracing
					   the process, and won't be followed
					   by a SIGCONT at any point */
					printf("Ignoring first SIGSTOP\n");
					ptrace(PTRACE_CONT, browserd_pid,
					       NULL, NULL);
					ignore_sigstop = 0;
					continue;
				}
				printf("Forwarding signal %d to browserd\n",
				       WSTOPSIG(status));
				ptrace(PTRACE_CONT, browserd_pid,
				       NULL, WSTOPSIG(status));
			}
		}

		/* Kill off browser UI
		   XXX: There is a race here with the restarting of the closed
		   browserd; if that happens before we kill the browser UI, the
		   newly started browserd may not close with the UI
		   XXX: Hope we don't cause data loss here! */
		printf("Killing MicroB\n");
		kill(pid, SIGTERM);
		waitpid(pid, &status, 0);

		/* Restore old SIGCHLD handler */
		if (sigaction(SIGCHLD, &oldact, NULL) == -1) {
			perror("restoring old SIGCHLD handler failed");
			exit(1);
		}
	} else {
		/* Child process */
		close(fd);
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
