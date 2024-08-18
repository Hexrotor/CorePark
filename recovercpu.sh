#!/bin/sh
i=1
cpuCount=20
while [ $i -ne $cpuCount ]
do
	echo 1 > /sys/devices/system/cpu/cpu$i/online
	i=$(( $i+1 ))
done
