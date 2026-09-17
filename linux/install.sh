#!/bin/bash

PREFIX=/usr/local

if [ -n "$1" ] ; then
  PREFIX="$1"
fi

echo "Installing Commsbus to ${PREFIX} ... (specify destination as command line argument if you want it elsewhere)"

BUILDDIR=../build/Commsbus_artefacts/Release

mkdir -p ${PREFIX}/bin
if ! cp ${BUILDDIR}/Standalone/commsbus  ${PREFIX}/bin/commsbus ; then
  echo
  echo "Looks like you need to run this as 'sudo $0'"
  exit 2
fi

mkdir -p ${PREFIX}/share/applications
cp commsbus.desktop ${PREFIX}/share/applications/commsbus.desktop
chmod +x ${PREFIX}/share/applications/commsbus.desktop

mkdir -p ${PREFIX}/share/pixmaps
cp ../images/commsbus_logo@2x.png ${PREFIX}/share/pixmaps/commsbus.png

# Commsbus installs the standalone application only -- the VST3/LV2 plugin
# targets were removed.

echo "Commsbus application installed"

