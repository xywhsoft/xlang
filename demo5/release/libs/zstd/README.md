# zstd

Zstandard compression package for xlang.

```xlang
import zstd

auto packed = zstd.compress_hex("hello")
print(zstd.decompress_hex(packed))

print(zstd.compress_file_level("input.bin", "input.bin.zst", 9))
print(zstd.decompress_file("input.bin.zst", "input.out"))
```

Exports:

- `version()`: returns the Zstandard runtime version string.
- `last_error()`: returns the last native error message.
- `compress_hex(text)`: compresses text with the default level and returns lowercase hex.
- `compress_hex_level(text, level)`: compresses text with an explicit zstd level.
- `decompress_hex(hex_text)`: decompresses hex produced by `compress_hex()`.
- `compress_file(input_path, output_path)`: compresses a file with the default level.
- `compress_file_level(input_path, output_path, level)`: compresses a file with an explicit zstd level.
- `decompress_file(input_path, output_path)`: decompresses a zstd frame file.
