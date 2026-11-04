# Native call admission and escaped callbacks

Native-view arguments enter their checked call scopes before the producer
receives a raw pointer. A native `CallBegin` success confirms that its scope
must be ended; it does not cancel a language callback failure raised during
admission. The enclosing error scope's independent `failed` bit is authoritative,
including when no diagnostic could be allocated or native code cleared it.

Generated C checks that outcome after each native-view admission. Failure stops
later argument admission and the producer. Every successfully entered scope is
still ended in reverse order. Secondary diagnostics and cleanup errors cannot
replace the first escaping failure. Direct calls, bound methods, protocol
witnesses and function-pointer fields use the same preparation code.

This adds inline outcome checks and diagnostic lookups to native-view admission;
it adds no allocation, copied payload, forwarding function or additional error
scope. It does not change the native ABI or turn native success into a promise
that an escaping language error was handled.

The CPU fixture `test/cases/foreign_native_preparation.xl` covers swallowed
diagnostics, allocation failure, secondary errors, paired scope cleanup and
multi-operand short-circuiting. `tools/check_foreign_native_preparation.py` runs
it at O0/O2 against hash-verified compiler objects and an explicit frontend
overlay. That focused gate does not qualify a complete release binary.
