# XGE error reporting and field snapshots

`xge.reportError(code,recoverable=1,subsystem=null,operation=null,message=null)`
directly calls `xgeReportError` once. Existing generic `c_pack` declarations
construct its sized record on the stack. Text pointers are borrowed only for
that call; nullable inputs preserve NULL versus a present empty string.
An embedded NUL in an input string is rejected before the producer runs.

The argument `code` is the error being reported. The native function's return
value is a separate status for dispatch itself: zero succeeds and another
status raises a checked error. Reporting code zero is invalid. Without a
registered observer, a valid report succeeds without invoking a callback.

`ErrorInfo.subsystem`, `.operation` and `.message` now return `string?` using
the already approved generic C pointer-field snapshot contract. Each getter
checks the callback epoch before reading its pointer, scans for NUL and copies
a present value into independently owned string storage. NULL remains absent;
an empty C string remains present. The source event expires on return. Saving
the event does not prolong it, while a copied string survives source mutation,
self-removal of the observer and the end of the callback.

This replaces the previous ErrorInfo getters returning `CString?` callback
views; applications read `info.message` directly instead of explicitly
copying such a view. The general scoped CString type is unchanged.

The native publisher still owns the registered closure and pins its invocation
snapshot through the full C return. Explicitly clear the observer when finished.
Callback errors and allocation failures propagate through the existing checked
boundary. Failed getter assignment preserves the previous string. Snapshot
failure does not undo earlier user work performed by the observer.

The focused fixture uses real native publication with mutable stack strings,
NULL, empty and embedded-NUL source values, all three expired getters, three
independently injected copy failures, NULL reads under zero allocation budget,
self-removal, invalid producer inputs and a console-only learning example.
An additional alias fixture changes captured nullable and nonnullable input
strings inside the observer, then reads the original native fields. Evaluated
input owners remain alive through the producer's C return.
The generated C witnesses direct field reads and the single packed native call.

The hot comparison copies and releases the same three strings on both sides
over 1000 dispatches: 3000 alloc/free, zero realloc in both language and C.
The initial diagnostic included language-only assertion literals and unwrap
copies; that unequal workload and its 9000 count are retained externally,
not reported as a bridge cost. Native registration, language closure/code
ownership, callback epochs, checked scopes, input NUL validation and scans
remain costs. Equal allocation counts do not establish CPU equivalence or
zero-cost callback dispatch. Each snapshot intentionally allocates and copies.
The separate mutation/alias fixture also emits three `xlir_string_copy` calls
before native entry to protect its captured/global input strings from callback
mutation. Those input snapshots can allocate/fail and are not part of the hot
comparison, which deliberately reuses ordinary stable local inputs. The stack
record itself neither allocates nor copies the string payloads.

Both platform gates use complete O2 compiler/TCC/XRT/VFS cohorts, the matched
existing XGE SDK and separate hosted/static packages. C11/C++11/C++17 public
headers and JIT/dynamic/static AOT run without engine Init or a window. Static
executables have no XGE host import; AOT does not import compiler/TCC DLLs.
The successor receipt is `xge-error-fields-20261115.json` under
`dev/release-materials/provenance/`. This is an incremental source qualification;
the final matched release/IDE installation and other pending contracts remain.
