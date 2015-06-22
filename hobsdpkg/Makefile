CFLAGS:=-ansi -ggdb -pedantic-errors -Wall -Werror -Wextra -Wmissing-prototypes -Wshadow

.PHONY: all
all: pkg_wipe

pkg_wipe: pkg_wipe.c Makefile
	$(CC) $(CFLAGS) -o $@ pkg_wipe.c

.PHONY: clean
clean:
	rm -f pkg_wipe
