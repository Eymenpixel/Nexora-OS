#!/bin/bash
# Kaya OS - Nano Package Build Script

VERSION="8.0"
NANO_SOURCE="https://www.nano-editor.org/dist/v8/nano-8.0.tar.gz"

echo "Downloading Nano $VERSION..."
wget -q -O nano-$VERSION.tar.gz $NANO_SOURCE

echo "Extracting Nano..."
tar -xzf nano-$VERSION.tar.gz
cd nano-$VERSION

echo "Configuring Nano for Kaya OS..."
./configure --prefix=/usr --sysconfdir=/etc

echo "Building Nano..."
make

echo "Installing Nano..."
make install DESTDIR=../kaya-root

echo "Patching version string..."
# Remove Ubuntu/Debian references
sed -i 's/Ubuntu/Kaya OS/g' /usr/bin/nano
sed -i 's/Debian/Kaya OS/g' /usr/bin/nano

echo "Copying build to /home/kaya/pool..."
mkdir -p /home/kaya/pool
cp -r kaya-root/* /home/kaya/pool/ 2>/dev/null || true

echo "Nano successfully built for Kaya OS!"
