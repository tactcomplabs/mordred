#! /bin/bash

# do the offered load
for (( i = 10; i <= 80; i = i+10 ));
do
    sst --model-options="$i false" ./offered_load.py >& OL.LF$i.txt
done

# do the clocked offered load
for (( i = 10; i <= 80; i = i+10 ));
do
    sst --model-options="$i true" ./offered_load.py >& COL.LF$i.txt
done
