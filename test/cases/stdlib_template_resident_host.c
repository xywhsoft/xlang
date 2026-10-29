#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_closure.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void* escaped;
static void* included;
static int mode;
static xstrview view(const char* value) {return (xstrview){value,strlen(value)};}
static bool invoke(void* unused,xstrview* output)
{
    (void)unused;void* context=xl6TemplateCallCreateV1();assert(context);
    if (!escaped) escaped=xl6TemplateCallCreateV1();
    if (xl6TemplateCallIsIncludeV1(context)) {
        if (included) assert(xl6TemplateCallResolveV1(context,included));
    } else if (mode==1) {
        assert(xl6TemplateCallArgumentCountV1(context)==2);
        assert(xl6TemplateCallFindV1(context,view("absent"),-1)==-1 && !xrtGetError());
        for (int64_t i=0;i<2;i++) {
            xvalue* argument=xl6TemplateCallArgumentV1(context,i);assert(argument);xrtValueRelease(argument);
            xvalue* value=xl6TemplateCallEvalV1(context,i);xstrview text;
            assert(value && xrtValueGetString(value,&text));assert(xl6TemplateCallWriteV1(context,text,-1));xrtValueRelease(value);
        }
    } else if (mode==2) {
        assert(xl6TemplateCallRenderBodyV1(context));
    } else if (mode==3) {
        xstrview raw=xl6TemplateCallRawV1(context);assert(raw.Data);
        assert(xl6TemplateCallWriteV1(context,raw,-1));xrtFree((void*)raw.Data);
    } else if (mode==4) {xl6TemplateCallDestroyV1(context);xrtSetErrorKind(XERR_RANGE);return false;}
    xl6TemplateCallDestroyV1(context);*output=(xstrview){xrtStrDupN("",0),0};return output->Data!=NULL;
}
static void same(xstrview actual,const char* expected,size_t size)
{assert(actual.Data && actual.Size==size && !memcmp(actual.Data,expected,size));xrtFree((void*)actual.Data);}
static void diagnosticOnlyFailure(void* compiled,const xvalue* definitions)
{
    xbuffer* output=xrtBufferCreate();assert(output);
    for (int op=0;op<4;op++) {
        xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
        assert(xrtMemDebugFailAfter(0));
        if(op==0)assert(!xl6TemplateRenderV1(compiled,NULL,NULL,NULL).Data);
        else if(op==1)assert(!xl6TemplateCreateExtensionsV1(view("{@custom}"),NULL,definitions,-1));
        else if(op==2)assert(xl6TemplateWriteToV1(compiled,NULL,NULL,NULL,NULL)==0);
        else assert(xl6TemplateAppendV1(compiled,output,NULL,NULL,NULL)==0 && output->Size==0);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        /* Clearing an optional diagnostic must not turn failed status into
         * successful empty output at a diagnostic-only foreign boundary. */
        xrtClearError();xl6_error_result result=xl6ErrorScopeEnd(&scope);
        assert(!result.ok);xrtErrorFree(result.error);if(op!=2)assert(hit);
    }
    xrtBufferDestroy(output);
}
static xvalue* definitions(const char* name,int type)
{
    xvalue* result=xrtValueArray();xvalue* row=xrtValueArray();assert(result && row);
    xvalue* item=xrtValueString(view(name));assert(item && xrtValueArrayAppendTake(row,&item));
    item=xrtValueInt(type);assert(item && xrtValueArrayAppendTake(row,&item));
    item=xrtValueInt(0);assert(item && xrtValueArrayAppendTake(row,&item));
    item=xrtValueInt(2);assert(item && xrtValueArrayAppendTake(row,&item));
    assert(xrtValueArrayAppendTake(result,&row));return result;
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    const XL6_ClosureRuntimeV1* runtime=xl6ClosureRuntimeV1Get();
    XL6_Closure* callback=runtime->Create(NULL,(void(*)(void))invoke,NULL,NULL,NULL,NULL,NULL,0,NULL);assert(callback);
    xvalue* data=xrtValueObject();xvalue* name=xrtValueString(view("Ada"));assert(data && name);
    assert(xrtValueObjectSetTake(data,view("name"),&name));
    void* compiled=xl6TemplateCreateV1(view("Hello {$name}"),NULL,-1);assert(compiled);
    same(xl6TemplateRenderV1(compiled,data,NULL,callback),"Hello Ada",9);
    assert(xl6TemplateNodeCountV1(compiled)>0);xvalue* node=xl6TemplateNodeV1(compiled,0);assert(node);xrtValueRelease(node);
    xl6TemplateDestroyV1(compiled);
    compiled=xl6TemplateCreateV1((xstrview){"a\0b",3},NULL,-1);assert(compiled);
    same(xl6TemplateSourceV1(compiled),"a\0b",3);same(xl6TemplateRenderV1(compiled,NULL,NULL,callback),"a\0b",3);
    xl6TemplateDestroyV1(compiled);
    for (int type=1;type<=4;type++) {
        const char* source=type==1?"{@custom:'A':'B'}":type==2?"{#custom:'A':'B'}":type==3?"{#custom}Y{#end}":"{#custom}{$raw}{#end}";
        mode=type<=2?1:type==3?2:3;
        xvalue* defs=definitions("custom",type);compiled=xl6TemplateCreateExtensionsV1(view(source),NULL,defs,-1);assert(compiled);xrtValueRelease(defs);
        same(xl6TemplateRenderV1(compiled,data,NULL,callback),type<=2?"AB":type==3?"Y":"{$raw}",type<=2?2:type==3?1:6);
        assert(escaped);assert(!xl6TemplateCallNameV1(escaped).Data);assert(xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
        xl6TemplateCallDestroyV1(escaped);escaped=NULL;xl6TemplateDestroyV1(compiled);
    }
    included=xl6TemplateCreateV1(view("{$name}"),NULL,-1);assert(included);
    compiled=xl6TemplateCreateV1(view("A{#include:'part'}B"),NULL,-1);assert(compiled);
    same(xl6TemplateRenderV1(compiled,data,NULL,callback),"AAdaB",5);
    xl6TemplateCallDestroyV1(escaped);escaped=NULL;xl6TemplateDestroyV1(compiled);xl6TemplateDestroyV1(included);included=NULL;
    assert(!xl6TemplateCreateV1(view("{#if:}"),NULL,-1));assert(xrtGetError());xrtClearError();
    compiled=xl6TemplateCreateV1(view("{$missing}"),NULL,-1);assert(compiled);
    assert(!xl6TemplateRenderV1(compiled,data,NULL,callback).Data);assert(xrtGetError());xrtClearError();xl6TemplateDestroyV1(compiled);
    xvalue* defs=definitions("custom",1);compiled=xl6TemplateCreateExtensionsV1(view("{@custom}"),NULL,defs,-1);assert(compiled);xrtValueRelease(defs);mode=4;
    assert(!xl6TemplateRenderV1(compiled,data,NULL,callback).Data);assert(xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    xl6TemplateDestroyV1(compiled);xl6TemplateCallDestroyV1(escaped);
    compiled=xl6TemplateCreateV1(view("plain"),NULL,-1);assert(compiled);defs=definitions("custom",1);
    diagnosticOnlyFailure(compiled,defs);xrtValueRelease(defs);xl6TemplateDestroyV1(compiled);
    runtime->Release(callback);xrtValueRelease(data);
    puts("template exact lengths, scope projection, registry kinds, resolver, expired context, checked errors passed");return 0;
}
