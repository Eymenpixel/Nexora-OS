# Capy and Application Compatibility Roadmap

**Status:** planning note. None of the compatibility targets below is implemented by this document.

## Names and responsibilities

- **BlackBit** remains its own freestanding 32-bit x86 kernel project.
- **Nexora OS** is the system project that can use BlackBit as its kernel and Kaya as its package manager.
- **Kaya** installs and removes packages. It is not an application runtime.
- **Capy** is the proposed application compatibility and launch layer. It selects a suitable runtime for an application format; it does not make every foreign binary native.
- **Sand** is Eymen's separate command/project. This roadmap does not define or implement Sand.

## Important current limit

BlackBit can boot to its kernel console, but it does not yet have a general executable loader, process scheduler, filesystem, libc, or complete userspace ABI. The existing Nexora CLI, Kayp, and UST are Linux/WSL host programs. They do not execute inside BlackBit. Therefore Capy cannot yet launch applications from the booted kernel.

## Formats need different actions

| Input | What the file is | Possible Capy action | Main requirement |
|---|---|---|---|
| `.exe` | Windows executable | Launch through a Wine-like compatibility runtime | Windows API compatibility and a suitable POSIX/userspace base. Wine is a compatibility layer, not a simple file converter. |
| `.deb` | Debian binary package archive | Inspect, then install/extract its files | A compatible Linux userspace, package metadata/dependency handling, and an installation root. A `.deb` is a package, not a program launcher. |
| `.apk` | Android application package | Delegate to an Android subsystem/runtime | ART plus Android framework services and native libraries; unpacking an APK alone does not run the app. |
| `.dmg` | macOS disk image | Inspect or mount/extract its contents | A disk-image reader. Extracting the image does not make its contents executable on BlackBit. |
| `.app` | macOS application bundle | No initial execution target | macOS/Darwin APIs and runtime compatibility; defer beyond the first Capy milestones. |

The most practical sequence is to build and test Capy as a Linux/WSL host tool first, with adapters that only activate when their runtime is installed. Native execution inside Nexora is a later target after BlackBit has a userspace and system-call ABI.

## Language targets

Language support means supplying a compiler or interpreter plus a stable Nexora ABI; a file extension alone is not enough.

| Language | Likely path |
|---|---|
| C, C++, Assembly | Cross-compile to a documented BlackBit/Nexora executable format after the loader and ABI exist. |
| C# | Port/package a compatible .NET runtime, or investigate ahead-of-time compilation for a supported target. |
| Python | Provide a Python interpreter and compatible standard-library/runtime support. |
| MicroPython | Port its interpreter and the platform functions it needs. |
| Scratch / TurboWarp | Use the TurboWarp Packager's HTML, archive, or executable output; Nexora would need a browser or JavaScript runtime to host it. |

## Suggested milestones

1. **Kernel execution foundation:** filesystem, executable loader, process lifecycle, memory isolation, and a documented system-call ABI.
2. **Native Nexora program:** compile and run one small C program through that ABI; then add C++ and assembly examples.
3. **Kaya integration:** define package metadata for target architecture, ABI, dependencies, and permissions. Keep install separate from launch.
4. **Capy host prototype:** add a read-only `capy inspect <file>` first. Add runtime adapters only when their prerequisites are detected; never silently run an unknown package.
5. **Compatibility adapters:** prioritize one target at a time. Defer Android and macOS application execution until the base system can host their much larger runtimes.
6. **Managed languages:** add Python, MicroPython, and C# only after the base ABI and runtime packaging are stable.

## BlackBit encryption feature

The requested encryption belongs to BlackBit. BlackBit currently has password verification based on PBKDF2-HMAC-SHA-256; password verification is not general file or disk encryption. The data to protect, key source, recovery behavior, and threat model still need definition before implementing encryption. Use established, reviewed cryptographic standards and implementations rather than inventing a new cipher. NIST distinguishes storage encryption modes such as XTS-AES from authenticated-encryption modes such as GCM; the correct choice depends on what data is being protected.

## References

- Wine project, [About Wine](https://www.winehq.org/about/)
- Android Open Source Project, [Android Runtime](https://source.android.com/docs/core/runtime)
- Debian Policy, [Binary packages](https://www.debian.org/doc/debian-policy/ch-binary.html)
- Apple Developer, [Packaging Mac software for distribution](https://developer.apple.com/documentation/xcode/packaging-mac-software-for-distribution)
- TurboWarp Docs, [Packager](https://docs.turbowarp.org/packager)
- Microsoft Learn, [.NET application deployment](https://learn.microsoft.com/en-us/dotnet/core/deploying/)
- NIST CSRC, [Cryptographic Standards and Guidelines](https://csrc.nist.gov/Projects/Cryptographic-Standards-and-Guidelines)
