# Image metadata, loading and PNG file output

The `xge` source package directly declares these native image operations:

| Language API | Native entry and contract |
| --- | --- |
| imageInfo(bytes / Buffer, size=-1) | xgeImageInfoMemory; exact prefix input, `(c_int width, c_int height)` output |
| loadImage(bytes / Buffer, flags=1, size=-1) | xgeImageLoadMemoryEx; owned Image with native cleanup |
| loadImage(path, flags=1) | xgeImageLoadEx; checked C-string path and the same owned Image |
| saveImagePng(path, width, height, bytes / Buffer, stride=0, flags=2) | xgeImageSavePNGEx; capacity-checked RGBA rows |

Image also exposes readonly width, height, stride, format and flags fields.
Its existing checked close/lifetime policy remains responsible for decoded
pixels and native record storage. Native loading defaults to premultiplied
alpha; PNG saving defaults to straight RGBA. Pass the explicit native flag for
premultiplied raw input. The source example `examples/image_io.xl` writes two
pixels, reads dimensions, loads an Image, and cleans up its own file.

The new memory overloads follow the shared byte-prefix convention: omitted
size uses the whole input, an oversized nonnegative size clamps to its actual
length, and values below -1 fail before native entry. No strlen is used for
encoded bytes. A raw RGBA region instead derives its read footprint from
width/height/stride and actual backing size; final-row padding is not required.
Bad regions are rejected before opening the target file. A subsequent native
write/OOM failure can leave a partial output file: this is not an atomic-save
API. Use an explicit temporary file/replacement workflow if required.

Factories reserve the common language owner, construct the native record and
roll back failed native output. Returned Image fields read the real native
record. Metadata uses native scalar output slots and tuple projection; PNG
saving passes the existing byte storage directly through a checked read region.
There are no new per-method C forwarding bodies or operation dispatch wrappers,
and this batch introduces no compiler/runtime source changes.

## Native prerequisites and measured boundary

The separate XGE batch fixes final-row overreads, short-write/close false
success, unsafe PNG compressor allocation failure and final-allocation rollback.
It reuses the shared XRT Deflate through stb's supported hook, with search-budget
mapping rather than treating stb quality 8 as Deflate level 8. Its two small
vendor corrections and PNG size bounds are documented in
`D:/GIT/xge/docs/image-png-contract.md`.

The native JPEG info precheck avoids allocating a JPEG decoder for a buffered
non-JPEG input. Warm PNG metadata queries therefore need no XRT heap operation;
other image formats can have native allocations. This is not a zero CPU or
all-allocator claim. Language calls still validate/pin spans, check output
status and run common error scopes; Image factories allocate their existing
control/native storage. Loading allocates and decodes pixels. PNG saving
filters/compresses, allocates encoded storage, performs file I/O and, for
premultiplied input, allocates/converts tight rows. Buffer/path creation by the
caller also retains its ordinary costs.

`tools/check_xge3_image_io.py` uses a complete compiler build report plus
matching dynamic/static SDKs. It covers JIT, dynamic/static executables, import
tables, strict public headers, exact prefixes, alpha flags, output-region
rejection and allocation-prefix rollback for memory/file loading and PNG save.
The separate native gate additionally uses protected pages, injected I/O
failures, /dev/full on Linux, C/C++ clients and aggregate UBSan. Raw evidence
stays outside the repository; the small language index is
`dev/release-materials/provenance/xge-image-io-20261112.json`.

XGE prerequisite `b556034a` passes 42 native checks; the complete Windows r21
and Linux r13 compiler cohorts pass 27 language checks each (54 total). Each
language execution mode covers 96 allocation budgets for each of memory load,
file load and PNG save, with successful and failed prefixes and exact cleanup.
PNG metadata hot loops run 1000 direct tuple queries with no XRT heap operation.

The separate counter-disabled native CPU samples use the same deterministic
pixels, O2 compiler and three rounds per workload. Median new/old CPU ratios
are about 0.50/0.43/0.66 on Windows and 0.67/0.46/0.67 on Linux for icon,
gradient and noise respectively. The icon PNG grows from 95 to 96 bytes;
gradient output drops from 2984 to 856, and noise from 221715 to 167932.
Counter-enabled samples separately show fewer allocations; their timings are
not used for those CPU ratios. These samples do not qualify every image or
isolated hardware throughput, RSS or all-allocator behavior.

A matrix-only static program with no PNG calls grows by 6144 bytes on Windows
and 22240 bytes on Linux against its earlier matched SDK. The new host images
themselves shrink by 3072 and 8192 bytes. The remaining static growth has not
yet been attributed to exact sections/symbols and requires follow-up before
final delivery. This batch does not claim perfect unused-code elimination.

## Remaining image work

This is a qualified subfamily, not full image integration. Native PNG memory
encoding still needs the proposed generic owned-byte output contract; mutable
premultiplication and pixel access need their full mutation/view contract.
Neither is replaced with an unsafe pointer or a per-method copying wrapper.
Actual shared-library imports still need the general package-initializer
contract. Matched final SDK/runtime/IDE delivery remains part of the full XGE
goal; this batch does not install release files.

## Owned memory encoding source preflight

`encodeImagePng(width,height,bytes/Buffer,stride=0,flags=2)` now maps
`xgeImageEncodePNGEx` directly through the generic `@c_output_bytes` contract.
It admits the exact resident-XRT block as language bytes without copying the
encoded payload; the ordinary xvalue shell still allocates. Both platforms'
source JIT and dynamic/static executables pass memory roundtrip, Buffer parity,
copy/mutation isolation, row-bound rejection and forty allocation budgets with
balanced live allocation/byte and invalid/double-free counters. The new readable
memory example also runs in all three modes. See
`docs/foreign-owned-bytes-output.md` for the exact allocator promise and source
qualification boundaries. Full O2 compiler/VFS rebuilds and matched final
delivery remain pending; earlier reports above retain their historical status.
