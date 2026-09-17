#!/bin/bash

# Commsbus builds the standalone application only, so the AAX/VST2 plugin SDK
# paths upstream SonoBus accepted here are no longer used.

cmake -G "Visual Studio 15 2017" -T "host=x64" -B build32

