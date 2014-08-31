#!/bin/sh

if [[ 1 -ne $# || ( "xmem" != "x$1" && "xdisk" != "x$1" ) ]]; then
	echo "Need \"mem\" (sleep) or \"disk\" (hibernate) input argument."
	exit 1
fi

slock &

# Discover video card's ID.
ID=`/sbin/lspci | grep VGA | awk '{print $1}' | sed -e 's@0000:@@' -e 's@:@/@'`

# Create temporary file and remove on error or exit.
TMP_FILE=`mktemp /var/tmp/video_state.XXXXXX`
trap 'rm -f $TMP_FILE' 0 1 15

# Switch to virtual terminal 1 to avoid graphics corruption in X.
chvt 1

# Run local shutdown.
/etc/rc.d/rc.local_shutdown

# Dump current data from the video card to the temporary file.
cat /proc/bus/pci/$ID > $TMP_FILE

# Sync storage.
sync

# Drop caches.
echo 3 > /proc/sys/vm/drop_caches

# Go down.
echo -n $1 > /sys/power/state

# We're back up, restore video card data from the temporary file.
cat $TMP_FILE > /proc/bus/pci/$ID

# Switch back to virtual terminal 7.
chvt 7

# Run local setup.
/etc/rc.d/rc.local
