# Nexora splash screen

This GTK 3 splash loads the checked-in logo from `Assets/gui/logo.png`. It searches relative to the executable, the project root, and the current directory, so it does not depend on a user-specific absolute path. Set `NEXORA_ASSETS_DIR` to the directory containing `logo.png`, or pass the logo path as the first command-line argument to override discovery.

Build from the repository root in WSL Ubuntu:

```sh
sudo apt install build-essential pkg-config libgtk-3-dev libcairo2-dev
mkdir -p build
g++ -std=c++17 src/gui/splash.cpp -o build/NexoraGUI $(pkg-config --cflags --libs gtk+-3.0 cairo)
./build/NexoraGUI
```

The `make nexora-iso` target uses the same logo asset as a static GRUB splash before starting BlackBit. It does not run this GTK application from the kernel. Launching GTK from Nexora itself requires a userspace, executable loader, graphics support, and a compatible runtime.
