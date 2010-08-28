/*
 * config.h -- definitions for Browser Switchboard configuration
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

#ifndef _CONFIG_H
#define _CONFIG_H

#define SWB_CONFIG_INITIALIZED			0x01
#define SWB_CONFIG_CONTINUOUS_MODE_SET		0x02
#define SWB_CONFIG_DEFAULT_BROWSER_SET		0x04
#define SWB_CONFIG_OTHER_BROWSER_CMD_SET	0x08
#define SWB_CONFIG_LOGGING_SET			0x10
#define SWB_CONFIG_AUTOSTART_MICROB_SET		0x20

struct swb_config {
	unsigned int flags;
	/* Array of pointers to the elements of the struct, in the order given
	   in swb_config_options[] */
	void *entries[5];

	int continuous_mode;
	char *default_browser;
	char *other_browser_cmd;
	char *logging;
	int autostart_microb;
};

struct swb_config_option {
	char *name;
	enum {
		SWB_CONFIG_OPT_STRING,
		SWB_CONFIG_OPT_INT
	} type;
	int set_mask;
};

void swb_config_copy(struct swb_config *dst, struct swb_config *src);
void swb_config_init(struct swb_config *cfg);
void swb_config_free(struct swb_config *cfg);

int swb_config_load(struct swb_config *cfg);

#endif /* _CONFIG_H */
