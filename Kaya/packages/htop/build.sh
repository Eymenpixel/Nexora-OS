#!/bin/bash
# Kaya OS - Htop Package Build Script

VERSION="3.2.2"
HTOP_SOURCE="https://github.com/htop-dev/htop/releases/download/$VERSION/htop-$VERSION.tar.xz"

echo "Downloading Htop $VERSION..."
wget -q -O htop-$VERSION.tar.xz $HTOP_SOURCE

echo "Extracting Htop..."
tar -xJf htop-$VERSION.tar.xz
cd htop-$VERSION

echo "Configuring Htop for Kaya OS..."
./configure --prefix=/usr --sysconfdir=/etc

echo "Building Htop..."
make

echo "Installing Htop..."
make install DESTDIR=../kaya-root

echo "Applying Kaya OS customizations..."
sed -i 's/Linux/Kaya OS/g' ../kaya-root/usr/bin/htop || true

echo "Copying build to /home/kaya/pool..."
mkdir -p /home/kaya/pool
cp -r ../kaya-root/* /home/kaya/pool/ 2>/dev/null || true

echo "Htop successfully built for Kaya OS!"
