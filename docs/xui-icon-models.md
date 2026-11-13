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
