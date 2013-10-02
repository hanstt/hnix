PREFIX?=~/opt

all:
	gcc -O2 main.c -o dacpi

install:
	mkdir -p $(PREFIX)/bin
	cp -f dacpi $(PREFIX)/bin
	chmod 755 $(PREFIX)/bin/dacpi
