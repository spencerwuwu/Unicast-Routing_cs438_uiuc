#!/bin/bash
./start_node.sh -d e2e/
echo "---------------------------------"

sleep 5
./manager_send 0 send 20 path
echo "./manager_send 0 send 20 path"

sleep 5

./stop_node.sh

echo "---------------------------------"
echo "------------ result -------------"
echo "---------------------------------"

echo "log0"
cat logs/log0
echo ""

echo "log1"
cat logs/log1
echo ""

echo "log2"
cat logs/log2
echo ""

echo "log20"
cat logs/log20
echo ""
