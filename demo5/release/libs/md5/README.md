# md5

Small MD5 extension package for xlang.

## API

```xlang
import md5

var value = md5.hex("abc")
var file_hash = md5.file("path\\to\\file.bin")
var ok = md5.valid(value)
```

Exports:

- `version() as string`
- `last_error() as string`
- `hex(string text) as string`
- `file(string path) as string`
- `valid(string text) as bool`

Known test vectors:

- `md5.hex("")` -> `d41d8cd98f00b204e9800998ecf8427e`
- `md5.hex("abc")` -> `900150983cd24fb0d6963f7d28e17f72`
