/*
 * browser-switchboard-config.c -- command-line configuration utility for 
 * Browser Switchboard
 * 
 * Copyright (C) 2009-2010 Steven Luo
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
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
#include <getopt.h>

#include "config.h"

extern struct swb_config_option swb_config_options[];

static int get_config_value(char *name) {
	struct swb_config cfg;
	int i;
	struct swb_config_option optinfo;
	int retval = 1;

	swb_config_init(&cfg);

	if (!swb_config_load(&cfg))
		return 1;

	for (i = 0; swb_config_options[i].name; ++i) {
		optinfo = swb_config_options[i];
		if (strcmp(name, optinfo.name))
			continue;

		switch (optinfo.type) {
		  case SWB_CONFIG_OPT_STRING:
			printf("%s\n", *(char **)cfg.entries[i]);
			break;
		  case SWB_CONFIG_OPT_INT:
			printf("%d\n", *(int *)cfg.entries[i]);
			break;
		  default:
			break;
		}
		retval = 0;
		break;
	}

	swb_config_free(&cfg);

	return retval;
}

static int set_config_value(char *name, char *value) {
	struct swb_config cfg;
	int i;
	struct swb_config_option optinfo;
	int retval = 1;

	swb_config_init(&cfg);

	if (!swb_config_load(&cfg))
		return 1;

	for (i = 0; swb_config_options[i].name; ++i) {
		optinfo = swb_config_options[i];
		if (strcmp(name, optinfo.name))
			continue;

		switch (optinfo.type) {
		  case SWB_CONFIG_OPT_STRING:
			/* Free any existing string */
			if (cfg.flags & optinfo.set_mask)
				free(*(char **)cfg.entries[i]);

			if (strlen(value) == 0) {
				/* If the new value is empty, clear the config
				   setting */
				*(char **)cfg.entries[i] = NULL;
				cfg.flags &= ~optinfo.set_mask;
			} else {
				/* Make a copy of the string -- it's not safe
				   to free value, which comes from argv */
				if (!(*(char **)cfg.entries[i] =
				      strdup(value)))
					exit(1);
				cfg.flags |= optinfo.set_mask;
			}
			break;
		  case SWB_CONFIG_OPT_INT:
			if (strlen(value) == 0) {
				/* If the new value is empty, clear the config
				   setting */
				cfg.flags &= ~optinfo.set_mask;
			} else {
				*(int *)cfg.entries[i] = atoi(value);
				cfg.flags |= optinfo.set_mask;
			}
			break;
		}
		retval = 0;
		break;
	}

	if (!retval)
		if (!swb_config_save(&cfg))
			retval = 1;

	swb_config_free(&cfg);

	/* Try to send SIGHUP to any running browser-switchboard process
	   This causes it to reread config files if in continuous_mode, or
	   die so that the config will be reloaded on next start otherwise */
	system("kill -HUP `pidof browser-switchboard` > /dev/null 2>&1");

	return retval;
}

void usage(void) {
	printf("Usage:\n");
	printf("  browser-switchboard-config -b -- Display default browser\n");
	printf("  browser-switchboard-config -c -- Display command used when default browser is \"other\"\n");
	printf("  browser-switchboard-config -m -- Display continuous mode setting\n");
	printf("  browser-switchboard-config -o option -- Display value of option\n");
	printf("\n");
	printf("  browser-switchboard-config -s [-b|-c|-m|-o option] value -- Set the selected option to value\n");
	printf("\n");
	printf("  browser-switchboard-config -h -- Show this message\n");
}

int main(int argc, char **argv) {
	int opt, done = 0;
	int set = 0;
	char *selected_opt = NULL;
	
	while (!done && (opt = getopt(argc, argv, "hsbcmo:")) != -1) {
		switch (opt) {
		  case 'h':
			usage();
			exit(0);
			break;
		  case 's':
			set = 1;
			break;
		  case 'b':
			selected_opt = "default_browser";
			done = 1;
			break;
		  case 'c':
			selected_opt = "other_browser_cmd";
			done = 1;
			break;
		  case 'm':
			selected_opt = "continuous_mode";
			done = 1;
			break;
		  case 'o':
			selected_opt = optarg;
			done = 1;
			break;
		  default:
			usage();
			exit(1);
			break;
		}
	}

	if (!selected_opt) {
		printf("Must specify one of -b, -c, -m, -o\n");
		usage();
		exit(1);
	}

	if (set) {
		if (optind >= argc) {
			printf("Value to set config option to not provided\n");
			usage();
			exit(1);
		}
		return set_config_value(selected_opt, argv[optind]);
	} else
		return get_config_value(selected_opt);
}
