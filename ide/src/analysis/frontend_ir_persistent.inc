/* Immutable finalized typed-module assets. Native nodes remain independently
 * owned by each result;only the original readonly tables and payloads share.
 * No previous result,AST,check,memo,or node arena is retained by this asset. */
#define XLS_IR_PERSISTENT_BYTES (32u*1024u*1024u)
#include "analysis/typed_module_internal.h"
typedef struct xls_ir_persistent_keys {xarray segments;size_t size,bytes;} xls_ir_persistent_keys;
static void xlsIrPersistentKeysFree(xls_ir_persistent_keys* keys)
{
    for(size_t i=0;i<keys->segments.Count;++i)xrtStrBufFree(xrtArrayGet(&keys->segments,i));
    xrtArrayUnit(&keys->segments);*keys=(xls_ir_persistent_keys){0};
}
static int xlsIrPersistentKeysBuild(xls_ir_persistent_keys* keys,const XL6_AstNode* root,size_t limit)
{
    if(!root || root->childCount==SIZE_MAX || (root->childCount && !root->children) ||
       root->childCount+1>limit/sizeof(xstrbuf) || !xrtArrayInit(&keys->segments,sizeof(xstrbuf)) ||
       !xrtArrayResize(&keys->segments,root->childCount+1) || keys->segments.Capacity>limit/sizeof(xstrbuf))return 0;
    keys->bytes=keys->segments.Capacity*sizeof(xstrbuf);
    /* Independent complete native-ID keys for the module envelope and every
     * declaration. Keep the original8MiB key guard per declaration rather
     * than forcing a large module through a single oversized encoder call. */
    XL6_AstNode envelope=*root;envelope.children=NULL;envelope.childCount=envelope.childCapacity=0;
    for(size_t i=0;i<keys->segments.Count;++i){
        if(xlsFrontendCancelPoint(XLS_CANCEL_IR_PHASE,1))return 0;
        xstrbuf* segment=xrtArrayGet(&keys->segments,i);
        if(!xlsFrontendIrAstKey(segment,i ? root->children[i-1] : &envelope,XLS_IR_KEY_EXACT) ||
           (!i && !xlsIrMemoNumber(segment,root->childCount,limit-keys->bytes)) ||
           segment->Capacity>limit-keys->bytes)return 0;
        keys->bytes+=segment->Capacity;keys->size+=segment->Size;
    }
    return 1;
}
static int xlsIrPersistentKeysEqual(const xls_ir_persistent_keys* a,const xls_ir_persistent_keys* b)
{
    if(a->segments.Count!=b->segments.Count || a->size!=b->size)return 0;
    for(size_t i=0;i<a->segments.Count;++i)
        if(!xlsIrMemoEqual(xrtArrayConstGet(&a->segments,i),xrtArrayConstGet(&b->segments,i)))return 0;
    return 1;
}
struct xls_ir_persistent_storage {
    xatomic32 references;
    XL6_IrModule tables; /* original destructor owns everything except nodes */
    xarray texts,fragments;
    xls_ir_persistent_keys key;
    size_t metadataBytes;
    uint64_t textNodes,textBytes;
};
static xls_ir_persistent_storage* xlsIrPersistentReady(const xls_ir_result* result)
{return result && xrtAtomic32Load(&result->persistentState,XMEMORY_ACQUIRE)==2 ? result->persistent : NULL;}
static xls_ir_persistent_storage* xlsIrPersistentRetain(xls_ir_persistent_storage* value)
{if(value)xrtAtomic32FetchAdd(&value->references,1,XMEMORY_RELAXED);return value;}
static void xlsIrPersistentRelease(xls_ir_persistent_storage* value)
{
    if(!value || xrtAtomic32FetchSub(&value->references,1,XMEMORY_ACQ_REL)!=1)return;
    xl6IrModuleFree(&value->tables);
    for(size_t i=0;i<value->texts.Count;++i)xrtFree(*(void**)xrtArrayGet(&value->texts,i));
    for(size_t i=0;i<value->fragments.Count;++i)xlsIrUnitRelease(*(xls_ir_unit**)xrtArrayGet(&value->fragments,i));
    xrtArrayUnit(&value->texts);xrtArrayUnit(&value->fragments);xlsIrPersistentKeysFree(&value->key);xrtFree(value);
}
static int xlsIrPersistentPositions(XL6_IrModule* module,const XL6_AstNode* node,size_t* at,size_t depth,int apply)
{
    if(!node)return 1;
    if(depth>512 || *at>=module->nodeCount || (node->childCount && !node->children) ||
       xlsFrontendCancelPoint(XLS_CANCEL_IR_NODE,0))return 0;
    XL6_IrNode* ir=&module->nodes[(*at)++];
    if(ir->sourceKind!=node->kind)return 0;
    if(apply){ir->span.line=node->line;ir->span.column=node->column;}
    else if(ir->span.line!=node->line || ir->span.column!=node->column)return 0;
    for(size_t i=0;i<node->childCount;++i)
        if(!xlsIrPersistentPositions(module,node->children[i],at,depth+1,apply))return 0;
    return 1;
}
static int xlsIrPersistentCapture(xls_ir_result* result,const XL6_AstNode* root,xls_ir_persistent_keys* key,size_t limit,int record)
{
    /* Lowered instructions have their own sourceSpan mapping and are outside
     * this typed-AST owner. Never reuse their coordinates without that proof. */
    if(!key->size || !result->module.ready || result->module.instructionCount || result->persistent ||
       limit<sizeof(xls_ir_persistent_storage) || key->bytes>limit-sizeof(xls_ir_persistent_storage))return 0;
    size_t at=0;
    if(!xlsIrPersistentPositions(&result->module,root,&at,0,0) || at!=result->module.nodeCount)return 0;
    xls_ir_persistent_storage* value=xrtCalloc(1,sizeof(*value));if(!value)return 0;
    size_t bytes=sizeof(*value)+key->bytes;
    if(result->fragments.Capacity>(limit-bytes)/sizeof(xls_ir_unit*) || !xrtArrayInit(&value->texts,sizeof(void*)))goto failed;
    bytes+=result->fragments.Capacity*sizeof(xls_ir_unit*);
    for(size_t i=0;i<result->module.nodeCount;++i){
        const XL6_IrNode* node=&result->module.nodes[i];
        if(xlsFrontendCancelPoint(XLS_CANCEL_IR_NODE,0))goto failed;
        int borrowed=i/8<result->borrowed.Count && (((const unsigned char*)result->borrowed.Data)[i/8]&(1u<<(i%8)));
        if(node->text || node->literalData)++value->textNodes;
        if(node->text)value->textBytes+=strlen(node->text)+1;
        if(node->literalData)value->textBytes+=node->literalSize+1;
        void* payloads[]={node->text,node->literalData};
        if(!borrowed)for(size_t j=0;j<2;++j)if(payloads[j]){
            if(value->texts.Count>=(limit-bytes)/sizeof(void*) || !xrtArrayPush(&value->texts,&payloads[j]) ||
               value->texts.Capacity>(limit-bytes)/sizeof(void*))goto failed;
        }
    }
    bytes+=value->texts.Capacity*sizeof(void*);
    /* All fallible operations completed. Transfer original table/payload
     * ownership atomically; neither the IR values nor old owners change. */
    xrtAtomic32Init(&value->references,1);value->metadataBytes=bytes;
    value->tables=result->module;value->tables.nodes=NULL;value->tables.nodeCount=value->tables.nodeCapacity=0;
    value->fragments=result->fragments;result->fragments=(xarray){0};
    value->key=*key;*key=(xls_ir_persistent_keys){0};xrtArrayUnit(&result->borrowed);
    result->persistent=value;
    if(record){result->persistentWork.captured=1;result->persistentWork.metadataBytes=bytes;}
    xls_typed_module* input=result->input;result->input=NULL;xlsTypedModuleRelease(input);
    xrtAtomic32Store(&result->persistentState,2,XMEMORY_RELEASE);
    return 1;
failed:
    /* Text entries are still borrowed from the ordinary result until commit. */
    xrtArrayUnit(&value->texts);xrtFree(value);return 0;
}
static xls_ir_memo* xlsIrPersistentMemo(const xls_ir_memo* previous,const XL6_AstNode* root,size_t bytes)
{
    if(!previous || !bytes)return NULL;
    if(bytes>XLS_IR_MEMO_BYTES)bytes=XLS_IR_MEMO_BYTES;
    const xls_entity_view* entities=xlsActiveIrEntities && xlsActiveIrEntities->root==root ? xlsActiveIrEntities->entities : NULL;
    if((!entities)!=(!previous->entities) || (entities &&
       (xlsEntityViewStore(entities)!=xlsEntityViewStore(previous->entities) ||
        xlsEntityViewInfo(entities).domain!=xlsEntityViewInfo(previous->entities).domain)))return NULL;
    xls_ir_memo* memo=xrtCalloc(1,sizeof(*memo));if(!memo)return NULL;
    if(sizeof(*memo)>bytes || !xlsIrMemoData(&memo->context,previous->context.Data,previous->context.Size,bytes-sizeof(*memo)) ||
       memo->context.Capacity>bytes-sizeof(*memo) ||
       previous->entries.Count>(bytes-sizeof(*memo)-memo->context.Capacity)/sizeof(xls_ir_unit*) ||
       !xrtArrayInit(&memo->entries,sizeof(xls_ir_unit*)) || !xrtArrayResize(&memo->entries,previous->entries.Count) ||
       memo->entries.Capacity>(bytes-sizeof(*memo)-memo->context.Capacity)/sizeof(xls_ir_unit*))goto failed;
    memo->bytes=sizeof(*memo)+memo->context.Capacity+memo->entries.Capacity*sizeof(xls_ir_unit*);
    for(size_t i=0;i<previous->entries.Count;++i){
        xls_ir_unit* unit=*(xls_ir_unit* const*)xrtArrayConstGet(&previous->entries,i);
        if(unit){if(unit->bytes>bytes-memo->bytes)goto failed;memo->bytes+=unit->bytes;}
        *(xls_ir_unit**)xrtArrayGet(&memo->entries,i)=xlsIrUnitRetain(unit);
    }
    memo->entities=xlsEntityViewRetain((xls_entity_view*)entities);return memo;
failed:xlsFrontendIrMemoDestroy(memo);return NULL;
}
static xls_ir_result* xlsIrPersistentClone(const xls_ir_result* previous,const XL6_AstNode* root,size_t rawBytes,size_t limit)
{
    xls_ir_persistent_storage* storage=xlsIrPersistentReady(previous);
    if(!storage || storage->metadataBytes>limit || previous->module.nodeCount>SIZE_MAX/sizeof(XL6_IrNode))return NULL;
    xls_ir_result* result=xrtCalloc(1,sizeof(*result));if(!result)return NULL;
    xrtAtomic32Init(&result->persistentState,2);
    result->module=storage->tables;result->module.nodes=NULL;
    result->persistent=xlsIrPersistentRetain(storage);
    size_t count=previous->module.nodeCount,capacity=previous->module.nodeCapacity;
    if(capacity<count || capacity>SIZE_MAX/sizeof(XL6_IrNode))goto failed;
    /* Keep the original producer's arena geometry. Alternating exact-sized
     * views with the native doubling builder creates different large-buffer
     * allocation sizes. Only live nodes are copied and verified. */
    if(capacity){result->module.nodes=xrtMalloc(capacity*sizeof(XL6_IrNode));if(!result->module.nodes)goto failed;}
    if(count)memcpy(result->module.nodes,previous->module.nodes,count*sizeof(XL6_IrNode));
    result->module.nodeCount=count;result->module.nodeCapacity=capacity;
    size_t at=0;if(!xlsIrPersistentPositions(&result->module,root,&at,0,1) || at!=count)goto failed;
    XL6_DiagList attempted;xl6DiagInit(&attempted);
    int okay=xl6IrVerifyModule(&result->module,&attempted) && !attempted.count;xl6DiagFree(&attempted);
    if(!okay || xlsFrontendCancelObserved())goto failed;
    result->memo=xlsIrPersistentMemo(previous->memo,root,rawBytes);
    result->work.bytes=result->memo ? result->memo->bytes : 0;
    result->work.textNodes=storage->textNodes;result->work.textBytes=storage->textBytes;
    result->persistentWork=(xls_ir_persistent_work){.reused=1,.verified=1,.nodesCopied=count,.tablesShared=1,
        .metadataBytes=storage->metadataBytes,.keyBytes=storage->key.size};
    return result;
failed:xlsFrontendIrResultDestroy(result);return NULL;
}
xls_ir_result* xlsFrontendIrResultBuildPersistent(const XL6_AstNode* root,XL6_DiagList* diags,
    const xls_ir_result* previous,size_t rawBytes,size_t ownerBytes,size_t persistentBytes)
{
    size_t limit=persistentBytes<XLS_IR_PERSISTENT_BYTES ? persistentBytes : XLS_IR_PERSISTENT_BYTES;
    xls_ir_persistent_keys key={0};int keyed=root && (!diags || !diags->count) && limit>=sizeof(xls_ir_persistent_storage) &&
        xlsIrPersistentKeysBuild(&key,root,limit-sizeof(xls_ir_persistent_storage));
    if(xlsFrontendCancelObserved()){xlsIrPersistentKeysFree(&key);return NULL;}
    xls_ir_persistent_storage* ready=xlsIrPersistentReady(previous);
    if(keyed && ready && xlsIrPersistentKeysEqual(&key,&ready->key)){
        xls_ir_result* result=xlsIrPersistentClone(previous,root,rawBytes,limit);
        if(result){xlsIrPersistentKeysFree(&key);return result;}
        if(xlsFrontendCancelObserved()){xlsIrPersistentKeysFree(&key);return NULL;}
    }
    if(!keyed)xlsIrPersistentKeysFree(&key);
    xls_ir_result* result=xlsFrontendIrResultBuildWithStorage(root,diags,previous,rawBytes,ownerBytes);
    if(result){result->persistentWork.built=1;result->persistentWork.verified=1;result->persistentWork.keyBytes=key.size;
        if(!keyed || !xlsIrPersistentCapture(result,root,&key,limit,1))result->persistentWork.declined=1;}
    xlsIrPersistentKeysFree(&key);
    if(xlsFrontendCancelObserved()){xlsFrontendIrResultDestroy(result);return NULL;}
    return result;
}
xls_ir_persistent_work xlsFrontendIrResultPersistentWork(const xls_ir_result* result)
{return result ? result->persistentWork : (xls_ir_persistent_work){0};}
xls_ir_result* xlsFrontendIrResultBuildEntitiesPersistent(const XL6_AstNode* root,XL6_DiagList* diags,
    const xls_ir_result* previous,size_t bytes,const xls_entity_view* entities,size_t persistentBytes)
{
    xls_ir_entity_scope scope={xlsActiveIrEntities,root,entities};xlsActiveIrEntities=&scope;
    xls_ir_result* result=xlsFrontendIrResultBuildPersistent(root,diags,previous,bytes,128u*1024u,persistentBytes);
    xlsActiveIrEntities=scope.previous;return result;
}
int xlsFrontendIrResultPreparePersistent(xls_ir_result* result,size_t persistentBytes)
{
    if(!result || !persistentBytes)return 0;
    size_t limit=persistentBytes<XLS_IR_PERSISTENT_BYTES ? persistentBytes : XLS_IR_PERSISTENT_BYTES;
    xls_ir_persistent_storage* ready=xlsIrPersistentReady(result);
    if(ready)return ready->metadataBytes<=limit;
    uint32 expected=0;
    if(!xrtAtomic32CompareExchange(&result->persistentState,&expected,1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE))return 0;
    const XL6_AstNode* root=xlsTypedModuleRoot(result->input);xls_ir_persistent_keys key={0};
    int okay=root && limit>sizeof(xls_ir_persistent_storage) &&
        xlsIrPersistentKeysBuild(&key,root,limit-sizeof(xls_ir_persistent_storage)) &&
        !xlsFrontendCancelObserved() && xlsIrPersistentCapture(result,root,&key,limit,0);
    xlsIrPersistentKeysFree(&key);
    if(!okay)xrtAtomic32Store(&result->persistentState,0,XMEMORY_RELEASE);
    return okay ? 2 : 0;
}
xls_ir_result* xlsFrontendIrResultBuildTyped(xls_typed_module* input,XL6_DiagList* diags,
    const xls_ir_result* previous,size_t rawBytes,const xls_entity_view* entities,size_t persistentBytes)
{
    const XL6_AstNode* root=xlsTypedModuleRoot(input);if(!root)return NULL;
    xls_ir_result* result=persistentBytes ? xlsFrontendIrResultBuildEntitiesPersistent(root,diags,previous,rawBytes,entities,persistentBytes) :
        xlsFrontendIrResultBuildEntities(root,diags,previous,rawBytes,entities);
    if(result && !xlsIrPersistentReady(result))result->input=xlsTypedModuleRetain(input);
    return result;
}
