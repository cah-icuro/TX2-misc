#!/bin/bash

# Command to search for crash: find / -name *.crash 2>/dev/null

SWEEP_PATH="/home/nvidia/sweep-sdk/libsweep/build"
command=$SWEEP_PATH/sweep-ctl" $@"

echo $command
$command
