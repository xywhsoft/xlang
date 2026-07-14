# XFlowSim

**XFlowSim** is a deterministic workflow and resource-scheduling simulator written
in [xlang](../../../README.md). It models a directed-acyclic-graph of jobs that run on
bounded resource pools (CPU cores, memory, worker slots), with priority/fairness
arbitration, retries with exponential backoff, timeouts, cancellation, caching with
invalidation, and deterministic failure injection. Every random-looking decision is a
pure function of `(seed, jobId, attempt)`, so an identical `(scenario, seed)` always
reproduces a byte-identical execution.

The whole simulator is a single, self-contained source file (`xflowsim.xl`,
~5,800 physical lines) that compiles to a standalone executable with no external
dependencies at run time.

---

## Quick start

All commands are run from the `release/` directory (so the compiler can locate its
standard library). The standalone executable runs from anywhere.

```text
release> xl.exe check  projects\dev\xagent5000\xflowsim.xl          # type-check the source
release> xl.exe build  projects\dev\xagent5000\xflowsim.xl -o projects\dev\xagent5000\xflowsim.exe
release> projects\dev\xagent5000\xflowsim.exe help                  # list commands
release> projects\dev\xagent5000\xflowsim.exe selftest              # run the built-in test suite
```

`check` must print `ok`; `build` produces `xflowsim.exe`; `selftest` reports
`RESULT: OK` with all assertions passing.

---

## Commands

| Command | Description |
| --- | --- |
| `help` | Show usage and the option reference. |
| `demo` | Run the built-in **demo** scenario and print the full report (graph analysis + schedule + utilization + critical path + per-job table) followed by an invariant check. |
| `validate [--scenario NAME] [--file PATH] [--text STR] [--seed N] [--json]` | Parse a scenario and validate its graph: missing refs, duplicate ids, cycles, topological order, strongly-connected components, critical path, sources/sinks. |
| `simulate [--scenario NAME] [--file PATH] [--text STR] [--seed N] [--verbose] [--json] [--save-snapshot PATH]` | Run a scenario end-to-end through the discrete-event scheduler and print the full report + invariants. `--save-snapshot PATH` exports a replay snapshot file. |
| `replay [FILE]` | With no file, build + replay the built-in demo snapshot in memory and prove determinism. With a file, read a previously exported snapshot and replay it. |
| `selftest` | Run the built-in test suite (43 modules, 355 assertions). |
| `stress <first-seed> <last-seed> [--scenario NAME] [--layers N] [--perLayer N]` | Generate and simulate every seed in the inclusive range and check post-run invariants + crash-freedom. |
| `benchmark [iters]` | Time the scheduler across the demo plus several generated scenarios. |
| `report [--scenario NAME] [--file PATH] [--text STR] [--seed N]` | Full report: summary + ASCII Gantt timeline + completion-time histogram + Jain fairness index + event log. |

`validate`, `simulate`, and `replay` all default to the built-in **demo** scenario, so
they need **no external files**. Input can alternatively come from `--scenario NAME`
(one of `demo, diamond, retry, cache, cycle`), `--file PATH`, or `--text STR`.

**Exit status** is `0` on success and **nonzero** on invalid input or any failed
invariant (unknown command → 2; bad args / failed invariant → 1).

---

## Determinism guarantee

- The only source of "randomness" is a counter-based PRNG seeded by `hash.u32`/
  `hash.u64` over `(seed, jobId, attempt)`. There is **no** dependence on wall-clock
  time or on the order events happen to be popped from the heap.
- The event heap is a binary min-heap keyed by `(time, seq)`, so ties are broken by a
  monotonically increasing sequence number — fully deterministic.
- Resource arbitration uses a total order: priority desc → deadline asc →
  (fairness deficit desc, when `fair=true`) → duration asc → job id asc.
- A snapshot carries the canonical scenario + seed + a digest of the event history and
  of the final state. `replay` re-runs the simulation and compares digests
  byte-for-byte; two runs of the same `(scenario, seed)` are identical.

---

## Scenario DSL

Scenarios are plain text. Directives (one per line, `//` is the comment marker — note
`#` is *not* a comment, it is a time-literal prefix in xlang):

```text
name demo
seed 7
horizon 12000
unit ms

pool cpu workers=4 cpu=8 mem=16384
pool gpu workers=1 cpu=4 mem=32768 tags=gpu

job fetch  cpu=1 mem=256  dur=40  prio=3 deadline=2000  cache=fetch.v3
job build  cpu=3 mem=2048 dur=120 prio=5 deadline=3000  deps=fetch
job ut     cpu=2 mem=512  dur=70  prio=4 deadline=4000  deps=build
job pack   cpu=1 mem=512  dur=30  prio=2 deadline=6000  deps=ut,it,gpu retry=2 backoff=15
edge build gpu
fail job ut rate=0.3 effect=fail
fail random rate=0.05
invalidate fetch.v3 on fetch fail
cache fetch.v3 value=artifact valid=true
policy fair=true tiebreak=prio-deadline-dur-id cacheDefault=true
```

| Directive | Meaning |
| --- | --- |
| `name / seed / horizon / unit` | Scenario metadata. |
| `pool ID workers=N cpu=N mem=N [tags=T]` | A resource pool. |
| `job ID cpu=N mem=N dur=N [prio=N] [deadline=N] [pool=P] [cache=K] [retry=N] [backoff=N] [backoffmax=N] [timeout=N] [deps=A,B] [tags=T]` | A job. |
| `edge FROM TO` | An explicit dependency edge. |
| `fail job\|pool\|random [TARGET] rate=R [effect=fail\|timeout]` | Failure-injection rule. |
| `cancel JOB at=N` | Cancel a running/ready job at a virtual time. |
| `invalidate KEY on JOB EVENT` | Invalidate a cache key when JOB reaches EVENT. |
| `cache KEY value=V valid=B [expiry=N]` | Initial cache entry. |
| `policy fair=B tiebreak=S cacheDefault=B` | Scheduler policy. |

`scenarioToText(scenario)` emits a canonical, re-parseable serialization of any
scenario (used by the round-trip tests and `report`).

---

## Architecture (`xflowsim.xl`)

The file is organized into numbered sections:

| Section | Responsibility |
| --- | --- |
| 0 — Constants | Tunable limits and defaults. |
| 1 — Global state + utilities | Global counters, string/dict/array helpers, sorting, JSON value formatting. |
| 2 — Deterministic PRNG + failure injection | Counter-stream `Rng`, `detRatio`, `decideOutcome`, cache validity. |
| 3 — DSL parser | Tokenizer, directive parsers, `parseScenario`. |
| 4 — Scenario model + built-ins + generator | Finalization, adjacency, the five built-in scenarios, `generateScenario`. |
| 5 — Graph analysis | Validation, iterative DFS cycle detection, topological sort, Tarjan SCC, critical path / slack. |
| 6 — Discrete-event scheduler | Binary min-heap, ready queue, dispatch, resource arbitration, retry/backoff, timeout, cancellation, cache, the event loop. |
| 7 — Metrics + reports | Utilization, latency/throughput, percentiles, summary/full reports, invariants. |
| 8 — Snapshot / export / replay | Snapshot build, JSON text, digest, write/read, deterministic replay. |
| 9 — CLI commands | Flag parsing, scenario resolution, every command handler. |
| **9b — Extended utilities** | Canonical serializer, transitive closure, transitive reduction, layer assignment, Jain fairness index, histogram, event-log formatter, ASCII Gantt timeline, `report`. |
| 10 — Test suite | 43 modules / 355 assertions covering every product path, including an adversarial/property-style hardening suite (tests 34–43). |
| 11 — Entry point | `main` command dispatch. |

**Data flow:** `parseScenario` → `validateGraph` → `runSimulation` (event loop mutates a
single `state` dict) → `computeMetrics` → reports / invariants / snapshot. The state
model uses a single module-level `state` dict (keyed access via `state[k]`) rather than
a class, which sidesteps the native-backed copy semantics of xlang classes.

---

## Built-in scenarios

| Name | Structure |
| --- | --- |
| `demo` | 8-job pipeline: fetch→build→{ut,it,gpu}→pack→sign→report, 2 pools, cache, retries, failure injection. |
| `diamond` | Classic 4-node diamond A→{B,C}→D. |
| `retry` | 3-stage chain with per-stage failure rates (retry/backoff). |
| `cache` | Producer/consumer sharing a cache key with invalidation-on-fail. |
| `cycle` | Intentionally cyclic — used to exercise validation (invalid). |

---

## Files

| File | Description |
| --- | --- |
| `xflowsim.xl` | The simulator source (single file, ~5,800 lines). |
| `xflowsim.exe` | Standalone executable produced by `xl build`. |
| `README.md` | This file. |
| `VERIFICATION.md` | Exact commands and recorded results for every verification mode. |

See `VERIFICATION.md` for the full, reproducible verification transcript (commands,
stdout, and exit codes for `check`, `build`, `help`, `demo`, `validate`, `simulate`,
`replay`, `selftest`, `stress`, `benchmark`, `report`, and the invalid-input cases).
