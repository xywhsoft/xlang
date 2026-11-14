# Native code annotations and search markers

`createCodeAnnotationStore()` owns an independent native store of numeric
records and copied strings. It keeps no Document, UI object or language callback.
The existing unique-leaf protocol enforces creator-thread use, active-call
close deferral and expired receiver/callable rejection. Close in `finally`.

| Language operation | Native entry |
| --- | --- |
| `setMarker` / `clearMarker` / `clearMarkers` | `xuiCodeAnnotationSetMarker` / `ClearMarker` / `ClearMarkers` |
| `setIndicator` / `clearIndicator` / `clearIndicators` | `xuiCodeAnnotationSetIndicator` / `ClearIndicator` / `ClearIndicators` |
| `setDiagnosticsColumns` / `clearDiagnostics` | `xuiCodeAnnotationSetDiagnostics` / `ClearDiagnostics` |
| `getDiagnosticCount` / `getDiagnosticVersion` | `xuiCodeAnnotationGetDiagnosticCount` / `GetDiagnosticVersion` |
| `trackEdit` / `clear` | `xuiCodeAnnotationTrackEdit` / `xuiCodeAnnotationStoreClear` |
| `clearSearchIndicators` | `xuiCodeSearchClearResultIndicators` |
| `CodeDocument.markAllPlain` / `markAllRegex` | `xuiCodeSearchMarkAllPlain` / `MarkAllRegex` |

Factory/destruction map to StoreCreate/StoreDestroy. There are 16 distinct visible
native declarations, plus native destruction through the lifetime policy: 14/19
annotation-family functions and all three mark-all search-indicator helpers.
Five annotation output functions remain pending typed bulk outputs and borrowed
string fields in output records. CodeEdit's internal store is not exposed as an
independent owner. Declaration inventory is not full runtime certification.

`setMarker(line,id,flags,text,userData=0)` passes checked C text; an overload
`setMarker(line,id,flags=0,userData=0)` injects an exact typed NULL for tooltip.
Thus absent and present empty tooltip remain distinct without nullable-C-string
syntax or a new compiler rule. Text is copied by native storage. Embedded NUL
in terminated input is rejected. User data is a numeric uintptr tag and does
not retain a managed object.

Markers use zero-based lines. Indicators and diagnostic ranges use half-open
UTF-8 byte offsets. Clearing an indicator's interior splits its two sides and
can allocate; native validation/OOM retains previous data. A missing marker or
indicator clear returns native unsupported, which these checked methods report
as an exception. `clear()` is the native idempotent whole-store clear; no hidden
"missing means success" policy is introduced. Clearing diagnostics when empty
succeeds, as the native function specifies.

`setDiagnosticsColumns(ranges,severities,codes,messages,sources,userData,size=-1)`
uses six strict parallel typed arrays. Nullable code/message/source elements
keep NULL separate from empty text. Default size requires equal lengths;
nonnegative size selects the common available prefix, oversized size truncates,
and another negative size throws. One zeroed native diagnostic array is
prepared; unspecified iSize remains zero, which this native API accepts. Range,
scalar and string pointers are checked/copied into that array, source backing
is pinned for the call, and native storage clones strings before publication.
No ordinary object/getter traversal, forwarding function or second data engine
is added. Native failures preserve the old diagnostic set/version; invalid
columns/NUL fail before native mutation.

Diagnostic version is a native revision, not the caller-supplied token version.
Identical content does not increment it. Clear/edit follows native touch/shift
rules. `trackEdit(start,end,newEnd,startLine,endLine,newEndLine)` validates byte
and line deltas before applying them; diagnostic changes update the index and
revision. Both the Document and store are admitted for a mark-all call; regex
diagnostics use the existing bounded write-pinned Buffer span.

Mark-all ignores backward/wrap flags. Plain matching keeps native case/word
rules. Regex marking skips zero-width matches while advancing across graphemes;
existing counted document NUL survives. Clearing selected old indicators and
appending new ones is not atomic rollback: OOM or a regex error can leave empty
or partial results. Failed counts are not assigned to the language destination.
The native regex optimizer in `45833875` compiles once and copies the document
once per operation instead of once per match. It preserves one independent
subject snapshot; it adds no global pattern cache or language wrapper.

The [lesson](../lib/bindings/xui/examples/code_annotations.xl) marks two `todo`
occurrences, publishes one warning via clearly named columns, checks the counts
and closes both models. It creates no window, class, DLL loader, font or network
service. Fault injection and native payload assertions are separate fixtures.
Native payload reads in the C fixture are not an unsafe public language API.

Prepared 1000-round count/version reads make 2000 native calls in both language
and C with zero XRT alloc/free/realloc. One create/close pair is 3/3/0 versus
native 1/1/0. One warmed identical two-row diagnostic publication is 1/1/0 versus
native 0/0/0: language prepares one 96-byte native descriptor array on the
qualified x64 ABI. Real changed diagnostics allocate native arrays/index and
string copies as in C. String NUL scans, column checks/pins, struct copies,
managed owner allocation, CodeLease bookkeeping, checked scopes, thread tests,
indirect callable formation/call and the native algorithm retain their costs.
These heap counters do not prove CPU equality or zero bridge overhead. Cold
compiler/package startup is outside the interval.

Exact compiler/SDK/case/header/import checks, native regressions, work counters
and unused-declaration size comparison are pinned in
`dev/release-materials/provenance/xui-code-annotations-20261114.json`. This is an
incremental batch; remaining output/alias contracts, module/global callback
cycles and final matched runtime/IDE/help installation remain in the full goal.

Both platforms pass 60 combined native/language/lesson build, execution,
C11/C++11/C++17 header and import checks, plus 16 unused-interface reachability
comparison steps. The control removes only the new annotation module and its
two coupled Document mark declarations. File, text and BSS deltas are zero on
Windows and Linux. Two small x64 layout proofs establish the 48-byte native
row size. Native repair/regression and legacy controls are separate receipts.
