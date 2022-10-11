#!/bin/bash

function new_terminal {
	echo "$2"
	screen -dmS $1 $2
}

IW=`which iw`

INTERFACE_NAME=$(iw dev | awk '$1=="Interface"{print $2}')
if [ -z $INTERFACE_NAME ]; then
	INTERFACE_NAME="wlan0"
fi 

if [ -z $IW ]; then
	INTERFACE_NAME="wlan0"
fi




echo "Using interface $INTERFACE_NAME"

sudo service network-manager stop
sudo pkill -9 wpa_supplicant

sudo modprobe  -r iwlmvm
sleep 1
sudo dmesg -C
sudo modprobe iwlmvm

sleep 2
SUPPLICANT=`which wpa_supplicant`

#ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=wheel
_cmd="$SUPPLICANT -Dnl80211 -i $INTERFACE_NAME -K -dd -T -s -c /etc/wpa_supplicant/wpa_supplicant.conf"
echo $_cmd
new_terminal STA "$_cmd"
