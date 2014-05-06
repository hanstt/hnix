#!/bin/sh

# Cronned to check battery level.

path=/sys/class/power_supply/BAT1
status=$path/status

status=`cat $path/status`
if [ "Charging" \= $status ]; then
	# Charging, so we're ok.
	exit 0
fi

alarm=`cat $path/alarm`
energy_now=`cat $path/energy_now`
if [ $alarm -gt $energy_now ]; then
	# Hibernate under alarm level.
	/opt/bin/acpi_hibernate
fi
