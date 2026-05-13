# miniz

miniz compression and ZIP package for xlang.

The package exposes zlib-compatible compression as lowercase hex strings and a small text-oriented ZIP API.

```xlang
import miniz

auto packed = miniz.compress_hex("hello miniz")
print(miniz.decompress_hex(packed))

print(miniz.zip_write_text("out.zip", "note.txt", "hello zip"))
print(miniz.zip_count("out.zip"))
print(miniz.zip_first_name("out.zip"))
print(miniz.zip_read_text("out.zip", "note.txt"))
```

Exports:

- `version()`: returns the bundled miniz version.
- `last_error()`: returns the last native error message for this thread.
- `compress_hex(text)`: compresses text and returns lowercase hex.
- `compress_hex_level(text, level)`: compresses text with an explicit miniz compression level.
- `decompress_hex(hex_text)`: decompresses hex produced by this package.
- `compress_file(input_path, output_path)`: compresses a file.
- `compress_file_level(input_path, output_path, level)`: compresses a file with an explicit level.
- `decompress_file(input_path, output_path)`: decompresses a file produced by this package.
- `zip_write_text(zip_path, entry_name, text)`: creates a ZIP archive containing one text entry.
- `zip_count(zip_path)`: returns the number of entries in a ZIP archive.
- `zip_first_name(zip_path)`: returns the first entry name.
- `zip_read_text(zip_path, entry_name)`: reads a text entry from a ZIP archive.
