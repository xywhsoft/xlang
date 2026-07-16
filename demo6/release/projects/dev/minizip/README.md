# minizip - LZ77 compression and file archiving tool

LZSS (LZ77 variant) compression with hash-chain match finding, custom MXA
archive format, and CRC32 integrity checking.

## usage

```
minizip compress <input_file> <output.mxz>       compress single file
minizip decompress <input.mxz> <output_file>     decompress single file
minizip archive create <archive.mxz> <files...>  create multi-file archive
minizip archive list <archive.mxz>               list archive contents
minizip archive extract <archive.mxz>            extract all files
minizip test [input_file]                       compress+decompress roundtrip
minizip bench <input_file> [iterations=10]       performance benchmark
minizip --help | -h                              show help
```

## algorithm

**LZSS** (LZ77 variant) with flag-byte grouped tokens:

| parameter       | value    | encoding           |
|-----------------|----------|--------------------|
| sliding window  | 4096 B   | 12-bit offset      |
| look-ahead      | 18 B     | 4-bit length       |
| min match       | 3 B      | 3..18 range        |
| match finding   | hash chain (zlib-style), max chain depth 256 |

Token stream: data grouped into 8-token blocks, each preceded by 1 flag byte.
Flag bit 1 = match (2 bytes: offset+length), bit 0 = literal (1 byte).

## archive format (MXA)

```
Header (8 bytes):
  "MXA" + version(1) + fileCount(uint16 LE) + reserved(2)

Per file:
  nameLen(uint16 LE) + name(UTF-8) + method(1) +
  origSize(uint32 LE) + storedSize(uint32 LE) + CRC32(uint32 LE) +
  data(storedSize bytes)

method: 0=stored (incompressible fallback), 1=LZSS
```

## verification

```bat
cd /d D:\GIT\x-lang\demo6\release
xl run  projects\dev\minizip\minizip.xl -- test
xl run  projects\dev\minizip\minizip.xl -- test <some_file>
xl build projects\dev\minizip\minizip.xl -o projects\dev\minizip\minizip.exe
```

Both paths must EXIT=0. Roundtrip (compress → decompress) verified via
byte-by-byte comparison and CRC32.

## implementation notes

- `Buffer` 是受管理的 native-backed class，可直接作为参数和返回值使用。
- **Compressed data** stored/transferred in `array<int>` (each element is a
  byte 0..255). Basic type, freely returnable/passable.
- typed 容器和 class 使用受管理引用语义；独立副本必须显式 `copy` 或 `clone`。
- **CRC32** implemented manually because `hash.*` stdlib functions only accept
  strings, not binary data. `hash.u64` used for file-name fingerprinting.
