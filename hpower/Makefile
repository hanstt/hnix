PREFIX?=~/opt

all: hpower

hpower: main.c
	gcc -O2 -pedantic-errors -Wall -Werror -Wextra -Wshadow $< -o $@

install:
	mkdir -p $(PREFIX)/bin
	cp -f hpower $(PREFIX)/bin
	chmod 755 $(PREFIX)/bin/hpower

uninstall:
	rm -f $(PREFIX)/bin/hpower

clean:
	rm -f hpower
