#include "release/include/xrt_decl.h"

/* Test-only adapters preserve XRT's const-qualified public declarations while
 * exposing the language's ordinary borrowed-var ABI to the fixtures. */
xvalue* xlir_json_observe_lifetime(xvalue* target)
{
    return xrtValueWeakRef(target);
}

bool xlir_json_lifetime_expired(xvalue* witness)
{
    return xrtValueWeakRefExpired(witness);
}
