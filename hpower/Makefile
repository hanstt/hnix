PREFIX?=~/opt

.PHONY: all
all: hpower

hpower: main.c
	gcc -O2 -pedantic-errors -Wall -Werror -Wextra -Wshadow $< -o $@

.PHONY: install
install:
	mkdir -p $(PREFIX)/bin
	cp -f hpower $(PREFIX)/bin
	chmod 755 $(PREFIX)/bin/hpower

.PHONY: uninstall
uninstall:
	rm -f $(PREFIX)/bin/hpower

.PHONY: clean
clean:
	rm -f hpower
