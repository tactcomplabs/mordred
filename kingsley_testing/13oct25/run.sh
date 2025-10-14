#! /bin/bash

# do the offered load
for (( i = 10; i <= 80; i = i+10 ));
#for (( i = 55; i <= 60; i = i+10 ));
do
    #echo "${PWD}:OL=$i"
    sst --model-options="$i false" ./noc_mesh_32_test.py >& OL.LF$i.txt
done

# do the clocked offered load
for (( i = 10; i <= 80; i = i+10 ));
#for (( i = 55; i <= 60; i = i+10 ));
do
    #echo "${PWD}:COL=$i"
    sst --model-options="$i true" ./noc_mesh_32_test.py >& COL.LF$i.txt
done

