/*
 * Copyright (c) 2015 Hans Toshihide Törnqvist <hans.tornqvist@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 256
#define MAX_LEN 24

int
main(void)
{
	char buf[BUF_SIZE];
	char *head;
	char const *home;
	size_t headlen, homelen;
	int i, level;

	if (NULL == getcwd(buf, BUF_SIZE)) {
		puts("<no path>");
		return 0;
	}

	home = getenv("HOME");
	if (NULL == home) {
		home = "";
	}
	for (homelen = strlen(home) - 1; 0 < homelen; --homelen) {
		if ('/' != home[homelen]) {
			break;
		}
	}
	++homelen;
	if (0 < homelen && 0 == strncmp(buf, home, homelen)) {
		head = buf + homelen - 1;
		head[0] = '~';
	} else {
		head = buf;
	}

	level = 0;
	headlen = strlen(head);
	for (i = headlen - 1; 0 < i; --i) {
		if (MAX_LEN < headlen - i ||
		    ('/' == head[i] && 2 == ++level)) {
			break;
		}
	}

	if (1 == i && '~' == head[0]) {
		--i;
	} else if (0 < i) {
		printf("...");
	}
	puts(head + i);

	return 0;
}
