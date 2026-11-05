# Retained callback text input: R36 investigation

No compiler, runtime, public package, release or example source was modified.

## Supported declaration

```xl
extern "const xstrview*" class TextView @header("native.h") @lifetime("callback")
 native point handle
 extern "Size" @c_field get length() as c_size
 extern "Data" @c_field @c_length("Size","size_t") @c_result("text_view") get text() as string
end
```

`length` is a byte count. `text` explicitly creates an owned language string
using Data + Size. It preserves embedded NUL; there is no strlen or C
forwarding function. The native callback receives a stack `const xstrview*`.
The native publisher must retain the backing JSON/value/source and callback
snapshot through the complete thunk return and snapshot Drop. Native empty
strings use a present Data pointer + size zero; a nonnullable text getter does
not reinterpret NULL Data as a present empty string.

A developer saves `view.text` inside the callback. Saving the epoch view itself
is permitted, but later reading its length/text (including from a captured
closure) fails checked after the callback ends. The view does not secretly
extend the JSON source lifetime. Ordinary language copies/capture ownership
and checked callback/module bookkeeping still apply.

## Existing restriction (not loosened)

`func(string)` and `func(string?)` native callback inputs are rejected in R36.
`xl6SemCheckNativeCallbackType` explicitly rejects managed string parameters
at src/sem/xl6_sem.c:21468. The closed
`xl6IrForeignCallbackParameterMatches` input branch in
src/ir/xl6_ir_foreign_parameter.c:645 independently requires trivial VALUE or
verified foreign-view BORROW. This is a real restriction, not evidence that
string escape handling was accepted. The xstrview branch in
xl6IrCgenCallbackNativeType is also used by the supported `owned_text` return
contract; it alone does not imply managed string inputs are supported.

## Actual evidence

Full frozen R36 compilers (Windows1416/Linux1415 source units), matching
runtime and fresh R36 Only-TCC/VFS providers were used. Per platform:

- Cgen and lowered IR; strict C11 GCC O0/O2 builds and execution.
- Actual TCC run/module/exe plus standalone execution (exe emits only).
- Three explicit ordinary-string callback semantic negatives, zero C output.

Each program execution performs 21 native callbacks over NUL + UTF8, a
present empty string and a single NUL. The native source is checked live and
unchanged, then poisoned and freed; allocator churn follows. Global strings,
explicitly copied closure captures, function-return strings, boxed strings and
owned-text return values remain correct. Nine copied strings are also proved
distinct from the live native allocation before it is freed. Saved views and
closures capturing the original view are checked-rejected after return.

The pure-length callback asserts exactly 0 allocations/frees/reallocations.
The explicit text-copy + owned_text return path measures exactly 1 allocation
and 1 corresponding native free. Other instrumented cases include ordinary
language global snapshots, closures, boxes and test comparisons, and are not
claimed to have only one total allocation. Epoch setup/checks, TLS/identity,
closure/code-owner pins and checked error scopes remain real work.

Early failed experiment reports remain preserved: the direct-string rejection
is expected; a C fixture initializer warning and a disallowed absolute @header
spelling were corrected in later isolated inputs. Neither was bypassed by a
Core or Native wrapper change. No GPU/UI/network operation was performed.
