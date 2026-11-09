# BlackBit

Maintainer: Eymen Akgün

BlackBit is an independent x86 kernel project. Kaya is not part of this project.

This project builds a bootable Multiboot2 ISO for QEMU using GRUB. The 32-bit kernel sets up its own GDT and IDT, handles CPU exceptions, remaps the PIC, receives keyboard input through IRQ1, tracks PIT timer ticks, and initializes a physical 4 KiB page allocator from the Multiboot2 memory map. VGA text and interactive COM1 serial consoles are available.

## Build and boot (WSL Ubuntu)

Install the toolchain if needed:

```sh
sudo apt install build-essential grub-pc-bin grub-common xorriso qemu-system-x86
```

Build and check the image:

```sh
make
make check
```

`make run` creates an ignored, blank 32 KiB credential disk named `blackbit-auth.img` if one does not exist. Keep this disk private and back it up; it contains the password verifier and salt. `make clean` leaves it intact. It is deliberately not included in the ISO or Git history.

Run the graphical VM or use the serial console:

```sh
make run
make run-serial
```

The ISO is `build/blackbit.iso`.

## Commands

The prompt supports `help`, `about`, `clear`, `mem`, `alloc`, `uptime`, `userdemo`, `userfault`, `getallow status`, `getallow setup`, `getallow login`, `getallow logout`, `getallow list`, `getallow grant user-demo memory.inspect`, `getallow revoke user-demo memory.inspect`, `getallow request <capability>`, `getallow audit`, `ember status`, and `halt`.

On first boot, `getallow setup` asks for a new 12–63 character admin password twice. It stores a random 16-byte RDRAND salt and a PBKDF2-HMAC-SHA-256 verifier (600,000 iterations) in two checksummed copies on sectors 8 and 9 of the separate IDE disk. Plaintext passwords are never written to the disk or kernel serial log. Enrollment fails closed if RDRAND or the disk is unavailable. `getallow login` opens a five-minute session; five failed logins trigger a 30-second in-memory delay. `getallow logout` closes the session and removes temporary grants.

`getallow` is a kernel policy module with deny-by-default checks and a 16-entry in-memory audit log. Try `getallow status`, `getallow list`, `getallow request system.halt`, `userdemo`, and `getallow audit`. The built-in demo runs at ring 3 and asks the kernel through `int 0x80`: `system.info` is allowed and `memory.inspect` is denied by default. After admin login, `getallow grant user-demo memory.inspect` permits it for the current session; logout or timeout removes that grant. The kernel assigns caller identity; the program cannot choose its own identity. `userfault` deliberately attempts to read a supervisor-only kernel page; the kernel catches its page fault, terminates that task, reclaims its pages, and returns to the shell.

## Scope

This is an early kernel, not a complete operating system. Paging currently identity-maps physical memory and provides an isolated user mapping for fixed test images; there is no general process loader or scheduler, filesystem, persistent account database, or broad device support. Credential storage supports one admin record on a dedicated legacy IDE disk in QEMU; general SATA/NVMe and FAT/ext4 are not implemented. The disk is not tamper-resistant: an attacker who can replace or edit the credential disk can reset/replace the verifier. There is no secure boot, TPM, or recovery flow. Use a strong, unique passphrase and do not describe this prototype as a hardened multi-user OS. EMBER remains unavailable until a host service protocol is designed; Python does not run inside this freestanding kernel.
