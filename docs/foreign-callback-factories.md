# Owning factories on callback-period views

A callback-period native view can call a status/output producer which creates
an independent owned root. This is the same checked C factory contract used
by free functions and ordinary managed methods; it does not convert the
callback view itself into a long-lived owner.

```xl
extern "Model*" class Model @header("model.h") @lifetime("owned") @policy("modelPolicy")
 native point handle
end
extern "modelReleaseChecked" @header("model.h") @no_raise @c_result("status",0) func rollback(Model value)
extern "Model*" class CallbackModel @header("model.h") @lifetime("callback")
 native point handle
 extern "modelShareOwned" @no_raise @c_result("owned_view") @factory(1,0,rollback) func share() as Model
end
```

The authoritative native signature is `int modelShareOwned(Model*, Model**)`.
Its successful output transfers one real native owner. A share operation must
acquire that owner in the actual native implementation; identical pointer
bits alone are insufficient. A callback may save the returned `Model` after
the physical callback has ended. Saving the original `CallbackModel` leaves
an expired checked view. Expired receiver use is rejected before the producer.

This additional member admission is limited to a validated callback-epoch
receiver and an independent owned-root status/output factory. It excludes
caller pointers, owner-child outputs, same-root borrowed outputs and in-place
compiler-record construction. Explicit no-raise producer effects, exact
output slots, authoritative C ABI proofs and typed cleanup remain required.
Sealed IR applies the same restriction to direct, bound and protocol calls.
Direct native pointer-return owning factories remain unsupported by this
contract; they must not be described as already available.

Generic root storage and code residency are reserved before the native call.
A reservation failure calls no business producer. An actual owner written by
a failed producer is still observed and retired with its original error
preserved. Input epoch validation and ordinary factory scope bookkeeping
remain. There is one direct native business call and no per-method C wrapper.
The reservation and owning-carrier allocations are real cold costs.

For no-scope leaf policies, cleanup requested within an active callback waits
for its physical return tail. Logical close already rejects later access;
the native owner remains held until safe cleanup. Failed partial outputs must
likewise retain their owner through that tail rather than free it early.

`tools/check_foreign_callback_factory.py` runs actual retained callbacks at
GCC O0/O2, verifies source-epoch preservation and independent owner survival,
checks expired access and pre-producer OOM, and rejects malformed declarations
and sealed IR before C/header output. Native test nodes contain only pure C
scalar data, so their leaf policy is deliberate. The gate is CPU-only and
records compiler object qualification separately from a shipping TCC/VFS
provider or the production Document model.
