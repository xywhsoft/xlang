#include "xl6_net_vector_internal.h"
XL6_ProductI64U64V1 xl6NetSocketSendVectorBufferV1(XL6_NativeClassArrayContextV1 x,void* r,const xvalue* v,int64_t s)
{return xl6NetVectorSendV1(xl6NetVectorBuffersCaptureV1,x,r,v,NULL,NULL,0,s);}
XL6_ProductI64U64V1 xl6NetSocketSendVectorToBufferV1(XL6_NativeClassArrayContextV1 x,void* r,const xvalue* v,const void* a,int64_t s)
{return xl6NetVectorSendV1(xl6NetVectorBuffersCaptureV1,x,r,v,a,NULL,1,s);}
XL6_ProductI64U64V1 xl6NetSocketSendMessageVectorBufferV1(XL6_NativeClassArrayContextV1 x,void* r,const xvalue* v,const void* a,const void* c,int64_t s)
{return xl6NetVectorSendV1(xl6NetVectorBuffersCaptureV1,x,r,v,a,c,3,s);}
XL6_ProductI64U64V1 xl6NetSocketSendConnectedMessageVectorBufferV1(XL6_NativeClassArrayContextV1 x,void* r,const xvalue* v,const void* c,int64_t s)
{return xl6NetVectorSendV1(xl6NetVectorBuffersCaptureV1,x,r,v,NULL,c,2,s);}
