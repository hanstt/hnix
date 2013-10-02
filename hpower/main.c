#include <stdio.h>
#include <string.h>

#define AC "/proc/acpi/ac_adapter/ADP1/"
#define BAT "/proc/acpi/battery/BAT1/"

enum {
	TYPE_NONE,
	TYPE_AC,
	TYPE_BAT
};

int
main(int argc, char **argv)
{
	char line[80];
	FILE *file;
	int type = TYPE_NONE;
	int capacity = 0;
	int rate = 0;
	int remain = 0;
	int percentage = 0;
	int percentageMin = 0;

	if ((file = fopen(AC"state", "r")) != NULL) {
		while (fgets(line, 80, file) != NULL) {
			if (strstr(line, "on-line")) {
				type = TYPE_AC;
				break;
			}
			if (strstr(line, "off-line")) {
				type = TYPE_BAT;
				break;
			}
		}
		fclose(file);
	}
	if (type == TYPE_NONE) {
		puts("No ACPI");
		return 0;
	}

	if ((file = fopen(BAT"info", "r")) != NULL) {
		while (fgets(line, 80, file) != NULL)
			if (strstr(line, "design capacity:")) {
				capacity = strtol(line + 16, NULL, 10);
				break;
			}
		fclose(file);
	}

	if ((file = fopen(BAT"state", "r")) != NULL) {
		while (fgets(line, 80, file) != NULL) {
			if (strstr(line, "present rate:"))
				rate = strtol(line + 13, NULL, 10);
			if (strstr(line, "remaining capacity:"))
				remain = strtol(line + 19, NULL, 10);
		}
		fclose(file);
	}

	if (capacity > 0) {
		percentage = 100 * remain / capacity;
		if (type == TYPE_BAT) {
			if (argc > 1)
				percentageMin = strtol(argv[1]);
			if (percentageMin < 15)
				percentageMin = 15;
			if (percentage <= percentageMin)
				printf("!");
		}
	}
	printf("%s", type == TYPE_AC ? "AC" : "Bat");
	if (percentage > 0)
		printf(" %d%%", percentage);
	if (type == TYPE_BAT && rate > 0)
		printf(" %dh %dmin", remain / rate, 60 * remain / rate % 60);
	puts("");
	return 0;
}
