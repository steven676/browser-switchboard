/*
 * configfile.c -- config file functions for browser-switchboard
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

#include "configfile.h"

FILE *open_config_file(void) {
	char *homedir, *configfile;
	size_t len;
	FILE *fp;

	/* Put together the path to the config file */
	if (!(homedir = getenv("HOME")))
		homedir = DEFAULT_HOMEDIR;
	len = strlen(homedir) + strlen(CONFIGFILE_LOC) + 1;
	if (!(configfile = calloc(len, sizeof(char))))
		return NULL;
	snprintf(configfile, len, "%s%s", homedir, CONFIGFILE_LOC);

	/* Try to open the config file */
	if (!(fp = fopen(configfile, "r"))) {
		/* Try the legacy config file location before giving up
		   XXX we assume here that CONFIGFILE_LOC_OLD is shorter
		   than CONFIGFILE_LOC! */
		snprintf(configfile, len, "%s%s", homedir, CONFIGFILE_LOC_OLD);
		fp = fopen(configfile, "r");
	}

	free(configfile);
	return fp;
}
