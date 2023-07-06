#!/bin/bash
LEADER_MAC=
FOLLOWER_MAC=


function new_terminal {
	echo command
	echo "$2"
	echo done
	screen -L -Logfile $1  -dmS $1 $2
}


#new_terminal STA "$_cmd"

function start_wphc2sys {

	PHC=$(dmesg | awk '/Registered PHC clock/ {print $11}')
	local phc2sys_s="CLOCK_REALTIME"
	local phc2sys_c="/dev/ptp$PHC"
	if [ $1 == "slave" ]
	then
		phc2sys_s="/dev/ptp$PHC"
		phc2sys_c="CLOCK_REALTIME"
	fi
	_cmd="phc2sys -s $phc2sys_s -c $phc2sys_c --step_threshold=1 --transportSpecific=1 -m -O 0"
	echo $_cmd
}


function start_wptp4l {

	local _config="wifi-leader.cfg"
	local _whose_mac="follower"
	if [ $1 == "slave" ]
	then
		_config="wifi-follower.cfg"
		_whose_mac="leader"
	fi

	#echo 'Starting ptp4l on leader.'

	local _peer_mac=$2
	if [ -z $_peer_mac ]
	then
		read -p "Enter the MAC address of the $whose_mac WiFi device and Press Enter to start ptp4l: " _peer_mac
	fi

	sed -i -e "/ptp_dst_mac/ s/ptp_dst_mac.*/ptp_dst_mac\t$_peer_mac/" $_config
	PHC=$(dmesg | awk '/Registered PHC clock/ {print $11}')
	local _cmd="ptp4l -p /dev/ptp$PHC -f $_config --step_threshold=1 -m"

	read -p "$_cmd ;  Continue ? (Y/N): " confirm && [[ $confirm == [yY] || $confirm == [yY][eE][sS] ]] || exit 1
	echo $_cmd
}

function usage {
	echo "Usage:"
	echo "./setup_wsync.sh -b {master|slave} -p <MAC address of peer>"
	echo
	echo "-b master or slave"
	echo "-p  MAC address of peer. eg: -p 01:be:ce:fd:22:21"
	exit -1
}

while getopts b:dhp: opt; do
	case ${opt} in
		b) MODE=${OPTARG} ;;
		p) PEER_MAC=${OPTARG} ;;
		*) help=1 ;;
	esac
done

# check & display usage
if [ -v help ]
then
	usage
fi

# variable check
if [ ${MODE} == 'NULL' ]
then
	usage
fi


# check if board name are valid
if [ "$MODE" == "master" ] || [ "$MODE" == "slave" ]
then
	true
else
	echo "mode $MODE is not valid. Should be either master or slave"
	exit -1
fi

if [ -z "$PEER_MAC" ]
then
	echo" MAC address of peer is a required parameter"
	usage
fi


if [ ! -f "wifi-leader.cfg" ]
then
	echo "Configuration file wifi-leader.cfg does not exist in this directory."
	exit -1
fi

if [ ! -f "wifi-follower.cfg" ]
then
	echo "Configuration file wifi-follower.cfg does not exist in this directory."
	exit -1
fi


new_terminal WIFI-PTP4L "$(start_wptp4l $MODE $PEER_MAC)"
echo 'ptp4l started.'

_msg="Proceed ?(Y/N): "
if [ "$MODE" == "master" ]
then
	_msg="Make sure ptp4l is started on the follower before proceeding. Start phc2sys locally (Y/N): "
else
	_msg="Make sure phc2sys is started on the leader before proceeding. Start phc2sys locally (Y/N): "
fi
read -p "$_msg" confirm && [[ $confirm == [yY] || $confirm == [yY][eE][sS] ]] || exit 1

new_terminal WIFI-PHC2SYS "$(start_wphc2sys $MODE)"
