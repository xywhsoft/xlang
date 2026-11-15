# Page navigation text snapshots

`Page.getText()` directly calls `xuiPageGetText` once and returns
`(first,last,previous,next)` as four independent `string` values. The generated
declaration reuses `c_outputs`, native status checking and the existing scoped
`cstring` snapshots. There is no product-specific C forwarding function or
new compiler/runtime contract.

The native Page has four embedded `char[16]` buffers. On successful lookup its
four output pointers are non-NULL, including when a label is empty; the public
results are therefore nonnullable. Initial text is `First`, `Last`, `<`, `>`.
Each label holds at most 15 UTF-8 bytes plus the terminator. The native helper
keeps complete grapheme clusters when truncating. Reading text preserves that
rule, including combining accents and regional-indicator flags.

```xl
page.setText("首页", "末页", "上一页", "下一页")
auto first,last,previous,next = page.getText()
assert first=="首页" and next=="下一页"
```

The returned strings remain usable after labels change or the Page/Context
closes. Native status, wrong-thread, closed-handle and allocation errors use
the existing checked boundaries. All four C-string conversions finish before
any result is delivered; a conversion failure releases earlier snapshots.
This does not add whole-assignment rollback for later destination conversions.

`test/cases/foreign_xui_page_text.*` covers direct, explicitly typed bound and
protocol calls, empty labels, UTF-8/grapheme capacity, independent snapshots,
32 allocation-failure prefixes and Context/Widget/thread lifetime boundaries.
The separate C client checks invalid widget kinds and the same native buffers.
The Page lesson displays and verifies the labels in its one-time initializer.

The query cost test reads four results, consumes their 21 total byte lengths
and releases them. Content assertions run outside that counter window. An
equivalent C baseline computes lengths and creates the same four owning
copies. Native buffer reads are borrowed, so language snapshots necessarily
scan/copy those bytes. Input/code/resource scopes, status/error checks, tuple
field moves and cleanup still execute; equal heap counts do not prove CPU
equivalence or overall zero bridge overhead.

An earlier measured loop included four string-equality assertions. Its extra
8000 allocations per 1000 rounds came from protecting comparison left operands
and constructing the four string literals, rather than from tuple delivery.
Generated-C evidence and the original loop/log are preserved externally. This
general string-comparison cost is a separate compiler follow-up, and is not
silently included in the query-only allocation claim.

Full O2 compiler/native SDK cohorts, C O0/O2 host/static clients, JIT/dynamic/
static AOT, C/C++ headers, bounded lesson rendering, imports and input hashes
are qualified by `tools/check_xui_page_text.py`. Final SDK/compiler/IDE/help
release installation remains part of the complete ecosystem goal.

The full generator was replayed in an isolated copy and emitted an identical
complete Page block. Its later strict storage review stopped on the existing
`xui_proxy_t.textDraw` record: both that helper's body and the public-header
fingerprint differ from the stored review. That failure is preserved as a
separate review task; it was not bypassed or relabeled a full-generator pass.
