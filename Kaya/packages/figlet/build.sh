#!/bin/bash
# Kaya OS - Figlet Package Build Script

VERSION="2.2.5"
FIGLET_SOURCE="http://www.figlet.org/figlet-$VERSION.tar.gz"

echo "Downloading Figlet $VERSION..."
wget -q -O figlet-$VERSION.tar.gz $FIGLET_SOURCE

echo "Extracting Figlet..."
tar -xzf figlet-$VERSION.tar.gz
cd figlet-$VERSION

echo "Configuring Figlet for Kaya OS..."
mkdir -p ../kaya-root/usr/bin
mkdir -p ../kaya-root/usr/share/figlet

echo "Building Figlet..."
make

echo "Installing Figlet..."
make install DESTDIR=../kaya-root prefix=/usr

echo "Copying build to /home/kaya/pool..."
mkdir -p /home/kaya/pool
cp -r ../kaya-root/* /home/kaya/pool/ 2>/dev/null || true

echo "Figlet successfully built for Kaya OS!"
