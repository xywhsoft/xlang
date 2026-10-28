# Embedded Compiler Delivery Closure

This ledger tracks the five requested delivery items, not optional package
unloading, sandboxing, public mutable globals or finer private-code pruning.
Existing concurrent changes must be preserved; source/build provenance and
runtime results are recorded separately.

| Item | Status | Completion Evidence |
| --- | --- | --- |
| Integrate current parallel compiler changes | r11 verified; later integration pending | Method-owner merge `88979d7bf` is included; active HTTP/SDK and IDE changes were not overwritten |
| Repair existing DLL example and Debug EXE failures | Verified | Ordinary Windows/Linux products, codec/bytes/time examples and debug native dependency plan |
| Complete CLI, DLL and interactive GUI acceptance | Verified for r11 | Windows 18-stage runtime/API matrix, both-platform package gates and real GUI keyboard interaction |
| Commit task changes and update delivery artifacts | Coherent candidate; shared promotion deferred | Feature/fix commits, matched DLL/CLI/GUI/SDK candidate and independent candidate smoke tests |
| Linux, sanitizer and full regression acceptance | Focused gates passed; full regression not passed | Full-source sanitizer and export gates passed; real global failures remain recorded below |

Failures remain failures until a passing rerun is recorded. A partial suite or a
source-only cohort cannot close the full regression or product-delivery items.

## Verified Work And Remaining Gates

- Feature commit: `549d6c246`; registry, compiler metadata, explicit per-package
  import and source-free host deployment are included. Concurrent changes were
  not staged wholesale.
- Fix commit: `2ea07f56e`; generated TCC bridges, explicit low-level Context
  lifetime, resident Context/Handler policies, GUI capture/UTF-8 fixes, CLI
  acceptance and Linux VFS/header staging are included.
- Windows r11: fresh ordinary CLI/DLL build; 18 runtime acceptance stages; 24
  compiled-package stages; 21-run GUI self-test. Six real keyboard/UI phases
  covered C/xlang, Chinese output, stderr, compile/runtime errors and recovery.
  Screenshots remain unavailable; accessibility evidence is not visual proof.
- Linux r11: fresh ordinary native CLI and both libraries; 24 shipping package
  stages; actual C and xlang embedded execution and standalone ELF programs.
- Each Windows DLL and Linux SO resolved all 3,910 XRT functions enumerated by
  the matched authoritative declarations and generated symbol table. This proves
  export presence, not exhaustive semantic testing of every function.
- Separate Linux source ASan/UBSan acceptance built 1,298 provider translation
  units and passed registry/IR/decoder/failure-prefix tests plus actual emitted
  TCC bridge C built by GCC O0/O2. The Windows O2 source cohort has 1,299 units.
- Windows r11 sample package exposure added 53,760 bytes (0.2872%); this is a
  controlled sample, not a universal size or zero-overhead guarantee.

## Global Regression Repairs

- `tools/check_delivery_case_matrix.py` executes 254 formerly unwired cases:
  157 successful C emissions and 97 exact nonzero semantic/checked rejections.
  Lex/parse failures, crashes, missing output and empty diagnostics do not count.
  This is compiler coverage, not runtime acceptance of all positive fixtures.
- `tools/check_xrt_exports.py` and `test/cases/xrt_exports_all.c` check both real
  libraries and a missing-symbol negative control; Windows and Linux entries
  invoke the checker.
- `tools/wiring_script_references.ps1` and `tools/wiring_python_literals.py`
  follow real runner references and finite literal tables without executing
  runners. Tests cover scopes, unbounded arguments, comments, root escapes,
  unquoted batch/package references and excluded generated hash reports.
- Tool wiring passed for 467 check scripts and 74 reviewed XLIR scenario
  workers. C ownership/wiring passed for 1,662 entry fixtures and 1,328 package
  fixtures. A wired file is not automatically a passed execution.
- `tools/check_delivery_native_fixtures.py` passed 76 actual native stages:
  compiler mutation contracts, ChaCha20 OOM, TLS, local network shutdown,
  package probes, three HTTP buffer transfers, retained Expat alternative
  syntax and independent BSON/CBOR/MessagePack encoder interoperability with
  malformed-input controls. Test dependencies are pinned in
  `test/requirements-delivery.txt`.
- `test/suites/09_delivery_supplement.bat` connects these gates and the previously
  missing extension workers. C ownership, native compiler contract, Color and
  foreign bridge workers passed. CSV, identity and protocol failures remain
  failures; a successful native-only subtest does not certify a language binding.
- The independent encoder lane uses Python 3.12 and the pinned test-only
  dependencies: `python -m pip install -r test/requirements-delivery.txt`.
  These encoders are not compiler/runtime dependencies and are not bundled.
- `tools/compiler_response.py` and the existing PowerShell compiler-response
  helper avoid Windows command-line limits. Real GCC tests cover more than
  32,767 characters, spaces/quotes/backslashes, invalid arguments and refusing
  evidence overwrite. Compiler selection and absolute PS5 paths were repaired.
- `op` standalone C entrypoints are named `delay_native.c` and
  `random_int_native.c`, avoiding accidental same-stem xlang companion loading.
  Both renamed C programs and both xlang programs passed actual execution.
- Generated iterator OOM acceptance distinguishes logical allocation failures
  from cold physical allocator failures and permits successful recovery only
  when the program assertions and cleanup/balance checks pass. Linux text
  acceptance passed 1,001 checks with zero failed groups. Native-product tests
  now review all current closed profiles instead of assuming three old tuples.
- Full-suite SDK includes and the import fixture's actual source dependencies
  were repaired. The complete Windows import/package suite passed.
- The numeric overload regression now compares default 64-bit types against
  explicit 32-bit alternatives instead of declaring duplicate alias overloads.
  The Linux numeric continuation passed 509 checks, including native/managed
  fault prefixes, sanitizers and strict generated-C/JIT/AOT execution.
- Linux Unicode/HTML passed 221 checks after the native probe linked its real
  foreign-error/native-handle dependencies and enabled ONCE. Encoding passed
  471 checks after matching the current closed-native-product diagnostic.
  These continuations are not a passing full-entry result.
- Test case wiring passed for 3,713 automated assets, one manual probe and 52
  prefixes. The regression-wiring repair is committed as `fe80cbb57`.

## Candidate And Evidence

Matched Windows candidate:
`build/delivery/embedded-20261028-r11a/`.
Its `candidate.json` records products, SDK hashes and 11 independent checks:
CLI help, xlang/C execution, EXE build/run, both C API clients, GUI self-test
and all public XRT exports. `Complete=true` applies to this candidate smoke,
while `FullRegressionAcceptance=false` and `SharedReleasePromoted=false` are
explicit. The source-free host example contains only `host.exe` and
`external.xl`; its build-time package is not needed at deployment.

Detailed local evidence root:
`D:/GIT/_codex_verify/embedded_delivery_20261028_r11/`.
Ordinary builds, `windows-runtime`, `packages-shipping`, `exports-all`,
`gui-interaction.json`, `global-regression/native-fixtures-r4` and the
`linux-evidence` subdirectories retain separate results and failed attempts.

The candidate SDK is deliberately the same r11 SDK as its DLLs. Later shared
declarations include additional HTTP functions; do not combine those headers
with the r11 DLLs or promote just the DLLs over an active shared SDK update.

## Required Remaining Work

The user confirmed that the collection/object ABI, xssh and native-handle work
is still running in parallel. Those implementation files and the active
frontend test shard remain outside this repair commit's ownership.

1. Resolve a real nested-container failure after the concurrent collection/ABI
   work is integrated. This independent reproducer compiles but fails at runtime
   with `TypeError`; CSV and native-sequence identity gates expose this family:

   ```xlang
   array<array<string>> rows = array[]
   rows.append(array["x"])
   assert rows[0][0] == "x"
   ```

2. Resolve xssh AOT teardown: its boundary program prints its success marker but
   exits nonzero with an invalid-state runtime error. The active xssh/native
   handle policy files were not changed by this regression-wiring work.
3. Refresh the still-concurrently-edited frontend test shard: it requires old
   `static char* xlir_fn_` output after native strings moved to their explicit
   ABI. The emitted program and the old textual check are different gates.
   The Linux scalar continuation also stopped at
   `xlir_integer_conversion_source_abi.xl`: its exact signed-conversion error
   text assertion no longer matches the tested runtime. No conversion
   implementation or assertion was changed while ABI work remains active.
4. Finish Linux full-regression reruns after pure test-contract repairs, then
   freeze the current merged tree, rebuild a matched SDK/product cohort and
   rerun the complete Windows/Linux entries before shared release promotion.

TCC's low-level Context retains libtcc's one-relocation contract: call either
`run` once, or `relocate` once and use `getSymbol`. Use `TccHandler` for repeated
main calls. Native symbol addresses and callback userdata remain borrowed;
owners must outlive the compiler and every call. Native C execution is not a
sandbox. Provider ASan coverage does not instrument the released TCC core or
machine code emitted by TCC; generated-C GCC sanitizer tests are reported
separately.
