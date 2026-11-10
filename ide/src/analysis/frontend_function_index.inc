#include "analysis/index_segments.h"
/* Original collector segments. AST/selection/type values and canonical text
 * keep their original producer. Only source metadata and range admission are
 * frozen here;no parser,semantic rule or independent canonicalizer is added. */
static void xlsIndexStorageRelease(xls_index_storage* storage)
{
    if(storage && xrtAtomic32FetchSub(&storage->references,1,XMEMORY_ACQ_REL)==1){
        xrtFree(storage->sources);xrtFree(storage);
    }
}
xls_function_index_asset* xlsFunctionIndexAssetRetain(xls_function_index_asset* asset)
{if(asset)xrtAtomic32FetchAdd(&asset->references,1,XMEMORY_RELAXED);return asset;}
const void* xlsFunctionIndexAssetMetadataOwner(const xls_function_index_asset* asset)
{return asset ? asset->storage : NULL;}
void xlsFunctionIndexAssetRelease(xls_function_index_asset* asset)
{
    if(!asset || xrtAtomic32FetchSub(&asset->references,1,XMEMORY_ACQ_REL)!=1)return;
    xls_index_storage* storage=asset->storage;
    if(asset->canonical)xlsIndexCanonicalRelease(asset->canonical);
    else for(size_t i=0;i<asset->count;++i)xrtFree(asset->facts[i].canonical);
    xrtFree(asset->facts);asset->facts=NULL;
    xlsTypedFunctionRelease(asset->function);asset->function=NULL;
    xlsIndexStorageRelease(storage);
}
static void xlsIndexStorageParentRelease(xls_index_storage* storage)
{
    /* Directory belongs solely to the complete index. Function segments have
     * their own buffers;one retained function never pins other fact buffers. */
    xrtFree(storage->rows);storage->rows=NULL;
    for(size_t i=0;i<storage->slots;++i)if(storage->chunks[i].storage)xlsFunctionIndexAssetRelease(&storage->chunks[i]);
    xlsIndexStorageRelease(storage);
}
int xlsFrontendIndexBindFunctions(xls_semantic_index* index,const xls_typed_module* module)
{
    if(!index || !module)return 0;
    XL6_IndexBuild* b=&index->build;xls_index_storage* storage=b->storage;const XL6_AstNode* root=xlsTypedModuleRoot(module);
    if(!storage || !root || b->root!=root || root->childCount+1!=storage->slots || !b->report || !b->report->complete)return 0;
    if(storage->sources)return 1;
    xls_typed_function* selected=NULL;
    for(size_t i=0;i<root->childCount && !selected;++i)selected=xlsTypedModuleFunction(module,i);
    if(!selected)return 0;
    xlsTypedFunctionRelease(selected);
    size_t count=b->report->count,bytes=0;
    if(count>SIZE_MAX/sizeof(xls_function_index_source))return 0;
    bytes=count*sizeof(xls_function_index_source);
    for(size_t i=0;i<count;++i){const char* path=b->report->items[i].path;
        if(!path)return 0;
        size_t length=strlen(path)+1;
        if(length>SIZE_MAX-bytes)return 0;
        bytes+=length;
    }
    if(!bytes || bytes>XL6_INDEX_BYTES-b->budget)return 0;
    xls_function_index_source* sources=xrtCalloc(1,bytes);if(!sources)return 0;
    char* text=(char*)(sources+count);
    for(size_t i=0;i<count;++i){const XL6_SourceReportItem* original=&b->report->items[i];xls_function_index_source* target=&sources[i];
        size_t length=strlen(original->path)+1;memcpy(text,original->path,length);target->path=text;text+=length;
        target->size=original->size;target->rawSize=original->rawSize;target->kind=original->kind;
        memcpy(target->textSha256,original->textSha256,32);memcpy(target->rawSha256,original->rawSha256,32);
    }
    storage->sources=sources;storage->sourceCount=count;storage->sourceBytes=bytes;(void)xl6IndexBudget(b,bytes);
    for(size_t slot=0;slot<root->childCount;++slot){
        xls_function_index_asset* asset=&storage->chunks[slot+1];
        if(!asset->storage || !asset->count || asset->facts[0].node!=root->children[slot])continue;
        xls_typed_function* function=xlsTypedModuleFunction(module,slot);if(!function)continue;
        if(xlsTypedFunctionRoot(function)!=asset->facts[0].node){xlsTypedFunctionRelease(function);continue;}
        size_t canonical=0;asset->pointFirst=SIZE_MAX;asset->pointEnd=0;asset->pointSource=0;
        asset->pointReusable=asset->facts[0].parent==1;

        for(size_t i=0;i<asset->count;++i){XL6_IndexFact* f=&asset->facts[i];const XL6_AstNode* n=f->node;
            f->validRanges=(xlsFrontendRange(b->report,f->source,&n->range).known ? 1u : 0u) |
                (xlsFrontendRange(b->report,f->source,&n->tokenRange).known ? 2u : 0u) |
                (xlsFrontendRange(b->report,f->selectionSource,f->selection ? &f->selection->tokenRange : NULL).known ? 4u : 0u);
            const XL6_AstRange* ranges[3]={&n->range,&n->tokenRange,f->selection ? &f->selection->tokenRange : NULL};
            unsigned sources[3]={f->source,f->source,f->selectionSource};
            for(unsigned at=0;at<3;++at)if(ranges[at] && ranges[at]->known){
                if(!(f->validRanges&(1u<<at)) || !sources[at] || sources[at]>count || !(b->report->items[sources[at]-1].kind&1))asset->pointReusable=0;
                if(asset->pointSource && asset->pointSource!=sources[at])asset->pointReusable=0;
                asset->pointSource=sources[at];
                if(ranges[at]->startOffset<asset->pointFirst)asset->pointFirst=ranges[at]->startOffset;
                if(ranges[at]->endOffset>asset->pointEnd)asset->pointEnd=ranges[at]->endOffset;
            }
            if(f->canonical){size_t length=strlen(f->canonical);canonical+=length+1;if(!length || length>8192)asset->pointReusable=0;}
            if(!!f->canonical!=!!f->type)asset->pointReusable=0;
        }
        asset->function=function;asset->canonicalBytes=canonical;
    }
    return 1;
}
xls_function_index_asset* xlsFrontendIndexFunctionAsset(const xls_semantic_index* index,const xls_typed_function* function)
{
    xls_index_storage* storage=index ? index->build.storage : NULL;size_t slot=xlsTypedFunctionRootSlot(function);
    if(!storage || !storage->sources || slot>=storage->slots-1)return NULL;
    xls_function_index_asset* asset=&storage->chunks[slot+1];
    return asset->function==function ? xlsFunctionIndexAssetRetain(asset) : NULL;
}
xls_function_index_info xlsFunctionIndexAssetInfo(const xls_function_index_asset* asset)
{
    if(!asset)return (xls_function_index_info){0};
    size_t metadata=asset->canonical ? sizeof(*asset->canonical)+asset->canonical->rows.Capacity*sizeof(xls_index_canonical_row) : 0;
    size_t canonical=asset->canonical ? asset->canonical->bytes-metadata : asset->canonicalBytes;
    return (xls_function_index_info){asset->first,asset->count,asset->capacity*sizeof(XL6_IndexFact)+metadata,canonical,
        asset->storage->metadataBytes+asset->storage->sourceBytes};
}
static xls_index_range xlsFunctionIndexRange(XL6_AstRange value)
{return (xls_index_range){value.startOffset,value.endOffset,value.startLine,value.startColumn,value.endLine,value.endColumn,value.known};}
int xlsFunctionIndexAssetFact(const xls_function_index_asset* asset,size_t at,xls_index_fact* out)
{
    if(!asset || !asset->function || !out || at>=asset->count)return 0;
    const XL6_IndexFact* f=&asset->facts[at];const XL6_AstNode* node=f->node;
    xlsFrontendFactValues(f,(unsigned)(asset->first+at+1),out);
    if(f->validRanges&1u)out->range=xlsFunctionIndexRange(node->range);
    if(f->validRanges&2u)out->tokenRange=xlsFunctionIndexRange(node->tokenRange);
    if(f->validRanges&4u)out->selectionRange=xlsFunctionIndexRange(f->selection->tokenRange);
    return 1;
}
int xlsFunctionIndexAssetPointFact(const xls_function_index_asset* asset,size_t at,xls_index_point_fact* out)
{
    if(!out)return 0;
    *out=(xls_index_point_fact){0};if(!xlsFunctionIndexAssetFact(asset,at,&out->fact))return 0;
    out->kind=xlsFrontendPointKind(asset->facts[at].node->kind);return 1;
}
size_t xlsFunctionIndexAssetSourceCount(const xls_function_index_asset* asset)
{return asset ? asset->storage->sourceCount : 0;}
int xlsFunctionIndexAssetSource(const xls_function_index_asset* asset,size_t at,xls_function_index_source* out)
{
    if(out)*out=(xls_function_index_source){0};
    if(!asset || !out || at>=asset->storage->sourceCount)return 0;
    *out=asset->storage->sources[at];return 1;
}

size_t xlsSemanticIndexSegmentCount(const xls_semantic_index* index)
{return index && index->build.storage ? index->build.storage->slots : 0;}
int xlsSemanticIndexSegment(const xls_semantic_index* index,size_t slot,xls_index_segment* out)
{
    if(out)*out=(xls_index_segment){0};
    if(!index || !out || !index->build.storage || slot>=index->build.storage->slots)return 0;
    const xls_function_index_asset* asset=&index->build.storage->chunks[slot];
    if(!asset->storage)return 1;
    *out=(xls_index_segment){asset->first,asset->count,asset->canonicalBytes,asset->pointFirst,asset->pointEnd,
        asset->function,asset->pointSource,asset->function && asset->pointReusable && asset->pointSource && asset->pointFirst!=SIZE_MAX};
    return 1;
}
