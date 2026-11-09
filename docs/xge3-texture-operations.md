# Texture operations and system font factory

This batch completes the listed Texture operations through direct declarations.
It does not finish the complete XGE/XUI/Document integration or install a release.
Native authority is XGE `9feb5fca`; the decoder OOM correction is `1bdba266` and
the preceding GL state correction is `56bacf1b`.

| xlang API | Native entry / behavior |
| --- | --- |
| `createTexture(width,height,pixels?,flags=0)` | `xgeTextureCreateRGBAExOwned`; Buffer/bytes capacity, lossless default |
| `createTexture(image,flags=0)` | `xgeTextureCreateFromImageOwned`; copies decoded pixels before the Image scope ends |
| `loadTexture(bytes/Buffer,flags=1)` | `xgeTextureLoadMemoryExOwned`; exact byte length, native decode flags |
| `loadTexture(path,flags=1)` | `xgeTextureLoadExOwned`; explicit C string projection, including resource URIs |
| `createYUV420PTexture(width,height)` | `xgeTextureCreateYUV420POwned`; odd dimensions keep native chroma rounding |
| `Texture.updateRGBA(x,y,width,height,pixels,stride=0)` | `xgeTextureUpdateRGBASized`; bytes/Buffer, capacity checked before reading |
| `Texture.updateYUV420P(y,strideY,u,strideU,v,strideV)` | `xgeTextureUpdateYUV420PSized`; three bytes or three Buffer inputs, all validated before writes |
| `Texture.readPixels(output,stride=0)` | `xgeTextureReadPixelsSized`; writes into an already sized Buffer |
| `Texture.sampler` / `setSampler(value)` | Native value return / call-scoped const struct pointer |
| `Texture.storageInfo()` | Native output struct; `@c_output_size` supplies its required `iSize` |
| `textureFallbackSetRGBA(width,height,pixels?)` | `xgeTextureFallbackSetRGBASized`; successful candidate replaces the previous fallback |
| `textureFallbackGet()` | `xgeTextureFallbackGetOwned`; independent owned copy |
| `loadSystemFont(role,size,weight,slant,language?)` | `xgeFontLoadSystemOwned`; platform lookup with an engine-owned obligation |

Existing Texture drawing, UV mapping, upload queue/flush, coordinate contract,
fallback clear and shared Material/RenderTarget slots remain available.
Lifetime helpers stay private. The raw caller-storage and unbounded-pointer
forms are replaced by these checked native counterparts in the safe API.

Required row capacity is `(height-1)*stride + rowBytes`; padding after the last
row is unnecessary. Nonpositive stride preserves XGE's tight-row rule. Invalid
capacity/stride leaves pixels unchanged. Driver failures can leave partially
committed pixels; they do not provide a GPU transaction. YUV keeps the existing
full-range conversion. ReadPixels returns the CPU shadow; use RenderTarget
readback for live rendered target content.

The previous system font declaration incorrectly passed `xge_font*` to the
caller-storage function expecting `xge_font`. The actual full compiler rejected
it with its C ABI assertion. The new native owned factory has the correct output
pointer, reserves admission before allocation, rolls back on error and causes
checked engine shutdown to report Busy while retained.

Image uses its explicit equivalent C pointer spelling `xge_image_t*`, allowing
the generic `@c_const` contract to prove FromImage's const pointer input.
Fallback setters explicitly name the authoritative header, preserving DLL
import attributes and C/C++ ABI assertions. No per-method language C wrapper
was added.

Validation is pinned in
`dev/release-materials/provenance/xge-texture-operations-20261110.json`:

- Windows and Linux each passed 44 native build/run steps at O0/O2, including
  real OpenGL, exact/padded capacities, late YUV-plane rejection, native OOM and
  system font ownership. Linux graphics used Xvfb.
- The full O2 compiler/TinyCC/VFS cohort passed 50 Windows language steps:
  emission, JIT, static AOT build/run and import checks across ten cases.
  Static executables import no XGE host DLL or xlang compiler/runtime DLL.
- Three generated public headers passed C11, C++11 and C++17 with warnings as
  errors. The 200-iteration CPU render/Texture loop had zero additional heap
  allocations in JIT and static AOT; this is not a complete timing or zero-cost
  claim. Factories, views and checked error handling retain their normal costs;
  resident sampler/YUV mutation also queries and restores GL state.
- Independent decoder memory load/info checks passed O0/O2, 20 allocation
  budgets and PNG/JPEG/GIF cases; Linux used ASan/UBSan. This is not sanitizer
  qualification of the whole engine or every image format.

Creator-thread admission and checked retirement for ordinary graphics calls
and foreign-thread final release were subsequently qualified against the
existing resident mechanism; see [thread lifetime evidence](xge3-thread-lifetimes.md).
The original functional checks alone did not establish that contract.
Linux language-side JIT/AOT, the remaining feature families, performance
comparison and the matched release/IDE delivery remain in the long task.

`lib/bindings/xge/examples/texture_pixels.xl` is a short CPU-only example:
create two pixels, change one, read and assert the resulting bytes, then close.
