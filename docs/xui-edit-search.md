# Direct editor search and replacement

`CodeEdit.getFindResult(index)` and `CodeFindScope.getResult(index)` return
`(start, end, line, column, preview)`. Start and end are UTF-8 byte offsets.
Line and column are zero-based, with the column counted in Unicode code points.
The preview is an independent string copied from the native inline buffer.
The native preview removes leading spaces and tabs, replaces remaining tabs
with spaces and ends at a complete UTF-8 code point when truncated. Invalid
indices report a checked error. To identify or navigate to a scoped match,
call `resultEditorName(index)` or `activateResult(index)` on the same scope.
The example [shared_find.xl](../lib/bindings/xui/examples/shared_find.xl) shows
how to read and display results across documents.

Use `getResultEditor(context, index)` or `getEditor(context, index)` to open a
managed view of an existing editor. `getActiveEditor(context)` and
`getFindWindow(context)` use the same contract. A missing result returns `null`.
Passing another live Context for a present widget raises a checked error.
The view belongs to the Context that owns the widget. Closing the scope
destroys its find window, while its editor views remain valid until their
editor or Context is closed.

The first managed lookup can allocate a widget view. Failure leaves the native
widget in its Context and allows retry. In 1,000 prepared rounds, each of the
four getters reused its existing view without XRT heap operations. The language
path still checks status, enters the owning Context and retains the view.

Each accessor makes one native call. On the prepared two-document fixture,
1,000 local and 1,000 scoped result reads allocate and free 2,000 preview
strings in xlang. The C comparison reads inline previews with no XRT heap
operations. The xlang path also scans the bounded preview, copies its bytes,
and enters checked native scopes.

`CodeFindScope.setActivate(callback)` observes activation from both scoped
navigation and `activateResult`. The callback returns `XUI_OK` on success.
Use `setActivate(null)` to remove it. The scope, editor and match arguments
are callback-time views. Save the returned preview string or offset values
when they are needed later. A replacement applies to subsequent activations,
and a callback can request scope closure while reading the current match.
An exception reaches the checked activation or Context dispatch call that
invoked the callback. Register and invoke it on the thread that created
the scope.

The native activation keeps one result record on the stack while it updates
focus. The callback has an owned registration reference and one temporary
dispatch reference. Preview access scans within the fixed UTF-8 buffer and
copies the text into a language string. Reading the offsets and line number
does not copy the preview.

CodeEdit exposes findAll, findNext, findPrevious, replaceCurrent, replaceAll,
replaceAllPlain and replaceAllRegex. TextEdit exposes findNext, findPrevious,
replaceCurrent and replaceAll. Each declaration calls the original native
function once. The existing general c_pack contract builds one options record
on the stack, including its size and temporary C-string inputs; there is no
search engine, operation dispatcher or widget-specific forwarding function in
the language bridge.

The options methods take query, replacement for replacement methods, flags, start,
rangeStart, rangeEnd and maxResults. Defaults are flags=0, start=-1,
rangeStart=0, rangeEnd=-1, maxResults=0. Positions are UTF-8 byte offsets.
The selection flag delegates range capture to the native widget. An explicit
rangeEnd greater than rangeStart limits the range. Native implementations
determine which options apply; in particular current CodeEdit/TextEdit methods
do not apply maxResults as a truncation limit. CodeFindScope.findAll has its
existing separate multi-editor behavior. C-string queries/replacements reject
embedded NUL rather than searching a truncated prefix.

findAll, replaceAll, replaceAllPlain and replaceAllRegex return a count on
native success. Zero is a successful no-match result. Failure throws before
publishing the count, preserving an assignment's previous destination. This
does not promise native document rollback after every mutation error.

findNext, findPrevious and replaceCurrent return the original c_int status.
XUI_OK means success, XUI_ERROR_UNSUPPORTED can mean no match, no suitable
current selection or a read-only replacement. Other native errors remain
statuses, including invalid regex/arguments; callers must check the return.
This preserves normal misses without conflating them with exceptions.
Expired receiver use still throws at the managed lifetime boundary. Native
error text is available through CodeEdit.getLastError where that native route
sets it; the binding does not invent diagnostics for every status.

These calls still incur checked receiver and error scopes, and C-string
validation/termination preparation. The options record contains borrowed
pointers valid only for that call. Native search may copy its persistent find
state, allocate match/highlight buffers or regex state, and replacement may
allocate and edit document/undo storage. Heap comparison for a warmed plain
no-match search does not establish CPU equivalence or cold/regex costs.
Re-evaluating a string literal on each iteration currently creates and frees
one language string per iteration. The gate records that path separately from
reusing one prepared language query; this pre-existing string-expression cost
must not be hidden by the prepared-input comparison.

The CPU test uses real Context-owned CodeEdit/TextEdit models. It covers all
eleven new declarations, plain/regex/capture replacement, UTF-8 positions,
bounded ranges, normal misses, read-only rejection, embedded-NUL rejection,
checked native allocation failure, output assignment stability and receiver
expiry. A separate native function checks the actual CodeEdit selection and
compares 1000 warmed no-match searches against handwritten C. The example is
in lib/bindings/xui/examples/edit_search.xl. No graphics, external service or
network-security test is required by this gate.

The qualified 1000-query runs on Windows/Linux make 2000 allocations and frees
in both the prepared language-input and C paths, with no reallocation. The
literal-expression path makes 3000 allocations and frees. JIT and independent
dynamic/static programs record all three paths. Native search allocation is
therefore retained even after warming, rather than described as allocation-free.
The matching receipt is xui-edit-search-20261114.json; final SDK/IDE installation
and real graphics acceptance remain separate from this model-only gate.
