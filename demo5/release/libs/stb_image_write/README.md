# stb_image_write

Lightweight image writing package for xlang, backed by `stb_image_write.h`.

The package writes raw pixel pointers:

```xlang
import stb_image
import stb_image_write as stbiw

var img = stb_image.load("input.png", 4)
stbiw.write_png("copy.png", stb_image.data(img), stb_image.width(img), stb_image.height(img), stb_image.channels(img))
stb_image.release(img)
```
