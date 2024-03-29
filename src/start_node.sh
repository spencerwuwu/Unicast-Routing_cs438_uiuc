#!/bin/bash

# Array of nodes
node_array=(0 1 2 3 4 5 6 7 255)
#node_array=(1 2 3 4)

# Functions
usage() {
	echo "Usage: ./start_node.sh -d <dir>" 1>&2; exit 1;
}

# First, parse the arguments
dir_flag=0
dir_name=""

for var in "$@"
do
	if [ "$var" = "-d" ]
	then
		dir_flag=1
	else
		if [ "$var" = "-h" ]
		then
			usage
		fi
		if [ $dir_flag != 1 ]
		then
			usage
		else
			dir_name=$var
		fi
	fi
done

for i in "${node_array[@]}"
#for i in {0..63}
do
	echo "./ls_router $i ${dir_name}testinitcosts$i logs/log$i &"
	./ls_router $i ${dir_name}testinitcosts$i logs/log$i &
done
