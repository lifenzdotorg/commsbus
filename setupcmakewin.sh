#!/bin/bash

# Commsbus builds the standalone application only, so the AAX/VST2 plugin SDK
# paths upstream SonoBus accepted here are no longer used.

cmake -G "Visual Studio 16 2019" -A "x64" -B build

