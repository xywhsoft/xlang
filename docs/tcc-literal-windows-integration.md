# Windows literal collection: complete build and remaining blocker

The complete O2 r25 compiler contains the code/data-owner literal implementation.
Its frozen inputs and CLI/runtime/compiler product hashes match its build report.
This is a partial integration result: a deeper source-free language-compiler
embedding case still fails. No SDK, release binary, or IDE was installed.

The immutable small evidence index is
`dev/release-materials/provenance/tcc-literal-windows-20261112.json`.
Historical failed candidates and their raw reports remain unchanged.

## Verified execution

- Matrix: 19 command checks, including real JIT, dynamic/static executable runs,
  import inspection, and C11/C++11/C++17 headers.
- Image: 27 command checks, including real JIT, dynamic/static execution, NUL and
  explicit-prefix inputs, alpha rules, failure rollback, and OOM paths.
- Literal delivery: 11 command checks across CLI JIT, an independent executable,
  and an actual shared library, each using its own embedded TCC. All six C children
  execute their live NUL/interior-pointer checks. With GC, unused string/float
  payloads disappear; without GC, they remain. The GC child changes from 308224
  to 5120 bytes in each host compared with the complete old compiler controls.

These 57 checks supplement, rather than replace, the focused 1358 C checks and
82 actual-object budget checks. They do not prove full graphics/backend coverage
or all compiler embedding modes.

## Measured sizes

The same matrix-only source and current image SDK produce:

| Complete compiler | Static executable bytes | Machine-code bytes |
| --- | ---: | ---: |
| Before literal collection, r21 | 3691008 | 1931142 |
| Code/data-owner literals, r25 | 3637248 | 1931142 |

The file shrinks by 53760 bytes (52.5 KiB). `.rdata` shrinks by 53808 bytes;
`.reloc` grows by 12 bytes, with the remaining difference due to file layout.

A separate comparison uses the **same r25 compiler** and source against the
pre-PNG and current native SDKs. Executable and code size deltas are both zero.
This qualifies unused-PNG trimming for this sample; it is not a whole-engine
zero-overhead claim.

There is also a real producer/delivery cost. Compared with the complete r21
build, `xl.exe` stays 72704 bytes; `xl.dll` grows from 15217152 to 15271424 bytes
(+54272), and `xlc.dll` from 21340160 to 21480448 (+140288). These increases must
not be hidden by reporting only consumer-executable savings. No complete
section-by-section attribution or CPU/peak-memory improvement is claimed here.

## Remaining full embedding failure

The source-free delivery gate passes native EXE/shared compilation and execution,
then fails building a parent that imports `xlang.compiler` and `tcc`. It must be
able to emit and run an independent child; this requirement is retained.

The actual r25 product trace has 42198 sections before loading the complete XRT
object. Its input budget is 24140, so the combined 66338 exceeds the existing
65280 index boundary even before reserved headroom. The linker refuses rather
than truncating an index. Successful embedded-TCC children above do not cover
this deeper full-language-compiler case.

An object-header audit of the selected static compiler/XRT manifest population
finds 30456 nonallocated relocation headers without any defining symbol. They
are link metadata, distinct from addressable code/data. Their current placement
in the numbered directory contributes to the limit. A definition or an allocated
runtime relocation table must retain its normal indexed representation.

The next correction is described in
[relocation metadata design](tcc-relocation-metadata.md). It is not implemented
by this report. Windows full embedding, Linux integration, and final matched
delivery remain unfinished.
