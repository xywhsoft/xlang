# wasmrun - WebAssembly i32 subset interpreter

A minimal WebAssembly binary module interpreter that parses and executes
the i32 instruction subset. Built to stress-test xlang's binary decoding,
value-stack manipulation, structured control flow, and recursive function
calls.

## Usage

```bat
cd /d D:\GIT\x-lang\release

:: run via interpreter
xl run projects\dev\wasmrun\wasmrun.xl -- test
xl run projects\dev\wasmrun\wasmrun.xl -- run    projects\dev\wasmrun\sample.wasm
xl run projects\dev\wasmrun\wasmrun.xl -- disasm projects\dev\wasmrun\sample.wasm

:: build standalone exe
xl build projects\dev\wasmrun\wasmrun.xl -o projects\dev\wasmrun\wasmrun.exe

wasmrun.exe test
wasmrun.exe run    sample.wasm
wasmrun.exe disasm sample.wasm
```

Note: `xl run <script.xl> -- <args>` — the `--` separator is required so
`xl` forwards positional args to the script instead of treating them as
link libraries.

## Commands

| Command | Description |
|---|---|
| `run <file.wasm>` | Parse, find `_start` or `main` export, invoke; exit code = return value |
| `disasm <file.wasm>` | Dump types / functions / exports / globals / per-function instruction listing |
| `test` | Run 18 built-in tests using hand-crafted `.wasm` byte sequences |
| `-h` / `--help` | Usage |

## Implemented WASM subset

### Binary format
- Magic + version header
- Type section (function signatures, all value types parsed)
- Function section (func -> type index binding)
- Global section (i32 init expressions, mutable + const)
- Export section (func / table / mem / global kinds)
- Code section (local declarations + body bytes)
- Unknown sections skipped

### i32 instructions
- **Constants**: `i32.const`
- **Stack**: `drop`, `select`
- **Comparison**: `i32.eqz`, `i32.eq`, `i32.ne`, `i32.lt_s`, `i32.gt_s`,
  `i32.le_s`, `i32.ge_s` (plus unsigned variants `lt_u` / `gt_u` / `le_u` / `ge_u`)
- **Arithmetic**: `i32.add`, `i32.sub`, `i32.mul`, `i32.div_s`, `i32.rem_s`,
  `i32.and`, `i32.or`, `i32.xor`, `i32.shl`, `i32.shr_s` (plus `div_u` / `rem_u` / `shr_u`)
- **Control flow**: `block`, `loop`, `if`, `else`, `end`, `br`, `br_if`,
  `return`, `call`
- **Variables**: `local.get`, `local.set`, `local.tee`, `global.get`, `global.set`
- `unreachable`, `nop`

### Execution model
- Per-call value stack (preallocated `array<int>`, 512 slots)
- Per-call label stack with target PC, stack height, arity, is-loop flag
- Structured control flow: each `block` / `loop` / `if` opener is pre-matched
  with its `end` (and `else` for `if`) during a separate analysis pass that
  correctly skips instruction operands (so `0x0b` / `0x05` bytes inside
  `i32.const` immediates are not mistaken for opcodes).
- `br L` / `br_if L`: pop L labels above the target; for `loop` targets the
  label is retained (re-entry), for `block` / `if` targets it is popped (exit).
- `call`: builds args from value stack, recurses via `invoke()`, pushes result.

## Test coverage (18 tests, 60+ assertions)

| # | Area | What it checks |
|---|---|---|
| T1 | `i32.const` | function return value |
| T2 | arithmetic | `(10+20)*3-8 == 82` |
| T3 | comparisons | all 6 signed variants |
| T4 | locals | `local.get` / `local.set` with params + negatives |
| T4b | `local.tee` | tee into a declared local |
| T5 | `call` | two-function module, `main` calls `add` |
| T6 | `block`+`loop`+`br`+`br_if` | iterative `sumTo(n)` |
| T7 | `if`/`else` with result | `abs(x)` |
| T8 | recursion | `fib(n)` up to fib(15)=987 |
| T9 | `drop` / `select` | stack manipulation |
| T10 | mutable globals | counter increment across calls |
| T11 | bit ops | `and` / `or` / `xor` / `shl` / `shr_s` (incl. arithmetic on negatives) |
| T12 | signed division | `div_s` / `rem_s` with mixed signs (truncation toward zero) |
| T13 | `i32.eqz` | |
| T14 | traps | `div_s` by zero throws (caught via try/catch) |
| T15 | `loop` + `mul` | iterative factorial fact(10)=3628800 |
| T16 | nested `if` | sign classifier (-1 / 0 / 1) |
| T17 | parser state | module validity after parse |

## Files

| File | Purpose |
|---|---|
| `wasmrun.xl` | Main source (interpreter + tests + CLI) |
| `gen_sample.xl` | Helper that emits `sample.wasm` (3 funcs: add, fib, main) |
| `sample.wasm` | 101-byte hand-built module for `run` / `disasm` demos |
| `wasmrun.exe` | Built executable |

## Limitations

- Blocktype assumed single-byte (void `0x40`, value `0x7f`/`0x7e`/`0x7d`/`0x7c`,
  or type index 0..63). Multi-byte s33 blocktypes not supported.
- `i64` / `f32` / `f64` value types parsed in signatures but not executed.
- Multi-result functions unsupported (`invoke` returns single `int`).
- No imports, no linear memory, no tables, no `br_table`, no reference types.
