# Graphics creator-thread calls and retirement

The existing resident ForeignView contract supplies the required thread gate.
This qualification closes the uncertainty recorded in the Texture batch; it
adds test fixtures and evidence, without a second graphics-specific call gate.

| Operation | Production behavior |
| --- | --- |
| Native method / native field access | `xl6ForeignViewCallBeginV1` checks the root creator before entering a native scope or exposing payload storage |
| Explicit close on another thread | `xl6ForeignViewCloseV1` rejects the request; the original resource remains usable on its creator |
| Last language reference released on a worker | Release consumes that reference and publishes an intrusive retirement item; it retains the actual native owner and code dependency |
| Worker tries to drain creator work | The queue only selects items belonging to the draining thread; creator work remains queued |
| Creator outside a native callback | Checked draining consumes the native owner and releases observation/code after the C call returns |
| Creator inside a native callback | Retirement remains deferred while its C stack is active; the outer generated checked boundary processes it |

The native owned factories independently enforce the engine creation thread.
Raw C APIs still require their documented caller-thread and pointer contracts;
ForeignView is a trusted native publisher protocol, not arbitrary C pointer
validation. Native retained-result operations use the current input scope, so
the safe declarations preserve the same creator identity through their calls.

`foreign_xge3_threads.xl` verifies actual language-thread rejection of native
properties, mutation and explicit close, then checks that the creator can still
use the original resources. The test-only C authority creates its own sole
ForeignView reference, transfers it to a raw worker, and verifies queued final
release using the actual package policy, native object and compiler-supplied
code owner (`@c_code_owner`). It never consumes a
borrowed language argument. Eight families cover Texture, system Font, Image,
Material with an owning texture slot, GPU Buffer, RenderTarget, Pass and shader
variants. Queue publication/draining on the worker is exercised with a warmed
execution context and the next logical allocation forced to fail; no allocation
is attempted and the native owner remains alive until creator drain.

`foreign_xge3_threads_gpu.xl` adds a real Shader in one bounded frame, verifies
language-thread field/uniform/close rejection, and checks callback-tail deferred
retirement at the actual checked Run boundary. It tests safe retirement after
window backend cleanup, rather than claiming a live-context GPU deletion timing.

The full O2 Windows compiler/TinyCC/VFS cohort and matched dynamic/static XGE
packages passed 25 emission/JIT/AOT/import steps, including the existing render
resources, OOM and readable batch example. Static executables import no XGE host
or xlang runtime/compiler DLL. The fault-injection dependency is explicitly
declared by the fixtures so AOT includes its optional XRT module; it is not added
to the shipping package's ordinary dependency set.

Evidence: `dev/release-materials/provenance/xge-thread-lifetimes-20261110.json`.
This is qualification of these resource families and the shared scope contract,
not proof for unimplemented Scene/async/other families. Linux language tests and
the final matched SDK/compiler/IDE delivery remain in the full integration task.
