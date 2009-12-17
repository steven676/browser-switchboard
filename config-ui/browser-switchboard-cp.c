/*
 * browser-switchboard-cp.c -- a hildon-control-panel applet for
 * selecting the default browser for Browser Switchboard
 * 
 * Copyright (C) 2009 Steven Luo
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
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#ifdef HILDON
#include <hildon/hildon-banner.h>
#include <hildon/hildon-program.h>
#ifdef HILDON_CP_APPLET
#include <hildon-cp-plugin/hildon-cp-plugin-interface.h>
#endif
#endif

#include "configfile.h"

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
	GtkWidget *continuous_mode_check;
	GtkWidget *default_browser_combo;
	GtkWidget *other_browser_cmd_entry;
	GtkWidget *other_browser_cmd_entry_label;
};
struct config_widgets cw;
GtkWidget *dialog;


/**********************************************************************
 * Configuration routines
 **********************************************************************/

static inline int get_continuous_mode(void) {
	return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cw.continuous_mode_check));
}
static inline void set_continuous_mode(int state) {
	return gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cw.continuous_mode_check), (gboolean)state);
}

static inline char * get_default_browser(void) {
	return browsers[gtk_combo_box_get_active(GTK_COMBO_BOX(cw.default_browser_combo))].config;
}
static void set_default_browser(char *browser) {
	gint i;

	/* Loop through browsers looking for a match */
	for (i = 0; browsers[i].config && strcmp(browsers[i].config, browser);
			++i);

	if (!browsers[i].config)
		/* No match found, set to the default browser */
		i = 0;

	gtk_combo_box_set_active(GTK_COMBO_BOX(cw.default_browser_combo), i);
}

static inline char * get_other_browser_cmd(void) {
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

	/* Put together the path to the new config file and the tempfile */
	if (!(homedir = getenv("HOME")))
		homedir = DEFAULT_HOMEDIR;
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

/**********************************************************************
 * Callbacks
 **********************************************************************/

static void default_browser_combo_callback(GtkWidget *widget, gpointer data) {
	if (!strcmp(get_default_browser(), "other")) {
		gtk_widget_set_sensitive(cw.other_browser_cmd_entry, TRUE);
		gtk_widget_set_sensitive(cw.other_browser_cmd_entry_label, TRUE);
	} else {
		gtk_widget_set_sensitive(cw.other_browser_cmd_entry, FALSE);
		gtk_widget_set_sensitive(cw.other_browser_cmd_entry_label, FALSE);
	}
}

static inline void close_dialog(void) {
	gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_NONE);
}

static void ok_callback(GtkWidget *widget, gpointer data) {
	save_config();
	/* TODO: is there any cleanup necessary? */
	close_dialog();
}

static void cancel_callback(GtkWidget *widget, gpointer data) {
	/* TODO: is there any cleanup necessary? */
	close_dialog();
}


/**********************************************************************
 * Interface
 **********************************************************************/

static GtkDialog * swb_config_dialog(void) {
	GtkWidget *dialog_vbox;

	GtkWidget *options_table;
	GtkWidget *default_browser_combo_label;
	int i;

	GtkWidget *action_area;
	GtkWidget *okbutton, *cancelbutton;

	dialog = gtk_dialog_new();
	gtk_widget_set_size_request(GTK_WIDGET(dialog), 580, 240);
	gtk_window_set_title (GTK_WINDOW(dialog), "Browser Switchboard");
	gtk_window_set_type_hint (GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);

	dialog_vbox = GTK_DIALOG(dialog)->vbox;

	/* Config options */
	options_table = gtk_table_new(3, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(options_table), 10);
	gtk_table_set_col_spacings(GTK_TABLE(options_table), 10);
	gtk_box_pack_start(GTK_BOX(dialog_vbox), options_table, FALSE, FALSE, 0);

	cw.default_browser_combo = gtk_combo_box_new_text();
	for (i = 0; browsers[i].config; ++i)
		gtk_combo_box_append_text(GTK_COMBO_BOX(cw.default_browser_combo),
					  browsers[i].displayname);
	gtk_combo_box_set_active(GTK_COMBO_BOX(cw.default_browser_combo), 0);
	default_browser_combo_label = gtk_label_new("Default browser:");
	gtk_misc_set_alignment(GTK_MISC(default_browser_combo_label), 0, 0.5);
	g_signal_connect(G_OBJECT(cw.default_browser_combo), "changed",
			 G_CALLBACK(default_browser_combo_callback), NULL);
	gtk_table_attach(GTK_TABLE(options_table),
			default_browser_combo_label,
			0, 1,
			0, 1,
			GTK_FILL, GTK_FILL|GTK_EXPAND,
			0, 0);
	gtk_table_attach_defaults(GTK_TABLE(options_table),
			cw.default_browser_combo,
			1, 2,
			0, 1);
	gtk_table_set_row_spacing(GTK_TABLE(options_table), 0, 5);

	cw.other_browser_cmd_entry = gtk_entry_new();
	cw.other_browser_cmd_entry_label = gtk_label_new("Command (%s for URI):");
	gtk_misc_set_alignment(GTK_MISC(cw.other_browser_cmd_entry_label), 0, 0.5);
	gtk_widget_set_sensitive(cw.other_browser_cmd_entry, FALSE);
	gtk_widget_set_sensitive(cw.other_browser_cmd_entry_label, FALSE);
	gtk_table_attach(GTK_TABLE(options_table),
			cw.other_browser_cmd_entry_label,
			0, 1,
			1, 2,
			0, GTK_FILL|GTK_EXPAND,
			0, 0);
	gtk_table_attach_defaults(GTK_TABLE(options_table),
			cw.other_browser_cmd_entry,
			1, 2,
			1, 2);

	cw.continuous_mode_check = gtk_check_button_new_with_label("Run browser launcher continuously in the background");
	gtk_table_attach_defaults(GTK_TABLE(options_table),
			cw.continuous_mode_check,
			0, 2,
			2, 3);


	/* Dialog buttons */
	action_area = GTK_DIALOG(dialog)->action_area;

	okbutton = gtk_button_new_from_stock(GTK_STOCK_OK);
	g_signal_connect(G_OBJECT(okbutton), "clicked",
			 G_CALLBACK(ok_callback), NULL);
	gtk_box_pack_start(GTK_BOX(action_area), okbutton, FALSE, FALSE, 0);

	cancelbutton = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	g_signal_connect(G_OBJECT(cancelbutton), "clicked",
			 G_CALLBACK(cancel_callback), NULL);
	gtk_box_pack_start(GTK_BOX(action_area), cancelbutton, FALSE, FALSE, 0);

	gtk_widget_show_all(dialog);
	return GTK_DIALOG(dialog);
}


/**********************************************************************
 * Entry
 **********************************************************************/

#ifdef HILDON_CP_APPLET
/*
 * Application was started from control panel.
 */
osso_return_t execute(osso_context_t * osso,
		      gpointer userdata, gboolean user_activated)
{
	HildonProgram *program;
	program = HILDON_PROGRAM(hildon_program_get_instance());
	
	if (osso == NULL)
		return OSSO_ERROR;

	/* enable help system on dialog */
	GtkDialog * dialog = GTK_DIALOG(swb_config_dialog());

	load_config();

	gtk_dialog_run(dialog);
	gtk_widget_destroy(GTK_WIDGET(dialog));

	return OSSO_OK;
}
#else
/*
 * Application was started from command line.
 */
int main(int argc, char *argv[])
{
	GtkDialog *dialog;
#ifdef HILDON
	HildonProgram *program = NULL;
	program = HILDON_PROGRAM(hildon_program_get_instance());
#endif

	gtk_init(&argc, &argv);

	g_set_application_name("Browser Switchboard");

	dialog = GTK_DIALOG(swb_config_dialog());
	load_config();
	gtk_dialog_run(dialog);
	gtk_widget_destroy(GTK_WIDGET(dialog));

	exit(0);
}
#endif
