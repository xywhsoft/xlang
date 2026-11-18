# Bounded 3D CPU particles

`createParticles(settings)` accepts the independent numeric
`Xge3dParticleDesc` returned by `threeDParticlesDefault()`. The native factory
copies it, including both color arrays, and preallocates bounded simulation
storage. Changing the input afterward does not change the emitter. Native
seed zero normalizes to one; identical seeds and update steps are deterministic.
Settings, capacity and finite/range validation retain their native semantics.

`Particles` exposes direct `setPosition`, `emit`, `update` and `info` methods.
Manual emission counts particles that fit as emitted and excess requests as
dropped; capacity exhaustion is successful bounded behavior. `update(dt,emit)`
accepts dt in [0,1] and emit 0/1; zero is a no-op and automatic emission retains
its fractional remainder. Passing emit=0 drains existing particles without
replenishing them. Statistics are independent 24-byte values with exact uint64
counters. `@c_const` preserves the Info function's const receiver, and
`@c_output_size` initializes its mandatory size before the one native call.

The native model owns only numeric settings and its CPU array. The existing
independent owned-leaf protocol is sufficient: one real native free consumes
that owner, while generic root scopes defer it during active calls. Close is
idempotent, invalidates aliases and typed bound methods immediately, and is
subject to the existing creator-thread rules. No shared ownership, native
callback, extra XRT implementation or per-method C forwarding function is
introduced. Synchronous callback probes verify the native allocation remains
live during an active close, then disappears at the protecting call's end,
including when the callback throws.

The sample `lib/bindings/xge/three_d/examples/particles.xl` is a linear CPU-only
program: emit twelve into capacity eight, observe four dropped, then drain to
zero. It requires the matching 3D SDK with `XGE3D_ENABLE_PARTICLES=1`; it creates
no window, renderer, model or network connection.

The QA fixture compares the actual native positions, colors and UVs against
the same DLL/archive after the same seeded workload. It also checks input
copies, seed normalization, uint32 maximum emission and uint64 counters,
invalid settings/timesteps/positions, sized output, thread and expiry guards,
both native constructor allocation failures, managed publication OOM prefixes
and whole-heap balance. A raw C-only Billboard read is an oracle inside the
test fixture, not a public language alternative to the missing output API.

The descriptor occupies 128 bytes and Info 24. Warm language/native loops
perform the same four calls per iteration: setPosition, emit(0), update(0,0),
info. Zero heap operations are checked under allocation budget zero. Native
creation and managed controls, descriptor/output copies, native math/array
work, scope/thread/error checks and bound-call closure costs remain. The
machine-readable receipt records actual cold allocation/byte counts; no CPU
equivalence or universal zero-overhead claim is made.

## Billboard snapshots and rendering

`Particles.collectBillboards(blend)` builds an immutable CPU batch from the
current live particles. The original particle capacity, blend and UV rules
apply. `BillboardBatch.geometry()` reads position, size, rotation, color, UV,
blend and depth state directly from the native array. The sequence holds an
independent reference to that batch.

`Renderer.setBillboards(batch, texture)` replaces the render queue. It also
accepts an array of numeric `BillboardGeometry` records and an optional
texture. The native renderer retains the texture references for the submitted
rows only after each row has passed validation. Submission failure leaves the
previous queue intact. The renderer accepts up to 65,536 rows, and an empty
array clears the queue. Use the batch
overload to pass the original native snapshot without converting a language
array. The numeric array overload uses the standard checked C array
projection and copies the selected elements into temporary native storage.

`createTexture` copies counted straight-alpha RGBA8 data and accepts explicit
sRGB and sampler settings. The renderer and texture keep graphics leases
through their final native release. Explicit engine shutdown reports Busy
while a managed lease remains. If a window backend has already ended, the
driver has reclaimed that context's GPU state. Creator-thread retirement then
releases CPU state and the lease, and rendering rejects the expired context.
Language close during a callback can defer physical release to the callback's
safe point.

`Renderer.render(scene,camera,clearColor)` renders to the active viewport and
returns native statistics. Its native entry initializes a frame descriptor
on the stack and uses the existing renderer. The general 3D target, atmosphere
and multi-texture Billboard submission contracts remain part of the wider
renderer integration.

Collecting a snapshot traverses the live particles once and uses one native
allocation for the header and all rows. The sequence adds ownership
bookkeeping, but no second array allocation or copy. Native submission still
validates and writes the renderer's own rows and retains textures, matching
the queue's persistence requirements. Warm native C submission tests cover
both legacy and batch entry points with allocation failure enabled. In the
Windows and Linux JIT checks, 64 warm submissions of the same native batch
recorded zero XRT allocations, frees and reallocations. This excludes cold
setup and the numeric-array overload. CPU throughput was not measured.

Windows and Linux also passed native pixel comparisons at O0 and O2, strict
C11 and C++ header checks, JIT, hosted and static AOT, and shared module
retirement. The native fixture compares actual textured frames and checks
queue preservation after rejected geometry and allocation failure.
