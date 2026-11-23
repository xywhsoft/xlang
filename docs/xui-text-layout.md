# Owned XUI text layout

`xui.createTextLayout(context,font,text,...)` directly calls the native layout
factory. `TextLayout` is an independent unique owner. Its Context link is weak;
its Font and shaped-paint references are real native owners. Closing Context
rejects further layout operations, while `layout.close()` can still release
those resources. A second owning Context is never manufactured from the weak
pointer.

| Operation | Native entry / result |
| --- | --- |
| createTextLayout | xuiTextLayoutCreate, owned TextLayout |
| reset | xuiTextLayoutReset, checked status |
| getState | xuiTextLayoutGetState, (Vec2 size, c_int lines, c_int truncated) |
| getSize / getLineCount / getTruncated | xuiTextLayoutGetState, one checked output |
| getLine | xuiTextLayoutGetLine, TextLine with complete byte offset/length and metrics |
| getText | xuiTextLayoutGetTextView, independent string snapshot |
| draw | xuiTextLayoutDraw, checked status, owned Surface target |
| close | existing managed close and native RequestClose / DestroyOwnerChecked |
| measureTextLayout | xuiTextMeasureLayout, Vec2 using the Context-owned native measure cache |

The factory/reset descriptors are stack-packed by general C declarations:
Font, counted text, maximum width/height, wrap, flags and line/paragraph gaps.
Counted text retains embedded NUL and rejects lengths beyond native `int`.
Native zero width/height means unbounded; use positive dimensions for a draw
rectangle. Wrapping uses the existing `XUI_TEXT_WRAP_*` constants. Metrics and
gaps retain native float precision; byte offsets are not character indexes.
`getState()` refreshes DPI once and delivers all three outputs atomically.
Use `getSize()`, `getLineCount()`, or `getTruncated()` to read a single
value. Each emits one direct call to the same checked native query and passes
null for the unused outputs. DPI refresh failures propagate through all four
forms.

`getText()` uses the general status + `xstrview*` output contract already used
by CodeEdit. Native failure is checked before reading the output; copying
occurs before the receiver scope ends. A present empty layout yields `""`;
this required result cannot silently become null. A nonempty snapshot needs
one independent language allocation and byte copy, with no NUL scan. Native
layout creation/reset already copies and shapes the source as part of its
own algorithm. DPI refresh can allocate and call the backend. These are real
costs in addition to language scope, code lease and ownership bookkeeping.

The resource policy follows the existing unique DocumentRenderer pattern.
Managed active calls defer physical deletion. Native text bodies protect their
own rebuild/draw calls. It does not invent a reference count, a no-op native
CallBegin, or a watched-root contract requiring a nonexistent native scope.
The safe API owns the unique layout throughout; destroying its raw pointer
through explicit C interop remains outside this managed ownership contract.
The existing native lifetime-watch API is retained for native C callers.

Close first rejects subsequent calls. Native deferred cleanup retains the
exact unique owner and its partial cleanup progress; it is retried after the
backend becomes ready. Status errors are delivered as checked errors and do
not discard unconsumed resources. Finalizers only free resources; they do not
implicitly draw or recover Context.

`examples/text_layout.xl` shows one layout, line queries and drawing into an
Image's Surface without test machinery. `tools/check_xui_text_layout.py`
separately checks packed calls, exact NUL bytes, empty text, failed assignment,
factory/reset OOM, a failed factory retaining busy paint, deferred close,
thread rejection and Context expiry. It compares 1000 hot state reads with
handwritten C allocation counters, and can run the real window lesson with a
pinned font in JIT/dynamic/static modes. Zero heap operations do not prove
equal CPU time or cold-path costs.

The remaining native Context query needs to keep the weak Context valid
through lookup of its existing language owner.
