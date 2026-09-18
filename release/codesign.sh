#!/bin/bash

# Code-sign and notarize the Commsbus app.
#
# Commsbus ships the standalone application only -- the AU/VST3/VST2/AAX plugin
# targets upstream SonoBus built were removed, so there is nothing else to sign.
#
# The signing identity is taken from CODESIGN_IDENTITY if set, otherwise the
# first "Developer ID Application" identity in the keychain is used.

set -e

if [ -z "${CODESIGN_IDENTITY}" ]; then
  CODESIGN_IDENTITY=$(security find-identity -v -p codesigning \
                      | sed -n 's/.*"\(Developer ID Application: [^"]*\)".*/\1/p' \
                      | head -1)
fi

if [ -z "${CODESIGN_IDENTITY}" ]; then
  echo "No Developer ID Application identity found, and CODESIGN_IDENTITY is not set." >&2
  exit 1
fi

echo "Signing with: ${CODESIGN_IDENTITY}"

AOPTS="--strict --force --options=runtime --sign ${CODESIGN_IDENTITY} --timestamp"

codesign ${AOPTS} --entitlements Commsbus.entitlements Commsbus/Commsbus.app

if [ "x$1" = "xonly" ] ; then
  echo Code-signing only
  exit 0
fi


mkdir -p tmp

./notarize-app.sh --submit=tmp/sbapp.uuid  Commsbus/Commsbus.app

if ! ./notarize-app.sh --resume=tmp/sbapp.uuid Commsbus/Commsbus.app ; then
  echo Notarization App failed
  exit 2
fi
