PREFIX?=~/opt

all:
	gcc -O2 main.c -o pwdparse

clean:
	rm -f pwdparse

install:
	mkdir -p $(PREFIX)/bin
	cp pwdparse $(PREFIX)/bin

uninstall:
	rm -f $(PREFIX)/bin/pwdparse
