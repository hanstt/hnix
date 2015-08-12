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

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_SYSCTL "hw.sensors.acpibat0"
#define MATCH(name) 0 == strncmp(name, buf + 20, sizeof(name) - 1)

enum {
	TYPE_NONE,
	TYPE_AC,
	TYPE_BAT
};

int
main(int argc, char **argv)
{
	char buf[256];
	FILE *pip;
	double capacity, now, percentage, percentage_min, rate, remain;
	int type;

	type = TYPE_NONE;
	capacity = 0.;
	now = 0.;
	rate = 0.;

	pip = popen("sysctl " PATH_SYSCTL, "r");
	if (NULL == pip) {
		printf("No bat (%s)", strerror(errno));
		return 0;
	}
	while (NULL != fgets(buf, sizeof buf, pip)) {
		if (MATCH("raw0=1")) {
			type = TYPE_BAT;
		}
		if (MATCH("raw0=2")) {
			type = TYPE_AC;
		}
		if (MATCH("watthour4=")) {
			capacity = strtod(buf + 30, NULL);
			continue;
		}
		if (MATCH("watthour3=")) {
			now = strtod(buf + 30, NULL);
			continue;
		}
		if (MATCH("power0=")) {
			rate = strtod(buf + 27, NULL);
			continue;
		}
	}
	pclose(pip);

	percentage = (100. * now) / capacity;
	if (TYPE_BAT == type) {
		percentage_min = 2 == argc ? strtod(argv[1], NULL) : 0.;
		percentage_min = 5. > percentage_min ? 5. : percentage_min;
		if (percentage <= percentage_min) {
			printf("!");
		}
		printf("Bat");
		remain = now;
	} else {
		printf("AC");
		remain = capacity - now;
	}
	printf(" %d%%", (int)percentage);
	if (1e-3 < rate) {
		printf(" (%dh %02dm)", (int)(remain / rate), (int)(remain /
		    (rate / 60)) % 60);
	}
	return 0;
}
