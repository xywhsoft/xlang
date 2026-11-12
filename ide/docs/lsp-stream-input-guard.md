# LSP read-only disk proof streaming (M7b5s)

`xideLanguageSnapshotGuard` previously called the retained disk reader and
immediately freed its normalized source text. It now uses a private streaming
reader in `services/language_snapshot.c`. Callers that need source text still use
`xideLanguageReadDiskProof`; the public API and all public layouts are unchanged.

The new reader uses XRT SHA-256 and strict UTF-8 primitives. Each read is at most
64 KiB; the stack buffer also carries at most three bytes of an unfinished UTF-8
scalar. It rejects embedded NUL, invalid or incomplete UTF-8, short/failed reads,
an extra EOF byte, cancellation and a failed close. It validates the initial and
final handle metadata, the named path's physical identity, both supported wire
timestamp schemas, raw/post-BOM sizes and both complete digests. With no BOM the
raw and text digests share one SHA stream; with BOM both distinct byte sequences
are hashed. Cancellation is checked between reads and at the closing boundaries.

This remains an observed read-only proof. It does not pin a name, lock source
bytes, authorize writes, implement a complete resolver/source closure or replace
the future transaction controller's admission and publication checks.

## Validation

The include-based test observes the actual producer, including the production
guard consumer, while delegating real file, encoding and digest operations to
XRT. Both the independent native XRT and the matched GUI DLL owner pass 757
checks. Coverage includes every split of 2/3/4-byte UTF-8 scalars across the read
boundary, physical BOM, malformed encodings, both timestamp schemas, an 8 MiB
file, all 131 observed cancellation points, all 128 failed and short reads,
initial/final metadata failures, named identity, EOF, close and same-byte
physical replacement. Direct producer allocation refusal still permits the
streaming proof. Eight compiled mutants are rejected by actual assertions,
including reverting the real guard consumer to the retained text reader.

Existing native input guards pass 104 checks plus 320 concurrent validations;
the source report tests pass 4508 checks. The new service passes 50 cases across
native completion, native analysis and automatic diagnostic protocol suites.
The GUI is rebuilt as a complete 128-unit cohort, with C LSP disabled; 55 GUI
suites and bounded actual C/xlang windows (24 frames each) pass, including the
established typing, deletion, undo, Unicode/IME and focus checks.

The native server is a qualified component build: one private implementation
unit is rebuilt and linked with the exact previously accepted native host/core
objects and archive. No native core or SDK rebuild is claimed. Its source,
header closure, inherited objects, linked archive and paired GUI are recorded
in the external materials. The default runtime, developer SDK and current
delivery index are preserved.

## Scoped measurements

Four sequential processes use ABBA retained/streaming pairs on the same real
file and proof, with a warm OS cache and independent native XRT ownership.
Each cell has 64 samples per method; all 768 raw rows are retained.

| Bytes/content | Retained P50/P95 ms | Streaming P50/P95 ms |
| --- | --- | --- |
| 1 KiB ASCII, no BOM | 0.0505 / 0.0626 | 0.0457 / 0.0597 |
| 1 KiB UTF-8, BOM | 0.0516 / 0.0707 | 0.0515 / 0.0844 |
| 1 MiB ASCII, no BOM | 8.3351 / 11.7373 | 4.1792 / 7.6549 |
| 1 MiB UTF-8, BOM | 8.6115 / 14.1750 | 8.0861 / 11.7214 |
| 8 MiB ASCII, no BOM | 67.5969 / 88.7567 | 33.7158 / 45.0549 |
| 8 MiB UTF-8, BOM | 70.0266 / 87.1019 | 67.4204 / 81.6883 |

The producer's two body allocations (largest `N+1`) become zero. Without BOM,
hashed bytes drop from `2N` to `N`; with BOM both methods hash `2N-3`. Both read
`N` bytes. The small BOM case has a worse observed P95 and is retained in the
report. These probes exclude allocations inside XRT/DLL/OS; the surrounding
guard can still allocate small environment buffers. This is neither a zero
whole-heap/RSS claim nor cold-disk or end-to-end IDE/LSP qualification. No
universal latency improvement is claimed.

## Remaining scope

The transaction host/controller, complete source/resolver proof and protocol
binding, consent/recovery UI and startup replay, remaining synchronous readers,
standard rename/applyEdit activation, final engine alignment and whole-goal
performance/resource/longevity acceptance remain open. The known compiler deep
unprefixed project-path and analysis-configuration cache-count issues are not
resolved by this stage. No Linux, sanitizer, DAP, debug or runtime-completion
qualification is claimed.

The material index is
`dev/release-materials/provenance/lsp-stream-input-guard-20261113.json`.
