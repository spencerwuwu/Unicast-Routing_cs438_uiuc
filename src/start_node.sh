#!/bin/bash

# Array of nodes
node_array=(0 1 3 4 5 255)

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
do
	echo "./ls_router $i ${dir_name}testinitcosts$i log$i &"
	./ls_router $i ${dir_name}testinitcosts$i log$i &
done
