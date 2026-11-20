# Owning explicit host backends

`lib/bindings/xge/src/backend.xl` targets native XGE
`466a1b003db896bbeb1dec10d624619ca34a7b45` and its pinned XRT producer
`4641cd2e4da09d8b1a66872cabe085c263d8b5ba`. It requires the compiler's generic
owned callback-group pack contract and a matching complete native SDK. This is
a qualified source batch, not a replacement of the installed runtime/IDE or
completion of the whole XGE integration.

`HostSessionDesc` exposes the pointer-free native configuration as an ordinary
value: `size` and `framebuffer` are `c_uint`, `width` and `height` are `c_int`,
and `dpiScale` is `c_float`. Copies are independent. This value holds no callback,
owner, context or GPU resource. Direct C consumers require `size` initialized
to their native `sizeof(xge_host_session_desc_t)`; the normal scalar-argument
`createHostSession` factory initializes that size through its existing pack
contract. A descriptor value by itself does not initialize a renderer.

`tools/check_xge3_host_value.py` checks C/C++ headers and actual C++17 O0/O2
archive calls in Windows/Linux JIT and independent dynamic/static AOT. It covers
20-byte value returns, pointer outputs, independent copies, dynamic boxing,
failed assignment and high-bit unsigned framebuffer values. The warmed
1,000-call language/native intervals have enabled XRT accounting and require
zero allocation/free/reallocation operations. Stack/value copies and checked
call boundaries still cost work; CPU equivalence is not measured.

## Direct declarations and ownership

`createPlatformBackend` builds an immutable platform table from kind, name and
init/unit/poll/present callbacks. `createGraphicsBackend` builds an immutable
graphics table from init/unit/begin/end callbacks. Each table's callbacks share
one owned environment through `pUser`. Native tables retain its graph edge and
code lifetime; the compiler generates typed callback entries. There is no XGE
method forwarding wrapper.

Both backend classes support `share()` and `close()`. Sharing acquires a real
independent native reference. A session retains its own references, so closing
the original backend views does not invalidate its tables. These objects and
the session belong to their creation thread.

`createHostSession(width,height,dpiScale,framebuffer,platform,graphics,
makeCurrent,isCurrent,release)` uses the system GL loader. Width/height are
physical framebuffer pixels; scale is separate. Initialize XGE in manual mode
first. The application supplies real context hooks and an accurate release
result. This drives an externally provided GL 3.3 or GLES 3 context; it does not
create an OS window.

The optional last argument is `GlProcResolver? resolver=null`. A custom resolver
directly enters a generated owned callback, using the same hooks group's
`pUser` and the native userdata-last ABI. Its `CallbackProcName` argument is
valid only during that callback. `name.length()` directly calls `strlen` and
`name.compare(text)` calls `strcmp`; neither copies the symbol name to a
managed string. Saving this view does not extend the native name's lifetime:
later access fails checked. The resolver returns a raw procedure `point`, as
required by the host ABI. The host must provide a compatible current context,
correct function addresses and retain any library supplying them; arbitrary
pointer values are not validated by the binding. This is an explicit native
host integration contract.

```xl
// Declare your own OS/context lookup function in its matching public header.
extern "hostFindGlProc" @header("my_host.h") @no_raise func findProc(xge.CallbackProcName name) as point

auto session=xge.createHostSession(width,height,scale,framebuffer,
 platform,graphics,makeCurrent,isCurrent,release,
 func(xge.CallbackProcName name) as point
  return findProc(name)
 end)
```

Resolver exceptions preserve their original language type; missing required
symbols fail checked. A partially initialized session still owes its native
graphics/platform/context cleanup, including truthful release consumption.
The public gate verifies both failure paths against real OS contexts.

| Language entry | Native entry / meaning |
| --- | --- |
| `info()` | `xgeHostSessionInfoGet`, sized native POD snapshot |
| `isCurrent()`, `makeCurrent()` | Corresponding native context-hook operations |
| `resize(...)` | Queue CPU dimensions for the next begin |
| `poll()` | `xgeHostSessionPoll`, nonnegative platform result |
| `beginFrame()` | `xgeHostSessionBegin`, begin one frame |
| `endFrame()` | `xgeHostSessionEnd`, consume the open frame |
| `present()`, `discardFrame()` | Consume a completed frame with/without presentation |
| `share()` | Acquire another native owner |
| `close()` | Common checked owner control and typed native cleanup policy |

`createWindowTarget()` can be used inside a nested offscreen pass. It
captures the host framebuffer's dimensions and follows the framebuffer
published by the latest successful `beginFrame()`. Passes and readback
through that target use the host's framebuffer even when its ID is nonzero.
After a resize, call `windowTarget.resize(width, height)` to update the
size of the view. A view larger than the current host framebuffer is
rejected before rendering or reading pixels.

An open frame, pass or active callback makes explicit close fail before logical
close, keeping `endFrame()` usable. GPU children can delay physical cleanup;
release them before expecting complete retirement. Destruction never
synthesizes end/present calls.

## Errors and consumption

Callback exceptions cross the checked boundary with their original language
error type. End/present operations already consumed by the native state machine
are not repeated because a callback failed. Each unit callback has one cleanup
obligation. `HostReleaseResult` carries `c_int status` and `bool consumed`
independently: false retains the external-context release duty for retry;
true consumes it even if status reports an error. Common owner cleanup can
retry at checked boundaries.

A throwing release callback yields the compiler's zero POD result, with
`consumed=false`, while retaining the language error separately. Do not destroy
an external context and then throw before returning the consumed result.
After consuming an OS resource, return its truthful status/consumption record,
so a later retry cannot destroy it twice.

## Costs and acceptance

Descriptor packs are stack records; methods call their named native entry once.
Creation still allocates native tables/names, generic callback-group environments
and closure/code owners. Shared references, checked view admission, ownership
bookkeeping and callback entry/exit pins are real costs. This is not zero total
overhead. Custom symbol lookup adds one language callback per native loader
request, callback-view lifetime bookkeeping, and any explicit `strlen`/`strcmp`
scans; lookups occur during session creation, not each frame. The binding adds
no per-method C forwarding function or symbol-name string copy. A previous
Windows diagnostic prototype observed no XRT alloc/free/
realloc over 1,000 warm language frames; it was not a timing comparison or a
proof about every allocator.

`tools/check_xge3_host_session.py` uses public `import xge`. Its OS-only WGL/EGL
fixture links neither XRT nor a compiler. Windows GL and Linux GL/GLES cover
JIT, dynamic/static AOT, pixels, independent shares, Busy-close recovery,
deferred release and seven callback-error/consumption cases. Dynamic/static
shared products run twice, clean up, physically unload and let the host exit
normally. Windows also calls the existing terminal
`xl_script_runtime_retire_checked()` before unloading embedded XRT code:
reusable module cleanup alone does not retire OS FLS destructors. Static
product import tables exclude XGE and compiler libraries.

The custom-resolver follow-up repeats system/custom GL and deferred release
variants, including Linux GLES. It also verifies that a saved symbol-name view
expires, and that throwing or empty-address resolvers release each partially
created OS context exactly once. System/custom dynamic/static shared products
both execute twice and physically unload. The matched latest-XRT full compiler
and SDK candidate hashes are recorded in
`dev/release-materials/provenance/xge3-host-resolver-20261117.json`.

`tools/check_xge3_host_nested.py` separately checks the embedded compiler path.
The outer program imports `xlang`; its inner source imports the XGE source
package and an OS-only fixture package. Each run executes a successful real
custom-resolver render, a deliberately invalid compilation, then another
successful render in the same compiler Context. Windows GL and Linux GL/GLES
pass through both actual CLI JIT and a standalone outer AOT executable. The
standalone process receives no added compiler-directory library search path;
its import table contains no compiler/runtime/engine image. Its work directory
contains only the explicitly supplied fixture header, with source extension
packages in a separate directory. This verifies embedded compilation and
recovery; it does not claim nested native/language cycle collection, physical
engine unload after each inner run, or a filesystem-denial sandbox.
Receipt: `dev/release-materials/provenance/xge3-host-nested-20261117.json`.

`tools/check_xge3_host_oom.py` covers the same real OS backends through JIT,
dynamic and static AOT. It arms the program's actual XRT allocation injector
before the owning factory call and checks every failure prefix up to the first
successful sentinel. After each attempt, all OS context duties are consumed,
unit counters match initialization counters, and live XRT count/bytes equal
the warmed baseline. The observed failing allocations precede platform init;
this does not simulate an OS/driver allocator failure after context creation.
The resolver-exception/missing-symbol tests above cover partial OS setup.

A separate test-only native stack snapshot measures from the return of native
graphics initialization to the owning factory's successful language return.
The full GL lookup sequence, generated callback entries and scalar name queries
perform zero XRT allocations/frees/reallocations in that interval. It excludes
the earlier factory/group/closure allocations and does not observe OS/driver
allocators or measure CPU time. Initial instrumentation wrote three managed
class fields inside the interval, adding three allocations/frees; the final
instrument uses a private native snapshot so those measurement writes are not
attributed to the binding. No public forwarding layer was added.
Receipt: `dev/release-materials/provenance/xge3-host-oom-20261117.json`.

Receipt: `dev/release-materials/provenance/xge3-host-session-20261117.json`.
Claimed native/language callback cycles remain unqualified: finalization can
need a callback whose group is already claimed by the ownership transaction.
Its proposed restricted entry rule needs a separate runtime contract and tests.
Do not rely on cycles for automatic HostSession cleanup yet. Borrowed backend
table APIs, complete callback admission under cold
OS TLS failure, nested compilation and latest-XRT/full SDK/IDE delivery remain
separate work. Primitive OS-key rollback is qualified separately in
[execution-context failure probes](foreign-context-os-failure.md).
No network tests run in this batch.
