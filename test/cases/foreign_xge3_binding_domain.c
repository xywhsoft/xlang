#include "test/cases/foreign_xge3_binding_domain.h"
#include <assert.h>
typedef struct BindingDomainNode { unsigned refs; } BindingDomainNode;
struct BindingDomainRoot { BindingDomainNode child; int value; };
static BindingDomainRoot* current;
static bool count(const void* raw,size_t* result)
{const BindingDomainNode* node=raw;if(!node || !result)return false;*result=node->refs;return true;}
static bool trace(const void* raw,xrtownershipvisitor visit,void* user)
{(void)raw;(void)visit;(void)user;return true;}
static const xrtownershipops nodeOps={count,trace};
static bool hold(const void* raw){++((BindingDomainNode*)raw)->refs;return true;}
static void drop(const void* raw){BindingDomainNode* node=(BindingDomainNode*)raw;assert(node->refs);--node->refs;}
/* The fixture exercises read admission, never commits lifecycle retirement of
 * this independently counted child; missing claim/clear capabilities are exact. */
static const xrtownershipadapterv1 nodeAdapter={sizeof(nodeAdapter),hold,drop,NULL,NULL,NULL,NULL,NULL};
static bool resolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
 const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** finish)
{
 (void)policies;(void)preparation;(void)finish;
 if(ref.Ops!=&nodeOps || !ref.Data)return false;
 *adapter=&nodeAdapter;return true;
}
static bool admits(xrtownershipref ref)
{return ref.Ops==&nodeOps && ref.Data;}
const XL6_OwnershipDomainV1 bindingNativeDomain={sizeof(bindingNativeDomain),NULL,resolve,admits};
static bool traceRoot(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{const BindingDomainRoot* root=raw;return visit((xrtownershipref){&root->child,&nodeOps},user);}
static bool destroy(void* raw){binding_domain_destroy(raw);return true;}
const XL6_ForeignViewDescriptorV1 bindingDomainRootPolicy={
 sizeof(bindingDomainRootPolicy),XL6_FOREIGN_VIEW_V1_TAG,"binding.domain.root",
 0,1,NULL,NULL,destroy,0,1,NULL,NULL,traceRoot,NULL};
int binding_domain_create(BindingDomainRoot** output)
{
 if(!output || current)return -1;
 BindingDomainRoot* root=xrtCalloc(1,sizeof(*root));if(!root)return -1;
 root->child.refs=1;root->value=37;current=root;*output=root;return 0;
}
void binding_domain_destroy(BindingDomainRoot* root)
{if(!root)return;assert(root==current && root->child.refs==1);current=NULL;xrtFree(root);}
int binding_domain_read(BindingDomainRoot* root){return root->value;}
int binding_domain_live(void){return current!=NULL;}
int binding_domain_check(void)
{
 if(!current)return 0;
 const XL6_OwnershipDomainV1* resident=xl6OwnershipDomainForeignViewV1Get();
 const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* finish=NULL;
 xrtownershipref ref={&current->child,&nodeOps};
 if(!resident->Resolve(ref,NULL,&adapter,&preparation,&finish) || adapter!=&nodeAdapter || preparation || finish || !resident->AdmitsRead(ref))return 0;
 return 1;
}
