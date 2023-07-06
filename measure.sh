while true; do offset=$(sudo phc_ctl /dev/ptp0 cmp |
grep "offset from" | awk '{print $6}' | sed -e 's/ns//');
echo $offset; sleep 1; done | tee time_offsets.csv
