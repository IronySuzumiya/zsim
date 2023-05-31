#!/bin/bash
ZSIMPATH=$(pwd)
PINPATH="$ZSIMPATH/pin"
LIBCONFIGPATH="$ZSIMPATH/libconfig"
NUMCPUS=$(grep -c ^processor /proc/cpuinfo)

echo "Compiling only ZSim ..."
export PINPATH
export LIBCONFIGPATH
scons -j$NUMCPUS
