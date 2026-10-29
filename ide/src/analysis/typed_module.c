#include "analysis/typed_module_internal.h"
#include <xrt.h>
struct xls_typed_module {xatomic32 references;XL6_AstNode* root;};
xls_typed_module* xlsTypedModuleTake(XL6_AstNode* root)
{
    if(!root)return NULL;
    xls_typed_module* module=xrtCalloc(1,sizeof(*module));if(!module)return NULL;
    xrtAtomic32Init(&module->references,1);module->root=root;return module;
}
xls_typed_module* xlsTypedModuleRetain(xls_typed_module* module)
{if(module)xrtAtomic32FetchAdd(&module->references,1,XMEMORY_RELAXED);return module;}
void xlsTypedModuleRelease(xls_typed_module* module)
{if(module && xrtAtomic32FetchSub(&module->references,1,XMEMORY_ACQ_REL)==1){xl6AstFree(module->root);xrtFree(module);}}
const XL6_AstNode* xlsTypedModuleRoot(const xls_typed_module* module)
{return module ? module->root : NULL;}
