#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 256

int
main(void)
{
	char buf[BUF_SIZE];
	char *home, *s;
	int homelen, i, level;

	if (getcwd(buf, BUF_SIZE) == NULL) {
		puts("<no path>");
		return 0;
	}

	home = getenv("HOME");
	for (homelen = strlen(home) - 1; 0 < homelen && '/' ==
	    home[homelen]; --homelen)
		;
	++homelen;
	if (strncmp(buf, home, homelen) == 0) {
		s = buf + homelen - 1;
		s[0] = '~';
	} else
		s = buf;

	for (i = strlen(s) - 1, level = 0; i > 0; i--)
		if (s[i] == '/' && ++level == 2)
			break;

	if (i == 1 && s[0] == '~')
		i--;
	else if (i > 0)
		printf("...");
	puts(s + i);

	return 0;
}
