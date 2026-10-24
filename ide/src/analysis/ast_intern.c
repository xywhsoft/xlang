#include "analysis/ast_intern.h"
#include <xrt.h>
#include <string.h>
struct xls_ast_intern {XL6_AstNode* forest;xls_ast_intern_work work;};
typedef struct intern_key {
    xstrbuf data;size_t limit;xmap paths;size_t nextPath,lastPath;
    const char* last; xls_ast_intern_work* work;
} intern_key;
static int keyData(intern_key* k,const void* data,size_t size)
{return size<=k->limit-k->data.Size && xrtStrBufAppend(&k->data,(xstrview){data,size});}
static size_t numberBytes(unsigned char* bytes,uint64_t number)
{
    size_t size=0;
    do{unsigned char byte=(unsigned char)(number&127u);number>>=7;bytes[size++]=(unsigned char)(byte|(number ? 128u : 0u));}while(number);
    return size;
}
static int keyText(intern_key* k,const char* text)
{
    size_t size=text ? strlen(text) : 0;unsigned char prefix[11]={text!=NULL};
    size_t encoded=1+numberBytes(prefix+1,size);
    return keyData(k,prefix,encoded) && (!size || keyData(k,text,size));
}
static int keyPath(intern_key* k,const char* path)
{
    size_t id=0;
    if(path==k->last)id=k->lastPath;
    else if(path){
        size_t length=strlen(path);xbytesview name={(const unsigned char*)path,length};size_t* old=xrtMapGet(&k->paths,name);
        if(old)id=*old;
        else{
            if(length>k->limit-k->work->keyBytes || sizeof(id)>k->limit-k->work->keyBytes-length)return 0;
            id=++k->nextPath;if(!xrtMapSet(&k->paths,name,&id))return 0;
            k->work->keyBytes+=length+sizeof(id);
        }
        k->last=path;k->lastPath=id;
    }
    unsigned char encoded[10];size_t size=numberBytes(encoded,id);return keyData(k,encoded,size);
}
static int bareLeaf(const XL6_AstNode* n)
{
    if(n->doc || n->literalData || n->inferredType || n->semanticDeclaredType || n->childCount)return 0;
    /* A representation case,not a list of language types. Kind/text/source
     * remain exact below;every other logical scalar must be zero. Ignore
     * allocation pointers/capacity and padding,as the general key does. */
    uint64_t fields=n->flags | n->literalSize | n->literalScalar | n->literalDecoded |
        n->semanticTypeId | n->semanticTypeAbiId | n->semanticSymbolId | n->semanticDeclaredSymbolId |
        n->semanticTargetSymbolId | n->semanticCompoundOperatorSymbolId | n->semanticCallKind | n->semanticLvalueKind |
        n->semanticOwnership | n->semanticExitKind | n->semanticScopeDepth | n->semanticTargetScopeDepth | n->semanticRegionId | n->semanticTargetRegionId |
        n->line | n->column | n->range.known | n->range.startOffset | n->range.endOffset | n->range.startLine | n->range.startColumn | n->range.endLine | n->range.endColumn |
        n->tokenRange.known | n->tokenRange.startOffset | n->tokenRange.endOffset | n->tokenRange.startLine | n->tokenRange.startColumn | n->tokenRange.endLine | n->tokenRange.endColumn;
    return fields==0;
}
static int keyNode(intern_key* k,const XL6_AstNode* n,size_t depth)
{
    int bare=n && bareLeaf(n);unsigned char present=n ? (bare ? 2 : 1) : 0;
    if(depth>512 || !keyData(k,&present,sizeof(present)))return 0;
    if(!n)return 1;
    if(bare){
        uint64_t fields[]={n->kind,n->source!=NULL,n->source ? n->source->completionQuery : 0,
            n->source ? n->source->completionStart : 0,n->source ? n->source->completionEnd : 0};
        unsigned char encoded[sizeof(fields)/sizeof(*fields)*10];size_t size=0;
        for(size_t i=0;i<sizeof(fields)/sizeof(*fields);++i)size+=numberBytes(encoded+size,fields[i]);
        return keyData(k,encoded,size) && keyText(k,n->text) && keyPath(k,n->source ? n->source->path : NULL);
    }
    uint64_t fields[]={n->kind,n->flags,n->literalSize,n->literalScalar,n->literalDecoded,
        n->semanticTypeId,n->semanticTypeAbiId,n->semanticSymbolId,n->semanticDeclaredSymbolId,
        n->semanticTargetSymbolId,n->semanticCompoundOperatorSymbolId,n->semanticCallKind,n->semanticLvalueKind,
        n->semanticOwnership,n->semanticExitKind,n->semanticScopeDepth,n->semanticTargetScopeDepth,n->semanticRegionId,n->semanticTargetRegionId,
        n->line,n->column,n->range.known,n->range.startOffset,n->range.endOffset,n->range.startLine,n->range.startColumn,n->range.endLine,n->range.endColumn,
        n->tokenRange.known,n->tokenRange.startOffset,n->tokenRange.endOffset,n->tokenRange.startLine,n->tokenRange.startColumn,n->tokenRange.endLine,n->tokenRange.endColumn,
        n->childCount,n->source!=NULL,n->source ? n->source->completionQuery : 0,n->source ? n->source->completionStart : 0,n->source ? n->source->completionEnd : 0};
    _Static_assert(sizeof(fields)/sizeof(*fields)<=64,"intern scalar mask overflow");
    uint64_t mask=0;for(size_t i=0;i<sizeof(fields)/sizeof(*fields);++i)if(fields[i])mask|=UINT64_C(1)<<i;
    /* Batch the compact scalar encoding in a bounded stack block. Exact XRT
     * map keys avoid hash-only equality and contain no C padding/pointers. */
    unsigned char encoded[(1+sizeof(fields)/sizeof(*fields))*10];size_t size=numberBytes(encoded,mask);
    for(size_t i=0;i<sizeof(fields)/sizeof(*fields);++i)if(fields[i])size+=numberBytes(encoded+size,fields[i]);
    present=n->literalData!=NULL;
    if(!keyData(k,encoded,size) || !keyText(k,n->text) || !keyText(k,n->doc) || !keyPath(k,n->source ? n->source->path : NULL) ||
       !keyData(k,&present,sizeof(present)) || (n->literalData && !keyData(k,n->literalData,n->literalSize)) ||
       !keyNode(k,n->inferredType,depth+1) || !keyNode(k,n->semanticDeclaredType,depth+1))return 0;
    if(n->childCount && !n->children)return 0;
    for(size_t i=0;i<n->childCount;++i)if(!keyNode(k,n->children[i],depth+1))return 0;
    return 1;
}
static int gather(const XL6_AstNode* const* values,size_t count,xarray* roots,xarray* references,
    size_t budget,xls_ast_intern_work* work)
{
    xmap index={0};intern_key key={.limit=budget,.work=work};int okay=xrtMapInit(&index,sizeof(size_t)) && xrtMapInit(&key.paths,sizeof(size_t));
    for(size_t i=0;okay && i<count;++i){
        xrtStrBufClear(&key.data);
        okay=keyNode(&key,values[i],0);if(key.data.Capacity>work->peakKeyBytes)work->peakKeyBytes=key.data.Capacity;
        if(!okay)break;
        xbytesview name={(const unsigned char*)key.data.Data,key.data.Size};size_t* old=xrtMapGet(&index,name);
        size_t at=old ? *old : roots->Count;
        if(!old){
            if(key.data.Size>budget-work->keyBytes || sizeof(at)>budget-work->keyBytes-key.data.Size){okay=0;break;}
            okay=xrtArrayPush(roots,&values[i]) && xrtMapSet(&index,name,&at);
            if(okay)work->keyBytes+=key.data.Size+sizeof(at);
        }
        if(okay)okay=xrtArrayPush(references,&at);
    }
    xrtMapUnit(&index);xrtMapUnit(&key.paths);xrtStrBufFree(&key.data);return okay;
}
void xlsAstInternDestroy(xls_ast_intern* owner)
{if(owner){xl6AstFree(owner->forest);xrtFree(owner);}}
xls_ast_intern_work xlsAstInternWork(const xls_ast_intern* owner)
{return owner ? owner->work : (xls_ast_intern_work){0};}
xls_ast_intern* xlsAstInternCapture(const XL6_AstNode** values,size_t count,size_t bytes,size_t keyBytes,xls_ast_intern_work* work)
{
    if(work)*work=(xls_ast_intern_work){0};
    if(!values || !count || !bytes || keyBytes>8u*1024u*1024u || count>bytes/sizeof(*values))return NULL;
    for(size_t i=0;i<count;++i)if(!values[i])return NULL;
    xarray roots={0},references={0};xls_ast_intern_work measured={.references=count};xls_ast_intern* owner=NULL;
    if(!xrtArrayInit(&roots,sizeof(XL6_AstNode*)) || !xrtArrayInit(&references,sizeof(size_t)))goto done;
    if(!keyBytes || !gather(values,count,&roots,&references,keyBytes,&measured)){
        roots.Count=references.Count=0;++measured.fallbacks;
        for(size_t i=0;i<count;++i)if(!xrtArrayPush(&roots,&values[i]) || !xrtArrayPush(&references,&i))goto done;
    }
    owner=xrtCalloc(1,sizeof(*owner));if(!owner || bytes<=sizeof(*owner))goto failed;
    XL6_AstNode forest={.kind=XL6_AST_MODULE,.children=(XL6_AstNode**)roots.Data,.childCount=roots.Count};
    size_t owned=0;owner->forest=xlsAstClone(&forest,&(xls_ast_clone_options){.bytes=bytes-sizeof(*owner)},&owned);
    if(!owner->forest)goto failed;
    measured.treeBytes=owned+sizeof(*owner);measured.uniqueRoots=roots.Count;measured.hits=count-roots.Count;
    owner->work=measured;
    for(size_t i=0;i<count;++i)values[i]=owner->forest->children[*(const size_t*)xrtArrayConstGet(&references,i)];
    if(work)*work=measured;
    goto done;
failed:xlsAstInternDestroy(owner);owner=NULL;
done:xrtArrayUnit(&roots);xrtArrayUnit(&references);return owner;
}
