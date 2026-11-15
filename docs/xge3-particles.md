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

`ParticlesBillboards` still needs safe writable output arrays and owned texture
integration; submission/rendering and the other 3D scene/GPU families remain
in the full objective. This CPU simulation batch does not certify them or
install a final SDK/compiler/IDE release.
