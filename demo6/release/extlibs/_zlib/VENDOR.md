# zlib vendor record

- Upstream: https://github.com/madler/zlib
- Version: 1.3.2
- Source archive: `v1.3.2.zip`
- SHA-256: `31FD9FEE98812ABCF147D0E103BC4D2F983C35A8D7A807A328A299F3A74E0050`
- License: zlib License, see `third_party/zlib/LICENSE`

Only the core Deflate/zlib sources required by `gzip` and `zip` are included.
The `gzopen` path API is intentionally omitted; xlang uses XRT streams so UTF-8
paths, Buffer input and VFS input share one implementation.
