# imagehash

Perceptual image hashing package for xlang.

The package is backed by `stb_image`, `stb_image_resize2`, and the C standard
`math.h` functions used by pHash DCT calculation. It does not require OpenCV.

Typical use:

```xlang
import imagehash

var a = imagehash.ahash("left.png")
var b = imagehash.ahash("right.png")
print(a)
print(imagehash.hamming(a, b))
```

Available functions:

- `ahash(path)`: returns a 64-bit average hash as 16 lowercase hex digits.
- `dhash(path)`: returns a 64-bit difference hash as 16 lowercase hex digits.
- `phash(path)`: returns a 64-bit perceptual hash as 16 lowercase hex digits.
- `hamming(left, right)`: returns the Hamming distance between two hex hashes,
  or `-1` if either hash is invalid.
- `valid(hash)`: returns whether a hash string can be parsed as up to 64 bits.
- `last_error()`: returns the last native error message.
