# Owned Emoji packs in XGE

Create an empty pack with `xge.createEmojiPack()`. Add an SVG with
`pack.addSvg(sequence, svg)` or pass an explicit `EmojiMetrics` value.
`svg` accepts bytes or a Buffer. XGE copies the exact SVG bytes and the UTF-8
sequence. Inserting the same sequence replaces its source and metrics while
keeping its ID. The built-in Twemoji pack is read-only and can be obtained
through `xge.loadBuiltinEmojiPack()`.

`pack.find(text, size=-1)` returns `(found, match)`. A hit has `found == 1`,
an ID, metrics and a UTF-8 byte length for the longest matching prefix.
A miss has `found == 0` and an empty result. The input can be a string,
bytes or a Buffer. A nonnegative size limits the byte prefix, and `-1` uses
all available bytes. Checked failures remain separate from a miss.

`pack.share()`, `xge.getDefaultEmojiPack()` and a shaped run each retain
their own native reference. Set the global default with
`xge.setDefaultEmojiPack(pack)` and clear it with `null`. The next default
lookup lazily uses the built-in pack and preserves its initialization error if
loading fails. Engine shutdown clears both global slots even before engine
initialization. Independent CPU pack references remain valid.

Pass an explicit pack as the last argument of `xge.shapeText` with
`XGE_TEXT_SHAPE_EMOJI` enabled. The result retains any pack used for its glyphs.
Advance and size are captured when shaping. A subsequent replacement of the
same ID can change what that glyph draws. Reshape after edits to refresh layout
metrics. The [Emoji example](../lib/bindings/xge/examples/emoji_pack.xl) uses
a packaged font and draws an SVG-backed glyph.

The binding uses direct C declarations. Built-in matching reads immutable
source without a mutex. Mutable pack matching and edits share the existing
cache mutex. SVG parsing runs outside the lock, holds the exact source
revision, and publishes its cache result only if the entry still uses that
source. SVG resource providers can query or edit the pack during parsing.
Recursive loading of the same entry revision reports Busy. The warm draw path
takes the mutex for cache acquisition and release. Cache eviction or a final
reference drop on another thread queues previously drawn SVGs for their drawing
thread. Window frames, host-session frames and context retirement drain that
work. This keeps cached raster textures alive until their drawing thread can
release them. End publishing and close retained packs and shaped runs before
checked engine shutdown. Cold parsing allocates the SVG and one cache-owner
record. The custom source adds a reference counter to its existing allocation.

## Earlier native prerequisite batch

At that checkpoint the source package only exposed `emojiPackClearDefault()`
and the native text engine's implicit default shaping.

Native XGE `2574c579` fixes four prerequisites: atomic pack references, UTF-8
edit transactions that cannot leave orphan payloads, retryable SVG-cache OOM,
and cold built-in loading/error callbacks outside the global publication spin.
Actual old-code assertions, fully instrumented XRT/aggregate TSan races and a
bounded callback-deadlock control precede the fixes. Upstream XRT and native
public headers are unchanged.

`xge-emoji-native-20261112.json` pins 109 passing command checks: 52 Windows and
57 Linux. Complete O2 host/static products on both platforms, plus Linux EGL
host/static, include 876 translation-unit occurrences (875 fresh and one
dependency-checked HarfBuzz reuse). Actual C11 O0/O2 and C++11/C++17 clients
exercise the public host/static API; aggregate tests separately check private
invariants, OOM, concurrency, reentry and arithmetic. The matching SVG GPU
regressions use actual OpenGL/software EGL. They do not qualify full Emoji
rendering or replace language JIT/AOT/shared-module acceptance.

Linux TSan instruments the aggregate and canonical XRT, including allocator,
locks and atomics; other native archive objects remain uninstrumented. UBSan
instruments the aggregate only. All frozen, gate and log inputs were reverified.
The native Linux pack remains 112 requested bytes by typed DWARF. Warm native
matching/acquisition loops perform no XRT heap operations, but still execute
UTF-8/trie traversal, a short acquisition slot lock and atomic reference work.
Cold competing builders can duplicate parsing/CRC/context allocation. No
throughput, RSS, all-allocator or zero-total-cost claim is made.

The nine native APIs cover creation, built-in/default acquisition, source
insertion, matching, default publication/clear and reference ownership. The
language design must map acquisition/destruction to owned classes, preserve
exact byte-prefix lengths and native diagnostics, and provide useful match
results. It should directly declare native calls rather than add per-method
forwarding bodies. Reference-management support need not become manual user
methods.

Before that interface is completed:

- Decide frozen publication versus matching/mutation serialization. A separate
  TSan control reproduces the existing mutable default race even with valid
  strong references; the current fix does not claim that sharing is safe.
- Review cold GetDefault errors and typed not-found results.
- Finish the pending engine pre-init/global cleanup, queued GPU references and
  module/code residency rules. Current cold Unit skips the built-in singleton;
  the public native tests use manual Init/Unit without starting a backend.
- Finish the generic package-initializer contract required by actual xge
  DLL/DSO imports, then add language ownership/callback/OOM tests and readable
  examples with matched SDK/compiler/runtime products.

No new Emoji language declarations, native image installation, release/IDE
replacement or publication are performed by this prerequisite batch.
