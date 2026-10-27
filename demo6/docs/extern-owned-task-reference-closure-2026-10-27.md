# Owned string task composition: reference/default closure

This batch closes the five reproducible compiler gaps from the source audit:

1. `future<func(...)?>` keeps the underlying callable signature and nullable
   presence through literal/stored `task.all` and `task.race`.
2. `task.spawn` accepts a derived class argument for a nullable base parameter,
   using the same conversion as an ordinary call.
3. Direct fixed calls bind default expressions to the already evaluated
   argument prefix. Generic, named, static, constructor and protocol paths
   share this lowering rule. Defaults run once in declaration order, and
   failure skips the target body.
4. Explicit closed generic function references can bind a shorter concrete
   function signature when the omitted parameters have defaults.
5. Error Arrays use the existing resident Error Value handle with its exact
   logical element identity. Native admission verifies private handle/trace
   provenance before retaining an element; a matching TypeId alone is
   insufficient. Non-null/nullable reads and iteration acquire error refs.

Escaping anonymous functions inside defaults also use the evaluated prefix.
Read-only captures use the existing closure copy/retain policy. Mutable
captures receive a private cell for that default activation, consistently
across direct calls and narrowed adapters. A default that does not capture a
mutable prefix emits no such cell. These bindings and decisions are compiler
metadata, not a new language value or an ordinary-string runtime tag.

The Error Array factory is a separately selected artifact component. It does
not alter `string`, `cstring`, the allocator or the existing C return ABI.
Error container slots retain boxed handles; this is not a claim that boxed
container operations have no allocation cost.

## Durable regression entry points

- `tools/check_extern_owned_string.py`: public compiler JIT/AOT/emitted C,
  including nullable callbacks, dependent defaults, Error Arrays and 96
  synchronous allocation prefixes for escaping default captures.
- `tools/check_extern_owned_string_public.py`: separately compiled C consumer
  at O0/O2, 120 allocation prefixes per consumer, native/IR contract tests.
- `test/cases/xlir_task_reference_contract.c`: eight corrupted nullable
  payload/sequence contracts rejected before any C or header bytes.
- `test/cases/xlir_error_array_admission.c`: fake handles/wrong identities,
  nullability/default rules, retained identity, and 64 allocation prefixes.
- `test/cases/extern_owned_task_error_arrays.xl`: non-null/nullable indexing,
  resize, iteration, both task aggregate forms and 48 allocation prefixes.

Build and result receipts are kept in the ignored `build/owned-string/`
evidence directory. The source-built candidates are kept separate from the
main checkout's release binaries, which concurrent work may still use.

## Verification completed

The final source candidates `driver-60` (Windows) and `linux-60` (Linux)
passed, independently on each platform:

- 121 public compiler checks, including JIT, AOT build/run, emitted C and
  expected rejection diagnostics;
- 27 independent C/header/IR checks, with the O0/O2 consumer, runtime OOM,
  compiler metadata OOM and 39 rejected IR mutations;
- 18 existing task/closure regression stages.

Receipts: `acceptance-windows-60/result.json`,
`acceptance-linux-60/result.json`, their `independent-C/result.json` files,
and `task-regression-{windows,linux}-60-focused/result.json`, all under
`build/owned-string/`. Both compiler build receipts seal the source/object
cohort and include the new Error Array component and private SDK.

The direct, imported-package, export and forwarding ordinary `@owned string`
fixtures produced byte-for-byte identical C to the audited `driver-51`
baseline. `generated-c-60-vs51.json` records the four hashes. This verifies
that these ordinary return paths gained no generated copy/allocation/call;
it does not claim that all task/container operations are free of costs.
