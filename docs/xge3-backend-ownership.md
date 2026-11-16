# Backend ownership prerequisite

XGE commit `38363a6b0358e8be90948fa7c1005e6167538071` adds independent,
immutable PlatformBackend and GraphicsBackend native references. A reference
copies the original descriptor/name, retains a real callback payload/code
owner and supports sharing, borrowed const description, checked release, and
the existing native graph/preparation/finalization/completion protocols.
Normal reads/share and final cleanup are confined to the creator thread.
Claimed/retiring snapshots reject access. Drop consumes its acquired reference
once even when cleanup reports an error.

Both OS source-unit gates pass O0/O2 lifecycle, trace/protocol, thread and OOM
tests, C11/C++11/C++17 headers and complete engine-source integration syntax.
There are eight commands per OS. The exact 09d-XRT runtime library is shared
with the compiler cohort; no private XRT is compiled into these tests.
The selected immutable native baselines exclude unrelated geometry/text work.
The original root working files and fixtures were preserved.

Creation adds one native allocation for body/lifetime/name storage and copies
the optional name, plus the owner's Hold. Sharing adds a native reference
without copying the descriptor/name or acquiring another payload owner. Thread,
state, graph and reference work remain real costs. There is no language/C CPU
equivalence claim.

This is not yet a public xlang backend class, a rebuilt matched SDK, or a
working host loop. The old native descriptor setters still only store/query
tables; the ordinary Run path does not dispatch them. The public header's new
opaque types/functions change the SDK fingerprint. It must be regenerated
before language delivery; the old profile flags serve only the source syntax
gate and are not presented as new ABI acceptance.

The user has now selected an explicitly driven owning HostSession, preserving
the Sokol Run loop, and approved generic function-pointer calls whose receiver
is a descriptor field such as `pUser`. Those decisions are settled; both
implementations remain required. No forwarding wrappers were introduced, and
no callback dispatch or backend-selection capability is claimed from merely
copying descriptors.

Exact external reports, native source commit, Linux baseline cache archive and
source hashes are indexed by
`dev/release-materials/provenance/xge-backend-owned-20261116.json`.
The permanent Linux validation source is under `/var/tmp/xlang-validation/`;
the archive and its per-file hashes remain on F for reconstruction. Runtime
release/IDE/help delivery and the remaining full-coverage task are still open.

## 2026-11-16 receiver projection update

The generic field receiver is now implemented as
`@c_field_call("pUser", "void*")`. It preserves the logical table scope while
passing its field at the existing native receiver slot, including bound,
protocol and region-observer calls. Windows/Linux each pass 40 build/run gates,
with actual full O2/TCC/XRT/VFS products and separate public-source mutation
tests. See [the contract and limits](c-field-call-receivers.md) and
`dev/release-materials/provenance/xge-field-receiver-20261116.json`.

The user also approved owning callback-group pack projection. Existing pack
callbacks are synchronous only and cannot retain several language closures
behind a common userdata field. The new group contract and owning HostSession
are still to be implemented. Descriptor snapshots and direct field calls alone
do not activate a native host renderer, and no matched SDK/IDE promotion is
claimed by this compiler batch.
