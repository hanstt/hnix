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

#include <sys/queue.h>
#include <sys/stat.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DB_PATH "/var/db/pkg/"
#define CONTENTS "+CONTENTS"
#define DESC "+DESC"
#define OPTION_MANUAL "@option manual-installation"
#define REQUIRED_BY "+REQUIRED_BY"
#define REQUIRING "+REQUIRING"

TAILQ_HEAD(PackageList, Package);
struct Package {
	char	name[256];
	int	do_delete;
	TAILQ_ENTRY(Package)	next;
};

static int	argmatch(char const *, char const *);
static void	ask(struct PackageList *, size_t);
static int	is_manual(char const *);
static int	is_required(char const *);

int
argmatch(char const *const a_request, char const *const a_match)
{
	size_t request_len;

	request_len = strlen(a_request);
	if (0 == request_len) {
		return 0;
	}
	return 0 == strncmp(a_request, a_match, request_len);
}

void
ask(struct PackageList *const a_list, size_t a_list_length)
{
	int do_print;

	do_print = 1;
	for (;;) {
		char buf[10];
		struct Package *pkg;
		char const *errstr;
		int id;

		if (do_print) {
			printf("Leaf packages to delete:\n");
			id = 0;
			TAILQ_FOREACH(pkg, a_list, next) {
				printf(" %2d [%c]: %s\n", id, pkg->do_delete ?
				    'x' : ' ', pkg->name);
				++id;
			}
			do_print = 0;
		}
		printf("> ");
		fflush(stdout);
		if (NULL == fgets(buf, sizeof buf, stdin)) {
			continue;
		}
		{
			size_t i;

			for (i = strlen(buf) - 1; 0 < i; --i) {
				if (!isspace(buf[i])) {
					break;
				}
			}
			buf[i + 1] = '\0';
		}
		if (argmatch(buf, "help")) {
			printf(" help  - Print this.\n");
			printf(" print - Print package list.\n");
			printf(" quit  - Quit without doing anything.\n");
			printf(" yes   - Delete selected packages!\n");
			printf(" #     - Toggle package for deletion.\n");
			continue;
		}
		if (argmatch(buf, "print")) {
			do_print = 1;
			continue;
		}
		if (argmatch(buf, "quit")) {
			TAILQ_FOREACH(pkg, a_list, next) {
				pkg->do_delete = 0;
			}
			return;
		}
		if (argmatch(buf, "yes")) {
			return;
		}
		id = strtonum(buf, 0, a_list_length, &errstr);
		if (NULL != errstr) {
			fprintf(stderr, "strtonum: %s\n", errstr);
			continue;
		}
		TAILQ_FOREACH(pkg, a_list, next) {
			if (0 == id) {
				break;
			}
			--id;
		}
		pkg->do_delete ^= 1;
	}
}

int
is_manual(char const *const a_name)
{
	FILE *file;
	char *path;
	size_t name_len, path_len;
	int num, ret;

	name_len = strlen(a_name);
	path_len = sizeof(DB_PATH)-1 + name_len + 1 + sizeof(CONTENTS)-1 + 1;
	path = malloc(path_len);
	num = snprintf(path, path_len, DB_PATH"%s/"CONTENTS, a_name);
	assert(num == (int)path_len - 1);
	file = fopen(path, "rb");
	if (NULL == file) {
		err(EXIT_FAILURE, "fopen(%s)", path);
	}
	free(path);
	for (;;) {
		char line[256];

		if (NULL == fgets(line, sizeof line, file)) {
			ret = 0;
			break;
		}
		if (0 == strncmp(line, OPTION_MANUAL, sizeof OPTION_MANUAL -
		    1)) {
			ret = 1;
			break;
		}
	}
	fclose(file);
	return ret;
}

int
is_required(char const *const a_name)
{
	struct stat sb;
	char *path;
	size_t name_len, path_len;
	int num, ret;

	name_len = strlen(a_name);
	path_len = sizeof(DB_PATH)-1 + name_len + 1 + sizeof(REQUIRED_BY)-1 +
	    1;
	path = malloc(path_len);
	num = snprintf(path, path_len, DB_PATH"%s/"REQUIRED_BY, a_name);
	assert(num == (int)path_len - 1);
	ret = stat(path, &sb);
	free(path);
	return 0 == ret;
}

int
main(void)
{
	int deleted_num;

	do {
		struct PackageList list;
		DIR *dir;
		struct Package *pkg;
		size_t list_length;

		dir = opendir(DB_PATH);
		if (NULL == dir) {
			err(EXIT_FAILURE, "opendir("DB_PATH")");
		}

		/*
		 * Find automatically installed packages which are not
		 * required by any others.
		 */
		TAILQ_INIT(&list);
		list_length = 0;
		for (;;) {
			struct dirent dent;
			struct dirent *result;

			readdir_r(dir, &dent, &result);
			if (NULL == result) {
				break;
			}
			if (&dent != result) {
				err(EXIT_FAILURE, "readdir_r");
			}
			if ('.' == dent.d_name[0]) {
				continue;
			}
			if (is_manual(dent.d_name)) {
				continue;
			}
			if (is_required(dent.d_name)) {
				continue;
			}

			pkg = malloc(sizeof *pkg);
			strlcpy(pkg->name, dent.d_name, sizeof pkg->name);
			pkg->do_delete = 1;
			TAILQ_INSERT_TAIL(&list, pkg, next);
			++list_length;
		}
		closedir(dir);
		if (TAILQ_EMPTY(&list)) {
			printf("No leaf packages found.\n");
			break;
		}

		ask(&list, list_length);

		/* Delete packages and clean up list. */
		deleted_num = 0;
		while (!TAILQ_EMPTY(&list)) {
			pkg = TAILQ_FIRST(&list);
			if (pkg->do_delete) {
				char *cmd;
				size_t cmd_len;

				cmd_len = sizeof("pkg_delete ")-1 +
				    strlen(pkg->name) + 1;
				cmd = malloc(cmd_len);
				snprintf(cmd, cmd_len, "pkg_delete %s",
				    pkg->name);
				system(cmd);
				free(cmd);
				++deleted_num;
			}
			TAILQ_REMOVE(&list, pkg, next);
			free(pkg);
		}
	} while (0 < deleted_num);

	return 0;
}
