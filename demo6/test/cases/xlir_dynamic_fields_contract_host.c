#include "src/ir/xl6_ir.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "xlir_dynamic_fields_contract.inc"

int main(void)
{
    assert(testDynamicFieldsBridgeAbi());
    puts("DynamicFields effect contracts verified");
    return 0;
}
