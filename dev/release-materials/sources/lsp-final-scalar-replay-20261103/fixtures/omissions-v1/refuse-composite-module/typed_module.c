_Thread_local int xlsNativeTestAllocation;
#include "analysis/typed_module_internal.h"
#include "analysis/ast_size.h"
#include <xrt.h>
struct xls_typed_input_identity {xatomic32 references;};
typedef struct xls_typed_forest xls_typed_forest;
struct xls_typed_function {xatomic32 references;XL6_AstNode* root;xls_typed_forest* forest;};
/* Only teardown is serialized. Original non-atomic source references and XRT
 * pools can span sibling subtrees. This record owns no module/root/check and
 * retains no sibling payload after that sibling's last function reference. */
struct xls_typed_forest {xatomic32 references;xmutex lock;size_t slots,bytes,knownBytes,knownSlots;xls_typed_function functions[];};
struct xls_typed_module {
    xatomic32 references;XL6_AstNode* root;xls_typed_input_identity* identity;xls_typed_forest* forest;
    xls_typed_function** borrowed;size_t borrowedSlots;
};
#define XLS_TYPED_FOREST_BYTES (8u*1024u*1024u)
static int function(const XL6_AstNode* node)
{return node && (node->kind==XL6_AST_FUNC || node->kind==XL6_AST_ASYNC_FUNC || node->kind==XL6_AST_EXPORT_FUNC);}
static xls_typed_forest* forestCreate(XL6_AstNode* root)
{
    if(root->kind!=XL6_AST_MODULE || !root->childCount || !root->children ||
       root->childCount>65536u || root->childCount>(XLS_TYPED_FOREST_BYTES-sizeof(xls_typed_forest))/sizeof(xls_typed_function))return NULL;
    size_t count=0;for(size_t i=0;i<root->childCount;++i)count+=function(root->children[i]);
    if(!count)return NULL;
    size_t bytes=sizeof(xls_typed_forest)+root->childCount*sizeof(xls_typed_function);
    xls_typed_forest* forest=xrtCalloc(1,bytes);if(!forest)return NULL;
    if(!xrtMutexInit(&forest->lock)){xrtFree(forest);return NULL;}
    xrtAtomic32Init(&forest->references,(int32)count+1);forest->slots=root->childCount;forest->bytes=bytes;
    for(size_t i=0;i<root->childCount;++i)if(function(root->children[i])){
        xls_typed_function* f=&forest->functions[i];f->root=root->children[i];f->forest=forest;xrtAtomic32Init(&f->references,1);
    }
    return forest;
}
static void forestRelease(xls_typed_forest* forest)
{if(forest && xrtAtomic32FetchSub(&forest->references,1,XMEMORY_ACQ_REL)==1){xrtMutexUnit(&forest->lock);xrtFree(forest);}}
xls_typed_function* xlsTypedFunctionRetain(xls_typed_function* f)
{if(f)xrtAtomic32FetchAdd(&f->references,1,XMEMORY_RELAXED);return f;}
void xlsTypedFunctionRelease(xls_typed_function* f)
{
    if(!f || xrtAtomic32FetchSub(&f->references,1,XMEMORY_ACQ_REL)!=1)return;
    xls_typed_forest* forest=f->forest;
    /* The final reference owns this transition. No reader can still access
     * this function; sibling destructors enter the same shared-owner lock. */
    xrtMutexLock(&forest->lock);xl6AstFree(f->root);f->root=NULL;xrtMutexUnlock(&forest->lock);
    forestRelease(forest);
}
const XL6_AstNode* xlsTypedFunctionRoot(const xls_typed_function* f){return f ? f->root : NULL;}
size_t xlsTypedFunctionMetadataBytes(const xls_typed_function* f){return f ? f->forest->bytes : 0;}
const void* xlsTypedFunctionMetadataOwner(const xls_typed_function* f){return f ? f->forest : NULL;}
size_t xlsTypedFunctionKnownBytes(const xls_typed_function* f)
{return f && (size_t)(f-f->forest->functions)<f->forest->knownSlots ? f->forest->knownBytes : 0;}
int xlsTypedModuleMeasureFunctions(xls_typed_module* module,size_t limit,int (*cancelled)(void*),void* user)
{
    xls_typed_forest* forest=module ? module->forest : NULL;
    if(!module)return 0;
    if(!forest)return 1;
    if(forest->knownBytes)return forest->knownBytes<=limit;
    if(forest->bytes>limit)return 0;
    xls_ast_size size={0};size_t slots=0,bytes=0;
    int okay=xlsAstSizeBegin(&size,limit-forest->bytes,1);
    for(size_t i=0;okay && i<forest->slots;++i){
        if(!forest->functions[i].root)continue;
        if(!xlsAstSizeTree(&size,forest->functions[i].root,0,cancelled,user)){
            /* Only a pure budget stop can keep earlier complete functions.
             * The failed tree may have charged new pools/sources: its partial
             * counter and maps are discarded,never installed in the receipt. */
            if(!size.exceeded || (cancelled && cancelled(user)))slots=0;
            break;
        }
        slots=i+1;bytes=size.bytes;
    }
    if(slots){forest->knownBytes=forest->bytes+bytes;forest->knownSlots=slots;}
    xlsAstSizeEnd(&size);return slots!=0;
}
size_t xlsTypedFunctionRootSlot(const xls_typed_function* f){return f ? (size_t)(f-f->forest->functions) : SIZE_MAX;}
xls_typed_function* xlsTypedModuleFunction(const xls_typed_module* module,size_t slot)
{
    if(module && slot<module->borrowedSlots && module->borrowed[slot])return xlsTypedFunctionRetain(module->borrowed[slot]);
    xls_typed_forest* forest=module ? module->forest : NULL;
    return forest && slot<forest->slots && forest->functions[slot].root ? xlsTypedFunctionRetain(&forest->functions[slot]) : NULL;
}
xls_typed_input_identity* xlsTypedInputIdentityCreate(void)
{
    xls_typed_input_identity* identity=xrtCalloc(1,sizeof(*identity));
    if(identity)xrtAtomic32Init(&identity->references,1);
    return identity;
}
size_t xlsTypedInputIdentityOwnedBytes(const xls_typed_input_identity* identity)
{return identity ? sizeof(*identity) : 0;}
xls_typed_input_identity* xlsTypedInputIdentityRetain(xls_typed_input_identity* identity)
{if(identity)xrtAtomic32FetchAdd(&identity->references,1,XMEMORY_RELAXED);return identity;}
void xlsTypedInputIdentityRelease(xls_typed_input_identity* identity)
{if(identity && xrtAtomic32FetchSub(&identity->references,1,XMEMORY_ACQ_REL)==1)xrtFree(identity);}
xls_typed_input_identity* xlsTypedModuleIdentity(const xls_typed_module* module)
{return module ? module->identity : NULL;}
xls_typed_module* xlsTypedModuleTake(XL6_AstNode* root)
{
    if(!root)return NULL;
    xls_typed_module* module=xrtCalloc(1,sizeof(*module));if(!module)return NULL;
    module->identity=xlsTypedInputIdentityCreate();
    xrtAtomic32Init(&module->references,1);module->root=root;module->forest=forestCreate(root);return module;
}
xls_typed_module* xlsTypedModuleTakeBorrowed(XL6_AstNode* root,xls_typed_function* const* owners,size_t count)
{
    if(!root || root->kind!=XL6_AST_MODULE || count!=root->childCount || count>65536u ||
       (count && (!owners || !root->children)))return NULL;
    size_t borrowed=0;
    for(size_t i=0;i<count;++i)if(owners[i]){
        if(!function(root->children[i]) || xlsTypedFunctionRoot(owners[i])!=root->children[i])return NULL;
        ++borrowed;
    }
    if(!borrowed)return xlsTypedModuleTake(root);
    xlsNativeTestAllocation=1;
    xls_typed_module* module=xrtCalloc(1,sizeof(*module));
    xlsNativeTestAllocation=0;if(!module)return NULL;
    module->borrowed=xrtCalloc(count,sizeof(*module->borrowed));
    if(!module->borrowed){xrtFree(module);return NULL;}
    xrtAtomic32Init(&module->references,1);module->root=root;module->borrowedSlots=count;
    module->identity=xlsTypedInputIdentityCreate();
    /* Mandatory allocations/validation precede any ownership change. Only fresh
     * children enter the new forest. Optional forest failure still leaves fresh
     * children owned by the root,and borrowed children independently retained. */
    for(size_t i=0;i<count;++i)if(owners[i]){
        module->borrowed[i]=xlsTypedFunctionRetain(owners[i]);root->children[i]=NULL;
    }
    module->forest=forestCreate(root);
    for(size_t i=0;i<count;++i)if(module->borrowed[i])root->children[i]=(XL6_AstNode*)xlsTypedFunctionRoot(module->borrowed[i]);
    return module;
}
xls_typed_module* xlsTypedModuleRetain(xls_typed_module* module)
{if(module)xrtAtomic32FetchAdd(&module->references,1,XMEMORY_RELAXED);return module;}
void xlsTypedModuleRelease(xls_typed_module* module)
{
    if(!module || xrtAtomic32FetchSub(&module->references,1,XMEMORY_ACQ_REL)!=1)return;
    /* Borrowed functions keep their original teardown lock. Detach them before
     * touching the fresh root;never hold one forest lock while releasing another. */
    for(size_t i=0;i<module->borrowedSlots;++i)if(module->borrowed[i])module->root->children[i]=NULL;
    xls_typed_forest* forest=module->forest;
    if(forest){
        /* The module's last reference exclusively owns its root presentation.
         * Detached children stay owned by function handles,never by that
         * presentation. Other declarations retire immediately with the root. */
        xrtMutexLock(&forest->lock);
        for(size_t i=0;i<forest->slots;++i)if(forest->functions[i].root)module->root->children[i]=NULL;
        xl6AstFree(module->root);xrtMutexUnlock(&forest->lock);
        for(size_t i=0;i<forest->slots;++i)if(forest->functions[i].root)xlsTypedFunctionRelease(&forest->functions[i]);
        forestRelease(forest);
    }else xl6AstFree(module->root);
    for(size_t i=0;i<module->borrowedSlots;++i)xlsTypedFunctionRelease(module->borrowed[i]);
    xrtFree(module->borrowed);xlsTypedInputIdentityRelease(module->identity);xrtFree(module);
}
const XL6_AstNode* xlsTypedModuleRoot(const xls_typed_module* module)
{return module ? module->root : NULL;}
