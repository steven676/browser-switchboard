/*
 * dbus-server-bindings.h -- definitions for the osso_browser interface
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

#ifndef _DBUS_SERVER_BINDINGS_H
#define _DBUS_SERVER_BINDINGS_H 1

#include "browser-switchboard.h"

GType osso_browser_get_type(void);
#define OSSO_BROWSER_TYPE (osso_browser_get_type())
typedef struct _OssoBrowser {
	GObject parent;
} OssoBrowser;
typedef struct _OssoBrowserClass {
	GObjectClass parent;
} OssoBrowserClass;

/* The com.nokia.osso_browser D-Bus interface */
gboolean osso_browser_load_url(OssoBrowser *obj,
		const char *uri, GError **error);
gboolean osso_browser_load_url_sb(OssoBrowser *obj,
		const char *uri, gboolean fullscreen, GError **error);
gboolean osso_browser_mime_open(OssoBrowser *obj,
		const char *uri, GError **error);
gboolean osso_browser_open_new_window(OssoBrowser *obj,
		const char *uri, GError **error);
gboolean osso_browser_open_new_window_sb(OssoBrowser *obj,
		const char *uri, gboolean fullscreen, GError **error);
gboolean osso_browser_top_application(OssoBrowser *obj, GError **error);
/* This is an "undocumented", non-standard extension; DO NOT USE */
gboolean osso_browser_switchboard_launch_microb(OssoBrowser *obj,
		const char *uri, GError **error);

void dbus_request_osso_browser_name(struct swb_context *ctx);
void dbus_release_osso_browser_name(struct swb_context *ctx);

const DBusGObjectInfo dbus_glib_osso_browser_object_info;

#endif /* _DBUS_SERVER_BINDINGS_H */
