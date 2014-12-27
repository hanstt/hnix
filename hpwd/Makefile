PREFIX?=~/opt

.PHONY: all
all: hpwd

hpwd: main.c
	gcc -O2 -pedantic-errors -Wall -Werror -Wextra -Wshadow $< -o $@

.PHONY: install
install:
	mkdir -p $(PREFIX)/bin
	cp -f hpwd $(PREFIX)/bin
	chmod 755 $(PREFIX)/bin/hpwd

.PHONY: uninstall
uninstall:
	rm -f $(PREFIX)/bin/hpwd

.PHONY: clean
clean:
	rm -f hpwd
