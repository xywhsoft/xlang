/* Additive XUI document primitive. Included in the Document implementation,
 * after its existing helpers; the IDE never inspects private document storage. */
typedef struct xui_document_batch_row_t {
    xui_code_document document;
    xui_lifetime_link_t lifetime;
    xui_code_document_call_t call;
    uint32_t version,changeVersion;
    uint64_t state,nextState;
    int beforeLength,beforeLines,afterLength,afterLines,undoCount,changed;
    char *buffer,*snapshot,*proof;
    xui_code_line_t* lines;
    xui_code_edit_group_t *undo,group;
} xui_document_batch_row_t;
struct xui_code_document_batch_t {
    uint64_t thread,serial;
    size_t count,used,limit;
    bool busy,attempted,ready;
    xui_document_batch_row_t* rows;
};
static void __xuiDocumentBatchGone(void* user)
{ ((xui_document_batch_row_t*)user)->document=NULL; }
static int __xuiDocumentBatchAllocate(size_t* used,size_t limit,size_t bytes,void** out,bool clear)
{
    *out=NULL;
    if(bytes>limit-*used)return XUI_ERROR_OUT_OF_MEMORY;
    void* memory=xrtMalloc(bytes);
    if(!memory)return XUI_ERROR_OUT_OF_MEMORY;
    if(clear)memset(memory,0,bytes);
    *used+=bytes;*out=memory;return XUI_OK;
}
static void __xuiDocumentBatchFreePayloadRow(xui_document_batch_row_t* row)
{
    xrtFree(row->proof);xrtFree(row->buffer);xrtFree(row->snapshot);xrtFree(row->lines);
    xrtFree(row->undo);__xuiCodeDocumentGroupClear(&row->group);
}
static void __xuiDocumentBatchFree(xui_code_document_batch batch)
{
    if(!batch)return;
    for(size_t i=0;i<batch->count;++i){
        xui_document_batch_row_t* row=batch->rows+i;
        xuiLifetimeUnwatch(&row->lifetime);
        __xuiDocumentBatchFreePayloadRow(row);
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
/* XUI_DOCUMENT_BATCH_WORKER */
XUI_API int xuiCodeDocumentBatchPrepare(const xui_code_document_batch_item_t* items,
    size_t count,size_t budget,xui_code_document_batch* output)
{
    if(!output)return XUI_ERROR_INVALID_ARGUMENT;
    *output=NULL;
    xui_code_document_batch batch=NULL;
    int status=xuiCodeDocumentBatchCapture(items,count,budget,&batch);
    if(status!=XUI_OK)return status;
    xui_code_document_batch_layout_t layout;
    xui_code_document_batch_source_t sources[512];
    xui_code_document_batch_text_t texts[512];
    status=xuiCodeDocumentBatchDescribe(batch,&layout,sources,512);
    if(status==XUI_OK)for(size_t i=0;i<count;++i)
        texts[i]=(xui_code_document_batch_text_t){sizeof(texts[i]),items[i].Before,items[i].After};
    xui_code_document_batch_payload payload=NULL;
    if(status==XUI_OK)status=xuiCodeDocumentBatchStage(&layout,sources,texts,budget,NULL,NULL,&payload);
    bool taken=false;
    if(status==XUI_OK)status=xuiCodeDocumentBatchAccept(batch,payload,&taken);
    if(!taken)xuiCodeDocumentBatchPayloadDestroy(payload);
    if(status!=XUI_OK){__xuiDocumentBatchFree(batch);return status;}
    *output=batch;return XUI_OK;
}
XUI_API int xuiCodeDocumentBatchCommitChecked(xui_code_document_batch batch,
    const xui_code_document_batch_callbacks_t* callbacks,bool* committed)
{
    if(!committed)return XUI_ERROR_INVALID_ARGUMENT;
    *committed=false;
    if(!batch)return XUI_ERROR_INVALID_ARGUMENT;
    if(batch->busy || batch->thread!=xrtThreadCurrentId())return XUI_ERROR_BUSY;
    if(batch->attempted || !batch->ready)return XUI_ERROR_INVALID_STATE;
    xui_code_document_batch_callbacks_t hooks={0};
    if(callbacks){if(callbacks->iSize!=sizeof(*callbacks))return XUI_ERROR_INVALID_ARGUMENT;hooks=*callbacks;}
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
    if(status==XUI_OK && hooks.beforePublish)status=hooks.beforePublish(hooks.pUser);
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
        if(hooks.published)hooks.published(hooks.pUser);
        for(size_t i=0;i<batch->count;++i){
            xui_document_batch_row_t* row=batch->rows+i;if(!row->changed || !__xuiDocumentBatchLive(row))continue;
            __xuiCodeDocumentEdited(row->document,0,row->beforeLength,row->afterLength,0,row->beforeLines-1,row->afterLines-1,false);
            if(__xuiDocumentBatchLive(row))__xuiCodeDocumentNotifyChanged(row->document);
        }
    }
    status=__xuiDocumentBatchFinishCalls(batch,status);batch->busy=false;return status;
}
XUI_API int xuiCodeDocumentBatchCommit(xui_code_document_batch batch,bool* committed)
{return xuiCodeDocumentBatchCommitChecked(batch,NULL,committed);}
XUI_API int xuiCodeDocumentBatchDestroy(xui_code_document_batch batch)
{
    if(!batch)return XUI_OK;
    if(batch->busy || batch->thread!=xrtThreadCurrentId())return XUI_ERROR_BUSY;
    __xuiDocumentBatchFree(batch);return XUI_OK;
}
