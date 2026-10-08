# Resident synchronization retirement checkpoint

This native slice implements the two human decisions of 2026-10-09: locks
stay manually unlocked, refused cleanup is checked instead of aborting, and
recovery is a host API. It does not complete the language/IR migration or
certify a rebuilt shipping DLL. The SDK declarations are new source APIs;
existing release products do not yet contain their exports.

The subsequent current-source Windows language checkpoint has also passed,
but its integration is still being assembled separately from this independently
accepted native slice. Neither checkpoint certifies shipping products.

## Owner and retirement contracts

- The common private handle ops authenticate provenance before inspecting raw
  data; full identity and immutable family are checked before casting native
  storage. Foreign ops with an invalid pointer are refused without dereferencing
  it. Clone aliases the actual object, not a second native lock.
- All method activity holds real bounded native references. Metadata gates are
  C11 atomic gates inside the ownership-world mutation scope. World mutation
  is not a mutex. Blocking native calls, physical Unit and error propagation
  run outside these gates and their own world scope.
- Last-shell Drop takes a real private transition pin around native Unit. If
  Unit refuses, an allocation-free intrusive retirement record takes its own
  reference before the transition pin is dropped. The expired language shell
  is not kept as a fake root. No native pointer is cleared, freed or lost.
- Phased graph Finish similarly preserves actual plan pins on refusal and
  creates an inspectable retirement record. Successful physical completion is
  idempotent. Neither Drop nor Finish automatically unlocks anything.
- Registry lock order is owner-before-registry; lookup takes a real pin before
  releasing the registry gate and then obtains the owner gate. There is no
  registry-to-owner lock inversion. Acquire reserves a separate real activity
  pin while allocating/binding a recovery shell, preventing concurrent Unit
  from racing that wrapping step. Busy/finishing Acquire may legitimately fail.
- Records use nonzero monotonically increasing tokens, never addresses or
  reused tokens. Counter exhaustion is an explicit bounded-state invariant.
  A failed record retains resident native code/data, not source-module hooks.

## Checked host API

`release/include/xl_sync_retirement.h` declares:

- `xlSyncRetirementSnapshotCheckedV1(rows, capacity, &total)`: allocation-free,
  coherent snapshot; total may exceed capacity. It does not pin listed tokens.
- `xlSyncRetirementAcquireCheckedV1(token, &handle)`: genuine owned recovery
  shell, released with `xrtValueRelease`. Normal reacquisition and fresh
  language-resource admission are refused for pending/cleared owners.
- `xlSyncRetirementUnlockCheckedV1(handle, mode)`: explicit mutex/read/write
  unlock only. Mutex unlock must occur on the original locking native thread;
  wrong-thread or wrong-mode refusal preserves the record.
- `xlSyncRetirementRetryCheckedV1(token)`: checked Unit, retaining the record
  on failure/busy and removing its registry reference only on actual success.

Each call returns 1/0 independently of diagnostic availability. Cleanup's
existing first error, including an inherited failure bit with a NULL error,
remains primary. Physical success can therefore coexist with an old diagnostic;
it is not inferred from `xrtGetError()==NULL`. A failed module finalizer/close
must be retried separately after repairing the native record. Successfully
retired objects are safe but inoperative even while recovery shells remain.

## Family selection

`xl6_sync.c` is the shared owner/retirement core. `xl6_mutex.c`, `xl6_cond.c`
and `xl6_rwlock.c` supply fixed resident physical kernels and native methods.
Hidden provider helpers are not a registration/callback/admission API. The
common object has no link reference to any family kernel or constructor. The
common owner authenticates the private handle and immutable family before
each provider casts its prefix. Unit/repair-unlock pointers are fixed native
provider code; generated semantic callbacks and arbitrary owner certification
are not supported.

`xl6_array_sync_admission.c` owns immutable leaf-policy metadata: full native
identity, exact mutex/cond/rwlock kind and nullability. Policy equivalence checks
all three, including when a C host intentionally reuses the same identity across
families. Admission authenticates resident provenance before reading owner data;
it allocates nothing and preserves prior errors. Nullable absence is accepted;
nonnullable absence and default filling are refused. These policies retain no
live object, arbitrary callback, generated schema span or source-module lease.

Independent native executables link common + exactly one family. Symbol
inspection proves the other two language constructors are absent. This is
native-provider selection, not proof of generated-C or static-XRT pruning:
these diagnostics use an existing genuine shared runtime containing kernels.

## Current evidence

Reports under `demo6/build/test`, each `sync.native.json`:

- `stdlib53-b135-sync-native-win-dev6`;
- `stdlib53-b135-sync-native-linux-dev6`;
- `stdlib53-b135-sync-native-linux-san-dev5`;
- `stdlib53-b135-sync-native-linux-race-dev4`.

All report `Complete=true`, `InputsUnchanged=true` and native family link
selection true. Each has 34 strict O0/O2 phases, with `-Werror/-Wshadow` and no
warning exclusions. ASan/UBSan and TSan instrument new providers/hosts only,
not the whole old runtime. TSan runs real concurrent native activity and
retirement operations without a concurrent uninstrumented-freeze observer.

Tests cover actual held mutex/read/write lock refusal, expired weak shells,
preserved STATE records, primary-error identity, wrong-thread unlock, recovery
wrapping allocation prefixes, each family's complete constructor allocation
prefix, alias state, invalid provenance, negative timeout, live cond activity
with last external shell release, and a real `XL6_OwnershipPlan` rooted in the
genuine owning value. Plan Finish fails while holding real shell/owner pins;
host repair + Unit retry lets the same plan Finish and Destroy successfully.
The plan tests cover both a direct native-owner root and a genuine owning
Array of mutexes; the resolver explicitly authorizes the exact resident policy.
The policy constructor's complete four-allocation prefix rolls back cleanly.
An explicit NULL-first-error carrier test is not presented as a naturally
observed diagnostic-allocation failure. Every successful native lane returns
tracked alloc/free/live/error memory to baseline. Failed early reports remain
as evidence and are not promoted to acceptance.

`stdlib53-b135-sync-source-win-dev3/build.json` records 254 current-source
translation units, including the actual XRT/xruntime implementation; there are
no compiler/provider substitutions. The released TCC core is loaded dynamically.
`stdlib53-b135-sync-language-win-dev5/sync.language.json` passes 120 phases,
including real JIT, deterministic generated C, O0/O2 AOT, required-owner NULL
guards, closed-schema mutation rejection, precise family selection, each actual
language constructor/policy allocation prefix, primary-error preservation and
checked global deactivation retry. The mutex/read/write/primary JIT tests confirm
that the real source code page is MEM_FREE before resident host recovery. All
tracked language/provider lifetimes return to baseline. Existing generated
unused-helper allowances and the compiler header's legacy shadow allowance are
explicit in the report commands; these are not the exclusion-free native lanes.

## Remaining integration, not waived

Fixed IR symbols and physical constructor outcomes, removal of old generated
aborting owners, module resolver/JIT exports and precise per-family artifact
selection now pass the current-source Windows checkpoint. The complete language
migration still needs its independent integration commit, Linux language proof,
rebuilt Win/Linux shipping DLL/VFS products, standalone static-XRT pruning and
full 53-module acceptance. The reports keep all those broader acceptance flags
false. Old release products still use the legacy generated sync Drop.
