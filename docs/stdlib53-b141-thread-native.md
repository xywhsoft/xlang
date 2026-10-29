# b141: resident Thread owner and native lifecycle proof

This checkpoint continues the approved 53-module integration. It adds the
Thread resident core; it does **not** yet replace the generated language Thread
backend, certify rebuilt shipping products, or complete the long-term goal.
The previously accepted held-lock checked host recovery contract is unchanged.

## Physical contract

`src/runtime/xl6_thread.c` owns the exact private handle/owner/graph operations.
Full nominal identity and private immutable handle policy are checked before
payload access. An actual coordinated V2 closure is borrowed at construction;
the exact `() -> i64` invocation ABI remains an IR/native-caller precondition.
Legacy V1 closures and forged foreign handles are refused. This is not a new
certificate for arbitrary closure captures or custom source owners: complete
graph admission must still admit each child before tracing it.

The shell is wrapped, bound and marked for phased cleanup **before** native
thread creation. Failed construction therefore never invokes user code. The
worker has a real owner RC pin and a real owning closure edge. Invocation uses
the existing resident Closure Enter/Leave contract, including a genuine code
scope/lease when present. No module-generated thread entry, drop or trace hook
is stored by the owner. Status APIs contain transient errors and restore the
caller's prior diagnostic.

Short metadata transitions use the ownership mutation domain and a private
C11 atomic gate. Blocking XRT waits, user invocation, capture cleanup and host
lease release occur outside those scopes/gates. In-flight methods retain real
activity pins. A last shell's ordinary release reserves a real transition pin
before attempting logical completion; self-release skips self-wait and leaves
the worker pin responsible for the tail. A native waiter survives losing the
last shell without accessing freed owner storage. Running workers remain
externally rooted by their actual pin; they are not graph-cleared as cycles.

The worker removes its owning closure edge before releasing the local owning
reference. Callback failure, including failure without a diagnostic, and
closure cleanup failure produce exit code 1. A successful i64 callback retains
the existing deterministic signed-low-32-bit exit representation. Graph Finish
is repeatable and detaches native storage under a protected transition before
destroying it outside the gate.

XRT threads are detached; Wait observes **logical completion**, not an OS-thread
join. Neither this core nor these tests establish physical OS-thread termination,
safe image unload or Linux terminal pthread-key retirement.

## Actual acceptance evidence

`tools/check_stdlib_thread_native.py` compiles the actual new provider and host
at O0/O2 with `-std=c11 -Wall -Wextra -Werror -Wshadow`. It uses genuine resident
closure/error/ownership/XRT implementations from the already rebuilt candidate
runtime library, not fake factories, synthetic graph counts or substituted
providers. It hashes the actual compiler, provider, host, headers and runtime
library and refuses to overwrite any output directory.

All final reports have `Complete=true` and `InputsUnchanged=true`:

| Report directory under `build/test/` | Phases | Scope |
| --- | ---: | --- |
| `stdlib53-b141-thread-native-win-dev7` | 26 | Windows strict native O0/O2 |
| `stdlib53-b141-thread-native-linux-dev5` | 26 | Linux strict native O0/O2 |
| `stdlib53-b141-thread-native-linux-asan-dev3` | 26 | Linux provider/host ASan + UBSan |
| `stdlib53-b141-thread-native-linux-tsan-dev2` | 8 | Linux provider/host TSan bounded concurrent-method/worker stress |

Windows links the genuine b138 rebuilt `xl.dll`; Linux links the genuine b140
rebuilt `libxl.so`. The new provider is compiled directly into each diagnostic
host, not claimed to be present in those earlier shipping libraries. Those
whole runtime libraries are **not instrumented**. The TSan scope excludes graph
freeze observers whose synchronization belongs to the uninstrumented runtime;
it is bounded stress, not a proof that the entire runtime is race-free.

The 86 phases include invalid identity/null/legacy closure/foreign-pointer
refusal; alias/status/prior-error preservation; actual owner/closure/capture
counts under freeze; refusal to claim a running worker; repeatable finished
graph cleanup; timeout/cooperative stop; last-shell waiting while a separate
native thread acquires a real graph freeze; an in-flight waiter retaining the
owner after last-shell release; worker self-release without self-wait; true
code-scope activity and lease survival after owner Close; callback/cleanup
errors; the complete **caller-thread** constructor allocation-failure prefix
(three failing points, no callbacks on rollback); 256 worker creations in the
worker stress mode and 32 sets of eight shared-owner waiters (256 native calls)
in the method stress mode. Each mode verifies heap
allocation/free balance, invalid/double-free and use-after-free counters.

The externally resident four-field test code owner intentionally has no graph
certificate: its lease Count refuses inspection even while the genuine code
scope is active. Testing scope lifetime does not admit that owner into a graph.
The callback-cleanup error test explicitly gates invocation until the caller's
closure reference is gone, so the failing capture is actually released on the
worker, not incidentally on the test's caller thread.

Earlier immutable development reports are retained, including host API spelling
errors and the incorrectly expected Count for an opaque four-field test owner.
They are not promoted to acceptance. Reports distinguish native diagnostics
from language/source/shipping/full53/image-unload acceptance, all still false.

## Next required integration

Replace the generated Thread frame/entry/drop/trace body with the fixed resident
ABI, declare native component dependencies and resolver/build/VFS inputs, and
add exact closed Thread admission policies for typed collections/trees/queues
without trusting tags or type IDs alone. Rebuild coherent Windows/Linux products
and batch language JIT/AOT/error/OOM/retirement/pruning tests. Channel, WaitQueue,
remaining dynamic collection/resource domains and broad full53 release gates
remain open. This checkpoint does not promote workspace release binaries.
