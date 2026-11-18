# ParticleWorld simulation and callbacks

`xge.ParticleWorld` owns the actual native CPU world. It needs the matched
particles-enabled SDK and a complete compiler, but no window, engine Init,
graphics context or renderer. No language wrapper class, operation dispatcher
or second simulation implementation is involved.

| Language API | Native operation |
| --- | --- |
| `createParticleWorld(maxEffects,maxParticles,maxEvents,maxSpawnPerStep,maxSubsteps,fixedStep)` | Stack `xge_particle_world_desc_t`, cold CodeLease admission, native `CreateOwnedWithCode` |
| `share()` / `close()` | Real native shared reference / checked language owner release |
| `reserve(definition,count)` | `xgeParticleWorldReserve` |
| `play(definition)` / `play(definition,options)` | `xgeParticlePlay`; default NULL or stack const-reference options; effect ID output |
| `release`, `restart`, `pause`, `stop` | Same native effect mutation functions |
| `setTransform`, `setParameters`, `setVisible` | Same native functions; POD values projected by const reference |
| `state(effect)` | `xgeParticleState` returns a nonnegative effect state or a checked error |
| `bounds(effect)` / `emit(effect,emitter,count)` | Checked native output: independent Rect / emitted count |
| `update(seconds)` / `prewarm(effect,seconds)` | Checked native simulation status |
| `stats()` | Native `ParticleStats` value; native error checked before publication |
| `pollEvent()` | `xgeParticleWorldPollEvent` removes at most one event and returns `(available, event)` |
| `visit(visitor)` | Synchronous native visitor; 0 stops, nonzero continues |
| `onCollision(callback)` / `onCollision()` | Direct owned native registration / clear |

Factory defaults are the native defaults: 128 effects, 32768 particles, 4096
events, 4096 spawns per step, 64 substeps and a 1/120-second step. Time is in
seconds, not the platform deadline ABI's milliseconds. Emitter indices are
zero-based. Use `particleEmitterInit()` and `particlePlayInit()` for native
defaults; zero POD constructors do not supply those defaults.

`pollEvent()` reads the world queue in FIFO order. When `available` is false,
ignore the zero-initialized event. Copied events remain readable after the
world closes. `state(effect)` returns `XGE_PARTICLE_PLAYING`, `PAUSED`,
`DRAINING` or `FINISHED`. A missing or released effect raises a checked error.

Use `particleSweepPlane()` or `particleSweepRect()` to find the first contact
along a segment. Both return `(hit, contact)`. Read `contact` when `hit` is
true. The record gives the particle center, outward normal and fraction of
the segment. Invalid arguments raise a checked error. The queries use the same
geometry as the existing C collision functions. The [particle world example](../lib/bindings/xge/examples/particle_world.xl)
uses the plane sweep inside a collision callback and drains the event queue
after updating.

## Rendering

Create a `ParticleRenderer` with `xge.createParticleRenderer(capacity,
batchCapacity)`. `capacity` limits the visible particles collected in one
draw. `batchCapacity` limits the particles in each native sprite batch.
`render(world)` draws without a culling rectangle. `render(world, viewport)`
culls by the supplied rectangle in particle coordinates. It does not change
simulation visibility or advance the world. Capacity exhaustion is reported
before drawing particles from that call.

`bind(definition, emitter, texture, material)` installs a drawing binding for
one emitter. The renderer holds the definition and texture, and copies the
material's color and pipeline settings while retaining its shader and textures.
The caller can then close those views. Use `unbind(definition)` to clear the
bindings for an entire definition. An unbound emitter uses the native white
texture tinted by the particle color.

The renderer belongs to its creating thread and keeps one graphics lifetime
reservation from creation to `close()`. The white texture and sprite storage
are prepared at the first draw. Close the renderer before shutting down the
engine. With a live renderer the checked shutdown reports Busy, so it can be
retried after closing. The current native renderer requires the graphics
thread and reports Unsupported in render-thread mode.

[Particle burst example](../lib/bindings/xge/examples/particle_renderer.xl)

## Lifetimes and failure

World methods require the creating thread. `share()` does not copy particle
state: it obtains an independently closable language owner of the same native
world. Closing one view invalidates its bound methods, while another native
owner can still operate. The real world traces both the code owner and the
collision closure; active native calls protect the entire callback and return
tail. Checked destruction observes the native consumed flag, including failure.

`onCollision` retains its typed closure until replaced, cleared or finalized.
It receives start/finish Vec2 values, radius and a writable `ParticleHitOutput`.
Return 0 for no collision or 1 with finite position/normal, a nonzero normal and
fraction in [0,1]. Negative results, other positive values and malformed hits
are checked failures. Language exceptions are retained by the generic callback
bridge, stop the native update, and reappear at the checked calling boundary.
Reentry into mutation is rejected while the world is busy.

Update and Prewarm can have partially changed particles when a callback fails.
They restore busy/event-suppression state, retain the earliest failure, discard
remaining whole steps rather than replaying them, and count dropped time.
Clearing the callback permits subsequent operations. This is not transactional
rollback. A callback may close its own World view; the active operation still
finishes before physical cleanup. No separate language-level event pump or
background thread is introduced.

`ParticleHitOutput`, readonly `ParticleSnapshot`, and the snapshot's readonly
`CallbackParticleDefinition` exist only during their callback. The nested
definition uses the already implemented generic `scope_view` field result,
deriving the parent's exact scope. Assigning a view to another variable does
not extend that scope; all later access fails with a checked StateError. Copy
numeric/POD fields, `definition.get(index)` or owned serialized text for longer
storage. The nested view has no independent close/share capability.

Snapshot fields cover effect/particle/emitter identities, definition, position,
velocity, size, age, life, rotation, frame and color. Hit position/normal/fraction
are direct typed native fields. No raw native pointer or callback userdata is
publicly projected. This does not resolve the separately pending rule for
long-lived aliases of existing owners.

## Measured costs and qualification

The native raw world stays 496 bytes. The managed world is 600 bytes, including
the new 32-byte callback-owner descriptor; the same small configuration uses
five allocations and 1336 requested live bytes (raw: five and 1232). No new
per-particle metadata is added. The cold factory additionally acquires/releases
a CodeLease and creates its managed view; sharing creates another language
view, while native sharing only increments the actual reference count.
Closure construction/registration can allocate their language records.

Each invocation takes/drops an independent native callback-owner reference.
The language path also checks its callback record, performs the typed closure
call, establishes/expires scope views, copies POD arguments/fields, and runs
checked error/receiver/cleanup boundaries. Reading a nested definition derives
a checked scope view; get copies a complete emitter record. Stringify uses the
existing native serializer and owned text adoption, not a borrowed string.

Across JIT, dynamic AOT and static AOT on both platforms, 1000 prepared
Update(0)/Stats pairs and 1000 actual collision calls had zero allocation,
free and reallocation counts, in both the language and equivalent C fixture.
This verifies heap behavior for those prepared workloads, not CPU equivalence
or a zero-cost whole binding. With only the World manifest entry disabled, the
same unused ShapeMatrix static program has text/BSS delta zero on both targets.
File delta is zero on Windows and +1 byte on Linux: the compiler's declaration
IDs change section names, adding one byte to ELF `.shstrtab`. Other section
sizes, flags, addresses and offsets match. The first Linux size gate rejected
that metadata-only delta; its report is preserved and the later audit records
the actual section-table cause instead of claiming zero file growth.

`tools/check_xge3_particle_world.py` requires matched O2 SDKs, one complete
compiler XRT and the independent native collision gate. Windows/Linux each
run JIT/dynamic/static tests plus the short lesson, C11/C++11/C++17 public
headers and C-fixture O0/O2 syntax. Tests cover all snapshot fields, owned
sharing, callback/child expiry, method values, wrong-thread rejection,
callback exceptions, self-close and 96 allocation prefixes per mode with
unchanged live storage and actual native reference counts. Import tables
verify independent AOT; static programs do not import the XGE host image.
See `dev/release-materials/provenance/xge-particle-world-20261114.json`.

`examples/particle_world.xl` is a short lexical-main lesson: configure one
emitter, create a definition/world, register a collision, emit and update,
assert removal, then close both owners in `finally`. Expect
`Collision removed the particle`. No external file or service is needed.

This batch excludes renderer/GPU ownership, conditional EventPoll/collision
helper outputs, the mixed value/error State result and independent definition
retention from a callback. Those need their general contracts. Global/module
CodeLease cycles and final matched runtime/IDE installation also remain open.
The XGE/XUI/Document goal is still active; CPU tests do not certify graphics.
