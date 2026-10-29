/* Old V1/V2 records/ops must never reach the V3 Copy/Take/Drop implementation.
 * Compile only and require an undefined-symbol link rejection; never execute. */
#include "src/runtime/xl6_sequence.h"
#ifdef XL6_TEST_RETIRED_SEQUENCE_V2
extern bool xl6SequenceAdmissionCopyV2(XL6_Sequence*,void*);
extern bool xl6SequenceAdmissionTakeV2(XL6_Sequence*,void*);
extern void xl6SequenceAdmissionDropV2(void*);
#define xl6SequenceAdmissionCopyV1 xl6SequenceAdmissionCopyV2
#define xl6SequenceAdmissionTakeV1 xl6SequenceAdmissionTakeV2
#define xl6SequenceAdmissionDropV1 xl6SequenceAdmissionDropV2
#else
extern bool xl6SequenceAdmissionCopyV1(XL6_Sequence*,void*);
extern bool xl6SequenceAdmissionTakeV1(XL6_Sequence*,void*);
extern void xl6SequenceAdmissionDropV1(void*);
#endif
int main(void)
{
    char oldRecord[sizeof(void*)*6]={0};
    bool result=xl6SequenceAdmissionCopyV1(NULL,oldRecord);
    result=xl6SequenceAdmissionTakeV1(NULL,oldRecord) || result;
    xl6SequenceAdmissionDropV1(oldRecord); return result ? 0 : 1;
}
