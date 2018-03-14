#!/bin/bash

# Command to search for crash: find / -name *.crash 2>/dev/null

CRASH_PATH="/var/crash"

sudo rm $CRASH_PATH/*
echo "Purged crash diretory"
