#include "../../build/test/xlir_export_transaction.h"
#include <assert.h>
void tx_test_mode(int);

/* Public declarations alone must expose a diagnostic-independent status.
 * Include no internal compiler or runtime header in this translation unit. */
void tx_header_check(void)
{
    xl6_foreign_error_scope scope = {NULL, NULL, NULL, false};
    xl6_foreign_error_result result;
    assert(xl6ForeignCallBegin(&scope));
    assert(tx_pure(7) == 7);
    result = xl6ForeignCallEnd(&scope);
    assert(result.ok && result.error == NULL);
    xrtErrorFree(result.error);
    tx_test_mode(2);
    assert(xl6ForeignCallBegin(&scope));
    tx_void();
    result = xl6ForeignCallEnd(&scope);
    assert(!result.ok && result.error == NULL);
    xrtErrorFree(result.error);
}
