# CodeEdit byte-offset geometry and native alias

The existing `CodeEdit.getOffsetRect(byteOffset)` directly calls
`xuiCodeEditGetOffsetRect` and returns a copied `Rect` through the checked native
output contract. It reports caret geometry in world coordinates. Input offsets
are document UTF-8 byte positions, including the end position; native invalid
positions fail without replacing a previous language result. Wrapping and
scrolling retain the native editor's geometry rules.

The public C name `xuiCodeEditGetTextOffsetRect` is an exact forwarding alias:

```c
return xuiCodeEditGetOffsetRect(pWidget, iByteOffset, pRect);
```

It therefore maps to the same safe language method. Adding another method is
unnecessary. `tools/check_xui_offset_queries.py` verifies the complete native
alias signature/body before accepting this replacement and pins its source
hash. The pure declaration inventory continues to list the second C name as
undeclared; that inventory alone is not a semantic capability checklist.

For an existing editor, read a value directly:

```xl
auto caret=editor.getOffsetRect(0)
println("Caret: ",caret.fX,", ",caret.fY)
```

The focused fixture compares the language result, direct native result and
native alias for every byte position in a short multiline UTF-8 document,
before and after wrapping. It tests negative/past-end positions, complete old
result preservation, creator-thread refusal and closed-view refusal. It
explicitly closes the editor, Context, Font and Proxy, then verifies the CPU
backend's owner reference and font/shaping cleanup counters.

The backend is the existing test-only owned Font/shaping provider. Its
deterministic metrics certify the result projection and alias behavior, not
system-font pixel accuracy or actual graphics rendering. The old raw fake-font
loader is incompatible with the shared owning resource contract and is not
used in the qualified fixture. A diagnostic with that old provider also exposed
the CLI's infinite failed-retirement wait; its logs and debugger stack remain
separate, with the failure-exit policy still pending. Successful geometry
qualification does not mark that issue fixed.

For 1000 prepared rounds with two queries, both language and handwritten C
perform zero XRT allocation/free/reallocation. The language loop also works
with a zero allocation budget. Each output uses a 16-byte temporary and may
copy that value when committing success. Thread/status checks, Widget/Context
scopes, code activity and native line/cache/geometry work remain. Cold layout
or shaping may allocate; equal warm heap counts do not prove CPU equivalence.

The gate uses complete O2 compiler cohorts, pinned matching SDKs and separately
staged host/static packages. It checks C11/C++11/C++17 public headers, actual
JIT/dynamic/static execution and independent executable imports. No native or
compiler implementation changed, no redundant language API was introduced,
and final ecosystem/IDE delivery remains separate.
