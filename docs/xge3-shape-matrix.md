# Direct affine-matrix declarations

All eleven native `xgeShapeExMatrix*` functions are available as
`xge.shapeExMatrix*`. Identity has moved from `functions.xl` to the dedicated
`shape_matrix.xl`; its public name is unchanged. The other ten declarations
are new. No per-method C wrapper, dispatch table or runtime adapter was added.

| Operations | Language result and semantics |
| --- | --- |
| Identity, Translate, Scale, Rotate, Skew | A complete new `ShapeExMatrix` value |
| Multiply(parent, local) | `parent * local`; the local transform acts first |
| Invert(matrix) | An inverse value; native singular/near-singular rejection becomes a checked error |
| Point(matrix, point) | A `Vec2`, including translation |
| Vector(matrix, vector) | A `Vec2`, without translation |
| RectBounds(matrix, rectangle) | The enclosing axis-aligned `Rect` |
| StrokeScale(matrix) | The native geometric-mean scale, with its native degenerate fallback |

Angles are radians. Matrix elements retain the six-float native layout and are
value fields. Assigning/calling does not create a shared mutable matrix object.
The native Translate/Scale/Rotate/Skew implementations overwrite their output;
some older native header comments describe them as left multiplication. The
language follows the actual implementation and uses explicit Multiply for
composition. This batch does not modify the matched native header bytes.

`@c_ref` projects const input records and `@c_output(0,0)` maps the native output
and status to one logical result. Generated C contains one native call with
stack record addresses, followed by common checked status/error handling.
The existing `@no_raise` declaration does not suppress that checked status.
Failure does not expose a partially admitted output.

## Qualification

The small evidence index is
`dev/release-materials/provenance/xge-shape-matrix-20261112.json`; external raw
reports and logs remain under `F:/xlang-validation/xge-full-20261110/`.
Matched native revision `2574c579` and complete Windows r21/Linux r13 compilers
are reused with exact product hashes. They were not rebuilt for declarations.

Each platform passes 19 command checks (38 total): generated C, real JIT,
dynamically linked executables, statically linked executables, import inspection,
and C11/C++11/C++17 public-header compilation. Both the behavioral fixture and
the readable `examples/shape_matrix.xl` run in all three execution modes. Tests
cover all eleven calls, composition order, const input/value-copy isolation,
nonzero skew, rotated bounds, typed first-class invocation, singular inverse and
the four factories' existing nonfinite-input rejection. Header checks include
an actual exported native-record signature and all generated field/type proofs.

1000 warmed multiply/first-class point iterations have no XRT allocation,
free or realloc operations. This is a heap-counter assertion, not a CPU timing
or all-allocator claim. Compared with handwritten raw C, language calls still
have value moves and common foreign error scopes/status checks; first-class
calls also use the ordinary callable invocation machinery. Static output
records and by-value arguments keep their native ABI costs. No per-call
resource control, native retain/release, array boxing or matrix heap owner is
introduced. Error construction and package bootstrap remain cold-path costs.

The static executable import tables have no XGE host image or xl/xlc runtime
DLL/SO dependencies; dynamic executables explicitly retain their XGE host
image. This does not qualify xge import inside a shared library: the existing
package top-level initializer is still awaiting its general declaration
contract. No window/GPU work, full-engine performance claim, SDK installation,
IDE replacement or final full-integration completion is implied.

Run the focused gate with a complete compiler report and matching staged SDKs:

```powershell
python -X utf8 -B tools/check_xge3_shape_matrix.py --root D:/GIT/x-lang --compiler <xl.exe> --compiler-report <build.json> --dynamic-packages <host-packages> --static-packages <static-packages> --out <new-evidence-directory>
```

On Linux use `python3 -B` and the matched Linux CLI/report/SDKs. The gate writes
only its new external evidence directory. Native public-surface inventory also
finds all eleven declarations with zero parse errors; it remains an inventory,
not a substitute for the execution evidence above.
