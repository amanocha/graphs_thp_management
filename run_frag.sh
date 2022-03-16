#!/bin/bash

EXP_NUM=$1

footprints=(8634 16123 16611 3121 12800 24374 25386 4736 8952 16555 17589 3252) # MB
datasets=(Kronecker_25 Twitter Sd1_Arc Wikipedia)

offset=3072

NUMA_NODE=1 # EDIT THIS VALUE (NUMA NODE)
MAX_RAM=64000 # EDIT THIS VALUE (AMOUNT OF MEMORY ON NUMA NODE)

re1='^[0-9]*\.00$' # 0.00 (integer) regex
re2='^-*[0-9]*\.50$' # 0.5 regex

for i in ${!footprints[@]}
do
	footprint=${footprints[$i]}
	size=$(( footprint + offset ))

	cmd="numactl --membind $NUMA_NODE ./numactl/memhog $(( MAX_RAM - size ))M"
	echo $cmd
	screen -dm -S memhog $cmd
	pid=$(screen -ls | awk '/\.memhog\t/ {print strtonum($1)}')
	sleep 20

	num_datasets=${#datasets[@]}
	if (( $i < $num_datasets )) ; then
		app=bfs
	elif (( $i < 2 * $num_datasets )) ; then
		app=sssp
	else
		app=pagerank
	fi

	d=$(( i % num_datasets ))
	dataset=${datasets[$d]}
	cmd="sudo bash frag.sh $dataset $app $NUMA_NODE $EXP_NUM"
	echo $cmd
	$cmd

	kill $pid
	echo ""
done
