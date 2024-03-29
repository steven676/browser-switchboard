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
#include <unistd.h>
#include <getopt.h>

#include "config.h"
#include "save-config.h"
#include "browsers.h"

extern struct swb_config_option swb_config_options[];

static int get_config_value(char *name) {
	struct swb_config cfg;
	struct swb_config_option *optinfo;
	void *entry;
	int retval = 1;

	swb_config_init(&cfg);

	if (!swb_config_load(&cfg))
		return 1;

	for (optinfo = swb_config_options; optinfo->name; ++optinfo) {
		if (strcmp(name, optinfo->name))
			continue;

		entry = (char *)&cfg + optinfo->offset;
		switch (optinfo->type) {
		  case SWB_CONFIG_OPT_STRING:
			if (*(char **)entry)
				printf("%s\n", *(char **)entry);
			break;
		  case SWB_CONFIG_OPT_INT:
			printf("%d\n", *(int *)entry);
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

static int get_default_browser(void) {
	struct swb_config cfg;
	int i;

	swb_config_init(&cfg);

	if (!swb_config_load(&cfg))
		return 1;

	/* Check to see if the configured default browser is installed
	   If not, report the default default browser */
	for (i = 0; browsers[i].config; ++i) {
		if (strcmp(browsers[i].config, cfg.default_browser))
			continue;

		if (browsers[i].binary && access(browsers[i].binary, X_OK))
			printf("%s\n", browsers[0].config);
		else
			printf("%s\n", browsers[i].config);

		break;
	}

	if (!browsers[i].config)
		/* Unknown browser configured as default, report the default
		   default browser */
		printf("%s\n", browsers[0].config);

	swb_config_free(&cfg);

	return 0;
}

static int set_config_value(char *name, char *value) {
	struct swb_config orig_cfg, cfg;
	struct swb_config_option *optinfo;
	void *entry;
	int retval = 1;

	swb_config_init(&orig_cfg);

	if (!swb_config_load(&orig_cfg))
		return 1;

	cfg = orig_cfg;

	for (optinfo = swb_config_options; optinfo->name; ++optinfo) {
		if (strcmp(name, optinfo->name))
			continue;

		entry = (char *)&cfg + optinfo->offset;
		switch (optinfo->type) {
		  case SWB_CONFIG_OPT_STRING:
			if (strlen(value) == 0) {
				/* If the new value is empty, clear the config
				   setting */
				*(char **)entry = NULL;
				cfg.flags &= ~optinfo->set_mask;
			} else {
				/* Make a copy of the string -- it's not safe
				   to free value, which comes from argv */
				if (!(*(char **)entry =
				      strdup(value)))
					exit(1);
				cfg.flags |= optinfo->set_mask;
			}
			break;
		  case SWB_CONFIG_OPT_INT:
			if (strlen(value) == 0) {
				/* If the new value is empty, clear the config
				   setting */
				cfg.flags &= ~optinfo->set_mask;
			} else {
				*(int *)entry = atoi(value);
				cfg.flags |= optinfo->set_mask;
			}
			break;
		}
		retval = 0;
		break;
	}

	if (!retval)
		if (!swb_config_save(&cfg))
			retval = 1;

	/* Reconfigure a running browser-switchboard, if present */
	swb_reconfig(&orig_cfg, &cfg);

	swb_config_free(&orig_cfg);
	/* XXX can't free all of cfg, it contains pointers to memory we just
	   freed above
	swb_config_free(&cfg); */
	if (optinfo->name && optinfo->type == SWB_CONFIG_OPT_STRING)
		free(*(char **)entry);

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
	} else if (!strcmp(selected_opt, "default_browser"))
		/* Default browser value needs special handling */
		return get_default_browser();
	else
		return get_config_value(selected_opt);
}
