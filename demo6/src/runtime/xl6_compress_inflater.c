#include "xl6_compress_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

typedef struct XL6_Inflater {XL6_TextThreadOwner owner;xinflate* stream;bool busy,failed;} XL6_Inflater;
static XL6_Inflater* check(void* raw,bool open)
{
    XL6_Inflater* self=raw;
    if (!self) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if (!xl6TextThreadCheck(&self->owner)) return NULL;
    if (self->busy || (open && !self->stream)) {xrtSetErrorKind(XERR_STATE);return NULL;}
    return self;
}
void xl6InflaterDestroyV1(void* raw)
{
    XL6_Inflater* self=raw;if (!self) return;
    if (self->busy) {xrtSetErrorKind(XERR_STATE);return;}
    xrtInflateDestroy(self->stream);xl6TextThreadDrop(&self->owner);xrtFree(self);
}
void* xl6InflaterCreateV1(int64_t format,int64_t limit,int64_t header,int64_t window)
{
    xinflateconfig config;if (!xl6InflateConfig(&config,format,limit,header,window)) return NULL;
    XL6_Inflater* self=xrtCalloc(1,sizeof(*self));if (!self) return NULL;
    if (!xl6TextThreadInit(&self->owner) || !(self->stream=xrtInflateCreate(&config))) {
        xerror* error=xrtTakeError();xl6InflaterDestroyV1(self);xrtClearError();xrtSetErrorTake(error);return NULL;
    }
    return self;
}
bool xl6InflaterResetV1(void* raw,int64_t format,int64_t limit,int64_t header,int64_t window)
{
    XL6_Inflater* self=check(raw,true);xinflateconfig config;
    if (!self || !xl6InflateConfig(&config,format,limit,header,window)) return false;
    self->busy=true;bool ok=xrtInflateReset(self->stream,&config);self->busy=false;
    if (ok) self->failed=false;
    return ok;
}
static bool write(void* raw,xbytesview input,void* target,bool final)
{
    XL6_Inflater* self=check(raw,true);xbuffer* output;
    if (!self || !xl6CompressOutputTarget(target,input,&output)) return false;
    if (self->failed) {xrtSetErrorKind(XERR_STATE);return false;}
    self->busy=true;bool ok=xrtInflateWrite(self->stream,input,final,xl6CompressOutput,output);
    self->busy=false;if (!ok) self->failed=true;return ok;
}
bool xl6InflaterWriteBytesV1(void* self,const xvalue* data,void* target,bool final,int64_t size)
{xbytesview input;return xl6BufferBytesInput(data,size,&input) && write(self,input,target,final);}
bool xl6InflaterWriteBufferV1(void* self,void* data,void* target,bool final,int64_t size)
{xbytesview input;return xl6BufferInput(data,size,&input) && write(self,input,target,final);}
bool xl6InflaterFinishV1(void* self,void* target)
{return write(self,(xbytesview){NULL,0},target,true);}
bool xl6InflaterDoneV1(void* raw)
{XL6_Inflater* self=check(raw,true);return self && xrtInflateDone(self->stream);}
bool xl6InflaterFailedV1(void* raw)
{XL6_Inflater* self=check(raw,true);return self && self->failed;}
uint64_t xl6InflaterOutputSizeV1(void* raw)
{XL6_Inflater* self=check(raw,true);return self?xrtInflateOutputSize(self->stream):0;}
bool xl6InflaterCloseV1(void* raw)
{
    XL6_Inflater* self=check(raw,false);if (!self) return false;
    xinflate* stream=self->stream;self->stream=NULL;xrtInflateDestroy(stream);return true;
}
bool xl6InflaterClosedV1(void* raw)
{XL6_Inflater* self=check(raw,false);return self && !self->stream;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6InflaterHandlePolicyV1Get,xl6InflaterDestroyV1)
