PREFIX?=/opt

BINS=hacpi_cron.sh \
     hacpi_ctrl.sh \
     hacpi_daemon.sh \
     hacpi_events.sh \
     hacpi_hibernate.sh \
     hacpi_powersave.sh \
     hacpi_sleep.sh
CFGS=hacpi_acpid

install:
	mkdir -p $(PREFIX)/bin
	cp -f $(BINS) $(PREFIX)/bin
	chmod 755 $(addprefix $(PREFIX)/bin/,$(BINS))
	cp -f $(CFGS) /etc/acpi/events

uninstall:
	rm -f $(addprefix $(PREFIX)/bin/,$(BINS)) $(addprefix /etc/acpi/events/,$(CFGS))
