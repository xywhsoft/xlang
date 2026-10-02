#include "src/runtime/xl6_regex_internal.h"
#include "src/runtime/xl6_closure.h"

/* A synchronous session owns input independently of the caller. Only the
 * active callback can snapshot its borrowed matcher; no view is exported. */
typedef struct XL6_RegexReplacer {
    XL6_RegexThreadOwner thread;
    xregex* regex;
    xvalue* input;
    xstrview text;
    size_t limit;
    const xregexmatcher* current;
    bool entered;
} XL6_RegexReplacer;
typedef struct XL6_RegexReplacementCall {
    XL6_RegexReplacer* session;
    XL6_Closure* closure;
    xerror* originalError;
} XL6_RegexReplacementCall;

void xl6RegexReplacerDestroyV1(void* handle)
{
    XL6_RegexReplacer* session=handle;
    if (session) {
        xrtRegexRelease(session->regex);
        xrtValueRelease(session->input);
        xl6RegexThreadDrop(&session->thread);
        xrtFree(session);
    }
}
void* xl6RegexReplacerCreateV1(void* regex,xstrview text,int64_t limit,int64_t size)
{
    size_t count;
    if (!xl6RegexInput(regex,&text,0,size) || !xl6RegexLimit(limit,&count)) return NULL;
    XL6_RegexReplacer* session=xrtCalloc(1,sizeof(*session));
    if (!session) return NULL;
    session->limit=count;session->regex=xrtRegexRef(regex);
    if (!xl6RegexThreadInit(&session->thread)) goto fail;
    if (!session->regex) goto fail;
    session->input=xrtValueString(text);
    if (!session->input || !xrtValueGetString(session->input,&session->text)) goto fail;
    return session;
fail:
    xl6RegexReplacerDestroyV1(session);return NULL;
}
void* xl6RegexReplacerSnapshotV1(void* handle)
{
    XL6_RegexReplacer* session=handle;
    if (!session) {xrtSetErrorKind(XERR_STATE);return NULL;}
    if (!xl6RegexThreadCheck(&session->thread)) return NULL;
    if (!session->current) {xrtSetErrorKind(XERR_STATE);return NULL;}
    return xl6RegexSnapshotCopy(session->regex,session->input,session->current);
}
static bool replace(const xregexmatcher* matcher,xstrbuf* output,void* context)
{
    XL6_RegexReplacementCall* call=context;
    XL6_ClosureScope scope={0};xstrview result=xl6TextFailure;
    call->session->current=matcher;
    const XL6_ClosureRuntimeV1* runtime=xl6ClosureRuntimeV1Get();
    runtime->Enter(&scope,call->closure);
    bool ok=((bool (*)(void*,xstrview*))call->closure->invoke)(call->closure->environment,&result);
    /* XRT normally wraps callback failures. Preserve the language exception
     * independently so Value/Range/State errors and their metadata survive. */
    if (!ok && xrtGetError()) call->originalError=xrtErrorRef(xrtGetError());
    runtime->Leave(&scope);
    call->session->current=NULL;
    if (ok) ok=xl6TextSelect(&result,-1) && xrtStrBufAppend(output,result);
    xrtFree((void*)result.Data);
    return ok;
}
bool xl6RegexReplacerRunOutputV1(void* handle,XL6_Closure* callback,xstrbuf* output,size_t* count)
{
    XL6_RegexReplacer* session=handle;
    if (!session || !callback || !callback->invoke) {xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if (!xl6RegexThreadCheck(&session->thread)) return false;
    if (session->entered) {xrtSetErrorKind(XERR_STATE);return false;}
    session->entered=true;
    XL6_RegexReplacementCall call={session,callback,NULL};
    bool ok=xrtRegexReplaceFuncTo(session->regex,session->text,session->limit,replace,&call,output,count);
    if (call.originalError) {xrtClearError();xrtSetErrorTake(call.originalError);return false;}
    return ok;
}
xstrview xl6RegexReplacerRunV1(void* handle,XL6_Closure* callback)
{
    xstrbuf output;xrtStrBufInit(&output);
    if (!xl6RegexReplacerRunOutputV1(handle,callback,&output,NULL)) {
        xrtStrBufFree(&output);return xl6TextFailure;
    }
    xstrview view={output.Data,output.Size};
    if (!xl6TextSelect(&view,-1)) {xrtStrBufFree(&output);return xl6TextFailure;}
    size_t size=output.Size;char* result=xrtStrBufTake(&output);
    xrtStrBufFree(&output);return xl6TextOwn(result,size);
}
