# XGE/XUI declarative package initialization

The `xge` and `xui` packages share the private `_xge_native` package. All three
package versions are `3.0.0`, with package ABI `2`. This is the generation of
the language binding; the current upstream XUI header still reports native
XUI version 2. The native XGE API reports version 3.

## One runtime

`_xge_native/src/bootstrap.xl` makes one checked top-level call during language
module initialization. Public packages explicitly import this private language
module: manifest dependencies resolve native resources but do not execute the
dependency's language sources. Repeated imports of the same public namespace
are idempotent. Importing both public modules currently executes a private
initializer in each import context; native identity binding is idempotent.
These cold calls precede public native calls in global initializers.
The C implementation first checks the nonzero SDK native ABI fingerprint and
feature mask using the engine's runtime-independent compatibility entry. It
then validates the exact generated XRT header fingerprint, data model and core
layout, and binds the engine imports
to the current language host. A repeated import in another module validates
the same runtime rather than initializing a private runtime.

The resolver is `xl6HostXrtResolveV1`. Its implementation executes inside the
actual host and returns immutable function addresses. Taking a function's
address from a TinyCC module is insufficient on Windows: separate modules can
receive distinct IAT thunks for the same host function. An engine retaining
one of those thunks would also acquire an undeclared dependency on that module.
The resident resolver avoids both problems without inspecting machine code or
guessing whether `xl.dll` or `xlc.dll` owns the process.

The full language host reuses its existing native symbol table. A standalone
artifact that explicitly needs the resolver generates it in the same
translation unit as the selected XRT implementation. Its catalog is filtered
by the actual implementation feature macros. It is not a fixed object
referencing every XRT module. The compiler's `IN_XRT_TRANSLATION_UNIT`
placement contract controls this emission, including explicitly requested
full-runtime artifacts. An embedded language host supplies the existing host
implementation instead.

Binding performs one native ABI comparison and a bounded sequence of cold
lookups, including on a second module's identity validation. The Frame/Cell
default candidates require 188 lookups on Windows and 186 on Linux. No such
lookup occurs in a drawing or UI method. Binding does not allocate on
success. Errors, including ABI mismatch, missing symbols, a different runtime
and competing initialization, return a checked status. Diagnostic allocation
failure still returns failure. No binding failure calls `assert` or `abort`.
All drawing, layout and other operations keep their native declarations.

## Resource policies and residency

`xge3_runtime.h` supplies policies for Context, Widget, Font, Texture,
Image, Material, CellOutput, Shape, ShapeScene, PathMeasure and AudioClip.
Context uses the native lifetime watch, call scope,
ownership trace and busy query. Widget watches actual native destruction and
belongs to its Context. Font and Texture release the adopted native records;
Image and Material release their payloads and their language-owned record
allocation. These distinct destruction contracts must not be interchanged.

CellOutput keeps one actual shared native reference. Its independent native
ownership node traces all value/callback/delegated-source edges; it is never
flattened into a leaf. Its root declares the exact cold native domain. Native
busy state supplies a separate preparation capability. Ordinary close consumes
only the language owner's reference, preserving an external native AddRef.
ReleaseOwnerChecked distinguishes consumption from cleanup success; status
errors are reported even if the native owner was consumed or diagnostic
allocation fails. No physical watch or extra native call pin is needed for
this owned RC root.

Shape, ShapeScene and AudioClip release their actual native RC references.
PathMeasure has unique independent flattened storage and no invented Retain.
These four native payloads have no retained language callbacks or values.
Their policies add no recursive language edges. The CPU acceptance checks
shared references, measurement after source destruction and decoding an
in-memory WAV without creating an audio device. Feature-disabled engines do
not acquire references to absent native implementations.

Policies are exposed through cold descriptor getters so Windows consumers do
not depend on importing native data symbols. Native operations do not route
through these getters. Generated code still performs the checked lifetime,
ownership and call-scope work required by its declared resource contract;
direct native symbol declarations do not mean that those checks have zero
cost. Other resource families require their own audited native contracts.

Package `links` and `loadlibs` use the actual matched engine DLL/shared object.
TinyCC keeps its real loader references in the module's `TCCState`; final
deletion closes those references. Repeated package `links`/`loadlibs` share
one loader reference per state. On ELF, explicit retained-library handles are
resolved before `RTLD_DEFAULT`; the latter can create an implicit glibc
`NODELETE` dependency that would defeat actual unloading. Explicit symbols
registered by the host still have precedence over dynamic lookup.

The module owns that state after its compiler context is destroyed. Foreign
resources and language callbacks retain the real module CodeOwner. If a
callback closes its own module, the final native Drop still has a physical C
return tail after releasing that owner. This marks retirement pending and
blocks automatic unloading on every thread. The host must stop/join the
native publishers, then call `xlRuntimeDrainRetiringChecked`; an arbitrary
language safe point does not prove global native quiescence. Keeping a DLL
loaded accidentally is not an ownership mechanism.

Rootless native callbacks use the resident checked error mailbox when no
immediate language boundary exists: first error plus failure count, then the
next successful safe checked boundary or explicit host Take. A boundary that
already failed preserves the pending batch. An active native callback scope
postpones delivery in that execution context. Native snapshots and the explicit
retirement rule above independently protect code residency. A failed native
Drop preserves its C `false` status and diagnostic while the mailbox owns a
separate real error reference for language delivery. A missing diagnostic still
records a failed event. This mailbox owns resident XRT diagnostic data, not a
generated closure or module. It does not make global native
callback registrations automatically collectible; their native owner must
explicitly clear them.

## Isolated staging and acceptance

Run `tools/stage_xge3_packages.py` with `--root`, `--engine-root`,
`--engine-build`, `--out` and `--report`. Both output paths must be fresh and
outside `release/`. The tool pins the engine binary, exact SDK headers,
feature configuration, generated host import contract, language XRT hash and
package sources. It independently recomputes the engine's native ABI
fingerprint from the actual headers/configuration/compiler version and rejects
SDK drift. It checks the resolver catalog against the native linkage
catalog and records every staged file hash. Use `--bootstrap-only` for the
private package's independent acceptance while public declarations are under
development. The default engine stages `xge` and `xui`; `xge3d` is staged only
when the matched engine feature inventory enables `XGE_ENABLE_3D`. All packages
use one shared build-configuration header before their first native include.
The optional `xgeDebug` package is staged only when the real native compiler
flags and first-include SDK configuration both enable debug APIs. A debug flag
without the corresponding SDK macro is rejected.

The upstream public headers' `extlibs/xrt/xrt.h` include spelling is preserved
by a declaration-only forwarding header. No second XRT implementation or
amalgamated full XRT source is copied into the package.

`test/cases/foreign_xge3_package_check.py` accepts `--root`, `--cohort-root`,
`--public-directory`, `--packages` and a fresh `--out`. It verifies a full
source compiler cohort, runs GCC O0/O2 clients and TinyCC JIT/module/standalone
paths, injects checked binding failures, and tests two modules surviving their
compiler contexts before the native library finally unloads. It is headless
and offline. Its scope does not certify the full public XGE/XUI API or GPU
behavior; consult the resulting report before making delivery claims.
The `--fixture foreign_xge3_package_public_init.xl` variant checks repeated
ordinary public imports and a native global initializer; its generated C must
contain two identity checks for the two public import contexts, rather than
four checks for four repeated imports, before the native allocation.
`foreign_xge3_package_cell_script_check.py` uses the actual public Cell methods
and generated language capture closure. Two real modules retain their cycles
after both compiler contexts are destroyed. The checked exported entries keep
their globals live; the separate one-shot API intentionally clears globals on
return. Physical watches
prove one destruction per cell after module cleanup, followed by real engine
unloading. It does not substitute a handcrafted C closure for script capture.

`foreign_xge3_package_tail_check.py` tests a DLL that continues executing after
the callback's last Drop, then joins its worker and proves actual unloading
after explicit checked retirement. `foreign_xge3_package_callback_drop_check.py`
tests real native callback finalizers failing with both an owned diagnostic
and a NULL diagnostic under OOM. Its optional runtime overlay is recorded as
development qualification rather than a new full compiler build.

On Linux, `stage_source_tcc_provider_linux.py` creates an isolated matching
producer, O2 target objects, VFS catalogs and provider. It uses `-Bsymbolic`
to isolate the provider's internal TinyCC state and never defines the
Windows-oriented `CONFIG_TCC_STATIC`. Public declaration headers accompany
the driver's private SDK. `stage_source_public_sdk_linux.py` can complete an
already verified historical source/provider stage without rebuilding its
objects or provider. `stage_source_tcc_producer_overlay_linux.py` explicitly
records a six-unit partial producer update, reused object hashes and flags,
and the resulting source fingerprint; it does not claim a full recompilation.
`check_source_provider_nested_linux.py` tests compiler-import JIT/module/AOT
with declarations-only adjacent SDKs and relocated standalone executables.

After regenerating the canonical native linkage catalog, regenerate the
derived resolver fragment with:

```powershell
python tools/stage_xge3_packages.py --root D:/GIT/x-lang --update-host-catalog
```

Runtime publication, matched SDK delivery and release evidence updates are
separate from staging. Source snapshots belong in
`dev/release-materials/sources/`; acceptance and delivery reports belong in
`dev/release-materials/provenance/`.
