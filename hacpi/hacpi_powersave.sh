#!/bin/sh

# Does a bunch of things to save money.

max_brightness=`cat /sys/class/backlight/intel_backlight/max_brightness`
echo "$max_brightness*0.8/1" | bc > /sys/class/backlight/intel_backlight/brightness

echo 5 > /proc/sys/vm/laptop_mode
echo 1 > /sys/module/snd_hda_intel/parameters/power_save
echo 1500 > /proc/sys/vm/dirty_writeback_centisecs

ethtool -s eth0 wol d
rfkill block bluetooth
rfkill block wwan
#iw dev wlan0 set power_save on

for i in `find /sys -path "*power/control"`
do
	echo auto > $i
done
for i in `find /sys -name link_power_management_policy`
do
	echo min_power > $i
done

for i in `ls /dev/input/by-id/*mouse*`
do
	devpath=`udevadm info --query=property --name=$i | grep DEVPATH=`
	base=`echo $devpath | sed 's/DEVPATH=//'`
	echo on > "/sys/$base/power/control"
done
