# lz4

LZ4 compression package for xlang.

The package stores compressed data in a small self-contained wrapper:
4 bytes of little-endian original size, then the LZ4 block payload.

```xlang
import lz4

auto packed = lz4.compress_hex("hello")
print(lz4.decompress_hex(packed))

print(lz4.compress_file("input.bin", "input.bin.lz4"))
print(lz4.decompress_file("input.bin.lz4", "input.out"))
```

Exports:

- `version()`: returns the LZ4 runtime version string.
- `last_error()`: returns the last native error message.
- `compress_hex(text)`: compresses UTF-8/text input and returns lowercase hex.
- `decompress_hex(hex_text)`: decompresses hex produced by `compress_hex()`.
- `compress_file(input_path, output_path)`: compresses a file.
- `decompress_file(input_path, output_path)`: decompresses a file produced by this package.
