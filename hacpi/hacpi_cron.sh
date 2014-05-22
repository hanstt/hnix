#!/bin/sh

# Auto-resurrects hacpi daemon.

pid=`ps x|grep hacpi_daemon.sh|grep opt|awk '{print $1}'`
if [ "x" == "x$pid" ]
then
	hacpi_daemon.sh &
fi
