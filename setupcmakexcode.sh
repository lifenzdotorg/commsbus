#!/bin/bash

# Commsbus builds the standalone application only, so the AAX/VST2 plugin SDK
# paths upstream SonoBus accepted here are no longer used.

TEAMOPT=""
if [ x"$APPLE_TEAMID" != x ] ; then
 TEAMOPT=-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=$APPLE_TEAMID
fi

# xcode
cmake -GXcode -B buildXcode ${TEAMOPT}

