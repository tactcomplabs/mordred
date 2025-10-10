#! /bin/bash

for dir in */;
do
    cd $dir
    # do the offered load
    #for (( i = 10; i <= 80; i = i+10 ));
    for (( i = 55; i <= 60; i = i+10 ));
    do
	#echo "${PWD}:OL=$i"
	sst --model-options="$i false" ./offered_load.py >& OL.LF$i.txt
    done

    # do the clocked offered load
    #for (( i = 10; i <= 80; i = i+10 ));
    for (( i = 55; i <= 60; i = i+10 ));
    do
	#echo "${PWD}:COL=$i"
	sst --model-options="$i true" ./offered_load.py >& COL.LF$i.txt
    done

    cd ..
done
