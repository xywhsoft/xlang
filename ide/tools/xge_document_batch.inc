/* Additive XUI document primitive. Included in the Document implementation,
 * after its existing helpers; the IDE never inspects private document storage. */
typedef struct xui_document_batch_row_t {
    xui_code_document document;
    xui_lifetime_link_t lifetime;
    xui_code_document_call_t call;
    uint32_t version,changeVersion;
    uint64_t state,nextState;
    int beforeLength,beforeLines,afterLength,afterLines,undoCount,changed;
    char *buffer,*snapshot;
    xui_code_line_t* lines;
    xui_code_edit_group_t *undo,group;
} xui_document_batch_row_t;
struct xui_code_document_batch_t {
    uint64_t thread;
    size_t count,used,limit;
    bool busy,attempted;
    xui_document_batch_row_t* rows;
};
static void __xuiDocumentBatchGone(void* user)
{ ((xui_document_batch_row_t*)user)->document=NULL; }
static int __xuiDocumentBatchAllocate(xui_code_document_batch batch,size_t bytes,void** out)
{
    *out=NULL;
    if(bytes>batch->limit-batch->used)return XUI_ERROR_OUT_OF_MEMORY;
    void* memory=xrtMalloc(bytes);
    if(!memory)return XUI_ERROR_OUT_OF_MEMORY;
    memset(memory,0,bytes);batch->used+=bytes;*out=memory;return XUI_OK;
}
static void __xuiDocumentBatchFree(xui_code_document_batch batch)
{
    if(!batch)return;
    for(size_t i=0;i<batch->count;++i){
        xui_document_batch_row_t* row=batch->rows+i;
        xuiLifetimeUnwatch(&row->lifetime);
        xrtFree(row->buffer);xrtFree(row->snapshot);xrtFree(row->lines);
        xrtFree(row->undo);__xuiCodeDocumentGroupClear(&row->group);
    }
    xrtFree(batch->rows);xrtFree(batch);
}
static int __xuiDocumentBatchFinishCalls(xui_code_document_batch batch,int status)
{
    xui_cleanup_receipt_t receipt={0};xuiInternalCleanupStart(&receipt,status);
    for(size_t i=batch->count;i;--i)
        xuiInternalCleanupAdd(&receipt,__xuiCodeDocumentCallEnd(&batch->rows[i-1].call,XUI_OK));
    return xuiInternalCleanupEnd(NULL,&receipt);
}
static bool __xuiDocumentBatchLive(const xui_document_batch_row_t* row)
{
    xui_code_document d=row->document;
    return d && !d->bDestroyPending && !d->bDestroying &&
        (!d->pInternalOwner || xuiInternalWidgetIsValid(d->pInternalOwner)) &&
        (!row->call.context || (!row->call.context->bDestroyPending &&
            !row->call.context->bDestroying && !row->call.context->bCloseRequested));
}
static bool __xuiDocumentBatchCurrent(const xui_document_batch_row_t* row)
{
    xui_code_document d=row->document;
    return __xuiDocumentBatchLive(row) && d->iVersion==row->version &&
        d->iChangeVersion==row->changeVersion && d->iState==row->state &&
        d->iNextState==row->nextState && d->iLength==row->beforeLength &&
        d->iUndoCount==row->undoCount && !d->iEditDepth && !d->bApplyingHistory;
}
XUI_API int xuiCodeDocumentBatchPrepare(const xui_code_document_batch_item_t* items,
    size_t count,size_t budget,xui_code_document_batch* output)
{
    if(!output)return XUI_ERROR_INVALID_ARGUMENT;
    *output=NULL;
    if((count && !items) || count>512 || budget>256u*1024u*1024u)return XUI_ERROR_INVALID_ARGUMENT;
    size_t limit=budget?budget:128u*1024u*1024u;
    if(sizeof(struct xui_code_document_batch_t)>limit)return XUI_ERROR_OUT_OF_MEMORY;
    xui_code_document_batch batch=xrtMalloc(sizeof(*batch));
    if(!batch)return XUI_ERROR_OUT_OF_MEMORY;
    memset(batch,0,sizeof(*batch));batch->thread=xrtThreadCurrentId();batch->limit=limit;
    batch->used=sizeof(*batch);batch->busy=true;
    int status=__xuiDocumentBatchAllocate(batch,(count?count:1)*sizeof(*batch->rows),(void**)&batch->rows);
    if(status!=XUI_OK){__xuiDocumentBatchFree(batch);return status;}
    batch->count=count;
    for(size_t i=0;status==XUI_OK && i<count;++i){
        xui_document_batch_row_t* row=batch->rows+i;row->document=items[i].pDocument;
        for(size_t j=0;j<i;++j)if(row->document==batch->rows[j].document)status=XUI_ERROR_INVALID_ARGUMENT;
        if(status==XUI_OK)status=__xuiCodeDocumentCallBegin(row->document,&row->call);
    }
    size_t beforeBytes=0,afterBytes=0;
    for(size_t i=0;status==XUI_OK && i<count;++i){
        xui_document_batch_row_t* row=batch->rows+i;xui_code_document d=row->document;
        const xui_code_document_batch_item_t* item=items+i;
        if(item->Before.Size>8u*1024u*1024u || item->After.Size>8u*1024u*1024u ||
            item->Before.Size>64u*1024u*1024u-beforeBytes || item->After.Size>64u*1024u*1024u-afterBytes ||
            (item->Before.Size && !item->Before.Data) || (item->After.Size && !item->After.Data)){
            status=XUI_ERROR_INVALID_ARGUMENT;break;
        }
        beforeBytes+=item->Before.Size;afterBytes+=item->After.Size;
        const char *before=item->Before.Data?item->Before.Data:"",*after=item->After.Data?item->After.Data:"";
        if(d->iEditDepth || d->bApplyingHistory || d->iNextState==UINT64_MAX ||
            d->iUndoCount<0 || d->iUndoCount==INT_MAX || d->iChangeVersion!=item->iChangeVersion ||
            d->iLength!=(int)item->Before.Size ||
            !__xuiCodeDocumentRangeEquals(d,0,d->iLength,before,(int)item->Before.Size)){
            status=XUI_ERROR_INVALID_STATE;break;
        }
        if(memchr(before,0,item->Before.Size) || memchr(after,0,item->After.Size) ||
            memchr(before,'\r',item->Before.Size) || memchr(after,'\r',item->After.Size)){
            status=XUI_ERROR_INVALID_ARGUMENT;break;
        }
        status=__xuiCodeDocumentValidateUtf8(d,after,(int)item->After.Size);
        if(status!=XUI_OK)break;
        row->version=d->iVersion;row->changeVersion=d->iChangeVersion;row->state=d->iState;row->nextState=d->iNextState;
        row->beforeLength=d->iLength;row->beforeLines=d->iLineCount;row->afterLength=(int)item->After.Size;row->undoCount=d->iUndoCount;
        row->changed=item->Before.Size!=item->After.Size || memcmp(before,after,item->Before.Size);
        status=xuiCodeDocumentLifetimeWatch(d,&row->lifetime,__xuiDocumentBatchGone,row);
        if(status!=XUI_OK || !row->changed)continue;
        row->afterLines=__xuiCodeDocumentTextLineBreaks(after,row->afterLength)+1;
        status=__xuiDocumentBatchAllocate(batch,item->After.Size+4096,(void**)&row->buffer);
        if(status==XUI_OK)status=__xuiDocumentBatchAllocate(batch,item->After.Size+1,(void**)&row->snapshot);
        if(status==XUI_OK)status=__xuiDocumentBatchAllocate(batch,(size_t)row->afterLines*sizeof(*row->lines),(void**)&row->lines);
        if(status==XUI_OK && (size_t)row->undoCount+1>SIZE_MAX/sizeof(*row->undo))status=XUI_ERROR_OUT_OF_MEMORY;
        if(status==XUI_OK)status=__xuiDocumentBatchAllocate(batch,((size_t)row->undoCount+1)*sizeof(*row->undo),(void**)&row->undo);
        if(status==XUI_OK)status=__xuiDocumentBatchAllocate(batch,sizeof(*row->group.pActions),(void**)&row->group.pActions);
        size_t strings=(item->Before.Size?item->Before.Size+1:0)+(item->After.Size?item->After.Size+1:0);
        if(status==XUI_OK && strings>batch->limit-batch->used)status=XUI_ERROR_OUT_OF_MEMORY;
        if(status!=XUI_OK)break;
        batch->used+=strings;row->group.iActionCount=row->group.iActionCapacity=1;
        status=__xuiCodeDocumentActionInit(d,row->group.pActions,0,before,row->beforeLength,after,row->afterLength);
        if(status!=XUI_OK)break;
        memcpy(row->buffer,after,item->After.Size);memcpy(row->snapshot,after,item->After.Size);
        int line=0,start=0;
        for(int j=0;j<row->afterLength;++j)if(after[j]=='\n'){
            row->lines[line++]=(xui_code_line_t){start,j+1,j};start=j+1;
        }
        row->lines[line]=(xui_code_line_t){start,row->afterLength,row->afterLength};
    }
    status=__xuiDocumentBatchFinishCalls(batch,status);batch->busy=false;
    for(size_t i=0;status==XUI_OK && i<count;++i)if(!batch->rows[i].document)status=XUI_ERROR_INVALID_STATE;
    if(status!=XUI_OK){__xuiDocumentBatchFree(batch);return status;}
    *output=batch;return XUI_OK;
}
XUI_API int xuiCodeDocumentBatchCommit(xui_code_document_batch batch,bool* committed)
{
    if(!committed)return XUI_ERROR_INVALID_ARGUMENT;
    *committed=false;
    if(!batch)return XUI_ERROR_INVALID_ARGUMENT;
    if(batch->busy || batch->thread!=xrtThreadCurrentId())return XUI_ERROR_BUSY;
    if(batch->attempted)return XUI_ERROR_INVALID_STATE;
    batch->busy=true;batch->attempted=true;int status=XUI_OK;
    for(size_t i=0;status==XUI_OK && i<batch->count;++i){
        xui_document_batch_row_t* row=batch->rows+i;
        status=__xuiCodeDocumentCallBegin(row->document,&row->call);
        if(status==XUI_OK && !__xuiDocumentBatchCurrent(row))status=XUI_ERROR_INVALID_STATE;
    }
    for(size_t i=0;status==XUI_OK && i<batch->count;++i){
        xui_document_batch_row_t* row=batch->rows+i;
        if(row->changed)status=__xuiCodeDocumentPrepareEdit(row->document,0,row->beforeLength,
            row->group.pActions->sInserted?row->group.pActions->sInserted:"",row->afterLength,true);
    }
    /* Every callback has finished before the final group fence. No allocation,
     * fallible operation or user callback occurs in the publication loop. */
    for(size_t i=0;status==XUI_OK && i<batch->count;++i)
        if(!__xuiDocumentBatchCurrent(batch->rows+i))status=XUI_ERROR_INVALID_STATE;
    if(status==XUI_OK){
        for(size_t i=0;i<batch->count;++i){
            xui_document_batch_row_t* row=batch->rows+i;xui_code_document d=row->document;
            if(!row->changed)continue;
            if(row->undoCount)memcpy(row->undo,d->pUndo,(size_t)row->undoCount*sizeof(*row->undo));
            row->group.iBeforeState=d->iState;row->group.iAfterState=d->iNextState+1;
            row->undo[row->undoCount]=row->group;memset(&row->group,0,sizeof(row->group));
            xrtFree(d->pUndo);d->pUndo=row->undo;row->undo=NULL;d->iUndoCount=row->undoCount+1;d->iUndoCapacity=d->iUndoCount;
            __xuiCodeDocumentClearRedo(d);
            xrtFree(d->sBuffer);xrtFree(d->sSnapshot);xrtFree(d->pLines);
            d->sBuffer=row->buffer;row->buffer=NULL;d->iLength=row->afterLength;d->iBufferCapacity=row->afterLength+4096;
            d->iGapStart=row->afterLength;d->iGapEnd=d->iBufferCapacity;
            d->sSnapshot=row->snapshot;row->snapshot=NULL;d->iSnapshotCapacity=row->afterLength+1;d->bSnapshotValid=1;
            d->pLines=row->lines;row->lines=NULL;d->iLineCount=d->iLineCapacity=row->afterLines;
            d->iPendingLineShiftStart=d->iPendingLineShiftDelta=0;
            ++d->iVersion;++d->iChangeVersion;d->iState=++d->iNextState;
            __xuiCodeDocumentUpdateDirty(d);__xuiCodeDocumentSetLastEditRange(d,0,d->iLength);
        }
        *committed=true;
        for(size_t i=0;i<batch->count;++i){
            xui_document_batch_row_t* row=batch->rows+i;if(!row->changed || !__xuiDocumentBatchLive(row))continue;
            __xuiCodeDocumentEdited(row->document,0,row->beforeLength,row->afterLength,0,row->beforeLines-1,row->afterLines-1,false);
            if(__xuiDocumentBatchLive(row))__xuiCodeDocumentNotifyChanged(row->document);
        }
    }
    status=__xuiDocumentBatchFinishCalls(batch,status);batch->busy=false;return status;
}
XUI_API int xuiCodeDocumentBatchDestroy(xui_code_document_batch batch)
{
    if(!batch)return XUI_OK;
    if(batch->busy || batch->thread!=xrtThreadCurrentId())return XUI_ERROR_BUSY;
    __xuiDocumentBatchFree(batch);return XUI_OK;
}
