/* Independent heap-RC node admission; no language or native graph is flattened.
 * This kernel tests frozen queries. Actual Cell retirement is a separate gate. */
#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_foreign_view.h"
#include <assert.h>
#include <stdio.h>

typedef struct Node { size_t references; } Node;
typedef struct Payload { Node* node; } Payload;
static unsigned queriesA,queriesB,counts,traces;
static bool readB=true,dirtyB,sameB;
static bool count(const void* raw,size_t* output)
{++counts;*output=((const Node*)raw)->references;return true;}
static bool trace(const void* raw,xrtownershipvisitor visit,void* user)
{(void)raw;(void)visit;(void)user;++traces;return true;}
static const xrtownershipops ops={count,trace},unknownOps={count,trace};
static bool hold(const void* raw)
{Node* n=(Node*)raw;if(n->references==SIZE_MAX)return false;++n->references;return true;}
static void drop(const void* raw)
{Node* n=(Node*)raw;assert(n->references);if(!--n->references)xrtFree(n);}
static bool claim(const void* raw,const void* token){return raw && token;}
static void restore(const void* raw,const void* token){assert(raw && token);}
static const xrtownershipadapterv1 adapterA={sizeof(adapterA),hold,drop,claim,restore,NULL,restore,NULL};
static const xrtownershipadapterv1 adapterB={sizeof(adapterB),hold,drop,claim,restore,NULL,restore,NULL};
static bool resolveA(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
 const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** finish)
{
 (void)preparation;(void)finish;assert(!policies);++queriesA;
 if(ref.Ops!=&ops || !ref.Data)return false;
 *adapter=&adapterA;return true;
}
static bool resolveB(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
 const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** finish)
{
 (void)preparation;(void)finish;assert(!policies);++queriesB;
 if(ref.Ops!=&ops || !ref.Data){if(dirtyB)*adapter=&adapterB;return false;}
 *adapter=sameB ? &adapterA:&adapterB;return true;
}
static bool admitsA(xrtownershipref ref){return ref.Ops==&ops && ref.Data;}
static bool admitsB(xrtownershipref ref){return readB && admitsA(ref);}
static const XL6_OwnershipDomainV1 domainA={sizeof(domainA),NULL,resolveA,admitsA};
static const XL6_OwnershipDomainV1 domainB={sizeof(domainB),NULL,resolveB,admitsB};
static bool destroy(void* raw){Payload* p=raw;drop(p->node);xrtFree(p);return true;}
static bool tracePayload(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{return visit((xrtownershipref){((const Payload*)raw)->node,&ops},user);}
static const XL6_ForeignViewDescriptorV1 descriptor={sizeof(descriptor),XL6_FOREIGN_VIEW_V1_TAG,
 "query.rc.payload",0,1,NULL,NULL,destroy,0,1,NULL,NULL,tracePayload,NULL};
static XL6_ForeignView* create(Node* node,const XL6_OwnershipDomainV1* domain)
{
 Payload* p=xrtCalloc(1,sizeof(*p));assert(p && hold(node));p->node=node;
 XL6_ForeignView* view=xl6ForeignViewCreateRootWithDomainV1(p,&descriptor,domain,NULL);assert(view);return view;
}
static void query(xrtownershipref ref,bool expected,bool readable,unsigned expectedA,unsigned expectedB)
{
 const XL6_OwnershipDomainV1* resident=xl6OwnershipDomainForeignViewV1Get();
 const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* finish=NULL;
 xerror* previous=xrtErrorCreate(XERR_STATE,"test",0,"existing error");assert(previous);xrtSetErrorTake(previous);
 xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
 xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
 assert(xrtMemDebugFailAfter(0));queriesA=queriesB=0;
 assert(resident->Resolve(ref,NULL,&adapter,&preparation,&finish)==expected);
 assert(queriesA<=expectedA && queriesB<=expectedB);
 if(expected)assert(queriesA==expectedA && queriesB==expectedB);
 assert(expected ? adapter==&adapterA : adapter==NULL);
 assert(!preparation && !finish);queriesA=queriesB=0;
 assert(resident->AdmitsRead(ref)==readable);
 assert(queriesA<=expectedA && queriesB<=expectedB);
 if(readable)assert(queriesA==expectedA && queriesB==expectedB);
 assert(!counts && !traces && xrtGetError()==previous);
 assert(xrtMemDebugFailAfter(-1));assert(xrtOwnershipScopeEnd(&freeze));xrtMemDebugSnapshot(&after);
 assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);
 xrtClearError();
}
int main(void)
{
 Node* node=xrtCalloc(1,sizeof(*node));assert(node);node->references=1;
 XL6_ForeignView* a=create(node,&domainA);XL6_ForeignView* duplicate=create(node,&domainA);
 query((xrtownershipref){node,&ops},true,true,1,0);
 query((xrtownershipref){node,&unknownOps},false,false,1,0);
 XL6_ForeignView* b=create(node,&domainB);
 query((xrtownershipref){node,&ops},false,false,1,1);
 sameB=true;query((xrtownershipref){node,&ops},true,true,1,1);
 readB=false;query((xrtownershipref){node,&ops},true,false,1,1);
 dirtyB=true;query((xrtownershipref){node,&unknownOps},false,false,1,1);
 assert(xl6ForeignViewReleaseV1(b));
 query((xrtownershipref){node,&ops},true,true,1,0);
 assert(xl6ForeignViewReleaseV1(duplicate) && xl6ForeignViewReleaseV1(a));
 assert(node->references==1);drop(node);assert(!xl6ForeignViewPendingV1(0) && !xrtGetError());
 puts("native domain: exact dedup/conflicts/read refusal/unknown Ops/first diagnostic/OOM no allocation PASS");
 return 0;
}
