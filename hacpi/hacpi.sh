#!/bin/sh

# Listens to ACPI events.

if [ $1 == "ac_adapter" ]; then
	if [ $4 -eq 0 ]; then
		# No power.
		xbacklight -set 80
	else
		# AC powah, let it roast!
		xbacklight -set 100
	fi
	exit 0
fi
if [ $1 == "button/lid" ]; then
	# Blank the screen.
	xset dpms force off
	exit 0
fi
if [ $1 == "button/power" ]; then
	# Do nothing. Maybe hibernate?
	exit 0
fi
