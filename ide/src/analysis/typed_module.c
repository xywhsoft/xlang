#include "analysis/typed_module_internal.h"
#include <xrt.h>
struct xls_typed_input_identity {xatomic32 references;};
struct xls_typed_module {xatomic32 references;XL6_AstNode* root;xls_typed_input_identity* identity;};
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
    xrtAtomic32Init(&module->references,1);module->root=root;return module;
}
xls_typed_module* xlsTypedModuleRetain(xls_typed_module* module)
{if(module)xrtAtomic32FetchAdd(&module->references,1,XMEMORY_RELAXED);return module;}
void xlsTypedModuleRelease(xls_typed_module* module)
{if(module && xrtAtomic32FetchSub(&module->references,1,XMEMORY_ACQ_REL)==1){xl6AstFree(module->root);xlsTypedInputIdentityRelease(module->identity);xrtFree(module);}}
const XL6_AstNode* xlsTypedModuleRoot(const xls_typed_module* module)
{return module ? module->root : NULL;}
