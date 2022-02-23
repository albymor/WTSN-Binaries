#!/bin/bash

function new_terminal {
	_cmd="bash -c /tmp/runner.sh" 
	echo "Starting GPTP Slave..."
	echo $_cmd
	chmod +x /tmp/runner.sh
	screen -dmS $1 $_cmd
	echo "All done!"
}

function start_8021as {
	local _mode="-T"
	local _log="Master_gptp.log"
	local _client="hostapd"

	if [ $3 == "slave" ]
	then
		_mode="-R 255"
		_log="Slave_gptp.log"
		_client="wpa_supplicant"
	fi

	local _command="/usr/sbin/daemon_cl -i $1 -C /var/run/$_client -G ptp -I $2 -W $_mode 2>&1 | tee ~/$_log"
	echo $_command > /tmp/runner.sh
	echo $_command
}

IW=`which iw`
INTERFACE=$(iw dev | awk '$1=="Interface"{print $2}' | head -n 1)

if [ -z $INTERFACE ]; then
        INTERFACE="wlan0"
fi

if [ -z $IW ]; then
        INTERFACE="wlan0"
fi

echo "Using Station interface $INTERFACE, Checking connectivity to AP ..."
BSSID=$(wpa_cli -i $INTERFACE  status | awk -F'[=]' '/bssid/{print $2}')

if [ -z $BSSID ]; then
	echo " Station ($INTERFACE) is not connected, Please connect the station first"
	exit 0
else
    echo "Found connection to AP!";
fi

ptp_file=$(ls -v /dev/ptp* | tail -1)
_suffix="${ptp_file##*[0-9]}"
PTP_IDX="${ptp_file%"$_suffix"}"
PTP_IDX="${PTP_IDX##*[!-0-9]}"
if [ -z $PTP_IDX ]; then
	PTP_IDX=1
fi

# Kill any older instance of the daemon
sudo pkill -9 daemon_cl
export LD_LIBRARY_PATH=/lib:/usr/lib:/usr/local/lib
ldconfig

sleep 2

new_terminal SYNC_8021AS "$(start_8021as $INTERFACE $PTP_IDX 'slave')"
