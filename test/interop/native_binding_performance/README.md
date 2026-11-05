# Native binding CPU comparison

This fixture compares raw C, handwritten checked C and xlang using the same
native SDK, CPU backend, workload and selected O2 runtime closure. It opens no
window, uses no graphics device and performs no network operation.

The raw program calls native functions or function-pointer fields directly.
The checked C program uses the resident view/error/retained-result protocols on
successful calls. Assertions terminate a failed C fixture; xlang instead runs
its normal checked failure path. The generated program also retains ordinary
language loop and value semantics. Ratios therefore describe these concrete
programs, not a universal cost of OOP syntax or an exact failure-path equivalent.
The C checked setup deliberately omits generated factory pre-reservation and
does not claim equivalent cold failure handling.

`prepare.py` takes `--root`, a verified full source `--build` manifest or an
explicit partial `--overlay`, matched
`--packages` and native `--engine` build directories, and a fresh `--out` path.
It supports `--fixture warm` and `--fixture shape`. The complete compiler is
used only by the offline IR/runtime planner. The timed executables contain
selected resident sources and one selected XRT implementation, all compiled
at O2. They do not link the compiler's object list or xl.dll. Selected sources,
objects, flags, schemas, binaries and dependency inputs are individually pinned.

The warm workload performs 500,000 capability reads, 500,000 fixed-rectangle
draw calls and 50,000 retained target queries. The shape workload builds a real
Native-owned Shape containing 4,096 clusters and 4,096 carets over 8,192 explicit
bytes, including NUL. It closes the original Shape/Font/Proxy references before
40,960,000 typed element visits. A rolling checksum prevents reduction hoisting.
The checked and language sequence owners preserve the real native reference.

Run `sample.py --preparation <out>/preparation.json --out <fresh-directory>`
after preparation. It refuses counter binaries, verifies input hashes, runs two
warmup rounds and twelve measured rounds, and rotates all six execution orders.
It keeps raw ticks, clock frequency, checksums, median, quartiles and range.
The measured intervals exclude process startup, setup, printing and cleanup.
CPU scheduling and background activity still affect the distributions.

Run a separate preparation with `--counter` for allocation/reference evidence.
Its timings are never performance results. Native and resident allocations,
explicit close and automatic function/module cleanup are separate phases.
Peak counters are process-wide peaks, not per-phase peaks. The static backend
requires no dynamic code lease; dynamic module lifetime is tested separately.

Do not describe the entire binding as zero cost. Direct declarations remove
per-method forwarding functions, but checked scopes, physical pins, retained
outputs and array projections have their own costs. A NativeSequence loop has
no per-element native getter, dynamic boxing or allocation; acquisition and one
read scope per loop remain real work. CPU backend timings do not predict GPU
or end-to-end UI throughput.
