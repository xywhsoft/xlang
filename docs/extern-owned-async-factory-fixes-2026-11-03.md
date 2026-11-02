# C owner async factory boundaries

The three defects identified after the async-result review are repaired in the
compiler and typed XLIR. Runtime tags, XRT layouts and read access are unchanged.

* Override-family lookup consumes the complete ancestry, with a declaration
  budget that rejects cycles. It never returns an intermediate override after
  an arbitrary depth limit. Base-typed and Level40 bindings reach Level33/40.
* Producer method parameters close against selected arguments. Caller free
  parameters become private generic parameters with distinct names and copied
  constraints; the worker is checked in its declaration scope. Callable-value
  call sites cloned during monomorphization retain their receiver and exact
  signature, even though they intentionally have no declaration target symbol.
* Named global and static async factories use logical Future payload matching
  and the same private-worker publication as bound instance methods. Exact
  signatures rank ahead of ownership conversion; overload probing does not
  publish a converted worker. Inherited static methods retain declaration scope,
  access flags, effects and defaults.

Existing Future values remain invariant. Callable inputs and containers remain
invariant. The accepted conversion applies when binding a producer declaration,
not when casting an already-created Future. This change does not add implicit
inference for generic named functions where the language requires explicit type
arguments.

## Cost

Private workers replace the producer body used by the selected binding; no
forwarding Future, task or additional suspension is introduced. Generic free
types add compiler specialization work and generated code. Virtual adaptation
retains a private dispatch family and its static metadata. Existing receiver
capture, closure allocation and ordinary-result materialization costs remain:
an ordinary callback result copies its C-owned text/data at that boundary.
This is not a claim that every callable or asynchronous operation has zero cost.

## Regression evidence

`tools/check_extern_owned_string.py` covers the three new fixtures using emitted
C, JIT, executable generation and the resulting AOT executable, alongside the
existing owner lifetime, OOM and rejected-ABI cases. Its independent C gate runs
`xlir_async_callable_result_contract.c` over the original async fixture and all
three new fixtures, checking private-worker suspension plans and rejecting a
mutated plan. The review's 41 programs supply exact-owner and ordinary controls
on Windows and Linux.

Validation drivers and raw reports are development material under
`dev/release-materials/`; this source repair does not replace shipping binaries.
