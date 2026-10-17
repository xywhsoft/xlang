#include "xl6_compress_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

typedef struct XL6_Deflater {XL6_TextThreadOwner owner;xdeflate* stream;bool busy,failed;} XL6_Deflater;
static XL6_Deflater* check(void* raw,bool open)
{
    XL6_Deflater* self=raw;
    if (!self) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if (!xl6TextThreadCheck(&self->owner)) return NULL;
    if (self->busy || (open && !self->stream)) {xrtSetErrorKind(XERR_STATE);return NULL;}
    return self;
}
void xl6DeflaterDestroyV1(void* raw)
{
    XL6_Deflater* self=raw;if (!self) return;
    if (self->busy) {xrtSetErrorKind(XERR_STATE);return;}
    xrtDeflateDestroy(self->stream);xl6TextThreadDrop(&self->owner);xrtFree(self);
}
void* xl6DeflaterCreateV1(int64_t format,int64_t level,int64_t strategy,int64_t limit,int64_t window)
{
    xdeflateconfig config;if (!xl6DeflateConfig(&config,format,level,strategy,limit,window)) return NULL;
    XL6_Deflater* self=xrtCalloc(1,sizeof(*self));if (!self) return NULL;
    if (!xl6TextThreadInit(&self->owner) || !(self->stream=xrtDeflateCreate(&config))) {
        xerror* error=xrtTakeError();xl6DeflaterDestroyV1(self);xrtClearError();xrtSetErrorTake(error);return NULL;
    }
    return self;
}
bool xl6DeflaterResetV1(void* raw,int64_t format,int64_t level,int64_t strategy,int64_t limit,int64_t window)
{
    XL6_Deflater* self=check(raw,true);xdeflateconfig config;
    if (!self || !xl6DeflateConfig(&config,format,level,strategy,limit,window)) return false;
    self->busy=true;bool ok=xrtDeflateReset(self->stream,&config);self->busy=false;
    if (ok) self->failed=false;
    return ok;
}
static bool write(void* raw,xbytesview input,void* target,int64_t flush)
{
    XL6_Deflater* self=check(raw,true);xbuffer* output;
    if (!self) return false;
    if (flush<0 || flush>3) {xrtSetErrorKind(XERR_RANGE);return false;}
    if (!xl6CompressOutputTarget(target,input,&output)) return false;
    if (self->failed || xrtDeflateDone(self->stream)) {xrtSetErrorKind(XERR_STATE);return false;}
    self->busy=true;
    bool ok=xrtDeflateWrite(self->stream,input,(xdeflateflush)flush,xl6CompressOutput,output);
    self->busy=false;if (!ok) self->failed=true;return ok;
}
bool xl6DeflaterWriteBytesV1(void* self,const xvalue* data,void* target,int64_t flush,int64_t size)
{xbytesview input;return xl6BufferBytesInput(data,size,&input) && write(self,input,target,flush);}
bool xl6DeflaterWriteTextV1(void* self,xstrview text,void* target,int64_t flush,int64_t size)
{return xl6TextSelectRaw(&text,size) && write(self,(xbytesview){(const uint8*)text.Data,text.Size},target,flush);}
bool xl6DeflaterWriteBufferV1(void* self,void* data,void* target,int64_t flush,int64_t size)
{xbytesview input;return xl6BufferInput(data,size,&input) && write(self,input,target,flush);}
bool xl6DeflaterFinishV1(void* self,void* target)
{return write(self,(xbytesview){NULL,0},target,XDEFLATE_FLUSH_FINISH);}
bool xl6DeflaterDoneV1(void* raw)
{XL6_Deflater* self=check(raw,true);return self && xrtDeflateDone(self->stream);}
bool xl6DeflaterFailedV1(void* raw)
{XL6_Deflater* self=check(raw,true);return self && self->failed;}
uint64_t xl6DeflaterOutputSizeV1(void* raw)
{XL6_Deflater* self=check(raw,true);return self?xrtDeflateOutputSize(self->stream):0;}
bool xl6DeflaterCloseV1(void* raw)
{
    XL6_Deflater* self=check(raw,false);if (!self) return false;
    xdeflate* stream=self->stream;self->stream=NULL;xrtDeflateDestroy(stream);return true;
}
bool xl6DeflaterClosedV1(void* raw)
{XL6_Deflater* self=check(raw,false);return self && !self->stream;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6DeflaterHandlePolicyV1Get,xl6DeflaterDestroyV1)
