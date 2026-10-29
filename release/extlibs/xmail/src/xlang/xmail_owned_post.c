/* Native handoff data is owned until the resident task Drop, after every
 * package callback has returned. The constructor's completion Promise moves
 * into this record; callback arguments remain caller-borrowed until publish. */
#include <xl_foreign_graph.h>
#include <stdlib.h>
typedef struct xmail_owned_post {
    xnettaskproc proc;
    void* data;
    xpromise* promise;
    void (*discard)(void*);
} xmail_owned_post;
static bool xmail_post_trace(const void* data,xrtownershipvisitor visit,void* context)
{
    const xmail_owned_post* post=data;
    return !post->promise||visit(xrtPromiseOwnership(post->promise),context);
}
static void xmail_post_task(xnetworker* worker,void* data)
{
    xmail_owned_post* post=data;
    xrtownershipscope mutation={0};
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    /* The callback now owns the creator through its original borrowed slot.
     * No snapshot may read this record while its native Task is active. */
    post->promise=NULL;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    post->proc(worker,post->data);
}
void xmail_post_destroy(void* data)
{
    xmail_owned_post* post=data;if(!post)return;
    xpromise* promise=post->promise;post->promise=NULL;
    if(promise)post->discard(post->data);
    xrtFree(post);
    if(promise){(void)xrtPromiseClose(promise);xrtPromiseDestroy(promise);}
}
static bool xmail_post_owned(xnetworker* worker,xnettaskproc proc,void* data,
    xpromise* promise,void (*discard)(void*),void* context)
{
    static const XL6_ForeignGraphDescriptorV1 descriptor={sizeof(descriptor),
        XL6_FOREIGN_GRAPH_V1_TAG,xmail_post_trace,NULL,xmail_post_task};
    xmail_owned_post* post=xrtCalloc(1,sizeof(*post));
    if(!post){
        xerror* error=xrtTakeError();discard(data);xrtPromiseDestroy(promise);
        xrtClearError();xrtSetErrorTake(error);return false;
    }
    post->proc=proc;post->data=data;post->promise=promise;post->discard=discard;
    void* record=xl6ForeignGraphCreateV1(post,&descriptor,context);
    if(!record){
        xerror* error=xrtTakeError();xmail_post_destroy(post);
        xrtClearError();xrtSetErrorTake(error);return false;
    }
    if(xrtNetEnginePostOwnedV1(xrtNetWorkerEngine(worker),xrtNetWorkerIndex(worker),
        record,xl6ForeignGraphTaskPolicyV1Get()))return true;
    xerror* error=xrtTakeError();xl6ForeignGraphDestroyV1(record);
    xrtClearError();xrtSetErrorTake(error);return false;
}
