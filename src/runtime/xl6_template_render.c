#include "xl6_template_internal.h"
#if defined(_WIN32)
#include <windows.h>
static DWORD contextKey=FLS_OUT_OF_INDEXES;
#else
#include <pthread.h>
static pthread_key_t contextKey;
#endif
typedef struct Render {
    XL6_Closure* callback;
    XL6_TemplateToken* token;
    bool callbackFailed;
    xerror* callbackError;
} Render;
static xonce contextOnce=XRT_ONCE_INIT;
static bool contextInit(void* unused)
{
    (void)unused;
#if defined(_WIN32)
    contextKey=FlsAlloc(NULL);return contextKey!=FLS_OUT_OF_INDEXES;
#else
    return pthread_key_create(&contextKey,NULL)==0;
#endif
}
static Render* current(void)
{
#if defined(_WIN32)
    return FlsGetValue(contextKey);
#else
    return pthread_getspecific(contextKey);
#endif
}
static bool setCurrent(Render* value)
{
#if defined(_WIN32)
    return FlsSetValue(contextKey,value)!=0;
#else
    return pthread_setspecific(contextKey,value)==0;
#endif
}
void xl6TemplateCallDestroyV1(void* raw)
{
    XL6_TemplateToken* token=raw;if (!token) return;
    if (xrtAtomic64FetchSub(&token->references,1,XMEMORY_ACQ_REL)!=1) return;
    xrtTemplateRelease(token->resolved);xl6TextThreadDrop(&token->thread);xrtFree(token);
}
XL6_TemplateToken* xl6TemplateTokenCheckV1(void* raw)
{
    XL6_TemplateToken* token=raw;
    if (!token) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if (!xl6TextThreadCheck(&token->thread)) return NULL;
    if (!token->active) {xrtSetErrorKind(XERR_STATE);return NULL;}return token;
}
void* xl6TemplateCallCreateV1(void)
{
    if (!xrtOnce(&contextOnce,contextInit,NULL)) {xrtSetErrorKind(XERR_MEMORY);return NULL;}
    Render* state=current();XL6_TemplateToken* token=state?state->token:NULL;
    if (!token || !token->active) {xrtSetErrorKind(XERR_STATE);return NULL;}
    uint64_t count=xrtAtomic64Load(&token->references,XMEMORY_RELAXED);
    if (count==UINT64_MAX) {xrtSetErrorKind(XERR_RANGE);return NULL;}
    /* Only the invoking thread can acquire new references; drops may race. */
    xrtAtomic64FetchAdd(&token->references,1,XMEMORY_RELAXED);return token;
}
bool xl6TemplateCallIsIncludeV1(void* raw)
{XL6_TemplateToken* token=xl6TemplateTokenCheckV1(raw);return token && token->include;}
uint64_t xl6TemplateCallRegistryV1(void* raw)
{XL6_TemplateToken* token=xl6TemplateTokenCheckV1(raw);return token?token->registry:0;}
int64_t xl6TemplateCallIndexV1(void* raw)
{XL6_TemplateToken* token=xl6TemplateTokenCheckV1(raw);return token?(int64_t)token->index:0;}
xstrview xl6TemplateCallNameV1(void* raw)
{XL6_TemplateToken* token=xl6TemplateTokenCheckV1(raw);return token?xl6TextCopy(token->name):xl6TextFailure;}
bool xl6TemplateCallResolveV1(void* raw,void* compiled)
{
    XL6_TemplateToken* token=xl6TemplateTokenCheckV1(raw);XL6_Template* self=compiled;
    if (!token) return false;
    if (!token->include) {xrtSetErrorKind(XERR_STATE);return false;}
    if (!xl6TemplateCheckV1(self)) return false;
    xtemplate* value=xrtTemplateRef(self->value);if (!value) return false;
    xrtTemplateRelease(token->resolved);token->resolved=value;return true;
}
bool xl6TemplateInvokeV1(xtemplatecall* call,uint64_t registry,size_t index,xstrview name,xtemplate** resolved)
{
    Render* state=current();
    if (!state || !state->callback || !state->callback->invoke) {xrtSetErrorKind(XERR_STATE);return false;}
    XL6_TemplateToken* token=xrtCalloc(1,sizeof(*token));if (!token) return false;
    xrtAtomic64Init(&token->references,1);
    if (!xl6TextThreadInit(&token->thread)) {xrtFree(token);return false;}
    token->active=true;token->include=resolved!=NULL;token->registry=registry;
    token->index=index;token->name=name;token->call=call;
    XL6_TemplateToken* previous=state->token;state->token=token;
    XL6_ClosureScope code={0};xl6_error_scope scope={0};xstrview ignored=xl6TextFailure;
    bool ok=xl6ErrorScopeBegin(&scope);
    if (ok) {
        const XL6_ClosureRuntimeV1* runtime=xl6ClosureRuntimeV1Get();
        runtime->Enter(&code,state->callback);
        ok=((bool (*)(void*,xstrview*))state->callback->invoke)(state->callback->environment,&ignored);
        if (!ok) xl6ErrorScopeCaptureFailure(&scope);
        runtime->Leave(&code);xrtFree((void*)ignored.Data);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);ok=outcome.ok;
        if (!ok) {
            if (!state->callbackFailed) {state->callbackFailed=true;state->callbackError=outcome.error;}
            else xrtErrorFree(outcome.error);
            xrtSetErrorTake(xrtErrorRef(state->callbackError));
        }
    }
    token->active=false;token->call=NULL;token->name=(xstrview){0};state->token=previous;
    if (ok && resolved) {*resolved=token->resolved;token->resolved=NULL;}
    xl6TemplateCallDestroyV1(token);return ok;
}
static bool resolve(ptr unused,xstrview name,xtemplate** output)
{(void)unused;return xl6TemplateInvokeV1(NULL,0,0,name,output);}
static bool renderConfig(const xvalue* options,xtemplaterenderconfig* config)
{
    xrtTemplateRenderConfigInit(config);config->Flags=XTEMPLATE_STRICT_UNDEFINED;
    if (!xl6TemplateOptionObjectV1(options)) return false;
#define LIMIT(Name,Field) if (!xl6TemplateSizeOptionV1(options,Name,&config->Field)) return false
    LIMIT("maxOutputBytes",MaxOutputBytes);LIMIT("maxSteps",MaxSteps);LIMIT("maxDepth",MaxDepth);
    LIMIT("maxLoopIterations",MaxLoopIterations);LIMIT("maxIncludeDepth",MaxIncludeDepth);
#undef LIMIT
    return xl6TemplateBoolOptionV1(options,"strictUndefined",XTEMPLATE_STRICT_UNDEFINED,&config->Flags) &&
        xl6TemplateBoolOptionV1(options,"escapeHtml",XTEMPLATE_ESCAPE_HTML_TEXT,&config->Flags);
}
bool xl6TemplateWriteOutputV1(void* raw,const xvalue* data,const xvalue* options,
    XL6_Closure* callback,XL6_TemplateWriter writer,ptr target)
{
    XL6_Template* self=raw;xtemplaterenderconfig config;
    if (!xl6TemplateCheckV1(self) || !renderConfig(options,&config)) return false;
    const xvalue* root=xl6TemplateOptionV1(options,"root");
    if (!root || xrtValueType(root)==XVALUE_NULL) root=data?data:xrtValueNull();
    const xvalue* value=xl6TemplateOptionV1(options,"current");
    if (!value || xrtValueType(value)==XVALUE_NULL) value=root;
    const xvalue* global=xl6TemplateOptionV1(options,"global");if (!global) global=xrtValueNull();
    xvalue* scopes=xrtValueArray();if (!scopes) return false;
    bool ready=xrtValueArrayAppend(scopes,root) && xrtValueArrayAppend(scopes,value) && xrtValueArrayAppend(scopes,global);
    xvalue* snapshot=ready?xl6TemplateDataV1(scopes):NULL;xl6TemplateDropValueV1(scopes);
    if (!snapshot) return false;
    config.Root=xrtValueArrayGet(snapshot,0);config.Current=xrtValueArrayGet(snapshot,1);config.Global=xrtValueArrayGet(snapshot,2);
    if (!xrtOnce(&contextOnce,contextInit,NULL)) {xl6TemplateDropValueV1(snapshot);xrtSetErrorKind(XERR_MEMORY);return false;}
    Render state={.callback=callback};Render* previous=current();
    if (!setCurrent(&state)) {xl6TemplateDropValueV1(snapshot);xrtSetErrorKind(XERR_MEMORY);return false;}
    config.Resolve=resolve;
    bool ok=xrtTemplateWrite(self->value,&config,writer,target);
    /* An already installed FLS binding must restore; otherwise retaining a
     * pointer into a returned stack would be an unrecoverable host failure. */
    if (!setCurrent(previous)) abort();
    if (state.callbackFailed) {
        xrtClearError();xrtSetErrorTake(xrtErrorRef(state.callbackError));ok=false;
        xl6ErrorScopePropagateFailure((xl6_error_result){false,state.callbackError});
    }
    xl6TemplateDropValueV1(snapshot);return ok;
}
static bool append(ptr output,xstrview text)
{return xl6TextSelect(&text,-1) && xrtStrBufAppend(output,text);}
xstrview xl6TemplateRenderV1(void* raw,const xvalue* data,const xvalue* options,XL6_Closure* callback)
{
    xstrbuf output;xrtStrBufInit(&output);
    if (!xl6TemplateWriteOutputV1(raw,data,options,callback,append,&output)) {
        xrtStrBufFree(&output);xl6TemplateForeignFailureV1();return xl6TextFailure;
    }
    size_t size=output.Size;char* text=xrtStrBufTake(&output);xrtStrBufFree(&output);
    xstrview result=xl6TextOwn(text,size);if (!result.Data) xl6TemplateForeignFailureV1();return result;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6TemplateCallHandlePolicyV1Get,xl6TemplateCallDestroyV1)
