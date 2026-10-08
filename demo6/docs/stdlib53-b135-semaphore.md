# Resident semaphore checkpoint

This independently completed native slice adds a resident semaphore owner and
an immutable, owned Sequence admission policy. It is not the final commit of
the Tree/resource IR and artifact integration, nor source/shipping acceptance.

## Contracts

- Clone aliases the real native semaphore; no generated callback or module
  pointer is retained. Every actual method pins the owner until the native call
  returns. Blocking waits do not hold the ownership-world mutation scope.
- Admission checks the genuine private handle ops, context and full identity
  before dereferencing opaque data. Busy owners remain valid members. Checks
  allocate nothing and leave an existing error unchanged.
- The policy owns the precise nonnullable/nullable identity. It retains neither
  a live semaphore nor its count/bounds. Nonnullable default slots are forbidden;
  nullable slots default to absent. Independently constructed equivalent
  policies compare their full immutable metadata, not pointer identity.
- Constructors report failure and roll back partially initialized owners.
  Bool methods are no-raise status operations: timeout/full/invalid-operation
  status does not replace the caller's diagnostic. Timeouts saturate safely.
- Quiescent private semaphore Unit is an invariant (there is no held-lock
  state). Graph retirement has a separately checked, retryable Finish, retaining
  its plan pin on failure. This does not resolve mutex/rwlock held-lock cleanup.

## Evidence

Reports under `demo6/build/test`:

- `stdlib53-b135-semaphore-native-win-dev1/semaphore.native.json`;
- `stdlib53-b135-semaphore-native-linux-dev2/semaphore.native.json`;
- `stdlib53-b135-semaphore-native-linux-san-dev3/semaphore.native.json`;
- `stdlib53-b135-semaphore-language-win-dev8/semaphore.language.json`;
- `stdlib53-b135-semaphore-language-linux-dev8/semaphore.language.json`.

All report `Complete=true` and `InputsUnchanged=true`. Each native lane completes
8 strict O0/O2 build/run phases. ASan/UBSan instruments the two new providers
and native host only, not the whole existing runtime. Tests include actual
alias state, foreign ops with invalid raw data, wrong identity/policy, overflow,
prior-error restoration, real blocking wait with last-external-shell release,
phased retirement and complete native owner/schema allocation prefixes.

Each latest language lane completes 67 phases using hash-verified compiler
overrides rebuilt from immutable `stdlib53-b135-semaphore-schema-dev6` inputs
(`stdlib53-b135-semaphore-compiler-{win,linux}-dev7`). This exercises actual
public JIT, deterministic generated C, strict C/Wshadow and O0/O2 AOT lifetime
hooks, plus existing sync/thread/task fixtures. Three negative language guards
preserve the output sentinel. Source-derived resource fixtures reject 114 real
IR mutations per platform, before output or artifact-plan publication.

The in-progress IR changes give Sem/default-Sem/Stack constructors an explicit
required-owner NULL-result failure contract. Source-derived tests require
exactly one outcome test per producer, after full metadata binding; generated
C must test the returned owner rather than probe a diagnostic pointer. Separate
actual checked-entry O0/O2 fault tests cover all 6 allocation prefixes for each
Sem constructor and all 8 for Stack<sem>, with balanced live/alloc/free/error
accounting. All observed failures have XRT's static, allocation-free OOM error;
no diagnostic-allocation failure was reproduced or claimed. No allocator,
factory, carrier, resolver or language module is faked to obtain a pass.

The diagnostic integration also closes the native-resource ANY-admission
loophole and fixes fresh owned collection results and flow-narrowed nullable
sync receivers. Unsupported resource domains remain fail-closed pending their
real providers; this is not a decision to remove them from the original scope.

All reports explicitly leave source product, shipping library and full-53
acceptance false. The language report also leaves standalone link pruning
false. Failed early syntax/build/probe reports are preserved, not promoted to
acceptance. These tools require explicit root/schema/compiler arguments and
a fresh output directory; current-source build rows and release DLL/VFS are
not validated by these diagnostic object overrides.

## Remaining work

The full IR/Cgen/API/artifact/build integration remains in the worktree and
will be committed coherently after the remaining native resource domains and
source-product gates are closed. Continuation handoff, standalone Sem pruning,
all shipping build/package rows and full 53-module acceptance are still needed.

User decision (2026-10-09): mutex/rwlock retain explicit manual unlock. Cleanup
failure must be inspectable, not direct abort or automatic unlock. The old
generated sync Drop still aborts on refused native Destroy. It must preserve
the real owner on failed checked retirement, not clear its pointer, silently
leak, free a live lock or merely remove the abort. This defect remains open.

The WSL distribution is already at `E:\WSL\Ubuntu`. New diagnostic outputs and
temporary files stay on D; no VM restart or additional VM snapshot was needed.
