#ifndef XL6_NET_VECTOR_INTERNAL_H
#define XL6_NET_VECTOR_INTERNAL_H
#include "xl6_buffer_internal.h"
#include "xl6_net_datagram_internal.h"
#include "xl6_net_socket_owner.h"
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
XL6_ProductI64U64V1 xl6NetVectorSendV1(XL6_NetVectorCaptureV1,XL6_NativeClassArrayContextV1,
    void*,const xvalue*,const void*,const void*,int,int64_t);
/* Four ordinary pointer+length calls, selected by declarative overloads. */
#define XL6_NET_VECTOR_PRIMITIVE_ENTRIES(Name,Capture) \
XL6_ProductI64U64V1 xl6NetSocketSendVector##Name##V1(void* r,const xvalue* v,int64_t s) \
{return xl6NetVectorSendV1(Capture,(XL6_NativeClassArrayContextV1){0},r,v,NULL,NULL,0,s);} \
XL6_ProductI64U64V1 xl6NetSocketSendVectorTo##Name##V1(void* r,const xvalue* v,const void* a,int64_t s) \
{return xl6NetVectorSendV1(Capture,(XL6_NativeClassArrayContextV1){0},r,v,a,NULL,1,s);} \
XL6_ProductI64U64V1 xl6NetSocketSendMessageVector##Name##V1(void* r,const xvalue* v,const void* a,const void* c,int64_t s) \
{return xl6NetVectorSendV1(Capture,(XL6_NativeClassArrayContextV1){0},r,v,a,c,3,s);} \
XL6_ProductI64U64V1 xl6NetSocketSendConnectedMessageVector##Name##V1(void* r,const xvalue* v,const void* c,int64_t s) \
{return xl6NetVectorSendV1(Capture,(XL6_NativeClassArrayContextV1){0},r,v,NULL,c,2,s);}
#endif
