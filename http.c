/* http.c
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
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "error.h"
#include "http.h"

static char* strnstrr(const char *str, const char *needle, size_t size) {

	char *ptr;
	size_t len, pos;

	if (!needle || !*needle)
		return (char *) str;

	len = strlen(needle);
	pos = size;
	for(ptr=(char*)str; *ptr; ptr = memchr(ptr+1, *needle, pos-1)) {
		pos = size - (ptr - str);
		if (pos < len)
			break;
		if (!strncmp(ptr, needle, len))
			return ptr + len;
	}
	return NULL;
}

static const char* url_filename(const char *url) {

	const char *start = url;

	for(; *url; url++) {
		if (*url != '/')
			continue;
		if (*(url+1) == 0)
			break;
		start = url + 1;
	}
	return start;
}

#define HDR_CONDISP "Content-Disposition:"

static size_t hdr_fname_cb(void *src, size_t smemb, size_t nmemb, void *data) {

	int pos, size = smemb * nmemb;
	char *ptr = (char *) src;
	char **filename = (char**) data;

	if (*filename || size < sizeof(HDR_CONDISP)-1 ||
		memcmp(ptr, HDR_CONDISP, sizeof(HDR_CONDISP)-1))
		return size;

	pos = sizeof(HDR_CONDISP)-1;
	ptr = strnstrr(ptr + pos, "filename=\"", size);
	if (ptr) {
		int start, len;
		start = pos = ptr - ((char*) src);
		ptr = (char *) src;
		for(len=0;;len++) {
			if (ptr[pos] == '"' && ptr[pos-1] != '\\')
				break;
			if (++pos > size)
				return 0;
		}
		if (len)
			*filename = strndup(ptr + start, len);
	}
	return size;
}

static size_t write_cb(void *src, size_t smemb, size_t nmemb, void *data) {

	struct http_data *dest = (struct http_data *) data;
	size_t size = smemb * nmemb;

	dest->block = realloc(dest->block, dest->len + size);
	if (dest->block == NULL) {
		error("out of memory");
		return 0;
	}
	memcpy(dest->block + dest->len, src, size);
	dest->len += size;

	return size;
}

static CURL* setup_connection(const char *url) {

	CURL *handle = curl_easy_init();

	curl_easy_setopt(handle, CURLOPT_URL, url);
	curl_easy_setopt(handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0);
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(handle, CURLOPT_TIMEOUT, 10);

	return handle;
}

struct http_data* http_fetch_page(const char *url) {

	CURL *handle;
	CURLcode res;
	struct http_data *data = malloc(sizeof(struct http_data));

	data->block = NULL;
	data->len = 0;

	handle = setup_connection(url);

	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, data);

	res = curl_easy_perform(handle);

	if (res != CURLE_OK) {
		error("curl: (%s) %s", url, curl_easy_strerror(res));
		goto error;
	}

	curl_easy_cleanup(handle);

	return data;
error:
	curl_easy_cleanup(handle);
	http_free(data);
	return NULL;
}

int http_download_file(const char *url, const char *dir) {

	int err;
	CURL *handle = NULL;
	CURLcode res;
	FILE *fd;
	char tmpfile[4096], *filename = NULL;

	/* Construct an filename from url. */
	snprintf(tmpfile, sizeof(tmpfile), "%s/%s", dir, url_filename(url));

	fd = fopen(tmpfile, "w");
	if (!fd)
		goto error;

	handle = setup_connection(url);

	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, fwrite);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, fd);

	/* try look for the real filename in the http header */
	curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, hdr_fname_cb);
	curl_easy_setopt(handle, CURLOPT_HEADERDATA, &filename);

	res = curl_easy_perform(handle);

	if (res != CURLE_OK) {
		error("curl: (%s) %s", url, curl_easy_strerror(res));
		goto error;
	}

	if (filename) {
		/* found the real file in http header.
		   move the old file. */
		char realfile[4096];

		snprintf(realfile, sizeof(realfile), "%s/%s", dir, filename);
		if (rename(tmpfile, realfile) < 0)
			goto error;
		free(filename);
	} else {
		fclose(fd);
	}

	curl_easy_cleanup(handle);

	return 0;
error:
	err = errno;
	if (filename)
		free(filename);
	fclose(fd);
	curl_easy_cleanup(handle);
	errno = err;
	return -1;
}

void http_free(struct http_data *data) {

	if (!data)
		return;
	if (data->block)
		free(data->block);
	free(data);
}
