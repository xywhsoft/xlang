# Selection and theme models

`createCodeSelection()` returns a unique `CodeSelectionModel` and
`createCodeTheme()` a unique `CodeTheme`. These native models contain only
numeric records and their own optional arrays. A selection does not retain
the document passed to an operation. A theme does not own a Font, Surface,
language value or callback. They use the existing pure-data unique resource
policy, with no reference-count fiction, native watch, no-op scope or extra
ownership edge. The generic language root enforces creator-thread calls,
active-call lifetime and alias invalidation on close.

| Model | Direct native operations |
| --- | --- |
| CodeSelectionModel | clear, setRange, setState, getRange, getState, hasSelection, getCount, getAt, add, selectAll, selectWord, selectLine, gotoOffset, gotoLineColumn, move |
| CodeTheme | setDefault, setStyle, getStyle, mapTokenKind, getTokenStyle, copyFrom |
| CodeEdit | setTheme copies the supplied native theme |

Offsets are UTF-8 byte positions. Native movement and clamping follow grapheme
boundaries, including combining marks; range direction and inactive extra
selections retain their native flags. `getRange` returns two checked outputs
from one native call. `getState`/`getAt` return independent `CodeSelection`
records. There is always a primary selection, so clear leaves count one.
Unsupported movement and invalid indexes raise checked errors without replacing
an existing output destination.

`getStyle` and `getTokenStyle` return pointer-free `CodeStyle` values with their
native `iSize`. Start from one of these records, change colors/flags, then pass
it to `setStyle`; undersized manually constructed records remain invalid.
Custom styles and token kinds use the native `*_CUSTOM_BASE` ranges. Missing
custom styles are checked errors; an unmapped custom token falls back to the
native default style. `copyFrom` prepares both arrays before replacing its
destination; native failure retains the old theme. It does not shallow-copy
array pointers. `CodeEdit.setTheme` uses that same copy and invalidates the
editor's style/render cache. Closing the source afterward is safe.
An error in later widget invalidation does not imply rollback of an already
completed native theme copy.

The language declarations make one native call per operation. Native getters
fill stack records/tuple slots and setters take their address; no dynamic
boxing, per-method C wrapper or secondary model implementation is added. Only
the two normal destructor-policy adapters translate the native void release
to the existing unique-root lifecycle protocol. They do not claim a checked
native destructor API. Custom array growth and theme copies retain their
original allocation/copy costs; factory/root admission and generic scopes are
additional cold/bookkeeping costs. Borrowed internal editor model getters need
their own safe owner-view contract; these declarations create no second owner
for those internal pointers.

The console lesson is [code_models.xl](../lib/bindings/xui/examples/code_models.xl).
It has no window, file I/O, network, explicit library loader or testing scaffold.
Separate fixture/provider sources exercise numeric results, graphemes, custom
style mapping/copy, editor copy independence, OOM, thread and close boundaries.

Windows and Linux full compilers pass 48 commands across JIT, independent
dynamic/static AOT, C11/C++11/C++17 public headers and the console lesson. The
native O2 engine/archive bytes are reused exactly from the qualified
CodeDocument SDK; no native implementation or compiler change is needed for
these declarations. 1000 warmed iterations make the same five model calls in
language and handwritten C, with zero allocation, free or reallocation on both
platforms and all three execution modes. This excludes factory/array growth
and does not establish CPU or cold-path equivalence. Generic view/error scopes,
document argument pins and stack record/value assignments remain real costs;
POD results are not a zero-copy reference to mutable native storage.

The selected unused Slider comparison uses the same compiler/static archive
and pre-batch declarations: code-section size changes by zero on both
platforms, file size by +0 on Windows and -32 bytes on Linux. ELF section
payload and header sizes are equal; the Linux difference is file padding from
section reordering, not an optimization guarantee. There are 12 build/check
commands. Final packages differ from the qualified ones only in README and
catalogue data; a final completed-report hash and whole-package comparison
verify code reuse. See `dev/release-materials/provenance/xui-code-models-20261114.json`.
