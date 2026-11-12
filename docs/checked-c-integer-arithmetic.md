# Checked native C integer arithmetic

The user selected checked C integer arithmetic on 2026-11-12. Overflow,
division by zero and invalid shifts must raise language errors. This is separate
from the already supported checked explicit native constructors/conversions.

The pure foundation is `src/runtime/xl6_checked_c_integer.h`. It performs
signed/unsigned add, subtract, multiply, divide, modulo and shifts for an exact
execution width without invoking undefined C arithmetic. Errors return an
authoritative status and leave the caller's output unchanged. It allocates
nothing and does not depend on a language frame or error allocation.

Signed minimum divided or reduced modulo minus one is overflow. Shift counts
must be nonnegative and less than the left operand's width; a full unsigned
count carrier preserves negative and very large counts as invalid values.
Signed left shift requires a nonnegative left operand and a representable
result. Signed right shift uses explicit arithmetic sign extension without
depending on implementation-defined C negative right shifts. Unsigned results
must fit their domain, including subtraction and left shift.

`tools/check_checked_c_integer.py` checks every pair of 8-bit values, boundary
and deterministic random cases through 64 bits, every relevant shift boundary,
invalid domains and failure-output preservation against a separate 128-bit
oracle. O0/O2 on Windows and Linux pass 4,710,526 assertions per execution;
Linux also passes undefined-behavior sanitization. The public foundation header
passes C++11/C++17 syntax checks. Evidence is pinned in
`checked-c-integer-foundation-20261112.json`.

The source compiler now carries an exact native integer execution domain in
Typed/Lowered XLIR, emits explicit checked RuntimeOps for ordinary arithmetic,
negation and compound updates, and verifies an adjacent failure CFG. Native
C type rows retain their actual C ABI. No AST recovery or C spelling lookup is
needed by Cgen. Shift counts keep the original integer width until validation.
Native helpers use the target C compiler's actual widths and signedness.

Arithmetic helpers return an explicit bool and write their output only on
success. ERROR_TEST uses that bool in ordinary, async and generator machines.
A missing allocated Error cannot turn a failed operation into a committed
write. Error creation is deliberately failed in actual generated C; global
storage remains unchanged and a following valid call succeeds. C ABI callers
also verify that caught conversion/arithmetic errors do not poison an outer
checked scope. Static and dynamic native conversion providers were corrected
to publish their diagnostic and false result without prematurely latching the
parent scope.

The existing semantic promotion rules remain authoritative: these checks apply
when the selected execution type is a native C integer. A mixed expression
promoted to a language numeric type retains that type's existing rules. This
batch does not introduce C's usual integer promotions or change language int
arithmetic. The language has no compound shift assignment syntax; ordinary
shift expressions and assignments are checked without adding that syntax.

Windows and Linux source preflights each pass 29 checks: all 17 native integer
aliases with target-derived bounds, JIT, O0/O2 generated C and real C ABI
callers, transactional property/field/fixed-array writes, conversion failure,
error allocation failure, three exported/runtime header standards, and
existing scalar/builtin integer regressions. At each optimization level,
53 IR mutations cause 106 empty C/header refusals. The pure 4,710,526-assertion
arithmetic oracle and UBSan foundation remain separately qualified.

A separate continuation gate verifies checked native arithmetic in async
functions and in a generator body with an int result carrier, including failure
after an await/yield. It does not add or certify generator<c_int>, which is
currently rejected by the existing type-pool contract.

Success-path helpers allocate nothing and introduce no dynamic value boxing.
Checks, output/status temporaries and ordinary C calls/stack storage still have
costs; no CPU throughput or zero-overhead claim is made. Complete O2 compiler,
TCC, XRT, fixed-object, VFS and embedded-source candidates are being rebuilt.
Source preflight does not certify or install those final runtime bytes. Evidence
is indexed in `checked-c-integer-preflight-20261112.json` under
`dev/release-materials/provenance/`.

Windows r30 complete O2 acceptance subsequently passed all 47 main commands and
five continuation follow-up commands. CLI, independent programs, actual embedded
compilers in parent EXE/Python shared hosts, image/particle/literal and source-free
gates pass. Async outcomes execute in CLI JIT and native EXE; the follow-up shared
fixture exercises a valid synchronous generator body, preserving the language's
existing restrictions on top-level execution/main/await in shared exports. Relative
to r29, xl.dll and xlc.dll each grow 7,168 bytes and xl.exe is unchanged. Linux r22
remains in progress. See `checked-c-integer-windows-20261112.json`; this qualifies
the isolated cohort and does not install final release/IDE products.
