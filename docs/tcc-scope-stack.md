# TinyCC lexical stack slots

The Windows Graph model integration exposed an executable stack overflow
before the test body ran. Its generated function reserved 1,125,824 bytes;
TinyCC executables normally reserve 1 MiB. JIT and Linux execution passed,
and a diagnostic executable copy with a larger reserve passed. That copy is
evidence of the cause, not the shipping fix.

TinyCC previously accumulated automatic slots from completed C compound
scopes. Generated ownership cleanup contains many short C blocks, so storage
that is never simultaneously live still accumulated in the function frame.
The frontend now restores the lexical allocation cursor and truncates spill
cache entries created inside a completed scope. A per-function low-water
mark preserves the maximum simultaneous fixed-frame requirement for the
backend prologue, stack probes and unwind metadata.

Statement-expression scopes retain their slots while an expression result
can refer to them. Bounds-check mode retains its previous allocation policy:
its function-wide local-region table has no lexical retirement protocol.
VLA scope cleanup and dynamic alloca storage keep their existing rules.
No language IR, ownership ordering, C ABI, runtime forwarding call, heap
allocation, or default executable stack size changes. The compiler maintains
two additional integers per lexical scope and one per-function low-water
value; the work occurs during C compilation.

The actual-provider regression compiles 2,048 sequential blocks. On Windows
the fixed frame changes from 2,097,200 to 1,072 bytes; on Linux from 2,097,156
to 1,028 bytes. Both O0/O2 provider builds execute the fixed program and check
outer lifetimes, aggregate/spill statement expressions, VLA, alloca, cleanup
and goto behavior. The oversized baseline frame is inspected without running
it. Existing literal pruning, PE unwind/exception pruning and relocation/
shared-load checks also pass: 58 outer steps across both operating systems,
including 960 preexisting fixture assertions. Independent verification pins
948 source, log and recorded artifact files across the two reports.

These are source/provider qualifications. Complete O2 compiler/runtime/VFS
rebuilds and the actual Graph JIT, hosted/static executable and shared-unload
replays are still in progress. The fix is not installed in `release/` and
does not complete the XGE/XUI/Document integration goal. The small evidence
index is `dev/release-materials/provenance/tcc-scope-stack-provider-20261117.json`.
