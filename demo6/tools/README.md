# demo6/tools

This directory contains DEMO6 toolchain setup scripts and engineering guard checks.

## Toolchain

- `xl6_toolchain_env.bat`: centralizes TCC, optional Flex/Lemon regeneration tools, XRT, include, lib, and default link settings.
- `bootstrap_tcc.bat`: builds `build\tmp\tcc_bootstrap.exe` from `lib\tcc` with MinGW `gcc` when no `XL6_TCC` override exists.
- `build_xrt_obj.bat`: builds the default runtime object from `lib\xruntime\xruntime.h`; the product contains its exact XRT dependency closure.
- `sync_xrt_snapshot.py`: validates XRT/xruntime maturity, synchronizes both product headers, and regenerates deterministic product metadata, the complete TCC symbol table, and the language capability matrix.
- `gen_xrt_xlang_matrix.py`: validates all low-level runtime symbols, generates the module capability matrix, and maintains the exact xhttp/xws/xregex migration boundary.
- `check_xrt_snapshot.ps1 -CheckIndex`: additionally checks all four staged header assets against staged metadata using raw Git blob bytes, detecting line-ending normalization mismatches before commit.
- `generate_stdlib_index.py`: cross-platform standard-library hash-index generator used by the Linux build.
- `gen_tcc_vfs_resources.py`: cross-platform LZMA VFS resource generator; its output is kept byte-identical with the PowerShell generator.
- `tcc_vfs_manifest_linux_runtime.txt`: minimal Linux runtime VFS resource list for `libxl.so`.
- `tcc_vfs_manifest_linux_xlc.txt`: Linux compiler VFS resource list for `libxlc.so`.
- `clear_release_tmp_files.ps1`: removes transient compiler-run output beneath the release product tree with bounded retries so cleanup fails hard instead of continuing after file-lock errors.
- `clear_test_artifacts.ps1`: removes the `build\test` regression tree through an exact-path safety boundary.
- `run_xlir_guards.ps1`: discovers specialized XLIR semantic guards and supplements the explicit full-regression or RC0 entries without running those entries twice. Standalone guards run directly; workers with mandatory inputs are delegated to the reviewed scenario runner.
- `run_xlir_parameterized_guards.ps1`: builds one current-source public compiler, one runtime-only compiler and one shared contract-driver set, then executes every parameterized XLIR worker through dependency-ordered immutable batch reports.
- `rc0_guard.ps1`: lightweight release-candidate guard for required files, static boundaries, build, command smoke, diagnostics, docs, risk ledger, cgen size, and wiring.

## Guard scripts and inventory

B66 explicit cleanup gate: `check_stdlib_sequence_clear.py --out <fresh-directory>`
requires `--public-directory` on Windows (first stage that directory with
`stage_xlir_artifact_runtime_sources.ps1`) and supports Linux `--sanitize`.
It checks native typed/erased Array and Stack first-error cleanup, pending-error
priority, complete reverse Drop, reentry refusal, allocation-free clearing and
last-owner callbacks. Minimal/synchronous/async/generator language fixtures
verify source-void/private-bool IR, malformed-contract refusal, strict O0/O2 C,
JIT and AOT. Linux instruments the selected native closure; shared-language
runtime and CLI/JIT are not instrumented. The regular Stack native gates execute
it as an independently frozen child. This is not language Array migration or
whole 53-module acceptance.

B18 time gates: `check_stdlib_time_core.ps1`, `check_stdlib_time_calendar.ps1`,
and `check_stdlib_time_text.ps1` require one immutable current-source
PublicSourceDirectory and separate fresh Directory values. The full Windows
stdlib suite builds the compiler once and executes all three gates. Linux
`check_stdlib_time_calendar.sh <fresh-directory>` covers core/calendar/snapshots,
process-local New York and Lord Howe DST oracles, and seven actual sanitized
native closures. Shared CLI/JIT libraries are not sanitized; their allocation
ledger does not measure physical allocator caches. `check_stdlib_time_text.sh`
adds exact pattern/protocol/DateTime text, native diagnostic OOM, closed bool/time
ABI mutations, and eight actual sanitized program closures. Each gate freezes
its input hashes; only a results.json with Complete=true denotes an accepted
run. These subsets do not replace the 53-module cross-group acceptance.

B19 environment gates: `check_stdlib_environment.ps1` shares that immutable
PublicSourceDirectory and uses a fresh Directory; the full Windows stdlib suite
reuses its existing compiler build. `check_stdlib_environment.sh <fresh-directory>`
tests exact inputs, nullable versus empty values, four closed typed bridges,
real native OOM and concurrent whole snapshots. Its three actual generated
program/native/resident closures use ASan/UBSan/leak detection. External native
writers require synchronization; libc/OS allocations are outside XRT failpoints.

B20a signal scalar gates: `check_stdlib_signal_core.ps1` shares the immutable
PublicSourceDirectory and needs a fresh Directory; Linux uses
`check_stdlib_signal_core.sh <fresh-directory>`. They cover eleven checked
bridges, uint64 totals, enum narrowing, owned names, real native OOM and
restoration of original dispositions. Native watch witnesses verify waiting
for callback retirement and dispatcher restart; only ignored/watched INT is
raised in private processes. Linux sanitizes the native adapter and two actual
generated-program/native/resident closures. This does not accept xlang signal
callbacks, channel/cancellation routes or the full signal/53-module surface.

B20b explicit signal pump gates: `check_stdlib_signal_queue.ps1` uses the same
frozen PublicSourceDirectory and a fresh Directory; Linux uses
`check_stdlib_signal_queue.sh <fresh-directory>`. Native tests exercise bounded
coalescing/saturation, allocation-free recording, transactional snapshots,
owner guards, constructor OOM, once/close retirement races, count conservation,
absolute deadlines and global disposition waits. Language cases verify
owner-thread-only dispatch, both on/once overloads, closure lifetime and errors,
retained class messages, channel backpressure and the TaskGroup cancel entry.
Linux sanitizes actual generated/native/resident closures, not shared CLI/JIT.
This does not accept the full TaskGroup/channel surface or the 53-module goal.

Parser source-differential mode additionally requires
BaselineGeneratedParser, so the old API adapter is never linked to current
generated parser tables and mistaken for an independent baseline.

The files matching `tools/check_*.ps1` are the canonical and exhaustive guard inventory. `check_tool_wiring.ps1` discovers that inventory directly and requires every script to be wired into the full regression and the RC0 guard (or an explicit full-regression-only exception). This README records the stable toolchain entry points and representative cross-cutting guards; it does not duplicate every specialized XLIR semantic gate name.

Representative and cross-cutting guards include:

- `check_async_task_soak.ps1`
- `check_batch_wiring.ps1`
- `check_c_fixture_wiring.ps1`
- `check_c_string_safety.ps1`
- `check_cgen_module_size.ps1`
- `check_cgen_statement_expr_allowlist.ps1`
- `check_cgenrule_docs.ps1`
- `check_cli_temp_cleanup.ps1`
- `check_debugger_not_released.ps1`
- `check_dev_projects.ps1`
- `check_dll_api_boundary.ps1`
- `check_docs_index.ps1`
- `check_example_wiring.ps1`
- `check_extlib_xregex.ps1`
- `check_extlib_xregex.sh`
- `check_extlib_mqtt.ps1`
- `check_format_extlibs.ps1`
- `check_frontend_fuzz.ps1`
- `check_generated_c_warnings.ps1`
- `check_markdown_links.ps1`
- `check_negative_diagnostics.ps1`
- `check_package_fixtures.ps1`
- `check_package_xrt_symbols.ps1`
- `check_release_extlibs.ps1`
- `check_release_tmp_artifacts.ps1`
- `check_risk_ledger.ps1`
- `check_source_encoding.ps1`
- `check_source_wiring.ps1`
- `check_xlir_contract.ps1`
- `check_xlir_cgen_determinism.ps1`
- `check_xlir_cgen_runtime_matrix.ps1`
- `check_xlir_intrinsic_calls.ps1`
- `check_xlir_collection_member_intrinsics.ps1`
- `check_xlir_dynamic_collection_methods.ps1`
- `check_xlir_migration_coverage.ps1`
- `check_xlir_default_backend_boundary.ps1`
- `check_standalone_compiler_vfs.ps1`
- `check_stdlib_coverage.ps1`
- `check_stdlib_dll_symbols.ps1`
- `check_stdlib_examples.ps1`
- `check_stdlib_test_wiring.ps1`
- `check_tcc_export_environment.ps1`
- `check_tcc_vfs_resources.ps1`
- `check_tcc_xrt_contract.ps1`
- `check_test_case_wiring.ps1`
- `check_test_data_wiring.ps1`
- `check_tool_wiring.ps1`
- `check_toolchain_paths.ps1`
- `check_xlang_output_markers.ps1`
- `check_xrt_snapshot.ps1`
- `check_xrt_product_trim.ps1`
- `check_xrt_header_abi.ps1`
- `check_xrt_ownership_contract.ps1`
- `check_xruntime_language_bridge.ps1`

`check_async_task_soak.ps1` runs a focused async/task/concurrency soak guard. The default mode covers direct `xl run` entries plus the `cgen_task_spawn.xl` build/run artifact path; use `-Repeat N -IncludeHeavy` before release cuts to include the larger `release\examples\task_api.xl` and `cgen_task_api.xl` paths.

`check_frontend_fuzz.ps1` builds and runs the deterministic lexer/parser mutation harness. CI enables `-Sanitize` on Linux so address and undefined-behavior failures stop the build; the normal Windows suite runs the same corpus without requiring an external sanitizer runtime.

`check_generated_c_warnings.ps1` compiles representative generated C translation units with GCC warning errors for missing returns and incompatible pointer types. It is part of both the full artifact-closure suite and the RC0 guard.

`check_debugger_not_released.ps1` verifies that the incomplete probe/DAP prototype is not advertised or accepted by the first-release CLI. A real debugger must return through a new reviewed design and cannot reuse synthetic stack or variable responses.

`check_release_tmp_artifacts.ps1` validates generated test text and also rejects tracked workspace-only output, control characters in tracked filenames, and unclassified files at the `demo6` root. Build products may remain ignored locally, but they cannot silently enter the source baseline.

`check_extlib_mqtt.ps1` compiles the MQTT C implementation against the fixed XRT snapshot, exercises allocation-failure and loopback TCP boundaries, and repeats the codec and TCP cases through xlang JIT and AOT paths.

`check_xrt_ownership_contract.ps1` prevents the removed XRT object-mode compatibility layer from returning. xlang uses retain, ownership transfer and explicit clone/copy; synchronization primitives provide cross-thread publication and visibility.

`check_linux_embedded_resources.py` compares the actual resource tables loaded
from Linux `libxl.so` and `libxlc.so` with their current manifest inventories and
source bytes, independently decoding LZMA with Python's standard library. It
rejects missing, renamed, stale and length/encoding-invalid resources, verifies
unchanged inputs and writes a fresh report. `test_linux.sh` runs its mutation
tests and product check before integration tests. This is resource freshness
evidence, not a self-hosting, Linux TCC unload or 53-module acceptance claim.

`check_stdlib_text_values.sh` verifies Linux B1-B4 editing/query/binary/glob/
distance, eager split/join and owned lazy iterators. It batches 14 strict C/JIT/
AOT fixtures and 17 rejection cases, determinism, IR mutations, native OOM,
cross-thread iterator boundaries, actual AOT/native-plan equality and sanitized
pruned XRT/resident/program closures. Within a frozen input inventory it compiles
each resident component once and caches identical native units separately for
normal and debug roots; only successful compilations enter the cache. It collects
failed groups before reporting failure. Shared CLI/JIT is not sanitizer-certified;
this gate does not imply all-thread OOM or 53-module acceptance.

`check_xrt_header_abi.ps1` compiles the XRT declaration SDK and xlang private runtime header in all supported include orders. It rejects duplicate declarations, type-layout drift and the historical `str` signedness mismatch.

`check_xrt_product_trim.ps1` builds minimal and typed representative programs, verifies that both use one xruntime implementation product, and rejects duplicate core implementation or unnecessary typed module retention.

`check_xruntime_language_bridge.ps1` fixes the native-container bridge contract: collection algebra and dictionary merge call xruntime directly, storage controls stay available to xlang, and signed bounds are validated before conversion.

## Maintenance

`check_extlib_xregex.ps1` verifies all 49 generated native prototypes, the single
resident regex engine contract, owning matcher/splitter inputs, allocation-failure
cleanup and C/xlang JIT/AOT example parity. `check_extlib_xregex.sh` repeats the
standalone C ownership and OOM tests under Linux ASan/UBSan and leak detection,
and can validate the actual generated prototypes on LP64. The main Linux suite
also runs the package through JIT and independent ELF AOT. These checks do not
certify concurrent mutation of one matcher, untested platforms or performance.

`check_xlir_pending_exception_route.ps1` now runs the full pending-error matrix by default (`-FullOutcome` remains a compatible alias). It covers post-try routing, managed/nested finally, nullable diagnostics and real OOM, return/break/continue overrides, internal catches, and actual await/yield suspension. Explicit pending-error owners, cleanup-preservation causes, cancellation and async/generator error-frame lifetimes are mutation-tested before C/header output; both fixtures have strict-C allocation-balance tests, deterministic C/header and C-host JIT/AOT runs. Cancellation/closed-Future terminal cleanup and other untested continuation combinations are not implied by this matrix.

`check_xlir_return_projection_owner.ps1` checks physical owner reads separately from flow-narrowed observations. Its runtime matrix covers single/product returns, repeated aliases, class widening, boxed scalars, nullable strings, partial evaluation, finally, mutable capture cells and both branches of conditional/coalesce results, with nullable diagnostics, real OOM and cleanup rollback. SSA producer ownership is shared by lowering and continuation analysis; product construction rejects untransferred borrows and duplicate acquired owners. A unique temporary initializer must dominate its read and preserve its acquisition identity; multiple branch initializers require an exact CFG activation and independently owned input on every branch. STORE/MOVE cannot manufacture ownership. The gate includes producer-ABI table checks, mutation rejection before C/header output, strict allocation balance, deterministic C/header, C-host JIT/AOT and the original cgen_var_return_exprs C ABI host. This does not claim a complete global affine-use proof across all local stores and managed SSA consumers.

- `run_release_core_batch.ps1` collects the focused core repair regression without fail-fast. It builds one current-source driver, freezes compiler/runtime hashes, supplies the default-backend worker's required directories and retains each log plus `results.json`. Its 26 checks are not a replacement for full RC0 or all parameterized scenarios.
- `check_stdlib_fs_posix.sh --host-only <fresh-evidence-directory>` runs the same 38 filesystem fixtures through the actual Linux TinyCC JIT and standalone ELF AOT, with isolated fixture data under `work/build/test` and per-case exit codes in `results.tsv`. The default mode separately validates strict GCC-generated C. Prepare binary archive vectors with `check_stdlib_vfs.ps1` first.
- New `check_*.ps1` files must be wired into `test.bat` or one of the `test/suites/*.bat` full-regression suites.
- New `check_*.ps1` files must also be wired into `rc0_guard.ps1`, unless explicitly listed as a full-regression-only exception in `check_tool_wiring.ps1`.
- Do not hand-maintain a second exhaustive filename list in documentation; `check_tool_wiring.ps1` enumerates `tools/check_*.ps1` and validates each discovered script.
- Keep batch scripts ASCII where practical and keep toolchain paths centralized through `xl6_toolchain_env.bat`.
- Synchronize XRT/xruntime with `python tools/sync_xrt_snapshot.py --sync --xrt-root <path>`; never edit either vendored product header or its metadata by hand.
- `build.bat` must run on a clean Windows machine with MinGW in `PATH`.
- `build.sh` dispatches to `build_linux.sh` on Linux and to `build.bat` through `cmd.exe` on Windows.
- Linux builds require GCC, GNU Make and Python 3, but do not require a preinstalled TCC.
- Flex/Lemon are only required when `XL6_REGEN_GRAMMAR=1`.

`check_xlir_scalar_component_kernel.sh` is the Linux B10 scalar/graph gate, wired
into `test_linux.sh`. It requires a current `build_linux.sh` build and a fresh
evidence directory. The gate exercises the production component-set closure
kernel through 257 nodes against an independent Floyd oracle, validates the
actual resident graph and native Future/network IR contracts, and checks safe
signed-minimum constants, corrupted-IR refusal before C/header output, strict
generated C, TinyCC JIT and standalone AOT. ASan/UBSan instrument the generic
set and signed generated-C host, not the linked shared libraries. This is a
scoped compiler foundation batch, not completion of all 53 stdlib modules.

`check_xlir_string_join.ps1` is the B11 frozen current-source string matrix.
The legacy bridges/repeat entry scripts delegate to it; custom mutable validation
drivers are no longer accepted as evidence. It verifies exact view/typed-result
ABIs, ten bad declarations, in-memory char conversion corruption, deterministic
outputs, native thread/OOM oracles, logical allocation failure propagation and
strict C/JIT/AOT. `check_xlir_string_join.sh OUT` now uses the current Linux release
compiler and a fresh evidence directory; it regenerates native outputs on Linux
rather than compiling historical Windows C. ASan/UBSan instrument the selected
native providers; generated libxl hosts use logical accounting. Both require a
separate actual-artifact closure check and do not claim full RC0/53 completion.

B12 extends the same gates with the bounded shared literal scanner, real lexer
positions, nested same-quote interpolation, ten literal/semantic negatives and
10000 real frontend fuzz iterations. `xlir_literal_span_contract.c` additionally
checks 101301 exact-buffer/truncation/lexer cases. The nested fixture covers owned
array/list string reads and compound updates with a complete logical OOM sweep;
IR uses the existing checked owning projection instead of an aborting read.
Linux ASan/UBSan now instrument the scanner and actual lexer/parser/fuzz as well
as selected native providers. Ten actual artifact closures remain independently
verified; this is still a scoped batch, not all 53 modules or whole RC0.

B13 `check_xlir_regex_values.ps1` is a reviewed central scenario with a frozen
PublicSourceDirectory and fresh Directory. Its Linux companion
`check_stdlib_regex.sh` is wired through test_linux.sh. Both cover compile budgets,
immutable independent find/at/full snapshots, exact NUL spans, ABI mutations and
negatives, native/threaded/OOM oracles and managed logical OOM. The artifact gate
independently checks actual AOT native assembly and CORE/MATCH pruning for three
fixtures. Replacement, all/split/set and Pattern are deliberately not claimed.

## Process configuration/run batch

`check_stdlib_process_run.ps1` requires a frozen full PublicSourceDirectory and
fresh Directory. Its Linux companion is wired into test_linux.sh. Both exercise
owned OS-text configuration, exact bytes input/output, independent result/status,
concurrent dual drains, cancellation/deadlines and logical OOM, with closed ABI,
owner and effect mutations/negatives and deterministic strict C/JIT/AOT. Linux
instruments actual XRT/resident/program closures; shared CLI/JIT is uninstrumented.
The POSIX launch-plan prefix sweep verifies every argument-copy failure and
cleanup. Live Process now adds real child ownership/unique stream lease,
parent-drop survival, nullable timeout/independent status, and factory OOM
rollback with explicit private waiter retirement in the native fixture.
Typed Process.waitAsync also covers prompt language cancellation, shared native
source isolation, parent-drop survival and native mapper/pending/terminal OOM
prefixes with explicit waiter retirement. Three generated live paths now sweep
every current-thread logical allocation prefix (native factory, managed boxing,
owned streams/status and parent drop) through the first no-hit. An actual native
Spawn observer joins private waiter tails; it does not mock results or give the
public API a waiter-join guarantee. Other-thread allocations are not injected.
Four generated Process Future paths additionally sweep the whole current-thread
logical allocation prefix: terminal/repeated wait, parent drop and explicit
cancellation. Tests use a typed catch and exact Future state, retire real
children/private waiters and the continuation executor, and require the first
no-hit. Only an observed failure of the optional CANCELLED diagnostic allocation
may preserve successful cancellation instead of MemoryError; no other error is
accepted. `-FutureOnly` (Linux `--future-only` after Directory) runs six Future
language fixtures, four exact ABI negatives and native factory/mapper prefixes;
results explicitly record the subset. Compilation/results are fresh and inputs
frozen; this mode does not claim other process or 53-module total acceptance.
Other-thread injection and Process Future provider-unload acceptance remain pending.
A native pipeline preflight additionally sweeps every calling-thread allocation
prefix for success and last-stage launch failure, with selected modules and
MODULE_ALL. It requires intact unrelated stdin, exact per-stage binary stderr,
collector-owned waiter retirement and balanced allocation ledgers. Linux
instruments both native profiles. The production initialization/first-error/
waiter fixes are recorded in stdlib53 B28a. B28b additionally covers public typed
ProcessPipeline/PipelineResult/ProcessStageResult, owned stage configuration
snapshots, exact binary input/output and independent per-stage stderr/status.
Its generated pipeline sweeps the complete calling-thread allocation prefix,
including native factories and managed boxing, through the first no-hit.
Configuration-only and executing artifacts verify two separate resident
components and actual native pruning; Linux instruments the actual generated
XRT/resident/program closures, including the debug OOM witness. This collector
retires its own waiters; no extra Spawn observer hides a retirement defect.
File redirection/PTY remain later batches;
passing this gate does not complete the seven process modules or all 53 modules.
The same gates cover process.open input/error transport with a controlled
association launcher (never claim OS launch acceptance), a real XRT terminal
capability probe, exact support ABI/effect negatives, and separately pruned
open-only/terminal-only artifacts. PTY sessions are still a later batch.

## Explicit Future producer cancellation

`check_stdlib_process_future_module.ps1` accepts a frozen full current-source
PublicSourceDirectory and fresh Directory; `check_stdlib_process_future_module.sh`
freezes Linux inputs and instruments the actual ELF and source API/XRT/resident
host with ASan/UBSan/leaks. Both verify one Process/two pending awaits, cancellation
isolation, independent exit statuses and an externally retained status pinning
its code after Future release. Windows performs actual FreeLibrary and TCC
delete/MEM_FREE; Linux performs actual dlclose/RTLD_NOLOAD refusal. The native
observer supplies real EOF and joins private waiter tails, not a public wait
promise. TCC's provider allocator/cache is checked separately. The Windows
`-GlobalNativeHandles` option and Linux second argument `globals` select the
owning module-global Command/Process fixture; default selects scoped ownership.
Both test exact resident native Ops+Trace admission, three pre-trace refusals,
invalid policy keeping its input slot, the complete native boxing allocation
prefix and first no-hit, 29 typed/lowered policy mutations rejected before C/H,
and a raw status handle surviving actual unmapping of its generated class code.
These gates do not replace per-artifact pruning, Linux TCC, other native policies
or total 53-module acceptance. The B32 diagnosis remains documented under
`test/fixtures/process_module_pending`; its fixture is now an accepted case in
`test/cases/xlir_process_future_module_globals.xl`.

`check_xlir_future_cancel_core.ps1` requires a frozen full source build and fresh
Directory; `check_xlir_future_cancel_core.sh` freezes the Linux SDK/source inputs.
Both check the actual producer node, outside-lock publication handshake, V1
ownership admission, ambient/OOM, terminal/source/commit/close races and canonical
generated cancellation with catch/finally. GCC O0/O2 and TCC preserve the prior
2106 interruption/300 race population and add 150 explicit public requests/50
provisional-successor cancels. Linux instruments the complete kernel closure;
that is not a per-program pruning, module-unload or 53-module acceptance claim.

`check_stdlib_buffer_values.ps1` verifies the current-source Buffer value/range
adapters and independent byte snapshot iterator through strict C, JIT and AOT,
five private-field/owner/ABI refusals, and actual selected XRT allocation-failure,
thread and allocator reentry checks. It freezes source, staged SDK and object
hashes and preserves prior runs. Legacy Buffer core/codecs, Linux and the total
53-module release boundary remain separate work; this gate is not that proof.

`check_stdlib_buffer_values.sh` runs the same six positive and five negative
fixtures, actual XRT allocation prefixes, strict C/JIT/AOT and instrumented
generated/resident/native closure ASan/UBSan/leak checks on Linux. The legacy
Buffer fixture's authentic C helper explicitly selects the existing full-XRT
input policy; pure-language fixtures verify the actual pruned native plan.
The shared CLI/JIT is not instrumented; legacy core/codecs remain separate work.

`check_xlir_module_product_globals.ps1` and `.sh` freeze current products/source
and run nine positive/five negative module-product/global/capture/print fixtures.
They verify canonical binding/storage/cell identities, original memory-error
propagation, strict C/JIT/AOT and actual selected native/resident artifacts.
Four new fixtures scan every logical allocation prefix through the first no-hit,
with balanced live/count/byte totals and idempotent module cleanup. The debug
probe is a separate C translation unit using actual SDK types, not duplicate
ABI layouts. Linux separately sanitizes real pruned generated/resident/native
closures; its full shared debug XRT OOM harness and CLI/JIT are not instrumented.
This does not certify every product/capture combination or all 53 modules.

`check_stdlib_buffer_codecs.ps1` and `.sh` freeze seven positive/four negative
fixtures for exact Hex/Base64, static/free native factory ABI and legacy Buffer
regressions. They verify actual pruned C/JIT/AOT closures, independent native
vectors and complete logical allocation prefixes, including managed factory
failure cleanup. Linux sanitizes real selected native/resident/generated closures;
shared CLI/JIT and the separate full shared debug-XRT harness are not instrumented.
`check_xlir_buffer_codec_ir.py` checks exact class/field/destructor identities.
Neither gate certifies legacy Buffer core or all 53 modules.

`check_stdlib_buffer_core.ps1` and `.sh` freeze five positive/three negative
fixtures for direct Buffer factories, u8 indexing, copy/fill and byte queries.
They check core/query pruning, exact class/field/destructor identities, native
vectors and all logical allocation prefixes through first no-hit. Failed growth
must preserve data/capacity and the original memory error; cleanup preserves
errors and balances allocation totals. Linux sanitizes the actual selected
generated/resident/native closures, not shared CLI/JIT or full shared debug XRT.
Trusted C pointer escapes and legacy file/network conversions remain separate.
`check_xlir_buffer_core_ir.py` rejects a return to the retired Buffer profile.

`check_stdlib_stack_native.ps1` / `.sh` freeze eighteen Stack positives,
three nullable dependency probes and sixteen negatives against current sources.
They cover actual typed-stack C/JIT/AOT/pruned
artifacts, six exact tuple mutations and parameter lifetime fact mutations,
exact owned/value pop/peek/peekAt result/effect mutations before C output,
bool/optional tryPop/tryPeek/tryPeekAt product-field mutations,
char/bytes TypeId refusal, all byte values, embedded NUL, value write isolation,
nullable elements, retained references, continuation cleanup and reentrant release.
Seven immutable nullable scalar profiles share canonical C carriers, delegate
copy/move/compare/hash to xrttype, ignore inactive payload/padding and select only
used resident descriptors. Literal-fit and width/identity refusal, finally/async/
closure/generator transport, and descriptor-free ordinary values/raw queues are
covered; the AVL probe selects only its used nullable value profile.
Eight native OOM paths and the original/value/owned/try/nullable language fixtures run every
allocation prefix through the first no-hit, preserving the original error and
exact allocation totals. Language allocation injection covers the calling thread,
not every background thread.
The Linux gate sanitizes actual selected generated/resident/native closures;
shared CLI/JIT and full shared debug XRT are not instrumented. Exact direct
owned-return and try/outcome APIs are covered. The three old caller-writeback
methods and their IR/Cgen/native exports are removed: semantic negatives,
generated declarations and actual linked symbol sets guard their retirement.
Batch/FixedStack, other containers and all 53 modules remain pending.

`check_stdlib_stack_ownership.ps1` / `.sh` validate the actual counted native
wrapper and every strong element slot, including duplicate edges, shared
shells, self/mutual cycles, rooted abort, weak quarantine and opaque refusal.
Native-thread probes cover callback tails outside the owning mutation and
cross-thread freezes; real create/refresh/commit OOM prefixes run through the
first no-hit with original MemoryError and exact allocator totals. Linux
sanitizes selected native graph/lifecycle code, not the shared public compiler.
Both platforms execute source/XLIR/JIT/retire cycles through the public API;
Windows additionally measures real TCC MEM_FREE at host O0/O2. The optional
Windows `-Fixture` / Linux second argument selects the exact recorded source;
the core suite also runs `test/fixtures/stdlib53_pending/stack_detached_cycle.xl`,
the original detached class/Stack code-lease-island regression. V4 discovers
physical borrowed backing anchors before every semantic duty and Commit,
never subtracting them as owning slots. Actual selected Class/Stack/code-lease
vectors cover foreign owners, original shell retirement before a copied island,
alias/deep-copy rooted abort, new finalizer islands, peer resurrection, and
the complete allocation prefixes through first no-hit. A borrowed finalizing
receiver itself remains non-retainable under the existing XRT contract. This is
an ownership prerequisite; direct owned-return APIs are separately covered by
the native Stack gate. This does not complete legacy Into retirement,
try/outcome products, batch/FixedStack or the remaining 53-module acceptance. Class-family
discovery does not certify arbitrary policies, other families of disconnected
cycles or every possible cross-module owning graph.

`check_stdlib_value_admission_native.py --out <fresh-directory>` validates the
data-only boxed language element domains: signed/narrow unsigned bounds,
lossless f32, exact char/string/bytes/time, nullable presence and bound nominal
identity refusal. It checks allocation-free/error-transparent predicates,
status/output projections, consuming collection validation, destructive cleanup
error preservation and every actual allocation prefix at O0/O2. Linux
`--sanitize` instruments its complete selected resident/native closure with
ASan/UBSan/leaks; the symbol check excludes Array/Stack and unrelated facades.

`check_stdlib_collection_admission.py --out <fresh-directory>` checks two
language fixtures through deterministic IR/C, strict O0/O2 C, JIT and AOT.
Windows requires `--public-directory <full-current-source-driver>` and the
driver's `stage_xlir_artifact_runtime_sources.ps1` deployment first. The gate
matches emitted rule/nullable/identity arguments to verified IR type facts,
checks narrow integer and Xid-as-bytes refusal, exact nullable/NUL values and
bytes read/write independence. Its strict C links the real shared runtime;
Linux `--sanitize` instruments generated C and the host, NOT those shared
libraries. Commands have a 60-second deadline; only the measured O2 sanitizer
compiler step has a 180-second budget, with timings and timeout failures saved.
`check_xlir_artifact_runtime.ps1` independently verifies selected
native closure pruning. None of these gates certifies language Array's complete
native-carrier migration, recursive element admission or all 53 modules.

Both admission gates also support `--nominal`. The native gate selects the
independent class/protocol identity component (without the primitive facade)
and verifies inheritance, closed witness tags, nullable presence, exact bound
non-object identity and invalid schemas with allocation/error-transparent
predicates and complete consuming-validation failure prefixes. Its identity
schema is borrowed for a synchronous call, NOT retained as an Array owner policy.
The language gate checks all four erased collection imports, rejection keeping
the destination unchanged, class/protocol dispatch and Xid versus plain bytes.
Both language modes compile a current-compiler mutation host, reject forged
family/set/nullable and unrelated instruction fields before any C/header output,
restore each fact and require canonical C equality. Matching uses resident
predicates and immutable verified identity data, not per-artifact matcher code.

`check_stdlib_array_admission.py --out <fresh-directory>` checks owned native
Array reference-element policies: ANY/bytes or `--nominal` class/protocol/exact
identity domains, distinct nullable-owner/base-value identities, write-time
typed/erased rejection before allocation, clone/cursor policy ownership, bulk
append policy equivalence, invalid physical layouts/schemas and profiled
Array/cursor cycle retirement. Each mode runs strict O0/O2 C and four complete
logical-allocation prefixes per level through the first no-hit, with the full
memory ledger checked. Linux `--sanitize` instruments the selected native and
resident closure, including stack-use-after-return and leaks; actual symbols
exclude the other admission flavor and unrelated/snapshot-only facades.
`--shared` instead checks the actual product ABI (Linux dynamic export table);
it does not certify pruning, and sanitizers instrument only its host.
The ordinary `check_stdlib_array_native.py` now executes both selected-native
modes as independently frozen child gates; the Stack/Sequence ownership parent
therefore exercises them on its existing regular path. No new default library
API, arbitrary C identity unforgeability, recursive element closure or complete
language Array/53-module migration is claimed.

`check_stdlib_array_defaults.py --out <fresh-directory>` checks the separate
resident boxed-array resize/default closure, not native Sequence migration.
It verifies all nine value tags, aliases/snapshots, refusal before allocation,
unchanged contents/count/capacity on validation/allocation failure, and strict
O0/O2 full allocation prefixes. Linux `--sanitize` instruments that closure;
`--shared` instead checks the actual product ABI (host-only instrumentation).
The regular native Array gate executes it as an independently frozen child.
`check_stdlib_collection_admission.py --defaults` adds current-compiler mutation,
canonical C, strict O0/O2, JIT and AOT checks for logical defaults and immediate
may-raise outcomes, including async/generator cleanup and committed-shrink
destructor errors. It also covers exact binary32 boxing without foreign identity.
It does not claim rollback of an already committed shrink after a destructor
error, native language Array migration, or complete 53-module acceptance.

`check_stdlib_array_primitive_admission.py --out <fresh-directory>` checks the
owned scalar/text/nullable native Array policies in eleven independently linked
physical closures. Strict O0/O2 vectors cover exact integer domains, binary32,
Unicode scalars, NUL text, nullable presence, nominal IDs, aliases, cloned policy,
cursor retirement, bulk-policy equivalence, refusal before allocation, pending
error transparency and complete logical allocation prefixes. Symbol checks
require only the selected optional physical factory/codecs. Linux `--sanitize`
instruments the full closure; `--shared` instead tests the actual product ABI
(host-only instrumentation), not pruning. The ordinary native Array gate runs
this independently frozen child on the existing ownership regression path.
Native typed resize now preserves contents/count/capacity and both storage
addresses on allocation/default-initialization failure, matching language boxed
resize. The primitive gate checks these identities through the actual native
owner under full fault prefixes. The canonical xruntime module and single-header
tests additionally exercise fallible defaults, reverse cleanup, primary errors,
allocator reentry and over-alignment; this is not a Cgen compensating adapter.
Neither native language Array migration nor whole 53-module acceptance is claimed
by this gate.

`check_stdlib_array_input.py` also validates the split Future aggregate V2 ABI
and optional native-owned metadata. Its metadata host instruments the resident
metadata/legacy bridge on Linux and checks exact owned UTF-8/NUL names, 1000
physical lifetimes, accepted-thread cooperative cancellation, and fourteen
complete O0/O2 allocation prefixes. Shared XRT remains outside this sanitizer
scope. `xlir_future_metadata_contract.c` rejects typed no-raise/borrow corruption
and missing or merged lowered error outcomes before C/header emission. The
metadata-only product must omit the aggregate kernel, Array adapter and legacy
Future bridge. Observation times are not actual task submission/completion.

`check_stdlib_collection_count.py` closes the ordinary, tagged-nullable and
open-var `.length` consumer contracts. It checks the actual native ARRAY BUSY
pin/domain, wire Array/List/Dict/Set, absence versus type errors, zero-allocation
scalar/present reads and complete owned-box allocation prefixes at O0/O2.
Its leaf declaration header compiles pedantically without XRT/Sequence headers,
runtime libraries or additional private include search paths.
`--language --public-directory <current-source>` adds pre-output IR/outcome
corruption refusal, deterministic C/IR, strict generated C, sync/async/generator,
JIT/AOT, normal module lifetime, and actual generated functions called with
native storage. `--sanitize` is Linux-only: standalone instruments the selected
native closure; `--shared` and language runs instrument host/generated C only.
The Array/Stack parent paths freeze and invoke this child. Independent artifact
checks require only CollectionCount/Sequence, not the Array facade or ArrayInput.
The source Array producer remains value-v1; this gate does not certify the full
language Array carrier migration or the remaining 53-module objective.

`check_stdlib_array_scalar_read.py` validates the four checked I64/U64/bool/F64
Array read consumers. Native reads validate the sealed ARRAY, BUSY state,
logical identities and canonical physical descriptor before copying one slot;
wire reads keep exact tag semantics. Both success paths allocate and box
nothing. The standalone leaf ABI header needs no XRT/Sequence dependencies.
`--language --public-directory <current-source>` adds deterministic C/IR,
pre-output metadata/outcome corruption refusal, strict O0/O2, JIT/AOT, normal
module lifetime and actual generated functions reading native storage. It also
checks loop snapshot cleanup after a failed item projection. Linux sanitizer
instruments the selected standalone native closure; shared/language lanes
instrument host/generated C only. Array/Stack parents freeze and invoke it.
Read-only artifact selection requires ScalarRead/Sequence, not ArrayInput,
Array facade or CollectionCount. This is not a full Array carrier migration or
an allocation-prefix/OOM proof for source construction.

`check_stdlib_sequence_publication.py` exercises the shared resident
creation/publication transaction and domain-preserving Array/Stack clone.
Five real profiles run 100 lifetime/ownership cycles and complete native
create/clone/push/batch/extract allocation-failure prefixes at O0/O2.
Malformed policies remain caller-owned; valid owned policies are consumed
on both paths. Single and batch Stack writes check the actual target domain
before allocation; clone metadata survives retirement of its source owner.
XRT push/append roll back count/content, not necessarily grown capacity.
Linux standalone sanitizes the selected native implementation; `--shared`
uses the real product and sanitizes host code only.

`check_stdlib_sequence_factories.py` builds nine construction-only actual
native closures independently at O0/O2, checking symbol selection and 100
owned-policy publication/clone cycles. They exclude Array/Stack facades,
other factories and `xrtTypedStack` algorithms. Linux `--sanitize` instruments
the entire selected closure. Array/Stack parents freeze and invoke both gates.
These are native construction/domain contracts, not the full source Array
carrier switch, source Stack closed-recipe publication or 53-module acceptance.

`check_stdlib_collection_admission.py --stacks` checks seven source fixtures:
the seven direct Stack recipe families, nested Stack owners, default class
fields, nullable C widths and construction-only owners. IR dumps must match
the exact factory call and nominal set in deterministic C; mutated recipe,
schema, payload, transfer and unrelated metadata fail before C/header output.
The rich fixture closes every O0/O2 language allocation prefix. Strict C,
JIT and AOT execute the real runtime. Linux `--sanitize` instruments generated
C and host code, not shared libraries. Wire Array children fail before C output.
The artifact-runtime gate independently checks both construction-only native
plans: no Stack/Array facade or TypedStack algorithms, exact child descriptor.

The nine construction-only native factories now take an explicit ARRAY/STACK
role. Their gate exercises 100 direct publication/clone cycles per role,
owned-policy retirement, BUSY/error-transparent exact-tuple probes and invalid
roles; Linux standalone instruments the entire selected closure. The nested
Sequence gate covers 16 nullable/required child domains across both roles,
including same-ID published wrong tuples and complete allocation prefixes.
These do not complete source Array migration, pending element domains,
nullable generic-resource member/unwrap semantics or the 53-module goal.

`check_stdlib_collection_admission.py --nullable-resources` verifies flow-observed
Stack receivers, invariant generic resource casts, identity/null/lift/checked
unwrap and producer-based ownership. It covers aliases, guarded storage copies,
nested Stack owners, exact NUL text/bytes, narrow scalar boundaries, closures
and fresh/projected Futures. Five semantic refusals preserve null guards and
linear Future copy restrictions. Cast/effect, receiver and wrap/unwrap mutations
must fail before C/header output. O0/O2, JIT and AOT run the real runtime;
the non-threaded resource/value fixtures close every allocation prefix. Future
normal runs include module retirement, but do not claim worker-thread OOM
injection. Linux sanitizes generated C/host only, not shared libraries. Stack
parent gates include this mode. Native artifact closure checks are independent;
this batch does not complete source Array migration or the 53-module goal.

`check_stdlib_fixed_stack.py` checks the ordinary generic `FixedStack<T>` facade,
canonical repeated product/nullable types, template-only dependent intrinsics
and exact concrete Stack ABI. Mutated admission/result/payload/effect/type rows
must be refused before C/header output. Three actual language programs run
strict O0/O2, JIT/AOT and six complete allocation-prefix sweeps with module
finalization twice. Windows requires a staged current-source public driver;
Linux supports generated-C/host sanitizer flags, not shared-runtime coverage.
Stack parent gates include the new gate. Independent native artifact validation
is separate; the source Array migration and the whole 53-module goal remain open.
