# Compiled-package native capabilities

The private XLP2 declaration format preserves a package's native capability
requirements, capabilities of its selected artifact, and manifest-declared
dependency names. XLP1 remains readable and supplies no capability claim or
native dependency authority. Old compilers reject XLP2; the public
`XL_CompiledPackage` record layout, version and C ABI remain unchanged.

Ordinary producers without capability metadata or manifest dependencies still
emit XLP1. Producers that have either emit XLP2. The module literal contains an
explicitly tagged NFM1 payload, with counted little-endian fields for package
name/version/ABI and three lists. The lists are sorted and unique; the existing
4 MiB interface, 64-package dependency and bounded AST limits continue to apply.
The payload is private data, not language source or executable initialization.

The wire reader validates the complete tree and canonical metadata before
publishing any declaration. Public API registration also matches its package
identity and requires every declared native dependency to occur in the actual
descriptor closure. Malformed metadata, trailing bytes, duplicate/unsorted keys,
invalid tokens and identity mismatches are rejected. Registration retains its
existing atomic publication and checked failure behavior.

When importing, the compiler copies the immutable feature snapshot into the
current compilation's package graph. Both source-to-registered and registered-
to-registered dependencies are checked. A missing registered dependency cannot
fall back to a filesystem package. Package imports needed for declarations may
be wider than the manifest's native dependency closure. The compiler preserves
the declared prefix separately, so those extra imports cannot grant capability
authority when producing another compiled interface.

Metadata validation and descriptor matching allocate no scratch memory.
Encoding allocates the payload plus an optional pointer sorting array; it does
not change the original list order. Decoding owns the three identity strings
and list entries, and rolls the entire result back on failure. Import snapshot
copies and repeated cold wire validation have a real compilation cost. Existing
registration still copies the wire bytes. Capability closure validation uses
the source-package validator's two scratch arrays when requirements exist.
No new operation is added to drawing, input, audio or other native method calls.
This is not a new CPU zero-overhead measurement.

Windows and Linux source/frontend gates each pass 172 steps at O0/O2. The 28
compiled-interface steps include canonical output across reordered inputs,
descriptor identity/dependency checks, every truncated wire prefix, header
mutations, zero-allocation validation, 96 metadata-decode and 96 AST-decode
allocation prefixes, encoder failure rollback, legacy isolation, transitive
capabilities, undeclared/unrelated import rejection, mixed source/registered
imports and dependency version mismatch. All checks use current real frontend
units with the matched XRT. Source inputs, logs and artifacts are independently
hashed and frozen outside the repository.

The first full compiler candidate was superseded before acceptance when review
found an additional old-format-only IR guard. The successor includes that guard
update and preserves the missing-dependency/no-filesystem-fallback diagnostic.
The IR's existing checksum, package arena, callable, lifetime and cycle checks
remain in place. The existing source IR test now supplies complete package
version/ABI fields instead of incomplete private test records.

Full O2 compiler/TCC/XRT/VFS builds passed on Windows and Linux. The 19 product
steps per OS cover public API registration rollback, the old registry suite, reused
compiler contexts switching AUTO JIT/AOT and explicit linkage, source-free
compiled-package production/nested compilation, and metadata decisions against
the matched real XGE SDKs. API registration exercised 13 allocation-failure
prefixes per client. Enabled 3D and missing debug capability checks use the
actual matched default SDKs and do not execute their graphics backend.

The producer/import/wire IR fixture also passed. Windows links the exact full
O2 compiler provider as a standalone source test and validates generated C at
O0/O2. Linux uses O0/O2 clients of the actual O2 compiler DSO and validates the
generated C at both levels. Windows exercises 4,293 allocation-failure prefixes;
each Linux client exercises 3,265. Counts differ with the platform's allocation
paths; each run covers every prefix through the first untriggered success.
Each profile also rejects 72 invalid host-binding mutations. Producer plan
identity remains stable after failure, all four wire trees reject every
truncated prefix, and import/alias/diamond rollback and retry pass. Header
mapping failures in the initial standalone syntax fixture are preserved in
external reports; the successful fixture reproduces the matched VFS layout.

Independent verification checks complete frozen compiler inputs, reports,
programs, logs, package bytes and generated artifacts. There are 392 outer
steps in this successor batch: 344 source/frontend, 38 product and ten IR
build/execution/syntax steps. Detailed paths, per-profile counts and scope are
indexed by `dev/release-materials/provenance/package-compiled-native-features-20261116.json`.
This is checked/OOM qualification with O2 providers, not an instrumented
ASan/UBSan provider build. No final release or IDE delivery has occurred.
Debug-enabled backend execution, latest XRT alignment and remaining XGE/XUI/
Document coverage are still separate requirements.
