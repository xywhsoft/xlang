# stb_image

Lightweight image loading package for xlang, backed by `stb_image.h`.

Typical use:

```xlang
import stb_image

var img = stb_image.load("input.png", 4)
print(stb_image.width(img))
print(stb_image.height(img))
print(stb_image.channels(img))
stb_image.release(img)
```

Loaded image handles own their native pixel buffer. Use `stb_image.data(img)` to
pass the raw pixel pointer to other low-level image packages such as
`stb_image_resize2` or `stb_image_write`.
