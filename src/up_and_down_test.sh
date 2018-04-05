#!/bin/bash

./bring_down.sh 4 7
./start_node.sh -d example_topology/
echo "---------------------------------"

sleep 5
./manager_send 5 send 7 path:5-6-7
echo "./manager_send 5 send 7 path:5-6-7" 

sleep 1
./bring_down.sh 6 7

sleep 5
./manager_send 5 send 7 cant-send
echo "./manager_send 5 send 7 cantsend"

sleep 1
./bring_up.sh 4 7
./bring_up.sh 6 7

sleep 5
./manager_send 5 send 7 path:5-1-4-7
echo "./manager_send 5 send 7 path:5-1-4-7"

sleep 1
./manager_send 6 send 3 path:6-5-2-3 
echo "./manager_send 5 send 7 path:6-5-2-3"

sleep 1
./manager_send 6 send 0 path:6-1-255-0 
echo "./manager_send 5 send 7 path:6-1-255-0"

sleep 1
./bring_up.sh 6 0

sleep 5
./manager_send 6 send 0 path:6-0 
echo "./manager_send 5 send 7 path:6-0"

./stop_node.sh

echo "---------------------------------"
echo "------------ result -------------"
echo "---------------------------------"
echo "log5"
cat logs/log5
echo ""

echo "log2"
cat logs/log2
echo ""

echo "log3"
cat logs/log3
echo ""

echo "log1"
cat logs/log1
echo ""

echo "log4"
cat logs/log4
echo ""

echo "log6"
cat logs/log6
echo ""

echo "log7"
cat logs/log7
echo ""

echo "log255"
cat logs/log255
echo ""

echo "log0"
cat logs/log0
echo ""
