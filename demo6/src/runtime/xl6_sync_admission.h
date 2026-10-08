#ifndef XL6_SYNC_ADMISSION_H
#define XL6_SYNC_ADMISSION_H
#include "src/runtime/xl6_sequence.h"
XL6_TEXT_API xvalue* xl6SequenceSyncCreateV1(unsigned int,uint64_t,uint64_t,uint64_t,bool,unsigned int);
XL6_TEXT_API const XL6_SequenceAdmissionOpsV2* xl6ArraySyncAdmissionOpsV2Get(void);
#endif
