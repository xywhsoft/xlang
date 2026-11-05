Mixed serialized-object successors preserve per-object provenance.

A schema-2 manifest is an incremental target-object set, not a full rebuild.
Each row retains its actual compiler SHA, flags, object path/hash and source
closure. A parent may already contain multiple compilers; inheritance keeps
those identities transitively. `currentProducer` names only new compilation,
while `catalogPreprocessor` identifies the separately inherited component scan.

Use tools/precompile_builtin_successor.py only for a reviewed body-only
successor with unchanged ABI/header/SDK/component/flag inputs. Its parent
executable is mandatory; schema-2 parents validate all registered compilers.
The producer change allowlist is an explicit review assertion, not automatic
proof that arbitrary code-generator changes are equivalent. The qualified
2026-11-06 successor changes only xl_api.c and linker-side tccelf.c; the latter
has no front-end/code-generator change. A C body hidden behind an unchanged
literal header/include is rejected, because the original cache walker follows
literal C-to-C includes only. Header, code-generator, target/profile or SDK
changes require an appropriate new qualification and usually full precompile.

Example (paths outside release):
  python -B tools/precompile_builtin_successor.py --root NEW --parent-root OLD \
    --parent-manifest PARENT/manifest.json --distribution TCC_DIST \
    --producer NEW_TCC --parent-producer OLD_TCC \
    --allow-producer-source-change lib/tcc/tccelf.c --expect-recompiled 6 \
    --output NEW_PRECOMPILED

The tool checks the complete object universe, unchanged literal C closures,
all project .h/.inc/.def bytes, exact flags, SDK headers, compiler executables
and both sole-XRT products. Inherited source/path/hash values are not relabeled.
`stage_source_tcc_provider.py` and `gen_tcc_vfs_resources.py` explicitly validate
schema 2. Existing schema-1 full-build calls remain supported.

SDK source fallbacks are a different concern from object provenance: an explicit
source-equivalence bridge refreshes aliases from the new source root while old
objects retain their actual old Source/Flags. This matters even when only a
linker body changes (for example __xl_compiler/lib/tcc/tccelf.c).

`check_precompile_builtin_successor.py --out NEW_DIR` checks mixed producers,
transitive inheritance and malformed metadata without executing a compiler.

For a provider acceptance that must not emit/execute TinyCC PE files, pass
`--skip-pe-smoke` to the Windows stager. It performs a JIT smoke and reports
peSmokeSkipped=true, peAotQualified=false and shippingDelivery=false. The default
stager behavior is unchanged. A JIT-only report does not qualify PE/AOT.

The v1 rebuilding CLI qualifies the TinyCC SDK-header profile. It rejects a
nonempty nativeSystemHeaders digest because it does not recompute the host
GCC system-header inventory. Historical Linux schema-2 records can be checked
with validate_mixed_manifest(..., verify_sources=False), which checks signed
metadata, compiler and object bytes but does not claim source/system-header
requalification. An external Linux source/header proof remains separate.

Read-only VFS packaging is distinct from permission to rebuild objects. For
schema-2 manifests with a nonempty nativeSystemHeaders digest, the packer
checks content checksums, per-object compiler/flags/provenance and object bytes with
verify_sources=False, then verifies the full project source_fingerprint. Its
stamp explicitly records nativeSystemHeadersRequalified=false and requires
the existing external native ABI/system-header qualification. It neither
executes a compiler nor claims to refresh that external proof. The rebuilding
CLI continues to reject an unrecomputed nonempty system-header inventory.
