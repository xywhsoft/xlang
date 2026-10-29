/* Reuse the complete genuine-key source host, including 1000 lifetimes and
 * whole-operation OOM/copy/project/box prefixes. The extra fault publishes
 * a genuine owned foreign result AND a diagnostic: caller must retire it. */
#ifdef XL6_NATIVE_VALUE_TEST_FAULTS
#include <stdbool.h>
static bool reportedForeignFailure;
#define XL6_NATIVE_VALUE_REPORTED_FAILURE reportedForeignFailure
#endif
#include "stdlib_native_value_ir_host.c"
#ifdef XL6_NATIVE_VALUE_TEST_FAULTS
extern XL6_SlotMapKeyV1 __real_xl6SlotMapKeyCloneV1(XL6_SlotMapKeyV1);
XL6_SlotMapKeyV1 __wrap_xl6SlotMapKeyCloneV1(XL6_SlotMapKeyV1 input)
{
    XL6_SlotMapKeyV1 result=__real_xl6SlotMapKeyCloneV1(input);
    if (fail(0)) {
        reportedForeignFailure=(faultAt%2)!=0;
        if (reportedForeignFailure) xrtSetErrorKind(XERR_STATE);
        else xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
        printf("owned-foreign-result reported=%u\n",(unsigned)reportedForeignFailure);
    }
    return result;
}
#endif
