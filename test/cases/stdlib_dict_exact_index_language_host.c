#include "src/runtime/xl6_dict.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* Authentic test input producer, separately accounted for in pruned links.
 * It is not a production artifact dependency or a source Dict factory proof. */
static const xstrview indexKeys[]={{"",0},{"a\0\xE4\xBD\xA0",5},{"a",1}};
static XL6_DictLayoutV1 layout(unsigned transfer,const xrttype* type)
{ return (XL6_DictLayoutV1){0xD710,{0xD711,0xD712,transfer,type->Size,type}}; }
static bool inputToText(const xvalue* value,const xrttype* type,ptr out,ptr context)
{
    assert(!context && type==xl6StringTypeV1()); xstrview text={0};
    return xrtValueGetString(value,&text) && xrtTypeCopyValue(type,out,&text);
}
static xvalue* inputFromText(const xrttype* type,const void* slot,ptr context)
{ assert(!context && type==xl6StringTypeV1()); return xrtValueString(*(const xstrview*)slot); }
static const xvalueconverter inputTextCodec={.ToTyped=inputToText,.FromTyped=inputFromText};
static xvalue* indexInput(void)
{
    XL6_DictLayoutV1 t=layout(2,xl6StringTypeV1());
    xvalue* witness=xl6SequencePrimitiveCreateV1(1,t.value.identity,t.value.elementIdentity,
        2,t.value.size,t.value.type,XL6_VALUE_ADMIT_STRING,false,t.value.elementIdentity); assert(witness);
    xvalue* dict=xl6DictProjectionTakeV1(xl6DictCreateTakeV1(&t,witness),&t,&inputTextCodec); assert(dict);
    for (size_t i=0;i<3;++i) assert(xl6DictSetV1(dict,&t,indexKeys[i],&indexKeys[i]));
    return dict;
}
static void indexText(const xvalue* value,xstrview expected)
{
    xstrview text={0}; assert(value && xrtValueGetString(value,&text) && text.Size==expected.Size &&
        text.Data && !memcmp(text.Data,expected.Data,text.Size));
}
static size_t indexPoint,indexSeen;
static bool indexProviders,indexArmed;
static bool indexFailure(void)
{ if (indexArmed && indexSeen++==indexPoint) { xrtClearError(); return true; } return false; }
extern bool __real_xl6ErrorScopeBegin(xl6_error_scope*);
bool __wrap_xl6ErrorScopeBegin(xl6_error_scope* scope) { return __real_xl6ErrorScopeBegin(scope); }
extern bool __real_xl6ErrorScopeBeginCleanup(xl6_error_scope*,bool);
bool __wrap_xl6ErrorScopeBeginCleanup(xl6_error_scope* scope,bool failed)
{ return __real_xl6ErrorScopeBeginCleanup(scope,failed); }
extern ptr __real_xrtTypedDictIterNext(xtypeddictiter*,xstrview*);
ptr __wrap_xrtTypedDictIterNext(xtypeddictiter* it,xstrview* key) { return __real_xrtTypedDictIterNext(it,key); }
extern bool __real_xl6DictErasedReadV1(const xvalue*,xstrview,bool,xvalue**,bool*);
bool __wrap_xl6DictErasedReadV1(const xvalue* source,xstrview key,bool take,xvalue** out,bool* found)
{ return indexProviders && indexFailure() ? false : __real_xl6DictErasedReadV1(source,key,take,out,found); }
extern bool __real_xl6DictErasedSetV1(const xvalue*,xstrview,const xvalue*);
bool __wrap_xl6DictErasedSetV1(const xvalue* source,xstrview key,const xvalue* item)
{ return indexProviders && indexFailure() ? false : __real_xl6DictErasedSetV1(source,key,item); }
static bool dictIndexLanguageAttempt(unsigned lane,size_t point,const char* mode)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init()); xvalue* dict=indexInput();
    xstrview key=lane==1 || lane==2 ? XRT_STR_LITERAL("new") : indexKeys[1];
    xvalue* item=xrtValueString(XRT_STR_LITERAL("replacement")),*result=NULL; assert(item);
    int64_t before=XL6_DICT_INDEX_CLEANUPS; xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent));
    indexPoint=point; indexSeen=0; indexProviders=!strcmp(mode,"providers"); indexArmed=strcmp(mode,"oom")!=0;
    if (!indexArmed) assert(xrtMemDebugFailAfter(point));
    bool success;
    if (lane<2) { XL6_DICT_INDEX_READ_RESULT r=XL6_DICT_INDEX_READ(dict,key); success=r.ok; result=r.value; }
    else { XL6_DICT_INDEX_WRITE_RESULT r=XL6_DICT_INDEX_WRITE(dict,key,item); success=r.ok; if (success) assert(r.value); }
    bool hit=indexArmed ? indexSeen>point : xrtMemDebugFailTriggered(); indexArmed=false; indexProviders=false;
    xrtMemDebugFailClear(); if (!success) xl6ErrorScopeCaptureFailure(&parent); xl6_error_result outcome=xl6ErrorScopeEnd(&parent);
    assert(success==!hit && outcome.ok==!hit && XL6_DICT_INDEX_CLEANUPS==before+1); if (hit) assert(!result);
    if (hit && strcmp(mode,"oom")) assert(!outcome.error);
    XL6_DictLayoutV1 t=layout(2,xl6StringTypeV1()); assert(xl6DictCountV1(dict,&t)==(lane==2 && !hit ? 4 : 3));
    if (success && lane<2) { if (lane==1) assert(result==xrtValueNull()); else indexText(result,indexKeys[1]); }
    /* Generated writes retain the caller's value before the consuming edge. */
    indexText(item,XRT_STR_LITERAL("replacement"));
    xrtErrorFree(outcome.error); xrtValueRelease(result); xrtValueRelease(item); xrtValueRelease(dict); xrtClearError();
    xlir_module_fini(); assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("Dict index language lane=%u mode=%s point=%zu hit=%d balanced\n",lane,mode,point,hit); return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2 && (!strcmp(argv[1],"oom") || !strcmp(argv[1],"providers")));
    for (unsigned lane=0;lane<4;++lane) {
        bool closed=false;
        for (size_t point=0;point<4096;++point) if (!dictIndexLanguageAttempt(lane,point,argv[1])) {
            printf("Dict index language closed lane=%u mode=%s runs=%zu first-no-hit=%zu\n",lane,argv[1],point+1,point);
            closed=true; break;
        }
        assert(closed);
    }
    return 0;
}
