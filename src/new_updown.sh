#!/bin/bash

./start_node.sh -d example_topology/
echo "---------------------------------"

sleep 3
./manager_send 3 send 0 path:6-1-255-0 
echo "./manager_send 6 send 0 path:6-1-255-0"

sleep 0.5
./bring_up.sh 6 0

sleep 3
./manager_send 3 send 0 path:6-0 
echo "./manager_send 6 send 0 path:6-0"

sleep 0.5
./bring_down.sh 6 0

sleep 0.5
./bring_up.sh 6 0
sleep 0.5
./bring_down.sh 6 0

sleep 0.5
./bring_up.sh 6 0
sleep 0.5
./bring_down.sh 6 0

sleep 3
./manager_send 3 send 0 path:6-1-255-0 
echo "./manager_send 6 send 0 path:6-1-255-0"


./stop_node.sh

echo "---------------------------------"
echo "------------ result -------------"
echo "---------------------------------"
echo "log6"
cat logs/log6
echo ""

echo "log0"
cat logs/log0
echo ""

echo "log1"
cat logs/log1
echo ""

echo "log255"
cat logs/log255
echo ""

