#!/bin/bash


function new_terminal {
	echo "$2"
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
iw phy phy0 interface add sta0 type managed
sleep 5
_cmd="/usr/sbin/wpa_supplicant -Dnl80211 -i sta0 -K -dd -T -s -c /etc/wpa_supplicant/wpa_supplicant-5g.conf"
new_terminal STA_5G "$_cmd"
sleep 2
_wpa_cmd="/usr/sbin/wpa_cli -i sta0 -a /usr/sbin/wpa_action_script.sh"
new_terminal STA_WPA "$_wpa_cmd"
