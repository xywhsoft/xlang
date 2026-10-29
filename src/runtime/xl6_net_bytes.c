#include "xl6_net_bytes_internal.h"
static void* copy(xbytesview view,int64_t size)
{
    if(!xl6BufferSelect(&view,size)){xl6NetBytesFailedV1();return NULL;}
    xnetbytes* result=xrtNetBytesCopy(view.Data,view.Size);
    if(!result)xl6NetBytesFailedV1();
    return result;
}
void* xl6NetBytesCreateV1(void){return copy((xbytesview){0},-1);}
void* xl6NetBytesAdoptV1(void* owned)
{
    if(!owned){xrtSetErrorKind(XERR_ARGUMENT);xl6NetBytesFailedV1();return NULL;}
    if(xrtNetBytesView(owned).Size>(size_t)INT64_MAX){
        xrtNetBytesDestroy(owned);xrtSetErrorKind(XERR_RANGE);xl6NetBytesFailedV1();return NULL;
    }
    return owned;
}
void* xl6NetBytesFromTextV1(xstrview text,int64_t size)
{return copy((xbytesview){(const uint8_t*)text.Data,text.Size},size);}
void* xl6NetBytesFromBytesV1(const xvalue* value,int64_t size)
{
    xbytesview view={0};
    if(!xl6BufferBytesInput(value,size,&view)){xl6NetBytesFailedV1();return NULL;}
    return copy(view,-1);
}
void xl6NetBytesDestroyV1(void* raw){xrtNetBytesDestroy(raw);}
int64_t xl6NetBytesLengthV1(const void* raw){return (int64_t)xrtNetBytesView(raw).Size;}
int64_t xl6NetBytesByteAtV1(const void* raw,int64_t index)
{
    xbytesview view={0};
    if(!xl6NetBytesRangeV1(raw,1,index,&view)||!view.Size){
        if(!xrtGetError())xrtSetErrorKind(XERR_RANGE);
        xl6NetBytesFailedV1();return 0;
    }
    return view.Data[0];
}
xvalue* xl6NetBytesToBytesV1(const void* raw,int64_t size,int64_t offset)
{
    xbytesview view={0};
    if(!xl6NetBytesRangeV1(raw,size,offset,&view)){xl6NetBytesFailedV1();return NULL;}
    xvalue* result=xrtValueBytes(view);if(!result)xl6NetBytesFailedV1();return result;
}
static bool trace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    return visit&&xrtValueGetHandle(value,&raw,&ops,&user)&&raw&&!user&&
        ops==xl6NetBytesHandlePolicyV1Get()->Ops&&visit(xrtNetBytesOwnership(raw),context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6NetBytesHandlePolicyV1Get,xl6NetBytesDestroyV1,trace)
