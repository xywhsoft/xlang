# libzip vendor record

- Upstream: https://github.com/nih-at/libzip
- Version: 1.11.4
- Commit: `6f8a0cdd24a0dc6cce9dac4a7679da784ab124ea`
- License: BSD-style license in `third_party/libzip/LICENSE`

The package retains only the official sources required by the manifest, a fixed
cross-platform configuration, generated error strings and two thin XRT platform
units. Only Deflate is enabled; bzip2/xz/zstd codecs, crypto backends, WinZip AES
and the native file-source implementations are intentionally excluded. Deflate
uses the shared `_zlib` package. The xlang-facing `zip` package never calls
libzip path-opening APIs.
