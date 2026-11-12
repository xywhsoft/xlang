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

Compiler integration is **not complete**. It must carry the execution domain
through Typed/Lowered XLIR, include unary negation and compound transactions,
validate failure CFG and malformed IR, and serialize calls through a shared
runtime contract. Cgen must not recover semantic information from source ASTs.
The current typed probe also shows a critical shift issue: `c_int << uint64`
currently narrows its count to `c_int`. Integration must preserve the original
count until validation, so a count of `2^32` cannot become zero. Mixed numeric
promotion must remain an explicit semantic decision.

Required subsequent gates include all C integer aliases on LLP64 and LP64,
normal/unary/compound expressions and every destination kind, OOM/error
propagation, exported C ABI, generated C, JIT, independent programs, and actual
embedded compilers. The foundation gate does not certify those paths or change
the current compiler's behavior by itself.
