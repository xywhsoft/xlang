#include "analysis/typed_module_internal.h"
#include <xrt.h>
#include <stdio.h>
static unsigned checks;
static size_t faultAt,faultCalls,faultDenied;
static int captureObjects,trackingOverflow;
static void* objects[16];
static size_t trackedObjects;
static XL6_AstNode* expectedRoot;
static unsigned rootFrees;
void* __real_xrtCalloc(size_t,size_t);
void __real_xrtFree(void*);
void __real_xl6AstFree(XL6_AstNode*);
void* __wrap_xrtCalloc(size_t count,size_t size)
{
 if(faultAt && ++faultCalls==faultAt){++faultDenied;return NULL;}
 void* p=__real_xrtCalloc(count,size);
 if(p && captureObjects){
  size_t slot=0;while(slot<16 && objects[slot])++slot;
  if(slot==16)trackingOverflow=1;else{objects[slot]=p;++trackedObjects;}
 }
 return p;
}
void __wrap_xrtFree(void* p)
{
 if(p)for(size_t i=0;i<16;++i)if(objects[i]==p){objects[i]=NULL;--trackedObjects;break;}
 __real_xrtFree(p);
}
void __wrap_xl6AstFree(XL6_AstNode* root)
{if(root && root==expectedRoot){++rootFrees;expectedRoot=NULL;}__real_xl6AstFree(root);}
static xls_typed_module* take(XL6_AstNode* root)
{captureObjects=1;xls_typed_module* module=xlsTypedModuleTake(root);captureObjects=0;return module;}
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_typed_identity_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static int identities(void)
{
 size_t before=trackedObjects;
 XL6_AstNode* root=xl6AstNew(XL6_AST_MODULE,NULL);CHECK(root);
 expectedRoot=root;rootFrees=0;
 xls_typed_module* module=take(root);CHECK(module);CHECK(trackedObjects==before+2);
 xls_typed_input_identity* identity=xlsTypedInputIdentityRetain(xlsTypedModuleIdentity(module));CHECK(identity);
 CHECK(xlsTypedInputIdentityOwnedBytes(identity)>0 && !xlsTypedInputIdentityOwnedBytes(NULL));
 CHECK(xlsTypedModuleRoot(module)==root);CHECK(xlsTypedModuleRetain(module)==module);
 CHECK(xlsTypedModuleIdentity(module)==identity);xlsTypedModuleRelease(module);
 CHECK(xlsTypedModuleRoot(module)==root && !rootFrees);xlsTypedModuleRelease(module);
 /* A retained identity owns exactly itself,not the module or its AST. */
 CHECK(rootFrees==1);CHECK(trackedObjects==before+1);
 for(unsigned i=0;i<1024;++i){
  root=xl6AstNew(XL6_AST_MODULE,NULL);CHECK(root);
  expectedRoot=root;rootFrees=0;module=take(root);CHECK(module);
  CHECK(xlsTypedModuleIdentity(module) && xlsTypedModuleIdentity(module)!=identity);
  xls_typed_input_identity* other=xlsTypedInputIdentityRetain(xlsTypedModuleIdentity(module));
  xlsTypedModuleRelease(module);CHECK(rootFrees==1);CHECK(other!=identity);xlsTypedInputIdentityRelease(other);
  CHECK(trackedObjects==before+1);
 }
 xlsTypedInputIdentityRelease(identity);CHECK(trackedObjects==before && !trackingOverflow);
 CHECK(!xlsTypedModuleIdentity(NULL));CHECK(!xlsTypedInputIdentityRetain(NULL));
 xlsTypedInputIdentityRelease(NULL);CHECK(!xlsTypedModuleTake(NULL));return 1;
}
static int faults(void)
{
 size_t before=trackedObjects;
 XL6_AstNode* root=xl6AstNew(XL6_AST_MODULE,NULL);CHECK(root);
 faultAt=1;faultCalls=faultDenied=0;
 xls_typed_module* module=take(root);
 faultAt=0;CHECK(!module && faultDenied==1);
 /* Failed Take leaves the original root with its caller. */
 CHECK(root->kind==XL6_AST_MODULE);xl6AstFree(root);CHECK(trackedObjects==before);
 root=xl6AstNew(XL6_AST_MODULE,NULL);CHECK(root);
 expectedRoot=root;rootFrees=0;
 faultAt=2;faultCalls=faultDenied=0;module=take(root);faultAt=0;
 CHECK(module && faultDenied==1);CHECK(xlsTypedModuleRoot(module)==root);
 CHECK(!xlsTypedModuleIdentity(module));CHECK(trackedObjects==before+1);
 xlsTypedModuleRelease(module);CHECK(rootFrees==1);CHECK(trackedObjects==before);return 1;
}
int main(void)
{
 /* Initialize ordinary XRT/AST allocation state before balance assertions. */
 XL6_AstNode* root=xl6AstNew(XL6_AST_MODULE,NULL);if(!root)return 1;
 xls_typed_module* warm=xlsTypedModuleTake(root);if(!warm){xl6AstFree(root);return 1;}xlsTypedModuleRelease(warm);
 if(!identities() || !faults())return 1;
 printf("analysis_typed_identity_test passed: %u checks; independent owner identity,root lifetime,address reuse resistance,optional allocation failure,allocation balance\n",checks);return 0;
}
