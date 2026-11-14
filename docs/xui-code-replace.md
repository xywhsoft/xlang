# Whole-document replacement

`CodeDocument.replaceAllPlain(pattern,replacement,flags=0)` maps directly to
`xuiCodeSearchReplaceAllPlain`. `replaceAllRegex(pattern,replacement,flags=0,
error=null)` maps to `xuiCodeSearchReplaceAllRegex`. Both return the replacement
count only after checked native success; no per-operation implementation or
second text engine is added to xlang.

The document keeps counted UTF-8, including embedded NUL. Pattern and replacement
use checked C strings and reject embedded NUL before C entry. Existing native
case/whole-word flags, regex captures such as `$1`, newline normalization and
undo semantics remain. Search ranges are exact UTF-8 byte spans, including a
base character inside a combining grapheme. The public document insertion API
continues to use its existing grapheme-clamped ranges. This requires the
matched counted-search SDK rather than an older installed DLL/archive.

The native repair corrects three observed cases: regex replacement previously
truncated counted text at NUL, plain replacement advanced by raw CRLF input
length rather than committed normalized length, and public grapheme clamping
could change a plain match range. A private counted edit reuses native history,
validation and observers while checking exact UTF-8 byte boundaries. Each whole
operation also pins the native document until its group and cleanup complete.
No public C signature or structure layout changes.

An error does not imply rollback. Plain replacement may have changed earlier
matches before OOM; either variant can commit before a change callback fails.
The output count is assigned to the language destination only on success.
Inspect the document after catching an exception and use its undo group as
appropriate. Earlier native outcome/diagnostic wins over later cleanup failure;
this batch's old SDK passed that first-error regression, so it is not recorded
as an additional original defect.

The optional regex diagnostic Buffer is writable and pinned across the call,
including notifications. The native capacity is its byte length, not a resize
request. The Buffer retains its original length and may contain a terminated,
truncated diagnostic followed by old bytes. Attempted reallocation in a callback
is checked. A malformed regex throws even without a diagnostic Buffer.
Documents remain creator-thread objects. Active native scope, native history,
owned callbacks, managed view, CodeLease and checked error propagation retain
their existing costs. Closed owners and bound methods reject later access.

The short [lesson](../lib/bindings/xui/examples/code_replace.xl) performs a
literal replacement/undo followed by captured-number formatting. No window,
asset, service, manual DLL load or test harness is needed. Fault injection and
heap comparison live in `test/cases/foreign_xui_code_replace.*` and
`tools/check_xui_code_replace.py`.

Prepared pattern/replacement strings require no C-string mirror. In the measured
1000 no-match plain calls, language and native C both allocate/free/reallocate
zero XRT objects; the language run also succeeds with allocation budget zero.
For 100 regex calls the two prepared paths have equal heap counts, including
native regex compilation and temporary text/output work. Using two literals
inside each loop adds two ordinary language-string allocations/frees per call.
There is no implicit compiled-regex cache. These measurements do not establish
CPU-time equivalence or zero bridge overhead: scans, receiver/error scopes,
thread checks, ownership pins, branches and calls still apply. Cold compiler,
package initialization and callable construction are outside those intervals.

Public writable-array result contracts, internal CodeEdit model aliases and
general module/global callback-cycle retirement remain separate work. The
complete XGE/XUI/Document goal and final matched runtime/IDE/help delivery are
still active; this batch does not install into `release/`.

Both platforms pass 24 primary fixture/lesson steps in JIT, dynamic and static
AOT, including public C11/C++11/C++17 header checks and independent executable
imports. Actual O0/O2 hosted/static native clients add 128 positive search,
compound-edit and document-lifetime regression steps. Legacy/intermediate
controls are recorded separately. Staged r3 preserves all executable,
declaration and asset bytes tested in r2; the only differences restore LF
line endings in two READMEs. No executable rerun is inferred from that
metadata-only restaging.

The same static SDK/full compiler CodeModels lesson comparison, with only the
two unused method declarations removed in the control package, measures file,
text and BSS reachability; exact per-platform deltas, source and SDK hashes are
in `dev/release-materials/provenance/xui-code-replace-20261114.json`. The native
O2 repair itself grows text by 1664 Windows/1600 Linux bytes and BSS by zero;
whole-image file deltas include linker/export layout.
