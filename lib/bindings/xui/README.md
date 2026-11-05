# XUI 3 declarations

`xui` uses native XUI controls through direct C declarations. It shares the
matching `_xge_native` SDK, the engine DLL/SO and the language's XRT. Package
version `3.0.0`, ABI `2`, does not change the native `XUI_VERSION_*` constants.

The default Document model has seven typed resource classes: `Document`,
`DocumentSnapshot`, `DocumentChangeSet`, `DocumentPrepare`, `DocumentFragment`,
`DocumentSourceIndex` and unique `DocumentTransaction`. Use `createDocument()`
or the explicit Owned import factories. Snapshot/index/fragment/prepare
`share()` acquires a real independent native owner. Notification arguments
are callback epoch views; call `share()` to save them beyond the callback.
Transaction `commit()` and `abort()` close the writer but keep its shell;
`close()` releases the unique native owner. The exact API and remaining C
interoperability boundaries are in
[`Document model map`](../../../docs/xge3-xui3-document-api-map.md).

Node and change-info queries project direct POD fields from one size-prefixed
native output struct. Borrowed pointers stay private. Text can be obtained
through scoped `Buffer` Copy/Read methods; Buffer lengths stay unchanged.
`setAttributesColumns` replaces the whole attributes value, with its fixed
font-family bytes reset to empty; `setTextStyle` accepts an exact POD for a
font family. Raw allocator hooks and libc-allocated `char**` results remain
explicit C interoperability. These declarations require the matching Model
engine, leased ownership-domain runtime and sized-record compiler contract.

`Context` owns its widget graph. Control classes extend `Widget`; a method such
as `button.setText(text)` calls its original native function. The package has
no C wrappers for individual methods and no opcode dispatch. Constructors
ending in `Native` expose caller managed descriptors. A default `null`
descriptor is available only after checking native validation. Such an API
does not imply safe ownership of nested font, surface or callback pointers.

`Widget.close()` calls the checked native widget
destructor. A protected internal child refuses destruction without invalidating
its live view. Ordinary interaction actions are named `closeMenu`, `closePopup`
or `closeBrowser`; they do not change the inherited `close()` lifetime meaning.

Persistent callbacks use native `Owned` registrations. `Event`, draw data and
other callback views expire when that callback returns. Saving a view is
allowed, but later access is checked and fails after expiry. Copy the required
text, bytes or value while the callback is active. `ByteSlice.size` reads its
explicit length; `ByteSlice.bytes` creates an independent bytes value.

`TableCellOutput` owns native cached cell data. `setText`/`setTooltip` accept
length-aware strings and reject embedded NUL according to the native UI text
contract; `setValue` follows native value ownership. Text getters
copy borrowed native text into a language value. `value()` retains an actual
language value before the native input scope ends. Underscore-prefixed native
fields stay private. A provider can `output.use(cachedCell)` without cloning
cached text; mutation while the native consumer holds a read pin is rejected.

The ordinary table provider gets a Context-owned `Widget`. Cell formatter and
renderer callbacks instead receive `CallbackWidget`, a callback epoch view
protected by the native consumer. It does not silently acquire a different
Context ownership root.

`TableGrid` uses owned adapter, validator, editor and editor-config callbacks.
The config callback receives a mutable epoch view; its editor receives a const
view. Private owning fields remain hidden. Native setters own copied enum text,
palette colors and text settings. `enumItem()` returns an owned string and the
four scalar attributes through one native call, with all results admitted
together. A failed copy cannot deliver a partial tuple. `setPalette(Buffer)`
accepts aligned native `uint32_t` elements; the compiler validates byte count
and alignment, and native code copies them before returning.

`createProxy()` creates the shared native XGE provider. Context construction
owns that immutable binding, and default/registered/control Font slots retain their
actual native references. Proxy, Font, Surface and Cell share the exact native
ownership domain. Resource getters acquire native references before the input
scope ends. This source requires the matching Native48 engine and language
runtime; earlier SDKs are not interchangeable.

Safe Context construction is `create(proxy)` and binds that owned provider
transactionally through native `xuiCreateOwned`. The no-backend `xuiCreate`
entry remains C interop only. This newer constructor requires its matching
native SDK; older Draw-only ABI reports do not validate the new entry.
`context.proxy()` returns an independent owning reference to that provider.

Proxy methods call actual native function-pointer fields. RGBA sources accept
bytes or scoped Buffer storage; readback requires enough initialized Buffer
bytes and preserves its length. Readback queries the native descriptor once
before admitting writable storage. Quad drawing accepts four exact native
vertices. Native allocation, decoding and batching costs still apply.

Text measuring/drawing and path drawing declare their native descriptor packs.
Language arguments fill a stack `xui_text_item_t` or `xui_path_style_t`, then
call the original Proxy field. Font inputs stay pinned; text/context keep exact
UTF-8 byte lengths, and nullable context/language preserve NULL versus empty.
Dash and paint-span bytes use checked readonly element spans. Optional named
arguments expose shaping, range, language, stroke and fill controls. The
`shape` parameter accepts an immutable owned `TextShape`; its actual native record stays
pinned through the call. `proxy.shapeText(font,text)` creates that owner, scalar
properties read the exact const record, and range measuring/drawing calls the
native Proxy fields directly. `cluster(index)` and `caret(index)` each make
one native query and return a record value; out-of-range indices are checked.
Paint hooks and writable cluster/caret pointers remain private. This newer source requires the
matching owned-Shape and pre-reserved factory runtime; earlier Draw-only
reports do not validate it.

`clusters()` and `carets()` return `NativeSequence<TextCluster>` and
`NativeSequence<TextCaret>`. Their native arrays are shared without copying
elements. Each result acquires an independent native Shape reference and a
resident owning carrier with code residency; closing the original Shape does
not invalidate it. Creation allocates that carrier and its language handle.
The sequence exposes `length`, `count()`, checked indexing, `slice(start,
count=-1)` and one- or two-variable `for`. A slice creates a small range handle
and retains the backing owner, without copying its elements. Indexing copies
one POD record; editing that copy leaves the sequence unchanged. Typed loops
hold one traversal scope and read the actual C array without per-element
boxing, allocation or native indexed-query calls. Individual indexed reads
have a checked read scope. There is no public mutation, raw pointer or close
operation on the sequence. Its owning references are released automatically.
These declarations require the matching NativeSequence compiler/runtime;
the prior indexed-query acceptance does not validate this new result carrier.
See the [NativeSequence projection map and costs](../../../../docs/xge3-xui3-native-sequence-api-map.json).

Owned `shapeText` defaults to native `DEFAULT | RETAIN_PAINT` (11), enabling
kerning/emoji shaping and retaining the paint needed for cached drawing.
Explicit `shapingFlags=0` requests lightweight measurement data; it does not
promise a drawable paint payload. These defaults follow the native producer
contract. CPU-provider tests do not certify graphics-driver execution.

`beginDraw(target)` creates an owned `DrawContext`. `finish()` requests native
End, completed by the last active native call scope; `close()` releases its
owner. Geometry methods accept either an owned DrawContext or the callback's
`CallbackDrawContext`, retaining each type's distinct lifetime checks. The
package does not expose the raw consuming provider `drawEnd` as a normal
method. The final matching runtime gate is separate from declaration/ABI
coverage; no GPU or desktop execution is used for acceptance.

Widget Surface storage, retained text/layout data and borrowed descriptor
projections remain explicitly classified in `docs/xge3-xui3-api-map.md` while
their native owning protocols are completed. Raw caller pointers do not stand
in for those protocols.

NinePatch uses `Button.setPatch/getPatch` and
`Progress.setTrackPatch/getTrackPatch/setFillPatch/getFillPatch`. Setters take
the Surface, source Rect, slice Thickness, color and mode, and fill one native
stack descriptor. Getters return `(Surface?, Rect, Thickness, c_uint, c_int)`
from one native call; the returned Surface owns an independent native reference.
They do not expose a mutable descriptor containing an untracked Surface pointer.
Use `hasPatch(state)` before `Button.getPatch(state)` when absence is expected:
the native Button getter reports an error for an unset patch. The two Progress
getters return a NULL Surface with zero fields when unset. Clear methods retain
their native behavior. These declarations require the borrowed-output-record
compiler contract and its matching runtime/SDK acceptance; prior ABI reports
do not validate these newer methods.

Typed bulk inputs use `Tabs.setItems(titles, size=-1)` and
`Tabs.setIcons(icons, sources, size=-1)`. Strings are NUL-checked; icon resources
are pinned through the native transaction and its cleanup. Icon source Rects
are explicit. `Toolbar.setItemsColumns(texts, tooltips, types, states, values,
groups, icons, iconSources, size=-1)` and
`MsgBox.setCustomButtonsColumns(texts, results, semantics, size=-1)` expose
explicit columnar descriptor input. Default size requires equal column lengths;
an explicit nonnegative size uses at most the common prefix. Their O(n)
temporary records and resource scopes are intentional, with no per-method C
wrapper. See [projection contracts and costs](../../../../docs/foreign-array-projections.md).
Ordinary mutable object arrays are not treated as native descriptor storage.
