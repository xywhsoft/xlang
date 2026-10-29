# Resident synchronization retirement checkpoint

This native slice implements the two human decisions of 2026-10-09: locks
stay manually unlocked, refused cleanup is checked instead of aborting, and
recovery is a host API. It does not complete the language/IR migration or
certify a rebuilt shipping DLL. The SDK declarations are new source APIs; the
main release-directory products remain old. Fresh independent Windows candidates
below contain and exercise their real exports without replacing those products.

The language/IR integration has subsequently passed on Windows and Linux,
including an isolated snapshot of the exact staged commit candidate. It removes
the old generated synchronization owners and selects fixed resident providers.
Neither the native slice nor this integration certifies shipping products.

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

Reports under `build/test`, each `sync.native.json`:

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

### Isolated integration candidate (b136)

`stdlib53-b136-stage-dev1/provenance.json` records the index tree at freeze,
base commit and genuine released TCC DLL hash. Its `frozen/demo6` was exported
from Git's index, not copied from the mixed worktree. Unrelated IDE, source
overlay, C binding/header and scalar class-default changes are excluded. The
Windows DLL build list is updated independently of the uncommitted response-file
refactor. Necessary Tree/Queue schema providers and build/VFS declarations are
included, so this candidate does not depend on untracked implementation files.

The candidate rebuilds 249 Windows and 248 Linux translation units with the
actual XRT/xruntime and compiler/native sources. The Linux list is derived from
the same canonical Windows source list, excluding its Windows-only UTF-8 CRT
shim and using a real Linux public driver. Only the genuine TCC core is reused.

The following candidate reports are complete with unchanged hashed inputs:

- `stdlib53-b136-source-win-dev1/build.json` and
  `stdlib53-b136-source-linux-dev1/build.json`: current-source builds;
- `stdlib53-b136-language-win-dev1/sync.language.json` and
  `stdlib53-b136-language-linux-dev1/sync.language.json`: 120 phases each,
  including actual JIT/AOT, held-lock checked failure/recovery, required-owner
  NULL outcomes, closed recursive recipes and allocation-failure rollback;
- `stdlib53-b136-artifact-win-dev1/sync.artifact.json`: 44 phases. Genuine TCC
  native-object symbol tables prove exact family selection; eight independently
  generated DLLs run their actual exports without xl.dll/xlc.dll imports and
  retire their runtime/TLS before real unload;
- `stdlib53-b136-tree-language-win-dev1/tree.language.json`: 21 deterministic
  C/IR/O0/O2 AOT phases covering Tree primitives, exact text and class references;
- `stdlib53-b136-tree-ownership-win-dev1/tree.ownership.json`: 38 phases,
  32 O0/O2 modes and 384 measured real JIT lifetimes, including five cyclic
  families and actual external roots. This is not AOT graph/unmap proof;
- `stdlib53-b136-queue-admission-win-dev1/queue.reference.admission.json`:
  868 strict native phases and eight complete OOM prefixes for mixed-depth
  Tree/Queue/Sequence schema composition and existing domain regressions;
- `stdlib53-b136-tree-native-win-dev1/tree.native.json`: 80 strict native
  phases, eight complete OOM prefixes, genuine graph edges and reentry checks.

Generated C now contains fixed provider calls and declarative closed recipes,
not synchronization lifecycle implementations or module-owned admission hooks.
Container schemas own immutable metadata, verify native provenance/full identity
and distinguish nullable absence from forbidden defaults. Recursion is bounded
at 64. Other resource/codec domains are not certified by accepting these domains.

### Still required

The subsequent b137 checks close static-XRT family selection for these four
synchronization fixtures. `stdlib53-b137-static-pruning-win-dev3/sync.artifact.json`
passes 64 phases and `stdlib53-b137-static-pruning-linux-dev2/sync.artifact.json`
passes 38, with unchanged hashed inputs. Both compile the real standalone XRT
implementation generator with genuine TCC and inspect defined object symbols.
The actual undeclared-C-source/full-runtime fallback is a negative control: it
contains all six checked anchors, while selected outputs contain only mutex,
mutex+cond, rwlock, or no synchronization kernel respectively. Semaphore, file
open and process run are absent from all selected outputs. The cond dependency
on mutex is deliberate and is not counted as unwanted code. O0/O2 refer to the
contract host; the selected and control kernel objects use genuine TCC.

Windows additionally repeats eight actual independent DLL executions/retirements;
Linux does **not** claim standalone ELF execution from object-table checks.
This is fixture-scoped kernel selection, not complete 53-module/function-level
pruning or acceptance of a rebuilt shipping library.

The b138 Windows candidate was rebuilt with the actual `build_dll.bat` from an
index export at `ecf184026`, not the mixed worktree. Bootstrap TCC, the XRT object
and both embedded VFS products were freshly rebuilt, not reused. Snapshot and
VFS gates passed, as did compilation of the existing DLL fixture hosts. Candidate
xl.dll SHA256 is `98028caadca0e822a2436c7f2ab882bd1f644770742941d853d3116d40d1f034`;
xlc.dll is `810bf0ef8c463a31efd423356c5000ed3c332873d4e276105d3883da13df59b3`.

`stdlib53-b138-shipping-proof-win-dev1/sync.shipping.json` passes 26 phases with
unchanged hashed inputs: both actual candidate DLLs, O0/O2 SDK hosts, held
mutex/read/write locks, original-assertion cleanup and checked global deactivation
retry. Each host loads TCC from the same DLL supplying the recovery API. Execution
runs in a separate directory containing no source tree, private/public headers
or TCC distribution; no JIT include path or external TCC library path is provided.
Real JIT pages are freed before local-scope host recovery. Allocations, errors and
owner lifetimes balance. `SyncShippingApiVfsAcceptance=true` is deliberately a
synchronization-only gate; overall shipping and full53 flags remain false.

### Linux shipping recovery and actual ELF execution (b139/b140)

The Linux source freeze is exported from the Git index with
`core.autocrlf=false`, including the docs required by the snapshot gate. The
b140 freeze starts at `bbcbb9bdb` with index tree
`190f391959dd3f58d96030ac8f0e3faef6cd0ba3`. Its source snapshot is
`stdlib53-b140-shipping-source-dev1/frozen/demo6`, copied to the fresh native
filesystem root `/var/tmp/xlang-stdlib53-b140-shipping-dev1/demo6`. Unrelated
IDE/header-binding/extension and response-file edits are not compiled.
The final verifier environment-isolation edit is separate from this immutable
source freeze; it does not alter the candidate compiler or providers.

`stdlib53-b140-shipping-build-linux-dev1/build.shipping.json` records a complete
unchanged-input build through the real `build_linux.sh`. TCC, the XRT object,
both embedded VFS images, libxl.so, libxlc.so and the CLI are rebuilt from
source, without reusing old provider/compiler objects. libxl.so SHA256 is
`295d395783d8b6d3a7615cb57d59274ea900279c23e1716dd356a972543cae1a`;
libxlc.so is
`cbafd42795926e84e85a8a015f4b690ee3f6f247dcc7469e55d6b1a0f47e5dea`.
Existing vendor `fread` warnings remain visible: this normal product build is
not advertised as an exclusion-free strict-warning build of all vendor code.

Both `stdlib53-b140-shipping-proof-linux-dev2/sync.shipping.json` and
`stdlib53-b140-shipping-proof-win-dev2/sync.shipping.json` pass 26 phases with
unchanged inputs. Each library supplies its own TCC and checked recovery API,
without external xlang JIT headers, a TCC distribution or native/source/loader
environment injection. Linux's libc headers, CRT objects and dynamic linker
remain target-system inputs, as explicitly required by the Linux VFS manifests;
these are not claimed to be embedded or excluded by the isolation test.
The five held-lock/error/global scenarios run on real O0/O2 SDK
hosts and balance measured allocations/owner/error lifetimes. Linux does not
claim the Windows MEM_FREE page-unmap observation.

The first isolated ELF batch exposed a genuine producer failure: TCC omitted
PT_GNU_STACK, and the current loader refused all four generated shared objects.
The producer now emits RW GNU_STACK for ordinary C output and preserves an
explicit executable-stack request from foreign `.note.GNU-stack` objects in
either merge order. No loader security setting, output-file patch or
executable-stack consumer workaround is used.

`stdlib53-b140-tcc-stack-linux-dev1/tcc.elf.stack.json` passes 12 phases:
fresh actual TCC executables and shared objects declare RW stack and execute;
foreign RWE controls are inspected only. The Windows source freeze also
successfully rebuilds its bootstrap TCC. The ported consumer is checked at
O0/O2 against eight existing independently generated DLLs: 16 actual
export/init/Unit/terminal-retire/unload executions in
`stdlib53-b139-artifact-host-win-dev1/host.port.json`.

`stdlib53-b140-shipping-elf-linux-dev2/sync.shipping.elf.json` passes 28 phases
with unchanged inputs. A copied real CLI and candidate libraries compile four
fixture texts using embedded xlang/TCC VFS, with no xlang source/include/TCC
distribution in the compiler directory; platform C dependencies remain on the
host. Real readelf tables confirm RW GNU_STACK, the actual
checked module exports and no xlang shared-library dependency. O0/O2 independent
consumers execute all four exported probes and checked module Unit successfully.
This is actual ELF execution, not private-provider pruning inferred from exports.

Terminal internal TLS retirement is explicitly Windows-only in the current XRT
contract. Linux consumers therefore keep the ELF image resident until process
exit. `SafeElfUnloadAcceptance` and `TerminalRuntimeRetirementAcceptance` remain
false; passing this batch does not justify calling dlclose on a runtime with
internal pthread-key callbacks. Failed b139 snapshots/ELF reports are retained
and are not promoted to acceptance.

The generated capability matrix is refreshed from the owned source snapshot:
the four changed rows reflect removal of old generated Tree/sync bodies and
their direct calls, not removal of language functionality. Deferred APIs are
unchanged; snapshot gates and all three matrix-reference unit tests pass.

### Remaining release boundaries

Linux terminal TLS/safe-unload support, the remaining resource domains and full
53-module acceptance are still required. The narrow synchronization API/VFS,
actual-ELF and stack gates do not certify these remaining contracts.
Family absence in a DLL export table is **not** pruning proof; the accepted
family proof above comes from actual TCC object tables. Some older diagnostic
tools use a narrow `SourceProductAcceptance` flag for their specific provider
proof; it does not imply shipping, full runtime instrumentation or full53
acceptance. Main-directory release binaries have not been promoted by this
batch. Candidate acceptance for the remaining domains
and broad product regression must precede promoting new release binaries.
