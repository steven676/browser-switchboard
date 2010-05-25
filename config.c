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
#include <string.h>

#include "configfile.h"
#include "config.h"

/* Initialize a swb_config struct with configuration defaults */
void swb_config_init(struct swb_config *cfg) {
	if (!cfg)
		return;

	cfg->continuous_mode = 0;
	cfg->default_browser = "microb";
	cfg->other_browser_cmd = NULL;
	cfg->logging = "stdout";

	cfg->flags = SWB_CONFIG_INITIALIZED;
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
		if (line.parsed) {
			if (!strcmp(line.key, "continuous_mode")) {
				if (!(cfg->flags &
				      SWB_CONFIG_CONTINUOUS_MODE_SET)) {
					cfg->continuous_mode = atoi(line.value);
					cfg->flags |=
						SWB_CONFIG_CONTINUOUS_MODE_SET;
				}
				free(line.value);
			} else if (!strcmp(line.key, "default_browser")) {
				if (!(cfg->flags &
				      SWB_CONFIG_DEFAULT_BROWSER_SET)) {
					cfg->default_browser = line.value;
					cfg->flags |=
						SWB_CONFIG_DEFAULT_BROWSER_SET;
				}
			} else if (!strcmp(line.key, "other_browser_cmd")) {
				if (!(cfg->flags &
				      SWB_CONFIG_OTHER_BROWSER_CMD_SET)) {
					cfg->other_browser_cmd = line.value;
					cfg->flags |=
						SWB_CONFIG_OTHER_BROWSER_CMD_SET;
				}
			} else if (!strcmp(line.key, "logging")) {
				if (!(cfg->flags & SWB_CONFIG_LOGGING_SET)) {
					cfg->logging = line.value;
					cfg->flags |= SWB_CONFIG_LOGGING_SET;
				}
			} else {
				/* Don't need this line's contents */
				free(line.value);
			}
		}
		free(line.key);
	}
	parse_config_file_end();

out:
	fclose(fp);
out_noopen:
	return 1;
}
