# Particle value declarations

`xge.ParticleCurve`, `ParticleGradient`, `ParticleEmitter` and `ParticleEvent`
map the complete pointer-free records in the matched XGE header. Inline arrays
keep their declared bounds and ordinary C value-copy semantics. Existing key,
range, burst, subemitter and play records remain the constituent types.

`particleCurveEval`, `particleGradientEval` and `particleEmitterInit` are direct
native declarations, with no particle-specific C forwarding functions. The
first two take addresses of exact trivial records; the last writes native
defaults into a complete output record. `ParticleEmitter()` alone is zero
initialization, not the native defaults. Eval keeps the native fallback for
empty/invalid curves and nonfinite sampling time. Names and paths in fixed
`c_char` arrays remain NUL-terminated C UTF-8 byte arrays. Numeric effect IDs in
events belong to the originating world; an event does not own a live effect.

The readable `lib/bindings/xge/examples/particle_curves.xl` needs no window or
graphics context. With a matched SDK in `XLANG_PACKAGE_PATH`, run it using
`xl run particle_curves.xl` from its examples directory. It samples a two-key
curve and initializes an emitter, asserting native defaults.

## Verification and costs

`tools/check_xge3_particle_values.py` uses complete Windows r27/Linux r19 O2
compiler cohorts and unchanged native image-r3 SDK payloads. Each platform
passes JIT, dynamic AOT and static AOT for both the fixture and example, plus
C11/C++11/C++17 public-header checks. A separately compiled C++17 object/archive
passes and returns all four records by value, including tail arrays and exact
64-bit event identifiers above 2^53. Language assignments preserve independent
inline-array storage. Generated calls use the native function names directly.

After warmup, equivalent C and language loops (1000 emitter initializations,
curve and gradient samples) each perform zero XRT allocation, reallocation and
free operations and produce the same checksum. This is an allocation result,
not a CPU throughput measurement: ordinary by-value record/array copies,
language checks, scalar conversions and stack storage still have costs.
Using the same compiler, equal-length diagnostic paths and unchanged native
payload, an unused-particle matrix program has exactly zero file and code size
delta on each platform. The evidence and package hashes are indexed by
`dev/release-materials/provenance/xge-particle-values-20261112.json`.

Worlds, definitions, simulation, collision callbacks, renderers, owned GPU
resources and final matched SDK/IDE delivery remain separate unfinished work.
This batch neither introduces a raw-handle workaround nor certifies those APIs.
