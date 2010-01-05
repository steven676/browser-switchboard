/*
 * browser-switchboard-cp.c -- a hildon-control-panel applet for
 * selecting the default browser for Browser Switchboard
 * 
 * Copyright (C) 2009-2010 Steven Luo
 * Copyright (C) 2009-2010 Faheem Pervez
 * 
 * Derived from services-cp.c from maemo-control-services
 * Copyright (c) 2008 Janne Kataja <janne.kataja@iki.fi>
 * Copyright (c) 2008 Nokia Corporation
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
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#ifdef HILDON
#include <hildon/hildon-banner.h>
#include <hildon/hildon-program.h>

#ifdef FREMANTLE
#include <hildon/hildon-touch-selector.h>
#include <hildon/hildon-picker-button.h>
#include <hildon/hildon-caption.h>
#include <hildon/hildon-entry.h>
#endif /* FREMANTLE */

#ifdef HILDON_CP_APPLET
#include <hildon-cp-plugin/hildon-cp-plugin-interface.h>
#endif /* HILDON_CP_APPLET */
#endif /* HILDON */

#include "configfile.h"

#define CONTINUOUS_MODE_DEFAULT 0

#if defined(HILDON) && defined(FREMANTLE)
#define _HILDON_SIZE_DEFAULT (HILDON_SIZE_AUTO_WIDTH|HILDON_SIZE_FINGER_HEIGHT)
#endif

struct browser_entry {
	char *config;
	char *displayname;
};
struct browser_entry browsers[] = {
	{ "microb", "MicroB" },	/* First entry is the default! */
	{ "tear", "Tear" },
	{ "fennec", "Mobile Firefox (Fennec)" },
	{ "midori", "Midori" },
	{ "other", "Other" },
	{ NULL, NULL },
};

struct config_widgets {
#if defined(HILDON) && defined(FREMANTLE)
	GtkWidget *continuous_mode_selector;
	GtkWidget *default_browser_selector;
#else
	GtkWidget *continuous_mode_off_radio;
	GtkWidget *continuous_mode_on_radio;
	GtkWidget *default_browser_combo;
#endif
	GtkWidget *other_browser_cmd_entry;
	GtkWidget *other_browser_cmd_entry_label;
};
struct config_widgets cw;
GtkWidget *dialog;


/**********************************************************************
 * Configuration routines
 **********************************************************************/

#if defined(HILDON) && defined(FREMANTLE)

static inline int get_continuous_mode(void) {
	return hildon_touch_selector_get_active(HILDON_TOUCH_SELECTOR(cw.continuous_mode_selector), 0);
}
static inline void set_continuous_mode(int state) {
	hildon_touch_selector_set_active(HILDON_TOUCH_SELECTOR(cw.continuous_mode_selector), 0, state);
}

static inline char *get_default_browser(void) {
	return browsers[hildon_touch_selector_get_active(HILDON_TOUCH_SELECTOR(cw.default_browser_selector), 0)].config;
}

#else /* !defined(HILDON) || !defined(FREMANTLE) */

static inline int get_continuous_mode(void) {
	return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cw.continuous_mode_on_radio));
}
static inline void set_continuous_mode(int state) {
	if (state)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cw.continuous_mode_on_radio), TRUE);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cw.continuous_mode_off_radio), TRUE);
}

static inline char *get_default_browser(void) {
	return browsers[gtk_combo_box_get_active(GTK_COMBO_BOX(cw.default_browser_combo))].config;
}

#endif /* defined(HILDON) && defined(FREMANTLE) */

static void set_default_browser(char *browser) {
	gint i;

	/* Loop through browsers looking for a match */
	for (i = 0; browsers[i].config && strcmp(browsers[i].config, browser);
			++i);

	if (!browsers[i].config)
		/* No match found, set to the default browser */
		i = 0;

#if defined(HILDON) && defined(FREMANTLE)
	hildon_touch_selector_set_active(HILDON_TOUCH_SELECTOR(cw.default_browser_selector), 0, i);
#else
	gtk_combo_box_set_active(GTK_COMBO_BOX(cw.default_browser_combo), i);
#endif
}

static inline char *get_other_browser_cmd(void) {
	return (char *)gtk_entry_get_text(GTK_ENTRY(cw.other_browser_cmd_entry));
}
static inline void set_other_browser_cmd(char *cmd) {
	gtk_entry_set_text(GTK_ENTRY(cw.other_browser_cmd_entry), cmd);
}

static void load_config(void) {
	FILE *fp;
	int continuous_mode_seen = 0;
	int default_browser_seen = 0;
	int other_browser_cmd_seen = 0;
	struct swb_config_line line;

	if (!(fp = open_config_file()))
		return;

	/* Parse the config file
	   TODO: should we handle errors differently than EOF? */
	if (!parse_config_file_begin())
		goto out;
	while (!parse_config_file_line(fp, &line)) {
		if (line.parsed) {
			if (!strcmp(line.key, "continuous_mode")) {
				if (!continuous_mode_seen) {
					set_continuous_mode(atoi(line.value));
					continuous_mode_seen = 1;
				}
			} else if (!strcmp(line.key, "default_browser")) {
				if (!default_browser_seen) {
					set_default_browser(line.value);
					default_browser_seen = 1;
				}
			} else if (!strcmp(line.key, "other_browser_cmd")) {
				if (!other_browser_cmd_seen) {
					set_other_browser_cmd(line.value);
					other_browser_cmd_seen = 1;
				}
			}
		}
		free(line.key);
		free(line.value);
	}
	parse_config_file_end();

out:
	fclose(fp);
	return;
}

static void save_config(void) {
	FILE *fp = NULL, *tmpfp = NULL;
	char *homedir, *tempfile, *newfile;
	size_t len;
	int continuous_mode_seen = 0;
	int default_browser_seen = 0;
	int other_browser_cmd_seen = 0;
	struct swb_config_line line;

	/* If CONFIGFILE_DIR doesn't exist already, try to create it */
	if (!(homedir = getenv("HOME")))
		homedir = DEFAULT_HOMEDIR;
	len = strlen(homedir) + strlen(CONFIGFILE_DIR) + 1;
	if (!(newfile = calloc(len, sizeof(char))))
		return;
	snprintf(newfile, len, "%s%s", homedir, CONFIGFILE_DIR);
	if (access(newfile, F_OK) == -1 && errno == ENOENT) {
		mkdir(newfile, 0750);
	}
	free(newfile);

	/* Put together the path to the new config file and the tempfile */
	len = strlen(homedir) + strlen(CONFIGFILE_LOC) + 1;
	if (!(newfile = calloc(len, sizeof(char))))
		return;
	/* 4 = strlen(".tmp") */
	if (!(tempfile = calloc(len+4, sizeof(char)))) {
		free(newfile);
		return;
	}
	snprintf(newfile, len, "%s%s", homedir, CONFIGFILE_LOC);
	snprintf(tempfile, len+4, "%s%s", newfile, ".tmp");

	/* Open the temporary file for writing */
	if (!(tmpfp = fopen(tempfile, "w")))
		/* TODO: report the error somehow? */
		goto out;

	/* Open the old config file, if it exists */
	if ((fp = open_config_file()) && parse_config_file_begin()) {
		/* Copy the old config file over to the new one line by line,
		   replacing old config values with new ones
		   TODO: should we handle errors differently than EOF? */
		while (!parse_config_file_line(fp, &line)) {
			if (line.parsed) {
				/* Is a config line, print the new value here */
				if (!strcmp(line.key, "continuous_mode")) {
					if (!continuous_mode_seen) {
						fprintf(tmpfp, "%s = %d\n",
							line.key,
							get_continuous_mode());
						continuous_mode_seen = 1;
					}
				} else if (!strcmp(line.key,
							"default_browser")) {
					if (!default_browser_seen) {
						fprintf(tmpfp, "%s = \"%s\"\n",
							line.key,
							get_default_browser());
						default_browser_seen = 1;
					}
				} else if (!strcmp(line.key,
							"other_browser_cmd")) {
					if (!other_browser_cmd_seen &&
					    strlen(get_other_browser_cmd())>0) {
						fprintf(tmpfp, "%s = \"%s\"\n",
							line.key,
							get_other_browser_cmd());
						other_browser_cmd_seen = 1;
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

	/* If we haven't written them yet, write out the new config values */
	if (!continuous_mode_seen)
		fprintf(tmpfp, "%s = %d\n",
			"continuous_mode", get_continuous_mode());
	if (!default_browser_seen)
		fprintf(tmpfp, "%s = \"%s\"\n",
			"default_browser", get_default_browser());
	if (!other_browser_cmd_seen && strlen(get_other_browser_cmd()) > 0)
		fprintf(tmpfp, "%s = \"%s\"\n",
			"other_browser_cmd", get_other_browser_cmd());

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
	return;
}

static void do_reconfig(void) {
	save_config();

	/* Try to send SIGHUP to any running browser-switchboard process
	   This causes it to reread config files if in continuous_mode, or
	   die so that the config will be reloaded on next start otherwise */
	system("kill -HUP `pidof browser-switchboard` > /dev/null 2>&1");
}


/**********************************************************************
 * Callbacks
 **********************************************************************/

#if defined(HILDON) && defined(FREMANTLE)
static void default_browser_selector_callback(GtkWidget *widget,
		gint column, gpointer data) {
#else
static void default_browser_combo_callback(GtkWidget *widget, gpointer data) {
#endif
	if (!strcmp(get_default_browser(), "other")) {
		gtk_editable_set_editable(GTK_EDITABLE(cw.other_browser_cmd_entry), TRUE);
		gtk_widget_set_sensitive(cw.other_browser_cmd_entry, TRUE);
		gtk_widget_set_sensitive(cw.other_browser_cmd_entry_label, TRUE);
	} else {
		gtk_editable_set_editable(GTK_EDITABLE(cw.other_browser_cmd_entry), FALSE); /* FREMANTLE: give the text the greyed-out look */
		gtk_widget_set_sensitive(cw.other_browser_cmd_entry, FALSE);
		gtk_widget_set_sensitive(cw.other_browser_cmd_entry_label, FALSE);
	}
}


/**********************************************************************
 * Interface
 **********************************************************************/

#if defined(HILDON) && defined(FREMANTLE)
/*
 * Fremantle Hildon dialog
 */
static GtkDialog *swb_config_dialog(gpointer cp_window) {
	GtkWidget *dialog_vbox;

	GtkWidget *default_browser_selector_button;
	GtkWidget *continuous_mode_selector_button;
	int i;
	HildonGtkInputMode input_mode;

	dialog = gtk_dialog_new_with_buttons(
		"Browser Switchboard",
		GTK_WINDOW(cp_window),
		GTK_DIALOG_MODAL,
		GTK_STOCK_OK,
		GTK_RESPONSE_OK,
		GTK_STOCK_CANCEL,
		GTK_RESPONSE_CANCEL,
		NULL);

	dialog_vbox = GTK_DIALOG(dialog)->vbox;

	/* Config options */
	cw.default_browser_selector = hildon_touch_selector_new_text();
	for (i = 0; browsers[i].config; ++i)
		hildon_touch_selector_append_text(HILDON_TOUCH_SELECTOR(cw.default_browser_selector), browsers[i].displayname);
	hildon_touch_selector_set_active(HILDON_TOUCH_SELECTOR(cw.default_browser_selector), 0, 0);
	default_browser_selector_button = hildon_picker_button_new(_HILDON_SIZE_DEFAULT, HILDON_BUTTON_ARRANGEMENT_HORIZONTAL);
	hildon_button_set_title(HILDON_BUTTON(default_browser_selector_button),
				"Default browser:");
	hildon_picker_button_set_selector(HILDON_PICKER_BUTTON(default_browser_selector_button), HILDON_TOUCH_SELECTOR(cw.default_browser_selector));
	hildon_button_set_alignment(HILDON_BUTTON(default_browser_selector_button),
				    0, 0.5, 0, 0);
	g_signal_connect(G_OBJECT(cw.default_browser_selector), "changed",
			 G_CALLBACK(default_browser_selector_callback), NULL);
	gtk_box_pack_start(GTK_BOX(dialog_vbox),
			   default_browser_selector_button, FALSE, FALSE, 0);

	cw.other_browser_cmd_entry = hildon_entry_new(_HILDON_SIZE_DEFAULT);
	/* Disable autocapitalization and dictionary features for the entry */
	input_mode = hildon_gtk_entry_get_input_mode(GTK_ENTRY(cw.other_browser_cmd_entry));
	input_mode &= ~(HILDON_GTK_INPUT_MODE_AUTOCAP |
			HILDON_GTK_INPUT_MODE_DICTIONARY);
	hildon_gtk_entry_set_input_mode(GTK_ENTRY(cw.other_browser_cmd_entry), input_mode);

	cw.other_browser_cmd_entry_label = hildon_caption_new(NULL,
			"Command (%s for URI):",
			cw.other_browser_cmd_entry,
			NULL, HILDON_CAPTION_OPTIONAL);
	gtk_widget_set_sensitive(cw.other_browser_cmd_entry, FALSE);
	gtk_widget_set_sensitive(cw.other_browser_cmd_entry_label, FALSE);
	hildon_gtk_widget_set_theme_size(cw.other_browser_cmd_entry_label, _HILDON_SIZE_DEFAULT);
	gtk_box_pack_start(GTK_BOX(dialog_vbox),
			   cw.other_browser_cmd_entry_label, FALSE, FALSE, 0);

	cw.continuous_mode_selector = hildon_touch_selector_new_text();
	hildon_touch_selector_append_text(HILDON_TOUCH_SELECTOR(cw.continuous_mode_selector), "Lower memory usage");
	hildon_touch_selector_append_text(HILDON_TOUCH_SELECTOR(cw.continuous_mode_selector), "Faster browser startup time");

	continuous_mode_selector_button = hildon_picker_button_new(_HILDON_SIZE_DEFAULT, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
	hildon_button_set_title(HILDON_BUTTON(continuous_mode_selector_button),
				"Optimize Browser Switchboard for:");
	hildon_picker_button_set_selector(HILDON_PICKER_BUTTON(continuous_mode_selector_button), HILDON_TOUCH_SELECTOR(cw.continuous_mode_selector));
	hildon_button_set_alignment(HILDON_BUTTON(continuous_mode_selector_button),
				    0, 0, 0, 0);
	set_continuous_mode(CONTINUOUS_MODE_DEFAULT);
	gtk_box_pack_start(GTK_BOX(dialog_vbox),
			   continuous_mode_selector_button, FALSE, FALSE, 0);

	gtk_widget_show_all(dialog);
	return GTK_DIALOG(dialog);
}

#else /* !defined(HILDON) || !defined(FREMANTLE) */
/*
 * GTK+/Diablo Hildon dialog
 */
static GtkDialog *swb_config_dialog(gpointer cp_window) {
	GtkWidget *dialog_vbox;

	GtkWidget *options_table;
	GtkWidget *default_browser_combo_label;
	GtkWidget *continuous_mode_label;
	int i;
#ifdef HILDON
	HildonGtkInputMode input_mode;
#endif

	dialog = gtk_dialog_new_with_buttons(
		"Browser Switchboard",
		GTK_WINDOW(cp_window),
		GTK_DIALOG_MODAL,
		GTK_STOCK_OK,
		GTK_RESPONSE_OK,
		GTK_STOCK_CANCEL,
		GTK_RESPONSE_CANCEL,
		NULL);

	dialog_vbox = GTK_DIALOG(dialog)->vbox;

	/* Config options */
	options_table = gtk_table_new(3, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(options_table), 5);
	gtk_box_pack_start(GTK_BOX(dialog_vbox), options_table, FALSE, FALSE, 0);

	cw.default_browser_combo = gtk_combo_box_new_text();
	for (i = 0; browsers[i].config; ++i)
		gtk_combo_box_append_text(GTK_COMBO_BOX(cw.default_browser_combo),
					  browsers[i].displayname);
	gtk_combo_box_set_active(GTK_COMBO_BOX(cw.default_browser_combo), 0);
	default_browser_combo_label = gtk_label_new("Default browser:");
	gtk_misc_set_alignment(GTK_MISC(default_browser_combo_label), 1, 0.5);
	g_signal_connect(G_OBJECT(cw.default_browser_combo), "changed",
			 G_CALLBACK(default_browser_combo_callback), NULL);
	gtk_table_attach(GTK_TABLE(options_table),
			default_browser_combo_label,
			0, 1,
			0, 1,
			GTK_FILL, GTK_FILL|GTK_EXPAND,
			5, 0);
	gtk_table_attach(GTK_TABLE(options_table),
			cw.default_browser_combo,
			1, 2,
			0, 1,
			GTK_FILL|GTK_EXPAND, GTK_FILL|GTK_EXPAND,
			5, 0);

	cw.other_browser_cmd_entry = gtk_entry_new();
#ifdef HILDON
	/* Disable autocapitalization and dictionary features for the entry */
	input_mode = hildon_gtk_entry_get_input_mode(GTK_ENTRY(cw.other_browser_cmd_entry));
	input_mode &= ~(HILDON_GTK_INPUT_MODE_AUTOCAP |
			HILDON_GTK_INPUT_MODE_DICTIONARY);
	hildon_gtk_entry_set_input_mode(GTK_ENTRY(cw.other_browser_cmd_entry), input_mode);
#endif
	cw.other_browser_cmd_entry_label = gtk_label_new("Command (%s for URI):");
	gtk_misc_set_alignment(GTK_MISC(cw.other_browser_cmd_entry_label), 1, 0.5);
	gtk_widget_set_sensitive(cw.other_browser_cmd_entry, FALSE);
	gtk_widget_set_sensitive(cw.other_browser_cmd_entry_label, FALSE);
	gtk_table_attach(GTK_TABLE(options_table),
			cw.other_browser_cmd_entry_label,
			0, 1,
			1, 2,
			GTK_FILL, GTK_FILL|GTK_EXPAND,
			5, 0);
	gtk_table_attach(GTK_TABLE(options_table),
			cw.other_browser_cmd_entry,
			1, 2,
			1, 2,
			GTK_FILL|GTK_EXPAND, GTK_FILL|GTK_EXPAND,
			5, 0);
	gtk_table_set_row_spacing(GTK_TABLE(options_table), 1, 15);

	continuous_mode_label = gtk_label_new("Optimize Browser Switchboard for:");
	gtk_misc_set_alignment(GTK_MISC(continuous_mode_label), 0, 0.5);
	cw.continuous_mode_off_radio = gtk_radio_button_new_with_label(NULL,
			"Lower memory usage");
	cw.continuous_mode_on_radio = gtk_radio_button_new_with_label_from_widget(
			GTK_RADIO_BUTTON(cw.continuous_mode_off_radio),
			"Faster browser startup time");
	set_continuous_mode(CONTINUOUS_MODE_DEFAULT);
	gtk_table_attach(GTK_TABLE(options_table),
			continuous_mode_label,
			0, 2,
			2, 3,
			GTK_FILL, GTK_FILL|GTK_EXPAND,
			5, 0);
	gtk_table_attach(GTK_TABLE(options_table),
			cw.continuous_mode_off_radio,
			0, 2,
			3, 4,
			GTK_FILL, GTK_FILL|GTK_EXPAND,
			20, 0);
	gtk_table_attach(GTK_TABLE(options_table),
			cw.continuous_mode_on_radio,
			0, 2,
			4, 5,
			GTK_FILL, GTK_FILL|GTK_EXPAND,
			20, 5);
	gtk_table_set_row_spacing(GTK_TABLE(options_table), 3, 0);


	gtk_widget_show_all(dialog);
	return GTK_DIALOG(dialog);
}

#endif /* defined(HILDON) && defined(FREMANTLE) */


/**********************************************************************
 * Entry
 **********************************************************************/

#ifdef HILDON_CP_APPLET
/*
 * Application was started from control panel.
 */
osso_return_t execute(osso_context_t *osso,
		      gpointer userdata, gboolean user_activated) {
	GtkDialog *dialog;
	gint response;

	if (osso == NULL)
		return OSSO_ERROR;

	dialog = GTK_DIALOG(swb_config_dialog(userdata));
	load_config();

	response = gtk_dialog_run(dialog);
	if (response == GTK_RESPONSE_OK)
		do_reconfig();

	gtk_widget_destroy(GTK_WIDGET(dialog));

	return OSSO_OK;
}
#else
/*
 * Application was started from command line.
 */
int main(int argc, char *argv[]) {
	GtkDialog *dialog;
	gint response;
#ifdef HILDON
	HildonProgram *program = NULL;
#endif

	gtk_init(&argc, &argv);
#ifdef HILDON
	program = HILDON_PROGRAM(hildon_program_get_instance());
#endif

	g_set_application_name("Browser Switchboard");

	dialog = GTK_DIALOG(swb_config_dialog(NULL));
	load_config();

	response = gtk_dialog_run(dialog);
	if (response == GTK_RESPONSE_OK)
		do_reconfig();

	gtk_widget_destroy(GTK_WIDGET(dialog));

	exit(0);
}
#endif /* HILDON_CP_APPLET */
