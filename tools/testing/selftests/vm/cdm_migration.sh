#!/usr/bin/bash
#
# Should work with any workoad and workload commandline.
# But for now ebizzy should be installed. Please run it
# as root.
#
# Copyright (C) Anshuman Khandual 2016, IBM Corporation
#
# Licensed under GPL V2

# Unload, build and reload modules
if [ "$1" = "reload" ]
then
	rmmod coherent_memory_demo
	rmmod coherent_hotplug_demo
	cd ../../../../
	make -s -j 64 modules
	insmod drivers/char/coherent_hotplug_demo.ko
	insmod drivers/char/coherent_memory_demo.ko
	cd -
fi

# Workload
workload=ebizzy
work_cmd="ebizzy -T -z -m -t 128 -n 100000 -s 32768 -S 10000"

pkill $workload
$work_cmd &

# File
if [ -e input_file.txt ]
then
	rm input_file.txt
fi

# Inputs
pid=`pidof ebizzy`
cp /proc/$pid/maps input_file.txt
if [ ! -e input_file.txt ]
then
	echo "Input file was not created"
	exit
fi
input=input_file.txt

# Migrations
dmesg -C
while read line
do
	addr_start=$(echo $line | cut -d '-' -f1)
	addr_end=$(echo $line | cut -d '-' -f2 | cut -d ' ' -f1)
	node=`expr $RANDOM % 5`

	echo $pid,0x$addr_start,0x$addr_end,$node > /sys/kernel/debug/coherent_debug
done < "$input"

# Analyze dmesg output
passed=`dmesg | grep "migration_passed" | wc -l`
failed=`dmesg | grep "migration_failed" | wc -l`
queuef=`dmesg | grep "queue_pages_range_failed" | wc -l`
empty=`dmesg | grep "list_empty" | wc -l`
missing=`dmesg | grep "vma_missing" | wc -l`

# Stats
echo passed	$passed
echo failed	$failed
echo queuef	$queuef
echo empty	$empty
echo missing	$missing

# Cleanup
rm input_file.txt
if pgrep -x $workload > /dev/null
then
	pkill $workload
fi
