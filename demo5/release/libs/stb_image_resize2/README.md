# stb_image_resize2

Lightweight image resizing package for xlang, backed by `stb_image_resize2.h`.

The package accepts raw `uint8` pixel pointers and returns an owned resized
image buffer:

```xlang
import stb_image
import stb_image_resize2 as stbir

var img = stb_image.load("input.png", 4)
var small = stbir.resize_uint8(stb_image.data(img), stb_image.width(img), stb_image.height(img), stb_image.channels(img), 128, 128)
stbir.release(small)
stb_image.release(img)
```
