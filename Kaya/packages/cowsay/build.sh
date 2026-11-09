#!/bin/bash
# Kaya OS - Cowsay Package Build Script

VERSION="3.7.0"
COWSAY_SOURCE="https://github.com/cowsay-org/cowsay/archive/refs/tags/v$VERSION.tar.gz"

echo "Downloading Cowsay $VERSION..."
wget -q -O cowsay-$VERSION.tar.gz $COWSAY_SOURCE

echo "Extracting Cowsay..."
tar -xzf cowsay-$VERSION.tar.gz
cd cowsay-$VERSION

echo "Configuring Cowsay for Kaya OS..."
mkdir -p ../kaya-root/usr/bin
mkdir -p ../kaya-root/usr/share/cowsay/cows

echo "Installing Cowsay..."
install -m 755 cowsay ../kaya-root/usr/bin/cowsay
ln -sf cowsay ../kaya-root/usr/bin/cowthink

echo "Installing cow files..."
cp cows/* ../kaya-root/usr/share/cowsay/cows/

echo "Copying build to /home/kaya/pool..."
mkdir -p /home/kaya/pool
cp -r ../kaya-root/* /home/kaya/pool/ 2>/dev/null || true

echo "Cowsay successfully built for Kaya OS!"
