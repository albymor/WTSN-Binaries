#!/bin/bash

function new_terminal {
	echo "$2"
	screen -dmS $1 $2
}



IW=`which iw`
INTERFACE_NAME=$(iw dev | awk '$1=="Interface"{print $2}' | head -1)
if [ -z $INTERFACE_NAME ]; then
	INTERFACE_NAME="wlan0"
fi 

if [ -z $IW ]; then
	INTERFACE_NAME="wlan0"
fi
echo "Using interface $INTERFACE_NAME"


sudo pkill -9 hostapd
sudo service network-manager stop

sudo modprobe  -r iwlmvm
sleep 1
sudo dmesg -C 
sudo modprobe iwlmvm

sleep 2

_cmd="/usr/sbin/hostapd -i $INTERFACE_NAME -K -dd -T -s /etc/hostapd/hostapd.conf"
echo $_cmd
new_terminal AP "$_cmd"
