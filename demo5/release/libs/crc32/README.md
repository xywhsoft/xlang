# crc32

CRC-32 checksum package for xlang.

Typical use:

```xlang
import crc32

print(crc32.checksum_hex("123456789"))
print(crc32.file_hex("data.bin"))
```

Streaming use:

```xlang
import crc32

var crc = crc32.init()
crc = crc32.update(crc, "hello ")
crc = crc32.update(crc, "world")
print(crc32.hex(crc32.finish(crc)))
```

Exports:

- `version()`: returns the package version string.
- `last_error()`: returns the last native error message.
- `checksum(text)`: returns the CRC-32 of a string as an integer.
- `checksum_hex(text)`: returns the CRC-32 of a string as 8 lowercase hex digits.
- `file(path)`: returns the CRC-32 of a file as an integer.
- `file_hex(path)`: returns the CRC-32 of a file as 8 lowercase hex digits.
- `init()`, `update(crc, text)`, `finish(crc)`: incremental CRC-32 API.
- `hex(crc)`: formats a CRC-32 integer as 8 lowercase hex digits.
