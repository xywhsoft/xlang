# Independent code-folding state

With a matching CodeEdit-enabled SDK, `xui.createCodeFoldState()` returns a
uniquely owned `CodeFoldState` with no
Context, widget, document, native child or stored callback. Its only native
data are copied numeric fold ranges and a lazy hidden-line cache. The existing
pure-data language root supplies creator-thread checks, active-call protection
and alias invalidation on close. No native reference-count fiction, watch,
fake activity callback or per-method C forwarding layer is introduced.

| Language API | Native API |
| --- | --- |
| `createCodeFoldState()` | `xuiCodeFoldStateCreate` |
| `clear()` | `xuiCodeFoldStateClear`; retains allocated capacity |
| `setRanges(ranges,size=-1)` | `xuiCodeFoldStateSetRanges`; typed const-record array projection |
| `getCount()` / `getRange(index)` | `xuiCodeFoldStateGetCount` / `GetRange`; independent 20-byte record |
| `copyRanges(output=null)` | `xuiCodeFoldStateGetRanges` |
| `trackEdit(startLine,endLine,newEndLine)` | `xuiCodeFoldStateTrackEdit` |
| `toggleLine(line)` / `foldAll()` / `unfoldAll()` | Same native fold-state functions |
| `isLineVisible(line)` | `xuiCodeFoldStateIsLineVisible`; checked integer output |
| `visibleLineCount(lineCount)` | `xuiCodeFoldStateGetVisibleLineCount` |
| `lineToVisibleRow(line)` | `xuiCodeFoldStateLineToVisibleRow` |
| `visibleRowToLine(lineCount,row)` | `xuiCodeFoldStateVisibleRowToLine` |
| `copyVisibleLines(lineCount,output=null)` | `xuiCodeFoldStateBuildVisibleLines` |
| `close()` | Checked generic view close; native Destroy after active calls finish |

All indices are zero-based logical lines or visible rows, not byte positions.
A collapsed range keeps its header visible and hides subsequent lines through
its inclusive end. Toggle selects an exact header, or the innermost containing
fold; absence is the native UNSUPPORTED error. Native mapping/clamping rules
are preserved, including negative/excessive visible rows and an empty document.

Use a zero `CodeFoldRange()` then assign start/end/level/flags. Native accepts
size 0 as its default; short nonzero size and non-increasing line ranges fail.
Set copies and normalizes each record's size and HEADER flag. Changing the
source array or a returned record cannot alter the model. The optional size
selects a prefix: -1 uses all, oversized counts clamp, and other negatives fail
before native entry. Empty input clears the range count through native Set.

TrackEdit adjusts strict interior folds and shifts later ranges. An edit
touching a header or closing line invalidates that fold. Overflow is validated
before mutation. Set validates all input before publication and native reserve
failure keeps old ranges. A first visible query can allocate its hidden cache;
failure is checked and a successful later query can retry. Native IsLineVisible
writes a provisional output before such failure, but the checked language
output contract does not assign that failed result to the caller's destination.

## Costs and evidence

The native source and O2 SDK are unchanged in this batch. Business calls invoke
the original native functions once. The only new C code is cold inline leaf
policy metadata that releases the native object. The full compiler's existing
factory, typed-array, output-value and checked view contracts are reused.

For prepared two-range inputs, create/close with language scope exit performs
3 allocations/frees versus C's 1: native state, managed view and CodeLease.
These intervals use an already initialized language/foreign runtime; process
and package startup are outside the measurement, not claimed allocation-free.
Setting ranges on a model with sufficient native capacity performs one
temporary projection allocation/free in xlang, zero in C. The O(n) projection
validates records and copies 20 bytes each; native Set separately validates
and copies them into its own storage. Constructing the boxed language array
itself is outside that interval and has its normal boxing/copying costs.

After preparing the hidden cache, 1000 rounds of six equivalent queries
(count, range, visibility, visible count, line-to-row, row-to-line) perform
zero allocation/free/reallocation in both language and C under a zero-allocation
budget. Lazy cache creation, range-capacity growth and native sorting retain
their costs. Checked receivers/errors, native calls, record copies, scope
cleanup and ownership bookkeeping still execute. No CPU equivalence or
whole-path zero-overhead claim follows from these allocation measurements.

An existing CodeSelection/CodeTheme static lesson imports the same xui package
without using FoldState. Removing only the new manifest entry changes its
file/text/BSS sizes by zero on both targets, with the same compiler and static
SDK. This is a selected unused-feature comparison, not a universal size claim.

`tools/check_xui_code_folding.py` requires a qualified complete compiler and
exact matched O2 hosted/static SDKs. Independent GCC O0/O2 clients reuse the
native fold-state suite (nested/overlapping folds, provider growth, boundary
overflow and row clamping) plus native allocation prefixes and failed outputs.
Language JIT/dynamic/static cases cover the public methods, prefix semantics,
record isolation, invalid output preservation, TrackEdit overflow, 96 allocation
prefixes per mode, typed array method values, active close, wrong thread and
expired bound methods. Case and beginner lesson expose C11/C++11/C++17 headers;
AOT imports prove no compiler/TCC dependency and no XGE host in static builds.
See `dev/release-materials/provenance/xui-code-folding-20261114.json`.

`examples/code_folding.xl` configures one range, collapses it, prints visible
line count 3 and row 2 mapping to line 5, then unfolds and closes in `finally`.
It needs no engine, font, window, file or service.

## Bulk queries

The same bulk queries are available on the live view returned by
`CodeEdit.foldState()`. `CodeDocument.buildCFoldRanges(output=null)` and
`CodeEditDocument.buildCFoldRanges(output=null)` scan a document.
`xui.buildCFoldRanges(source,output=null,size=-1)` scans an explicit UTF-8
byte span, including embedded NUL. `size` selects a byte prefix.
`xui.buildCodeVisibleLines(lineCount,ranges,size=-1,output=null)` uses a
prefix of a typed `array<CodeFoldRange>` to compute the visible line numbers.

Omit the Buffer to query the count. To receive data, resize a reusable
Buffer to a whole number of native records or integers. On Windows and
Linux x64 these are 20 and 4 bytes respectively. Each method returns the
full item count and writes only the prefix that fits. Excess bytes stay
untouched. Buffers use native layout and byte order.

Each query calls the original native function once. The compiler validates
Buffer alignment and whole-element sizing, then derives the native capacity
from its actual byte length. Native code writes directly to that Buffer.
`buildCodeVisibleLines` projects its typed input into temporary native
records. The first folded-state query may still allocate its native
hidden-line cache. If a checked call fails, no count is returned. The
C text scanner can write a partial prefix before detecting a native parser
limit, so callers should use the output only after a successful return.

The provider callback that writes caller-supplied ranges still needs a
separate callback contract.
