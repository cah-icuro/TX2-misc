#!/bin/bash

function max_settings() {
    command="sudo nvpmodel -m 0"
    echo $command
    $command

    command="sudo $HOME/jetson_clocks.sh"
    echo $command
    $command
}

function default_settings() {
    command="sudo $HOME/jetson_clocks.sh --restore"
    echo $command
    $command
}

if [ $# -ge 1 ] && [ $1 == "on" ] ; then
    max_settings
elif [ $# -ge 1 ] && [ $1 == "off" ] ; then
    default_settings
else
    echo "No argument given, defaulting to 'on'"
    max_settings
fi

