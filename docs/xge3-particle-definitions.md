# Immutable particle definitions

`xge.ParticleDefinition` exposes the CPU definition family without an engine,
window, GPU, language wrapper class or C forwarding function. It requires a
matched particles-enabled native SDK and full compiler. The native definition
contains immutable emitter records and one real reference count; no language
callback, raw user pointer or graphics resource is hidden inside this root.

| Language API | Native API and result |
| --- | --- |
| `createParticleDefinition(emitters,size=-1)` | `xgeParticleDefinitionCreate`; checked owned root |
| `definition.count()` | `xgeParticleDefinitionCount`; `c_int` |
| `definition.get(index)` | `xgeParticleDefinitionGet`; checked independent `ParticleEmitter` value |
| `definition.stringify(xlon=0)` | `xgeParticleDefinitionStringify`; owned UTF-8 `string` |
| `definition.stringifyBytes(xlon=0)` | Same producer; owned `bytes` |
| `parseParticleDefinition(text,xlon=0,error=null,size=-1)` | `xgeParticleDefinitionParse`; `string`, `bytes` and `Buffer` overloads |
| `loadParticleDefinition(uri,xlon=0,error=null)` | `xgeParticleDefinitionLoad`; native resource registry, checked owned root |
| `definition.close()` | Generic checked foreign-root close, one native `Free` |

Use `particleEmitterInit()` for valid native defaults before changing selected
fields. A zero `ParticleEmitter()` does not describe a valid emitter. Native
validation retains its limits: 1–16 emitters, finite/ranged values, terminated
fixed strings, bounded bursts/keys and acyclic subemitter links. JSON and XLON
use schema version 1. The native parameter is historically named `bXson`, but
its parser/stringifier are the XRT XLON implementation. The language does not
substitute another parser, schema, compression or serialization algorithm.

The optional size is a byte count for parse and an element count for create.
`-1` uses the complete input; larger counts clamp to its actual length; other
negative counts fail before entering native code. A zero prefix reaches native
validation and is rejected. Counted parse input preserves interior NUL, without
a C-string scan. URI input is a C string and rejects interior NUL. Parser grammar
and accepted trailing input remain the native XRT parser's rules.

An optional diagnostic Buffer must already have the desired **logical length**,
for example `Buffer("................................")`. Native code may change
its bounded prefix even when parsing/loading fails; it does not change that
length. NULL and a present empty Buffer remain distinct protected-span inputs.
Checked status is still authoritative when no diagnostic text is requested.
Do not decode the entire diagnostic Buffer as if its unused suffix were absent.

## Ownership and threading

Creation copies the emitter values into native immutable storage. Later changes
to the language array or a returned emitter do not change a definition. The
language root owns exactly one native reference; assignment/method values retain
the language view, rather than inventing another native emitter allocation.
Independent native users retain their own real reference through AddRef/Free.
Closing the language owner releases its reference while those users remain valid.

The ordinary creator-thread checks and deferred active-call destruction apply.
Calling from another language thread or through a saved method after close raises
`StateError`. Method values retain their receiver/code lifetime and add closure
creation and indirect-call costs. No public raw emitter pointer or manual native
reference-count field is exported. These local-root checks do not resolve the
separately pending module-global/CodeLease ownership cycle.

Loading uses the existing owned resource-provider contract. Provider exceptions
cross the checked load boundary, provider registrations still need explicit
cleanup, and neither a successful resource read nor a partial native allocation
is treated as a successful definition. Failed factory admission releases the
unpublished native reference.

## Measured costs and acceptance

Windows and Linux use the unchanged, fully rebuilt compiler cohorts and the
matched O2 `code-editing-sdk-*-r1` native images/archives. This batch changes
declarations and one cold lifetime policy; it does not rebuild or patch XGE.
The native product source files match the committed upstream revision. Existing
unrelated XGE/XUI workspace edits are excluded from these frozen SDKs.

`tools/check_xge3_particle_definition.py` runs actual JIT, dynamic AOT and static
AOT programs, a small console lesson, C11/C++11/C++17 public headers, and four
independent GCC O0/O2 hosted/static clients per platform. The native checks cover
all allocation prefixes through successful JSON/XLON parse and stringify, nested
keys/bursts/subemitters, invalid DAGs and cleanup without live-allocation growth.
Language checks add array/value isolation, size/index failures, bounded diagnostics,
owned resource-provider success/exception, constructor/output OOM including output
admission tails, method values, wrong-thread use and expired access. A test-only
read of the exact pinned private layout verifies native references 1 → 2 → 1;
the binding does not depend on that private layout.

The paired workloads report heap operations, not CPU throughput equivalence:

| Prepared workload | Language | Handwritten C |
| --- | --- | --- |
| 1000 `count` + `get` pairs | 0 alloc/free/realloc | 0 alloc/free/realloc |
| Create and release two emitters, language array already prepared | 5 allocations + 5 frees | 2 allocations + 2 frees |
| Serialize the same definition to JSON and release text | 464 allocations + 464 frees + 16 reallocations | Same counts |

Creation's three additional allocations are the temporary typed-record array,
managed view and CodeLease. The projection validates/pins the source, checks each
boxed record and copies `n * sizeof(xge_particle_emitter_t)` bytes; native Create
then performs its normal validation and immutable copy. Preparing the language
array itself also boxes/copies records and is outside that interval. Get returns
a whole record by value, including fixed arrays, rather than a borrowed pointer.

The owned text/bytes contracts take over the resident-XRT allocation without
copying its serialized payload; string output additionally validates UTF-8 and
the terminator. Native serialization builds a temporary XRT value graph, accounting
for its many allocations. The text interval checks length, avoiding the separate
two string copies generated by a language string-equality assertion in the first
exploratory measurement. Error scopes, view/code retention, thread checks and slot
cleanup still execute, and are not certified CPU-free by zero heap counts.

The same static archive/full compiler and an unused ShapeMatrix lesson have
file/text size delta 0 on both platforms when only the manifest's new declaration
entry is disabled. This is a selected build/import comparison, not a universal
size claim. Dynamic binaries import the actual host image; static binaries import
neither that image nor compiler/TCC DLLs/DSOs.

Run `examples/particle_definition.xl` for a short, linear create/read/JSON-roundtrip
lesson with explicit `finally` cleanup. The receipt is
`dev/release-materials/provenance/xge-particle-definitions-20261114.json`.
The later [ParticleWorld batch](xge3-particle-world.md) qualifies CPU simulation,
owned collision callbacks and scoped snapshots. Conditional events, renderer/GPU
ownership and final matched release installation remain unfinished. Definition
tests do not certify simulation, rendering, networking or complete coverage.
