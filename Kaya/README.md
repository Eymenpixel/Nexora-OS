# Kaya in Nexora OS

This folder contains Kayp, the C++17 `.kaya` package backend, and UST, the C++ HTTP/HTTPS source downloader. The root Makefile builds Kayp by default; `make ust` builds UST when the libcurl development package is installed.

The current BlackBit kernel has no general executable loader or libc/runtime, so these host-built tools are not yet executable from the booted kernel. `kaya fetch` calls UST; `kaya install` passes a local `.kaya` archive to Kayp. `make kaya-demo` exercises install/list/remove against a temporary fake root and never modifies the host system.

Only use trusted packages. The current archive installer is an early prototype and has not been audited for hostile archives or installer scripts.
