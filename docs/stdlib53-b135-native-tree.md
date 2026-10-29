# B135: resident Tree migration (not sealed yet)

Historical checkpoint: rebuilt products/VFS and held-sync recovery subsequently
advanced in B143/B144. [B145](stdlib53-b145-container-graph-shipping.md) closes
the actual shipping Tree/Queue cyclic-graph and Queue continuation evidence
with both DLL/SO products, without provider overrides. The pending independent
AOT graph, public-surface cleanup and full53 boundaries remain explicit there;
the development-only reports and requirements below are not current release proof.

This is a development checkpoint, not full 53-module or shipping-DLL acceptance.
The migration remains uncommitted until its complete
language surface and retirement contracts are closed. Independently completed
core and diagnostic-test batches are committed separately, without sealing it.

## Implemented boundary

- XLIR owns canonical key/value Array-schema identities, ordering and transfer
  ABIs, complete nominal admission sets, effects and checked failure edges.
  Explicit and default/class-field construction both preserve those sets.
- Cgen renders the closed layout and direct resident-provider calls. The old
  generated `xlir_avl_*` owner, methods and error-discard helpers are removed.
- `xl6_tree.c` uses actual XRT typed-tree storage. Its owner is operation-pinned,
  rejects concurrent/reentrant access, traces each actual retained value slot,
  and participates in phased ownership retirement. Immutable admission metadata
  is retained independently of the temporary construction witnesses.
- Missing keys, empty endpoints and inequality are data results, independent of
  the operation's failed foreign-error scope, including failures with no allocated
  diagnostic. Endpoint copies keep both initialized outputs unchanged before
  publication failure. Invalid overlapping outputs retain the prior contract.
- Explicit Tree equality compares retained references through the existing
  resident value-equality contract. It does not invent a total order or Hash
  callback for retained physical slots.

## Completed evidence

Native providers: `stdlib53-b135-native-win3`, `...-linux3` and `...-linux-san3`,
each with `tree.native.json` declaring `Complete=true`, `InputsUnchanged=true`.
Each report contains 80 phases, eight complete allocation-failure prefixes,
and 1026 rejected mutations of real source-derived XLIR. O0/O2 compile the new
native Tree body as well as the host. Actual repeated slot edges, shared handle
aliases, reentry rejection and callback release of the last outside shell are
checked with allocation/free/live-byte/invalid-free/double-free/use-after-free
accounting. The sanitizer lane instruments the Tree body and host, **not the
whole resident runtime**.

Windows current-source product `stdlib53-b135-source-win5` stages the declared
private headers and 568 runtime source/header files. The following actual
language fixtures pass public JIT and strict generated-C O0/O2 AOT, including
module init/main/fini memory balance:

- `xlir_avl_resource_abi.xl`;
- `stdlib_tree_native_projection.xl` (NUL/UTF-8, nullable char, nominal values,
  nested Array snapshots and queue transfers);
- `stdlib_tree_native_reference.xl` (the previous shared-class equality defect,
  class-field defaults, clone/merge and missing-result output preservation).

The Windows C/AOT report is `stdlib53-b135-language-win5/tree.language.json`:
21 phases, `Complete=true`, `InputsUnchanged=true`, deterministic C and closed
key/value schema identities. Intermediate failing reports are retained separately.
They exposed missing nominal-construction validation, the class-default producer
path, a private-header package row and a missing converter prototype. In the last
case TCC's implicit `int` return truncated a pointer; the emitted declaration,
not a permissive compiler flag or cast, fixes the defect.

Linux fifth-source-product C/AOT is also complete: the report in
`stdlib53-b135-language-linux5/tree.language.json` has 21 phases,
`Complete=true`, `InputsUnchanged=true`. Actual Linux public CLI JIT passes all
three fixtures; logs are in `stdlib53-b135-tree-claim-finish-dev2`.

## Subsequent retirement fixes and actual language graphs

Actual public compilation exposed two resident retirement defects:

- A claim reserves the owner for retirement; it must not prevent a language
  finalizer from observing intact Tree fields before Clear. The access boundary
  is BUSY/cleared, matching resident Sequence/queue owners, not claim alone.
- Successful physical Finish must return its own completion status, independently
  of a failed scope inherited from the caller. The caller's primary error is
  preserved without requesting a second storage retirement.

The new native phased test verifies access after Claim, frozen Clear, preserved
primary error and repeatable successful Finish. Fresh native reports in
`stdlib53-b135-native-win-dev6`, `...-linux-dev6` and `...-linux-san-dev6` are all
`Complete=true`, `InputsUnchanged=true`: 80 phases, eight complete allocation-
failure prefixes and 1026 rejected real XLIR mutations per lane. Native body
and host compile at O0/O2; only those two are instrumented in the sanitizer lane.
These reports use explicit provider/host overrides against immutable product5
(`SourceProductAcceptance=false`), not the earlier native3 inputs or a rebuilt
shipping product.

`stdlib53-b135-tree-ownership-win-dev2/tree.ownership.json` and the corresponding
`...-linux-dev2` report both declare `Complete=true`, `InputsUnchanged=true`:
38 phases / 32 modes per platform, 12 measured actual compile/JIT/retire lifetimes
per mode (768 measured lifetimes across both platforms). They cover self cycles,
duplicate edges, Array-valued cycles, aliased fields and cloned owners, with host
roots retaining the class, its Tree or an independent clone. All finalizers run
once with intact fields and live module text; allocation/free/live-byte and
invalid/double/use-after-free counts balance.

These graph reports explicitly use a new resident-provider override against the
immutable fifth compiler island. `SourceProductAcceptance=false`: they are
diagnostic proof of the provider fixes, not proof that a newly rebuilt shipping
or source product contains them. No generated finalizer/resolver is replaced.

## Subsequent recursive Tree/Queue domains (diagnostic evidence)

The closed resident reference recipe now includes Tree and each independently
selected SPSC/MPSC/MPMC Queue. Canonical construction witnesses supply immutable
owned metadata only; Queue capacity, mutable storage and language callbacks are
not part of an admission descriptor. Each genuine family checks its private
handle provenance before dereferencing a pointer, plus the complete child
schema identity/layout/owned policy. Mixed Array/Stack/Tree/Queue recipes share
the same bounded depth budget. No IDs-only, `ANY` or generated-owner fallback
has been added to cover missing domains.

Reports below are under `build/test`, use explicit provider/compiler object
overrides, and declare `SourceProductAcceptance=false` and
`ShippingLibraryAcceptance=false`. The immutable sixth Windows source product
(239 TUs / 570 staged files) is preserved: it predates the corrected depth-scoped
Cgen schema locals and the explicit Tree factory declaration. It is not counted
as acceptance of the latest code.

- Tree plus the original 16 Sequence domains: `stdlib53-b135-composite-win-dev1`,
  `...-linux-dev1`, `...-linux-san-dev1`, each 942 phases / four complete OOM
  prefixes (`tree.admission.json`). O0/O2 compile the actual three providers and
  hosts; the sanitizer lane instruments only those bodies and hosts.
- Recursive Queue admission: `stdlib53-b135-queue-admission-win-dev2`,
  `...-linux-dev2`, `...-linux-san-dev2`, each 868 phases / eight complete OOM
  prefixes (`queue.reference.admission.json`). All nine provider bodies compile
  at O0/O2, including the independent three Queue admission TUs; only those and
  the hosts are instrumented in the sanitizer lane. The corrected host is an
  explicit hashed override, and the earlier failed reset assumption is retained
  in a separate report. Nonempty Queue reset remains a refused operation.
- Actual language resource domains: Windows
  `stdlib53-b135-resource-domains-win-dev2/resource.domains.json` has 24 phases;
  Linux `...-linux-dev1/resource.domains.linux.json` has 20 phases. Both execute
  `stdlib_tree_resource_values.xl` and `stdlib_tree_queue_values.xl` through real
  JIT and strict generated-C O0/O2 AOT with balanced init/main/fini lifetimes.
  Each platform refuses 256 mutations of real source-derived IR, preserving
  pre-existing C output and clearing failed artifact plans. The Queue fixture
  selects exactly all three recursive admission components; the Windows plain
  SPSC fixture selects none.
- Original Queue kernel regression: Linux
  `stdlib53-b135-queue-legacy-linux-dev1/results.json` has 277 phases / 45 complete
  fault prefixes: O0/O2, provider/host ASan-UBSan, real concurrent producers and
  consumers, active pins, Future graphs, cleanup failures and native-symbol
  pruning. This does not certify every remaining Queue migration contract.

All of the listed reports finish with `Complete=true`, `InputsUnchanged=true`.
The Linux factory failure was an actual missing private-header declaration:
TCC assumed `int`, truncating the returned pointer. Generated C now carries the
precise factory ABI declaration, and the two Linux language fixtures pass JIT.
An initial API diagnostic gate covered external outputs but not JIT. It has now
been moved into a common generated-module helper, narrowly scoped away from
user/vendor C. Fresh diagnostic acceptance in
`stdlib53-b135-declaration-gate-linux-dev3` (11 phases) and
`...-win-dev4` (14 phases) proves real JIT/external refusal before native factory
execution, preserved external output and successful user-C policy restoration.
Windows also covers module relocation and DLL output with a valid library
fixture. `stdlib53-b135-gate-regression-win-dev1` and its Linux counterpart each
run 14 existing public-JIT fixtures successfully. This independently completed
API/helper/test batch is committed separately; the native migration is not.
See `generated-c-declaration-gate.md` for scope and required bundled-VFS/final
source-product acceptance. No earlier report is changed to claim a later fix.
The previous Linux resource-domain report explicitly records
`StrictApiNegativeControlAcceptance=false` and is not retroactively promoted.

Subsequent independently closed Queue core probes are documented in
`stdlib53-b135-queue-domain-probes.md` and committed separately from this
migration. `stdlib53-b135-queue-busy-win-dev3`, `...-linux-dev3` and
`...-linux-san-dev3` each finish ten phases with unchanged inputs: actual native
operation pins, independently equivalent/wrong full policies, wrong family and
canonical identity, allocation/diagnostic transparency, and balanced release of
the last external shell during a paused operation. Only provider bodies/hosts
are instrumented; no generated callback is introduced into a production policy.

`stdlib53-b135-queue-family-win-dev2/queue.family.contract.json` and its Linux
counterpart each complete 39 phases with unchanged inputs. Source-derived
artifact components, provider source rows and selected XRT module masks are
exactly 1/2/4 for the three independent recursive families; plain SPSC has core
mask 1 and admission mask 0. Sixteen actual IR mutations per family and level
are refused (96 per platform), preserving output and clearing failed plans.
The three single-family NUL/UTF-8 fixtures pass deterministic generated C,
public JIT and strict `-Wshadow` O0/O2 AOT with balanced module lifetimes.
`StandaloneLinkPruningAcceptance=false`: these AOT hosts link a complete
diagnostic compiler/provider island. Actual standalone object/link-map pruning
and final source/shipping products are still required; plan masks alone are not
proof of a minimum-sized executable.

## Independent standalone products

`stdlib53-b135-queue-standalone-win-dev2/queue.standalone.json` and
`stdlib53-b135-queue-standalone-linux-dev3/queue.standalone.json` each finish
117 phases with `Complete=true`, `InputsUnchanged=true`. Each platform executes
four actual public-TCC executable outputs: the three single-family recursive
Tree/Queue fixtures and plain SPSC. The public API saves the exact executable
C and selected XRT implementation; a separate observer derives the genuine
executable artifact plan from parsed, checked and verified/lowered XLIR.

At O0 and O2, GCC compiles and links only that plan's resident provider source
rows plus the saved module/XRT source. No diagnostic compiler objects or xlang
shared libraries are linked into these eight standalone programs per platform.
Every product executes successfully. Defined-symbol and dependency inspection
proves exactly the selected language/typed queue family; plain SPSC has no
recursive Queue admission factory. Native provider bodies compile under
`-std=c11 -Wall -Wextra -Werror` without warning exclusions; the saved generated
module additionally uses `-Wshadow` with the established generated-C exclusions.

Typed MPSC legitimately needs raw MPMC code for its free-slot ring: multiple
producers take/recycle slots, including failed-publication rollback. The audited
kernel sets are SPSC -> raw SPSC, MPSC -> raw MPSC + MPMC, MPMC -> raw MPMC.
Neither extra language queue owners nor another typed queue implementation is
accepted. Removing the actual MPSC free-ring dependency to satisfy a substring
test would break the implementation, rather than improve pruning.

The reports declare `StandaloneGccLinkPruningAcceptance=true` and
`TccStandaloneRunAcceptance=true`. `TccSymbolPruningAcceptance=false`: ordinary
public TCC ELF/PE outputs lack a symbol table, so execution and sizes alone are
not promoted into proof of exact TCC binary symbol contents. The fresh compiler
hosts still use explicitly recorded object overrides; rebuilt source/shipping
and full-53 acceptance remain false. Earlier failed probe/staging/assertion
reports are preserved and do not count as successful evidence.

Reproduce from `demo6` with `tools/check_stdlib_queue_standalone.py` and explicit
`--root`, `--schema-root`, `--compiler`, `--out` arguments. Choose a fresh output
directory. Windows uses read-only directory junction inputs; Linux uses read-only
directory symlink inputs. The genuine API path provider is separately compiled
to locate that package; no test resolver/factory or generated-module replacement
is introduced. All new outputs/temporary files reside on D, not the WSL disk.

## Required before sealing

Resident Sem native and diagnostic-language progress is recorded separately in
`stdlib53-b135-semaphore.md`. Latest Windows/Linux language lanes each pass
67 phases, including genuine checked-entry allocation prefixes. This does not
seal this migration or resolve the legacy held-mutex/rwlock cleanup abort.

1. Freeze the complete batch and rebuild current source products with both
   retirement fixes; repeat native, C/AOT/JIT and ownership evidence there.
2. Exercise continuation handoff and language checked-failure/OOM. Existing
   native edge/diagnostic graph probes do not substitute for these contracts.
3. Close all previously admitted resource-valued Tree/Array/Stack domains.
   Recursive Array/Stack/Tree and the three Queue families now have closed
   resident recipes. Thread/Channel/WaitQueue/synchronization resources,
   dynamic list/set/dictionary domains and the other native-handle domains
   remain pending: require genuine resident ops and complete owned metadata,
   not nominal IDs, an ANY policy or a legacy generated-owner fallback.
4. Verify component pruning and every shipping build/package source list. Current
   Windows evidence is a source-product build, not rebuilt release DLL/VFS. Do
   not claim method-level minimum size from component-level selection.
5. Isolate only this batch's edits when committing mixed workspace
   files; preserve unrelated changes.

The WSL Ubuntu distribution now resides at `E:\WSL\Ubuntu`. Do not terminate it
for compaction while other user tasks are running. C has about 50 GiB free; E
has about 1.2 GiB free. New diagnostic builds and reports are on D (about 21.9
GiB free), not additional Linux snapshots on E. No WSL shutdown is needed.
