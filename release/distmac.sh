#!/bin/bash

if [ -z "$1" ] ; then
   echo "Usage: $0 <version>"
   exit 1
fi

VERSION=$1


BUILDDIR=../build/Commsbus_artefacts/Release

rm -rf Commsbus

mkdir -p Commsbus


cp ../doc/README_MAC.txt Commsbus/

# Commsbus ships the standalone application only -- the AU/VST3/VST/AAX plugin
# builds were removed along with the plugin targets.
cp -pLRv ${BUILDDIR}/Standalone/Commsbus.app  Commsbus/


# this codesigns and notarizes everything
if ! ./codesign.sh ; then
  echo
  echo Error codesign/notarizing, stopping
  echo
  exit 1
fi

# make installer package (and sign it)

rm -f macpkg/CommsbusTemp.pkgproj

if ! ./update_package_version.py ${VERSION} macpkg/Commsbus.pkgproj macpkg/CommsbusTemp.pkgproj ; then
  echo
  echo Error updating package project versions
  echo
  exit 1
fi

if ! packagesbuild  macpkg/CommsbusTemp.pkgproj ; then
  echo 
  echo Error building package
  echo
  exit 1
fi

mkdir -p CommsbusPkg
rm -f CommsbusPkg/*

if ! productsign --sign ${INSTSIGNID} --timestamp  macpkg/build/Commsbus\ Installer.pkg CommsbusPkg/Commsbus\ Installer.pkg ; then
  echo 
  echo Error signing package
  echo
  exit 1
fi

# make dmg with package inside it

if ./makepkgdmg.sh $VERSION ; then

   ./notarizedmg.sh ${VERSION}/commsbus-${VERSION}-mac.dmg

   echo
   echo COMPLETED DMG READY === ${VERSION}/commsbus-${VERSION}-mac.dmg
   echo
   
fi
