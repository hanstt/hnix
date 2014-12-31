#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_PS "/sys/class/power_supply"
#define MATCH(name) 0 == strncmp(name, buf, sizeof(name) - 1)
#define STRTOL(p) strtol(p, NULL, 10)
#define SYSFSFIND(dir, name) sysfsfind(dir, name, sizeof name - 1)

enum {
	TYPE_NONE,
	TYPE_AC,
	TYPE_BAT
};

static char	*sysfsfind(DIR *, char const *, size_t);

char *
sysfsfind(DIR *const a_dir, char const *const a_type, size_t const a_type_len)
{
	for (;;) {
		char type_path[256];
		FILE *file;
		struct dirent *dirent;

		errno = 0;
		dirent = readdir(a_dir);
		if (NULL == dirent) {
			if (0 != errno) {
				printf("ACPI readddir (%s)\n",
				    strerror(errno));
				exit(0);
			}
			break;
		}
		snprintf(type_path, sizeof type_path, PATH_PS"/%s/type",
		    dirent->d_name);
		file = fopen(type_path, "rb");
		if (NULL != file) {
			char line[80];

			while (fgets(line, sizeof line, file) != NULL) {
				if (0 == strncmp(a_type, line, a_type_len)) {
					fclose(file);
					return strdup(dirent->d_name);
				}
			}
			fclose(file);
		}
	}
	return NULL;
}

int
main(int argc, char **argv)
{
	char buf[256];
	FILE *file;
	DIR *dir;
	char *path;
	int capacity, now, percentage, percentage_min, rate, remain, type;

	type = TYPE_NONE;
	capacity = 0;
	now = 0;
	rate = 0;

	dir = opendir(PATH_PS);
	if (NULL == dir) {
		printf("No PS sysfs (%s)\n", strerror(errno));
		return 0;
	}
	path = SYSFSFIND(dir, "Mains");
	if (NULL == path) {
		printf("No AC");
		closedir(dir);
		return 0;
	}
	snprintf(buf, sizeof buf, PATH_PS"/%s/uevent", path);
	free(path);
	file = fopen(buf, "rb");
	if (NULL != file) {
		while (fgets(buf, sizeof buf, file) != NULL) {
			if (0 == strncmp("POWER_SUPPLY_ONLINE=", buf, 20)) {
				type = '0' == buf[20] ? TYPE_BAT : TYPE_AC;
				break;
			}
		}
		fclose(file);
	}
	if (type == TYPE_NONE) {
		printf("No AC");
		closedir(dir);
		return 0;
	}

	rewinddir(dir);
	path = SYSFSFIND(dir, "Battery");
	closedir(dir);
	if (NULL != path) {
		snprintf(buf, sizeof buf, PATH_PS"/%s/uevent", path);
		free(path);
		file = fopen(buf, "rb");
		if (NULL != file) {
			while (fgets(buf, sizeof buf, file) != NULL) {
				if (MATCH("POWER_SUPPLY_CHARGE_FULL=")) {
					capacity = STRTOL(buf + 25);
					continue;
				}
				if (MATCH("POWER_SUPPLY_CHARGE_NOW=")) {
					now = STRTOL(buf + 24);
					continue;
				}
				if (MATCH("POWER_SUPPLY_CURRENT_NOW=")) {
					rate = STRTOL(buf + 25);
					continue;
				}
				if (MATCH("POWER_SUPPLY_ENERGY_FULL=")) {
					capacity = STRTOL(buf + 25);
					continue;
				}
				if (MATCH("POWER_SUPPLY_ENERGY_NOW=")) {
					now = STRTOL(buf + 24);
					continue;
				}
				if (MATCH("POWER_SUPPLY_POWER_NOW=")) {
					rate = STRTOL(buf + 23);
					continue;
				}
			}
			fclose(file);
		}
	}

	percentage = (100.0 * now) / capacity;
	if (TYPE_BAT == type) {
		percentage_min = 2 == argc ? STRTOL(argv[1]) : 0;
		percentage_min = 5 > percentage_min ? 5 : percentage_min;
		if (percentage <= percentage_min) {
			printf("!");
		}
		printf("Bat");
		remain = now;
	} else {
		printf("AC");
		remain = capacity - now;
	}
	printf(" %d%%", percentage);
	if (0 != rate) {
		printf(" (%d:%02d)", remain / rate, remain / (rate / 60) %
		    60);
	}
	return 0;
}
