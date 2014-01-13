#!/bin/bash

for i in `seq 1 8`
do
    echo "vegas-hpc$i"
    IP_=`host vegas-hpc$i-10 | cut -d \  -f4`
    echo $IP_
    MAC_=`ssh vegas-hpc$i '/sbin/ifconfig 2> /dev/null' 2> /dev/null | grep -B 1 $IP_ | grep HWaddr | cut -d \  -f11`
    echo $MAC_
done
