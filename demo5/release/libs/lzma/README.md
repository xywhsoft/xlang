# lzma

LZMA compression package for xlang.

The package stores compressed data in a small self-contained wrapper:
5 bytes of LZMA properties, 8 bytes of little-endian original size, then the LZMA payload.

```xlang
import lzma

auto packed = lzma.compress_hex("hello")
print(lzma.decompress_hex(packed))

print(lzma.compress_file_level("input.bin", "input.bin.lzma", 7))
print(lzma.decompress_file("input.bin.lzma", "input.out"))
```

Exports:

- `version()`: returns the package version string.
- `last_error()`: returns the last native error message.
- `compress_hex(text)`: compresses text with level 5 and returns lowercase hex.
- `compress_hex_level(text, level)`: compresses text with an explicit LZMA level.
- `decompress_hex(hex_text)`: decompresses hex produced by `compress_hex()`.
- `compress_file(input_path, output_path)`: compresses a file with level 5.
- `compress_file_level(input_path, output_path, level)`: compresses a file with an explicit LZMA level.
- `decompress_file(input_path, output_path)`: decompresses a file produced by this package.
