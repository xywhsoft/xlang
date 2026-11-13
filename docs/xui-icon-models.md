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
CPU-only gate. File/memory/custom sources, owned formatter/custom closures,
same-object reentry across loaders/drawing, full graphics and final matching
SDK/compiler/IDE installation remain separate work. Full XGE/XUI/Document
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
Two reused internal helper functions currently become ELF-visible/PLT calls;
localized internal visibility is a separate performance follow-up. No timing
equivalence is claimed.

`xui-icon-picker-format-lifetime-20261113.json` records this floor. It does not
yet expose retained language custom-Icon/formatter closures, prove reentrant
cache freshness/mutation safety or install the final SDK/IDE.
