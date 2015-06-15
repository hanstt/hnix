#!/bin/sh

export DISPLAY=:0.0

echo "$1" >> /tmp/hacpi.log
if [ "xconnected" == "x$1" ]
then
	xrandr --output HDMI1 --auto --primary
else
	xrandr --output LVDS1 --auto --primary
	xrandr --output HDMI1 --off
fi
