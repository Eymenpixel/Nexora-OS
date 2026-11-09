# UST - Universal Source Tool (C++)

UST is a curl-style HTTP CLI written in portable C++17. It uses libcurl for HTTP/HTTPS, TLS verification, and redirects. The same source can be built on Linux/WSL, macOS, and Windows. Kaya's future host can also integrate the C++ source where a compatible libcurl build is available.

## Dependencies

- C++17 compiler
- CMake 3.16+
- libcurl development headers and library

Ubuntu/WSL: `sudo apt install build-essential cmake libcurl4-openssl-dev`

## Build

~~~sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
~~~

The executable is `build/ust` on Linux/macOS and usually `build/Release/ust.exe` with a multi-configuration Windows generator. Run `ust --help` for all options.

## Examples

~~~sh
ust https://example.com
ust -L --fail --max-time 30 -o downloads/file.zip https://example.com/file.zip
ust -I https://example.com
ust -H 'Accept: application/json' https://api.example.com/data
ust -X POST -H 'Content-Type: application/json' -d '{"ok":true}' https://api.example.com/items
ust https://example.com -o page.html
ust -sL https://example.com
~~~

Supports options after the URL and combined short flags such as `-sL`. Available options: `-o/--output`, `-O/--remote-name`, `-I/--head`, `-i/--include`, `-X/--request`, repeatable `-H/--header`, repeatable `-d/--data`, `--data-binary @FILE`, `-u/--user` (Basic auth), `-A/--user-agent`, `-L/--location`, `-v/--verbose`, `-s/--silent`, `--fail`, `--fail-with-body`, and `--max-time`.

Downloads stream to a temporary sibling file and replace the destination after successful transfer. TLS peer and host verification stay enabled. Requests are limited to HTTP/HTTPS, and `-L` follows redirects only to HTTPS. `--max-time` accepts fractional seconds, for example `--max-time 0.5`. A HEAD request with `-I -o` prints headers without creating an empty output file. Kaya should still verify package hashes independently.

This is native C++ source, not a `.kay` manifest or WASM module. A Nexora build needs a libcurl port/package and a compatible TLS backend.
