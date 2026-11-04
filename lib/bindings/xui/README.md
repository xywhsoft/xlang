# XUI 3 declarations

`xui` uses native XUI controls through direct C declarations. It shares the
matching `_xge_native` SDK, the engine DLL/SO and the language's XRT. Package
version `3.0.0`, ABI `2`, does not change the native `XUI_VERSION_*` constants.

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

Font, proxy, surface and retained external arrays need the explicit native
ownership contracts listed in the repository's `docs/xge3-xui3-api-map.md`.
They are not made safe by changing their type to a caller pointer.
