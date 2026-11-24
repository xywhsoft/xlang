# XUI 3 declarations

`xui` uses native XUI controls through direct C declarations. It shares the
matching `_xge_native` SDK, the engine DLL/SO and the language's XRT. Package
version `3.0.0`, ABI `2`, does not change the native `XUI_VERSION_*` constants.

Start learning with [the examples guide](examples/README.md) and
[button.xl](examples/button.xl). Lessons use top-level initialization callbacks;
the shared window loop is ordinary source in `examples/_window.xl`.

`createCodeDocument()` creates an independent text model without a window.
Its counted `getText()` preserves NUL, history methods use native byte offsets,
and `setChangeCallback()` owns the selected closure through callback reentry.
The callback document view expires on return; copy its text for later use.
See [CodeDocument contracts](../../../docs/xui-code-document.md) and
[the console lesson](examples/code_document.xl).

`prepareCodeDocumentBatch()` prepares multiple text edits for one checked
commit with one undo step per edited document. `batch.wasCommitted()` reports
whether an observer error followed publication. See the
[batch editing lesson](examples/code_document_batch.xl).
For larger edits, capture the batch on the document thread and stage its
detached description on a worker. The document thread accepts the staged
payload and commits it. See the [worker example](examples/code_document_batch_worker.xl).

`captureCodeLayout(document, viewport)` captures the visible code rows and
the content geometry. `codeLayoutHitTest()` resolves screen coordinates to a
document position, and `codeLayoutCaretRect()` resolves a position to a
rectangle. The folded variants accept typed fold columns. The results work
with an independent document or a document owned by CodeEdit. See
[the code layout lesson](examples/code_layout.xl).

`createCodeSelection()` and `createCodeTheme()` own independent numeric models.
Selection movement uses native grapheme boundaries and UTF-8 byte positions;
theme styles are typed records. `CodeEdit.setTheme()` copies the native theme,
so its source can be closed afterward. These models retain no Document or
language closure. See [model contracts](../../../docs/xui-code-models.md) and
[the small console lesson](examples/code_models.xl).

`Context.createLanguage()` creates a Context-owned `Language`; `languagePack()`
returns a watched child or null. Own-slot `Language.text()` preserves unset vs
empty text; `Context.translate()` applies native fallback. `setLanguageText()`
copies a translation, while `clearLanguageText()` directly passes typed NULL
to remove that override. Pack metadata/text results are independent string
snapshots. Closing Context expires every pack alias; dropping a pack value does
not unregister it. See `docs/xui-localization.md` for costs and qualification.

Geometry queries such as `slider.getMetrics()`, `scrollView.getOffset()` and
`chart.pixelToData(x,y)` directly project native scalar/record outputs into
ordinary values. Native failure publishes no partial result. See
`docs/xui-geometry-queries.md` and `examples/geometry_queries.xl`.
`Chart.getViewRange()`, `getBrushRange()`, `getAnimation()`,
`getSeriesAreaFill(series)`, `getSeriesValueRadius(series)` and
`getSeriesValueColor(series)` return the native 0/1 flag followed by the
queried values. Negative native errors reject the entire result. Disabled
options can retain their configured values; inspect the flag separately.
See `docs/xui-chart-queries.md` and `examples/chart.xl`.

`Cascader.setSelectedPath(values)` selects a path by item value at each level.
The `parents` column in `setItems()` uses item indices. An empty path clears the
selection. `selectedPath()` returns the depth and an independent fixed-size
array of values. Read only the entries before the depth. `getItem(index)`
returns independent text with the item's value, parent index and flags. See
[the Cascader example](examples/cascader.xl).

`Context.styleSetDefault`, `styleSetNamed`, `styleSetClass` and
`styleSetStateClass` install batches of rules. `Widget.setInlineStyle`
targets one control. The columns share one count. Use a registered property ID
with a null name, or supply its name. A count of zero sends an empty rule.
The compiler projects the columns into a temporary C record array with both
size fields initialized. XUI copies any text it keeps.

`Context.styleRegisterProperty` registers a context-wide property.
`styleGetPropertyInfo`, `Widget.getInlineStyleProperty` and the resolved
property queries return independent snapshots, including nullable strings.
The resolved queries read the current resolved properties. Call
`Widget.resolveStyle()` to explicitly recompute them. See
[style_rules.xl](examples/style_rules.xl) for a per-control example.

`Context.findWidgetType`, type getters such as `Context.buttonType`, and
`Widget.widgetType` return observed `WidgetType` views. Each view holds its
Context. A successful native unregister or Context shutdown expires the
views, and access to an expired view raises a checked error. Unregister
requires the type to have no live instances or derived types.
`Context.registerWidgetType` registers a data-free custom subtype and
`createWidget(type)` uses that type's default creation data.

`styleSetType` and `styleRemoveType` manage type rules.
`styleRegisterProperty` accepts an optional type scope, which
`styleGetPropertyScope` retrieves as a watched view.
Unregister retires that type's rule and scoped registry properties.
See [widget_type_style.xl](examples/widget_type_style.xl).

`Context.registerWidgetTypeWithCallbacks` registers a custom type with
optional initialization, destruction, measurement, layout, painting,
update and cursor callbacks. The Context registry retains one group per
type. Derived instances use the group of the type that declared each
callback. Widget, draw, and native data arguments are callback-scope views.
The language registration uses a zero-size native instance descriptor.
Use `WidgetType.setUserData` and `getUserData` for data shared by a type.
Both use independent snapshots of public data. Add `XUI_WIDGET_TYPE_MANAGED_STATE`
to the type flags to give each instance an initially null `var` slot.
Derived types inherit the slot. `setInstanceState` retains the assigned value
with its identity, and `getInstanceState` returns an independent owning
reference to that value. The Context traces the slot for ownership collection.
Type destructors can read the state until they return. Writes become invalid
once destruction is requested. The slot adds one pointer to each opted-in
instance allocation. See [custom_widget.xl](examples/custom_widget.xl).

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

Text getters return independent snapshots. Reviewed C-string queries preserve
native NULL/empty behavior and copy within the receiver scope.
`CodeEdit.getText()` and `Widget.editGetText()` use checked status/extent
outputs so CodeEdit's embedded NUL is preserved and native snapshot OOM is not
reported as empty text. See [text snapshot contracts](../../../docs/xui-text-snapshots.md)
for copying costs and verification boundaries.

CodeEdit and TextEdit expose native find/replace options directly. Count
methods return zero on an ordinary miss and throw before publishing a failed
output. Navigation and replaceCurrent return the original native status, so
callers can distinguish a miss from a successful operation. Positions are
UTF-8 byte offsets; C-string queries reject embedded NUL. See
[search contracts](../../../docs/xui-edit-search.md) and the short
[model-only lesson](examples/edit_search.xl). These declarations create no
additional search engine or widget forwarding layer.

The zero-argument `radioGroup()` and `scrollFrame()` select native NULL
descriptor defaults. `webView(userDataFolder)` packs a sized native descriptor
with no raw callback/user pointers. `Cascader.setItems` projects equal-length
columns to native items, whose texts the native control copies. `DockPanel`
windows start hidden: `dockWindow` places them in a pane and returns its ID.

CodeEdit bracket matching is enabled by default; `setBracketMatching(0)`
disables it without changing other display options. Tabs expose independent
`setScrollButtonsVisible` and `setTabListButtonVisible` switches. Directional
buttons appear only when scrolling in that direction is possible; their rect
queries return empty rects otherwise. DockPanel uses
`setTabScrollButtonsVisible(1)` and `getPaneTabScrollRects(pane)` returns
`(backward, forward)` from one native call. See the CodeEdit, Tabs and DockPanel
examples for these controls. All eleven new methods are direct native
declarations with the existing resource lifetime checks.

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

`Menu.getItem(index)`, `MenuBar.getItem(index)`, `Toolbar.getItem(index)`
and `StatusBar.getItem(index)` return typed item data. Indices are zero-based.
Text fields are independent snapshots and preserve the distinction between null
and empty text. Menu and menu bar queries return optional live submenu views.
Closing the native submenu invalidates those views. A toolbar icon returned by
the query holds its own Surface reference and remains usable after the toolbar
is closed. An invalid index raises a checked error.

`NumericInput.setFormatter(formatter)` accepts a closure from `c_float` to
`string`. The control holds the closure until it is replaced, cleared with
`setFormatter(null)`, or destroyed. It formats the current value during
registration and updates the text when the value is edited or restored.
Return up to 95 bytes of UTF-8 without embedded NUL. An empty string clears
the displayed text. Recursive value or text edits on the same control return
`Busy`. The numeric parser keeps its existing input rules.
See [the numeric input example](examples/numericinput.xl).

`Toolbar.setOverflow(enabled, buttonSize, onOverflow=null)` sets the
overflow button and its optional handler. The callback receives a
`CallbackWidget` and the first index and count of hidden items. Read those
items through `Toolbar.getItem()`. The sender view is valid during the
call. The toolbar retains the handler until it is replaced, cleared, or
destroyed. See [the toolbar example](examples/toolbar.xl) for opening a menu
with the hidden commands.

## Document UI

`createDocumentView(context, document)` and
`createDocumentEditor(context, document)` create Context-owned Widget subtypes.
Their native descriptors are built on the call stack, including exact nested
size fields and typed Font inputs; legacy callback/userdata fields remain NULL.
Editor inherits the View selection, search and layout queries. MessageList
adds a copied-text `addNode`, Document attachment, independent Owned activate
and task callbacks, and retained Document retrieval.

`createDocumentRenderer(context)` creates an independent unique owner. It
retains its actual native Snapshot/Font/Proxy resources and observes Context
weakly: Context expiry rejects further business methods, while `close()` still
releases the renderer. Font resolver outputs and object measurement outputs
are callback-period views. `setFont` takes a real native reference; mutable
Vec2/float projections update the actual native output. A callback Snapshot
requires explicit `share()` to save it. Callback text has `length` and `text`;
reading `text` makes an owned copy, preserving its counted byte length.

WebProvider, HtmlInteraction and ImageRequest are distinct Context-owned
composite child views. Their declarations/headers are checked, but acceptance
does not start a browser, decode a host file, access the network or use the OS
clipboard. Image load and Poll intentionally omit the optional borrowed
registry-entry output. They do not invent a safe owning Resource handle.

Each method calls the native symbol directly. Resource scopes, checked
errors, Native allocations/copies and callback epochs have real costs; this
is not a claim of equal runtime cost to unchecked C. The API map records
lifecycle infrastructure, caller rectangle arrays and legacy void-userdata
adapters separately. `clearTableSelection`, `clearFindScope`, MessageList
`clearDocument` / `clearDocumentTableSelection`, Renderer `draw` without a
selection, and Editor `replaceAll` / `replaceAllEx` without a scope use the
Native NULL branches directly. Ordinary View selection and Renderer active
position still require real positions: their Native APIs reject NULL.

## Code providers

`editor.providers()` observes the provider set owned by that CodeEdit. Use the
editor's `set…Provider` methods to register callbacks, and the view's `clear()`
to remove them together. Use the view to request results directly, passing the
editor as the callback target. It also supports invoking and querying command
providers directly. `CodeEdit.executeCommand` and `queryCommandEnabled` run the
editor's own command dispatch, including its built-in behavior.

The view expires when the editor or its Context closes. `captureHover`,
`requestCompletion` and `requestSignature` return independent snapshots that
can outlive both. `captureHover` also works on standalone sets. It owns the
reply block until closed and `info()` returns the range, text, optional content
type and user data. See `examples/code_edit_provider_snapshot.xl`.

Use `createCodeProviderSet()` when an application wants to issue code service
requests directly. A set owns five independent provider registrations. Passing
`null` removes one registration, and `clear()` removes all five. The callbacks
run synchronously on the set's creating thread. They use the same completion,
hover, signature and command callback types as CodeEdit. Request offsets are
UTF-8 byte offsets. The `Widget` argument supplies the callback's widget view.

Use `requestCompletion(editor,offset,prefix,capacity=32)` and
`requestSignature(editor,offset)` to retain results after the set or editor
closes. Completion snapshots expose `count()` and `item(index)`. Signature
snapshots expose `info()` and `parameter(index)`. These native snapshots acquire
the reply storage directly. Reading an item copies each requested text field
into an independent language string. `requestHover(editor,offset)` returns those
string snapshots immediately. Empty strings and absent fields remain distinct.

`canExecuteCommand(editor,command)` and `executeCommand(editor,command)` return
`1` for a positive decision and `0` for a negative one. Calling a role with no
registered provider raises a checked error. The widget and reply views passed
to a callback expire when it returns. See `examples/code_provider_set.xl` for a
headless example.

## Shared native find and current engine capabilities

`createCodeFindScope()` creates an independent native search scope. Add existing
CodeEdit instances with `addEditorNamed`, select the current target with
`setActiveEditor`, and use `findAll`, `findNext` and `refresh`. The editors share
one native find/replace window; the Context still owns every editor and window.
Closing a scope detaches its editors, and native editor/Context destruction
removes entries automatically. Use `editor.setFindScope(scope)` to join or
switch scopes and `editor.setFindScope(null)` to leave. A failed switch
preserves the previous membership. `context.getCodeFindScope(editor)` returns
the existing managed scope, or `null` for an unattached editor. It checks
Context identity and rejects scopes created only through unmanaged C.
Results can be inspected and explicitly
activated. `getEditor`, `getActiveEditor`, `getResultEditor` and `getFindWindow`
take the owning `Context` and return checked optional widget views. Editor
references remain usable after scope closure while their Context is open.
Native identity getters end in `Native` and return opaque C-interop identities.
See `examples/shared_find.xl` for a minimal headless search.

`create(proxy, systemFontPolicy, size=0)` uses the sized native owned-Context
constructor. `XUI_SYSTEM_FONT_AUTO` loads the system font on demand;
`XUI_SYSTEM_FONT_EAGER` loads it during creation; `XUI_SYSTEM_FONT_DISABLED`
requires an application font. `contextDescDefault()` exposes the native defaults,
`Context.ensureDefaultFont()` checks the selected policy, and
`Proxy.loadSystemFont(size=0,flags=0)` creates a shared native font resource.
Reserved flags must be zero. Existing `create(proxy)` follows native AUTO policy.

`showMsgBox` and `showFileDialog` create and open a new managed dialog in one
native call. Reopen a retained instance with `setOpen(1)`, rather than introducing
a second owner through the raw C output-slot reuse API. `RenderStats` includes
all ten new prepare/propagation/render-node/structure counters, and custom widget
types can declare `XUI_WIDGET_TYPE_PREPARE_FRAME_DEPENDENT`.

These APIs use direct native calls, stack descriptor projection or a direct
Proxy function-pointer call. Resource creation, scope storage, text conversion
and the existing checked resource lifetime bookkeeping still have their normal
cost; no per-method forwarding C implementation is added.

`builtinAssetGetName(index)` and `vectorIconGetName(index)` return independent
`string?` snapshots of the built-in read-only name tables. Out-of-range indices
return null. Use the existing count functions to enumerate them; no Context or
window is needed. Each present name costs a length scan, one allocation and a
copy. See `examples/asset_names.xl`. Atlas decode and icon rendering keep their
existing resource contracts.

`createIconCategory(context,name,iconCategoryDescDefault())` creates a registry
owned `IconCategory`. `addSvgPath` and `addAlias` create typed `Icon` views;
`find`, `findById`, `iconAt` and `iconCategoryAt` preserve nullable lookup results.
Enumeration indices are zero based; native IDs remain stable, including holes
after removals. Names are independent string snapshots.

`createIconPicker(context)` supplies native defaults. Its typed `setCategory`,
`category`, `setSelectedId`, `selectedId` and `selectedIcon` methods call the
native functions directly. A picker retains its category, and an alias retains
its target. Dropping a language view does not unregister native data. Explicit
`unregister()` removes the registry owner; views expire at physical deletion,
which may be delayed by a picker or alias. Closing the Context expires all its
views. Cross-Context edges are rejected.

See `examples/icon_picker.xl`. This batch covers SVG-path/alias models and typed
picker edges; file sources and the remaining
same-object reentry paths are tracked separately. Lifetime watches use one
native pointer per Category/Icon (8 bytes on both qualified x64 targets), and
cold binding storage. They do not add a per-business-method C wrapper. String
reads still scan, allocate and copy; C-string inputs and checked view scopes
retain their existing costs.

`Icon.intrinsicSize()` returns a typed `Vec2` using the native checked output;
`Icon.prepare(width=0,height=0)` and `IconCategory.preload(width=0,height=0)` use
the category/native sizing defaults. Invalid sizes are checked failures. With
the SVG-path/alias fixture, 1000 rounds of these three language calls allocate,
free and reallocate no XRT memory, including a zero-allocation budget. Other
source types may decode or allocate native resources during size/prepare work;
the source-specific costs still apply.

`IconPicker.setFormatter(callback)` directly registers an owned native text
formatter; passing `null` clears it. Set the text mode to
`XUI_ICON_PICKER_TEXT_CUSTOM`. The callback receives `CallbackWidget` and
`CallbackIcon` views valid only during that invocation and returns a string.
Saved callback views throw after the callback ends. The callback closure is
part of the Context ownership graph; replacement and explicit Context close
release it safely, including replacement or close inside the callback.
The native display preview is limited to 255 UTF-8 bytes, trims an incomplete
trailing code point and stops at the first embedded NUL. This does not change
the language string's byte-preserving storage rules.

See `docs/xui-icon-models.md` and `tools/check_xui_icon_formatter.py` for the
checked error, OOM and real owned CPU-frame acceptance boundaries and measured
costs. This does not certify every custom Icon/cache mutation or real graphics.

`IconCategory.addSvgMemory(name,bytes)` and `addRasterMemory(name,bytes)` now
declare the native byte-length factories directly. Native Icon storage copies
the input; later caller-buffer lifetime is independent. `Icon.touch()` clears
cached image data, while `IconCategory.setDescription(desc)` updates the native
configuration and clears its SVG caches. These are checked calls with no
business wrappers. Empty-cache mutations need no heap allocation; image reload
and decoder costs remain native. See the readable `examples/icon_memory.xl`.

The cache-clear successor detaches old slots before running backend finalizers
and holds the Icon/category/Context until all cleanup readers finish. This
prevents duplicate destruction and preserves a reentrantly published replacement.
The full goal still includes the separate protocol for mutation while a backend
is actively reading a Surface; that controlled reader gap is not yet closed.

`IconCategory.addCustom(name,draw,measure=null,prepare=null,destroy=null)`
registers four independently retained, immutable callback slots directly with
the native owned factory. `measure` writes the callback `Vec2`; `prepare`
receives actual integer pixel dimensions. `destroy` runs once after published
registration is retired, including explicit Context close. Pre-publication
failure releases acquired closures without invoking the business destructor.
Explicit close remains necessary for objects captured in retained callbacks;
this does not certify general module/global callback-cycle retirement.

The draw callback receives a `CallbackPainter`, `Rect` and nullable
`CallbackIconDrawDesc`. Painter methods directly call native drawing functions;
`drawContext()` derives a view in the same callback epoch. Saved Painter/Icon/
size/style/Draw views are rejected after return. A callback Painter cannot end
the enclosing native drawing session. See `examples/icon_custom.xl`.

Native resource checks reject foreign-backend Font/Surface inputs before the
backend sees their payload. Native owned custom groups add one 160-byte x64
allocation plus four optional owner slots on registration, with no Icon or
Category layout growth. Hot callbacks use the existing operation pin and a
private descriptor-tag branch; they do not acquire four owners per invocation.
The qualified 1000 size/prepare rounds (2000 callbacks), both native C and xlang,
allocate/free/reallocate no XRT memory even with a zero-allocation budget. This
is an allocation result, not an equal-CPU-time claim: language checked scopes,
epoch tracking, retained callback bookkeeping and resource capability checks
still apply. Callback string reads, captures and drawing operations retain
their own costs. Manual Painter sessions, per-Icon configuration overloads and
native fixed-array literal construction are separate remaining work.

`IconCategory.addSvgFile(name,path)` and `addRasterFile(name,path)` directly
register native file sources. Names/paths are checked C strings; native storage
copies the path and loading remains lazy. Empty paths and embedded NUL are
rejected. `beginUpdate()`/`endUpdate()` coalesce nested registry changes into
one native revision update. Balance the pair with `try`/`finally`; an unmatched
end is a checked error. `clear()` removes the registry owners and expires
physically deleted Icon views. The enclosing Context scope carries cleanup
failures even when the original void C entry has no diagnostic object.

`IconPicker.ensureVisible(id)` preserves native invalid-ID rejection;
`getMetrics()` returns border width and scrollbar size from one native call.
See `examples/icon_file.xl` and its two bundled assets. The shared asset helper
accepts the lesson, runtime or source working directory. Metadata updates and
Surface/resource-source factories still need their complete safe contracts;
the current language entry does not overwrite opaque host user data.

`createTextLayout(context,font,text,...)` creates an independent, uniquely owned
`TextLayout`. `reset`, `getState`, `getLine`, `getText` and `draw` call native
entries directly. Counted strings preserve embedded NUL; `getText` returns an
independent snapshot. Context expiry rejects reads but still permits layout
cleanup. Close retains deferred native cleanup until its paint is ready.
See `examples/text_layout.xl` and `docs/xui-text-layout.md` for ownership,
measurement, byte offsets, checked failures, costs and remaining alias work.

`CodeDocument` also exposes the eight native selection editing commands:
`insertSelectedText`, `deleteBackward`, `deleteForward`, `deleteWordBackward`,
`deleteWordForward`, `indent`, `outdent`, and `toggleLineComment`. They use an
independent `CodeSelectionModel` and native undo grouping. C-string arguments
reject embedded NUL; existing counted document bytes are retained. A callback
failure can follow committed edits and does not imply rollback. See
`examples/code_editing.xl` and `docs/xui-code-editing.md` in the development tree.

`createCodeFoldState()` creates an independent numeric folding model. Use
`setRanges(array<CodeFoldRange>)`, `toggleLine`/`foldAll`/`unfoldAll`, `trackEdit`,
and checked visibility/row queries. Collapsed headers remain visible; indices
are zero-based lines/rows. Range input and returned records are independent
copies. Calls require the creating thread; close in `finally`. No window,
Context or document is required. See `examples/code_folding.xl` and the source
repository's `docs/xui-code-folding.md` for exact behavior, costs and remaining
bulk-output/internal-model alias work. Array projection costs O(n) and one
temporary allocation; warmed queries add no heap operations but retain checks.

`CodeDocument.replaceAllPlain(pattern,replacement,flags=0)` and
`replaceAllRegex(pattern,replacement,flags=0,error=null)` return the native
replacement count after checked success. Patterns/replacements are C strings;
existing document NUL bytes are preserved. Native replacement is grouped for
undo; failures can follow partial or committed edits. The optional diagnostic
Buffer is write-pinned across the native call and callbacks. See
`examples/code_replace.xl` and `docs/xui-code-replace.md`. Prepared string inputs
add no string mirror; literal construction and native regex compilation retain
their own allocations. This requires the matched counted-search SDK.

`createCodeTokenBuffer()` creates an independent creator-thread token cache.
`set(tokens,size,version)` and `setRange(tokens,size,version,start,end)`
project typed `CodeToken` records and let native storage keep its own copy.
`getVersion`, `getRange` and `getCount` return checked metadata. Tokens describe
UTF-8 byte positions, not characters. See `examples/code_tokens.xl`; close in
`finally`. Bulk token output and aliases of CodeEdit's internal cache still
require their general contracts and are not exposed by this numeric owner.

`createCodeAnnotationStore()` creates independent native marker, indicator and
diagnostic storage. Set/clear markers and intervals, publish strict diagnostic
columns with `setDiagnosticsColumns`, query count/version and `trackEdit` after
byte/line edits. The store copies text and retains no Document or language
callback. `CodeDocument.markAllPlain/markAllRegex` write search indicators and
return the checked match count. Native missing-marker/indicator clears report
checked unsupported; `clear()` clears all storage. See `examples/code_annotations.xl`.
Bulk output, string record snapshots and internal CodeEdit aliases remain pending.

`Context.inputKeyDownEx`/`inputKeyUpEx`/`inputTextEx` return native input flags;
`inputImeCompositionEx` packs the complete counted descriptor. Command hotkeys
and queued commands use owned Widget handlers, and callback `event.text` is an
independent counted snapshot. See [input contracts](../../../docs/xui-input-events.md)
and [the input lesson](examples/input_events.xl). Native queue delivery and
synchronous input-result calls keep their different timing contracts.

`Context.hotKeyRegister(target,key,modifiers,callback)` and
`Context.setErrorCallback(callback)` retain owned language closures through
native registration and active invocation. `Widget.hotKeyClearWidget()` removes
its registrations. An error callback receives callback-scoped Context,
Widget and ErrorInfo views; `info.operation` and `info.message` are independent
nullable string snapshots. Saving a view does not extend its callback epoch.
These methods require the matched owned-input SDK. See
[the callback lesson](examples/input_callbacks.xl) and
[owned input contracts](../../../docs/xui-input-owned.md).

`Context.setAccessibilityEventCallback` retains a creator-thread observer.
Its Context, Widget and numeric event views expire after each callback;
`Widget.notifyAccessibility(type,nodeId)` dispatches synchronously. See
[the notification lesson](examples/accessibility_events.xl) and
[accessibility event contracts](../../../docs/xui-accessibility-events.md).

`createCodeCommandMap()` creates an independent numeric editor key map with
native defaults, exact key/modifier binding, lookup and copied `CodeKeyBinding`
records. Missing lookup/removal remains checked UNSUPPORTED. See
[the command-map lesson](examples/code_commands.xl) and
[its contracts and costs](../../../docs/xui-code-commands.md).

`Widget.measureContent(constraint)` and `Widget.measure(constraint)` return
independent `Vec2` values through the existing checked native-output contract.
`Terminal.getCell(column,row)` returns the complete pointer-free `TerminalCell`,
including all sixteen combining codepoints. See `examples/widget_measure.xl`,
`examples/terminal_cells.xl` and [query contracts](../../../docs/xui-widget-queries.md).

`Terminal.setImageCallback(handler)` delivers Sixel and iTerm2 inline image
commands as the Terminal processes its output queue. The callback receives
the current zero-based cell, protocol, options and encoded payload. The view
expires on return. Read `image.options` or `image.payload` during the callback
to save that text. The parser accepts single and multipart iTerm2 images
with `inline=1` and bounds each image's options and payload to 4 MiB. The
application controls decoding, placement and drawing. Pass `null` to
unregister the handler. See [the terminal image example](examples/terminal_image_callback.xl).

`TagInput.setTags(tags,size=-1)` replaces a list of strings in one native call,
with native trimming, copy ownership and atomic preparation failure. The
[tag-input lesson](examples/taginput.xl) and its generated gallery scene now
use it; see [batch semantics and costs](../../../docs/xui-tag-batch.md).

`RadioGroup.addOption(text=null,font=null,textColor=0,disabledTextColor=0,
textFlags=0,checked=0,indicatorSize=0,gap=0,useBuiltinAtlas=0)` creates and
attaches a Radio in one native call. Zero descriptor fields retain native
defaults. The returned view belongs to the same Context; explicit parent
destruction invalidates it. Selection observers receive `Widget?`, with NULL
and index -1 when cleared. See `examples/radiogroup.xl` and
[descriptor, ownership and failure contracts](../../../docs/xui-radio-option.md).
This requires the matched Radio initialization/cache-fix SDK; it adds no
per-method C forwarding function.

The matched choice-cache SDK also fixes CheckBox/Toggle partial initialization
cleanup and clean-frame external image refresh for Button/CheckBox/Toggle.
Unchanged image generations reuse caches; clean-frame dependency probes remain
real native work. See [cache and failure evidence](../../../docs/xui-choice-cache.md).

The constructor-retirement SDK fixes partial initialization cleanup in
Hyperlink/Input/TagInput/TextEdit/Terminal and borrowed-string rollback in Menu.
The latter also corrects CodeEdit and composite input-constructor failures.
See [allocation-failure contracts](../../../docs/xui-constructor-retirement.md).

`Menu.setItemsColumns(texts,shortcuts,types,states,values,icons,submenus,size=-1)`
replaces complete item records in one native call. `Menu.addItem` also accepts
an optional final `submenu`. Native submenu links are weak within one Context;
closing a child disables referring items, while closing a parent leaves the
child usable. See `examples/menu_items.xl` and
[input, ownership and measured projection costs](../../../docs/xui-menu-items.md).

`createCodeMargin()` creates a standalone, watched margin model with direct
metadata, geometry and owned callback declarations. It does not adopt the model
already owned by a CodeEdit. See `examples/code_margin.xl` and
[native ownership, current qualification and remaining aliases](../../../docs/xui-code-margin.md).

`TableView.getOffset`, `getCellContentRect`, `getCellRect` and `getItemAt` expose
native geometry directly. No hit remains `(-1,-1)`. The tableview lesson displays
the clicked cell's content rectangle; see
[coordinates, provider failures and measured costs](../../../docs/xge-input-table-queries.md).

`Context.reportError(code,stage,recoverable=1,operation=null,message=null)`
publishes an application notification to the owned error observer. A second
overload adds `Widget?` and `Rect` after `recoverable`. Both directly pack the
native sized record on the stack; error text getters return independent
nullable snapshots. See `examples/error_report.xl` and
[notification, failure and copy costs](../../../docs/xui-error-report.md).

`TimeLineView.hitTest(x,y)` returns a copied `TimelineHit` in local coordinates;
an ordinary miss has type `XUI_TIMELINE_HIT_NONE` and -1 IDs/indexes.
`getOffset()` returns the actual native scroll offsets. See
`examples/timeline_queries.xl` and
[coordinates, checked outputs and costs](../../../docs/xui-timeline-queries.md).

`InventoryGrid.hitTest(worldX,worldY)` returns a numeric `InventoryHit` with
local output geometry. `getVisibleRange()` reports current visibility, distinct
from `getLastPaintRange()`. See `examples/inventory_queries.xl` and
[world/local coordinates, empty results and costs](../../../docs/xui-inventory-queries.md).

`CodeEdit.getOffsetRect(byteOffset)` also covers the native spelling alias
`xuiCodeEditGetTextOffsetRect`; it returns caret geometry in world coordinates.
See [the exact alias mapping and focused qualification](../../../docs/xui-code-offset-queries.md).

`Page.getText()` returns `(first,last,previous,next)` as four independent,
nonnullable strings. Empty labels stay empty; the native 15-byte payload and
complete-grapheme truncation remain authoritative. Text snapshots survive Page
or Context closure. See `examples/page.xl` and
[the output, lifetime and measured-cost contract](../../../docs/xui-page-text.md).
