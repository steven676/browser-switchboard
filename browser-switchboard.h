/*
 * browser-switchboard.h -- definitions common to all of browser-switchboard
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

#ifndef _BROWSER_SWITCHBOARD_H
#define _BROWSER_SWITCHBOARD_H 1

struct swb_context {
	int continuous_mode;
	void (*default_browser_launcher)(struct swb_context *, char *);
	char *other_browser_cmd;
	DBusGConnection *session_bus;
	DBusGProxy *dbus_proxy;
};

#endif /* _BROWSER_SWITCHBOARD_H */