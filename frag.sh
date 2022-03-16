#!/bin/bash

DATASET=$1
APP=$2
NUMA_NODE=$3
EXP_NUM=$4

ORDER=9
FILE="done.txt"

levels=(0 25 50 75)

if [ $EXP_NUM == 5 ] ;  then
	levels=(50)
fi

re1='^[0-9]+\.00$' # 0.00 (integer) regex
re2='^[0-9]+\.50$' # 0.5 regex

for frag_level in ${levels[@]}
do
	# INITIALIZATION
	echo "INITIALIZATION"
	echo "--------------"
	
	echo "sync ; echo 3 > /proc/sys/vm/drop_caches"
	sync ; echo 3 > /proc/sys/vm/drop_caches

	echo "echo 1 > /proc/sys/vm/compact_memory"
	echo 1 > /proc/sys/vm/compact_memory

	echo "echo madvise > /sys/kernel/mm/transparent_hugepage/enabled"
	echo madvise > /sys/kernel/mm/transparent_hugepage/enabled

	echo "echo madvise > /sys/kernel/mm/transparent_hugepage/defrag"
	echo madvise > /sys/kernel/mm/transparent_hugepage/defrag

	if [ -f "$FILE" ]; then
		echo "rm $FILE"
		rm $FILE
	fi

	echo ""

	# FRAGMENT MEMORY
	echo "FRAGMENT"
	echo "--------"
	
	cmd="numactl --membind $NUMA_NODE ./utils/fragm fragment $NUMA_NODE $ORDER $frag_level"
	echo $cmd
	screen -dm -S frag $cmd
	
	pid=$(screen -ls | awk '/\.frag\t/ {print strtonum($1)}')

	echo "Waiting for fragmentation to finish..."
	while [ ! -f "$FILE" ] 
	do
		sleep 1
	done
	
	echo "Done!"

	echo ""

	# EXECUTE APP
	echo "EXECUTE APP"
	echo "-----------"
	
	cmd="sudo bash thp.sh $EXP_NUM $DATASET $APP $frag_level"
	echo $cmd
 	$cmd

	echo ""

	# CLEANUP
	echo "CLEANUP"
	echo "-------"

	echo "kill $pid"
	kill $pid

	echo ""

	cmd="./utils/free $FILE $ORDER $NUMA_NODE"
	echo $cmd
	$cmd

	echo ""

	echo "rm $FILE"
	rm $FILE

	echo ""
done
