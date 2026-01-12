#!/bin/bash

# User configuration has moved to config.mk.

# Make arguments.
MAKE_ARGS="-j V=1"

# Determine the make verb from the user action.
if [ "$1" == "build" ]; then
    MAKE_VERB=""
elif [ "$1" == "clean" ]; then
    MAKE_VERB="clean"
elif [ "$1" == "deploy-sd" ]; then
    MAKE_VERB="deploy-sd"
elif [ "$1" == "deploy-ftp" ]; then
    MAKE_VERB="deploy-ftp"
elif [ "$1" == "deploy-ryu" ]; then
    MAKE_VERB="deploy-ryu"
elif [ "$1" == "deploy-yuzu" ]; then
    MAKE_VERB="deploy-yuzu"
elif [ "$1" == "make-npdm-json" ]; then
    MAKE_VERB="npdm-json"
elif [ "$1" == "ryu-launch-log" ]; then
    MAKE_VERB="ryu-launch-log"
elif [ "$1" == "ryu-tail" ]; then
    MAKE_VERB="ryu-tail"
elif [ "$1" == "ryu-screenshot" ]; then
    MAKE_VERB="ryu-screenshot"
else
    echo "Invalid arg. (build/clean/deploy-sd/deploy-ftp/deploy-ryu/deploy-yuzu/ryu-launch-log/ryu-tail/ryu-screenshot)"
    exit 1
fi

make $MAKE_ARGS $MAKE_VERB
