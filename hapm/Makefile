PREFIX?=/usr/local

BINS=hapm_cron.sh \
     hapm_daemon.sh \
     hapm_xrandr.sh

install:
	mkdir -p ${PREFIX}/bin
	install ${BINS} ${PREFIX}/bin

uninstall:
.for i in ${BINS}
	rm -f ${PREFIX}/bin/${i}
.endfor
