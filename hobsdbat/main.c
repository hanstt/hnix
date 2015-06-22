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
	capacity = 0.0;
	now = 0.0;
	rate = 0.0;

	pip = popen("sysctl " PATH_SYSCTL, "r");
	if (NULL == pip) {
		printf("No bat (%s)", strerror(errno));
		return 0;
	}
	while (fgets(buf, sizeof buf, pip) != NULL) {
		if (MATCH("raw0=1")) {
			type = TYPE_BAT;
		}
		if (MATCH("raw0=2")) {
			type = TYPE_AC;
		}
		if (MATCH("watthour0=")) {
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
	fclose(pip);

	percentage = (100.0 * now) / capacity;
	if (TYPE_BAT == type) {
		percentage_min = 2 == argc ? strtol(argv[1], NULL, 10) : 0;
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
	printf(" %d%%", (int)percentage);
	if (0 != rate) {
		printf(" (%d:%02d)", (int)(remain / rate), (int)(remain /
		    (rate / 60)) % 60);
	}
	return 0;
}
