PREFIX?=~/opt

CFLAGS:=-ansi -O2 -pedantic-errors -Wall -Werror -Wextra -Wmissing-prototypes -Wshadow -Wstrict-prototypes

.PHONY: all
all: pkg_wipe

pkg_wipe: pkg_wipe.c Makefile
	gcc $(CFLAGS) -o $@ pkg_wipe.c

.PHONY: install
install:
	mkdir -p $(PREFIX)/bin
	install pkg_wipe $(PREFIX)/bin

.PHONY: uninstall
uninstall:
	rm -f $(PREFIX)/bin/pkg_wipe

.PHONY: clean
clean:
	rm -f pkg_wipe
