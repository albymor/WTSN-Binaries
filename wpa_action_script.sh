#!/bin/bash
#
# This script file is passed as parameter to wpa_cli, started as a daemon,
# so that the wpa_supplicant events are sent to this script
# and actions executed, like :
#    - start DHCP client when STA is connected.
#    - stop DHCP client when STA is disconnected.
#    - start DHCP client when P2P-GC is connected.
#    - stop DHCP server when  P2P-GO is disconnected.
#
# This script skips events if connmand (connman.service) is started
# Indeed, it is considered that the Wifi connection is managed through
# connmand and not wpa_cli
#

IFNAME=$1
CMD=$2
INTERFACE_NAME="wlp1s0"
HOSTAPD=$(which hostapd)

function new_terminal {
	echo "$2"
	screen -dmS $1 $2
}


echo "event $CMD received from wpa_supplicant"

# if Connman is started, ignore wpa_supplicant
# STA connection event because the DHCP connection
# is triggerd by Connman
#if [ `systemctl is-active connman` == "active" ] ; then
#    if [ "$CMD" = "CONNECTED" ] || [ "$CMD" = "DISCONNECTED" ] ; then
#        echo "event $CMD ignored because Connman is started"
#        exit 0
#    fi
#fi

if [[ "$CMD" == "CONNECTED" ]]; then
    #kill_daemon udhcpc /var/run/udhcpc-$IFNAME.pid
    #udhcpc -i $IFNAME -p /var/run/udhcpc-$IFNAME.pid -S
    # virtual interface is connected,let us start the AP in 5GHz mode 
    _cmd="$HOSTAPD -i $INTERFACE_NAME -K -dd -T -s /etc/hostapd/hostapd-softap.conf"
    echo $_cmd
    new_terminal AP "$_cmd"
fi

if [[ "$CMD" == "DISCONNECTED" ]]; then
    #kill_daemon udhcpc /var/run/udhcpc-$IFNAME.pid
    #ifconfig $IFNAME 0.0.0.0
	# Nothing to do here 
    echo "DISCONNECTED - Nothing to be done"
fi

#if [ "$CMD" = "P2P-GROUP-STARTED" ]; then
#    GIFNAME=$3
#    if [ "$4" = "GO" ]; then
#        kill_daemon udhcpc /var/run/udhcpc-$GIFNAME.pid
#        ifconfig $GIFNAME 192.168.42.1 up
#        cp /etc/wpa_supplicant/udhcpd-p2p.conf /etc/wpa_supplicant/udhcpd-p2p-itf.conf
#        sed -i "s/INTERFACE/$GIFNAME/" /etc/wpa_supplicant/udhcpd-p2p-itf.conf
#        udhcpd /etc/wpa_supplicant/udhcpd-p2p-itf.conf
#    fi
#    if [ "$4" = "client" ]; then
#        kill_daemon udhcpc /var/run/udhcpc-$GIFNAME.pid
#        kill_daemon udhcpd /var/run/udhcpd-$GIFNAME.pid
#        udhcpc -i $GIFNAME -p /var/run/udhcpc-$GIFNAME.pid
#    fi
#fi

#if [ "$CMD" = "P2P-GROUP-REMOVED" ]; then
#    GIFNAME=$3
#    if [ "$4" = "GO" ]; then
#        kill_daemon udhcpd /var/run/udhcpd-$GIFNAME.pid
#        ifconfig $GIFNAME 0.0.0.0
#    fi
#    if [ "$4" = "client" ]; then
#        kill_daemon udhcpc /var/run/udhcpc-$GIFNAME.pid
#        ifconfig $GIFNAME 0.0.0.0
#    fi
#fi


