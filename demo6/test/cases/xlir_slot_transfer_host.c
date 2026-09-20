/* Keep generated declaration-only C in its own translation unit. The native
 * inspector consumes real XRT headers separately, with no declaration shims. */
#include "xlir_slot_transfers.c"
#include "xlir_slot_transfers.bind.h"
void slot_test_write(int64_t value) { SLOT_WRITE(value); }
int64_t slot_test_read(void) { return SLOT_READ(); }
