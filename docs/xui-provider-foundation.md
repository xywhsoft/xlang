# Code provider native prerequisite

This is the lifetime-only baseline. The successor
`xui-provider-owned-foundation.md` adds five owned native registrations and
result-owner pinning, with a larger matched model/scope ABI.

The native XUI provider set now has one destroy right, checked consumed
destruction/clear, lifetime watches, address-stable call scopes and quiescent
ownership inspection. CodeEdit claims its internal set; `GetProviders` is a
borrow and cannot destroy that owner. Standalone raw C sets retain their
externally synchronized, quiescent thread-transfer behavior.

The five request paths preserve their original typed C callbacks, snapshot the
function/user pair, reject publication after callback-triggered editor or
Context close, and reset failed outputs. Completion, signature and menu-title
consumers keep the whole editor operation alive through result consumption.
Menu callback errors are reported. A menu title may already be assigned when
its subsequent menu update fails; status does not imply rollback.

A deferred Context close still owns its resources. New operations fail, but
quiescent ownership inspection must remain possible until explicit checked
destruction consumes it. This is separate from closing an editor, whose
physical destruction runs at the protecting outer operation's tail.

This batch adds no public `CodeProviderSet` language class. Native callback
userdata, returned strings and parameter arrays still belong to the raw C
provider. Owning language callbacks/results and long-lived internal model
aliases require their remaining contracts. Exposing these as a callback-free
leaf would hide language references and be incorrect.

The matched hosted/static SDK packages reuse the existing declarative
bindings, without XGE forwarding functions or a second XRT implementation.
Existing CodeModels, text reads, standalone CodeMargin and CodeEdit offset
clients are checked in JIT, dynamic AOT and static AOT. Native provider tests
cover callback reentry/close, ambient errors, ownership/thread/scope rules,
constructor allocation prefixes and whole-heap balance on Windows/Linux.
The machine-readable receipt is
`dev/release-materials/provenance/xui-provider-foundation-20261115.json`.

The x64 native model is 120 requested bytes in one allocation, compared with
the measured Windows baseline of 80. Native scopes occupy 72 stack bytes;
warmed 1000-scope/request loops have no heap operations. Checks, thread
queries, mutation admission, diagnostic references and Context bookkeeping
remain real costs. This is not a zero-overhead or full ecosystem completion
claim. This batch does not install release binaries or update the mutable
delivery index.
