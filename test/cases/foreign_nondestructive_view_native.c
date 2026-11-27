/* Native authority for a root-owned node with no independent deletion API.
 * --baseline only verifies rejection, before an observer dereferences payload.
 * The ordinary mode specifies the proposed capability; do not claim it passes
 * until a matching resident runtime has been qualified. */
#include <__xl_runtime/xl6_foreign_view.h>
#include <__xl_runtime/xrt_decl.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct Link Link;
struct Link {Link* next;Link** previous;void(*invalidate)(void*);void* user;};
typedef struct Node {Link* links;int value;} Node;
typedef struct Parent {unsigned refs;Node child;} Parent;
static unsigned parents,drops,watches,unwatches;
static void unwatch(void* storage)
{
    Link* link=storage;
    if(link->previous){*link->previous=link->next;if(link->next)link->next->previous=link->previous;++unwatches;}
    *link=(Link){0};
}
static void invalidate(Node* node)
{
    while(node->links){Link* link=node->links;void(*notify)(void*)=link->invalidate;void* user=link->user;
        unwatch(link);notify(user);}
}
static bool watch(void* raw,void* storage,void(*notify)(void*),void* user)
{
    Node* node=raw;Link* link=storage;
    assert(!link->next && !link->previous && !link->invalidate && !link->user);
    link->next=node->links;link->previous=&node->links;link->invalidate=notify;link->user=user;
    if(link->next)link->next->previous=&link->next;node->links=link;++watches;return true;
}
static Parent* create(void)
{Parent* p=xrtCalloc(1,sizeof(*p));assert(p);p->refs=2;p->child.value=7;++parents;return p;}
static void drop(Parent* p)
{assert(p && p->refs);++drops;if(!--p->refs){invalidate(&p->child);--parents;xrtFree(p);}}
static bool destroy(void* raw){drop(raw);return true;}
static const XL6_ForeignViewDescriptorV1 parentPolicy={
    .size=sizeof(parentPolicy),.abiTag=XL6_FOREIGN_VIEW_V1_TAG,.family="nondestructive.parent",
    .watchAlignment=1,.Destroy=destroy,.callAlignment=1
};
static const XL6_ForeignViewDescriptorV1 childPolicy={
    .size=sizeof(childPolicy),.abiTag=XL6_FOREIGN_VIEW_V1_TAG,.family="nondestructive.node",
    .watchBytes=sizeof(Link),.watchAlignment=_Alignof(Link),.Watch=watch,.Unwatch=unwatch,.callAlignment=1
};
static XL6_ForeignView* child(XL6_ForeignView* owner,Parent* p)
{
    XL6_ForeignViewCallV1 scope={0};assert(xl6ForeignViewCallBeginV1(owner,&scope,NULL,0));
    XL6_ForeignView* result=xl6ForeignViewBorrowChildV1(&p->child,owner,&childPolicy,NULL);
    assert(xl6ForeignViewCallEndV1(&scope));return result;
}
static void check(bool baseline)
{
    Parent* p=create();XL6_ForeignView* owner=xl6ForeignViewCreateRootV1(p,&parentPolicy,NULL);assert(owner);
    unsigned observed=watches;XL6_ForeignView* node=child(owner,p);
    if(baseline){
        assert(!node && watches==observed);xrtClearError();
        assert(xl6ForeignViewCloseV1(owner));assert(xl6ForeignViewReleaseV1(owner));
        assert(p->refs==1 && !p->child.links && p->child.value==7);drop(p);return;
    }
    assert(node && p->refs==2 && watches==observed+1);
    for(unsigned i=0;i<1000;++i){XL6_ForeignView* again=child(owner,p);assert(again==node);
        assert(xl6ForeignViewReleaseV1(again));assert(p->refs==2 && watches==observed+1);}
    assert(xl6ForeignViewCloseV1(node));assert(!p->child.links && p->child.value==7 && p->refs==2);
    XL6_ForeignViewCallV1 expired={0};assert(!xl6ForeignViewCallBeginV1(node,&expired,NULL,0));xrtClearError();
    assert(xl6ForeignViewReleaseV1(node));node=child(owner,p);assert(node);
    /* Releasing one native root reference leaves the external owner's node
     * alive. All observations belonging to this language root must detach. */
    assert(xl6ForeignViewCloseV1(owner));assert(p->refs==1 && !p->child.links && p->child.value==7);
    assert(!xl6ForeignViewCallBeginV1(node,&expired,NULL,0));xrtClearError();
    assert(xl6ForeignViewReleaseV1(node));assert(xl6ForeignViewReleaseV1(owner));
    drop(p);
}
int main(int argc,char** argv)
{
    bool baseline=argc==2 && !strcmp(argv[1],"--baseline");
    check(baseline);xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);check(baseline);xrtMemDebugSnapshot(&after);
    assert(!parents && before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
    printf("%s: native drops=%u, watch=%u, unwatch=%u, exact warm heap baseline\n",
        baseline ? "Old rejection control":"Non-destructive child",drops,watches,unwatches);return 0;
}
