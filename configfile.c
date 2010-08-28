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
#include <sys/types.h>
#include <regex.h>

#include "configfile.h"

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

static regex_t re_ignore, re_config1, re_config2;
static int re_init = 0;

/* Open config file for reading */
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

/* Initialize the config file parser
   Returns 1 if initialization successful, 0 otherwise */
int parse_config_file_begin(void) {
	/* Just return if parser's already been initialized */
	if (re_init)
		return 1;

	/* compile regex matching blank lines or comments */
	if (regcomp(&re_ignore, REGEX_IGNORE, REGEX_IGNORE_FLAGS))
		return 0;
	/* compile regex matching foo = "bar", with arbitrary whitespace at
	   beginning and end of line and surrounding the = */
	if (regcomp(&re_config1, REGEX_CONFIG1, REGEX_CONFIG1_FLAGS)) {
		regfree(&re_ignore);
		return 0;
	}
	/* compile regex matching foo = bar, with arbitrary whitespace at
	   beginning of line and surrounding the = */
	if (regcomp(&re_config2, REGEX_CONFIG2, REGEX_CONFIG2_FLAGS)) {
		regfree(&re_ignore);
		regfree(&re_config1);
		return 0;
	}

	re_init = 1;
	return 1;
}

/* End config file parsing and free the resources */
void parse_config_file_end(void) {
	/* Just return if parser's not active */
	if (!re_init)
		return;

	regfree(&re_ignore);
	regfree(&re_config1);
	regfree(&re_config2);
	re_init = 0;
}

/* Read the next line from a config file and store it into a swb_config_line,
   parsing it into key and value if possible
   Caller is responsible for freeing the strings in the swb_config_line if
   call returns successfully
   Returns 0 on success (whether line parsed or not), 1 on EOF, -1 on error */
int parse_config_file_line(FILE *fp, struct swb_config_line *line) {
	regmatch_t substrs[3];
	size_t len;
	char *tmp;

	if (!re_init || !fp || !line)
		return -1;

	line->parsed = 0;
	line->key = NULL;
	line->value = NULL;

	if (!(line->key = calloc(MAXLINE, sizeof(char))))
		return -1;

	/* Read in the next line of the config file
	   XXX doesn't deal with lines longer than MAXLINE */
	if (!fgets(line->key, MAXLINE, fp)) {
		free(line->key);
		line->key = NULL;
		if (feof(fp))
			return 1;
		else
			return -1;
	}

	/* no need to parse blank lines and comments */
	if (!regexec(&re_ignore, line->key, 0, NULL, 0))
		goto finish;

	/* Find the substrings corresponding to the key and value
	   If the line doesn't match our idea of a config file entry,
	   don't parse it */
	if (regexec(&re_config1, line->key, 3, substrs, 0) &&
	    regexec(&re_config2, line->key, 3, substrs, 0))
		goto finish;
	if (substrs[1].rm_so == -1 || substrs[2].rm_so == -1)
		goto finish;

	/* copy the config value into a new string */
	len = substrs[2].rm_eo - substrs[2].rm_so;
	if (!(line->value = calloc(len+1, sizeof(char)))) {
		free(line->key);
		line->key = NULL;
		return -1;
	}
	strncpy(line->value, line->key+substrs[2].rm_so, len);
	/* calloc() zeroes the memory, so string is automatically
	   null terminated */

	/* make key point to a null-terminated string holding the
	   config key */
	len = substrs[1].rm_eo - substrs[1].rm_so;
	memmove(line->key, line->key+substrs[1].rm_so, len);
	line->key[len] = '\0';

	/* done parsing the line */
	line->parsed = 1;

finish:
	if (!line->parsed) {
		/* Toss a trailing newline, if present */
		len = strlen(line->key);
		if (line->key[len-1] == '\n')
			line->key[len-1] = '\0';
	}
	/* Try to shrink the allocation for key, to save space if someone
	   wants to keep it around */
	len = strlen(line->key);
	if ((tmp = realloc(line->key, len+1)))
		line->key = tmp;
	return 0;
}
