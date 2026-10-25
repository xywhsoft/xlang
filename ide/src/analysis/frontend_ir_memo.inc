/* Included immediately before the audited raw AST->IR builder,after its
 * allocation/type helpers. Cache pre-pass values only;all original module
 * transformations and verification still run on independent mutable arenas. */
#include "analysis/frontend_ir_memo.h"
#include "analysis/frontend_sem.h"
#define XLS_IR_MEMO_BYTES (32u*1024u*1024u)
typedef struct xls_ir_dependency {XL6_SymbolId symbol;XL6_IrNodeId binding;XL6_IrTypeId type,storage;} xls_ir_dependency;
typedef struct xls_ir_write {XL6_SymbolId symbol;XL6_IrNodeId binding;} xls_ir_write;
typedef struct xls_ir_unit {
    xatomic32 references;
    xstrbuf key,types,data;
    xarray dependencies,writes;
    size_t startNode,startChild,nodes,children,bytes,textNodes,textBytes;
} xls_ir_unit;
struct xls_ir_memo {xstrbuf context;xarray entries;size_t bytes;};
typedef struct xls_ir_capture {xls_ir_unit* unit;xmap dependencies,writes;int okay;} xls_ir_capture;
typedef struct xls_ir_memo_build {
    struct xls_ir_memo_build* previous;
    const XL6_AstNode* root;XL6_IrModule* module;
    const xls_ir_memo* baseline;xls_ir_memo* memo;
    xls_ir_capture* capture;xls_ir_memo_work work;size_t limit;
} xls_ir_memo_build;
static _Thread_local xls_ir_memo_build* xlsActiveIrMemo;
static XL6_IrNodeId xlsIrBuildAstNodeOriginal(XL6_IrModule*,const XL6_AstNode*,const XL6_AstNode*,const XL6_AstNode*,size_t,XL6_DiagList*,int*);
static int xlsIrMemoData(xstrbuf* b,const void* data,size_t size,size_t limit)
{
    if(b->Size>limit || size>limit-b->Size)return 0;
    if(size){
        size_t end=b->Size+size;
        if(end>b->Capacity && !xrtStrBufReserve(b,end))return 0;
        /* Private inputs are stack records or independent module storage. */
        memcpy(b->Data+b->Size,data,size);b->Size=end;b->Data[end]=0;
    }
    return b->Capacity<=limit;
}
static int xlsIrMemoNumber(xstrbuf* b,uint64_t value,size_t limit)
{
    unsigned char bytes[10];size_t n=0;
    do{unsigned char part=(unsigned char)(value&127u);value>>=7;bytes[n++]=(unsigned char)(part|(value ? 128u : 0u));}while(value);
    return xlsIrMemoData(b,bytes,n,limit);
}
static int xlsIrMemoString(xstrbuf* b,const char* text,size_t limit)
{
    size_t size=text ? strlen(text) : 0;
    return xlsIrMemoNumber(b,text!=NULL,limit) && xlsIrMemoNumber(b,size,limit) && (!size || xlsIrMemoData(b,text,size,limit));
}
static int xlsIrMemoEqual(const xstrbuf* a,const xstrbuf* b)
{return a->Size==b->Size && (!a->Size || !memcmp(a->Data,b->Data,a->Size));}
static int xlsIrMemoContext(const XL6_AstNode* root,xstrbuf* key,size_t limit)
{
    /* Raw type interning reads root nominal declarations and aliases,never
     * function signatures/bodies. Function identities and actual symbol reads
     * are proved separately. In particular,an earlier local declaration may
     * renumber later parameters without changing this type environment. */
    xarray declarations={0};int okay=xrtArrayInit(&declarations,sizeof(XL6_AstNode*));
    for(size_t i=0;okay && i<root->childCount;++i){
        XL6_AstNode* node=root->children[i];
        if(node && (node->kind==XL6_AST_FUNC || node->kind==XL6_AST_ASYNC_FUNC || node->kind==XL6_AST_EXPORT_FUNC))continue;
        okay=declarations.Count<limit/sizeof(node) && xrtArrayPush(&declarations,&node);
    }
    XL6_AstNode filtered=*root;filtered.children=(XL6_AstNode**)declarations.Data;filtered.childCount=filtered.childCapacity=declarations.Count;
    okay=okay && xlsFrontendIrAstKey(key,&filtered,0);xrtArrayUnit(&declarations);return okay;
}
/* Exact state of the original interner's input prefix. No new type identity
 * or equivalence:all scalar fields/strings/arrays are compared byte-exactly,
 * without pointer values,capacity or C padding. The header digest is audited. */
static int xlsIrMemoTypes(const XL6_IrTypePool* pool,xstrbuf* key,size_t limit)
{
    if(!xlsIrMemoNumber(key,pool->count,limit))return 0;
    for(size_t i=0;i<pool->count;++i){
        const XL6_IrType* t=&pool->items[i];const XL6_IrSequenceLayout* q=&t->sequenceLayout;
        uint64_t fields[]={t->kind,t->cAbi,t->collectionAbi,t->collectionCarrierAbi,t->runtimeResourceAbi,t->ownershipAbi,t->nullAbi,
            t->persistenceValueAbi,t->persistenceRestoreScalarAbi,t->persistenceRestoreCarrierAbi,t->persistencePayloadType,t->persistenceWireFormat,
            t->integerTarget,t->nativeProductAbi,t->nativeSlotAbi,q->abi,q->role,q->elementType,q->baseElementType,q->slotAbi,q->codecAbi,
            q->transferAbi,q->admissionAbi,q->nullable,q->defaultAbi,q->childRole,q->childElementType,q->childSlotAbi,q->childTransferAbi,
            t->xvalueAdmissionAbi,t->xvalueAdmissionNullable,t->xvalueAdmissionTypeValueId,t->sourceTypeId,t->abiId,t->typeValueId,t->nominalSymbol,
            t->paramCount,t->returnCount,t->callableResultType,t->callableErasureAbi,t->hash,
            t->params!=NULL,t->parameterModes!=NULL,t->parameterAbis!=NULL,t->returns!=NULL};
        if(!xlsIrMemoData(key,fields,sizeof(fields),limit) || !xlsIrMemoData(key,t->cName,sizeof(t->cName),limit) ||
           !xlsIrMemoData(key,t->errorResultCName,sizeof(t->errorResultCName),limit) ||
           !xlsIrMemoString(key,t->name,limit) || !xlsIrMemoString(key,t->displayName,limit))return 0;
        if((t->paramCount && !t->params) || (t->returnCount && !t->returns))return 0;
        if((t->paramCount && !xlsIrMemoData(key,t->params,t->paramCount*sizeof(*t->params),limit)) ||
           (t->parameterModes && !xlsIrMemoData(key,t->parameterModes,t->paramCount*sizeof(*t->parameterModes),limit)) ||
           (t->parameterAbis && !xlsIrMemoData(key,t->parameterAbis,t->paramCount*sizeof(*t->parameterAbis),limit)) ||
           (t->returnCount && !xlsIrMemoData(key,t->returns,t->returnCount*sizeof(*t->returns),limit)))return 0;
    }
    return 1;
}
static xls_ir_unit* xlsIrUnitRetain(xls_ir_unit* unit)
{if(unit)xrtAtomic32FetchAdd(&unit->references,1,XMEMORY_RELAXED);return unit;}
static void xlsIrUnitRelease(xls_ir_unit* unit)
{
    if(unit && xrtAtomic32FetchSub(&unit->references,1,XMEMORY_ACQ_REL)==1){
        xrtStrBufFree(&unit->key);xrtStrBufFree(&unit->types);xrtStrBufFree(&unit->data);
        xrtArrayUnit(&unit->dependencies);xrtArrayUnit(&unit->writes);xrtFree(unit);
    }
}
static void xlsIrOwnerReset(const XL6_IrModule* module)
{
    xls_ir_result* owner=xlsActiveIrOwner;if(!owner || &owner->module!=module)return;
    for(size_t i=0;i<owner->fragments.Count;++i)xlsIrUnitRelease(*(xls_ir_unit**)xrtArrayGet(&owner->fragments,i));
    owner->fragments.Count=owner->borrowed.Count=0;
}
/* Grow optional ownership indexes transactionally. A failed/oversized
 * index leaves earlier borrowed fragments intact;this unit uses owned text.
 * XRT still owns all buffer allocation,growth and release. */
static int xlsIrOwnerPrepare(XL6_IrModule* module,size_t nodes)
{
    xls_ir_result* owner=xlsActiveIrOwner;if(!xlsIrOwnerBorrowing(module))return 0;
    size_t end=module->nodeCount+nodes,refs=owner->fragments.Count+1;
    if(end>SIZE_MAX-7 || refs>owner->storageLimit/sizeof(xls_ir_unit*))return 0;
    size_t bits=(end+7)/8;
    if(bits>owner->storageLimit-refs*sizeof(xls_ir_unit*))return 0;
    if(bits<=owner->borrowed.Capacity && refs<=owner->fragments.Capacity)
        return xrtArrayResize(&owner->borrowed,bits);
    xarray nextBits={0},nextRefs={0};int okay=0;
    if(!xrtArrayInit(&nextBits,1) || !xrtArrayResize(&nextBits,bits) ||
       !xrtArrayInit(&nextRefs,sizeof(xls_ir_unit*)) || !xrtArrayReserve(&nextRefs,refs) ||
       nextRefs.Capacity>owner->storageLimit/sizeof(xls_ir_unit*) ||
       nextBits.Capacity>owner->storageLimit-nextRefs.Capacity*sizeof(xls_ir_unit*))goto done;
    if(owner->borrowed.Count)memcpy(nextBits.Data,owner->borrowed.Data,owner->borrowed.Count);
    if(owner->fragments.Count)memcpy(nextRefs.Data,owner->fragments.Data,owner->fragments.Count*sizeof(xls_ir_unit*));
    nextRefs.Count=owner->fragments.Count;
    xrtArrayUnit(&owner->borrowed);xrtArrayUnit(&owner->fragments);
    owner->borrowed=nextBits;owner->fragments=nextRefs;nextBits=(xarray){0};nextRefs=(xarray){0};okay=1;
 done:xrtArrayUnit(&nextBits);xrtArrayUnit(&nextRefs);return okay;
}
static int xlsIrOwnerMark(XL6_IrModule* module,const xls_ir_unit* unit)
{
    xls_ir_result* owner=xlsActiveIrOwner;if(!owner || &owner->module!=module)return 1;
    size_t end=module->nodeCount+unit->nodes;
    if(!xrtArrayPush(&owner->fragments,&unit))return 0;
    xlsIrUnitRetain((xls_ir_unit*)unit);
    for(size_t i=module->nodeCount;i<end;++i)((unsigned char*)owner->borrowed.Data)[i/8]|=(unsigned char)(1u<<(i%8));
    owner->textNodes+=unit->textNodes;owner->textBytes+=unit->textBytes;return 1;
}
void xlsFrontendIrMemoDestroy(xls_ir_memo* memo)
{
    if(!memo)return;
    for(size_t i=0;i<memo->entries.Count;++i)xlsIrUnitRelease(*(xls_ir_unit**)xrtArrayGet(&memo->entries,i));
    xrtArrayUnit(&memo->entries);xrtStrBufFree(&memo->context);xrtFree(memo);
}
static xbytesview xlsIrSymbolKey(const XL6_SymbolId* symbol)
{return (xbytesview){(const unsigned char*)symbol,sizeof(*symbol)};}
static void xlsIrMemoDependency(XL6_IrModule* module,XL6_SymbolId symbol)
{
    xls_ir_memo_build* build=xlsActiveIrMemo;xls_ir_capture* c=build && build->module==module ? build->capture : NULL;
    if(!c || !c->okay || !symbol)return;
    XL6_IrNodeId id=symbol<module->symbolNodeCapacity ? module->symbolNodes[symbol] : 0;
    if(id>c->unit->startNode && id<=module->nodeCount)return; /* already written by this unit */
    const XL6_IrNode* node=xl6IrNodeGet(module,id);
    if(xrtMapHas(&c->dependencies,xlsIrSymbolKey(&symbol)))return;
    xls_ir_dependency value={symbol,node ? id : 0,node ? node->type : 0,node ? node->storageType : 0};
    size_t at=c->unit->dependencies.Count;
    c->okay=at<build->limit/sizeof(value) && xrtArrayPush(&c->unit->dependencies,&value) && xrtMapSet(&c->dependencies,xlsIrSymbolKey(&symbol),&at);
}
static void xlsIrMemoWrite(XL6_IrModule* module,XL6_SymbolId symbol,XL6_IrNodeId binding)
{
    module->symbolNodes[symbol]=binding;
    xls_ir_memo_build* build=xlsActiveIrMemo;xls_ir_capture* c=build && build->module==module ? build->capture : NULL;
    if(!c || !c->okay)return;
    size_t* old=xrtMapGet(&c->writes,xlsIrSymbolKey(&symbol));
    if(old){((xls_ir_write*)xrtArrayGet(&c->unit->writes,*old))->binding=binding;return;}
    xls_ir_write value={symbol,binding};size_t at=c->unit->writes.Count;
    c->okay=at<build->limit/sizeof(value) && xrtArrayPush(&c->unit->writes,&value) && xrtMapSet(&c->writes,xlsIrSymbolKey(&symbol),&at);
}
static void xlsIrMemoInitial(XL6_IrModule* module,XL6_SymbolId symbol,XL6_IrNodeId binding)
{
    if(symbol!=XL6_SYMBOL_ID_INVALID){xlsIrMemoDependency(module,symbol);if(module->symbolNodes[symbol]==XL6_IR_NODE_INVALID)xlsIrMemoWrite(module,symbol,binding);}
}
static const XL6_IrNode* xlsIrMemoRead(XL6_IrModule* module,XL6_SymbolId symbol)
{xlsIrMemoDependency(module,symbol);return xl6IrNodeGet(module,module->symbolNodes[symbol]);}
/* Sparse storage of the original raw struct,not a second IR schema. Pointers
 * and spans are separately owned/rebound. A zero-filled compiler node with
 * unionField=SIZE_MAX is the default;encode only differing machine words. */
#define XLS_IR_NODE_WORDS ((sizeof(XL6_IrNode)+7)/8)
_Static_assert(XLS_IR_NODE_WORDS<=64,"audit raw IR fragment word mask");
static int xlsIrMemoEncodeNode(xstrbuf* data,const XL6_IrNode* in,size_t limit)
{
    XL6_IrNode value=*in;value.text=NULL;value.literalData=NULL;value.span=(XL6_IrSpan){0};value.unionField^=SIZE_MAX;
    uint64_t words[XLS_IR_NODE_WORDS]={0},packed[1+XLS_IR_NODE_WORDS]={0};size_t count=1;
    memcpy(words,&value,sizeof(value));
    for(size_t i=0;i<XLS_IR_NODE_WORDS;++i)if(words[i]){packed[0]|=UINT64_C(1)<<i;packed[count++]=words[i];}
    if(!xlsIrMemoData(data,packed,count*sizeof(*packed),limit))return 0;
    size_t textSize=in->text ? strlen(in->text) : 0;
    return xlsIrMemoNumber(data,in->text!=NULL,limit) && xlsIrMemoNumber(data,textSize,limit) &&
        (!in->text || xlsIrMemoData(data,in->text,textSize+1,limit)) &&
        xlsIrMemoNumber(data,in->literalData!=NULL,limit) &&
        (!in->literalData || (in->literalSize<SIZE_MAX && xlsIrMemoData(data,in->literalData,in->literalSize+1,limit)));
}
typedef struct xls_ir_reader {const unsigned char* data;size_t at,size;} xls_ir_reader;
static int xlsIrMemoReadNumber(xls_ir_reader* r,uint64_t* value)
{
    *value=0;
    for(unsigned i=0;i<10 && r->at<r->size;++i){unsigned char part=r->data[r->at++];if(i==9 && part>1)return 0;*value|=(uint64_t)(part&127u)<<(i*7);if(!(part&128))return 1;}
    return 0;
}
static int xlsIrMemoReadText(xls_ir_reader* r,char** text,int borrow)
{
    uint64_t present,size;*text=NULL;
    if(!xlsIrMemoReadNumber(r,&present) || present>1 || !xlsIrMemoReadNumber(r,&size) || size>r->size-r->at)return 0;
    if(present){
        if(size==r->size-r->at || r->data[r->at+(size_t)size])return 0;
        *text=borrow ? (char*)r->data+r->at : xrtStrDupN((const char*)r->data+r->at,(size_t)size);
        if(!*text)return 0;
        r->at+=(size_t)size+1;
    }else if(size)return 0;
    return 1;
}
static int xlsIrMemoDecodeNode(xls_ir_reader* r,XL6_IrNode* out,int borrow)
{
    uint64_t words[XLS_IR_NODE_WORDS]={0},mask,present;
    if(r->size-r->at<sizeof(mask))return 0;
    memcpy(&mask,r->data+r->at,sizeof(mask));r->at+=sizeof(mask);
    if(mask & ~(UINT64_MAX>>(64-XLS_IR_NODE_WORDS)))return 0;
    for(size_t i=0;i<XLS_IR_NODE_WORDS;++i)if(mask&(UINT64_C(1)<<i)){
        if(r->size-r->at<sizeof(*words))return 0;
        memcpy(&words[i],r->data+r->at,sizeof(*words));r->at+=sizeof(*words);
    }
    memcpy(out,words,sizeof(*out));out->text=NULL;out->literalData=NULL;out->unionField^=SIZE_MAX;
    if(!xlsIrMemoReadText(r,&out->text,borrow) || !xlsIrMemoReadNumber(r,&present) || present>1)return 0;
    if(present){
        if(out->literalSize>=SIZE_MAX || out->literalSize>=r->size-r->at || r->data[r->at+out->literalSize])return 0;
        out->literalData=borrow ? (unsigned char*)r->data+r->at : xrtMalloc(out->literalSize+1);
        if(!out->literalData)return 0;
        if(!borrow)memcpy(out->literalData,r->data+r->at,out->literalSize+1);
        r->at+=out->literalSize+1;
    }
    return 1;
}
/* Decode into reserved but unpublished arena slots. No module count or
 * symbol value changes until the whole fragment has been proved/placed. */
static int xlsIrMemoDecodePlace(const xls_ir_unit* unit,xls_ir_reader* reader,XL6_IrModule* module,
    const XL6_AstNode* source,size_t* at,size_t* touched,size_t depth,int borrow)
{
    if(!source || depth>512 || *at>=unit->nodes || xlsFrontendCancelPoint(XLS_CANCEL_IR_NODE,0))return 0;
    XL6_IrNode* node=&module->nodes[module->nodeCount+(*at)++];
    node->text=NULL;node->literalData=NULL;++*touched;
    if(!xlsIrMemoDecodeNode(reader,node,borrow) || node->sourceKind!=source->kind)return 0;
    if(node->parent){
        if(node->parent<=unit->startNode || node->parent>unit->startNode+unit->nodes)return 0;
        node->parent=(XL6_IrNodeId)(module->nodeCount+node->parent-unit->startNode);
    }
    if(node->firstChild<unit->startChild || node->firstChild>unit->startChild+unit->children)return 0;
    node->firstChild=module->childCount+node->firstChild-unit->startChild;
    node->span.line=source->line;node->span.column=source->column;
    for(size_t i=0;i<source->childCount;++i)
        if(!xlsIrMemoDecodePlace(unit,reader,module,source->children[i],at,touched,depth+1,borrow))return 0;
    return 1;
}
static int xlsIrMemoDependencies(const xls_ir_unit* unit,const XL6_IrModule* module)
{
    for(size_t i=0;i<unit->dependencies.Count;++i){
        const xls_ir_dependency* d=xrtArrayConstGet(&unit->dependencies,i);
        XL6_IrNodeId binding=d->symbol<module->symbolNodeCapacity ? module->symbolNodes[d->symbol] : 0;
        const XL6_IrNode* n=xl6IrNodeGet(module,binding);
        if(!!n!=!!d->binding || (n && (n->type!=d->type || n->storageType!=d->storage)))return 0;
    }
    return 1;
}
static XL6_IrNodeId xlsIrMemoRestore(const xls_ir_unit* unit,XL6_IrModule* module,const XL6_AstNode* source)
{
    size_t at=0,touched=0,start=module->nodeCount;XL6_IrNodeId result=0;
    xls_ir_reader reader={(const unsigned char*)unit->data.Data,0,unit->data.Size};
    if(!unit->nodes || unit->nodes>UINT32_MAX-start || unit->children>SIZE_MAX-module->childCount ||
       !xlsIrMemoDependencies(unit,module))return 0;
    while(start+unit->nodes>module->nodeCapacity)
        if(!xl6IrGrow((void**)&module->nodes,&module->nodeCapacity,module->nodeCapacity,sizeof(*module->nodes)))return 0;
    while(module->childCount+unit->children>module->childCapacity)
        if(!xl6IrGrow((void**)&module->children,&module->childCapacity,module->childCapacity,sizeof(*module->children)))return 0;
    int borrow=xlsIrOwnerPrepare(module,unit->nodes);
    if(!xlsIrMemoDecodePlace(unit,&reader,module,source,&at,&touched,0,borrow) || at!=unit->nodes ||
       unit->children>(reader.size-reader.at)/sizeof(XL6_IrNodeId) ||
       unit->children*sizeof(XL6_IrNodeId)!=reader.size-reader.at)goto failed;
    for(size_t i=0;i<unit->children;++i){
        XL6_IrNodeId id;memcpy(&id,reader.data+reader.at+i*sizeof(id),sizeof(id));
        if(id<=unit->startNode || id>unit->startNode+unit->nodes)goto failed;
        module->children[module->childCount+i]=(XL6_IrNodeId)(start+id-unit->startNode);
    }
    for(size_t i=0;i<unit->writes.Count;++i){
        const xls_ir_write* write=xrtArrayConstGet(&unit->writes,i);
        if(write->binding<=unit->startNode || write->binding>unit->startNode+unit->nodes ||
           !xl6IrEnsureSymbolSlot(module,write->symbol))goto failed;
    }
    if(borrow && !xlsIrOwnerMark(module,unit))goto failed;
    /* Capacity-only effects are safe on fallback. Publish owned decoded
     * nodes/children and replay bindings only after every fallible step. */
    module->nodeCount+=unit->nodes;module->childCount+=unit->children;
    for(size_t i=0;i<unit->writes.Count;++i){
        const xls_ir_write* write=xrtArrayConstGet(&unit->writes,i);
        module->symbolNodes[write->symbol]=(XL6_IrNodeId)(start+write->binding-unit->startNode);
    }
    result=(XL6_IrNodeId)(start+1);return result;
failed:
    for(size_t i=0;i<touched;++i){
        XL6_IrNode* node=&module->nodes[start+i];
        if(!borrow){xrtFree(node->text);xrtFree(node->literalData);}
        node->text=NULL;node->literalData=NULL;
    }
    return 0;
}
static int xlsIrMemoCapture(xls_ir_unit* unit,const XL6_IrModule* module,size_t limit)
{
    unit->nodes=module->nodeCount-unit->startNode;unit->children=module->childCount-unit->startChild;
    for(size_t i=unit->startNode;i<module->nodeCount;++i){
        const XL6_IrNode* node=&module->nodes[i];if(!xlsIrMemoEncodeNode(&unit->data,node,limit))return 0;
        if(node->text || node->literalData)++unit->textNodes;
        if(node->text)unit->textBytes+=strlen(node->text)+1;
        if(node->literalData)unit->textBytes+=node->literalSize+1;
    }
    if(unit->children && !xlsIrMemoData(&unit->data,module->children+unit->startChild,unit->children*sizeof(*module->children),limit))return 0;
    unit->bytes=sizeof(*unit)+unit->key.Capacity+unit->types.Capacity+unit->data.Capacity+
        unit->dependencies.Capacity*sizeof(xls_ir_dependency)+unit->writes.Capacity*sizeof(xls_ir_write);
    return unit->nodes && unit->bytes<=limit;
}
static XL6_IrNodeId xl6IrBuildAstNode(XL6_IrModule* module,const XL6_AstNode* root,const XL6_AstNode* source,
    const XL6_AstNode* parent,size_t childIndex,XL6_DiagList* diags,int* okay)
{
    xls_ir_memo_build* build=xlsActiveIrMemo;
    int function=source && (source->kind==XL6_AST_FUNC || source->kind==XL6_AST_ASYNC_FUNC || source->kind==XL6_AST_EXPORT_FUNC);
    if(!build || build->module!=module || build->root!=root || parent!=root || !function || build->capture ||
       !*okay || (diags && diags->count) || childIndex>=build->memo->entries.Count)
        return xlsIrBuildAstNodeOriginal(module,root,source,parent,childIndex,diags,okay);
    size_t limit=build->limit-build->memo->bytes;
    xstrbuf key={0},types={0};XL6_IrNodeId result=0;
    int usable=limit>sizeof(xls_ir_unit) && xlsFrontendIrAstKey(&key,source,0) && key.Capacity<limit && xlsIrMemoTypes(&module->types,&types,limit-key.Capacity);
    const xls_ir_unit* old=build->baseline ? *(xls_ir_unit* const*)xrtArrayConstGet(&build->baseline->entries,childIndex) : NULL;
    if(usable && old && old->bytes<=limit && xlsIrMemoEqual(&old->key,&key) && xlsIrMemoEqual(&old->types,&types)){
        result=xlsIrMemoRestore(old,module,source);
        if(result){
            *(xls_ir_unit**)xrtArrayGet(&build->memo->entries,childIndex)=xlsIrUnitRetain((xls_ir_unit*)old);
            build->memo->bytes+=old->bytes;++build->work.reused;build->work.nodesReused+=old->nodes;
            xrtStrBufFree(&key);xrtStrBufFree(&types);return result;
        }
    }
    ++build->work.checked;
    xls_ir_unit* unit=usable ? xrtCalloc(1,sizeof(*unit)) : NULL;
    xls_ir_capture capture={.unit=unit};
    if(unit){
        xrtAtomic32Init(&unit->references,1);unit->key=key;unit->types=types;key=(xstrbuf){0};types=(xstrbuf){0};
        unit->startNode=module->nodeCount;unit->startChild=module->childCount;
        capture.okay=xrtArrayInit(&unit->dependencies,sizeof(xls_ir_dependency)) && xrtArrayInit(&unit->writes,sizeof(xls_ir_write)) &&
            xrtMapInit(&capture.dependencies,sizeof(size_t)) && xrtMapInit(&capture.writes,sizeof(size_t));
        build->capture=&capture;
    }
    result=xlsIrBuildAstNodeOriginal(module,root,source,parent,childIndex,diags,okay);build->capture=NULL;
    if(unit){
        xstrbuf after={0};
        if(result && *okay && (!diags || !diags->count) && capture.okay && !xlsFrontendCancelObserved() &&
           xlsIrMemoTypes(&module->types,&after,limit) && xlsIrMemoEqual(&unit->types,&after) && xlsIrMemoCapture(unit,module,limit)){
            *(xls_ir_unit**)xrtArrayGet(&build->memo->entries,childIndex)=unit;build->memo->bytes+=unit->bytes;++build->work.captured;unit=NULL;
        }else ++build->work.declined;
        xrtStrBufFree(&after);xrtMapUnit(&capture.dependencies);xrtMapUnit(&capture.writes);xlsIrUnitRelease(unit);
    }else ++build->work.declined;
    xrtStrBufFree(&key);xrtStrBufFree(&types);return result;
}
int xlsFrontendIrBuildMemo(XL6_IrModule* module,const XL6_AstNode* root,XL6_DiagList* diags,
    const xls_ir_memo* previous,xls_ir_memo** result,xls_ir_memo_work* work,size_t bytes)
{
    if(result)*result=NULL;
    if(work)*work=(xls_ir_memo_work){0};
    if(bytes>XLS_IR_MEMO_BYTES)bytes=XLS_IR_MEMO_BYTES;
    xls_ir_memo* memo=bytes && root ? xrtCalloc(1,sizeof(*memo)) : NULL;
    if(memo){
        if(!xlsIrMemoContext(root,&memo->context,bytes) || memo->context.Capacity>bytes ||
           root->childCount>(bytes-memo->context.Capacity)/sizeof(xls_ir_unit*) ||
           !xrtArrayInit(&memo->entries,sizeof(xls_ir_unit*)) || !xrtArrayResize(&memo->entries,root->childCount) ||
           sizeof(*memo)+memo->context.Capacity+memo->entries.Capacity*sizeof(xls_ir_unit*)>bytes){xlsFrontendIrMemoDestroy(memo);memo=NULL;}
        else memo->bytes=sizeof(*memo)+memo->context.Capacity+memo->entries.Capacity*sizeof(xls_ir_unit*);
    }
    if(!memo)return xl6IrBuildModuleFromAst(module,root,diags);
    xls_ir_memo_build build={.previous=xlsActiveIrMemo,.root=root,.module=module,.memo=memo,.limit=bytes};
    if(previous && previous->entries.Count==memo->entries.Count && xlsIrMemoEqual(&previous->context,&memo->context))build.baseline=previous;
    xlsActiveIrMemo=&build;int accepted=xl6IrBuildModuleFromAst(module,root,diags);xlsActiveIrMemo=build.previous;
    build.work.bytes=memo->bytes;if(work)*work=build.work;
    if(accepted && !xlsFrontendCancelObserved() && result)*result=memo;else xlsFrontendIrMemoDestroy(memo);
    return accepted;
}

void xlsFrontendIrResultDestroy(xls_ir_result* result)
{
    if(!result)return;
    xls_ir_result* previous=xlsActiveIrOwner;xlsActiveIrOwner=result;
    xl6IrModuleFree(&result->module);xlsActiveIrOwner=previous;
    xlsFrontendIrMemoDestroy(result->memo);xrtArrayUnit(&result->fragments);xrtArrayUnit(&result->borrowed);xrtFree(result);
}
xls_ir_result* xlsFrontendIrResultBuildWithStorage(const XL6_AstNode* root,XL6_DiagList* diags,
    const xls_ir_result* previous,size_t bytes,size_t storageBytes)
{
    xls_ir_result* result=xrtCalloc(1,sizeof(*result));if(!result)return NULL;
    result->storageLimit=storageBytes<128u*1024u ? storageBytes : 128u*1024u;
    if(!xrtArrayInit(&result->fragments,sizeof(xls_ir_unit*)) || !xrtArrayInit(&result->borrowed,1)){
        xlsFrontendIrResultDestroy(result);return NULL;
    }
    xls_ir_result* before=xlsActiveIrOwner;xlsActiveIrOwner=result;
    int okay=xlsFrontendIrBuildMemo(&result->module,root,diags,previous ? previous->memo : NULL,&result->memo,&result->work,bytes);
    xlsActiveIrOwner=before;
    if(!okay || xlsFrontendCancelObserved()){xlsFrontendIrResultDestroy(result);return NULL;}
    result->work.textNodes=result->textNodes;result->work.textBytes=result->textBytes;
    result->work.ownerBytes=result->fragments.Capacity*sizeof(xls_ir_unit*)+result->borrowed.Capacity;
    return result;
}
const XL6_IrModule* xlsFrontendIrResultModule(const xls_ir_result* result)
{return result ? &result->module : NULL;}
xls_ir_memo_work xlsFrontendIrResultWork(const xls_ir_result* result)
{return result ? result->work : (xls_ir_memo_work){0};}

xls_ir_result* xlsFrontendIrResultBuild(const XL6_AstNode* root,XL6_DiagList* diags,
    const xls_ir_result* previous,size_t bytes)
{return xlsFrontendIrResultBuildWithStorage(root,diags,previous,bytes,128u*1024u);}
