#include "xl6_text_value.h"
#include "xl6_native_class_borrow.h"
#include "src/api/xl_foreign_error.h"
void* xl6NetAddressListFromArrayV1(XL6_NativeClassArrayContextV1 context,const xvalue* array)
{
    xl6_error_scope work={0};if(!xl6ErrorScopeBegin(&work)){
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return NULL;}
    XL6_NativeClassArrayPinV1 pin={0};xnetaddr* addresses=NULL;xnetaddrlist* result=NULL;
    bool ok=xl6NativeClassArrayBeginV1(&pin,context,array,xl6NetAddressHandlePolicyV1Get());size_t count=0;
    if(ok){count=xrtTypedArrayCount(xl6SequenceStorageV1(pin.owner));
        if(!count){xrtSetErrorKind(XERR_VALUE);ok=false;}
        else if(count>INT64_MAX||count>SIZE_MAX/sizeof(*addresses)){xrtSetErrorKind(XERR_RANGE);ok=false;}}
    if(ok){addresses=xrtMalloc(count*sizeof(*addresses));ok=addresses!=NULL;}
    for(size_t i=0;ok&&i<count;++i){
        void* raw=NULL;xvalue* handle=xl6NativeClassArrayRetainAtV1(&pin,i,&raw);
        if(!handle){ok=false;break;}
        addresses[i]=*(const xnetaddr*)raw;xrtValueRelease(handle);
        if(xrtGetError()||work.failed)ok=false;
    }
    if(ok){result=xrtNetAddrListCreate(addresses,count);ok=result!=NULL;}
    if(!ok||xrtGetError()||work.failed)xl6ErrorScopeCaptureFailure(&work);
    xrtFree(addresses);xl6NativeClassArrayEndV1(&pin);
    if(xrtGetError()||work.failed)xl6ErrorScopeCaptureFailure(&work);
    xl6_error_result outcome=xl6ErrorScopeEnd(&work);
    if(!outcome.ok){xrtNetAddrListDestroy(result);result=NULL;xl6ErrorScopePropagateFailure(outcome);}
    else xrtErrorFree(outcome.error);
    return result;
}
