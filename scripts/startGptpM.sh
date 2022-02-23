#!/bin/bash


function new_terminal {
	echo "Starting GPTP Master..."
	echo "$2"
	screen -dmS $1 $2
	echo "All done!"
}

function start_8021as {
	local _mode="-T"
	local _log="Master_gptp.log"
	local _client="hostapd"

	if [ $3 == "slave" ]
	then
		_mode="-L"
		_log="Slave_gptp.log"
		_client="wpa_supplicant"
	fi

	local _command="/usr/sbin/daemon_cl -i $1 -C /var/run/$_client -G ptp -I $2 -W $_mode 2>&1 | tee ~/$_log"
	echo $_command
}

IW=`which iw`
INTERFACE=$(iw dev | awk '$1=="Interface"{print $2}' | head -n 1)
if [ -z $INTERFACE ]; then
        INTERFACE="uap0"
fi

if [ -z $IW ]; then
        INTERFACE="uap0"
fi
INTERFACE="wlp1s0"

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

new_terminal SYNC_8021AS "$(start_8021as $INTERFACE $PTP_IDX 'master')"
