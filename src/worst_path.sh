#!/bin/bash

./start_node.sh -d worst_path/
echo "---------------------------------"

sleep 5
./manager_send 1 send 4 path:1-2-4

sleep 1
./bring_down.sh 1 2

sleep 5
./manager_send 1 send 4 path:1-3-4

./stop_node.sh

echo "---------------------------------"
echo "------------ result -------------"
echo "---------------------------------"

echo "log1"
cat logs/log1
echo ""

echo "log2"
cat logs/log2
echo ""

echo "log3"
cat logs/log3
echo ""

echo "log4"
cat logs/log4
echo ""
