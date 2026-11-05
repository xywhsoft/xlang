# XUI composite manager bindings

This source milestone covers MsgBox, Toast, FileDialog and MsgTip. It uses the
real native objects and direct declarations; it is not the final release SDK.

## Ownership and callbacks

A Context uniquely owns each manager in its intrusive native directory. The
language value is a composite child view of that real Context root. Releasing
the last language child does not silently destroy its native manager. Explicit
`close()`, primary Widget destruction, and Context teardown request logical
close. A manager watch invalidates the language view before native storage can
be freed; actual cleanup waits for active native callback tails and any retained
layout/paint to become ready. The pending work remains in the existing Context
directory, with no independent manager RC, fake leaf, or global retirement queue.

Context traces each actual callback, Font, Surface and paragraph Layout owner
once. Child policies do not duplicate those graph edges. Callback registrations
use exact typed Owned setters, separate callback/user/owner data, Hold before
publication, and physical snapshots through the native callback return tail.
FileDialog result/context-menu paths have an independent copied owner until
Hold, callback and Drop all finish, so reentrant directory replacement cannot
invalidate the callback input. That input is a callback-epoch CString; use its
explicit copy operation to retain it as language text.

The public constructors use declarative `@c_pack` to fill the real descriptor on
the stack and call the native factory. A safe Context requires an owned Proxy:

```xlang
auto proxy=xui.createProxy()
auto context=xui.create(proxy)
auto message=xui.createMsgBox(context,"Title","Body")
message.setModal(0)
message.setOpen(1)
```

MsgBox construction keeps the native modal default of 1. The descriptor has a
separate `bHasModal` bit; an earlier candidate's modal constructor argument was
ignored and has been removed. Use `setModal()` before displaying the manager.
The three FileDialog convenience factories are direct declarations of native
open/save/folder business operations, which intentionally force modal mode.

## Borrowed native getters

All 81 pure manager Get/Is functions use creator-first, read-only admission.
They allocate nothing and do not enter/leave a Context operation, drain pending
retirement, dispatch callbacks, or execute user code. A native borrowed result
remains valid until the next operation that can change or destroy its owner.
The language's existing outer Context scope keeps its copy/Retain window open.
Lazy layout, mutation, constructors and callback consumers still use complete
native scopes. Native raw callers must hold a valid receiver and obey creator
thread affinity; the pure check is not permission to use a freed C pointer.

The regression creates a real Context-owned pending peer whose final owner Drop
would destroy the queried receiver. All four pure getter families return without
running that Drop. The next explicit operation performs cleanup exactly once.
The same test rejects worker-thread reads before mutable UI state is inspected
and preserves ambient errors. The old candidate fails this exact regression.

## Costs

| Mechanism | Accounted cost |
| --- | --- |
| Native unique-manager directory node | 72 bytes per manager; no node allocation |
| Whole-manager operation scope | 64 bytes of stack state; existing Context/mutation bookkeeping |
| Installed Owned callback | 32-byte native owner record; real Hold/Drop and graph edge |
| Lifetime watch | Existing 32-byte intrusive link per actual observer |
| FileDialog result/context notification | One path allocation and `length+1` copy per notification, freed after callback tail |
| Paragraph rendering | A reusable actual Layout owner slot; real Font/Paint retention and prepare/cleanup |
| Pure native getter | Creator/thread and logical-live checks; no scope, mutation gate, allocation or cleanup tail |
| Language child values/closures | Existing ForeignView/watch/CodeLease and closure lifetime work; not claimed free |

There is no per-method C forwarding wrapper, opcode dispatch, or dynamic boxing
added by this binding. This does not claim that ownership/callback machinery has
zero total overhead or provide a device performance measurement.

## Qualification and remaining boundary

The frozen native candidate is `native-managers-final-overlay-r5`, a 13-file
manifest over the shared Ambient r9 baseline plus its exact Core10 requested-queue
fix. It recompiles seven production units, including TreeView's private part
factory; all other objects retain their recorded hashes. Public/private layouts
and the normally derived SDK fingerprint stay unchanged. Production DLLs contain
no FileDialog test exports; the legacy pure-path test seams are compiled only
into their separate test executable.

The final batch runs GCC O0/O2 native publishers, the existing four-family
regressions, pure getters, descriptor-Pack language calls, allocation-budget OOM,
owned callback exceptions, and two real JIT modules with Manager/Context closure
cycles. The latter outlive their compilers, preserve the other module's live
global during the first cleanup, and unload native code after final cleanup.
The Linux JIT test explicitly selects the already fixed source TCC provider;
its older embedded VFS is not certified as a current standalone AOT provider.

No real window, GPU, network or OS clipboard is exercised. FileDialog's native
convenience factories run with the CPU provider in native tests; their language
ABI probe is compiled but deliberately not executed with the device backend.
The language OOM callback can use a resident static OOM diagnostic; native
false-with-NULL cleanup is separately covered by actual native publisher tests.

The only remaining safe API projection gap in these four families is MsgBox
`SetCustomButtons`: its descriptor array contains C-string pointers. It is not
exposed as bytes or an untyped point. Existing preset buttons, title changes,
modal state, metrics/colors, icons, complete lifecycle, typed callbacks and all
other safe native capabilities are represented. General safe descriptor-array
projection remains a separately recorded prerequisite. Legacy raw callback and
void destroy entries are replaced by their Owned/checked forms.
