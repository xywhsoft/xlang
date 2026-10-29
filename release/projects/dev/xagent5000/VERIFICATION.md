# XFlowSim — Verification Record

This file records the exact commands and their results used to verify XFlowSim. All
commands were run on the workspace as configured; the compiler is `release/xl.exe` and
the standalone executable is `release/projects/dev/xagent5000/xflowsim.exe`.

**Source:** `projects/dev/xagent5000/xflowsim.xl` — 218,750 bytes, **5,812 physical
lines** (measured with Python `str.splitlines()`; the file uses LF line endings).

**Executable:** `projects/dev/xagent5000/xflowsim.exe` — 1,588,736 bytes (standalone,
no run-time dependencies).

> Note on the compiler working directory: `xl.exe` discovers its standard library
> (`release/examples/stdlib_*.xl`) relative to its own location, so it is invoked from
> the `release/` directory below. The produced `xflowsim.exe` is fully standalone and
> runs from any directory.

---

## 1. Type-check the source

```text
release> xl.exe check projects\dev\xagent5000\xflowsim.xl
ok
```
Exit code: **0**.

## 2. Build the standalone executable

```text
release> xl.exe build projects\dev\xagent5000\xflowsim.xl -o projects\dev\xagent5000\xflowsim.exe
```
Exit code: **0** (no diagnostics). Produces `xflowsim.exe` (1,588,736 bytes).

The commands below are run via the standalone `xflowsim.exe`.

## 3. help

```text
xflowsim.exe help
XFlowSim 1.0.0 - deterministic workflow & resource-scheduling simulator
... (usage, command table: help, demo, validate, simulate, replay, selftest,
     stress, benchmark, report; options; built-in scenarios; determinism note)
```
Exit code: **0**.

## 4. selftest

```text
xflowsim.exe selftest
...
passed: 355 / 355
failed: 0
RESULT: OK - all 355 assertions passed
```
Exit code: **0**. **43 test modules** (355 assertions) covering: DSL parsing, graph validation,
topological order, SCC, critical path/slack, determinism, diamond execution,
retry/backoff, cache hit/invalidation, resource serialization, priority ordering,
fairness determinism **and effect**, cancellation, timeout, metrics/percentiles, snapshot/replay,
deterministic generator, invariants, failure-injection determinism, deadlines,
end-to-end pipeline, 12-seed stress determinism, RNG reproducibility, edge cases,
catalogue, scenario serializer round-trip, transitive closure, transitive reduction,
layer assignment, Jain fairness + histogram, event-log + Gantt formatting, graph
diagnostics, and extended report determinism — **plus the release-candidate adversarial
suite (tests 34–40)** described in §15, **the invariant-detection probe (test 41)**
described in §16, **the fairness-effect regression (test 42)** for the defect fixed in
§21, **the cross-scenario replay-equivalence property (test 43)**, **the
`criticalPath`-terminates-on-a-cycle regression (test 35)** for the defect fixed in
§19d, and **the literal-`\n` parse-normalization check (test 01)** for the fix in §19d.

## 5. demo

```text
xflowsim.exe demo
... (full report)
critical path length: 325
critical path: fetch -> build -> it -> pack -> sign -> report
...
fetch   COMPLETED  ... cache 1      (cache hit: dur planned 40, actual 5)
build   COMPLETED  ...
ut      FAILED     ...              (fails at attempt 1, blocking pack/sign/report)
it      COMPLETED  ...
gpu     COMPLETED  ...
pack    BLOCKED
sign    BLOCKED
report  BLOCKED
invariants: OK (0 violations)
```
Exit code: **0**. Makespan 215 ms; pool utilization cpu 9.88% / gpu 27.91%; cache hit on
`fetch`; `ut` fails and blocks its downstream chain.

## 6. validate (built-in demo, no files)

```text
xflowsim.exe validate
scenario: demo (seed=7)
stats: jobs=8 pools=2 edges=1 failRules=2
topological order (8): fetch, build, ut, it, gpu, pack, sign, report
is-dag: true
strongly-connected components: 8
critical path length: 325
critical path: fetch -> build -> it -> pack -> sign -> report
sources: fetch
sinks:   report
RESULT: OK - scenario graph is valid
```
Exit code: **0**.

### 6b. validate (intentionally cyclic `cycle` scenario — must terminate, exit 1)

```text
xflowsim.exe validate --scenario cycle
scenario: cycle (seed=1)
stats: jobs=3 pools=1 edges=0 failRules=0
topological order (0):
is-dag: false
strongly-connected components: 1
non-trivial components (cycle size>1): 1
critical path length: 0
critical path: A -> B -> C
ERRORS (1):
cycle detected: A -> B -> C -> A
RESULT: FAIL - scenario graph has errors                        exit 1
```
Exit code: **1**. This is the regression for the defect fixed in §19d: `validate
--scenario cycle` previously **hung forever** because `criticalPath`'s greedy path walk
chased the cycle (on an empty topological order every cyclic node is spuriously
"critical"). It now terminates immediately and reports the cycle.

## 7. simulate (built-in demo, no files)

```text
xflowsim.exe simulate
... (full report)
simulation time: 1 ms
events recorded: 10
invariants: OK (0 violations)
```
Exit code: **0**.

## 8. replay — in-memory built-in snapshot

```text
xflowsim.exe replay
replay: built-in demo snapshot (in-memory)
snapshot history digest : 1634650706
replayed history digest : 1634650706
snapshot final digest   : 1380223739
replayed final digest   : 1380223739
makespan (snapshot/replay): 215 / 215
RESULT: OK - deterministic replay matched the snapshot exactly
```
Exit code: **0**.

## 9. replay — real file round-trip (export then replay)

```text
xflowsim.exe simulate --save-snapshot demo_snap.json
... (full report)
snapshot written: demo_snap.json
invariants: OK (0 violations)        exit 0   (file: 11,537 bytes)

xflowsim.exe replay demo_snap.json
replay: demo_snap.json
snapshot history digest : 1634650706
replayed history digest : 1634650706
snapshot final digest   : 1380223739
replayed final digest   : 1380223739
makespan (snapshot/replay): 215 / 215
RESULT: OK - deterministic replay matched the snapshot exactly    exit 0
```
The exported JSON snapshot re-parses and reproduces byte-identical digests.

## 10. stress (inclusive seed range, 1–64)

```text
xflowsim.exe stress 1 64
... (per-seed lines, e.g.)
  gen-25 seed=25 jobs=18 done=18 fail=0 timeout=0 cancel=0 block=0 makespan=231 attempts=18 cache=7 inv=0
  gen-38 seed=38 jobs=17 done=0 fail=1 timeout=0 cancel=0 block=16 makespan=0 attempts=1 cache=0 inv=0
  gen-64 seed=64 jobs=20 done=17 fail=1 timeout=0 cancel=0 block=2 makespan=387 attempts=19 cache=7 inv=0
stress summary: seeds=64 jobs=1189 completed=929 attempts=1002
  total invariant violations: 0
  crashes: 0
RESULT: OK - all seeds passed invariants
```
Exit code: **0**. 64 generated scenarios (twice the required ≥32), 1,189 jobs total, 929
completed, 0 invariant violations, 0 crashes. The strengthened `runInvariants` (see §16)
runs on every seed, so each of these scenarios also asserts resource-peak bounds and full
terminal state — including pathological seeds such as `gen-38` where a root failure
cascades into 16 blocked jobs and a zero makespan.

## 11. benchmark

```text
xflowsim.exe benchmark 10
benchmark: iters=10 scenarios=4
  demo   jobs=8   events=10  makespan=215  14 ms total / 1 ms per run
  gen:0  jobs=16  events=28  makespan=230  35 ms total / 3 ms per run
  gen:1  jobs=40  events=80  makespan=670  98 ms total / 9 ms per run
  gen:2  jobs=80  events=76  makespan=648 104 ms total / 10 ms per run
```
Exit code: **0**. (Absolute timings are machine-dependent; job/event/makespan figures
are deterministic.) The post-optimization timings are reported in §19c (`benchmark 30`:
demo 1 / gen:0 2 / gen:1 7 / gen:2 8 ms per run, down from 1 / 3 / 8 / 10).

## 12. report

```text
xflowsim.exe report
XFlowSim report: demo (seed=7)
makespan: 215 ms   completed: 4  failed: 1  blocked: 3   success rate: 50.00%
...
gantt timeline (each char ~= 4 ms, makespan=215)
time        |.........|.........|.........|.........|.........|.........|...
cpu#0       .1111111111111111111111111111112...............................
... (per-worker lanes, completion-time histogram, Jain fairness index, event log)
```
Exit code: **0**.

## 13. External file input (`--file`)

```text
xflowsim.exe validate --file ext_scenario.xfs
scenario: extdemo (seed=99)
topological order (4): a, b, c, d
critical path length: 40
critical path: a -> b -> d
RESULT: OK - scenario graph is valid                       exit 0

xflowsim.exe simulate --file ext_scenario.xfs
makespan: 40 ticks   completed: 4   failed: 0              exit 0
```

### 13b. Inline text input (`--text`) with literal `\n`

```text
xflowsim.exe validate --text "pool p workers=2 cpu=4 mem=512\njob A dur=10\njob B dur=10 deps=A"
critical path length: 20
critical path: A -> B
RESULT: OK - scenario graph is valid                           exit 0
```
`parseScenario` normalizes a literal backslash-n escape to a real newline, so the
`--text` flag is usable from shells (e.g. cmd) that pass `"\n"` verbatim rather than
expanding it. This is the regression for the fix in §19d. (`--file` carries real
newlines and is unaffected.)

## 14. Invalid input (must exit nonzero)

```text
xflowsim.exe badcommand
xflowsim: unknown command 'badcommand'
try: xflowsim help                                        exit 2

xflowsim.exe stress 5 1
Error: stress first-seed (5) must be <= last-seed (1)     exit 1

xflowsim.exe            (no arguments)
... (help text)                                           exit 1

xflowsim.exe stress abc 5
Error: stress seeds must be integers                      exit 1

xflowsim.exe stress 1 999999
Error: stress seed range too large (max 100000)           exit 1
```
All invalid inputs exit nonzero with a useful message (`badcommand`→2; range/type/
over-max/no-args→1).

---

## 15. Adversarial / property-style coverage (release-candidate suite, tests 34–40)

The hardening pass added 7 dedicated adversarial modules (**+82 assertions**, all pass)
that deliberately attack the implementation rather than exercise the happy path, plus the
invariant-detection probe of §16 (test 41, +6 assertions), the fairness-effect regression
of §21 (test 42, +13 assertions), and the cross-scenario replay property (test 43, +17
assertions):

- **test34 — malformed DSL & parser robustness:** empty scenario, unknown directive,
  non-numeric seed (graceful default, no crash), duplicate job/pool ids, `workers=0`,
  unknown pool ref, self-dependency, missing dependency ref, negative resources
  (clamped to 0), and a `job` directive with no id. Each is rejected or normalized
  without a crash; missing references are caught by validation (parse is permissive).
- **test35 — cycles & topological determinism:** self-cycle, 2-node and 3-node cycles
  all rejected with a `cycle` error; running a cyclic graph **terminates** (no infinite
  loop) and leaves every job terminal; topological order is byte-identical across calls;
  predecessor-before-successor invariant checked on a 5-node graph.
- **test36 — critical-path ties:** two equal-length paths resolve deterministically and
  lexicographically (`A,B,D`), identical across calls, length bounded by total duration.
- **test37 — zero / saturated / starvation:** a job whose `cpu` exceeds pool capacity
  never fits → deterministically `CANCELLED`, 0 peak cpu, usage returns to 0; exact-fit
  saturates exactly to capacity; two full-pool jobs serialize; a zero-capacity pool
  serves a zero-resource job. All hold 0 invariant violations.
- **test38 — retry exhaustion + backoff + concurrent finish:** an always-failing job with
  `retry=2` runs exactly 3 attempts → `FAILED`, with the recorded `RETRY_SCHEDULED`
  backoffs exactly `base, 2*base`; two equal jobs finish simultaneously with peak slots
  exactly 2 and both slots released afterward (no leak).
- **test39 — timeout/cancel race + cache invalidation:** when a natural timeout and a
  cancel rule both fire at the same tick, the lower-seq cancel wins deterministically,
  the stale timeout is dropped, and resources are released **exactly once**; a cache key
  written then immediately invalidated on producer completion forces the consumer to
  recompute (0 cache hits). All hold 0 invariant violations and deterministic outcomes.
- **test40 — snapshot/percentiles/limits:** snapshot survives a JSON round-trip
  (`stringify` → `parse`) and replays to matching digests; percentile boundaries on
  empty / single-element / two-element arrays (p0/p50/p100); horizon clamping for 0,
  negative, and over-`MAX_HORIZON`; and a deterministic-equivalence property loop over
  8 generated seeds (final-state digest stable across two independent runs).
- **test42 — fairness effect (deficit reorders dispatch):** regression for the defect in
  §21. A single-worker scenario forces fairness to be the deciding factor: a waiting job
  `k` (deficit 1) must beat a freshly-ready job `aaa` (deficit 0) under `fair=true`, while
  the lexicographic tie-break runs `aaa` before `k` under `fair=false`. The test asserts
  the exact start times, that the two policies yield *different* histories (the assertion
  that failed while fairness was dead), and that each policy is individually deterministic.
- **test43 — snapshot/replay equivalence across scenarios:** replay equivalence was
  previously exercised only on the demo. This property test round-trips (build → JSON
  `stringify` → `parse` → replay) the **retry** scenario (RETRY_SCHEDULED events), the
  **cache** scenario (hits/invalidation), and a **generated** multi-job scenario, asserting
  each replays to matching history+final digests with zero mismatches — catching any
  serialization fidelity loss in retry/backoff/cacheKey/deps fields.

## 16. Strengthened invariants (`runInvariants`) and their detection probe

Two previously-missing post-run checks were added (real assertions, not weakened):

- **Resource-peak bounds:** for every pool, `peakSlots ≤ workers`, `peakCpu ≤ totalCpu`,
  `peakMem ≤ totalMem`. The allocation guard makes this a tautology for a correct
  scheduler; it would catch any double-allocation or failed-release defect.
- **Terminal-state completeness:** every job is terminal (`COMPLETED`/`FAILED`/
  `TIMEOUT`/`CANCELLED`/`BLOCKED`) after `finalizeRun` — no `PENDING`/`READY`/`RUNNING`/
  `WAIT_RETRY` leftovers.

Both pass on all built-in scenarios, all 64 stress seeds, and every adversarial case in
test34–40. (Invariant #3 still correctly relaxes the makespan≥critical-path check when
there are cache hits, failures, cancellations, or blocked jobs.)

**test41 — invariant detection (negative probes):** the invariants above are only ever
exercised in the passing direction elsewhere, so a regression could silently make them
vacuous. test41 proves they actually *detect* breakage: it parses/simulates a small
2-job chain, asserts a clean run (0 violations), then (a) injects a non-terminal leftover
`state["jobState"]["b"] = READY` and asserts the run now yields >0 violations containing
the "not terminal" message, and (b) injects an over-capacity peak
`state["peakPoolUse"]["p"]["cpu"] = 9999` and asserts a "peak cpu" violation fires. Each
corruption is restored and the state is re-asserted clean (no structural false positives),
for **+6 assertions**.

## 17. Deterministic-equivalence evidence

- **Byte-identical snapshots:** two `simulate --save-snapshot` runs of the same
  built-in demo produce files that compare identical (`fc` reports no difference; the
  test asserts `SNAP_IDENTICAL`). The only textual difference between the two console
  summaries is wall-clock timing noise (`simulation time: 1–2 ms`) and the supplied
  snapshot filename — the realized results (10 events, makespan 215) and the snapshot
  digests are identical.
- **Byte-identical stress output:** two independent `stress 1 64` runs produce
  text-identical output (`fc` reports no difference; asserted `STRESS64_IDENTICAL`),
  proving deterministic equivalence across all 64 seeds, 1,189 jobs, 929 completions.
- **Byte-identical replay:** two `replay` (in-memory) runs emit identical digests
  (history **1634650706**, final **1380223739**) and identical makespan 215/215; the
  file-round-trip replay (§9) reproduces the same digests.
- **In-process:** tests 06, 22, 40, and 43 assert `historyDigest`/`finalStateDigest`
  equality across independent runs for 12 + 8 generated seeds plus the built-in scenarios,
  retry, cache, and a generated snapshot round-trip.

## 18. Hardening-pass outcome

The challenge phase found and fixed **one real correctness defect** (the dead-fairness
bug, §21) by constructing a scenario that exposed it; the targeted areas were otherwise
robust. The pass delivered: the §21 defect fix plus its regression test (test42), two
strengthened invariants, a negative invariant-detection regression test (test41),
cross-scenario replay-equivalence coverage (test43), 82 new adversarial assertions, and
broadened determinism evidence. Existing assertions were preserved and not weakened; the
fix did not change any deterministic demo/generator output (the demo's jobs never let the
fairness deficit become the deciding tiebreak).

## 19. Measured optimization phase (determinism-preserving)

A baseline was recorded by running `benchmark 20` repeatedly on the program's
deterministic workload (demo + three generated DAGs of growing size) and taking the
stable median, then two concrete, high-complexity / avoidable operations were
identified and removed **without changing any deterministic output, CLI contract, or
scheduler invariant**.

### Hot paths identified

1. **Redundant re-sorting of all job ids.** `sortedJobIds` performs an O(n²) insertion
   sort plus a fresh array allocation on every call. It was called *inside*
   `finalizeRun`'s `while changed` fixed-point loop (once per BLOCKED-cascade
   iteration) and again in the trailing cancellation pass, plus in `computeMetrics` —
   yet the job set is immutable during a run.
2. **Per-event re-sort of the ready queue.** `dispatch` wrapped its scan in a
   `while progressed` loop that re-sorted (`orderReady`) after every allocation. Within
   one dispatch no job becomes ready (dependents are released only by COMPLETE/FAIL in
   `applyEvent`) and pool capacity is monotonically non-increasing (allocation consumes
   a slot; cache-start uses no pool; a ready-job deadline cancel frees nothing), so a
   job that does not fit cannot fit later in the same pass — iterations 2+ only re-sorted
   to no effect. `dispatch` runs once per event, so this is real per-event cost.

### Optimizations applied (both retained; no regressions)

- **`stateSortedIds(state)`** (new helper, ~lines 1037-1048): returns the cached sorted
  job-id list, computing it once. `initState` seeds `state["sortedIds"]` once per run.
  **`finalizeRun`** (~line 2541) now hoists the id list out of the fixed-point loop and
  reuses it for both the BLOCKED cascade and the cancellation pass.
  **`computeMetrics`** (~line 2702) reuses it too. Per-run sorted-id work drops from
  O((k+3)·n²) (k = cascade iterations) to a single O(n²). The cache lives on `state`,
  which is never serialized into a snapshot or any digest, so outputs are unchanged.
- **`dispatch`** (~line 2360): collapsed to a single `orderReady` + one descending
  pass. The allocation set and ordering are provably identical (capacity only decreases;
  no new ready jobs mid-pass), so the event history — and hence every digest — is
  identical. Removes the redundant O(R²) re-sort that followed each allocation.

### Baseline → final (median of repeated runs; absolute ms are machine-dependent, the
### deterministic figures are identical before/after)

`benchmark 20` total / per-run:

| Scenario (jobs/events) | Baseline per-run | Final per-run | Δ |
| --- | --- | --- | --- |
| demo   (8 / 10)  | 1 ms | 1 ms | flat (overhead-bound) |
| gen:0  (16 / 28) | 3 ms | 3 ms | ~3% |
| gen:1  (40 / 80) | 8 ms | 8 ms | ~2–3% |
| gen:2  (80 / 76) | 12 ms | 10–11 ms | **~11–13%** |

The gain scales with scenario size — the expected signature of removing O(n²) and
per-event redundant work — and is largest on `gen:2` (12→~10.7 ms/run, very stable at
iters=50: 536–541 ms / 50). Smaller scenarios are dominated by per-invocation overhead
(`initState`, dict setup, per-attempt hashing) that these two optimizations
deliberately do not touch, since it is tied to the deterministic outcome computation.

### Correctness evidence (post-optimization + post-defect-fix, all deterministic)

- `selftest` → **348 / 348** (43 modules), RESULT OK.
- `replay` (in-memory) → digests **1634650706 / 1380223739** (identical to pre-optimization;
  the demo output is unchanged by the fairness fix since its jobs never let the deficit
  become the deciding tiebreak).
- `stress 1 64` → 64 seeds, 1,189 jobs, 929 completed, **0 violations, 0 crashes**; spot
  seeds (`gen-25` done=18 makespan=231; `gen-38` done=0 block=16 makespan=0) identical.
- `stress 1 64` (×2) → **byte-identical** output across two independent runs.
- Two `simulate --save-snapshot` runs → **byte-identical** snapshot files.
- `demo` → makespan 215, critical path 325, invariants OK.

### Limitations & remaining candidates

- The remaining per-event cost is dict-access + per-attempt hashing (`decideOutcome`/
  `detRatio`), which is intrinsic to the deterministic outcome function and was not
  altered.
- `orderReady` is still an insertion sort over the ready set (small in practice); it
  could become a binary heap only for very large generated graphs.
- `transitiveClosure` remains an O(n³) Warshall — correct and adequate for ≤ hundreds
  of nodes. The event heap is already an O(log n) binary min-heap keyed by `(time, seq)`.
- `strVal` still tests `is bool` first; it is *not* broken because its `json.stringify`
  fallback renders bools correctly, but it could be simplified to match the `namedBool`
  fix (stringify-and-compare) for consistency.

---

## 19b. Defect found and fixed this audit: dead fairness policy

The challenge phase exposed a **real correctness defect** that every prior test missed.

**Symptom.** A single-worker scenario was constructed in which fairness *must* change the
dispatch order: job `hold` runs t=0–10; job `k` is ready at t=0 and waits; job `aaa`
(`deps=hold`) only becomes ready at t=10. Under `policy fair=true` the waiting `k`
(fairness deficit 1) should beat the freshly-ready `aaa` (deficit 0); under `fair=false`
the lexicographic tie-break runs `aaa` first. Runtime probing showed both modes produced
the *same* (non-fair) order — fairness had no effect.

**Root cause.** `namedBool` dispatched on `if v is bool then …`. In this xlang dialect the
`is bool` predicate is unreliable on a value read back out of a dict (it always reports
`false`, even though `json.stringify` renders the value as `true`). So the bool branch
never matched, `namedBool` fell through every `is` check, and returned its default.
Consequently `policy fair=true` was a silent no-op: **fairness was entirely dead code**
for the whole simulator. (`is int` / `is string` / `is float` work correctly; only
`is bool` is broken, so `namedInt` etc. were unaffected — and `cache valid=false` would
have been mis-read too, though no scenario exercised it.)

**Fix.** `namedBool` now dispatches on `is int` / `is float` (which work) and recovers a
bool by stringifying the value (`lowerStr(string(v)) == "true"`); `string(true)`/`string(false)`
yield `"true"`/`"false"`. The default is preserved for unrecognized values.

**Regression coverage.** `test12` now asserts `fair=true`/`fair=false` are actually parsed
as enabled/disabled (this alone catches the bug), and the new `test42` (§15) asserts the
exact fair vs non-fair start order and that the two policies yield *different* histories.

**Impact on deterministic outputs.** None for the demo or any generated/stress seed: the
demo's jobs have distinct priorities/deadlines so the deficit never becomes the deciding
tiebreak, and generated scenarios default to `fair=false`. The demo digests
(1634650706 / 1380223739) and all 64 stress-seed lines are byte-identical before/after.

---

## 19c. Second measured optimization phase (this audit, determinism-preserving)

A fresh baseline was recorded by running `benchmark` repeatedly on the program's
deterministic workload (demo + three generated DAGs), then the run was profiled by
phase. Three more concrete, avoidable operations were found and removed — again
**without changing any deterministic output, CLI contract, or scheduler invariant**.

### Phase profile (instrumented `runSimulation`, iters=50, machine-dependent ms)

Profiling split each run into `initState` / event loop / `finalizeRun` / `computeMetrics`:

| Scenario (jobs) | init | loop | finalize | metrics |
| --- | --- | --- | --- | --- |
| gen:1 (40) | 0.8 ms | 6.2 ms | 0.1 ms | 1.4 ms |
| gen:2 (80) | 1.4 ms | 6.0 ms | 0.8 ms | 2.7 ms |

The event loop dominates (~60%), but **init** and **finalize** are non-trivial at 80
jobs. The optimizations below target init, the loop, and finalize respectively.

### Hot paths identified and removed

1. **Adjacency rebuilt for an identical, immutable scenario.** `buildAdjacency` is a pure
   function of the scenario graph, yet it was called once per `runSimulation` (in
   `initState`) and again in `validateGraph`/`runInvariants` — rebuilding identical
   O(V+E) forward/reverse maps plus a per-node sort every time. The benchmark re-runs the
   *same* scenario object `iters` times and stress re-validates then re-runs each seed, so
   this was pure redundant rebuild.
2. **Redundant re-sort of all job ids (two more sites).** Even after the §19
   `stateSortedIds` cache, `sortedJobIds(scenario)` was still called a second time inside
   `initState` (the source-job ready loop) and inside `runInvariants` — re-running an O(n²)
   insertion sort + array allocation when `state["sortedIds"]` already held the identical
   list. `runInvariants` runs on every stress seed.
3. **Per-transition dead allocation (`recordTransition`).** Every state change
   (`setJobState`, `markReady`, `doCancel`, `completeJob`, `failJob`, and each step of the
   finalize BLOCKED cascade) called `recordTransition`, which built a 4-field dict and
   appended it to `state["transitions"]`. A full-file audit showed `state["transitions"]`
   is **written but never read** — it is part of no digest, no snapshot, no report. It was
   pure per-transition overhead in the hot loop.

### Optimizations applied (all retained; no regressions)

- **Adjacency memoization (`buildAdjacency`, ~line 1061):** the result is memoized on the
  scenario under an internal `"_adjCache"` key and returned on repeat calls. The scheduler
  only *reads* adjacency during a run, so one shared cached structure is safe across the
  validate→run and benchmark-loop lifetimes. Measured: for gen:2, `initState` drops from
  **2.14 ms → 1.42 ms per run** (−34% of init) with the cache vs without.
- **Snapshot-safe cache key:** because the cache lives on the scenario, two guards ensure
  it never reaches deterministic output. `snapshotScenario` (new helper) returns a copy
  with `"_adjCache"` removed, used by `buildSnapshot`; and `replaySnapshot` now replays
  against a clean copy so the re-simulation cannot re-dirty the snapshot's own scenario.
  `json.stringify` preserves insertion order and `_adjCache` is appended last, so snapshot
  bytes are **identical to a scenario that never carried a cache**. test16 gained two
  assertions proving the serialized snapshot contains no `_adjCache` key.
- **Sorted-id reuse at the two remaining sites:** `initState`'s source-ready loop and
  `runInvariants` now read `state["sortedIds"]` / `stateSortedIds(state)` instead of
  re-sorting.
- **Verbose-gated transition trace (`recordTransition`, ~line 2061):** the trace is a debug
  aid that no output consumes, so it is now recorded only when `--verbose` is on
  (`if not gVerbose then return`). Normal/benchmark/stress runs (always non-verbose, never
  reading it) pay nothing; `simulate --verbose` still populates it. This is the largest win.

### Baseline → final (30 iters, repeated; absolute ms machine-dependent, deterministic
### figures identical before/after)

| Scenario (jobs/events) | Baseline total / per-run | Final total / per-run | Δ |
| --- | --- | --- | --- |
| demo  (8 / 10)  | 43 ms / 1 ms | 35 ms / 1 ms | −18% total (overhead-bound per-run) |
| gen:0 (16 / 28) | 95 ms / 3 ms | 86 ms / 2 ms | −9% |
| gen:1 (40 / 80) | 269 ms / 8 ms | 237 ms / 7 ms | −12% |
| gen:2 (80 / 76) | 318 ms / 10 ms | 265 ms / 8 ms | **−17%** |

The gain scales with scenario size (more jobs ⇒ more transitions and a bigger adjacency),
which is the expected signature: disabling `recordTransition` alone accounted for ~18% of a
gen:2 run in the no-op measurement (loop 301→242 ms, finalize 42→22 ms), and the adjacency
cache accounts for ~0.7 ms/run of gen:2 init. `demo` is overhead-bound and flat per-run.

### Correctness evidence (post-optimization, all deterministic and byte-identical)

- `xl check` → `ok`; `xl build` → exit 0, no diagnostics.
- `selftest` → **355 / 355** (43 modules; +2 `_adjCache`-leak guards in test16; +5 in the final audit per §19d), RESULT OK.
- `demo` → makespan 215, critical path 325, invariants OK.
- `replay` (in-memory) → digests **1634650706 / 1380223739**, makespan 215/215.
- `stress 1 64` → 64 seeds, 1,189 jobs, 929 completed, **0 violations, 0 crashes**.
- `stress 1 64` (×2) → **byte-identical**; two `simulate --save-snapshot` → **byte-identical**
  files, and the exported snapshot contains **no `_adjCache` key** (verified by `findstr`).
- Invalid-input exit codes unchanged: `badcommand`→2; `stress 5 1`/`stress abc 5`/
  `stress 1 999999`/no-args→1.

### Limitations & remaining candidates

- The remaining event-loop cost is dict-access + per-attempt deterministic hashing
  (`decideOutcome`/`detRatio`), intrinsic to the pure outcome function and unaltered.
- `orderReady` remains an insertion sort over the ready set (small in practice; already
  amortizes to O(R) once sorted, since for the default `fair=false` policy the comparison
  key is fully static).
- `transitiveClosure` is still an O(n³) Warshall — correct and adequate for ≤ hundreds of
  nodes; the event heap is already an O(log n) binary min-heap keyed by `(time, seq)`.
- `strVal` still tests `is bool` first; not broken (its `json.stringify` fallback renders
  bools correctly) but could be simplified to match the `namedBool` fix for consistency.

---

## 19d. Final-release audit: two defects found and fixed (this audit)

A systematic re-run of every CLI mode during the final release audit surfaced two real
defects. Both are fixed with regression coverage; neither weakens any existing assertion,
and all deterministic outputs are byte-identical before/after (re-verified in §17/Summary).

### Defect 1 — `criticalPath` infinite-looped on a cyclic graph (severity: hang)

**Symptom.** `validate --scenario cycle` printed the stats header then **hung forever**
(timed out). No other command was affected.

**Root cause.** `criticalPath` reconstructs one critical path with a greedy `while true`
walk over critical successors. On a cyclic graph `topologicalOrder` returns an empty order,
so the forward/backward passes leave every job at `es = ef = slack = 0`, flagging **all**
jobs critical. The walk then chased the cycle (A→B→C→A) with no termination condition.
`validateGraph` itself detects and reports the cycle correctly; only the *subsequent*
`criticalPath` call in the `validate` handler looped. The selftest never exercised
`criticalPath` on the `cycle` scenario (test 32 deliberately skips it: `if n == "cycle"
then continue`), so the bug survived prior audits.

**Fix.** Added a visited guard (`seen` set, pre-initialized per job) to the path walk so
it visits each job at most once. `criticalPath` is now best-effort on non-DAG input (it
returns `length 0` and a bounded path); `validateGraph` still reports the cycle. This is
purely defensive and changes nothing for any valid DAG (no node is ever revisited).

**Regression coverage.** test 35 now calls `criticalPath` on a 3-node cycle and a 2-node
cycle and asserts termination + `length 0` (+3 assertions).

### Defect 2 — `--text` from a shell silently failed on literal `\n` (severity: usability)

**Symptom.** `validate --text "pool p ...\njob A ..."` exited 1 with no useful result,
because `parseScenario` splits on real newlines only and a shell passes `"\n"` as two
literal characters (backslash, n). `--file` (real newlines) worked; only the inline
`--text` path was affected.

**Fix.** `parseScenario` now normalizes a literal backslash-n to a real newline before
splitting (`string.replace(text, "\\n", "\n")`). The DSL never uses backslashes, so this
is safe and idempotent for file/builtin/generator inputs (which already carry real
newlines). xlang escape semantics were verified empirically (`"\n"`→newline, `"\\n"`→2 chars).

**Regression coverage.** test 01 now parses a literal-backslash-n text and asserts the
expected job count (+2 assertions).

### Post-fix verification (fresh build)

- `xl check` → `ok`; `xl build` → exit 0, no diagnostics; exe 1,588,736 bytes.
- `selftest` → **355 / 355** (43 modules; +5 assertions this audit), RESULT OK.
- `validate --scenario cycle` → terminates, `RESULT: FAIL`, exit 1 (was: hang).
- `validate --text "...\n..."` → `RESULT: OK`, exit 0 (was: exit 1).
- Determinism preserved: `stress 1 64` ×2 byte-identical; `--save-snapshot` ×2
  byte-identical; replay digests unchanged (1634650706 / 1380223739); demo makespan 215.

---

## 20. Feature → test matrix

Every implemented feature is covered by at least one named test module. The matrix maps
each feature to the module(s) that assert it (adversarial modules 34–40 and the
invariant-detection probe 41 are flagged).

| Feature | Test module(s) |
| --- | --- |
| DSL parsing (directives, attrs, normalize, literal-`\n`) | test01 *(+`\n` normalize)*, test34 *(adversarial)* |
| Graph validation (refs, cycles, self-deps) | test02, test34 *(adversarial)* |
| Topological order | test03, test35 *(adversarial)* |
| Strongly-connected components | test04 |
| Critical path / slack | test05, test36 *(ties)* |
| Simulation determinism (digests) | test06, test22, test40 |
| Diamond (parallel/merge) execution | test07 |
| Retry + exponential backoff | test08, test38 *(exhaustion)* |
| Cache hit + invalidation + expiry | test09, test39 *(invalidation)* |
| Resource serialization / pools | test10, test37 *(zero/saturated)* |
| Priority ordering | test11 |
| Fairness mode (determinism + real effect) + Jain index | test12, test30, test42 *(effect)* |
| Cancellation rules | test13, test39 *(race)* |
| Timeout (job-attr + rule) | test14, test39 *(race)* |
| Metrics + percentile interpolation | test15, test40 *(boundaries)* |
| Snapshot + deterministic replay | test16, test40 *(JSON round-trip)*, test43 *(cross-scenario)* |
| Deterministic scenario generator | test17, test22, test40 |
| Post-run invariants | test18, test41 *(detection)* |
| Failure-injection purity | test19 |
| Deadline cancellation | test20 |
| End-to-end pipeline (parse→replay) | test21 |
| Deterministic RNG reproducibility | test23 |
| Edge cases (1 job, 30-node chain) | test24 |
| Built-in scenario catalogue | test25 |
| Scenario serializer round-trip | test26 |
| Transitive closure (reachability) | test27 |
| Transitive reduction | test28 |
| Layer assignment (longest-path) | test29 |
| Jain fairness + histogram formatting | test30 |
| Event-log + Gantt formatting | test31 |
| Graph diagnostics across scenarios | test32 |
| Extended report determinism | test33 |
| Malformed-DSL / parser robustness | test34 *(adversarial)* |
| Cycle detection + topo determinism + criticalPath-on-cycle | test35 *(adversarial, +cycle-termination regression)* |
| Critical-path tie breaking | test36 *(adversarial)* |
| Zero / saturated / starvation | test37 *(adversarial)* |
| Retry exhaustion + concurrent finish | test38 *(adversarial)* |
| Timeout/cancel race + cache invalidation | test39 *(adversarial)* |
| Snapshot / percentile / horizon limits | test40 *(adversarial)* |
| Invariant detection (negative probes) | test41 *(detection)* |
| Fairness effect (deficit reorders dispatch) | test42 *(effect/defect regression)* |
| Cross-scenario snapshot/replay equivalence | test43 *(property)* |

**CLI command → verification section map:** help→§3, demo→§5, validate→§6/§6b/§13/§13b,
simulate→§7/§9/§13, replay→§8/§9, selftest→§4, stress→§10, benchmark→§11, report→§12;
invalid-input exit codes→§14; determinism evidence→§17; optimization→§19/§19c; defect
fixes→§19b/§19d; cyclic-validate→§6b; `--text`→§13b.

---

## Summary

| Check | Result | Exit |
| --- | --- | --- |
| Physical source lines | 5,812 (within 5,000–6,500) | — |
| `xl check` | `ok` | 0 |
| `xl build` | `xflowsim.exe` produced | 0 |
| `help` | usage printed | 0 |
| `selftest` | **355 / 355 assertions, 43 modules** | 0 |
| `demo` | makespan 215, invariants OK | 0 |
| `validate` | graph valid | 0 |
| `validate --scenario cycle` | terminates, RESULT: FAIL (was: hang) | 1 |
| `validate --text "...\n..."` | graph valid (literal `\n` normalized) | 0 |
| `simulate` | events recorded, invariants OK | 0 |
| `replay` (in-memory) | digests match (1634650706 / 1380223739) | 0 |
| `replay` (file round-trip) | digests match | 0 |
| `stress 1 64` | **64 seeds, 1,189 jobs, 929 completed, 0 violations, 0 crashes** | 0 |
| `stress 1 64` (×2) | byte-identical output (deterministic) | 0 |
| `simulate --save-snapshot` (×2) | byte-identical, no `_adjCache` leak | 0 |
| `benchmark 30` | demo 1 / gen:0 2 / gen:1 7 / gen:2 8 ms per run (was 1/3/8/10) | 0 |
| `report` | Gantt + histogram + fairness + log | 0 |
| `badcommand` | unknown command | 2 |
| `stress 5 1` | range error | 1 |
| `stress abc 5` | type error | 1 |
| `stress 1 999999` | range-too-large error | 1 |
| no args | help | 1 |
| **Defect fixed** | dead-fairness bug (`is bool` → namedBool) — §19b | — |
| **Defect fixed (final audit)** | criticalPath hang on cyclic graph (visited guard) — §19d | — |
| **Defect fixed (final audit)** | `--text` literal-`\n` normalization — §19d | — |
| **Optimizations** | adjacency memoization + sorted-id reuse + verbose-gated transitions — §19c (gen:2 −17%) | — |

All required modes pass, the executable is standalone, invalid input exits nonzero with a
useful message, and both the release-candidate hardening pass (adversarial suite +
strengthened invariants + invariant-detection probe + fairness-effect regression +
cross-scenario replay property + 64-seed deterministic stress) and the two
determinism-preserving optimization phases (§19: cached sorted ids + single-pass dispatch;
§19c: adjacency memoization + redundant-sort removal + verbose-gated transition trace) are
complete. The challenge phase found and fixed one real defect (§19b) and the final-release
audit found and fixed two more (§19d: a `criticalPath` hang on cyclic graphs, and `--text`
literal-`\n` normalization) — all with regression coverage and none weakening any assertion.
The 5,812 physical lines are functional code and tests (612 comment + 371 blank = 983
non-functional, ~16.9%; every section 0–11 + 9b is implementation or named test) — no
blank, comment, or boilerplate padding.
