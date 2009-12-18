/*
 * configfile.h -- definitions for config file parsing
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

#ifndef _CONFIGFILE_H
#define _CONFIGFILE_H

#include <stdio.h>

#define DEFAULT_HOMEDIR "/home/user"
#define CONFIGFILE_DIR "/.config/"
#define CONFIGFILE_NAME "browser-switchboard"
#define CONFIGFILE_NAME_OLD "browser-proxy"
#define CONFIGFILE_LOC CONFIGFILE_DIR CONFIGFILE_NAME
#define CONFIGFILE_LOC_OLD CONFIGFILE_DIR CONFIGFILE_NAME_OLD

struct swb_config_line {
	/* Whether or not the line has been parsed */
	int parsed;
	/* If parsed, the config key; otherwise, the entire line */
	char *key;
	/* If parsed, the config value */
	char *value;
};

FILE *open_config_file(void);

int parse_config_file_begin(void);
void parse_config_file_end(void);
int parse_config_file_line(FILE *fp, struct swb_config_line *line);

#endif /* _CONFIGFILE_H */
