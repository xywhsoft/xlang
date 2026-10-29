/* Separate TU, as for a real embedded artifact. Test-only observation of the
 * generated private capability does not add a production escape API. */
#include "xlir_code_owner_binding.generated.h"
#include "xlir_code_owner_binding.generated.c"
const XL6_CodeOwner* test_code_owner(void) { return &xlir_code_owner; }
int test_code_owner_sealed(void) { return xlir_code_owner_sealed; }
