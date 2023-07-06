#!/bin/bash
SUDO='sudo'

# Set interface name
INTERFACE_NAME="wlp88s0"
echo "Using interface $INTERFACE_NAME"

# set DST ip for identifying stream
DST_IP_ADDR=$1
DST_PORT=7788

# Allow a delay for IPERF to setup traffic, this is necessary for proper alignment of
# start of traffic and start time of Qbv gates
IPERF_START_DELAY=1000
d=`date +%s.%N`
s=$(echo $d | cut -d. -f1)
n=$(echo $d | cut -d. -f2)
s_b=$(($s + 10))
n_i=$(($n - $IPERF_START_DELAY))
BASE_TIME="$s_b"

# clear IPTABLES
$SUDO iptables -t mangle -F
# clear qdiscs
$SUDO tc qdisc del dev $INTERFACE_NAME \
 parent root

sleep 2
# set socket priority of the stream using the stream identifier
# set skb_prio for all udp traffic going to port 5010
$SUDO iptables -t mangle -A POSTROUTING -p udp --dport $DST_PORT -j CLASSIFY --set-class 0:6
# set DSCp prio to CS6 which corresponds to AC_VO for Wifi  
$SUDO iptables -t mangle -A POSTROUTING -p udp --dport $DST_PORT -j DSCP --set-dscp-class CS6

$SUDO tc qdisc show dev $INTERFACE_NAME

# Configure TAPRIO
# default priority -> TC 1
# Priority 6 > TC 0
# cycle time 1 ms, 250us for TC traffic, 700us for BE traffic and 5us Guard interval
# Queue IDX 0 mapped to TC0, Queue IDX 1 mapped to TC1
$SUDO tc -d qdisc replace dev $INTERFACE_NAME \
    parent root \
    handle 100 \
    taprio \
    num_tc 2 \
    map 1 1 1 1 1 1 0 1 1 1 1 1 1 1 1 1 \
    queues 1@0 1@1 \
    base-time $BASE_TIME \
    sched-entry S 01 250000 \
    sched-entry S FE 700000 \
    sched-entry S 00 50000 \
    clockid CLOCK_REALTIME

# display Qdisc configuration
$SUDO tc qdisc show dev $INTERFACE_NAME


# remove any other instances of iperf
$SUDO killall -9 iperf

#Start iperf for RT traffic on port 5010
#$SUDO taskset -c 2 iperf -c $DST_IP_ADDR -u -e -z -l 60 -b 1000pps -i10 -p 5010 --txstart-time $BASE_TIME.$n_i -t $3 &
#iperf3 -c $DST_IP_ADDR -u -b 0 --length 60 -p 5010 -t $3&
#Start iperf for BE traffic on default port 5001
#Note: Change -b through $2 each time for various data rates from 1 to300Mbps

#$SUDO taskset -c 3 iperf -c $DST_IP_ADDR -u -e -i10 -p 5020 --txstart-time $BASE_TIME.$n_i -t $3 -l 1400
#iperf3 -c $DST_IP_ADDR -u -b 0 -t $3 --length 1000
 
