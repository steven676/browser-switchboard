/*
 * launcher.h -- definitions for the browser launching functions
 *
 * Copyright (C) 2009 Steven Luo
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

#ifndef _LAUNCHER_H
#define _LAUNCHER_H 1

#include "browser-switchboard.h"

void launch_microb(struct swb_context * ctx, char * uri);
void launch_browser(struct swb_context * ctx, char * uri);
void update_default_browser(struct swb_context * ctx, char * default_browser);

#endif /* _LAUNCHER_H */
