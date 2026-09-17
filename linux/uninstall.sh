#!/bin/bash

PREFIX=/usr/local

if [ -n "$1" ] ; then
  PREFIX="$1"
fi

echo "Un-Installing Commsbus from ${PREFIX} ... (specify destination as command line argument if you have it elsewhere)"

# remove old binary name
if [ -f ${PREFIX}/bin/Commsbus ] ; then
  if ! rm -f ${PREFIX}/bin/Commsbus ; then
    echo
    echo "Looks like you need to run this with 'sudo $0'"
    exit 2
  fi
fi

if [ -f ${PREFIX}/bin/commsbus ] ; then
  if ! rm -f ${PREFIX}/bin/commsbus ; then
    echo
    echo "Looks like you need to run this with 'sudo $0'"
    exit 2
  fi
fi

rm -f ${PREFIX}/share/applications/commsbus.desktop
rm -f ${PREFIX}/pixmaps/commsbus.png

# Kept so that upgrading from an older SonoBus/Commsbus install that did ship
# plugins still cleans them up, even though Commsbus no longer builds them.
rm -rf ${PREFIX}/lib/vst3/Commsbus.vst3
rm -rf ${PREFIX}/lib/vst3/CommsbusInstrument.vst3
# remove old VST name
rm -rf ${PREFIX}/lib/vst3/commsbus.vst3

echo "Commsbus uninstalled"
