# Native code capability continuations

`xl6ForeignViewRetainCodeLeaseV1` exports an independent, already accepted code
capability from an exact live owning ForeignView on its creator thread. It
retains code and dependencies, without retaining the native resource or its
control record. Children, closed roots, claimed roots and unadopted storage
are rejected. Failure clears the output. A successful NULL output means that
this root was explicitly created with externally resident code; it is not a
capability discovered from an arbitrary native pointer.

`xl6ForeignViewCreateRootFromLeaseV1` borrows that accepted capability and
retains one child before observing or publishing the new native owner. It
never calls host Acquire, so it can continue an existing capability after
the host closes new admission. The supplied descriptor, native dependencies
and exact ownership domain must all be covered by that capability. A NULL
parent is an explicit externally resident C-host promise. Failure consumes
neither the caller's native owner nor its parent capability. Ordinary Watch,
creator admission, domain validation and checked retirement remain in force.

These are cold lifecycle operations. ForeignView layout and ordinary native
method calls are unchanged. The new root reuses existing control allocation,
ownership tracing and creator-thread cleanup. It introduces no second native
queue or language borrowing syntax. The retained capability must ultimately
be released by resident code after native return, or by a caller with another
independent residency capability.

The focused gate `tools/check_foreign_view_code_continuation.py` accepts a
verified full compiler build or an honestly identified partial overlay. It
rebuilds the changed resident TU and runs O0/O2 plus C++11/C++17 header checks.
It covers independent code survival after original resource destruction,
continuation after host Close, worker-last-release creator cleanup, wrong
thread/child/claim/closed rejection, allocation-free capability export under
OOM, allocation-prefix rollback, refused Watch, invalid domains, explicit
process residency and exactly-once final host release. It does not itself
qualify COM services, a complete compiler/provider rebuild or final delivery.
