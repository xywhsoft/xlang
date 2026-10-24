#include "xl6_tls_role_context_internal.h"
#include "xl6_array_input.h"
#include <stdlib.h>
#include <string.h>
static int compare(const void* left,const void* right)
{
    const xstrview* a=left;const xstrview* b=right;
    size_t count=a->Size<b->Size?a->Size:b->Size;int order=memcmp(a->Data,b->Data,count);
    return order?order:(a->Size>b->Size)-(a->Size<b->Size);
}
bool xl6TlsProtocolsReadV1(const xvalue* values,XL6_TlsProtocolsV1** output)
{
    if(xrtGetError())return false;
    if(!output || *output)return xl6TlsFailV1(XERR_ARGUMENT,"TLS protocol input needs an empty output");
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope)){
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return false;}
    XL6_ArrayInputV1 input={0};XL6_TlsProtocolsV1* prepared=NULL;xstrview local[16],*sorted=local;
    /* TLS extension_data includes the two-byte ProtocolNameList length. The
     * shortest protocol consumes two more bytes; check before detaching slots. */
    bool ok=xl6ArrayInputAcquireBoundedV1(values,(UINT16_MAX-2u)/2u,&input);
    size_t bytes=0,wire=2u;
    for(size_t i=0;ok && i<input.count;++i){
        xbytesview view={0};ok=xrtValueGetBytes(input.items[i],&view);
        if(ok && (!view.Size || view.Size>UINT8_MAX || view.Size+1u>UINT16_MAX-wire))
            ok=xl6TlsFailV1(XERR_RANGE,"TLS protocol names need 1..255 bytes within the complete ALPN budget");
        if(ok){wire+=1u+view.Size;bytes+=view.Size;}
    }
    if(ok && input.count){
        size_t prefix=sizeof(*prepared)+input.count*sizeof(*prepared->views);
        prepared=xrtMalloc(prefix+bytes);ok=prepared!=NULL;
        if(ok){
            prepared->count=input.count;prepared->bytes=bytes;char* data=(char*)prepared+prefix;
            for(size_t i=0;ok && i<input.count;++i){
                xbytesview view={0};ok=xrtValueGetBytes(input.items[i],&view);
                if(ok){prepared->views[i]=(xstrview){data,view.Size};memcpy(data,view.Data,view.Size);data+=view.Size;}
            }
        }
        if(ok && input.count>sizeof(local)/sizeof(*local)){sorted=xrtMalloc(input.count*sizeof(*sorted));ok=sorted!=NULL;}
        if(ok){
            memcpy(sorted,prepared->views,input.count*sizeof(*sorted));qsort(sorted,input.count,sizeof(*sorted),compare);
            for(size_t i=1;ok && i<input.count;++i)if(!compare(&sorted[i-1],&sorted[i]))
                ok=xl6TlsFailV1(XERR_EXISTS,"TLS protocol names must be distinct exact byte sequences");
        }
    }
    if(!ok || scope.failed || xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    if(sorted!=local)xrtFree(sorted);
    xl6ArrayInputReleaseV1(&input);if(xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xrtFree(prepared);xl6ErrorScopePropagateFailure(outcome);return false;}
    xrtErrorFree(outcome.error);*output=prepared;return true;
}
