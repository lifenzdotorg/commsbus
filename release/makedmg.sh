#!/bin/bash

if [ -z "$1" ] ; then
  echo "Usage: $0 <version>"
  exit 1
fi

VERSION=$1

rm -f Commsbus.dmg

if dropdmg --layout-folder CommsbusLayout --volume-name="Commsbus v${VERSION}"  --APP_VERSION=v${VERSION}  --signing-identity=C7AF15C3BCF2AD2E5C102B9DB6502CFAE2C8CF3B Commsbus
then
  mkdir -p ${VERSION}
  mv -v Commsbus.dmg ${VERSION}/commsbus-${VERSION}-mac.dmg  	
else
  echo "Error making DMG"
  exit 2
fi

