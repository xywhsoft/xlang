# FlowGraph configuration callback Busy contract

The accepted rule is implemented upstream in XGE commit `9564879c`.
While copying or dropping a model-owned configuration invokes custom XRT value
callbacks, ordinary reentry into that same model returns `XUI_ERROR_BUSY`.
Independent models remain usable. Creator-thread or external serialization is
still required; the change adds no cross-thread sharing guarantee.

All 63 integer entrypoints taking an existing graph check Busy before reading
other arguments or changing outputs. This includes configuration reads/writes,
command snapshots, undo/redo, deletion, history cleanup and checked destruction.
The three new APIs are `xuiFlowGraphDestroyChecked`,
`xuiFlowGraphGetRevisionChecked` and `xuiFlowGraphGetCleanRevisionChecked`.
Rejected queries preserve their output; rejected destruction preserves the model.
Integer count/find/boolean queries can return a negative Busy status.

The legacy void/unsigned functions refuse the operation and report an XRT state
error with domain `xui.flow_graph` and code -17 if there is no earlier error.
They preserve an existing error. Ownership tracing during a configuration
callback fails without invoking its visitor; tracing requires quiescence.

The 30 integer Workflow entrypoints also reject access to that model during its
configuration callbacks. Staged replacement carries the logical destination's
callback identity through copying the new configuration and releasing the old
one. Legacy Workflow destruction cannot free the owner in that interval, and
its graph getter returns NULL with the same error-reporting rule.

The regression additionally exposed a separate valid-input bug: optional node
coordinates and edge-route numbers were read with checked numeric getters even
when absent/null, leaving a spurious XRT error after a successful load. Missing
fields now preserve their zero defaults without invoking those getters.

The source gate is `D:/GIT/xge/tools/test/check_flow_graph_reentry.py`. It checks
the entrypoint inventory against the actual public header, output canaries,
independent and nested models, legacy error preservation, history and cleanup,
workflow replacement, 48 command and 128 replacement allocation-failure prefixes,
and repeated heap balance. Existing model/schema/XSON tests run at O0 and O2.

`tools/check_xui_flow_busy_sdk.py` validates real matched host/static SDKs with
the canonical XRT from the complete qualified compiler, without compiling a
substitute model implementation into those clients. Five private checks
remain in the source gate and are explicitly excluded from public DLL clients.
The SDK gate also includes the existing graph-widget suite, generated C/C++
headers, and JIT/dynamic/static AOT with direct declarations and import checks.
The test-only custom native value keeps its borrowed graph in a controlled
synchronous scope; it is not a public retained-callback constructor.

The legacy Schema suite returns success while leaving `invalid argument` in
XRT after negative getter cases. The gate compiles the byte-identical old suite
against the frozen predecessor model/workflow and compares this exact pending
error with every new SDK result before freeing it and checking heap balance.
This documented legacy behavior is not claimed fixed. The new Busy fixture
still requires a clean error state; its assertion is not relaxed.

The native model grows from 928 to 944 bytes on the measured Windows x64 ABI:
16 additional bytes for callback depth, replacement-owner pointer and padding.
Copy/Drop brackets update these counters; ordinary entries perform a busy check.
There is no added config clone, lock or global exclusion. Checked Busy rejection
allocates nothing; legacy error reporting can allocate. These facts do not
establish CPU equivalence or zero bridge overhead.

The previous invalidation-only stage remains recorded in
`xui-flow-config-sdk.md`; its old fixture/report bytes are retained. The new
three declarations change the SDK fingerprint, so the new candidates require
complete O2 host/static rebuilds instead of reusing incompatible old objects.
Storage-review updates are limited to two header witnesses: a separate audit
confirms that existing physical declarations and record layouts are unchanged.

This prerequisite does not publish a default `xui.FlowGraph` class or certify
general Workflow callbacks, implicit cycle retirement, retained code/resource
trees, canvas ownership transfer or long-lived Widget/Workflow aliases. In
particular, a model Busy guard alone does not make destruction/replacement of
every owning widget safe. Those integration contracts remain separate work.
Candidate SDKs, raw logs and source snapshots stay external. Final release,
IDE/catalog installation and the full XGE/XUI/Document goal remain active.
The exact reports and acceptance totals belong in this batch's small provenance
receipt, not in the ignored runtime tree.
