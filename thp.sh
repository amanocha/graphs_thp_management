#!/bin/bash

EXP=$1
DATASET=$2
APP=$3
CONFIG=$4

# baseline
echo "sync ; echo 3 > /proc/sys/vm/drop_caches"
sync ; echo 3 > /proc/sys/vm/drop_caches

echo "echo madvise > /sys/kernel/mm/transparent_hugepage/enabled"
echo madvise > /sys/kernel/mm/transparent_hugepage/enabled

echo "echo madvise > /sys/kernel/mm/transparent_hugepage/defrag"
echo madvise > /sys/kernel/mm/transparent_hugepage/defrag
        
echo "sudo python3 go.py --experiment=$EXP --dataset=$DATASET --app=$APP --config=$CONFIG"
sudo python3 go.py --experiment=$EXP --dataset=$DATASET --app=$APP --config=$CONFIG

# thp
echo "sync ; echo 3 > /proc/sys/vm/drop_caches"
sync ; echo 3 > /proc/sys/vm/drop_caches

echo "echo always > /sys/kernel/mm/transparent_hugepage/enabled"
echo always > /sys/kernel/mm/transparent_hugepage/enabled

echo "echo always > /sys/kernel/mm/transparent_hugepage/defrag"
echo always > /sys/kernel/mm/transparent_hugepage/defrag
        
echo "sudo python3 go.py --experiment=$EXP --dataset=$DATASET --app=$APP --config=$CONFIG"
sudo python3 go.py --experiment=$EXP --dataset=$DATASET --app=$APP --config=$CONFIG

exit

# thp no defrag
echo "sync ; echo 3 > /proc/sys/vm/drop_caches"
sync ; echo 3 > /proc/sys/vm/drop_caches

echo "echo always > /sys/kernel/mm/transparent_hugepage/enabled"
echo always > /sys/kernel/mm/transparent_hugepage/enabled

echo "echo madvise > /sys/kernel/mm/transparent_hugepage/defrag"
echo madvise > /sys/kernel/mm/transparent_hugepage/defrag
        
echo "sudo python3 go.py --experiment=$EXP --dataset=$DATASET --app=$APP --config=$CONFIG"
sudo python3 go.py --experiment=$EXP --dataset=$DATASET --app=$APP --config=$CONFIG

