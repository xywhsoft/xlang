# Choice cache and constructor corrections

The matched XGE SDK fixes two kinds of native defects found while validating
the existing declarative xlang surface. No compiler rule or public method ABI
changes in this batch.

CheckBox and Toggle now free their partial text buffers when initialization
fails. The generic construction chain still destroys only successful stages.
Widget-owned cache/events and borrowed Font admission retain their existing
transaction. The native prefix test exercises Button, CheckBox, Toggle and
Radio with complete text descriptors and NULL descriptors, 64 budgets each:
512 cases per run, including cleared outputs and exact heap cleanup.

Button, CheckBox and Toggle now use the existing frame-dependent prepare flag,
as Radio already does. A clean widget subtree must still inspect external
image generations. Previously a cached indicator/icon could stay unchanged
after its Surface was updated. The isolated test uses one control per tree;
siblings cannot mask a missing prepare pass. It first checks unchanged-image
cache reuse, then mutates the source and requires actual cache drawing.

The language fixture uses real native owned Proxy/Font/Surface/Draw resources
over a deterministic CPU test provider. Production methods remain direct
declarations. Each kind has its own Context and checks generation refresh,
64 constructor allocation budgets, old-value preservation, exact heap cleanup
and resource balance. Its idle loop is compared with direct C calls using the
same provider, Context and workload. The same atlas in multiple state slots is
deduplicated by existing native code: 1000 idle frames query its generation
1000 times and perform zero heap operations on both paths. Native slot scans,
tree traversal, foreign thread/liveness checks and call bookkeeping still
run. This is not a CPU-equivalence claim. Other distinct images require their
own native dependency checks.

Existing Button, CheckBox and Toggle lessons run as bounded real windows in
JIT, dynamic AOT and static AOT. Same-platform pixel outputs must match.
Linux uses Xvfb/Mesa; this does not qualify physical GPUs or drivers. The
installed CJK font used for readback is hashed, not redistributed.

The native full Checkbox/Toggle/Radio suites also run. The old Button full
suite requires unrelated newer test-proxy geometry fields; its isolated cache
and constructor tests run without substituting that fixture. A separate scan
of 56 exported NULL-descriptor constructors has found additional initialization
leaks. Follow-up with full Context retirement confirms ten other leaking
constructors and a CodeEdit invalid-free diagnostic at budget 99. Window's
earlier byte delta was retained event-queue capacity, reclaimed at Context
retirement. The corrected diagnostic retains both sets of original reports.
The remaining findings are not covered by this four-control receipt.

Exact inputs, SDK relink/reuse counts and validation commands are recorded in
`dev/release-materials/provenance/xui-choice-cache-20261115.json`. Full
XGE/XUI/Document integration and matched shipping delivery remain in progress.
