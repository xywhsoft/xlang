# stb_truetype

Lightweight TrueType font package for xlang, backed by `stb_truetype.h`.

```xlang
import stb_truetype as stbtt

var font = stbtt.load("C:\\Windows\\Fonts\\arial.ttf")
var glyph = stbtt.find_glyph_index(font, 65)
var bitmap = stbtt.codepoint_bitmap(font, 65, 32.0)
print(stbtt.bitmap_width(bitmap))
print(stbtt.bitmap_height(bitmap))
stbtt.bitmap_release(bitmap)
stbtt.release(font)
```

Font handles own the loaded font bytes. Bitmap handles own a single-channel
8-bit glyph bitmap returned by `stb_truetype`.
