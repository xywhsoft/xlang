# Borrowed C string results

Declare a NUL-terminated borrowed `const char*` result with
`@c_result("cstring")` and a logical `string` or `string?` return type:

```xlang
extern "native_title" @header("native.h") @no_raise @c_result("cstring") func title() as string?
```

The compiler calls the native producer once and copies its result before
releasing receiver/argument temporaries. The returned string owns its bytes.
NULL becomes absent for `string?`; non-NULL empty text remains present. NULL
for `string` is a checked value error. A copy failure is checked even when
allocating an error diagnostic also fails, and does not replace an existing
assignment target. `@no_raise` describes the producer; the language copy is
still fallible.

The native producer must return readable, terminated text for the duration of
the call boundary. Embedded NUL terminates the result; use `text_view` for
length-preserving text. This contract copies bytes and does not add Unicode
validation. If the producer reports an XRT error, admission checks that error
before reading the pointer. The compiler neither frees nor retains the
borrowed native pointer.

This is a direct function-return contract, including methods, property getters
and callable values. It cannot be combined with `@owned`, field reads, receiver
dereferencing, output slots or counted-field annotations. IR records the
physical `const char*` result separately from its managed logical type and
rejects mismatched contracts before emitting any C or public-header bytes.

There is no forwarding C wrapper. A present result costs a length scan, one
allocation and a byte copy plus existing checked-boundary bookkeeping; input
projection may have its own cost. It is not a zero-copy string view. The
fixture measures one producer invocation and one allocation for a normal
result, separately from input setup.

`tools/check_foreign_cstring_results.py` checks frozen public-source compiler
cohorts: JIT/module/EXE, GCC O0/O2, C/C++ prototypes, temporary input aliases,
independent snapshots, nullable results, failed producers with unreadable
sentinel pointers, diagnostic-OOM assignment rollback, declaration negatives
and IR mutation refusal. Full O2/VFS/embedded compiler qualification and final
SDK/IDE installation are recorded separately.

Windows and Linux source cohorts each passed 29 commands, including 55 IR
mutations and 110 refused C/header emissions at each optimization level.
The successor receipt is
`dev/release-materials/provenance/foreign-cstring-source-20261113.json`.

Windows full O2 r32 subsequently passes the 17-command actual delivery gate,
including independent and nested EXE/shared hosts. `getTitle()` passes 19
checks; old text/bytes CLI regressions and the ten-command source-free gate
also pass. All 13,525 frozen inputs and three products remain unchanged.
Compared with r31, the CLI is unchanged and each DLL adds 2,048 bytes.
See `foreign-cstring-windows-20261113.json`; Linux full qualification and final
installation remain separate.

Linux full O2 r25 subsequently passes the same 17/19/6/10 checks. Its 15,084
frozen inputs and all products remain unchanged; total executable/shared
library sizes equal Linux r24. See `foreign-cstring-linux-20261113.json`.
Both-platform CPU/compiler qualification for this contract is complete;
package shared initialization, graphics and final installation retain their
separate gates.
