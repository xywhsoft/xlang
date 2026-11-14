# Read-only Image pixel access

The native `xgeImageGetPixels` contract is read-only. Safe declarations use two
checked native entries which expose the byte extent and destination capacity:

| xlang API | Native API | Result/cost |
| --- | --- | --- |
| `image.pixels()` | xgeImageGetPixelView | Independent bytes snapshot, one allocation and payload copy |
| `image.readPixels(output)` | xgeImageCopyPixels | One direct copy into an already-sized Buffer; no temporary container |

The result contains stored pixel bytes, without alpha/color conversion. Native
RGBA8/A8 spans include inter-row padding and end at the last active row; they do
not assume final-row padding. A nonpositive stride means tight rows. Other
formats are rejected instead of guessing planar layout. Ordinary decoded Images
have valid tight RGBA backing. Raw C callers remain responsible for the actual
initialized backing capacity; the public image struct has no allocation-size
or provenance field.

Snapshots preserve NUL and all binary values and survive mutation or closing of
the original Image. Buffer copies preserve excess destination bytes and do not
resize it. A short destination raises checked code -13 before any write; an
unsupported native format raises -7. Snapshot admission OOM preserves the prior
language assignment target. Creator-thread and expired-resource checks remain
the ordinary Image policy. The primitive C query/copy itself does not allocate
or call XRT; raw C borrows do not escape through the safe snapshot API.

No per-method C bridge was added to the xlang runtime. Both declarations reuse
the existing `c_const`, Buffer span and `c_output_view("bytes")` contracts. The
snapshot is copied before the native call scope releases the Image. Copying a
borrowed address into a language-owned bytes value is real work, not a zero-copy
view. In the qualified 1000-round workload, the Buffer operation has zero XRT
alloc/free/realloc in both language and C; language snapshots have 1000
allocations and frees and copy 16,000 payload bytes. The raw C query has zero
heap operations but does not provide an owned snapshot. These are heap/copy
measurements, not equal-CPU or whole-binding zero-overhead claims. Native
layout validation, resource scopes, code/owner/thread checks and Buffer pins
retain their costs.

A zero-copy owning pixel sequence is still outstanding: the current native
Image exclusively owns its backing and has no independent pixel-storage
reference. Snapshot/copy convenience APIs do not close that lifecycle gap or
match the cost of the legacy borrowed pointer. The shared-backing/Busy versus
Image-bound invalidation rule is a separate pending ownership decision.

`examples/image_pixels.xl` encodes and decodes two in-memory pixels, obtains a
snapshot and a reusable Buffer result, closes the Image and verifies both. It
needs no engine, window, file or service. Dedicated cases cover exact bytes,
short-buffer preservation, unsupported state, snapshot OOM, mutation, thread
and expired aliases. Native C/C++ clients additionally cover padded rows with
no final padding, A8, stride defaults, geometry, NULL output and memmove overlap.

The pure native operations are a separate archive member; a C/C++ client using
them links no XRT implementation, decoder or graphics library. Host and static
SDKs are built together from frozen source with O2. C11/C++11/C++17 headers and
actual JIT/independent dynamic/static programs have separate recorded gates;
see `dev/release-materials/provenance/xge-image-pixels-20261114.json`. This batch
does not complete remaining Image/engine families or install the final matched
runtime/SDK/compiler/IDE delivery.
