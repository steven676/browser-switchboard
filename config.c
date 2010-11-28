/*
 * config.c -- configuration functions for Browser Switchboard
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
#include <stddef.h>
#include <string.h>

#include "configfile.h"
#include "config.h"

/* The Browser Switchboard config file options */
struct swb_config_option swb_config_options[] = {
	{ "continuous_mode", SWB_CONFIG_OPT_INT, SWB_CONFIG_CONTINUOUS_MODE_SET, offsetof(struct swb_config, continuous_mode) },
	{ "default_browser", SWB_CONFIG_OPT_STRING, SWB_CONFIG_DEFAULT_BROWSER_SET, offsetof(struct swb_config, default_browser) },
	{ "other_browser_cmd", SWB_CONFIG_OPT_STRING, SWB_CONFIG_OTHER_BROWSER_CMD_SET, offsetof(struct swb_config, other_browser_cmd) },
	{ "logging", SWB_CONFIG_OPT_STRING, SWB_CONFIG_LOGGING_SET, offsetof(struct swb_config, logging) },
	{ "autostart_microb", SWB_CONFIG_OPT_INT, SWB_CONFIG_AUTOSTART_MICROB_SET, offsetof(struct swb_config, autostart_microb) },
	{ NULL, 0, 0, 0 },
};

/* Browser Switchboard configuration defaults */
static struct swb_config swb_config_defaults = {
	.flags = SWB_CONFIG_INITIALIZED,
	.continuous_mode = 1,
	.default_browser = "microb",
	.other_browser_cmd = NULL,
	.logging = "stdout",
	.autostart_microb = -1,
};


/* Initialize a swb_config struct with configuration defaults */
inline void swb_config_init(struct swb_config *cfg) {
	*cfg = swb_config_defaults;
}

/* Free all heap memory used in an swb_config struct
   This MUST NOT be done if any of the strings are being used elsewhere! */
void swb_config_free(struct swb_config *cfg) {
	int i;
	void *entry;

	if (!cfg)
		return;
	if (!(cfg->flags & SWB_CONFIG_INITIALIZED))
		return;

	for (i = 0; swb_config_options[i].name; ++i) {
		entry = (char *)cfg + swb_config_options[i].offset;
		if (cfg->flags & swb_config_options[i].set_mask) {
			switch (swb_config_options[i].type) {
			  case SWB_CONFIG_OPT_STRING:
				free(*(char **)entry);
				*(char **)entry = NULL;
				break;
			  default:
				break;
			}
		}
	}

	cfg->flags = 0;
}

/* Load a value into the part of a struct swb_config indicated by name */
static int swb_config_load_option(struct swb_config *cfg,
				  char *name, char *value) {
	struct swb_config_option *opt;
	void *entry;

	/* Search through list of recognized config options for a match */
	for (opt = swb_config_options; opt->name; ++opt) {
		if (strcmp(name, opt->name))
			continue;

		if (!(cfg->flags & opt->set_mask)) {
			entry = (char *)cfg + opt->offset;
			switch (opt->type) {
			  case SWB_CONFIG_OPT_STRING:
				*(char **)entry = value;
				break;
			  case SWB_CONFIG_OPT_INT:
				*(int *)entry = atoi(value);
				free(value);
				break;
			}
			cfg->flags |= opt->set_mask;
		} else {
			/* Option was repeated in the config file
			   We want the first value, so ignore this one */
			free(value);
		}
		return 1;
	}

	/* Unrecognized config option */
	free(value);
	return 0;
}

/* Read the config file and load settings into the provided swb_config struct
   Caller is responsible for freeing allocated strings with free()
   Returns true on success, false otherwise */
int swb_config_load(struct swb_config *cfg) {
	FILE *fp;
	struct swb_config_line line;

	if (!cfg || !(cfg->flags & SWB_CONFIG_INITIALIZED))
		return 0;

	if (!(fp = open_config_file()))
		goto out_noopen;

	/* Parse the config file
	   TODO: should we handle errors differently than EOF? */
	if (!parse_config_file_begin())
		goto out;
	while (!parse_config_file_line(fp, &line)) {
		if (line.parsed)
			swb_config_load_option(cfg, line.key, line.value);
		free(line.key);
	}
	parse_config_file_end();

out:
	fclose(fp);
out_noopen:
	return 1;
}
