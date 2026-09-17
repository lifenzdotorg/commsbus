#!/bin/bash

CONFIG="Release"

# Commsbus builds the standalone application only, so the AAX/VST2 plugin SDK
# paths upstream SonoBus accepted here are no longer used.

if [ "$1" = "debug" ] ; then
  CONFIG="Debug"
fi

cmake -DCMAKE_BUILD_TYPE=$CONFIG -B build

