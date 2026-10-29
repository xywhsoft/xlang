# xlang

**One language. Native speed. Script-level agility. No boundary in between.**

**English** · [中文](README_zh.md)

---

## What is xlang?

xlang is a statically-typed, C-interoperable programming language that compiles through C to native code, yet runs with the immediacy of a scripting language. Its entire toolchain — compiler, in-memory native backend, standard library, package system, debugger, IDE — is self-contained in this repository.

Two ideas define xlang:

1. **The wall between "compiled program" and "script" does not exist.** An xlang program executes xlang source text at runtime; that script can execute further scripts — the same language all the way down, with no dialect, no second runtime, no ecosystem split.
2. **Static speed and dynamic flexibility live in the same object.** Fix the fields you care about into native-layout slots; let the optional long tail attach as dynamic keys — one instance, one line of syntax, no hand-built hybrid types.

---

## Why xlang

### 1 · One language — no wall between compiled code and scripts

Most ecosystems bolt a scripting language onto a compiled one: C++ hosting Lua, or VB versus VBScript. Two languages, two type systems, two sets of tools, FFI glue everywhere, and a hard line between "engine" and "mod".

xlang removes that line by construction:

- **Compiled mode** (`xl build`): ahead-of-time compilation to a standalone native `.exe` / `.dll`.
- **Immediate mode** (`xl run`): the same source is compiled in memory to machine code and executed — no temp files, no interpreter, no penalty for being "just a script".
- **Runtime execution**: a *running* xlang program can compile and run xlang source text on the fly:

```xlang
import xlang

// Inside a compiled xlang program — run xlang source text, right now.
// It goes through the exact same pipeline: lex → parse → semantic
// type-check → C codegen → in-memory native code.
assert xlang.run("println(\"hello from a script\")\n")

// Run a mod from disk...
assert xlang.runFile("mods/boss_ai.xl")

// ...or hold a long-lived script context with its own state.
XLangContext ctx = xlang.Context()
if not ctx.run(modSource) then
    println(ctx.lastError)
end
```

That script can itself call `xlang.run(...)` — nesting has no bottom, and every level is type-checked and compiled, not interpreted.

**Why this matters for games (and any engine + mod architecture):** write the engine as an xlang `.exe`, ship game modules as `.xl` text files, and both halves are the same language — same types, same standard library, same packages, same debugger and IDE. The module is not a guest inside a foreign runtime; it is a first-class citizen of the same one.

**Embedding from C is equally first-class.** `xl.dll` (runtime) and `xlc.dll` (compiler) expose a small, stable C API:

```c
#include "xl.h"

XL_ContextOptions opts = xlDefaultContextOptions();
XL_Context* ctx = xlCreateContext(&opts);

// Expose engine functions to scripts; xlang declares them
// with `extern "host_spawn_particle" func ...`.
xlRegisterCFunction(ctx, "host_spawn_particle", &spawn_particle);

// Run a script file: type-checked, compiled to native code in-process.
xlRunScriptFile(ctx, "scripts/main.xl");

// Or keep a module alive and call its exports:
//   XL_Module* m = xlCompileScriptFileToModule(ctx, "mods/game.xl");
//   xlActivateModule(ctx, m, arg);
//   void* fn = xlModuleGetSymbol(m, "mod_update");
// (xlc.dll) Or compile straight to a standalone exe:
//   xlCompileScriptFileToExe(ctx, "scripts/main.xl", "game.exe");
```

There is even a `tcc.*` standard-library module, so an xlang program can compile and run **C source** at runtime too — useful for shader-like snippets, plugins, and JIT-style extension points.

### 2 · Static speed, dynamic flexibility — in the same object

Native languages give you a fast struct with fixed fields — but real domains (game entities, config, JSON from the wild) are full of optional fields you don't know up front. Scripting languages give you a flexible object — but every field access is a hash lookup, and every value is boxed.

xlang lets one instance carry both halves, and *you* choose which world each field lives in:

```xlang
class Player extends object
    derive value

    int    id    = 0            // static field — fixed slot, native access speed
    string name  = ""
    Stats  stats = Stats()      // nested static object
    var    tags  = []           // dynamic value slot
end

auto p = Player()
p.id            = 7             // static write: plain memory access
p.stats.hp      = 120           // nested static access
p.rank          = 9             // dynamic field: appears on the instance on demand
p["title"]      = "knight"      // dynamic key access

// Serialization sees both halves and round-trips them typed:
auto jsonText = json.stringify(p)
auto q        = json.parse::<Player>(jsonText)
println(q.name)                 // static field, typed
println(q.rank)                 // dynamic field, preserved
```

`extends object` is the only ceremony — no builder classes, no variant visitors, no reflection framework. C++ and C# *can* build something equivalent by hand; xlang makes it the default shape of an object, as simple as a scripting language, while the static half still compiles down to plain C.

The same philosophy runs through the whole type system:

| Capability | What you write |
|---|---|
| Dynamic values when wanted | `var x = ...` — scalars, containers, functions, objects |
| Nullable with flow typing | `T?`, `?.`, `?[...]`, `??`; `if v != null` narrows the type |
| Runtime type checks | `v is T`, `v as T` (throws) / `v as T?` (returns null) |
| Typed containers | `array<T>`, `list<T>`, `set<T>`, `dict<T>` |
| Operator protocols on your types | `__add__`, `__call__`, `__iter__`, `__contains__`, `__eq__`, ... |
| Derived behavior | `derive string / hash / equals / value` |
| Protocols as constraints | `func cloneAll<T is cloneable>(array<T>) as array<T>` |
| Managed weak references | `weak<T>` with `lock()` returning `T?` |

---

## Language at a glance

```xlang
// UTF-8 source; Chinese identifiers are ordinary identifiers.
func 斐波那契(int n) as int
    if n < 2 then
        return n
    end
    return 斐波那契(n - 1) + 斐波那契(n - 2)
end

// Enums with data: static tag + union payload layout.
enum FetchResult<T>
    pending
    ready(T value)
    failed(int code, string message)
end

func formatResult(FetchResult<int> result) as string
    return match result
    case pending then "pending"
    case ready(value) then f"ready:{value}"
    case failed(code, message) then f"failed:{code}:{message}"
    end
end

func main(...) as int
    auto name = "xlang"
    println(f"hello {name}, fib(10) = {斐波那契(10)}")
    println(formatResult(FetchResult::<int>.ready(42)))
    return 0
end
```

- **Syntax**: newline- or `;`-separated; nested `/* */` comments; f-strings / raw / multiline strings; time literals; array / list / dict / set literals.
- **Functions**: first-class, typed signatures, multiple return values (`as (int, string)`), default and named arguments, `...` array / `***` dict variadics.
- **OOP**: classes with inheritance, `virtual` / `override` / `final`, access control, static members, properties (`get` / `let`), native-backed classes wrapping C handles.
- **Protocols & generics**: user-declared protocols, structural conformance (no `implements` ceremony), turbofish instantiation `max::<int>(a, b)`.
- **Errors**: `try` / `catch` / `finally` / `throw` with a classified error model (e.g. `ZeroDivisionError`, `TypeError`, `ArithmeticError`).
- **Concurrency**: `async` / `await` / `yield`, threads, mutex / cond / semaphore, `channel<T>`, generators, task APIs (`spawn` / `all` / `race` / `cancel`).
- **Modules**: `import geometry`, `import geometry as geo`, `from geometry import Point, distance as dist`; `export` for public symbols; `.xl` source is the single source of truth for public interfaces.
- **Text**: strings are UTF-8 by default (`len` counts codepoints, `byteLen` counts bytes); `Buffer` for binary data.

## Compiled to C, native everywhere

The pipeline is fixed and fully visible:

```
source (.xl) ─▶ lex ─▶ parse ─▶ semantic / typed HIR (XLIR) ─▶ C codegen ─▶ embedded TCC ─▶ native
```

- **C interop is the backbone, not a bridge**: `.xl` and `.c` files compile together in one `xl build`; C functions enter xlang via `extern "symbol"` declarations; xlang exports back to C with ABI metadata and auto-generated headers; callbacks work in both directions.
- **In-memory backend**: `xl run` keeps everything in RAM by default (no `tmp/` litter); a built-in VFS (dynamic → disk → static) serves embedded sources.
- **Ship either way**: run immediately, compile to `.exe` / `.dll`, or compile at runtime from inside your program.

## Standard library & package ecosystem

- **Core standard library**: 500+ public functions across dozens of modules — `net` (TCP / UDP / TLS / resolver / proxy), `crypto` (AES-GCM, ChaCha20-Poly1305, SHA family, Ed25519, X25519, RSA...), `json` / `xson`, `regex`, `template`, `fs` / `dir` / `file` (sync + async), `process`, `os`, `time`, `encoding`, `codec`, `log`, `math`, `random`, `signal`, `task`, and more — built on [XRT](docs/XRT集成原则.md), the shared runtime base.
- **214 ready extension packages** (`release/extlibs/`), one `xlang.package.json` manifest each, multi-version coexistence supported: `sqlite`, `zeromq`, `imgui`, `webview`, `glad`, `freetype`, `harfbuzz`, `stb_image`, `zip` / `zstd` / `brotli` / `lz4`, `yaml` / `toml` / `xml` / `csv`, `jwt`, `etcd`, `docker`, `kubernetes`, plus an AI corner (`ggml`, `whisper`, `faiss`, `hnswlib`, `tokenizers`) and the full `xge` game stack and `xui` / `ui.auto` UI stacks.
- **Real games, shipped as examples** (`release/examples/games/`): nine complete games — Snake, Tetris, 2048, Flappy, Pong, Breakout, Minesweeper, Asteroids, Shooter — written in xlang on the XGE extension library (window, input, shapes, fonts, textures, sound).

```bat
release\xl.exe run release\examples\games\snake.xl
```

## Toolchain

```bat
xl run main.xl                  :: compile in memory and run, no temp files
xl build main.xl                :: native exe (add -shared for a dll)
xl check main.xl                :: full semantic checking
xl lex / parse / ir / cgen / abi :: inspect every pipeline stage
xl impdef module.dll -o m.def   :: import C symbols from existing DLLs
```

- Windows resources (version info, icon, manifest) via `--res file.json`.
- A dedicated **xlang IDE** (`ide/`, XGE-based) provides project management, C/xlang dual-syntax editing, semantic completion and navigation, and build/run — one tool for both halves of the boundary.

## Platforms & building

- **Platforms**: Windows x64 (primary) and Linux x86_64.
- **Self-contained toolchain**: the compiler is written in C11 and builds with an embedded TCC; no external SDK is required to bootstrap.

```bat
build.bat          :: Windows
test.bat           :: main regression (4,700+ case files)
```

```bash
bash build.sh && bash test_linux.sh
```

## Repository layout

| Path | Contents |
|---|---|
| `src/` | Compiler, runtime integration and standard library |
| `docs/` | Design documents (Chinese) — the single source of truth for design decisions |
| `release/examples/` | 450+ example programs incl. games and apps |
| `test/` | Regression assets: cases, suites, fixtures, interop tests |
| `ide/` | xlang IDE sources, builds, and portable releases |
| `DEMO6说明.md` | Feature checklist of the current compiler (Chinese) |

## Status

xlang is in release-candidate hardening: the four-stage pipeline, module/package system, DLL embedding API, and a large regression suite (4,700+ cases, guarded by `rc0_guard`) are in place; work continues on network data-path ownership models and the extension-library ecosystem. Design documentation is currently Chinese-first under `docs/`.

License: to be announced.
