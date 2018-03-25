#!/bin/bash
ps | grep ls_router | awk '{system("echo kill " $1); system("kill -9 " $1)}'
