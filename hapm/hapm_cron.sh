#!/bin/sh

# Auto-resurrects hapm daemon.

pid=`ps x|grep hapm_daemon.sh|grep local|awk '{print $1}'`
if [ "x" == "x$pid" ]
then
	/usr/local/bin/hapm_daemon.sh &
fi
