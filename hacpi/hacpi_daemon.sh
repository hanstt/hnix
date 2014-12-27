#!/bin/sh

# Polls and emulates "missing" ACPI events, plus performs some actions.

ac_path=/sys/class/power_supply/ACAD
bat_path=/sys/class/power_supply/BAT1

last_ac_adapter=-1

while true
do
	cur_ac_adapter=`cat $ac_path/online`
	if [ "x$last_ac_adapter" != "x$cur_ac_adapter" ]
	then
		hacpi_events.sh ac_adapter 0 0 $cur_ac_adapter
		last_ac_adapter=$cur_ac_adapter
	fi

	alarm=`cat $bat_path/alarm`
	energy_now=`cat $bat_path/energy_now`
	if [ $energy_now -lt $alarm ]
	then
		status=`cat $bat_path/status`
		if [ "Discharging" == $status ]
		then
			hacpi_hibernate.sh
		fi
	fi

	sleep 5
done
