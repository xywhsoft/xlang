/* Preinclude actual generated C and compiler-derived bindings. No alternate
 * public factory or fake List. Restoration returns genuine private owners. */
#include "src/runtime/xl6_list_graph_access.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static size_t scopeSeen,scopeCountdown;
extern bool __real_xl6ErrorScopeBegin(xl6_error_scope*);
bool __wrap_xl6ErrorScopeBegin(xl6_error_scope* scope)
{
    if (scopeCountdown && ++scopeSeen==scopeCountdown) { xrtClearError(); return false; }
    return __real_xl6ErrorScopeBegin(scope);
}
static void verify(const xvalue* root)
{
    const xvalue* list=xrtValueObjectGet(root,(xstrview){"narrow",6});
    ptr raw=NULL,user=NULL; const xvaluehandleops* ops=NULL; XL6_ListGraphViewV1 view={0};
    assert(list && xrtValueType(list)==XVALUE_HANDLE && xrtValueGetHandle(list,&raw,&ops,&user));
    assert(xl6ListGraphBeginV1(ops,raw,user,xrtValueTypeId(list),&view)==1);
    assert(xrtTypedListCount(view.storage)==2); xl6ListGraphEndV1(&view);
    xvalue* item=NULL; bool found=false; int64 value=0;
    assert(xl6ListErasedReadV1(list,0,false,&item,&found) && found && xrtValueGetInt(item,&value) && value==-128);
    xrtValueRelease(item);
}
static bool attempt(unsigned format,size_t point,bool scopes)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init());
    xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
    XL6_LIST_DEFAULT_RESULT target=XL6_LIST_DEFAULT_FN(); assert(target.ok && target.value);
    if (scopes) { scopeSeen=0; scopeCountdown=point+1; }
    else assert(xrtMemDebugFailAfter(point));
    const char* raw=format%2 ?
        "{\"narrow\":[-128,127],\"words\":[\"a\\u0000你\",null],\"items\":[{\"number\":31}],\"nested\":[[char(\"A\"),char(\"你\")]]}" :
        "{\"narrow\":[-128,127],\"words\":[\"a\\u0000你\",null],\"items\":[{\"number\":31}],\"nested\":[[65,20320]]}";
    char path[4096];
    if (format>=2) {
        const char* directory=getenv("XL6_PERSIST_FILE_ROOT"); assert(directory);
        int written=snprintf(path,sizeof(path),"%s/host-list.data",directory);
        assert(written>0 && (size_t)written<sizeof(path));
        FILE* output=fopen(path,"wb"); assert(output);
        assert(fwrite(raw,1,strlen(raw),output)==strlen(raw) && !fclose(output)); raw=path;
    }
    xvalue* result=NULL;
    bool ok=format==3 ? XL6_LIST_XLON_READ_FN((xstrview){raw,strlen(raw)},target.value,&result) :
        format==2 ? XL6_LIST_JSON_READ_FN((xstrview){raw,strlen(raw)},target.value,&result) :
        format==1 ? XL6_LIST_XLON_FN((xstrview){raw,strlen(raw)},target.value,&result) :
        XL6_LIST_PARSE_FN((xstrview){raw,strlen(raw)},target.value,&result);
    bool hit=scopes ? scopeSeen>=scopeCountdown : xrtMemDebugFailTriggered();
    scopeCountdown=0; xrtMemDebugFailClear(); if (!ok) xl6ErrorScopeCaptureFailure(&boundary);
    xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
    if (ok==hit || outcome.ok==hit) fprintf(stderr,"restore format=%u point=%zu hit=%d ok=%d scope-ok=%d\n",format,point,hit,ok,outcome.ok);
    assert(ok==!hit && outcome.ok==!hit);
    if (hit) assert(!result); else { assert(result); verify(result); }
    xrtErrorFree(outcome.error); xrtValueRelease(result); xrtClearError(); xlir_module_fini();
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("List restore format=%u point=%zu hit=%d balanced\n",format,point,hit); return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2); bool scopes=!strcmp(argv[1],"scopes"),vectors=!strcmp(argv[1],"vectors");
    assert(scopes || vectors || !strcmp(argv[1],"oom"));
    for (unsigned format=0;format<4;++format) {
        if (vectors) { assert(!attempt(format,4095,false)); continue; }
        size_t point=0; while(point<4096 && attempt(format,point,scopes)) ++point;
        assert(point<4096); printf("List restore closed-batch format=%u runs=%zu first-no-hit=%zu\n",format,point+1,point);
    }
    return 0;
}
