#!/bin/bash
./start_node.sh -d e2e/
echo "---------------------------------"

sleep 5
./manager_send 0 send 39 path

sleep 1

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

echo "log31"
cat logs/log31
echo ""

echo "log39"
cat logs/log39
echo ""
