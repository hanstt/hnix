PREFIX?=/opt

install:
	mkdir -p $(PREFIX)/bin
	cp -f hacpi_*.sh $(PREFIX)/bin
	chmod 755 $(PREFIX)/bin/hacpi_*.sh
	cp -f hacpi_acpid /etc/acpi/events
