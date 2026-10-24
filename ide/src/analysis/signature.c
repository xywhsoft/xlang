#include "analysis/signature_internal.h"
#include "analysis/frontend_internal.h"
#include <stdio.h>
#include <string.h>
#define XLS_SIGNATURE_BYTES (4u*1024u*1024u)
#define XLS_SIGNATURE_ITEMS 256u
void xlsSignatureDataDestroy(xls_signature_data* data)
{
    if(!data)return;
    for(size_t i=0;i<data->records.Count;++i){xls_signature_record* r=xrtArrayGet(&data->records,i);xrtFree(r->label);xrtFree(r->parameters);}
    xrtArrayUnit(&data->records);xlsCallContextRelease(data->index);xrtFree(data->path);xrtFree(data);
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
static xls_signature_help* createSignature(xls_context* context,const char* entry,const char* path,size_t offset,
    const xls_semantic_input* inputs,size_t count,xls_call_context* index,xls_entry_cache* cache,
    xls_signature_help* previous,int memoEnabled,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    if(error && capacity)*error=0;
    const xls_semantic_input* target=NULL;
    if(!path || !xrtPathIsAbs(path) || !entry || !xrtPathIsAbs(entry) || !inputs || !count || count>64)goto invalid;
    for(size_t i=0;i<count;++i)if(inputs[i].path && xl6CompletionPathEqual(path,inputs[i].path)){if(target)goto invalid;target=&inputs[i];}
    if(!target || !target->text || target->size>8u*1024u*1024u)goto invalid;
    xls_signature_data* data=xrtCalloc(1,sizeof(*data));if(!data)goto memory;
    data->active=SIZE_MAX;data->offset=offset;data->path=xrtPathAbs(path);
    if(!data->path || !xrtArrayInit(&data->records,sizeof(xls_signature_record)) || !xrtArrayReserve(&data->records,XLS_SIGNATURE_ITEMS)){xlsSignatureDataDestroy(data);goto memory;}
    const xls_text* prior=xlsCallContextText(index);
    if(prior && xlsTextSize(prior)==target->size && !memcmp(xlsTextData(prior),target->text,target->size))data->index=xlsCallContextRetain(index);
    else{xls_text* text=xlsTextCreate(target->text,target->size,8u*1024u*1024u,NULL);if(!text){xlsSignatureDataDestroy(data);goto memory;}data->index=xlsCallContextCreate(text,64u*1024u*1024u,cancelled,user,error,capacity);xlsTextRelease(text);}
    if(!data->index){xlsSignatureDataDestroy(data);return NULL;}
    const xls_text* held=xlsCallContextText(data->index);size_t bom=xlsTextSize(held)>=3 && !memcmp(xlsTextData(held),"\xef\xbb\xbf",3) ? 3 : 0;
    data->source=xlsTextData(held)+bom;data->size=xlsTextSize(held)-bom;
    data->bytes=sizeof(*data)+data->records.Capacity*sizeof(xls_signature_record);
    int found=xlsCallContextLookupRecovering(data->index,offset,&data->site);
    if(found<0){xlsSignatureDataDestroy(data);goto invalid;}
    data->hasSite=found>0 && !data->site.inComment && !data->site.embedded;
    if(found && data->site.embedded)data->error="signature query needs embedded expression recovery";
    xls_semantic_completion* semantic=xlsSemanticSignatureCreateInContext(context,entry,path,offset,inputs,count,data,cache,xlsSignatureSemantic(previous),memoEnabled,cancelled,user,error,capacity);
    if(!semantic)return NULL;
    xls_signature_help* result=xrtCalloc(1,sizeof(*result));
    if(!result){xlsSemanticCompletionDestroy(semantic);goto memory;}
    xrtAtomic32Init(&result->references,1);result->semantic=semantic;result->offset=offset;result->site=data->site;return result;
invalid:if(error && capacity)snprintf(error,capacity,"invalid signature input, target or UTF-8 cursor");return NULL;
memory:if(error && capacity)snprintf(error,capacity,"cannot allocate signature query");return NULL;
}
xls_signature_help* xlsSignatureCreateInContext(xls_context* context,const char* entry,const char* path,size_t offset,
    const xls_semantic_input* inputs,size_t count,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{return createSignature(context,entry,path,offset,inputs,count,NULL,NULL,NULL,0,cancelled,user,error,capacity);}
xls_signature_help* xlsSignatureCreateRetainedInContext(xls_context* context,const char* entry,const char* path,size_t offset,
    const xls_semantic_input* inputs,size_t count,xls_call_context* index,xls_entry_cache* cache,xls_signature_help* previous,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{return createSignature(context,entry,path,offset,inputs,count,index,cache,previous,1,cancelled,user,error,capacity);}
xls_semantic_completion* xlsSignatureSemantic(const xls_signature_help* result){return result ? result->semantic : NULL;}
xls_signature_data* xlsSignatureData(const xls_signature_help* result){return xlsSemanticSignatureData(xlsSignatureSemantic(result));}
xls_signature_help* xlsSignatureRetain(xls_signature_help* result){if(result)xrtAtomic32FetchAdd(&result->references,1,XMEMORY_RELAXED);return result;}
void xlsSignatureRelease(xls_signature_help* result){if(result && xrtAtomic32FetchSub(&result->references,1,XMEMORY_ACQ_REL)==1){xlsSemanticCompletionDestroy(result->semantic);xrtFree(result);}}
xls_signature_work xlsSignatureWork(const xls_signature_help* result){xls_sem_work work=xlsSemanticCompletionSemWork(xlsSignatureSemantic(result));return (xls_signature_work){work.checked,work.reused};}
int xlsSignatureValidate(xls_signature_help* result)
{
    const xls_signature_data* data=xlsSignatureData(result);
    return data && (!data->hasSite || xlsSemanticCompletionValidate(xlsSignatureSemantic(result)));
}
xls_signature_info xlsSignatureInfo(const xls_signature_help* result)
{
    const xls_signature_data* data=xlsSignatureData(result);if(!data)return (xls_signature_info){.activeSignature=SIZE_MAX};
    xls_completion_info info=xlsSemanticCompletionInfo(xlsSignatureSemantic(result));int available=data->hasSite && info.available && !data->error && data->records.Count;
    return (xls_signature_info){data->path,data->error ? data->error : info.error,result->offset,available ? data->records.Count : 0,available ? data->active : SIZE_MAX,result->site.argument,available,data->projected};
}
int xlsSignatureItem(const xls_signature_help* result,size_t index,xls_signature_item* item)
{
    if(item)*item=(xls_signature_item){0};
    if(!item || index>=xlsSignatureInfo(result).count)return 0;
    const xls_signature_record* r=xrtArrayConstGet(&xlsSignatureData(result)->records,index);
    *item=(xls_signature_item){r->label,r->documentation,r->count,r->active,r->symbol};return 1;
}
int xlsSignatureParameter(const xls_signature_help* result,size_t index,size_t parameter,xls_signature_parameter* value)
{
    if(value)*value=(xls_signature_parameter){0};
    if(!value || index>=xlsSignatureInfo(result).count)return 0;
    const xls_signature_record* r=xrtArrayConstGet(&xlsSignatureData(result)->records,index);if(parameter>=r->count)return 0;*value=r->parameters[parameter];return 1;
}
int xlsSignatureInputs(const xls_signature_help* r,xls_inputs* inputs){return xlsSemanticCompletionInputs(xlsSignatureSemantic(r),inputs);}
int xlsSignatureSource(const xls_signature_help* r,size_t index,xls_input_source* source){return xlsSemanticCompletionSource(xlsSignatureSemantic(r),index,source);}
int xlsSignatureProbe(const xls_signature_help* r,size_t index,xls_input_probe* probe){return xlsSemanticCompletionProbe(xlsSignatureSemantic(r),index,probe);}

char* xlsSignatureInputJson(xls_signature_help* result,size_t* size){return xlsSemanticSignatureInputJson(xlsSignatureSemantic(result),size);}
/* With identical inputs the projection skips the entire current argument.
 * A closed, proven call and unchanged argument/name boundaries therefore share
 * the same native semantic model, even across literal/whitespace boundaries. */
xls_signature_help* xlsSignatureReposition(xls_signature_help* previous,size_t offset)
{
    if(!previous || !xlsSignatureInfo(previous).available)return NULL;
    const xls_signature_data* data=xlsSignatureData(previous);xls_call_site next={0};
    if(!data || xlsCallContextLookup(data->index,offset,&next)!=1)return NULL;
    const xls_call_site* old=&previous->site;
    if(!old->closed || !next.closed || !old->argumentKnown || !next.argumentKnown || old->inComment || next.inComment ||
       old->embedded || next.embedded || old->sourceError || next.sourceError ||
       old->calleeStart!=next.calleeStart || old->calleeEnd!=next.calleeEnd || old->nameStart!=next.nameStart || old->nameEnd!=next.nameEnd ||
       old->argumentsStart!=next.argumentsStart || old->argumentsEnd!=next.argumentsEnd || old->argument!=next.argument ||
       old->argumentStart!=next.argumentStart || old->namedStart!=next.namedStart || old->namedEnd!=next.namedEnd)return NULL;
    if(!xlsSignatureValidate(previous))return NULL;
    xls_signature_help* result=xrtCalloc(1,sizeof(*result));if(!result)return NULL;
    xrtAtomic32Init(&result->references,1);result->semantic=xlsSemanticCompletionRetain(previous->semantic);result->offset=offset;result->site=next;return result;
}
