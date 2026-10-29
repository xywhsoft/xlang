#ifndef XL6_NET_VECTOR_INPUT_H
#define XL6_NET_VECTOR_INPUT_H
#include "xl6_buffer_internal.h"
#include "xl6_native_class_borrow.h"
#include "xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"
enum {VECTOR_MAX=64,TEXT_INPUT=1,BYTES_INPUT=2};
typedef struct XL6_NetVectorInputsV1 {
    XL6_Sequence* primitive;
    XL6_SequenceAdmissionV3 admission;
    XL6_NativeClassArrayPinV1 classes;
    xvalue* held[VECTOR_MAX];
    xbuffer* buffers[VECTOR_MAX];
    xnetspan spans[VECTOR_MAX];
    size_t count;
} XL6_NetVectorInputsV1;
typedef bool (*XL6_NetVectorCaptureV1)(XL6_NetVectorInputsV1*,XL6_NativeClassArrayContextV1,const xvalue*);
bool xl6NetVectorPrimitiveCaptureV1(XL6_NetVectorInputsV1*,const xvalue*,int,
    bool (*)(const XL6_SequenceAdmissionV3*,uint64_t));
bool xl6NetVectorBuffersCaptureV1(XL6_NetVectorInputsV1*,XL6_NativeClassArrayContextV1,const xvalue*);
bool xl6NetVectorOutputCaptureV1(XL6_NetVectorInputsV1*,XL6_NativeClassArrayContextV1,const xvalue*);
void xl6NetVectorCaptureFailureV1(xl6_error_scope*,bool);
void xl6NetVectorDiscardV1(XL6_NetVectorInputsV1*,xl6_error_scope*);
bool xl6NetVectorSelectSizeV1(XL6_NetVectorInputsV1*,int64_t);
#endif
