/* Old smaller V1 records must never reach the V2 Copy/Take/Drop implementation.
 * Compile only and require an undefined-symbol link rejection; never execute. */
#include "src/runtime/xl6_sequence.h"
extern bool xl6SequenceAdmissionCopyV1(XL6_Sequence*,void*);
extern bool xl6SequenceAdmissionTakeV1(XL6_Sequence*,void*);
extern void xl6SequenceAdmissionDropV1(void*);
int main(void)
{
    char oldRecord[sizeof(void*)*6]={0};
    bool result=xl6SequenceAdmissionCopyV1(NULL,oldRecord);
    result=xl6SequenceAdmissionTakeV1(NULL,oldRecord) || result;
    xl6SequenceAdmissionDropV1(oldRecord); return result ? 0 : 1;
}
