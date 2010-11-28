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

#ifdef FREMANTLE
#include <sys/wait.h>
#endif

#include "configfile.h"
#include "config.h"

extern struct swb_config_option swb_config_options[];

/* Outputs a config file line for the named option to a file descriptor */
static void swb_config_output_option(FILE *fp, unsigned int *oldcfg_seen,
			      struct swb_config *cfg, char *name) {
	struct swb_config_option *opt;
	void *entry;

	for (opt = swb_config_options; opt->name; ++opt) {
		if (strcmp(opt->name, name))
			continue;

		entry = (char *)cfg + opt->offset;
		if (!(*oldcfg_seen & opt->set_mask) &&
		    (cfg->flags & opt->set_mask)) {
			switch (opt->type) {
			  case SWB_CONFIG_OPT_STRING:
				fprintf(fp, "%s = \"%s\"\n",
					opt->name,
					*(char **)entry);
				*oldcfg_seen |= opt->set_mask;
				break;
			  case SWB_CONFIG_OPT_INT:
				fprintf(fp, "%s = %d\n",
					opt->name,
					*(int *)entry);
				*oldcfg_seen |= opt->set_mask;
				break;
			}
		}
		break;
	}
}

/* Save the settings in the provided swb_config struct to the config file
   Returns true on success, false otherwise */
int swb_config_save(struct swb_config *cfg) {
	FILE *fp = NULL, *tmpfp = NULL;
	char *homedir, *tempfile, *newfile;
	size_t len;
	int retval = 1;
	struct swb_config_line line;
	unsigned int oldcfg_seen = 0;
	int i;

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
				swb_config_output_option(tmpfp, &oldcfg_seen,
							 cfg, line.key);
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
	for (i = 0; swb_config_options[i].name; ++i)
		swb_config_output_option(tmpfp, &oldcfg_seen, cfg,
					 swb_config_options[i].name);

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

/* Reconfigure a running browser-switchboard process with new settings */
void swb_reconfig(struct swb_config *old, struct swb_config *new) {
#ifdef FREMANTLE
	int microb_was_autostarted, microb_should_autostart;
	pid_t pid;
	int status;
#endif

	/* Try to send SIGHUP to any running browser-switchboard process
	   This causes it to reread config files if in continuous_mode, or
	   die so that the config will be reloaded on next start otherwise */
	system("kill -HUP `pidof browser-switchboard` > /dev/null 2>&1");

#ifdef FREMANTLE
	if (!old || !new)
		return;

	microb_was_autostarted = (old->autostart_microb == 1) ||
				 (!strcmp(old->default_browser, "microb") &&
				  old->autostart_microb);
	microb_should_autostart = (new->autostart_microb == 1) ||
				  (!strcmp(new->default_browser, "microb") &&
				   new->autostart_microb);
	if (!microb_was_autostarted && microb_should_autostart) {
		/* MicroB should be started if it's not running */
		status = system("pidof browser > /dev/null");
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			if ((pid = fork()) == -1)
				return;

			if (!pid) {
				/* Child process, start MicroB */
				execl("/usr/bin/maemo-invoker", "browser",
				      (char *)NULL);
			}
		}
	}
	/* XXX: We'd like to stop MicroB if (microb_was_autostarted &&
	   !microb_should_autostart), but we don't know if the open MicroB
	   process has open windows. */
#endif /* FREMANTLE */

	return;
}
