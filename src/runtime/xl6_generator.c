#define XRT_MODULE_MEMORY
#define XRT_MODULE_ATOMIC
#include "src/common/xl6_xrt.h"
#include "src/api/xl_foreign_error.h"
#include "xl6_generator.h"
#include "xl6_ownership_scope.h"
#include <string.h>

typedef struct XL6_GeneratorFrame {
    XL6_Generator* owner; /* borrowed, attached until the single frame detach */
    XL6_CodeContextTrace trace;
    const XL6_ActivationCaptureLayoutV1* layout;
    const XL6_ActivationCaptureLayoutV2* layoutV2;
    XL6_CodeLease* lease;
    volatile int32_t references;
    const void* claim;
    size_t bytes;
    bool published, active, payloadLive, cleared;
    max_align_t alignment;
    unsigned char data[];
} XL6_GeneratorFrame;
/* Private allocation prefix preserves the public V1 control layout. A V1
 * control stays uniquely owned; only explicit V2 creation enables real pins. */
typedef struct GeneratorControl {
    volatile int32_t references;
    const XL6_CaptureLayoutV1* itemLayout;
    const XL6_CaptureLayoutV2* itemLayoutV2;
    const void* claim;
    bool cleared;
    max_align_t alignment;
    unsigned char data[];
} GeneratorControl;
static GeneratorControl* control_header(const XL6_Generator* generator)
{ return (GeneratorControl*)((unsigned char*)generator - offsetof(GeneratorControl,data)); }
static const xrtownershipops frame_managed_ops, generator_managed_ops;
static bool frame_managed(const XL6_GeneratorFrame* f) { return f->layout || f->layoutV2; }
static bool control_managed(const GeneratorControl* c) { return c->itemLayout || c->itemLayoutV2; }
static bool frame_layout_valid(const XL6_GeneratorFrame* f)
{
    return f->layoutV2 ? !f->layout && xl6ActivationCaptureRuntimeV2Get()->Validate(f->layoutV2,f->bytes) :
        xl6ActivationCaptureRuntimeV1Get()->Validate(f->layout,f->bytes);
}
static bool item_layout_valid(const GeneratorControl* c,size_t bytes)
{
    return c->itemLayoutV2 ? !c->itemLayout && xl6CaptureRuntimeV2Get()->Validate(c->itemLayoutV2,bytes) :
        xl6CaptureRuntimeV1Get()->Validate(c->itemLayout,bytes);
}
static void frame_discard(void*);
static void frame_release(void*);
static void generator_destroy(XL6_Generator*);
static XL6_GeneratorFrame* frame_header(const void* data)
{ return (XL6_GeneratorFrame*)((unsigned char*)data - offsetof(XL6_GeneratorFrame,data)); }
static xerror* generator_error(xerrkind kind, const char* name, const char* message)
{
    xerrordesc desc={kind,0,0,"xlang","throw",message,name,NULL};
    xerrorlocation location={"",0,0}; xerror* error=xrtErrorBuildAt(&desc,&location);
    if (!error) { xrtSetErrorKind(XERR_MEMORY); error=xrtTakeError(); }
    return error;
}
static void* frame_allocate_body(size_t size)
{
    XL6_GeneratorFrame* frame;
    if (!size || size>SIZE_MAX-sizeof(*frame)) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    frame=xrtMalloc(sizeof(*frame)+size); if (!frame) return NULL;
    *frame=(XL6_GeneratorFrame){0}; frame->references=1;frame->bytes=size;frame->payloadLive=true;
    return frame->data;
}
static void* frame_allocate(size_t size)
{ XL6_OWNERSHIP_RETURN(void*,frame_allocate_body(size)); }
static void frame_free_body(void* data)
{
    if (!data) return;
    if (frame_header(data)->owner || frame_managed(frame_header(data))) abort();
    xrtFree(frame_header(data));
}
static void frame_free(void* data)
{ XL6_OWNERSHIP_RETURN_VOID(frame_free_body(data)); }
static bool generator_count(const void* data,size_t* count)
{
    const XL6_Generator* generator=data;
    if (!generator || !count || generator->running) return false;
    const GeneratorControl* control=control_header(generator);
    if(control_managed(control)){
        if(control->references<=0||control->cleared)return false;
        *count=(size_t)control->references;return true;
    }
    *count=1; return true;
}
static bool frame_count(const void* data,size_t* count)
{
    const XL6_GeneratorFrame* frame=data;
    if(frame&&frame_managed(frame)){
        if(!count||frame->references<=0||!frame->published||frame->active||frame->cleared||
            (frame->owner&&frame->owner->running))return false;
        *count=(size_t)frame->references;return true;
    }
    if (!frame || !count || !frame->owner || frame->owner->frame!=frame->data || frame->owner->running) return false;
    *count=1; return true;
}
static bool frame_trace(const void* data,xrtownershipvisitor visit,void* context)
{
    const XL6_GeneratorFrame* frame=data; size_t count;
    if(!visit||!frame_count(data,&count))return false;
    if(frame_managed(frame))return (!frame->lease||visit(xl6CodeLeaseOwnership(frame->lease),context))&&
        (!frame->payloadLive||(frame->layoutV2 ?
            xl6ActivationCaptureRuntimeV2Get()->Trace(frame->layoutV2,frame->data,frame->bytes,visit,context) :
            xl6ActivationCaptureRuntimeV1Get()->Trace(frame->layout,frame->data,visit,context)));
    return frame->trace&&frame->trace(frame->data,visit,context);
}
static const xrtownershipops frame_ops={frame_count,frame_trace};
static const xrtownershipops frame_managed_ops={frame_count,frame_trace};
static bool generator_trace(const void* data,xrtownershipvisitor visit,void* context)
{
    const XL6_Generator* generator=data; size_t count;
    if (!visit || !generator_count(data,&count)) return false;
    if (generator->lease && !visit(xl6CodeLeaseOwnership(generator->lease),context)) return false;
    if (generator->frame){const XL6_GeneratorFrame* frame=frame_header(generator->frame);
        if(!visit((xrtownershipref){frame,frame_managed(frame)?&frame_managed_ops:&frame_ops},context))return false;}
    const GeneratorControl* control=control_header(generator);
    if(control->itemLayoutV2)return !generator->has_item||xl6CaptureRuntimeV2Get()->Trace(control->itemLayoutV2,generator->item,generator->item_size,visit,context);
    if(control->itemLayout)return !generator->has_item||xl6CaptureRuntimeV1Get()->Trace(control->itemLayout,generator->item,visit,context);
    return !generator->has_item || (generator->item_trace && generator->item_trace(generator->item,visit,context));
}
static const xrtownershipops generator_ops={generator_count,generator_trace};
static const xrtownershipops generator_managed_ops={generator_count,generator_trace};
static xrtownershipref generator_ownership(const XL6_Generator* generator)
{ return (xrtownershipref){generator,generator ? (control_managed(control_header(generator))?&generator_managed_ops:&generator_ops) : NULL}; }
static XL6_Generator* generator_create_body(const XL6_CodeOwner* owner,void* data,XL6_GeneratorResume resume,
    XL6_GeneratorFree drop,size_t item_size,XL6_GeneratorFree item_drop,XL6_CodeContextTrace trace,XL6_CodeContextTrace item_trace,
    const XL6_CaptureLayoutV1* item_layout,const XL6_CaptureLayoutV2* item_layout_v2)
{
    XL6_Generator* generator; GeneratorControl* control;XL6_CodeLease* lease=NULL; XL6_CodeLeaseAdmission admission;
    if (!data || !resume || !item_size || item_size>SIZE_MAX-sizeof(*generator)-sizeof(*control) ||
        frame_header(data)->owner || frame_header(data)->published ||
        frame_header(data)->references!=1 || frame_header(data)->active ||
        frame_header(data)->cleared || !frame_header(data)->payloadLive ||
        (item_layout_v2 ? item_layout || !frame_header(data)->layoutV2 || !frame_layout_valid(frame_header(data)) ||
            !xl6CaptureRuntimeV2Get()->Validate(item_layout_v2,item_size) ||
            item_layout_v2->count!=1 || item_layout_v2->slots[0].value.offset!=0 || item_layout_v2->slots[0].value.bytes!=item_size :
        item_layout ? !frame_header(data)->layout || frame_header(data)->layoutV2 || !xl6CaptureRuntimeV1Get()->Validate(item_layout,item_size) ||
            !frame_layout_valid(frame_header(data)) ||
            item_layout->count!=1 || item_layout->slots[0].offset!=0 || item_layout->slots[0].bytes!=item_size :
            frame_managed(frame_header(data)) || !drop || !trace || !item_trace)) {
        xrtSetErrorTake(generator_error(XERR_STATE,"RuntimeError","invalid generator machine ABI")); return NULL;
    }
    admission=xl6CodeLeaseAcquireScoped(owner,&lease);
    if (admission!=XL6_CODE_LEASE_ACQUIRED) {
        if (!xrtGetError()) xrtSetErrorKind(admission==XL6_CODE_LEASE_CLOSED?XERR_CLOSED:
            admission==XL6_CODE_LEASE_RESOURCE_FAILURE?XERR_MEMORY:XERR_ARGUMENT);
        return NULL;
    }
    control=xrtMalloc(sizeof(*control)+sizeof(*generator)+item_size);
    if (!control) {
        xl6CodeLeaseRelease(lease);
        xrtSetErrorTake(generator_error(XERR_MEMORY,"MemoryError","generator allocation failed")); return NULL;
    }
    *control=(GeneratorControl){0};control->references=1;control->itemLayout=item_layout;control->itemLayoutV2=item_layout_v2;
    generator=(XL6_Generator*)control->data;
    *generator=(XL6_Generator){0}; generator->frame=data; generator->resume=resume;
    generator->frame_free=drop; generator->item_free=item_drop; generator->item_trace=item_trace;
    generator->lease=lease; generator->item_size=item_size;
    frame_header(data)->trace=trace; frame_header(data)->owner=generator;
    if(item_layout || item_layout_v2){frame_header(data)->lease=xl6CodeLeaseRetain(lease);frame_header(data)->published=true;}
    return generator;
}
static XL6_Generator* generator_create(const XL6_CodeOwner* owner,void* data,XL6_GeneratorResume resume,
    XL6_GeneratorFree drop,size_t size,XL6_GeneratorFree item_drop,XL6_CodeContextTrace trace,XL6_CodeContextTrace item_trace)
{ XL6_OWNERSHIP_RETURN(XL6_Generator*,generator_create_body(owner,data,resume,drop,size,item_drop,trace,item_trace,NULL,NULL)); }

/* Only transitions are mutations. The resident scope keeps callback code alive
 * while the actual source body, cleanup or native call may wait or park. */
static bool generator_begin_body(XL6_Generator* generator,XL6_CodeScope* scope,bool closing)
{
    GeneratorControl* control=control_header(generator);
    if (generator->running || control->cleared) {
        xrtSetErrorTake(generator_error(XERR_STATE,"RuntimeError","generator is already running")); return false;
    }
    if (control_managed(control)) {
        if (control->references<=0 || control->references==INT32_MAX) {
            xrtSetErrorKind(XERR_STATE); return false;
        }
        /* The executing resident call owns a real reference. Reentrant drops
         * may release other owners, but cannot retire this invocation's data. */
        if (xrtRefRetain(&control->references)<=1) abort();
    }
    xl6CodeScopeEnter(scope,generator->lease); generator->running=true;
    if (closing) generator->closing=true;
    return true;
}
static bool generator_begin(XL6_Generator* generator,XL6_CodeScope* scope,bool closing)
{ XL6_OWNERSHIP_RETURN(bool,generator_begin_body(generator,scope,closing)); }
static void generator_end_body(XL6_Generator* generator)
{ if (!generator->running) abort(); generator->running=false; }
static void generator_end(XL6_Generator* generator,XL6_CodeScope* scope)
{
    xrtownershipscope mutation={0};
    if (!xrtOwnershipMutationBegin(&mutation)) abort();
    generator_end_body(generator);
    if (!xrtOwnershipScopeEnd(&mutation)) abort();
    if (control_managed(control_header(generator))) generator_destroy(generator);
    xl6CodeScopeLeave(scope);
}
static bool generator_put_body(XL6_Generator* generator,const void* item,size_t size)
{
    if (!generator || !generator->running || !item || generator->has_item || size!=generator->item_size) {
        xrtSetErrorTake(generator_error(XERR_STATE,"RuntimeError","generator yielded an invalid item ABI")); return false;
    }
    memcpy(generator->item,item,size); generator->has_item=true; return true;
}
static bool generator_put(XL6_Generator* generator,const void* item,size_t size)
{ XL6_OWNERSHIP_RETURN(bool,generator_put_body(generator,item,size)); }
static void generator_state_body(XL6_Generator* generator,uint32_t state)
{ if (!generator || !generator->running) abort(); generator->state=state; }
static void generator_state(XL6_Generator* generator,uint32_t state)
{ XL6_OWNERSHIP_RETURN_VOID(generator_state_body(generator,state)); }
static void* generator_detach_body(XL6_Generator* generator)
{
    void* frame=generator->frame;
    if (!generator->running) abort();
    generator->done=true; generator->frame=NULL;
    if (frame) frame_header(frame)->owner=NULL;
    return frame;
}
static void* generator_detach(XL6_Generator* generator)
{ XL6_OWNERSHIP_RETURN(void*,generator_detach_body(generator)); }
static void generator_finish(XL6_Generator* generator)
{
    void* frame=generator_detach(generator);
    if (frame){if(frame_managed(frame_header(frame)))frame_discard(frame);else generator->frame_free(frame);}
}
static bool item_owned(const XL6_Generator* generator)
{
    const XL6_CaptureLayoutV2* v2=control_header(generator)->itemLayoutV2;
    if(v2)return v2->slots[0].value.kind!=XL6_CAPTURE_TRIVIAL;
    const XL6_CaptureLayoutV1* layout=control_header(generator)->itemLayout;
    return layout?layout->slots[0].kind!=XL6_CAPTURE_TRIVIAL:generator->item_free!=NULL;
}
static void item_release(XL6_Generator* generator)
{
    const XL6_CaptureLayoutV2* v2=control_header(generator)->itemLayoutV2;
    if(v2){
        xl6_error_result r=xl6CaptureRuntimeV2Get()->Cleanup(v2,generator->item,generator->item_size,(xl6_error_result){true,NULL});
        if(!r.ok)xl6ErrorScopePropagateFailure(r);
        return;
    }
    const XL6_CaptureLayoutV1* layout=control_header(generator)->itemLayout;
    if(layout)xl6CaptureRuntimeV1Get()->Cleanup(layout,generator->item);
    else if(generator->item_free)generator->item_free(generator->item);
}
static bool generator_item_detach_body(XL6_Generator* generator)
{
    bool present=generator->has_item;
    if (!generator->running) abort();
    generator->has_item=false; return present;
}
static bool generator_item_detach(XL6_Generator* generator)
{ XL6_OWNERSHIP_RETURN(bool,generator_item_detach_body(generator)); }
static bool generator_advance(XL6_Generator* generator)
{
    bool yielded;
    if (generator_item_detach(generator)) item_release(generator);
    if (generator->done || !generator->frame) return false;
    yielded=generator->resume(generator);
    if (yielded!=generator->has_item) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,generator_error(XERR_STATE,"RuntimeError","generator resume violated its item ABI")});
        /* Retain any frame for the verified close path, as before. */
        xrtownershipscope mutation={0}; if (!xrtOwnershipMutationBegin(&mutation)) abort();
        generator->done=true; if (!xrtOwnershipScopeEnd(&mutation)) abort(); return false;
    }
    return yielded;
}
static XL6_GeneratorStep generator_next(XL6_Generator* generator)
{
    xl6_error_scope boundary={0}; xl6_error_result outcome; XL6_CodeScope code={0};
    XL6_GeneratorStep step={false,false};
    if (!xl6ErrorScopeBegin(&boundary)) abort();
    if (!generator)
        xl6ErrorScopePropagateFailure((xl6_error_result){false,generator_error(XERR_TYPE,"TypeError","iterate null generator")});
    else if (generator_begin(generator,&code,false)) {
        step.value=generator_advance(generator); generator_end(generator,&code);
    } else xl6ErrorScopeCaptureFailure(&boundary);
    outcome=xl6ErrorScopeEnd(&boundary); step.ok=outcome.ok; xrtSetErrorTake(outcome.error); return step;
}
static bool generator_item_get_body(XL6_Generator* generator,void* output,size_t size,bool take)
{
    if (!generator || !output || generator->running || control_header(generator)->cleared ||
        !generator->has_item || size!=generator->item_size || take!=item_owned(generator)) {
        if (output && size) memset(output,0,size);
        xrtSetErrorTake(generator_error(XERR_STATE,"RuntimeError","generator item transfer violated its ownership ABI")); return false;
    }
    memcpy(output,generator->item,size); if (take) generator->has_item=false; return true;
}
static bool generator_item_get(XL6_Generator* generator,void* output,size_t size,bool take)
{ XL6_OWNERSHIP_RETURN(bool,generator_item_get_body(generator,output,size,take)); }
static void generator_close_item(XL6_Generator* generator,xl6_error_scope* closing)
{
    xl6_error_scope step={0}; xl6_error_result outcome;
    if (!generator_item_detach(generator)) return;
    if (!xl6ErrorScopeBegin(&step)) abort();
    item_release(generator);
    outcome=xl6ErrorScopeEnd(&step);
    if (!outcome.ok) { xrtSetErrorTake(outcome.error); xl6ErrorScopeCaptureFailure(closing); }
}
static xl6_error_result generator_close_active(XL6_Generator* generator)
{
    xl6_error_scope closing={0};
    if (!xl6ErrorScopeBeginCleanup(&closing,false)) abort();
    generator_close_item(generator,&closing);
    while (generator->frame && !generator->done && generator->state) {
        xl6_error_scope step={0}; xl6_error_result outcome; bool yielded;
        if (!xl6ErrorScopeBegin(&step)) abort();
        yielded=generator->resume(generator); outcome=xl6ErrorScopeEnd(&step);
        if (!outcome.ok) { xrtSetErrorTake(outcome.error); xl6ErrorScopeCaptureFailure(&closing); }
        if (yielded!=generator->has_item || (!yielded && !generator->done)) {
            xrtSetErrorTake(generator_error(XERR_STATE,"RuntimeError","generator close violated its continuation ABI"));
            xl6ErrorScopeCaptureFailure(&closing);
            xrtownershipscope mutation={0}; if (!xrtOwnershipMutationBegin(&mutation)) abort();
            generator->done=true; if (!xrtOwnershipScopeEnd(&mutation)) abort();
        }
        generator_close_item(generator,&closing);
    }
    {
        xl6_error_scope step={0}; xl6_error_result outcome;
        if (!xl6ErrorScopeBegin(&step)) abort();
        generator_finish(generator); outcome=xl6ErrorScopeEnd(&step);
        if (!outcome.ok) { xrtSetErrorTake(outcome.error); xl6ErrorScopeCaptureFailure(&closing); }
    }
    return xl6ErrorScopeEnd(&closing);
}
static xl6_error_result generator_close(XL6_Generator* generator)
{
    XL6_CodeScope code={0}; xl6_error_result outcome;
    if (!generator) return (xl6_error_result){true,NULL};
    if (!generator_begin(generator,&code,true)) return (xl6_error_result){false,xrtTakeError()};
    outcome=generator_close_active(generator); generator_end(generator,&code); return outcome;
}
static void generator_dispose_body(XL6_Generator* generator)
{
    XL6_CodeLease* lease=generator->lease;
    if (!generator->running || generator->frame || generator->has_item) abort();
    xrtFree(control_header(generator)); xl6CodeLeaseRelease(lease);
}
static void generator_dispose(XL6_Generator* generator)
{ XL6_OWNERSHIP_RETURN_VOID(generator_dispose_body(generator)); }
static void generator_destroy(XL6_Generator* generator)
{
    XL6_CodeScope code={0}; xl6_error_result outcome;
    if (!generator) return;
    GeneratorControl* control=control_header(generator);
    if (control_managed(control)) {
        xrtownershipscope mutation={0};
        if (!xrtOwnershipMutationBegin(&mutation)) abort();
        if (control->references<=0) abort();
        if (control->references>1) {
            xrtRefRelease(&control->references);
            if (!xrtOwnershipScopeEnd(&mutation)) abort();
            return;
        }
        /* A running V2 call still owns its invocation reference; releasing
         * that borrowed reference would be a caller ownership violation. */
        if (generator->running) abort();
        if (control->cleared || (generator->done && !generator->frame && !generator->has_item)) {
            XL6_CodeLease* lease=generator->lease;
            if (generator->frame || generator->has_item || !generator->done) abort();
            if (xrtRefRelease(&control->references)!=0) abort();
            xrtFree(control);
            if (!xrtOwnershipScopeEnd(&mutation)) abort();
            xl6CodeLeaseRelease(lease);
            return;
        }
        /* Transfer the last released owner to the actual close invocation.
         * It remains retainable during finally; resurrection keeps a closed
         * control alive and the collector observes that new external owner. */
        if (!generator_begin_body(generator,&code,true)) abort();
        if (xrtRefRelease(&control->references)!=1) abort();
        if (!xrtOwnershipScopeEnd(&mutation)) abort();
        outcome=generator_close_active(generator);
        generator_end(generator,&code);
        if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
        return;
    }
    if (!generator_begin(generator,&code,true)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return;
    }
    outcome=generator_close_active(generator); generator_dispose(generator);
    xl6CodeScopeLeave(&code); /* after every generated callback has returned */
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
}
const XL6_GeneratorRuntimeV1* xl6GeneratorRuntimeV1Get(void)
{
    static const XL6_GeneratorRuntimeV1 api={frame_allocate,frame_free,generator_create,generator_ownership,
        generator_next,generator_item_get,generator_close,generator_destroy,generator_put,generator_state,generator_finish};
    return &api;
}

static void* frame_allocate_v2_body(size_t bytes,const XL6_ActivationCaptureLayoutV1* layout)
{
    if(!xl6ActivationCaptureRuntimeV1Get()->Validate(layout,bytes)){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    void* data=frame_allocate_body(bytes);
    if(data)frame_header(data)->layout=layout;
    return data;
}
static void* frame_allocate_v2(size_t bytes,const XL6_ActivationCaptureLayoutV1* layout)
{XL6_OWNERSHIP_RETURN(void*,frame_allocate_v2_body(bytes,layout));}
static void* frame_allocate_v3_body(size_t bytes,const XL6_ActivationCaptureLayoutV2* layout)
{
    if(!xl6ActivationCaptureRuntimeV2Get()->Validate(layout,bytes)){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    void* data=frame_allocate_body(bytes);
    if(data)frame_header(data)->layoutV2=layout;
    return data;
}
static void* frame_allocate_v3(size_t bytes,const XL6_ActivationCaptureLayoutV2* layout)
{XL6_OWNERSHIP_RETURN(void*,frame_allocate_v3_body(bytes,layout));}
static void frame_release(void* data)
{
    XL6_GeneratorFrame* frame;if(!data)return;frame=frame_header(data);
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
    if(!frame_managed(frame)||frame->references<=0||(frame->active&&frame->references==1))abort();
    int32_t refs=xrtRefRelease(&frame->references);if(refs<0)abort();
    if(refs){if(!xrtOwnershipScopeEnd(&mutation))abort();return;}
    if(frame->owner||frame->payloadLive)abort();
    XL6_CodeLease* lease=frame->lease;xrtFree(frame);
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    xl6CodeLeaseRelease(lease);
}
static void frame_discard(void* data)
{
    XL6_GeneratorFrame* frame;if(!data)return;frame=frame_header(data);
    XL6_CodeScope code={0};xl6_error_scope cleanup={0};xl6_error_result outcome;
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
    if(!frame_managed(frame)||frame->owner||frame->active||frame->cleared||frame->references<=0||!frame->payloadLive)abort();
    frame->active=true;xl6CodeScopeEnter(&code,frame->lease);
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    if(!xl6ErrorScopeBeginCleanup(&cleanup,false))abort();
    outcome=frame->layoutV2 ? xl6ActivationCaptureRuntimeV2Get()->Cleanup(frame->layoutV2,data,frame->bytes,(xl6_error_result){true,NULL}) :
        xl6ActivationCaptureRuntimeV1Get()->Cleanup(frame->layout,data,(xl6_error_result){true,NULL});
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);
    outcome=xl6ErrorScopeEnd(&cleanup);
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    frame->payloadLive=false;frame->active=false;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    frame_release(data);xl6CodeScopeLeave(&code);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);else xrtSetErrorTake(outcome.error);
}
static XL6_Generator* generator_create_v2(const XL6_CodeOwner* owner,void* frame,XL6_GeneratorResume resume,
    const XL6_CaptureLayoutV1* item)
{
    XL6_OWNERSHIP_RETURN(XL6_Generator*,generator_create_body(owner,frame,resume,NULL,item?item->bytes:0,NULL,NULL,NULL,item,NULL));
}
static XL6_Generator* generator_create_v3(const XL6_CodeOwner* owner,void* frame,XL6_GeneratorResume resume,
    const XL6_CaptureLayoutV2* item)
{
    XL6_OWNERSHIP_RETURN(XL6_Generator*,generator_create_body(owner,frame,resume,NULL,item?item->bytes:0,NULL,NULL,NULL,NULL,item));
}
static bool control_hold(const void* data)
{
    GeneratorControl* control=control_header(data);
    return control->references>0&&control->references<INT32_MAX&&xrtRefRetain(&control->references)>0;
}
static XL6_Generator* generator_retain_body(XL6_Generator* generator)
{
    if(!generator)return NULL;
    GeneratorControl* control=control_header(generator);
    if(!control_managed(control)||control->cleared||!control_hold(generator)){xrtSetErrorKind(XERR_STATE);return NULL;}
    return generator;
}
static XL6_Generator* generator_retain(XL6_Generator* generator)
{XL6_OWNERSHIP_RETURN(XL6_Generator*,generator_retain_body(generator));}
static bool frame_hold(const void* data)
{
    XL6_GeneratorFrame* frame=(XL6_GeneratorFrame*)data;
    return frame->references>0&&frame->references<INT32_MAX&&xrtRefRetain(&frame->references)>0;
}
static void* frame_retain_body(void* data)
{
    if(!data)return NULL;
    XL6_GeneratorFrame* frame=frame_header(data);
    if(!frame_managed(frame)||!frame->published||frame->cleared||!frame_hold(frame)){xrtSetErrorKind(XERR_STATE);return NULL;}
    return data;
}
static void* frame_retain(void* data){XL6_OWNERSHIP_RETURN(void*,frame_retain_body(data));}
static xrtownershipref frame_ownership(const void* data)
{
    if(!data)return (xrtownershipref){0};
    const XL6_GeneratorFrame* frame=frame_header(data);
    return (xrtownershipref){frame,frame_managed(frame)?&frame_managed_ops:&frame_ops};
}
const XL6_GeneratorRuntimeV2* xl6GeneratorRuntimeV2Get(void)
{
    static const XL6_GeneratorRuntimeV2 api={frame_allocate_v2,frame_discard,generator_create_v2,
        generator_retain,frame_retain,frame_release,frame_ownership};return &api;
}
const XL6_GeneratorRuntimeV3* xl6GeneratorRuntimeV3Get(void)
{
    static const XL6_GeneratorRuntimeV3 api={frame_allocate_v3,frame_discard,generator_create_v3,
        generator_retain,frame_retain,frame_release,frame_ownership};return &api;
}
static void control_drop(const void* data){generator_destroy((XL6_Generator*)data);}
static void frame_drop(const void* data){frame_release(((XL6_GeneratorFrame*)data)->data);}
static bool control_claim(const void* data,const void* token)
{
    XL6_Generator* generator=(XL6_Generator*)data;GeneratorControl* control=control_header(generator);
    if(!token||control->cleared||generator->running||(control->claim&&control->claim!=token))return false;
    control->claim=token;return true;
}
static void control_restore(const void* data,const void* token)
{
    GeneratorControl* control=control_header(data);
    if(!token||control->claim!=token||control->cleared)abort();
    control->claim=NULL;
}
static bool control_ready(const void* data)
{
    const XL6_Generator* generator=data;
    return !generator->running&&generator->done&&!generator->frame&&!generator->has_item;
}
static xrtownershipprepareresult control_prepare(const void* data,const void* token)
{
    XL6_Generator* generator=(XL6_Generator*)data;GeneratorControl* control=control_header(generator);
    xrtownershipscope mutation={0};XL6_CodeScope code={0};
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    if(!token||control->claim!=token||control->cleared)abort();
    if(generator->running){if(!xrtOwnershipScopeEnd(&mutation))abort();return XRT_OWNERSHIP_PREPARE_BUSY;}
    if(control_ready(data)){if(!xrtOwnershipScopeEnd(&mutation))abort();return XRT_OWNERSHIP_PREPARE_READY;}
    if(!generator_begin_body(generator,&code,true))abort();
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    xl6_error_result outcome=generator_close_active(generator);generator_end(generator,&code);
    if(!outcome.ok){xrtSetErrorTake(outcome.error);return XRT_OWNERSHIP_PREPARE_FAILED;}
    return XRT_OWNERSHIP_PREPARE_READY;
}
static void control_clear(const void* data,const void* token)
{
    GeneratorControl* control=control_header(data);
    if(!token||control->claim!=token||control->cleared||!control_ready(data))abort();
    control->cleared=true; /* Physical lease remains until the final real Drop. */
}
static bool frame_claim(const void* data,const void* token)
{
    XL6_GeneratorFrame* frame=(XL6_GeneratorFrame*)data;
    if(!token||frame->cleared||frame->active||(frame->owner&&frame->owner->running)||(frame->claim&&frame->claim!=token))return false;
    frame->claim=token;return true;
}
static void frame_restore(const void* data,const void* token)
{
    XL6_GeneratorFrame* frame=(XL6_GeneratorFrame*)data;
    if(!token||frame->claim!=token||frame->cleared)abort();
    frame->claim=NULL;
}
static bool frame_ready(const void* data)
{
    const XL6_GeneratorFrame* frame=data;
    return !frame->owner&&!frame->active&&!frame->payloadLive;
}
static xrtownershipprepareresult frame_prepare(const void* data,const void* token)
{
    const XL6_GeneratorFrame* frame=data;xrtownershipscope mutation={0};
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    if(!token||frame->claim!=token||frame->cleared)abort();
    bool ready=frame_ready(data);
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    /* Only the owning control's normal close may run this payload's duties. */
    return ready?XRT_OWNERSHIP_PREPARE_READY:XRT_OWNERSHIP_PREPARE_BUSY;
}
static void frame_clear(const void* data,const void* token)
{
    XL6_GeneratorFrame* frame=(XL6_GeneratorFrame*)data;
    if(!token||frame->claim!=token||frame->cleared||!frame_ready(data))abort();
    frame->cleared=true;
}
const xrtownershipadapterv1* xl6GeneratorOwnershipAdapterV1(xrtownershipref ref,const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 control={sizeof(control),control_hold,control_drop,control_claim,control_restore,NULL,control_clear,NULL};
    static const xrtownershippreparationv1 controlPreparation={sizeof(controlPreparation),&control,control_ready,control_prepare};
    static const xrtownershipadapterv1 frame={sizeof(frame),frame_hold,frame_drop,frame_claim,frame_restore,NULL,frame_clear,NULL};
    static const xrtownershippreparationv1 framePreparation={sizeof(framePreparation),&frame,frame_ready,frame_prepare};
    size_t count;if(!preparation||!ref.Data)return NULL;
    if(ref.Ops==&generator_managed_ops){
        const GeneratorControl* header=control_header(ref.Data);
        if(!generator_count(ref.Data,&count)||!item_layout_valid(header,((const XL6_Generator*)ref.Data)->item_size))return NULL;
        *preparation=&controlPreparation;return &control;
    }
    if(ref.Ops==&frame_managed_ops){
        const XL6_GeneratorFrame* header=ref.Data;
        if(!frame_count(ref.Data,&count)||!frame_layout_valid(header))return NULL;
        *preparation=&framePreparation;return &frame;
    }
    return NULL;
}
