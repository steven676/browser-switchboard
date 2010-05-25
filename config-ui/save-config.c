/*
 * save-config.c -- functions to save a Browser Switchboard configuration
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
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "configfile.h"
#include "config.h"

/* Save the settings in the provided swb_config struct to the config file
   Returns true on success, false otherwise */
int swb_config_save(struct swb_config *cfg) {
	FILE *fp = NULL, *tmpfp = NULL;
	char *homedir, *tempfile, *newfile;
	size_t len;
	int retval = 1;
	struct swb_config_line line;
	unsigned int oldcfg_seen = 0;

	/* If CONFIGFILE_DIR doesn't exist already, try to create it */
	if (!(homedir = getenv("HOME")))
		homedir = DEFAULT_HOMEDIR;
	len = strlen(homedir) + strlen(CONFIGFILE_DIR) + 1;
	if (!(newfile = calloc(len, sizeof(char))))
		return 0;
	snprintf(newfile, len, "%s%s", homedir, CONFIGFILE_DIR);
	if (access(newfile, F_OK) == -1 && errno == ENOENT) {
		mkdir(newfile, 0750);
	}
	free(newfile);

	/* Put together the path to the new config file and the tempfile */
	len = strlen(homedir) + strlen(CONFIGFILE_LOC) + 1;
	if (!(newfile = calloc(len, sizeof(char))))
		return 0;
	/* 4 = strlen(".tmp") */
	if (!(tempfile = calloc(len+4, sizeof(char)))) {
		free(newfile);
		return 0;
	}
	snprintf(newfile, len, "%s%s", homedir, CONFIGFILE_LOC);
	snprintf(tempfile, len+4, "%s%s", newfile, ".tmp");

	/* Open the temporary file for writing */
	if (!(tmpfp = fopen(tempfile, "w"))) {
		retval = 0;
		goto out;
	}

	/* Open the old config file, if it exists */
	if ((fp = open_config_file()) && parse_config_file_begin()) {
		/* Copy the old config file over to the new one line by line,
		   replacing old config values with new ones
		   TODO: should we handle errors differently than EOF? */
		while (!parse_config_file_line(fp, &line)) {
			if (line.parsed) {
				/* Is a config line, print the new value here */
				if (!strcmp(line.key, "continuous_mode")) {
					if (!(oldcfg_seen &
					      SWB_CONFIG_CONTINUOUS_MODE_SET)) {
						fprintf(tmpfp, "%s = %d\n",
							line.key,
							cfg->continuous_mode);
						oldcfg_seen |=
							SWB_CONFIG_CONTINUOUS_MODE_SET;
					}
				} else if (!strcmp(line.key,
						   "default_browser")) {
					if (!(oldcfg_seen &
					      SWB_CONFIG_DEFAULT_BROWSER_SET)) {
						fprintf(tmpfp, "%s = \"%s\"\n",
							line.key,
							cfg->default_browser);
						oldcfg_seen |=
							SWB_CONFIG_DEFAULT_BROWSER_SET;
					}
				} else if (!strcmp(line.key,
						   "other_browser_cmd")) {
					if (!(oldcfg_seen &
					      SWB_CONFIG_OTHER_BROWSER_CMD_SET)) {
						fprintf(tmpfp, "%s = \"%s\"\n",
							line.key,
							cfg->other_browser_cmd);
						oldcfg_seen |=
							SWB_CONFIG_OTHER_BROWSER_CMD_SET;
					}
				} else if (!strcmp(line.key,
						   "logging")) {
					if (!(oldcfg_seen &
					      SWB_CONFIG_LOGGING_SET)) {
						fprintf(tmpfp, "%s = \"%s\"\n",
							line.key,
							cfg->logging);
						oldcfg_seen |=
							SWB_CONFIG_LOGGING_SET;
					}
				}
			} else {
				/* Just copy the old line over */
				fprintf(tmpfp, "%s\n", line.key);
			}
			free(line.key);
			free(line.value);
		}
		parse_config_file_end();
	}

	/* If we haven't written them yet, write out any new config values */
	if (!(oldcfg_seen & SWB_CONFIG_CONTINUOUS_MODE_SET) &&
	    (cfg->flags & SWB_CONFIG_CONTINUOUS_MODE_SET))
		fprintf(tmpfp, "%s = %d\n",
			"continuous_mode", cfg->continuous_mode);
	if (!(oldcfg_seen & SWB_CONFIG_DEFAULT_BROWSER_SET) &&
	    (cfg->flags & SWB_CONFIG_DEFAULT_BROWSER_SET))
		fprintf(tmpfp, "%s = \"%s\"\n",
			"default_browser", cfg->default_browser);
	if (!(oldcfg_seen & SWB_CONFIG_OTHER_BROWSER_CMD_SET) &&
	    (cfg->flags & SWB_CONFIG_OTHER_BROWSER_CMD_SET))
		fprintf(tmpfp, "%s = \"%s\"\n",
			"other_browser_cmd", cfg->other_browser_cmd);
	if (!(oldcfg_seen & SWB_CONFIG_LOGGING_SET) &&
	    (cfg->flags & SWB_CONFIG_LOGGING_SET))
		fprintf(tmpfp, "%s = \"%s\"\n",
			"logging", cfg->logging);

	/* Replace the old config file with the new one */
	fclose(tmpfp);
	tmpfp = NULL;
	rename(tempfile, newfile);

out:
	free(newfile);
	free(tempfile);
	if (tmpfp)
		fclose(tmpfp);
	if (fp)
		fclose(fp);
	return retval;
}
