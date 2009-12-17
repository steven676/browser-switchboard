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
#include <sys/types.h>
#include <regex.h>

#define DEFAULT_HOMEDIR "/home/user"
#define CONFIGFILE_LOC "/.config/browser-switchboard"
#define CONFIGFILE_LOC_OLD "/.config/browser-proxy"
#define MAXLINE 1024

/* regex matching blank lines or comments */
#define REGEX_IGNORE "^[[:space:]]*(#|$)"
#define REGEX_IGNORE_FLAGS REG_EXTENDED|REG_NOSUB

/* regex matching foo = "bar", with arbitrary whitespace at beginning and end
   of line and surrounding the = */
#define REGEX_CONFIG1 "^[[:space:]]*([^=[:space:]]+)[[:space:]]*=[[:space:]]*\"(.*)\"[[:space:]]*$"
#define REGEX_CONFIG1_FLAGS REG_EXTENDED

/* regex matching foo = bar, with arbitrary whitespace at beginning of line and
   surrounding the = */
#define REGEX_CONFIG2 "^[[:space:]]*([^=[:space:]]+)[[:space:]]*=[[:space:]]*(.*)$"
#define REGEX_CONFIG2_FLAGS REG_EXTENDED|REG_NEWLINE


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
