# Immutable shaped text and glyph bitmaps

The text resource declarations complement FontFace, FontFamily and Font. A
`GlyphRun` owns an immutable native shaping result and the native Font references
used by its glyphs. Closing the input Font cannot invalidate that result.
`GlyphBitmap` owns a raster result independently of its source Font.

| Native capability | xlang declaration |
| --- | --- |
| TextShapeOwned | `shapeText(font, text, flags, context, contextOffset, script, language, emojiPresentation, emojiLinePolicy, emojiScale)` |
| GlyphRunMeasure / HitTest | `run.measure()` / `run.hitTest(x,y)` |
| GlyphRunGetGlyphs / GetCarets | `run.glyphs()` / `run.carets()` |
| GlyphRunGetFont | `run.font(index)` returns an independently owned nullable Font |
| GlyphRunDraw / DrawSpans / DrawDecorated | direct `run.draw`, `drawSpans`, `drawDecorated` calls |
| GlyphRunAppendShapeEx | `run.appendOutline(shape,x,y)` |
| GlyphRunRetainedBytes | `run.retainedBytes()` |
| FontGlyphRasterizeOwned / ByIndexOwned | `font.rasterize(codepoint)` / `font.rasterizeByIndex(index)` |
| GlyphBitmapResourceGetInfo / GetPixels | `bitmap.info()` / `bitmap.pixels()` |
| Native release | explicit checked `close()` and ordinary owning-view cleanup |

The shape descriptor is packed on the stack by the generic declaration
contract. Text/context use the language string's actual byte length, including
embedded NUL. Context offsets, clusters and caret offsets are UTF-8 byte offsets.
An omitted language is null; an explicit language must be a valid native BCP-47
tag, so an empty tag is rejected. Native shaping/backend flags keep their native
meaning and unsupported configurations remain checked failures. An explicit
EmojiPack owner is a remaining capability to integrate with the emoji/SVG group;
it is not represented by an unowned pointer argument here.

`GlyphPosition` uses the general [read-only record projection](foreign-native-record-projection.md).
Its declared numeric fields are available to the language; native `pFont` stays
private. Glyph/caret/pixel sequences each retain an independent native reference.
They survive closing the originating Run or Bitmap. A copied element is an
independent value. The backing array remains immutable and a traversal may not
suspend. Normal creator-thread admission applies to these resources.

Ordinary operations call their original native symbols. Resource creation has
native cold ownership support; policies do not forward business methods.
Sequence traversal copies no backing array and allocates no heap memory per
element. A projected value does initialize and load its declared scalar fields;
TCC can emit a memset call for that initialization. Typed spans/decorations use
the existing checked array projection and its O(n) temporary conversion cost.
Checked admission, code/lifetime retention and error boundaries remain costs.
There is no blanket zero-overhead or hardware throughput claim.

The native ownership backend adds 8 bytes to its private Run backend on x64.
An owned Run adds a 72-byte outer record allocation (an empty Run also needs its
backend); a Bitmap adds a 48-byte outer record allocation. Those are native
storage costs, not measurements of the complete language view's cold cost.
Queued GPU drawing retains the native font/atlas resources through flush even
after public Font/Run references are closed.

The same generated 20-pixel font and inputs were also measured after warmup,
using XRT requested live allocations/bytes. Windows and Linux JIT/static AOT
produce the same deltas:

| Operation | Process-resident C | Language |
| --- | --- | --- |
| Shape `ffi` into an owned Run | 5 allocations / 416 bytes | 7 / 944 |
| Rasterize `A` into an owned Bitmap | 2 / 191 | 4 / 719 |
| Get glyph/pixel span and retain its owner | 0 / 0 | 5 / 736 |

The resource view difference is 2 allocations / 528 requested bytes. The C
baseline uses the same native algorithms and independent native references; it
does not provide a JIT module code owner or language admission/error semantics.
These are retained construction deltas after warmup, not first-use totals or
allocator capacity/system-heap/hardware-throughput measurements. Acquire a
sequence once outside repeated access; traversing its elements allocates zero
heap memory. Keep the field initialization and checked/code-lifetime costs
described above separate from that zero-allocation result.

Native source changes are committed as XGE `2f8d7084`. The dedicated native
cohort has 376 passing checks across Windows/default Linux/software EGL, covering
text ownership/OOM, Font regressions, renderer context and queued Sokol resources.
The old cohort fails the newline Font-reference control. Windows complete O2
compiler r16 and Linux complete O2 compiler r8 pass 50 language/header/link
checks across default Windows/default Linux/software EGL: real JIT, standalone
static AOT, PE import inspection, strict C11/C++11/C++17 public headers and actual
C++ policy linkage. The EGL fixture reads 696 painted pixels after public
Run/Font close; all three cohorts cover embedded NUL, literal/default empty
strings, context/language validation, indexed rasterization and retained spans.
The general projection foundation separately passes 82 checks and 24 IR damage
mutations. Exact results/costs are indexed in
`dev/release-materials/provenance/xge-text-resources-20261111.json`. Formal
release/IDE installation, module-global code residency and the full remaining
XGE/XUI/Document scope remain open.

The readable example is `lib/bindings/xge/examples/shaped_text.xl`; its generated
font fixture needs no operating-system font, window or audio device. Raw logs,
builds and candidates stay under `F:/xlang-validation/xge-full-20261110`.
