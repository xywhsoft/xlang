/* Capture/Describe are owner-only; Stage/DestroyPayload own no live GUI links.
 * A payload moves exactly once into Accept. No worker dereferences the ticket. */
struct xui_code_document_batch_payload_t {
    uint64_t serial;
    size_t count,used,limit;
    xui_document_batch_row_t* rows;
};
static xatomic64 __xuiDocumentBatchSerial={0};
static uint64_t __xuiDocumentBatchNextSerial(void)
{
    uint64 value=xrtAtomic64Load(&__xuiDocumentBatchSerial,XMEMORY_RELAXED);
    do{if(value==UINT64_MAX)return 0;}
    while(!xrtAtomic64CompareExchange(&__xuiDocumentBatchSerial,&value,value+1,XMEMORY_RELAXED,XMEMORY_RELAXED));
    return value+1;
}
static size_t __xuiDocumentBatchCaptureBytes(size_t count)
{return sizeof(struct xui_code_document_batch_t)+(count?count:1)*sizeof(xui_document_batch_row_t);}
XUI_API int xuiCodeDocumentBatchCapture(const xui_code_document_batch_item_t* items,
    size_t count,size_t budget,xui_code_document_batch* output)
{
    if(!output)return XUI_ERROR_INVALID_ARGUMENT;
    *output=NULL;
    if((count && !items) || count>512 || budget>256u*1024u*1024u)return XUI_ERROR_INVALID_ARGUMENT;
    size_t limit=budget?budget:128u*1024u*1024u;
    if(__xuiDocumentBatchCaptureBytes(count)>limit)return XUI_ERROR_OUT_OF_MEMORY;
    xui_code_document_batch batch=xrtMalloc(sizeof(*batch));
    if(!batch)return XUI_ERROR_OUT_OF_MEMORY;
    memset(batch,0,sizeof(*batch));batch->thread=xrtThreadCurrentId();batch->limit=limit;
    batch->used=sizeof(*batch);batch->busy=true;
    int status=__xuiDocumentBatchAllocate(&batch->used,limit,(count?count:1)*sizeof(*batch->rows),(void**)&batch->rows,true);
    if(status!=XUI_OK){__xuiDocumentBatchFree(batch);return status;}
    batch->count=count;batch->serial=__xuiDocumentBatchNextSerial();
    if(!batch->serial)status=XUI_ERROR_LIMIT_EXCEEDED;
    for(size_t i=0;status==XUI_OK && i<count;++i){
        xui_document_batch_row_t* row=batch->rows+i;row->document=items[i].pDocument;
        for(size_t j=0;j<i;++j)if(row->document==batch->rows[j].document)status=XUI_ERROR_INVALID_ARGUMENT;
        if(status==XUI_OK)status=__xuiCodeDocumentCallBegin(row->document,&row->call);
    }
    size_t total=0;
    for(size_t i=0;status==XUI_OK && i<count;++i){
        xui_document_batch_row_t* row=batch->rows+i;xui_code_document d=row->document;
        if(d->iEditDepth || d->bApplyingHistory || d->iNextState==UINT64_MAX ||
            d->iUndoCount<0 || d->iUndoCount==INT_MAX || d->iChangeVersion!=items[i].iChangeVersion){status=XUI_ERROR_INVALID_STATE;break;}
        if(d->iLength<0 || d->iLength>8*1024*1024 || (size_t)d->iLength>64u*1024u*1024u-total){status=XUI_ERROR_INVALID_ARGUMENT;break;}
        total+=(size_t)d->iLength;row->version=d->iVersion;row->changeVersion=d->iChangeVersion;
        row->state=d->iState;row->nextState=d->iNextState;row->beforeLength=d->iLength;
        row->beforeLines=d->iLineCount;row->undoCount=d->iUndoCount;
        status=xuiCodeDocumentLifetimeWatch(d,&row->lifetime,__xuiDocumentBatchGone,row);
    }
    status=__xuiDocumentBatchFinishCalls(batch,status);batch->busy=false;
    for(size_t i=0;status==XUI_OK && i<count;++i)if(!batch->rows[i].document)status=XUI_ERROR_INVALID_STATE;
    if(status!=XUI_OK){__xuiDocumentBatchFree(batch);return status;}
    *output=batch;return XUI_OK;
}
XUI_API int xuiCodeDocumentBatchDescribe(xui_code_document_batch batch,
    xui_code_document_batch_layout_t* layout,xui_code_document_batch_source_t* sources,size_t capacity)
{
    if(!batch || !layout || (batch->count && !sources))return XUI_ERROR_INVALID_ARGUMENT;
    if(batch->thread!=xrtThreadCurrentId() || batch->busy)return XUI_ERROR_BUSY;
    if(batch->attempted || batch->ready)return XUI_ERROR_INVALID_STATE;
    if(capacity<batch->count)return XUI_ERROR_BUFFER_TOO_SMALL;
    *layout=(xui_code_document_batch_layout_t){sizeof(*layout),batch->serial,batch->count,__xuiDocumentBatchCaptureBytes(batch->count)};
    for(size_t i=0;i<batch->count;++i)sources[i]=(xui_code_document_batch_source_t){sizeof(*sources),batch->rows[i].beforeLength,batch->rows[i].undoCount};
    return XUI_OK;
}
static int __xuiDocumentBatchStopped(int (*cancelled)(void*),void* user)
{return cancelled && cancelled(user);}
static int __xuiDocumentBatchCopy(char* destination,const char* source,size_t size,
    int (*cancelled)(void*),void* user)
{
    for(size_t at=0;at<size;){
        if(__xuiDocumentBatchStopped(cancelled,user))return XUI_ERROR_INVALID_STATE;
        size_t step=size-at>65536?65536:size-at;memcpy(destination+at,source+at,step);at+=step;
    }
    return XUI_OK;
}
static int __xuiDocumentBatchBuildRow(xui_code_document_batch_payload payload,
    xui_document_batch_row_t* row,const xui_code_document_batch_source_t* source,
    const xui_code_document_batch_text_t* text,int (*cancelled)(void*),void* user)
{
    if(source->iSize!=sizeof(*source) || text->iSize!=sizeof(*text) || source->iLength<0 ||
        source->iLength>8*1024*1024 || source->iUndoCount<0 || source->iUndoCount==INT_MAX ||
        text->Before.Size>8u*1024u*1024u || text->After.Size>8u*1024u*1024u ||
        (text->Before.Size && !text->Before.Data) || (text->After.Size && !text->After.Data))return XUI_ERROR_INVALID_ARGUMENT;
    if((size_t)source->iLength!=text->Before.Size)return XUI_ERROR_INVALID_STATE;
    const char *before=text->Before.Data?text->Before.Data:"",*after=text->After.Data?text->After.Data:"";
    if(memchr(before,0,text->Before.Size) || memchr(after,0,text->After.Size) ||
        memchr(before,'\r',text->Before.Size) || memchr(after,'\r',text->After.Size))return XUI_ERROR_INVALID_ARGUMENT;
    if(__xuiCodeDocumentValidateUtf8(NULL,before,(int)text->Before.Size)!=XUI_OK ||
        __xuiCodeDocumentValidateUtf8(NULL,after,(int)text->After.Size)!=XUI_OK)return XUI_ERROR_INVALID_ARGUMENT;
    row->beforeLength=source->iLength;row->undoCount=source->iUndoCount;row->afterLength=(int)text->After.Size;
    row->changed=text->Before.Size!=text->After.Size || memcmp(before,after,text->Before.Size);
    int status=XUI_OK;
    if(!row->changed){
        if(text->Before.Size){
            status=__xuiDocumentBatchAllocate(&payload->used,payload->limit,text->Before.Size+1,(void**)&row->proof,false);
            if(status==XUI_OK){status=__xuiDocumentBatchCopy(row->proof,before,text->Before.Size,cancelled,user);row->proof[text->Before.Size]=0;}
        }
        return status;
    }
    row->afterLines=__xuiCodeDocumentTextLineBreaks(after,row->afterLength)+1;
    if(__xuiDocumentBatchStopped(cancelled,user))return XUI_ERROR_INVALID_STATE;
    status=__xuiDocumentBatchAllocate(&payload->used,payload->limit,text->After.Size+4096,(void**)&row->buffer,false);
    if(status==XUI_OK)status=__xuiDocumentBatchAllocate(&payload->used,payload->limit,text->After.Size+1,(void**)&row->snapshot,false);
    if(status==XUI_OK)status=__xuiDocumentBatchAllocate(&payload->used,payload->limit,(size_t)row->afterLines*sizeof(*row->lines),(void**)&row->lines,false);
    if(status==XUI_OK && (size_t)row->undoCount+1>SIZE_MAX/sizeof(*row->undo))status=XUI_ERROR_OUT_OF_MEMORY;
    if(status==XUI_OK)status=__xuiDocumentBatchAllocate(&payload->used,payload->limit,((size_t)row->undoCount+1)*sizeof(*row->undo),(void**)&row->undo,false);
    if(status==XUI_OK)status=__xuiDocumentBatchAllocate(&payload->used,payload->limit,sizeof(*row->group.pActions),(void**)&row->group.pActions,true);
    size_t strings=(text->Before.Size?text->Before.Size+1:0)+(text->After.Size?text->After.Size+1:0);
    if(status==XUI_OK && strings>payload->limit-payload->used)status=XUI_ERROR_OUT_OF_MEMORY;
    if(status!=XUI_OK)return status;
    payload->used+=strings;row->group.iActionCount=row->group.iActionCapacity=1;
    status=__xuiCodeDocumentActionInit(NULL,row->group.pActions,0,before,row->beforeLength,after,row->afterLength);
    if(status==XUI_OK)status=__xuiDocumentBatchCopy(row->buffer,after,text->After.Size,cancelled,user);
    if(status==XUI_OK)status=__xuiDocumentBatchCopy(row->snapshot,after,text->After.Size,cancelled,user);
    if(status!=XUI_OK)return status;
    row->snapshot[text->After.Size]=0;
    int line=0,start=0;
    for(int j=0;j<row->afterLength;++j){
        if(!(j%65536) && __xuiDocumentBatchStopped(cancelled,user))return XUI_ERROR_INVALID_STATE;
        if(after[j]=='\n'){row->lines[line++]=(xui_code_line_t){start,j+1,j};start=j+1;}
    }
    row->lines[line]=(xui_code_line_t){start,row->afterLength,row->afterLength};return XUI_OK;
}
XUI_API void xuiCodeDocumentBatchPayloadDestroy(xui_code_document_batch_payload payload)
{
    if(!payload)return;
    for(size_t i=0;i<payload->count;++i)__xuiDocumentBatchFreePayloadRow(payload->rows+i);
    xrtFree(payload->rows);xrtFree(payload);
}
XUI_API int xuiCodeDocumentBatchStage(const xui_code_document_batch_layout_t* layout,
    const xui_code_document_batch_source_t* sources,const xui_code_document_batch_text_t* texts,
    size_t budget,int (*cancelled)(void*),void* user,xui_code_document_batch_payload* output)
{
    if(!output)return XUI_ERROR_INVALID_ARGUMENT;
    *output=NULL;
    if(!layout || layout->iSize!=sizeof(*layout) || !layout->iCapture || layout->iDocuments>512 ||
        layout->iCaptureBytes!=__xuiDocumentBatchCaptureBytes(layout->iDocuments) ||
        (layout->iDocuments && (!sources || !texts)) || budget>256u*1024u*1024u)return XUI_ERROR_INVALID_ARGUMENT;
    size_t limit=budget?budget:128u*1024u*1024u;
    if(layout->iCaptureBytes>limit || sizeof(struct xui_code_document_batch_payload_t)>limit-layout->iCaptureBytes)return XUI_ERROR_OUT_OF_MEMORY;
    if(__xuiDocumentBatchStopped(cancelled,user))return XUI_ERROR_INVALID_STATE;
    xui_code_document_batch_payload payload=xrtMalloc(sizeof(*payload));
    if(!payload)return XUI_ERROR_OUT_OF_MEMORY;
    memset(payload,0,sizeof(*payload));payload->serial=layout->iCapture;payload->limit=limit;
    payload->used=layout->iCaptureBytes+sizeof(*payload);
    int status=__xuiDocumentBatchAllocate(&payload->used,limit,(layout->iDocuments?layout->iDocuments:1)*sizeof(*payload->rows),(void**)&payload->rows,true);
    if(status==XUI_OK)payload->count=layout->iDocuments;
    size_t beforeBytes=0,afterBytes=0;
    for(size_t i=0;status==XUI_OK && i<payload->count;++i){
        if(__xuiDocumentBatchStopped(cancelled,user)){status=XUI_ERROR_INVALID_STATE;break;}
        if(texts[i].Before.Size>64u*1024u*1024u-beforeBytes || texts[i].After.Size>64u*1024u*1024u-afterBytes){status=XUI_ERROR_INVALID_ARGUMENT;break;}
        beforeBytes+=texts[i].Before.Size;afterBytes+=texts[i].After.Size;
        status=__xuiDocumentBatchBuildRow(payload,payload->rows+i,sources+i,texts+i,cancelled,user);
    }
    if(status==XUI_OK && __xuiDocumentBatchStopped(cancelled,user))status=XUI_ERROR_INVALID_STATE;
    if(status!=XUI_OK){xuiCodeDocumentBatchPayloadDestroy(payload);return status;}
    *output=payload;return XUI_OK;
}
static const char* __xuiDocumentBatchBefore(const xui_document_batch_row_t* row)
{return row->changed?(row->group.pActions->sDeleted?row->group.pActions->sDeleted:""):(row->proof?row->proof:"");}
static bool __xuiDocumentBatchBeforeMatches(xui_code_document d,const xui_document_batch_row_t* staged)
{
    if(d->iLength!=staged->beforeLength)return false;
    const char* before=__xuiDocumentBatchBefore(staged);int prefix=d->iGapStart;
    return (!prefix || !memcmp(d->sBuffer,before,(size_t)prefix)) &&
        (prefix==d->iLength || !memcmp(d->sBuffer+d->iGapEnd,before+prefix,(size_t)(d->iLength-prefix)));
}
XUI_API int xuiCodeDocumentBatchAccept(xui_code_document_batch batch,
    xui_code_document_batch_payload payload,bool* taken)
{
    if(!taken)return XUI_ERROR_INVALID_ARGUMENT;
    *taken=false;
    if(!batch || !payload)return XUI_ERROR_INVALID_ARGUMENT;
    if(batch->thread!=xrtThreadCurrentId() || batch->busy)return XUI_ERROR_BUSY;
    if(batch->ready || batch->attempted || batch->serial!=payload->serial || batch->count!=payload->count || payload->used>batch->limit)return XUI_ERROR_INVALID_STATE;
    batch->busy=true;int status=XUI_OK;
    for(size_t i=0;status==XUI_OK && i<batch->count;++i){
        xui_document_batch_row_t* row=batch->rows+i;const xui_document_batch_row_t* staged=payload->rows+i;
        status=__xuiCodeDocumentCallBegin(row->document,&row->call);
        if(status==XUI_OK && (!__xuiDocumentBatchCurrent(row) || row->beforeLength!=staged->beforeLength ||
            row->undoCount!=staged->undoCount || !__xuiDocumentBatchBeforeMatches(row->document,staged)))status=XUI_ERROR_INVALID_STATE;
    }
    if(status==XUI_OK){
        for(size_t i=0;i<batch->count;++i){
            xui_document_batch_row_t *row=batch->rows+i,*staged=payload->rows+i;
            row->changed=staged->changed;row->afterLength=staged->afterLength;row->afterLines=staged->afterLines;
            row->proof=staged->proof;row->buffer=staged->buffer;row->snapshot=staged->snapshot;
            row->lines=staged->lines;row->undo=staged->undo;row->group=staged->group;
            staged->proof=staged->buffer=staged->snapshot=NULL;staged->lines=NULL;staged->undo=NULL;
            memset(&staged->group,0,sizeof(staged->group));
        }
        batch->used=payload->used;batch->ready=true;*taken=true;
        xuiCodeDocumentBatchPayloadDestroy(payload);
    }
    status=__xuiDocumentBatchFinishCalls(batch,status);batch->busy=false;return status;
}
