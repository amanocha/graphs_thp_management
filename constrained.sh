#!/bin/bash

footprints=(8634 16123 16611 3121 12800 24374 25386 4736 8952 16555 17589 3252) # MB
datasets=(Kronecker_25 Twitter Sd1_Arc Wikipedia)
offsets=(3072 2560 2048 1536 1024 512 0 -512) # MB

NUMA_NODE=1 # EDIT THIS VALUE (NUMA NODE)
MAX_RAM=64000 # EDIT THIS VALUE (AMOUNT OF MEMORY ON NUMA NODE)

EXP_NUM=3

re1='^[0-9]*\.00$' # 0.00 (integer) regex
re2='^-*[0-9]*\.50$' # 0.5 regex

for i in ${!footprints[@]}
do
	footprint=${footprints[$i]}
	for o in ${!offsets[@]}
	do 
		offset=${offsets[$o]}
		size=$(( footprint + offset ))

		cmd="numactl --membind $NUMA_NODE ./numactl/memhog $(( MAX_RAM - size ))M"
		echo $cmd
		screen -dm -S memhog $cmd
	
		pid=$(screen -ls | awk '/\.memhog\t/ {print strtonum($1)}')

		size=$offset
		gb_size=`echo "scale=2; $size/1024" | bc -l`
		if [[ $gb_size =~ $re1 ]] ; then
			gb_size=$(( $size/1024 ))
		elif [[ $gb_size =~ $re2 ]] ; then
			gb_size=`echo "scale=1; $size/1024" | bc -l`
		fi

		if [[ $gb_size = .* ]] ; then
			gb_size=0$gb_size
		elif [[ $gb_size  = -.* ]] ; then
			neg_size=`echo "$gb_size * -1" | bc -l`
			gb_size=-0$neg_size
		fi

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
		cmd="sudo bash thp.sh $EXP_NUM $dataset $app ${gb_size}GB"
		echo $cmd
		$cmd

		kill $pid
		echo ""
	done
done
