#ifndef XL6_SEMAPHORE_ADMISSION_H
#define XL6_SEMAPHORE_ADMISSION_H
#include "src/runtime/xl6_sequence.h"
XL6_TEXT_API xvalue* xl6SequenceSemCreateV1(unsigned int, uint64_t, uint64_t, uint64_t, bool);
XL6_TEXT_API const XL6_SequenceAdmissionOpsV2* xl6ArraySemAdmissionOpsV2Get(void);
#endif
