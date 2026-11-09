# Nexora OS

Nexora OS is an early-stage operating system prototype by **Eymen Akgün**. It brings together the bootable **BlackBit** x86 kernel and **Kaya**, a package-management project for `.kaya` packages.

## How the projects fit together

- **Nexora OS** is the overall operating system project and development workflow.
- **BlackBit** is a separate 32-bit x86 kernel project that Nexora can use as its kernel component. Its ISO boots and identifies as BlackBit, not as Nexora. It boots through GRUB Multiboot2 and can be launched in QEMU.
- **Kaya** provides the `.kaya` package workflow. Its Kayp C++ tool handles package operations, while UST handles HTTP/HTTPS downloads.
- **Nexora CLI** is a Linux/WSL host-side developer tool that brings kernel and Kaya commands together.

> **Current integration boundary:** BlackBit does not yet provide a general userspace, executable loader, or process runtime. Kayp, UST, and the Nexora CLI currently run on Linux/WSL; they do not run inside the booted kernel. This repository is a prototype, not a production-ready or secure daily-use operating system.

The printable Turkish project overview is available at [`docs/Nexora_OS_Proje_Tanitimi.pdf`](docs/Nexora_OS_Proje_Tanitimi.pdf).

## Build in WSL Ubuntu

Install the build and emulator dependencies:

```sh
sudo apt update
sudo apt install build-essential cmake libcurl4-openssl-dev grub-pc-bin grub-common xorriso qemu-system-x86
```

From the repository root:

```sh
make
make check
make kaya-demo
make kaya-package
```

Build UST separately with `make ust`. Its HTTP client uses libcurl.

Run the Nexora developer CLI:

```sh
./build/nexora
./build/nexora status
./build/nexora kaya list
```

Boot the BlackBit kernel in QEMU:

```sh
make run
```

## Repository layout

- `kernel/` - BlackBit kernel source, build rules, and security notes.
- `Kaya/kayp/` - C++ package-management tool.
- `Kaya/ust/` - C++ source for the UST downloader.
- `Kaya/packages/` - package source and examples.
- `src/` - Nexora host-side CLI.
- `Assets/gui/` - interface assets reserved for future GUI work.
- `docs/` - project documentation.

Only install `.kaya` packages from sources you trust. Package installation can execute package-provided actions.


## License

Nexora-specific files are licensed under the MIT License; see `LICENSE`. Components that carry their own license or licensing notice retain those terms. In particular, BlackBit has its own license file under `kernel/`, and Kaya component notices under `Kaya/` remain applicable. The user-provided GUI assets in `Assets/gui/` are not relicensed by this notice.
