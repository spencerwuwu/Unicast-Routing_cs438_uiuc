#!/bin/bash
./start_node.sh -d e2e/
echo "---------------------------------"

sleep 7
./manager_send 0 send 45 path
echo "./manager_send 0 send 63 path"

sleep 1

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

echo "log63"
cat logs/log63
echo ""
