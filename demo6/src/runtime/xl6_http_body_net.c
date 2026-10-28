#include "xl6_http_body_internal.h"
#include "xl6_net_buffer_internal.h"
static int64_t xl6HttpBodyReadQueue(void* raw,void* input,void* target,bool end,bool discard)
{
    XL6_HttpBodyReader* self=raw;XL6_NetBufferV1* queue=input;
    XL6_HttpGuard body={0};XL6_NetBufferGuardV1 buffer={0};xbuffer* out=discard?NULL:xl6BufferChecked(target);
    if((!discard&&!out)||!xl6HttpEnter(&self->gate,&body))return 0;
    if(!xl6NetBufferEnterV1(queue,&buffer)){xl6HttpLeave(&body);return 0;}
    xnetspan span={0};(void)xrtNetBufFront(&queue->native,&span);
    xbytesview view={span.Data,span.Size};int64_t state=0;
    bool ok=discard||xl6HttpOutput(out,view,&out);
    if(ok)state=xl6HttpBodyReadLocked(self,view,out,end&&span.Size==queue->native.Size);
    if(ok&&!self->failed&&!xrtGetError()&&state==XHTTP1_BODY_MORE&&!self->consumed&&span.Size<queue->native.Size){
        /* Data chunks use the first owned span directly. Only a split framing
         * line / trailer prefix is coalesced, bounded by its configured limit.
         * Include one overflow byte so a full limit cannot loop as More. */
        size_t maximum=self->body.Limits.MaxChunkLine;
        if(self->body.Limits.MaxTrailer>maximum)maximum=self->body.Limits.MaxTrailer;
        if(maximum<SIZE_MAX)++maximum;
        size_t wanted=queue->native.Size<maximum?queue->native.Size:maximum;
        if(wanted>span.Size){
            ok=xrtNetBufPullup(&queue->native,wanted,&span);
            if(ok){view=(xbytesview){span.Data,span.Size};state=xl6HttpBodyReadLocked(self,view,out,end&&span.Size==queue->native.Size);}
        }
    }
    if(ok&&!self->failed&&!xrtGetError()){
        if(self->consumed>queue->native.Size)xrtSetErrorKind(XERR_INTERNAL);
        else if(xrtNetBufConsume(&queue->native,self->consumed)!=self->consumed)xrtSetErrorKind(XERR_INTERNAL);
    }
    xl6NetBufferLeaveV1(&buffer);xl6HttpLeave(&body);return state;
}
int64_t xl6Http1BodyReaderReadFromNetBufferV2(void* raw,void* input,void* target,bool end)
{return xl6HttpBodyReadQueue(raw,input,target,end,false);}
int64_t xl6Http1BodyReaderDiscardFromNetBufferV2(void* raw,void* input,bool end)
{return xl6HttpBodyReadQueue(raw,input,NULL,end,true);}
