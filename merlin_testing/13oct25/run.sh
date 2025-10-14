#! /bin/bash

# do the offered load
for (( i = 10; i <= 80; i = i+10 ));
#for (( i = 55; i <= 60; i = i+10 ));
do
    #echo "${PWD}:OL=$i"
    sst --model-options="$i false" ./mesh_trafficgen.py >& OL.LF$i.txt
done

# do the clocked offered load
for (( i = 10; i <= 80; i = i+10 ));
#for (( i = 55; i <= 60; i = i+10 ));
do
    #echo "${PWD}:COL=$i"
    sst --model-options="$i true" ./mesh_trafficgen.py >& COL.LF$i.txt
done

