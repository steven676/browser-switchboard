/*
 * browser-switchboard-cp.c -- a hildon-control-panel applet for
 * selecting the default browser for Browser Switchboard
 * 
 * Copyright (C) 2009 Steven Luo
 * 
 * Derived from services-cp.c from maemo-control-services
 * Copyright (c) 2008 Janne Kataja <janne.kataja@iki.fi>
 * Copyright (c) 2008 Nokia Corporation
 * Contact: integration@maemo.org
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


#include <stdio.h>
#include <string.h>
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

static inline int get_continuous_mode(void) {
	return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cw.continuous_mode_check));
}
static inline char * get_default_browser(void) {
	return browsers[gtk_combo_box_get_active(GTK_COMBO_BOX(cw.default_browser_combo))].config;
}
static inline char * get_other_browser_cmd(void) {
	return (char *)gtk_entry_get_text(GTK_ENTRY(cw.other_browser_cmd_entry));
}


static inline void close_dialog(void) {
	gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_NONE);
}

static void do_reconfig(void) {
	printf("continuous_mode: %d\n", get_continuous_mode());
	printf("default_browser: %s\n", get_default_browser());
	printf("other_browser_cmd: '%s'\n", get_other_browser_cmd());

	/* TODO: actually implement configuration */
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

static void ok_callback(GtkWidget *widget, gpointer data) {
	do_reconfig();
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
	data.osso_context = osso;

	/* enable help system on dialog */
	GtkDialog * dialog = GTK_DIALOG(swb_config_dialog());

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
	gtk_dialog_run(dialog);
	gtk_widget_destroy(GTK_WIDGET(dialog));

	return 0;
}
#endif
