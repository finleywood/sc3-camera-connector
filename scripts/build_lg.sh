#!/bin/sh

set -e

WORKDIR=$(pwd)
ARCH=$(uname -m)

curl --output lg.zip http://abyz.me.uk/lg/lg.zip

unzip lg.zip

cd lg

make -j$(nproc)

mv liblgpio.* /usr/lib/$ARCH-linux-gnu
mv lgpio.h /usr/include

cd ..

rm -rf lg*