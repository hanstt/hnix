#!/bin/sh

hdmi_prev=0

while true
do
	xrandr_hdmi=`xrandr | grep HDMI | grep disconnected`
	if [ "x" == "x$xrandr_hdmi" ]
	then
		if [ $hdmi_prev -eq 0 ]
		then
			xrandr --output HDMI1 --auto --primary
			hdmi_prev=1
		fi
	else
		if [ $hdmi_prev -ne 0 ]
		then
			xrandr --output HDMI1 --off
			hdmi_prev=0
		fi
	fi
	sleep 5
done
