# Native Queue domain probes

This independently closed core slice records a Queue's canonical construction
schema identity before replacing the owner layout identity with the Queue's
identity. The per-family `AdmitsSchemaV1` probe checks both, the complete physical
tuple and the independently owned admission policy. It checks genuine private
family ops before dereferencing a payload, and refuses cleared owners.

Admission is not a Queue element operation. An active producer/consumer pin does
not prevent a metadata check: no queue lane is entered, slots are not visited,
no allocation is required and the caller's diagnostic is unchanged. Immutable
resident-policy equivalence may be evaluated; it is not a language callback.

## Evidence

Reports under `demo6/build/test` explicitly use hashed native/compiler overrides:

- `stdlib53-b135-queue-busy-win-dev3/queue.reference.busy.json`;
- `stdlib53-b135-queue-busy-linux-dev3/queue.reference.busy.json`;
- `stdlib53-b135-queue-busy-linux-san-dev3/queue.reference.busy.json`.

Each completes ten phases with `Complete=true`, `InputsUnchanged=true`. Actual
provider bodies and the native host compile at O0/O2; ASan/UBSan instruments
those three providers and the host, not the entire resident runtime. The test
pauses an actual operation in native-only leaf metadata and checks all three
families, independent equivalent/wrong policies, cross-family refusal, a wrong
canonical seed identity, pre-existing errors and an armed allocation failpoint.
Both normal lifetime and last-external-shell release during that real operation
finish with balanced allocation/free/live-byte/invalid/double/use-after-free
accounting. The custom scheduling policy is test-only; production recursive
Queue descriptors acquire no generated or user callback.

The separate native kernel regression report
`stdlib53-b135-queue-legacy-linux-dev1/results.json` verifies O0/O2, provider/host
sanitizers, real concurrent producers/consumers, Future graphs and cleanup failure
with 277 phases and 45 complete fault prefixes against the same core changes.

These are not rebuilt source/shipping-product or full 53-module acceptance.
The recursive Queue/Tree schema factories, their IR/Cgen integration and the
full native Tree migration remain a separate unsealed batch.
