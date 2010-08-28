/*
 * browsers.h -- the list of known browsers
 *
 * Copyright (C) 2010 Steven Luo
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

#ifndef _BROWSERS_H
#define _BROWSERS_H 1

struct browser_entry {
	char *config;
	char *displayname;
	char *binary;
};

struct browser_entry browsers[] = {
	{ "microb", "MicroB (stock browser)", NULL }, /* First entry is the default! */
	{ "tear", "Tear", "/usr/bin/tear" },
	{ "fennec", "Firefox Mobile", "/usr/bin/fennec" },
	{ "opera", "Opera Mobile", "/usr/bin/opera" },
	{ "midori", "Midori", "/usr/bin/midori" },
	{ "other", "Other", NULL },
	{ NULL, NULL, NULL },
};

#endif /* _BROWSERS_H */
