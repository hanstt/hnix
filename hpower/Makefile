PREFIX?=~/opt

all: hpower

hpower: main.c
	gcc -ggdb -O2 -pedantic-errors -Wall -Werror -Wextra $< -o $@

install:
	mkdir -p $(PREFIX)/bin
	cp -f hpower $(PREFIX)/bin
	chmod 755 $(PREFIX)/bin/hpower

clean:
	rm -f hpower
