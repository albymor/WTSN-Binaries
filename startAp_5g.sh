#!/bin/bash


function new_terminal {
	echo "starting $2"
	screen -dmS $1 $2
}

# first start the wpa_supplicant and connect to 5g ap
pkill wpa_supplicant
pkill hostapd
modprobe -r iwlmvm
sleep 2
dmesg -C
modprobe iwlmvm
sleep 2

IFACE=$(iw dev | awk '/Interface/{print $2}')
sed -i "s/INTERFACE_NAME=.*/INTERFACE_NAME=$IFACE/" /usr/sbin/wpa_action_script.sh

# Create a virtual interface to connect to a real AP
iw phy phy0 interface add sta0 type managed
sleep 5

WPA_SUPP=`which wpa_supplicant`
_cmd="$WPA_SUPP -Dnl80211 -i sta0 -K -dd -T -s -c /etc/wpa_supplicant/wpa_supplicant-5g.conf"

# Connect to the real commercial AP
new_terminal STA_5G "$_cmd"
sleep 2

# Start wpa actio script to create a SoftAP when client is connected
WPA_CLI=`which wpa_cli`
_wpa_cmd="$WPA_CLI -i sta0 -a /usr/sbin/wpa_action_script.sh"
new_terminal STA_WPA "$_wpa_cmd"
