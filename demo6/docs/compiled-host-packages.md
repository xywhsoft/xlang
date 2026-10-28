# Compiled Host Packages

Implementation and focused acceptance contract. Completed evidence is listed
separately below; this is not a full toolchain acceptance report.

## Contract

A host explicitly makes a compiled package available to an XL_Context. Script
source still imports that package. Registration does not publish identifiers
without an import and does not search an executable's operating-system exports.
The compiler, not the application, produces interface and symbol records.

The language surface is:

```xlang
import xlang
import mylib
auto engine = xlang.Context()
assert engine.importPackage(xlang.compiledPackage("mylib"))
assert engine.runFile("scripts/main.xl")
```

compiledPackage requires a constant package name resolved by this compilation.
Only explicitly requested packages and their dependency closure are retained.
Their required callable entries become XLIR reachability roots before pruning.
Ordinary imports do not implicitly make every package callable from scripts.
This first implementation conservatively retains all callable declarations of
the selected packages, including private helpers needed by templates/defaults.
It does not promise minimal private-function retention within an exposed package.

## Representation

An immutable versioned descriptor contains package identity, ABI identity,
bounded interface bytes, native entry records and dependency descriptors.
Interfaces retain declaration/default/ownership/type information. Ordinary
function bodies, comments, debug information, native source and vendor headers
do not belong in the deployed interface. Open generic implementations require
explicit template information; a signature alone cannot instantiate a new T.

Imported ordinary functions reuse the host's verified ABI and implementation.
Binding happens during compilation/linking, not through a boxed opcode router.
Native-backed classes preserve the same native handle and destruction contract.
Unsupported physical boundaries must fail explicitly, never recompile a missing
ordinary implementation or fall back to an unrelated filesystem package.

Ordinary language classes, constructors, properties, typed method values,
callbacks, managed results and multiple results use the same host ABI. Parameter
rebind, read-only borrowing and cleanup effects are frozen from the verified
provider, not inferred from the consumer's empty function bodies. Constructor
initialization is proved in the provider; the consumer retains its required
field obligations and calls that exact verified constructor.

Async and generator entries refer to the provider's existing launchers. Workers,
continuation frames and schedulers are not reconstructed from stub bodies.
Open generic recipes are retained because new script types need executable
template information. Public import aliases and selective imports use the normal
language import rules; private implementation names are not published.

## Registration And Lifetime

Registration is context-local, idempotent for the same identity, transactional
across dependency closure, and rejects conflicting identities or native symbols.
Registered identities take precedence over disk and environment package paths.
They must not alter the process working directory or global package/VFS state.
Context records own only native metadata. Compiled modules must not depend on
their originating context remaining alive.

Bound interfaces never search for companion C files. Their imports resolve only
to registered dependencies or resident feature modules, including when a disk
package with the same name is present. Interface import cycles fail before
recursive expansion. Dependencies include both manifest edges and actual package
imports in the host source.

Identity includes name, version, ABI version, interface bytes, callable addresses
and dependencies. Repeated identical imports are idempotent. Native entry order
does not change identity. Conflicting explicit C registrations, other packages
and the actual default runtime symbol population are rejected before publication;
sharing an identical XRT address is allowed. Failed allocation leaves the old
registry intact and permits a later retry.

Code pointers are residency capabilities, not ownership proofs. A process EXE
is resident for process lifetime. An unloadable provider needs independent code
and state ownership; it cannot be certified by copying an address table. The
existing Context leaf policy must not silently acquire a module or xvalue edge.

Automatic language providers therefore require the ordinary executable entry
deployment. JIT and shared-library providers are rejected. A C application can
supply a resident descriptor, but it must keep both native code and initialized
provider state alive until every consumer module has finished. Import does not
pin or authorize unloading a native provider. A module can outlive its importing
Context because declarations/symbols are consumed during compilation; the native
provider lifetime requirement remains in force.

## Public C API

Both DLLs expose the declarations in `release/include/xl_package.h`:

```c
int xlImportCompiledPackage(XL_Context*, const XL_CompiledPackage*);
const XL_CompiledPackage* xlFindCompiledPackage(XL_Context*, const char*);
const void* xlCompiledPackageRuntimeIdentity(void);
uint64_t xlCompiledPackageInterfaceHash(const void*, size_t);
```

Input metadata and dependency records are copied. Native addresses are borrowed.
The pointer returned by `xlFindCompiledPackage` is const, context-owned and stable
across later registrations, but invalid after Context destruction. Compiler API
clients register through `xlcRuntimeContext(compiler)` before compiling a consumer.
Checking, C emission and relocation remain available. A consumer cannot be emitted
as an independent EXE/DLL: its addresses belong to this process. Raw TCC environment
registration likewise refuses to publish these addresses for external output.

The descriptor validates size, version, pointer width, compiler ABI, exact runtime
identity, bounds, native names and an interface checksum. The checksum is not an
authentication mechanism. Only valid native descriptor pointers may be passed;
this API is not a memory-safety sandbox for hostile native code or unsafe pointers.

## Deliberate Bounds

- One Context has at most 64 packages, 65,536 native entries and 4 MiB of interface
  bytes in total. Each interface has at most 65,536 nodes and nesting depth 128.
- Package names, versions and native symbol identifiers are bounded ASCII data.
  The interface is compiler-produced, versioned implementation data, not a portable
  interchange format or a public hand-authored AST API.
- Mutable public globals and mutable static fields require typed accessors and
  are rejected by the producer. Private mutable globals stay only in the host.
- Public constants must have immutable literal expressions. Computed/global
  object initialization is not replayed inside script environments. Generic and
  default recipes that need private state must reach it through host functions.
- Native libraries/resources not statically linked into the executable remain
  deployment dependencies. A compiled-package descriptor does not bundle them.
- Contexts do not inherit packages implicitly. Registration is explicit for each
  environment. Replacing/removing a mounted identity is intentionally unsupported;
  use a fresh Context for a different package generation.
- Normal import visibility is not a security boundary. Native/FFI capabilities
  remain available; exposing packages does not turn the engine into a sandbox.

## Size And Acceptance

Report interface bytes, entry records, necessary adapters and retained code
separately. Exporting a whole package can retain code unused by the host itself;
this is not metadata overhead and cannot be hidden by compression or lazy lookup.

Required acceptance includes source-free executable deployment, native and
language functions/classes, parameter/default/overload and error behavior,
dependency diamonds/cycles/conflicts, context isolation, ABI/runtime mismatch,
malformed bounds, allocation-failure rollback, repeated calls, provider lifetime,
and comparison against an otherwise identical host without package exposure.
Record shipping-product tests separately from source/white-box tests.

## Running The Checks

Use a fresh ordinary build or an isolated candidate so concurrent work does not
overwrite shared outputs. For source/white-box checks, the resource must come from
that same fresh build, not a previous default build directory:

```powershell
./tools/build_xlir_public_source_driver.ps1 -Directory build/test/compiled_package_source `
    -RuntimeResource build/tmp/tcc_builtin_resources_runtime.c
./tools/check_compiled_packages.ps1 -PublicDirectory build/test/compiled_package_source `
    -RuntimeDirectory release -Directory build/test/compiled_package_acceptance
```

Either `PublicDirectory` or `RuntimeDirectory` can be selected independently.
The former verifies the source receipt, complete interface construction/decoder
allocation-failure prefixes, malformed IR rejection and strict O0/O2 consumer C.
The latter executes native clients against both actual DLLs at O0/O2, compiler
consumer checks, EXE-only provider rejection, source-free deployment, disk-shadow
precedence, system-only PE dependencies and the runnable example.

`results.json` records the selected acceptance lanes and compares otherwise
equivalent hosts with/without package exposure. Total EXE delta includes retained
implementations, alignment and metadata; it is not a claim that all extra bytes
are interface bytes or that compilation/calls have zero overhead.
The baseline/exposure sources are checked to differ only by one explicit
registration assertion and use equal-length source filenames. `HostBytes`,
`BaselineBytes`, `DeltaBytes` and `DeltaPercent` describe that controlled pair;
`FunctionalHostBytes` separately records the larger behavioral test host.

Registration copies native metadata, sorts entry records and checks conflicts.
The first mount reserves 64 stable descriptor slots in the Context. Script
compilation decodes its own declaration tree. These cold allocations, copies,
hash scans and bookkeeping are intentional and are not zero-overhead claims.

The standalone example is `release/examples/embedded_package/README.md`. Its
deployment contains only the executable and `external.xl`.

## Focused Evidence: 2026-10-28

Windows x64 candidate built through the ordinary `build.bat`, with fresh source
TCC/XRT, both DLLs, the CLI and both embedded VFS images. The build receipt has
`Complete=true` and `InputsUnchanged=true`. The source cohort contains 1,296
translation units and has its own verified receipt; it is not a released binary
compiler fallback.

Evidence directories under `D:/GIT/_codex_verify`:

- `compiled_packages_shipping_20261028_c/build/build.shipping.json`: ordinary
  candidate build, input hashes and product provenance.
- `compiled_packages_final_20261028/results.json`: combined source/product
  acceptance under Windows PowerShell 5, with unchanged acceptance inputs.
- `compiled_packages_controlled_frozen_20261028/results.json`: final 29-stage
  combined acceptance, including the equal-source size controls; complete,
  source/product accepted and 49 primary acceptance inputs unchanged.
- `compiled_packages_context_policy_20261028_c_rsp/results.json`: ten O0/O2
  Context policy phases, audited native layouts, IR and real native lifetime
  calls; complete and unchanged inputs.
- `compiled_packages_acceptance_tree_20261028.json`: frozen compiler source with
  only the current feature test/tool overlay. Parallel source updates are not
  mixed into that tree or reverted in the working checkout.

The feature tests cover 1,726 producer-plan failure prefixes, 35 simple-import,
47 alias-import and 1,558 diamond-import failure prefixes. Successful recovery
must produce an identical expanded AST. Decoder prefixes are 828/21/19/21 for
the four real interfaces; 72 malformed host-binding/lifetime-ABI mutations are
rejected. Registry failure prefixes are 27 for a fresh Context and 26 for a
prepopulated Context, with atomic rollback and retry. Exact aggregate interface,
entry and package quotas, runtime/default-symbol conflicts and identity checks
also run against both actual DLLs at O0/O2.

The four-package sample has 14,835 interface bytes, 33 entry records (528 bytes),
448 bytes of descriptors and 32 bytes of dependency pointers, before identity
strings/relocations. Executable size also includes retained implementations and
alignment. The runnable example passes with only EXE plus `external.xl`; the
functional host passes without package files and against a same-name disk decoy.

The controlled baseline executable is 18,693,120 bytes; adding only the explicit
package-registration assertion produces 18,747,392 bytes. The total delta is
54,272 bytes (53 KiB), or 0.2903%. Both controls already embed the same script
engine and import the same package. The behavioral test host is 18,772,480 bytes
and is not used as the exposed control. These figures measure this sample and
are not a universal size guarantee for arbitrary extension packages.

Adjacent null-context, threaded-host, module-dependency lifetime and XRT export
checks passed. In that earlier cohort, two unrelated checks failed with both
the new and pre-feature DLLs: `xlc_dll_embed_string` used a bytes result as a
string, and Debug EXE emission could not resolve `xrtStrBufAppendFormatV`.
Both are repaired and pass in the subsequent delivery-closure verification.
The Debug dependency plan now consumes the actual C-generation options rather
than planning a non-debug module for a debug executable.

The evidence above is feature-scoped, not a full standard library/CLI regression
or release-promotion claim. The subsequent Windows/Linux, sanitizer and real
GUI results, remaining full-regression failures and promotion state are tracked
in [the delivery ledger](embedded-package-delivery-2026-10-28.md).
