#!/bin/bash

if [ -z "$1" ] ; then
   echo "Usage: $0 <version> <certpassword>"
   exit 1
fi

VERSION=$1

CERTPASS=$2

if [ -z "$CERTFILE" ] ; then
  echo You need to define CERTFILE env variable to sign anything
  exit 2
fi

#BUILDDIR='../Builds/VisualStudio2017/x64/Release'
#BUILDDIR32='../Builds/VisualStudio2017/Win32/Release32'
BUILDDIR='../build/Commsbus_artefacts/Release'
BUILDDIR32='../build32/Commsbus_artefacts/Release'

rm -rf Commsbus

mkdir -p Commsbus

# Commsbus ships the standalone application only -- the VST3/VST/AAX plugin
# targets were removed.
cp -v ../doc/README_WINDOWS.txt Commsbus/README.txt
cp -v ${BUILDDIR}/Standalone/Commsbus.exe Commsbus/

if [ -f ${BUILDDIR32}/Standalone/Commsbus.exe ] ; then
  cp -v ${BUILDDIR32}/Standalone/Commsbus.exe Commsbus/Commsbus32.exe
fi


# sign executable
#signtool.exe sign /v /t "http://timestamp.digicert.com" /f "$CERTFILE" /p "$CERTPASS" Commsbus/Commsbus.exe

mkdir -p instoutput
rm -f instoutput/*


iscc /O"instoutput" /DSIGN "/Ssigntool=signtool.exe sign /t http://timestamp.digicert.com /f ${CERTFILE} /p ${CERTPASS} \$f"  /DSBVERSION="${VERSION}" wininstaller.iss

#signtool.exe sign /v /t "http://timestamp.digicert.com" /f SonosaurusCodeSigningSectigoCert.p12 /p "$CERTPASS" instoutput/

#ZIPFILE=commsbus-${VERSION}-win.zip
#cp -v ../doc/README_WINDOWS.txt instoutput/README.txt
#rm -f ${ZIPFILE}
#(cd instoutput; zip  ../${ZIPFILE} Commsbus\ Installer.exe README.txt )

EXEFILE=commsbus-${VERSION}-win.exe
rm -f ${EXEFILE}
cp instoutput/Commsbus-${VERSION}-Installer.exe ${EXEFILE}
