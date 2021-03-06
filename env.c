/* env.c
 *
 *   Copyright (C) 2011       Henrik Hautakoski <henrik@fiktivkod.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *   MA 02110-1301, USA.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "error.h"
#include "buffer.h"
#include "env.h"

static char *base = NULL;

static void free_base() {

	if (base)
		free(base);
}

static void get_base() {

	struct buffer buf = BUFFER_INIT;
	char *ptr = getenv("HOME");

	if (!ptr)
		ptr = ".";
	buffer_append_str(&buf, ptr);
	buffer_append_str(&buf, "/.dlight");

	base = buffer_cstr_release(&buf);
	atexit(free_base);
}

const char* env_get_dir() {

	if (!base)
		get_base();

	if (mkdir(base, 0700) < 0 && errno != EEXIST) {
		fatal("Unable to create '%s': %s\n",
			base, strerror(errno));
	}
	return base;
}
