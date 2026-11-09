#!/bin/bash
# Kaya OS - Vim Package Build Script

VERSION="9.0"
VIM_SOURCE="https://github.com/vim/vim/archive/v$VERSION.tar.gz"

echo "Downloading Vim $VERSION..."
wget -q -O vim-$VERSION.tar.gz $VIM_SOURCE

echo "Extracting Vim..."
tar -xzf vim-$VERSION.tar.gz
cd vim-$VERSION/src

echo "Configuring Vim for Kaya OS..."
./configure --prefix=/usr \
  --with-features=huge \
  --enable-gui=no \
  --enable-python3interp=yes \
  --enable-rubyinterp=no \
  --with-python3-config-dir=/usr/lib/python3/config

echo "Building Vim..."
make

echo "Installing Vim..."
make install DESTDIR=../../kaya-root

echo "Creating vim symlinks..."
ln -sf vim ../../kaya-root/usr/bin/vi

echo "Patching for Kaya OS..."
sed -i 's/Ubuntu/Kaya OS/g' ../../kaya-root/usr/bin/vim || true

echo "Copying build to /home/kaya/pool..."
mkdir -p /home/kaya/pool
cp -r ../../kaya-root/* /home/kaya/pool/ 2>/dev/null || true

echo "Vim successfully built for Kaya OS!"
