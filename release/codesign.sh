#!/bin/bash



# codesign them with developer ID cert

POPTS="--strict  --force --options=runtime --sign C7AF15C3BCF2AD2E5C102B9DB6502CFAE2C8CF3B --timestamp"
AOPTS="--strict  --force --options=runtime --sign C7AF15C3BCF2AD2E5C102B9DB6502CFAE2C8CF3B --timestamp"

codesign ${AOPTS} --entitlements Commsbus.entitlements Commsbus/Commsbus.app
codesign ${POPTS} --entitlements Commsbus.entitlements  Commsbus/Commsbus.component
codesign ${POPTS} --entitlements Commsbus.entitlements Commsbus/Commsbus.vst3
codesign ${POPTS} --entitlements Commsbus.entitlements Commsbus/CommsbusInstrument.vst3
codesign ${POPTS} --entitlements Commsbus.entitlements  Commsbus/Commsbus.vst

# AAX is special
if [ -n "${AAXSIGNCMD}" ]; then
 echo "Signing AAX plugin"
 ${AAXSIGNCMD}  --in Commsbus/Commsbus.aaxplugin --out Commsbus/Commsbus.aaxplugin
fi


if [ "x$1" = "xonly" ] ; then
  echo Code-signing only
  exit 0
fi


mkdir -p tmp

# notarize them in parallel
./notarize-app.sh --submit=tmp/sbapp.uuid  Commsbus/Commsbus.app
./notarize-app.sh --submit=tmp/sbau.uuid Commsbus/Commsbus.component
./notarize-app.sh --submit=tmp/sbvst3.uuid Commsbus/Commsbus.vst3
./notarize-app.sh --submit=tmp/sbinstvst3.uuid Commsbus/CommsbusInstrument.vst3
./notarize-app.sh --submit=tmp/sbvst2.uuid Commsbus/Commsbus.vst 

if ! ./notarize-app.sh --resume=tmp/sbapp.uuid Commsbus/Commsbus.app ; then
  echo Notarization App failed
  exit 2
fi

if ! ./notarize-app.sh --resume=tmp/sbau.uuid Commsbus/Commsbus.component ; then
  echo Notarization AU failed
  exit 2
fi

if ! ./notarize-app.sh --resume=tmp/sbvst3.uuid Commsbus/Commsbus.vst3 ; then
  echo Notarization VST3 failed
  exit 2
fi

if ! ./notarize-app.sh --resume=tmp/sbinstvst3.uuid Commsbus/CommsbusInstrument.vst3 ; then
  echo Notarization Inst VST3 failed
  exit 2
fi
  
if ! ./notarize-app.sh --resume=tmp/sbvst2.uuid Commsbus/Commsbus.vst ; then
  echo Notarization VST2 failed
  exit 2
fi

#if ! ./notarize-app.sh Commsbus/Commsbus.aaxplugin ; then
#  echo Notarization AAX failed
#  exit 2
#fi





