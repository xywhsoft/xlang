# Featured Extension Libraries RC Gate

This document defines the release-candidate gate for the extension libraries
that have a dedicated page on the xlang website.

## Scope

The gate covers these 19 packages:

- aho_corasick, argon2, blake2, blake3, bloom, brotli, bson, bz2
- crc32, lz4, lzma, md5, miniz, murmur3, sha3, siphash128, snappy, xxhash, zstd

`test/featured_extlibs.json` is the executable manifest.  It binds each
package to a focused xlang test and an executable example.  Packages with a
stable standalone C surface also have a C smoke program.

Run the gate from `demo6`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_featured_extlibs_rc.ps1
```

Or run it through the suite entry:

```text
test\suites\11_featured_extlibs_rc.bat
```

## RC Requirements

- The package manifest, focused test, and example must all exist.
- Every focused xlang test must exit successfully.
- Every example must run successfully against `release/xl.exe`.
- C smoke programs must compile and run with `tools/run_extlib_c.ps1`.
- A package must not use retired XRT extension contracts such as
  `xrtSetError(message, take)` or `xrtBufferAdopt`.

## Ownership And Errors

`xlang_extlib_runtime.h` is a deliberately small shared adapter for source
packages.  It provides two rules:

- `xl6ExtlibSetError` publishes a normal current-XRT error with a package
  domain.
- `xl6ExtlibBufferTake` transfers an `xrtMalloc` byte block into an XRT
  buffer through `xrtBufferCreateTake`.

The adapter is not a second XRT ABI and must stay limited to public XRT APIs.

## Focused Boundary Coverage

- Argon2 validates cost bounds before allocation and reports invalid salt and
  memory parameters as language errors.
- Bloom checks malformed serialization, incompatible merge behavior, and
  counting-filter corruption.
- BSON checks malformed payloads, stream tails, ObjectId validation, and an
  UTF-8 file path through XRT file APIs.
- The BLAKE2/BLAKE3/SHA3/Murmur3/SipHash family exercises one-shot,
  streaming, clone, file, error, and Buffer-return paths through its xlang
  tests and examples.

Standalone C validation for hash packages with private streaming XRT source
modules remains a separate host-ABI task.  Their xlang package path is fully
covered by this gate; no unverified direct-C claim is made here.
