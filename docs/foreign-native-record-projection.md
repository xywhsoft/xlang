# Read-only native record projection

An immutable native sequence may contain C records with private pointer fields.
Use an explicit projection to expose their declared scalar fields without
pretending that the language value has the native record's layout:

```text
extern "NativeGlyph" struct Glyph @header("glyphs.h") @c_projection()
 c_uint codepoint
 c_float advance
end
```

The header might define `NativeGlyph` with a private Font pointer and additional
members. `NativeSequence<Glyph>` retains its independent native owner and reads
that original array directly. Its count, alignment and stride describe
`NativeGlyph`. Indexing and iteration materialize a separate language `Glyph`
value by loading only `codepoint` and `advance`; they do not copy private fields,
the whole native record or the backing array. The ordinary sequence lifetime,
creator-thread and no-suspension contracts continue to apply.

Declared fields can be exact C numeric/boolean scalars or recursively declared
scalar-only C records. `@c_name` can select the corresponding native member.
Pointers, callbacks, owning references, nullable fields and mutable native
storage are excluded. The C header must prove the exact native type of every
declared field. Nested projections keep their own native backing identity;
packed native records use their original alignment and stride.

Without `@c_projection()`, an extern record still requires exact native size,
alignment, offsets and field types. It cannot embed a projected language value.
A projected record can be copied, modified locally, returned by a language
function and boxed as its exact nominal type. Those operations affect its
independent logical value. A native extern function cannot take or return it
as a by-value native C record: that requires a separate explicit conversion
contract and is rejected here. Sequence storage remains read-only.

XLIR carries `foreignRecordAbi` (none, exact, projected), `foreignRecordSource`
(native backing type) and `cName` (logical value type). The verifier rejects
missing/contradictory identities, including nested records which are not roots
of a sequence producer. Cgen uses these sealed facts and never infers projection
from a type name or recovers it from the AST. Public headers describe the
logical value and include native field proofs.

Projection itself allocates no heap memory and adds no native function call per
field. It does materialize declared scalar fields and zero the logical value
before loading them, including its padding. TCC may emit a `memset` call for
that initialization; GCC may optimize it. No whole-array zero-cost or throughput
claim follows from a zero-allocation loop. Opening/slicing a sequence and its
owning view keep the existing lifetime and checked-admission costs.

The dedicated fixtures cover nested/private fields, packed storage, language
copies/boxing, owner retention, strict C/C++ headers and rejected source/IR
contracts. Full compiler/SDK acceptance is recorded separately; diagnostic
compiler overlays are explicitly not release qualification.

Windows complete O2 compiler r16 and Linux complete O2 compiler r8 pass 82
build/run/header/source-rejection checks. Both include TinyCC, XRT and the
rebuilt embedded compiler; nested JIT/static programs exercise the projection
and their imports exclude xlang/TCC runtime libraries. A separate O0 IR damage
gate matches the nine exact compiler source overrides and rejects 24 damaged
contracts before either C or a public header is emitted. Its diagnostic build
is identified separately from the complete compilers.

Both platforms' 24 allocation budgets produce 16 checked refusals and 8
successes, always releasing native owners back to baseline. Evidence and exact
source/product hashes are in
`dev/release-materials/provenance/native-record-projection-20261111.json`.
These isolated qualifications do not install or publish a release/IDE bundle.
