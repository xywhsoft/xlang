# Owned CodeProvider native foundation

The matched XUI SDK adds five typed native registrations: Completion, Hover,
Signature, Command and CommandEnabled, each suffixed `Owned`. Their immutable
registration nodes hold real callback owners and appear in the ownership graph.
Clear, replacement and destruction detach first; active request scopes retain
the exact selected nodes through result consumption. Legacy C callbacks keep
their external ownership rules.

Completion, Hover and Signature return borrowed strings/parameter arrays. Owned
registrations require an outer native Provider scope; without it the request
is rejected before invoking the callback. CodeEdit's internal completion and
signature consumers open that scope and copy/use results before ending it,
including requests originating from editor events. Self-removal therefore does
not release callback backing storage before CodeEdit has copied it. Scalar
Command/CommandEnabled requests do not need another scope. Caller-scope
generation checks reject end/reuse reentry; editor/Context close still rejects
publication. Checked outcomes retain diagnostics and consumption semantics.

This is not a public language `CodeProviderSet` class yet. Owning callback output
marshalling and a long-lived alias of an editor's internal model still need
their separate general contracts. The approved C-string *field* snapshots are
already implemented, including `ErrorInfo`'s `string?` fields; they do not solve
callback-returned strings, writable arrays or arbitrary pointer records.

Native O0/O2 hosted/static tests cover all five roles, self-clear/self-destroy,
Hold/Drop failure/reentry, ambient errors, graph edges, missing scopes,
scope-generation reuse, zero-budget hot requests and complete-heap allocation
prefix cleanup through real CodeEdit consumers. Original raw callbacks,
constructor retirement/OOM and owned Margin behavior remain in the same gate.
Windows/Linux language regression uses the existing complete O2 compiler cohorts
with freshly staged matched hosted/static SDKs: CodeModels, text reads, Margin,
CodeEdit offset queries and C-string field/error callback snapshots, including
the latter's three execution modes and linear console example.

The x64 model is one 176-byte allocation, previously 120; each owned node adds
one 56-byte allocation and one owner Hold. Its scope occupies 136 caller stack
bytes, previously 72. Warm 5000-request native loops allocate nothing and do not
repeat the owner's Hold. Reference counters, five-slot snapshots, generation,
thread/state checks, mutation admission, errors and Context protection remain
work. No CPU-equivalence claim is made. Language field snapshots still scan and
copy present strings; their established copy costs are unchanged.

The public native scope layout changed, so use the matching rebuilt SDK/header.
The isolated SDKs are qualified against the canonical shared XRT and static
external-XRT contract; they do not add another XRT implementation. This batch
does not install release binaries, change the version/current-delivery index
or publish online. Full XGE/XUI/Document and final compiler/SDK/IDE/help delivery
remain active. See the pinned machine-readable receipt
`dev/release-materials/provenance/xui-provider-owned-20261115.json` and the
native `docs/code-provider-owned-callbacks.md`.
