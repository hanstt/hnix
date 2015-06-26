#!/bin/sh

while true
do

	percent=`apm -l`
	if [ $percent -lt 5 ]
	then
		ac=`apm -a`
		if [ $ac -eq 0 ]
		then
			apm -Z
		fi
	fi
	sleep 5
done
