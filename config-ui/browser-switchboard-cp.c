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

#include "config.h"
#include "save-config.h"
#include "browsers.h"

#define CONTINUOUS_MODE_DEFAULT 0

#if defined(HILDON) && defined(FREMANTLE)
#define _HILDON_SIZE_DEFAULT (HILDON_SIZE_AUTO_WIDTH|HILDON_SIZE_FINGER_HEIGHT)
#endif

struct swb_config orig_cfg;

struct config_widgets {
#if defined(HILDON) && defined(FREMANTLE)
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


struct browser_entry *installed_browsers;
void init_installed_browsers(void) {
	struct browser_entry *cur = browsers;
	unsigned int count = 0;

	installed_browsers = calloc(sizeof browsers, 1);
	if (!installed_browsers)
		exit(1);

	count = 0;
	for (; cur->config; ++cur)
		if (!cur->binary || !access(cur->binary, X_OK))
			installed_browsers[count++] = *cur;
}

/**********************************************************************
 * Configuration routines
 **********************************************************************/

#if defined(HILDON) && defined(FREMANTLE)

static inline char *get_default_browser(void) {
	return installed_browsers[hildon_touch_selector_get_active(HILDON_TOUCH_SELECTOR(cw.default_browser_selector), 0)].config;
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
	return installed_browsers[gtk_combo_box_get_active(GTK_COMBO_BOX(cw.default_browser_combo))].config;
}

#endif /* defined(HILDON) && defined(FREMANTLE) */

static void set_default_browser(char *browser) {
	gint i;

	/* Loop through browsers looking for a match */
	for (i = 0;
	     installed_browsers[i].config && strcmp(installed_browsers[i].config, browser);
	     ++i);

	if (!installed_browsers[i].config)
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
	swb_config_init(&orig_cfg);

	swb_config_load(&orig_cfg);

#ifndef FREMANTLE
	set_continuous_mode(orig_cfg.continuous_mode);
#endif
	set_default_browser(orig_cfg.default_browser);
	if (orig_cfg.other_browser_cmd)
		set_other_browser_cmd(orig_cfg.other_browser_cmd);
}

static void save_config(void) {
	struct swb_config new_cfg;

	new_cfg = orig_cfg;

#ifndef FREMANTLE
	if (get_continuous_mode() != orig_cfg.continuous_mode) {
		new_cfg.continuous_mode = get_continuous_mode();
		new_cfg.flags |= SWB_CONFIG_CONTINUOUS_MODE_SET;
	}
#endif
	if (strcmp(get_default_browser(), orig_cfg.default_browser)) {
		new_cfg.default_browser = get_default_browser();
		new_cfg.flags |= SWB_CONFIG_DEFAULT_BROWSER_SET;
	}
	if (strlen(get_other_browser_cmd()) == 0) {
		new_cfg.other_browser_cmd = NULL;
		new_cfg.flags &= ~SWB_CONFIG_OTHER_BROWSER_CMD_SET;
	} else if (!(orig_cfg.other_browser_cmd &&
		     !strcmp(get_other_browser_cmd(),
			     orig_cfg.other_browser_cmd))) {
		new_cfg.other_browser_cmd = get_other_browser_cmd();
		new_cfg.flags |= SWB_CONFIG_OTHER_BROWSER_CMD_SET;
	}

	swb_config_save(&new_cfg);

	/* Reconfigure a running browser-switchboard, if present */
	swb_reconfig(&orig_cfg, &new_cfg);
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
	init_installed_browsers();
	cw.default_browser_selector = hildon_touch_selector_new_text();
	for (i = 0; installed_browsers[i].config; ++i)
		hildon_touch_selector_append_text(HILDON_TOUCH_SELECTOR(cw.default_browser_selector), installed_browsers[i].displayname);
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

	init_installed_browsers();
	cw.default_browser_combo = gtk_combo_box_new_text();
	for (i = 0; installed_browsers[i].config; ++i)
		gtk_combo_box_append_text(GTK_COMBO_BOX(cw.default_browser_combo),
					  installed_browsers[i].displayname);
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
		save_config();

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
		save_config();

	gtk_widget_destroy(GTK_WIDGET(dialog));

	exit(0);
}
#endif /* HILDON_CP_APPLET */
