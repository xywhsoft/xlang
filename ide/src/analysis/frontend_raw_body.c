#include "analysis/frontend_raw_body.h"
#include "src/sem/xl6_completion.h"
#include <string.h>
#include <stdlib.h>
#define XLS_RAW_BODY_BYTES (32u*1024u*1024u)
static xbytesview address(const void* node)
{return (xbytesview){(const unsigned char*)node,sizeof(void*)};}
xls_raw_scope* xlsRawBodyScopeCurrent(void)
{xls_raw_scope** slot=xlsFrontendRawBodySlot();return slot ? *slot : NULL;}
void xlsRawBodyScopeBegin(xls_raw_scope* s,const void* baseline,size_t bytes)
{
    *s=(xls_raw_scope){.baseline=baseline,.limit=bytes<XLS_RAW_BODY_BYTES ? bytes : XLS_RAW_BODY_BYTES};
    xls_raw_scope** slot=xlsFrontendRawBodySlot();if(!slot)return;s->previous=*slot;*slot=s;
    s->ready=s->limit && xrtArrayInit(&s->bindings,sizeof(xls_raw_binding)) && xrtMapInit(&s->functions,sizeof(size_t));
    s->environmentReady=s->ready;s->encoder=xlsRawBodyEnvironmentKey;
}
static void rawEnvironmentClear(xls_raw_scope*);
void xlsRawBodyScopeEnd(xls_raw_scope* s)
{
    xls_raw_scope** slot=xlsFrontendRawBodySlot();if(!slot || *slot!=s)return;
    for(size_t i=0;i<s->bindings.Count;++i)xlsRawUnitRelease(((xls_raw_binding*)xrtArrayGet(&s->bindings,i))->unit);
    xrtArrayUnit(&s->bindings);xrtMapUnit(&s->functions);rawEnvironmentClear(s);xrtStrBufFree(&s->preprocess);*slot=s->previous;
}
xls_raw_binding* xlsRawBodyBinding(const XL6_AstNode* function)
{
    xls_raw_scope* s=xlsRawBodyScopeCurrent();if(!s || !s->ready || !function)return NULL;
    size_t* at=xrtMapGet(&s->functions,address(&function));return at ? xrtArrayGet(&s->bindings,*at) : NULL;
}
int xlsRawBodyAdmit(xls_raw_unit* unit,const char* path,const XL6_AstRange* range,size_t* bytes,void* user)
{
    xls_raw_scope* s=user;return s && s->ready && s->baseline && xlsRawBodyMemoAdmit(s->baseline,unit,path,range,bytes);
}
void xlsRawBodyRecord(xls_raw_unit* unit,const char* path,XL6_AstNode* function,int deferred,void* user)
{
    xls_raw_scope* s=user;if(!s || !s->ready || !function || !unit)return;
    if(function->kind!=XL6_AST_FUNC && function->kind!=XL6_AST_ASYNC_FUNC && function->kind!=XL6_AST_EXPORT_FUNC)return;
    size_t owned=0,slot=s->bindings.Count;
    /* A full optional charge for every independent retained raw declaration.
     * XRT map capacity has a conservative per-record allowance;this local
     * budget is neither process RSS nor a global transient allocation cap. */
    if(!path || xrtMapHas(&s->functions,address(&function)) || slot>=65536 ||
       !(xlsRawBodyMemoAdmit(s->baseline,unit,path,&function->range,&owned) || xlsRawUnitBytes(unit,s->limit,&owned)) || s->unitBytes>s->limit || owned>s->limit-s->unitBytes ||
       slot+1>(s->limit-s->unitBytes-owned)/(sizeof(xls_raw_binding)+128))goto failed;
    if(!xrtArrayReserve(&s->bindings,slot+1) || s->bindings.Capacity>(s->limit-s->unitBytes-owned)/(sizeof(xls_raw_binding)+128))goto failed;
    if(!xrtMapSet(&s->functions,address(&function),&slot))goto failed;
    ((xls_raw_binding*)s->bindings.Data)[s->bindings.Count++]=(xls_raw_binding){function,xlsRawUnitRetain(unit),owned,deferred,0,0,0};
    s->unitBytes+=owned;++s->registered;s->deferred+=!!deferred;return;
failed:s->ready=0;++s->declined;if(deferred || s->deferred)s->failed=1;
}
int xlsRawBodyCopyParse(const xls_syntax* syntax,XL6_ParseResult* out)
{
    xls_raw_scope* s=xlsRawBodyScopeCurrent();return s && s->ready ? xlsRawBodySyntaxCopy(syntax,NULL,out,s) : xlsSyntaxCopyParse(syntax,out);
}
int xlsRawBodyCopyCursorParse(const xls_syntax* syntax,XL6_CompletionQuery* query,XL6_ParseResult* out)
{
    xls_raw_scope* s=xlsRawBodyScopeCurrent();return s && s->ready ? xlsRawBodySyntaxCopy(syntax,query,out,s) : xlsSyntaxCopyCursorParse(syntax,query,out);
}
int xlsRawBodyCopySignatureParse(const xls_syntax* syntax,XL6_CompletionQuery* query,XL6_ParseResult* out,size_t callOpen)
{
    xls_raw_scope* s=xlsRawBodyScopeCurrent();
    return s && s->ready ? xlsRawBodySyntaxCopySignature(syntax,query,out,callOpen,s) : xlsSyntaxCopySignatureParse(syntax,query,out,callOpen);
}
void xlsRawBodyRegisterSyntax(const xls_syntax* syntax,XL6_AstNode* out)
{xls_raw_scope* s=xlsRawBodyScopeCurrent();if(s && s->ready)xlsRawBodySyntaxRegister(syntax,out,s);}
void xlsRawBodyStdlibBegin(xls_raw_stdlib_frame* frame,XL6_AstNode* function)
{
    xls_raw_scope* s=xlsRawBodyScopeCurrent();*frame=(xls_raw_stdlib_frame){0};if(!s || !s->ready)return;
    frame->previous=s->stdlibFrame;frame->binding=xlsRawBodyBinding(function);s->stdlibFrame=frame;
}
void xlsRawBodyStdlibEnd(xls_raw_stdlib_frame* frame)
{
    xls_raw_scope* s=xlsRawBodyScopeCurrent();if(!s || s->stdlibFrame!=frame)return;
    if(frame->binding)frame->binding->stdlibVisited=1;
    s->stdlibFrame=frame->previous;
}
void xlsRawBodyStdlibMark(void)
{
    xls_raw_scope* s=xlsRawBodyScopeCurrent();
    for(xls_raw_stdlib_frame* frame=s ? s->stdlibFrame : NULL;frame;frame=frame->previous)
        if(frame->binding)frame->binding->stdlibMarks=1;
}
int xlsRawBodySealPreprocess(const XL6_SourceReport* report)
{
    xls_raw_scope* s=xlsRawBodyScopeCurrent();if(!s || !s->ready)return !s || !s->deferred;
    s->preprocessValid=xlsRawBodyPreprocessInputs(&s->preprocess,report);
    if(s->deferred && (!s->preprocessValid || !xlsRawBodyMemoPreprocessEqual(s->baseline,&s->preprocess)))s->failed=1;
    return !s->failed;
}
#include "analysis/frontend_raw_environment.inc"
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
#include "analysis/frontend_raw_domains_current.inc"
#else
#include "analysis/frontend_raw_domains.inc"
#endif
