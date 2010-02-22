/*
 * log.c -- logging functions for browser-switchboard
 *
 * Copyright (C) 2010 Steven Luo
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

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>

#include "log.h"

#define DEFAULT_LOGGER LOGTO_STDOUT
static enum {
	LOGTO_NONE,
	LOGTO_STDOUT,
	LOGTO_SYSLOG,
} logger = DEFAULT_LOGGER;

/* Configure the logging target, performing any required setup for that
   target */
void log_config(char *logger_name) {
	if (!logger_name) {
		/* No logger configured, use the default log target */
		logger = DEFAULT_LOGGER;
		return;
	}

	if (!strcmp(logger_name, "stdout"))
		logger = LOGTO_STDOUT;
	else if (!strcmp(logger_name, "syslog")) {
		/* XXX allow syslog facility to be configured? */
		openlog("browser-switchboard", LOG_PID, LOG_USER);
		logger = LOGTO_SYSLOG;
	}
	else if (!strcmp(logger_name, "none"))
		logger = LOGTO_NONE;
	else
		/* Invalid logger configured, use the default log target */
		logger = DEFAULT_LOGGER;

	return;
}

/* Log output to the chosen log target */
void log_msg(const char *format, ...) {
	va_list ap;

	if (!format)
		return;

	va_start(ap, format);
	switch (logger) {
		case LOGTO_NONE:
			break;
		case LOGTO_SYSLOG:
			/* XXX allow syslog priority to be set by caller? */
			vsyslog(LOG_DEBUG, format, ap);
			break;
		case LOGTO_STDOUT:
		default:
			vprintf(format, ap);
			break;
	}
	va_end(ap);
}

/* Log strerror(errnum), with the string in prefix appended
   Behaves like perror() except that it logs to chosen target, not stderr */
void log_perror(int errnum, const char *prefix) {
	char *errmsg;

	if (!prefix)
		return;
	if (!(errmsg = strerror(errnum)))
		return;

	log_msg("%s: %s\n", prefix, errmsg);
}
