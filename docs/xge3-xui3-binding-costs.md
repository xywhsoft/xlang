# Native binding CPU cost qualification

These results qualify the pinned CPU fixtures, not a final GUI delivery or a
universal zero-overhead claim. Raw C, handwritten checked C and xlang execute
the same native SDK/backend/workload within each platform cohort. All selected
runtime/XRT sources and the three clients use O2. The complete compiler is only
an offline IR planner; timed clients do not link its object list or xl.dll.

The sampler keeps two warmup rounds and twelve measured rounds, rotating all
six client orders. Each cell below is the median nanoseconds per operation
(Shape: per element). Raw ticks, distributions, checksums, input hashes and
commands are indexed by the acceptance receipt. Startup, setup, printing and
cleanup are outside the measured intervals. Scheduling/background load still
affect these small CPU measurements.

| Platform / operation | Raw C | Checked C | xlang | xlang / checked |
| --- | ---: | ---: | ---: | ---: |
| win_x64 / capability read | 1.676 | 349.167 | 398.908 | 1.142 |
| win_x64 / fixed rectangle draw | 1.603 | 736.190 | 790.766 | 1.074 |
| win_x64 / retained target query | 43.532 | 1826.196 | 2009.221 | 1.100 |
| win_x64 / Shape array element | 0.480 | 0.609 | 0.670 | 1.101 |
| linux_x64 / capability read | 1.387 | 299.546 | 309.769 | 1.034 |
| linux_x64 / fixed rectangle draw | 1.425 | 667.800 | 660.062 | 0.988 |
| linux_x64 / retained target query | 56.230 | 1629.887 | 1753.131 | 1.076 |
| linux_x64 / Shape array element | 0.523 | 0.675 | 0.792 | 1.173 |

Checked C uses the resident receiver/error/retained-result protocols on the
successful path. Its assertions terminate a failed fixture; xlang performs
normal checked unwinding. Checked C cold setup omits generated factory
pre-reservation. The generated program also retains ordinary language loop
and value semantics. The columns are therefore concrete programs, rather than
a proof of identical failure behavior or an isolated measurement of OOP syntax.

The expensive capability/draw rows are intentionally tiny native operations.
They expose real receiver admission, physical pins, error scopes, atomic/lock
and native lifecycle costs rather than hiding them behind a costly backend.
No per-method forwarding function is added, but direct declaration does not
remove these checks. Native/checked scope boundaries and retained results are
different from an unchecked raw C pointer. These numbers do not predict GPU
or end-to-end UI throughput.

## Shape arrays

The fixture creates a real native-owned Shape with 4,096 clusters and 4,096
carets over 8,192 counted bytes including NUL, then closes the original
Shape/Font/Proxy owners. Two owning sequences keep the native Shape alive.
5,000 passes perform 40,960,000 typed visits, checking a rolling result of
101357502464. The element storage is not copied. Both O2 disassemblies reduce
the two inner loops to six instructions: load the selected 32-bit field,
advance by the actual 28/12-byte C stride, XOR, add, compare and branch. There
is no per-element getter, boxing, allocation, full-record copy or function
call in those measured loops. The language POD-copy semantics remain valid;
the optimizer eliminates unused fields for this workload.

Sequence acquisition still allocates an owning value and adds a real native
reference. Every traversal opens/closes one resident read scope and retains
its sequence value; these operations remain in the measured outer loop. The
assembly observation does not remove their cost or explain a timing ratio by
one cause.

## Allocation and ownership evidence

Counter builds are separate; their timings are not performance results. Both
platforms observe the same counts:

| Phase | Raw C | Checked C | xlang |
| --- | --- | --- | --- |
| Warm native setup | 3 allocations / 880 bytes | 6 / 2,288 bytes | 6 / 2,288 bytes |
| 500,000 capability reads | 0 allocations | 0 | 0 |
| 500,000 draws | 0 allocations | 0 | 0 |
| 50,000 retained target queries | 0 allocations | 50,000 allocations and frees | 50,000 allocations and frees |
| Shape setup | 6 allocations / 164,808 bytes | 14 / 166,152 bytes | 17 / 167,544 bytes |
| 40,960,000 Shape element visits | 0 allocations / native calls | 0 / 0 | 0 / 0 |

Warm managed setup adds three real ForeignView controls, including the Draw
call storage: 1,408 bytes beyond raw native setup. The two checked owning
sequences add eight allocations / 1,344 bytes beyond native Shape setup; the
language fixture additionally keeps three source ForeignViews / 1,392 bytes.
Generated module initialization separately adds nine allocations / 230 bytes.
The native backend is held and dropped once; explicit close and automatic
module/function cleanup are reported in distinct phases. Counter liveCount
and liveBytes fields are phase deltas, not absolute final process counts;
processPeak fields are process-wide peaks. These fixtures do not establish
zero process-global TLS/cache memory or replace separate lifetime/OOM tests.

The static CPU backend uses an explicitly NULL code owner, so this experiment
does not measure dynamic CodeLease acquisition, external COM capsules, retained
language callback invocation, projected arrays or cold failure rollback.
Dynamic module lifetime has separate real DLL/DSO tests. Shape lifetime
metadata is production native metadata; the fixture does not fabricate an
array view over a freed owner.

## Reproduction and provenance

Use [the fixture instructions](../test/interop/native_binding_performance/README.md).
The Windows measurements use the R38 three-TU compiler successor and Native
Model A Windows R4 SDK/engine. Linux uses the R39 sized-output-record six-TU
successor and final Native Model A Linux R6 SDK/engine. These are explicit
partial overlays over verified complete source builds, not new complete
compiler rebuilds. Each platform compares its three clients against exactly
one pinned SDK/engine. Later DataObject/DocUI work and final shipping artifacts
are not silently included in this qualification.

The machine-readable index is
[`native-binding-costs-20261105.json`](../dev/release-materials/provenance/native-binding-costs-20261105.json).
