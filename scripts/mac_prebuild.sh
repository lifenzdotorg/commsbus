#!/bin/bash

#cp -v ../../scripts/Commsbus-mac-sandbox.entitlements Commsbus.entitlements

if grep sandbox Commsbus.entitlements &> /dev/null ; then
   cp -v ../../scripts/Commsbus-mac.entitlements Commsbus.entitlements
fi
