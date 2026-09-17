#!/bin/bash

if [ -z "$1" ] ; then
  echo "Usage: $0 <version>"
  exit 1
fi

VERSION=$1

rm -f CommsbusPkg.dmg

cp Commsbus/README_MAC.txt CommsbusPkg/

if dropdmg --config-name=CommsbusPkg --layout-folder CommsbusPkgLayout --volume-name="Commsbus v${VERSION}"  --APP_VERSION=v${VERSION}  --signing-identity=C7AF15C3BCF2AD2E5C102B9DB6502CFAE2C8CF3B CommsbusPkg
then
  mkdir -p ${VERSION}
  mv -v CommsbusPkg.dmg ${VERSION}/commsbus-${VERSION}-mac.dmg  	
else
  echo "Error making package DMG"
  exit 2
fi

