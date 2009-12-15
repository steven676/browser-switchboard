/*
 * dbus-server-bindings.c -- osso_browser D-Bus interface implementation
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
#include <dbus/dbus-glib.h>

#include "browser-switchboard.h"
#include "launcher.h"
#include "dbus-server-bindings.h"

extern struct swb_context ctx;

G_DEFINE_TYPE(OssoBrowser, osso_browser, G_TYPE_OBJECT);
static void osso_browser_init(OssoBrowser *obj)
{
}

static void osso_browser_class_init(OssoBrowserClass *klass)
{
}

#include "dbus-server-glue.h"


static void open_address(const char *uri) {
	char *new_uri;
	size_t new_uri_len;

	if (!uri)
		/* Not much to do in this case ... */
		return;

	if (uri[0] == '/') {
		new_uri_len = strlen("file://") + strlen(uri) + 1;
		if (!(new_uri = calloc(new_uri_len, sizeof(char))))
			exit(1);
		snprintf(new_uri, new_uri_len, "%s%s", "file://", uri);

		launch_browser(&ctx, new_uri);
		/* If launch_browser didn't exec something in this process,
		   we need to clean up after ourselves */
		free(new_uri);
	} else {
		printf("open_address '%s'\n", uri);
		launch_browser(&ctx, (char *)uri);
	}
}


/*
 * The com.nokia.osso_browser D-Bus interface
 */
gboolean osso_browser_load_url(OssoBrowser *obj,
		const char *uri, GError **error) {
	open_address(uri);
	return TRUE;
}

gboolean osso_browser_mime_open(OssoBrowser *obj,
		const char *uri, GError **error) {
	open_address(uri);
	return TRUE;
}

gboolean osso_browser_open_new_window(OssoBrowser *obj,
		const char *uri, GError **error) {
	open_address(uri);
	return TRUE;
}

gboolean osso_browser_top_application(OssoBrowser *obj,
		GError **error) {
	launch_microb(&ctx, "new_window");
	return TRUE;
}

/* This is a "undocumented", non-standard extension to the API, ONLY
   for use by /usr/bin/browser wrapper to implement --url */
gboolean osso_browser_switchboard_launch_microb(OssoBrowser *obj,
		const char *uri, GError **error) {
	launch_microb(&ctx, (char *)uri);
	return TRUE;
}


/* Register the name com.nokia.osso_browser on the D-Bus session bus */
void dbus_request_osso_browser_name(struct swb_context *ctx) {
	GError *error = NULL;
	guint result;

	if (!ctx || !ctx->dbus_proxy)
		return;

	if (!dbus_g_proxy_call(ctx->dbus_proxy, "RequestName", &error,
			       G_TYPE_STRING, "com.nokia.osso_browser",
			       G_TYPE_UINT, DBUS_NAME_FLAG_REPLACE_EXISTING|DBUS_NAME_FLAG_DO_NOT_QUEUE,
			       G_TYPE_INVALID,
			       G_TYPE_UINT, &result,
			       G_TYPE_INVALID)) {
		printf("Couldn't acquire name com.nokia.osso_browser\n");
		exit(1);
	}
	if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {	
		printf("Couldn't acquire name com.nokia.osso_browser\n");
		exit(1);
	}
}

/* Release the name com.nokia.osso_browser on the D-Bus session bus */
void dbus_release_osso_browser_name(struct swb_context *ctx) {
	GError *error = NULL;
	guint result;

	if (!ctx || !ctx->dbus_proxy)
		return;

	dbus_g_proxy_call(ctx->dbus_proxy, "ReleaseName", &error,
			  G_TYPE_STRING, "com.nokia.osso_browser",
			  G_TYPE_INVALID,
			  G_TYPE_UINT, &result,
			  G_TYPE_INVALID);
}