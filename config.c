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
	{ "continuous_mode", SWB_CONFIG_OPT_INT, SWB_CONFIG_CONTINUOUS_MODE_SET },
	{ "default_browser", SWB_CONFIG_OPT_STRING, SWB_CONFIG_DEFAULT_BROWSER_SET },
	{ "other_browser_cmd", SWB_CONFIG_OPT_STRING, SWB_CONFIG_OTHER_BROWSER_CMD_SET },
	{ "logging", SWB_CONFIG_OPT_STRING, SWB_CONFIG_LOGGING_SET },
	{ NULL, 0, 0 },
};

/* Browser Switchboard configuration defaults */
static struct swb_config swb_config_defaults = {
	.flags = SWB_CONFIG_INITIALIZED,
	.continuous_mode = 0,
	.default_browser = "microb",
	.other_browser_cmd = NULL,
	.logging = "stdout",
};


/* Copy the contents of an swb_config struct
   The entries[] array means that the standard copy will not work */
void swb_config_copy(struct swb_config *dst, struct swb_config *src) {
	if (!dst || !src)
		return;

	dst->entries[0] = &(dst->continuous_mode);
	dst->entries[1] = &(dst->default_browser);
	dst->entries[2] = &(dst->other_browser_cmd);
	dst->entries[3] = &(dst->logging);

	dst->flags = src->flags;

	dst->continuous_mode = src->continuous_mode;
	dst->default_browser = src->default_browser;
	dst->other_browser_cmd = src->other_browser_cmd;
	dst->logging = src->logging;
}

/* Initialize a swb_config struct with configuration defaults */
void swb_config_init(struct swb_config *cfg) {
	swb_config_copy(cfg, &swb_config_defaults);
}

/* Free all heap memory used in an swb_config struct
   This MUST NOT be done if any of the strings are being used elsewhere! */
void swb_config_free(struct swb_config *cfg) {
	if (!cfg)
		return;
	if (!(cfg->flags & SWB_CONFIG_INITIALIZED))
		return;

	if (cfg->flags & SWB_CONFIG_DEFAULT_BROWSER_SET) {
		free(cfg->default_browser);
		cfg->default_browser = NULL;
	}
	if (cfg->flags & SWB_CONFIG_OTHER_BROWSER_CMD_SET) {
		free(cfg->other_browser_cmd);
		cfg->other_browser_cmd = NULL;
	}
	if (cfg->flags & SWB_CONFIG_LOGGING_SET) {
		free(cfg->logging);
		cfg->logging = NULL;
	}

	cfg->flags = 0;
}

/* Load a value into the part of a struct swb_config indicated by name */
static int swb_config_load_option(struct swb_config *cfg,
				  char *name, char *value) {
	struct swb_config_option *opt;
	ptrdiff_t i;
	int retval = 0;

	for (opt = swb_config_options; opt->name; ++opt) {
		if (strcmp(name, opt->name))
			continue;

		if (!(cfg->flags & opt->set_mask)) {
			i = opt - swb_config_options;
			switch (opt->type) {
			  case SWB_CONFIG_OPT_STRING:
				*(char **)cfg->entries[i] = value;
				break;
			  case SWB_CONFIG_OPT_INT:
				*(int *)cfg->entries[i] = atoi(value);
				free(value);
				break;
			}
			cfg->flags |= opt->set_mask;
		}
		retval = 1;
		break;
	}

	if (!retval)
		free(value);

	return retval;
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
