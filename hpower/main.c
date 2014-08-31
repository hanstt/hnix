#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AC "/sys/class/power_supply/ACAD/"
#define BAT "/sys/class/power_supply/BAT1/"

enum {
	TYPE_NONE,
	TYPE_AC,
	TYPE_BAT
};

int
main(int argc, char **argv)
{
	char line[80];
	char status[80];
	FILE *file;
	int type = TYPE_NONE;
	int capacity = 0;
	int now = 0;
	int percentage = 0;
	int percentageMin = 0;
	int rate = 0;
	int remain = 0;

	if ((file = fopen(AC"uevent", "rb")) != NULL) {
		while (fgets(line, 80, file) != NULL) {
			if (0 == strncmp("POWER_SUPPLY_ONLINE=", line, 20)) {
				type = '0' == line[20] ? TYPE_BAT : TYPE_AC;
				break;
			}
		}
		fclose(file);
	}
	if (type == TYPE_NONE) {
		puts("No AC ACPI");
		return 0;
	}

	if ((file = fopen(BAT"uevent", "r")) != NULL) {
		while (fgets(line, 80, file) != NULL) {
			if (!strncmp("POWER_SUPPLY_STATUS=", line, 20)) {
				strcpy(status, line + 20);
				continue;
			}
			if (!strncmp("POWER_SUPPLY_POWER_NOW=", line, 23)) {
				rate = strtol(line + 23, NULL, 10);
				continue;
			}
			if (!strncmp("POWER_SUPPLY_CURRENT_NOW=", line, 25)) {
				rate = strtol(line + 25, NULL, 10);
				continue;
			}
			if (!strncmp("POWER_SUPPLY_CHARGE_FULL=", line, 25)) {
				capacity = strtol(line + 25, NULL, 10);
				continue;
			}
			if (!strncmp("POWER_SUPPLY_ENERGY_FULL=", line, 25)) {
				capacity = strtol(line + 25, NULL, 10);
				continue;
			}
			if (!strncmp("POWER_SUPPLY_CHARGE_NOW=", line, 24)) {
				now = strtol(line + 24, NULL, 10);
				continue;
			}
			if (!strncmp("POWER_SUPPLY_ENERGY_NOW=", line, 24)) {
				now = strtol(line + 24, NULL, 10);
				continue;
			}
		}
		fclose(file);
	}

	percentage = now / (capacity / 100);
	if (TYPE_BAT == type) {
		if (argc > 1)
			percentageMin = strtol(argv[1], NULL, 10);
		if (percentageMin < 5)
			percentageMin = 5;
		if (percentage <= percentageMin)
			printf("!");
		printf("Bat");
		remain = now;
	} else {
		printf("AC");
		remain = capacity - now;
	}
	printf(" %d%%", percentage);
	if (0 != rate) {
		printf(" (%d:%02d)\n", remain / rate, remain / (rate / 60) %
		    60);
	}
	return 0;
}
