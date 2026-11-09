#!/bin/bash
# Kaya OS - Screenfetch Package Build Script

VERSION="3.9.1"
SCREENFETCH_SOURCE="https://github.com/KittyKatt/screenFetch/archive/v$VERSION.tar.gz"

echo "Downloading Screenfetch $VERSION..."
wget -q -O screenfetch-$VERSION.tar.gz $SCREENFETCH_SOURCE

echo "Extracting Screenfetch..."
tar -xzf screenfetch-$VERSION.tar.gz
cd screenFetch-$VERSION

echo "Configuring Screenfetch for Kaya OS..."
mkdir -p ../kaya-root/usr/bin
mkdir -p ../kaya-root/usr/share/screenfetch

echo "Installing Screenfetch..."
install -m 755 screenfetch ../kaya-root/usr/bin/screenfetch

echo "Installing themes and assets..."
cp -r screenFetch_Themes ../kaya-root/usr/share/screenfetch/

echo "Installing Kaya OS logo..."
cp kaya-logo.txt ../kaya-root/usr/share/screenfetch/kaya-logo.txt

echo "Patching for Kaya OS..."
sed -i 's/Ubuntu/Kaya OS/g' ../kaya-root/usr/bin/screenfetch
sed -i 's/Debian/Kaya OS/g' ../kaya-root/usr/bin/screenfetch

echo "Copying build to /home/kaya/pool..."
mkdir -p /home/kaya/pool
cp -r ../kaya-root/* /home/kaya/pool/ 2>/dev/null || true

echo "Screenfetch successfully built for Kaya OS!"
