#!/bin/sh

# Responds to ACPI events.

if [ $1 == "ac_adapter" ]
then
	if [ $4 -eq 0 ]
	then
		# No power.
		/opt/bin/powersave.sh &
	else
		# AC powah, let it roast!
		echo 100 > /sys/class/backlight/acpi_video0/brightness
	fi
	exit 0
fi

if [ $1 == "button/lid" ]
then
	# Blank the screen.
	xset dpms force off
	exit 0
fi

if [ $1 == "button/power" ]
then
	# Do nothing.
	exit 0
fi
