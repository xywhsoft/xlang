/* Shared production signature component; included in the owning TU. */
#define XLS_SIGNATURE_BYTES (4u*1024u*1024u)
#define XLS_SIGNATURE_ITEMS 256u
void xlsSignatureDataDestroy(xls_signature_data* data)
{
    if(!data)return;
    for(size_t i=0;i<data->records.Count;++i){xls_signature_record* r=xrtArrayGet(&data->records,i);xrtFree(r->label);xrtFree(r->parameters);}
    xrtArrayUnit(&data->records);xlsSyntaxRelease(data->syntax);xlsCallContextRelease(data->index);xrtFree(data->path);xrtFree(data);
}
void xlsSignatureDataResetCaptured(xls_signature_data* data)
{
    if(!data)return;
    for(size_t i=0;i<data->records.Count;++i){xls_signature_record* r=xrtArrayGet(&data->records,i);xrtFree(r->label);xrtFree(r->parameters);}
    data->records.Count=0;data->active=SIZE_MAX;data->error=NULL;data->projected=0;
    data->bytes=sizeof(*data)+data->records.Capacity*sizeof(xls_signature_record);
    xlsSyntaxRelease(data->syntax);data->syntax=NULL;data->parseWork.cacheBytes=0;
}
int xlsSignatureAppend(xls_signature_scope* scope,xls_signature_record record)
{
    xls_signature_data* data=scope ? scope->data : NULL;size_t bytes=record.labelCapacity;
    int okay=data && !data->error && record.label && record.count<=128 && record.count<=SIZE_MAX/sizeof(*record.parameters) &&
        record.count*sizeof(*record.parameters)<=XLS_SIGNATURE_BYTES && bytes<=XLS_SIGNATURE_BYTES-record.count*sizeof(*record.parameters);
    if(okay){bytes+=record.count*sizeof(*record.parameters);okay=data->records.Count<XLS_SIGNATURE_ITEMS && data->bytes<=XLS_SIGNATURE_BYTES && bytes<=XLS_SIGNATURE_BYTES-data->bytes && xrtArrayPush(&data->records,&record);}
    if(!okay){xrtFree(record.label);xrtFree(record.parameters);if(data)data->error="signature information exceeds allocation or capacity limit";return 0;}
    size_t at=data->records.Count-1;data->bytes+=bytes;
    if(data->active==SIZE_MAX)data->active=at;
    else{
        const xls_signature_record* old=xrtArrayConstGet(&data->records,data->active);
        if((record.ranked && (!old->ranked || record.rank<old->rank)) ||
           (!old->ranked && !record.ranked && old->active==SIZE_MAX && record.active!=SIZE_MAX))data->active=at;
    }
    return 1;
}
