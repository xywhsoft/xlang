# Typed NULL aliases for const descriptor inputs

Two declarations may refer to the same C function or callable field when one
omits an input const POD-record reference or a const packed descriptor and
supplies an exact typed NULL at that physical position. The declarations keep
separate logical signatures; this does not implicitly convert their argument
types or turn a resource into a raw pointer.

```xlang
extern "nativeSet" @no_raise @c_ref(0) @c_result("status",0)
    func configure(Config value)
extern "nativeSet" @no_raise @c_null(1,"const NativeConfig*") @c_result("status",0)
    func clearConfiguration()
```

The example assumes a nonstatic receiver at physical position zero and a
`Config` C-record declaration with the exact native `NativeConfig` type and
authoritative header. A const packed descriptor may replace the `c_ref` input
under the same rules. Both declarations require the same authoritative header,
native target, result, status predicate, effects and ownership contract.

Semantic analysis compares the physical parameter sequence. Every unchanged
input retains its exact logical type, passing mode, string/span/callback
contract and resource lifetime. Later inputs may have different logical
indices because an earlier descriptor was omitted; their contracts are matched
by physical position, including columns of unchanged packed descriptors.
The sealed IR independently repeats this comparison before C or header output.
Nested record witnesses are `RECORD` entries in the same pack-field directory,
so their full paths and exact C types participate in the comparison. Each
parent-directory identity is validated against its own declaration, pack and
unique immediate path before indices from separate declarations are normalized.

The substitution does not accept output positions, mutable references, resource
views, spans, callbacks, hidden module CodeOwner inputs, arbitrary C expressions
or unrelated pointee types. It does not weaken nullable-resource or callback
epoch checks. Existing fixed NULL inputs which are not substituted remain
strictly identical. Declaration order does not change acceptance.

Emission continues to use the ordinary direct C call or captured native
function pointer. A clear operation supplies `(const NativeConfig*)0`; the
configured operation uses the existing call-period record/pack preparation and
resource scopes. This compatibility check adds no forwarding C function,
runtime dispatch, copy, allocation or scan. The existing descriptor, string,
resource and checked-error costs still apply to each declaration.

The CPU fixture `test/cases/foreign_null_compat.xl` covers direct, bound,
protocol, global, owned-factory and parallel-descriptor calls in both
declaration orders. `tools/check_foreign_null_compat.py` checks C11/C++11/C++17
headers, O0/O2 execution, malformed declarations, sealed-IR mutations with zero
C/header output, and mismatched authoritative C signatures. It performs no
GUI, GPU, browser, network or clipboard operations.
