#!/bin/bash
ZSIMPATH=$(pwd)
PINPATH="$ZSIMPATH/pin"
LIBCONFIGPATH="$ZSIMPATH/libconfig"

echo "Cleaning ..."
export PINPATH
export LIBCONFIGPATH
scons -c
