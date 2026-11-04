#ifndef XL6_TEST_FOREIGN_VIEW_NATIVE_FIXTURE_H
#define XL6_TEST_FOREIGN_VIEW_NATIVE_FIXTURE_H
#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_foreign_view.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/runtime/xl6_closure.h"
#include "src/runtime/xl6_cell.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct Watch {
    void (*invalidate)(void*);
    void* user;
    struct Watch** slot;
} Watch;
typedef struct Native Native;
struct Native {
    Watch* watch;
    Native* root;
    Native* children;
    Native* next;
    XL6_ForeignView* capture;
    XL6_Closure* callback;
    XL6_Cell* cell;
    xfuture* future;
    xvalue* object;
    unsigned depth, failures;
    bool closing, failWithoutDiagnostic, failEnd;
};
typedef struct NativeScope { struct NativeScope* self; Native* root; } NativeScope;
static unsigned nativeDestroyed, childDestroyed;
static unsigned codeReferences=1;
static int codeAcquire(void* data) { (void)data; ++codeReferences; return 1; }
static void codeRelease(void* data) { (void)data; assert(codeReferences>1); --codeReferences; }
static const XL6_CodeOwner codeOwner={sizeof(codeOwner),NULL,codeAcquire,codeRelease};

static Native* native_create(void)
{ Native* n=xrtCalloc(1,sizeof(*n)); assert(n); n->root=n; return n; }
static Native* native_child(Native* root)
{
    Native* n=xrtCalloc(1,sizeof(*n)); assert(n);
    n->root=root; n->next=root->children; root->children=n; return n;
}
static bool watch(void* raw,void* storage,void (*invalidate)(void*),void* user)
{
    Native* n=raw; Watch* w=storage;
    if (n->closing || n->watch || w->slot || w->invalidate || w->user) return false;
    w->slot=&n->watch; w->invalidate=invalidate; w->user=user; n->watch=w; return true;
}
static void unwatch(void* storage)
{
    Watch* w=storage;
    if (w->slot) *w->slot=NULL;
    *w=(Watch){0};
}
static void invalidate_native(Native* n)
{
    if (n->closing) return;
    n->closing=true;
    if (n->watch) {
        Watch* w=n->watch; void (*fn)(void*)=w->invalidate; void* user=w->user;
        unwatch(w); fn(user);
    }
}
static void flush_native(Native* root)
{
    if (root->depth) return;
    Native** slot=&root->children;
    while (*slot) {
        Native* n=*slot;
        if (root->closing || n->closing) { *slot=n->next; ++childDestroyed; xrtFree(n); }
        else slot=&n->next;
    }
    if (root->closing) {
        XL6_ForeignView* capture=root->capture;
        XL6_Closure* callback=root->callback;
        XL6_Cell* cell=root->cell;
        xfuture* future=root->future;
        xvalue* object=root->object;
        root->capture=NULL; ++nativeDestroyed; xrtFree(root);
        xl6ForeignViewReleaseV1(capture);
        xl6ClosureRuntimeV1Get()->Release(callback);
        xl6CellRuntimeV1Get()->Release(cell);
        xrtFutureDestroy(future);
        xrtValueRelease(object);
    }
}
static bool scope_begin(void* raw,void* storage)
{
    Native* root=raw; NativeScope* s=storage;
    if (root->closing || s->self || s->root) return false;
    s->self=s; s->root=root; ++root->depth; return true;
}
static bool scope_end(void* storage)
{
    NativeScope* s=storage;
    assert(s->self==s && s->root && s->root->depth);
    Native* root=s->root; bool fail=root->failEnd; root->failEnd=false;
    *s=(NativeScope){0}; --root->depth; flush_native(root);
    if (fail) { xrtSetErrorKind(XERR_RANGE); return false; }
    return true;
}
static bool destroy_native(void* raw)
{
    Native* n=raw; Native* root=n->root;
    if (n->failures) {
        --n->failures;
        if (!n->failWithoutDiagnostic) xrtSetErrorKind(XERR_VALUE);
        return false;
    }
    invalidate_native(n);
    if (n==root) for (Native* child=root->children; child; child=child->next) invalidate_native(child);
    flush_native(root); return true;
}
static bool trace_native(const void* raw,xrtownershipvisitor visit,void* context)
{
    const Native* n=raw;
    return (!n->capture || visit(xl6ForeignViewOwnershipV1(n->capture),context)) &&
        (!n->callback || visit(xl6ClosureRuntimeV1Get()->Ownership(n->callback),context)) &&
        (!n->cell || visit(xl6CellRuntimeV1Get()->Ownership(n->cell),context)) &&
        (!n->future || visit(xrtFutureOwnership(n->future),context)) &&
        (!n->object || visit(xrtValueOwnership(n->object),context));
}
static bool busy_native(const void* raw)
{return ((const Native*)raw)->depth!=0;}
static const XL6_ForeignViewDescriptorV1 rootDescriptor={
    sizeof(rootDescriptor),XL6_FOREIGN_VIEW_V1_TAG,"native.test.root",
    sizeof(Watch),_Alignof(Watch),watch,unwatch,destroy_native,
    sizeof(NativeScope),_Alignof(NativeScope),scope_begin,scope_end,trace_native,busy_native,0,NULL,NULL};
static const XL6_ForeignViewDescriptorV1 childDescriptor={
    sizeof(childDescriptor),XL6_FOREIGN_VIEW_V1_TAG,"native.test.child",
    sizeof(Watch),_Alignof(Watch),watch,unwatch,destroy_native,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};


#endif
