/* Deliberately retain a legacy C forward call without a prototype: the stricter
 * generated-language ABI policy must not leak into a user C translation unit.
 * The implicit callee really returns C int; the explicitly exported forwarder
 * uses the language int / int64_t ABI. Neither returns an implicit pointer. */
#include <stdint.h>
int64_t declaration_gate_legacy_forward(void)
{
    return declaration_gate_legacy_value();
}
int declaration_gate_legacy_value(void)
{
    return 42;
}
