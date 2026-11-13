# Icon models and picker edges

The registry owns IconCategory and Icon data; the language holds typed views
which retain their Context. Dropping a language variable does not unregister
the data. Each child installs a native physical lifetime watch in the Context's
existing ownership directory. It does not invent another native graph edge.

`createIconCategory(context,name,iconCategoryDescDefault())` declares the native
factory directly. Category SVG-path and alias factories, lookups, descriptions,
counts and Icon metadata use the same direct declarations. `createIconPicker`
passes a typed null descriptor to use native defaults. The picker's category
and selected icon getters return views of the same native model objects.

A picker holds a real category reference. An alias holds a real icon reference.
`unregister()` removes only the registry owner and returns the native accepted
flag; callback failure is still checked even if removal was accepted. The data
can remain alive through those native references. Physical deletion invalidates
all language aliases before the native custom finalizer. Context close expires
all descendants. Native cross-Context checks remain authoritative.

Names use the generic borrowed CString contract: one native call, length scan,
one allocation and copy. Nullable tags/lookups preserve absence. Numeric getters
do not copy model arrays. Enumeration uses zero-based active indices; stable
native IDs can have holes after deletion.

The four new native observation/removal APIs are in XGE commit `b4409240`.
Each private native Category and Icon grows by exactly 8 bytes on Windows/Linux
x64. A thousand pairs of caller-owned watch/unwatch storage operations add zero
XRT allocations, frees or reallocations. Cold language lifetime descriptors and
two factory rollback adapters handle ownership and checked diagnostics; there
is no per-business-method C forwarding implementation. Normal view validation,
root scope and C-string argument conversion still have their existing costs.
This is not a claim that the entire binding is zero overhead.

Run `tools/check_xui_icon_models.py` with a full compiler build report, explicit
matched dynamic/static packages and native source root. Each platform's gate
has 15 commands, including JIT, independent executables, import inspection,
three public header standards for both the fixture and learning example, and
example C generation. The runtime fixture covers native alias/picker retention,
physical expiry, Context close, cross-Context rejection, independent strings
and 96 factory allocation budgets. Static programs import no XGE DLL/DSO;
neither independent program imports the compiler or TinyCC library.

Evidence is indexed by
`dev/release-materials/provenance/xui-icon-models-20261113.json`. The graphical
`lib/bindings/xui/examples/icon_picker.xl` is compiled, not executed by this
CPU-only gate. At that checkpoint, file/memory/custom sources, owned formatter/custom closures,
same-object reentry across loaders/drawing, full graphics and final matching
SDK/compiler/IDE installation remained separate work. Full XGE/XUI/Document
integration remains active.

The successor native SDK includes `7f167dd1`: `IconClear` temporarily retains
its category through finalizers which unregister that category or clear the
registry. The existing Context receipt alone did not protect the category
array. Four reentry routes, 16 diagnostic combinations and 1000 zero-heap empty
clears pass in both platforms' 24-command native gate; this document's
15-command language gate also passes against both successor SDKs. There is no
public ABI or private layout change in that fix. The native DLL/DSO each grows
by 512 bytes relative to the watch SDK.

`xui-icon-clear-reentry-20261113.json` indexes the successor. The initial
producer's broad freeze incorrectly included three generated HarfBuzz cache
outputs. Those failures are preserved. Independent receipts exclude only the
three exact compiler outputs, verify all remaining 12,063 original inputs and
all staged output hashes, and record them unchanged. This does not change the
scope of the runtime tests or turn declaration presence into full acceptance.

Native successor `7a91ec09` holds an Icon/Context receipt through intrinsic-size,
prepare and draw callbacks, and a Category receipt through preload iteration.
Callbacks may unregister their active data or request Context destruction;
physical retirement follows the complete native reader. Alias roots preserve
their immutable target chain. Original owned diagnostics survive a failed
return and final cleanup.

Both platforms pass 28 native commands (56 lifetime/error combinations and
4000 normal operations with zero XRT heap activity), four fresh O2 SDK producer
commands and the complete 15-command language consumption gate. Each platform
now has a separate source/cache tree, excluding generated build caches from
its 12,066 pinned inputs. All those inputs remain unchanged. Public/private
layouts stay the same. Relative to the clear SDK, the Windows DLL grows by
2560 bytes and the Linux DSO total is unchanged. Normal operations add Context
scope and temporary reference bookkeeping; no timing equivalence is claimed.
The raw Painter fixture explicitly holds the Context for its session.

`xui-icon-operation-lifetime-20261113.json` indexes this floor. Cache mutation
from callbacks, retained formatter/custom closures and complete graphics/UI
acceptance remain separate requirements for the full integration.

The language declarations now expose `Icon.intrinsicSize()` as a checked `Vec2`
output, `Icon.prepare(width=0,height=0)` and
`IconCategory.preload(width=0,height=0)`. Each calls its native entry directly.
With the SVG-path/alias model fixture, 1000 rounds (3000 language calls) pass
under an allocation budget of zero, with unchanged XRT allocation/free/realloc
counters. The POD output introduces no heap box on this typed path. Invalid
negative dimensions produce checked failure and leave the model usable.

Both Windows/Linux 15-command gates pass the new calls in JIT and independent
dynamic/static programs, retaining the 96 factory OOM budgets and public header
checks. Other source kinds may decode/allocate during intrinsic-size or prepare
work; this measured workload does not remove those native costs. No new native
or compiler build was needed: the calls consume the preceding qualified SDK.
See `xui-icon-queries-20261113.json`.

Native successor `91b68742` retains the Picker's selected Icon through its
complete formatter/draw reader. A formatter may unregister the selection,
clear its category, switch categories or request Widget/Context destruction
without retiring the Icon inside that callback. The final native reference
is released only after the body stops using its output. This reuses the
existing Icon/Context operation receipt and adds no object fields.

The same review found that best-effort cache preparation could discard a
negative callback status before a checked frame saw it, especially when no
diagnostic object existed. The common cache callback consumer now seals that
status before draw cleanup or pending destruction. A subsequent error-report
hook cannot replace the first status or its exact nullable diagnostic. Raw
render remains best-effort; checked native callers balance their frame
receipt, as the language receiver scope does.

Both platforms pass 36 Icon/cache commands, including 40 selection/alias/
close/error and 24 generic outcome combinations, plus the complete 16-command
CodeEdit/TextEdit read and CPU-rendering regression gate. Both full compiler
consumers pass the existing 15-command model gate in JIT and independent
dynamic/static programs. Four fresh O2 SDK producer commands per platform
and all 12,071 pinned source inputs verify unchanged. Controlled old-runtime
guards exit before a retired Icon is reused, or after a status-only cache
failure is lost; their evidence remains intact.

The prior and successor CPU Proxy workload each performs 1000 allocations and
1000 frees for 1000 warm Picker renders, with zero reallocations. This is an
unchanged native allocation budget, not an allocation-free renderer. The
Windows DLL's total bytes are unchanged; the Linux DSO grows by 4048 bytes.
At that checkpoint two reused internal helper functions became ELF-visible/PLT
calls; localized internal visibility was a separate performance follow-up. No timing
equivalence is claimed.

`xui-icon-picker-format-lifetime-20261113.json` records this floor. It does not
yet expose retained language custom-Icon/formatter closures, prove reentrant
cache freshness/mutation safety or install the final SDK/IDE.

## Retained text formatter

`IconPicker.setFormatter(IconPickerTextProc?)` now calls the native
`xuiIconPickerSetFormatterOwned` directly using the existing `owned_text`
callback declaration. There is no business-operation C wrapper, new syntax or
compiler change. The callback returns an owned XRT `xstrview` by value. Native
code holds the selected callback owner and Icon/Context reader until it has
copied the preview, frees the returned text with `xrtFree`, then drops the
callback snapshot. Failed status/diagnostics are handled before reading the
text extent; cleanup cannot overwrite the first outcome. Publication uses the
existing transactional callback directory, and replaces the legacy formatter
slot rather than maintaining two independent active callbacks.

The language arguments are callback-epoch `CallbackWidget` and `CallbackIcon`
views. `CallbackIcon` exposes copied name/displayName/tags, ID/source type and
checked unregister. A view saved beyond the invocation throws on access; it
does not acquire a registry owner by pretending to be a normal `Icon` view.
The callback can copy metadata or resolve an independently checked registry
view for longer use. The existing native preview is still at most 255 bytes,
with an incomplete UTF-8 tail trimmed and embedded NUL ending visible text.

The private Picker record remains 856 bytes before/after on both qualified
x64 platforms: a union shares the callback pointer, and a tag uses existing
alignment space. The two reused internal operation helpers now have localized
hidden ELF visibility; dynamic exports and their internal PLT indirection are
removed. This does not change optimization from O2. The matched Windows host
grows by 1024 bytes; the matched Linux host's total size is unchanged relative
to the preceding reader/outcome floor. This includes both the new API and
visibility change and does not isolate their individual machine-code sizes.

`tools/check_xui_icon_formatter.py` executes full-compiler JIT and independent
dynamic/static programs on Windows/Linux. A test-only CPU provider maps public
owned resource handles to the native raw test renderer. It uses actual native
Proxy/Font/Surface/Draw factories and ownership edges; no private resource
header offsets or fabricated managed handles are used. Every resource-sensitive
backend callback is adapted; optional retained text shaping is disabled. It
does not qualify provider module unloading or real GPU/pixel correctness.

The language tests cover nine normal/replacement/unregister/close/error/OOM/
empty/NUL scenarios, callback-epoch escape rejection, captured owner references
released by explicit Context close, and twenty registration OOM budgets that
preserve the previous closure. Native O0/O2 tests add twenty exact outcome,
reentry and extent cases, eight OOM budgets, failed/reentrant Hold, consuming
Drop errors and complete Context graph visitation. The earlier 36 Icon/cache,
16 editor and 15 language-model checks remain in the qualified successor.

The performance observation compares the same CPU provider, native revision,
owned callback contract and constant `"owned"` result over 100 warm forced
redraws. Language and handwritten C callbacks each perform 400 allocations,
400 frees and zero reallocations in all three execution modes on both targets.
The difference is zero heap operations in this workload, not zero total bridge
cost. The generated thunk still admits the callback/code owner, establishes
error and native scopes, creates two checked ephemeral argument views, invokes
the closure, ends its epoch and delivers checked failure. The returned text is
allocated/copied once in both callbacks. Reading metadata inside the callback
adds its normal independent string copy; escaping an argument can additionally
materialize epoch storage. No CPU timing equivalence is asserted.

`xui-icon-picker-owned-formatter-20261113.json` indexes the frozen sources,
matched SDKs and acceptance receipts. Custom Icon callback groups, arbitrary
cache mutation/freshness, real rendering and final SDK/IDE delivery remain
separate unfinished work in the full integration goal.

## Memory sources and cache clear

`IconCategory.addSvgMemory(name,bytes)` and `addRasterMemory(name,bytes)` map
directly to the existing native length-aware factories. The input span is
admitted for one C call; native Icon storage copies it. There is no temporary
Buffer or extra language adaptation copy. This is not a claim that image input
or decoder storage is zero-copy. `Icon.touch()` invalidates its cached image,
and `IconCategory.setDescription(IconCategoryDesc)` directly passes a typed POD
configuration. Invalid settings throw without changing the prior descriptor.
`examples/icon_memory.xl` shows two self-contained valid SVG inputs and category
sizing; it needs no separate image files. Its source/public header are compiled
in this batch; actual window/decoder rendering remains separate acceptance.

Native cache clear used to call backend destructors while the old array or
loaded-Surface slot was still attached. A destructor reentering Touch could
destroy the same Surface again; removing its carrier or closing Context could
also retire a cleanup reader. The successor detaches the old slots first and
uses existing Icon/category/Context operation receipts. Reentrantly created
replacement slots survive outer cleanup. No native public header or private
object layout changes. A category descriptor clear holds each current Icon
while calling its backend, and does not keep a pointer into a mutable registry
array across that call.

The 48-command native gate retains the preceding 40 checks and adds 60 raw
cache reentry/lifetime/error cases, 72 actual-owned-Surface outcome cases, and
2000 zero-heap empty mutations at O0/O2. Owned finalizer failure keeps its exact
first negative status and nullable diagnostic, even with an ambient error and
a later cleanup error. Controlled prior-runtime guards stop with exit 81 before
duplicate physical destruction on both platforms. No double-free/UAF is
deliberately executed.

The full-compiler cache consumer executes JIT and independent dynamic/static
programs on Windows/Linux. Real owned CPU frames check reuse, Touch reload,
descriptor invalidation, checked rejection and balanced resource teardown. The
test backend verifies complete NUL-containing input bytes and a distinct native
input copy. It deliberately does not decode those opaque test markers as real
SVG/raster formats. Each execution also checks 64 memory-factory OOM budgets
and 2000 empty typed mutation calls under a zero-allocation budget with unchanged
allocation/free/realloc counters. Receiver admission, POD passing and checked
scope bookkeeping remain; no CPU timing equivalence is asserted.

The matched floor retains all 16 editor, 15 Icon model and 14 owned formatter
checks on each platform. `xui-icon-cache-clear-20261113.json` indexes snapshots,
SDKs and receipts. This fixes finalizer-driven clear reentry; it does not yet
fix a backend Touch during an active Surface read. That independent controlled
probe still exits 83 before physical release on the successor. Reader snapshot
semantics are pending user choice. Custom Icon callbacks, cache reader/freshness
work, real graphics and final matched delivery remain unfinished.
