# Generated-C declaration ABI gate

XLIR/Cgen must declare each native function's exact signature. A missing
declaration must fail compilation, never become an implicit `int` return that
can truncate a native pointer.

`xlTccCompileGeneratedModule` in `src/api/xl_api.c` is the shared boundary for
JIT/relocatable language modules and external EXE/DLL output. It enables TCC's
`-Werror=implicit-function-declaration` for the generated module, then restores
the ordinary category policy before compiling user C, private native providers
or XRT. It preserves the primary compilation diagnostic on failure. Raw C-only
compilation does not enter this helper.

## Diagnostic acceptance

The reports below are in `build/test`. They use explicit hashed compiler
object/header overrides; they do not certify a rebuilt shipping or complete
source product. The bundled-VFS build lane and final 53-module source-product
acceptance remain required.

- `stdlib53-b135-declaration-gate-linux-dev3/generated.declaration.gate.json`:
  11 phases, `Complete=true`, `InputsUnchanged=true`. Actual public CLI JIT and
  external-output compilation refuse the real previous Cgen's missing Tree
  factory declaration. GDB proves that the native factory is never executed;
  existing external output remains unchanged. Correct Cgen executes the two
  real recursive resource-domain fixtures through JIT.
- `stdlib53-b135-declaration-gate-win-dev4/generated.declaration.gate.json`:
  14 phases, the same completion and input-integrity flags. Public JIT, module
  relocation, EXE and DLL paths refuse real prior Cgen/private-header inputs,
  with GDB execution guards and preserved EXE/DLL outputs. The isolated positive
  directory includes an explicitly pinned source package for real TCC output.
- Both reports successfully compile and execute a mixed language/C fixture in
  JIT and an actual TCC-built executable. Its C sidecar deliberately contains a
  legacy implicit forward call returning C `int`; the explicitly declared
  language-facing wrapper returns `int64_t`. This proves the strict language
  policy does not leak into a later user C translation unit.
- `stdlib53-b135-gate-regression-win-dev1/gate.regression.json` and the Linux
  counterpart execute 14 representative existing fixtures each: CRT math,
  independent Queue families, Array algebra/Future references, scalar Stack,
  environment, number conversion/formatting and regex. They are JIT regressions,
  not additional allocation/OOM, concurrency or full-module acceptance claims.

The separate native Tree/Queue migration remains unsealed. Its earlier report
with `StrictApiNegativeControlAcceptance=false` is retained unchanged, rather
than reinterpreted as evidence for this subsequently corrected common helper.

## Checkers

`tools/check_generated_c_declaration_gate.py` accepts a Linux diagnostic
`overlay.linux.json`; the `_win.py` checker accepts its Windows `overlay.json`
counterpart. Both require `--root`, `--compiler`, a fresh `--out`, and repeated
`--fixture` arguments. Each manifest must name both the fixed public driver and
a genuine old-Cgen missing-prototype control; inputs are verified before and
after execution. Old report directories are never overwritten.
