#define XL6_STATIC_INDEX_NO_MAIN
#include "stdlib_list_static_index_native_host.c"

#define TUPLE(Name,Transfer,CType,Descriptor) \
    (XL6_ListLayoutV1){XL6_STATIC_##Name##_OWNER,{XL6_STATIC_##Name##_SCHEMA, \
        XL6_STATIC_##Name##_ELEMENT,Transfer,sizeof(CType),Descriptor}}
static void languageVectors(void)
{
    XL6_ListLayoutV1 t=TUPLE(readInt,1,int64_t,xrtTypeInt64());
    xvalue* list=profile(&t,XL6_VALUE_ADMIT_I64,false,t.value.elementIdentity,NULL);
    int64_t integer=0; assert(xl6ListSetV1(list,&t,INT64_MIN,&integer));
    XL6_STATIC_readInt_RESULT i=XL6_STATIC_readInt(list,INT64_MIN); assert(i.ok && i.value==0);
    XL6_STATIC_update_RESULT u=XL6_STATIC_update(list,INT64_MAX,10); assert(u.ok && u.value==14);
    i=XL6_STATIC_readInt(list,INT64_MAX); assert(i.ok && i.value==14);
    assert(XL6_STATIC_readSafe_OWNER==t.identity && XL6_STATIC_readSafe_SCHEMA==t.value.identity);
    XL6_STATIC_readSafe_RESULT safe=XL6_STATIC_readSafe(list,7); assert(safe.ok && !safe.value.hasValue);
    safe=XL6_STATIC_readSafe(list,INT64_MIN); assert(safe.ok && safe.value.hasValue && safe.value.value==0);
    safe=XL6_STATIC_readSafe(NULL,0); assert(safe.ok && !safe.value.hasValue);
    int64_t before=XL6_STATIC_CLEANUPS; i=XL6_STATIC_readInt(list,7);
    assert(!i.ok && xrtGetError() && xrtErrorKind(xrtGetError())==XERR_RANGE && XL6_STATIC_CLEANUPS==before+1);
    xrtClearError(); xrtValueRelease(list);
    t=TUPLE(readUInt,1,uint64_t,xrtTypeUInt64()); list=profile(&t,XL6_VALUE_ADMIT_U64,false,t.value.elementIdentity,NULL);
    uint64_t wide=UINT64_MAX; assert(xl6ListSetV1(list,&t,-1,&wide));
    XL6_STATIC_readUInt_RESULT ui=XL6_STATIC_readUInt(list,-1); assert(ui.ok && ui.value==UINT64_MAX); xrtValueRelease(list);
    t=TUPLE(readBool,1,bool,xrtTypeBool()); list=profile(&t,XL6_VALUE_ADMIT_BOOL,false,t.value.elementIdentity,NULL);
    bool bit=false; assert(xl6ListSetV1(list,&t,0,&bit));
    XL6_STATIC_readBool_RESULT b=XL6_STATIC_readBool(list,0); assert(b.ok && !b.value); xrtValueRelease(list);
    t=TUPLE(readFloat,1,double,xrtTypeFloat64()); list=profile(&t,XL6_VALUE_ADMIT_F64,false,t.value.elementIdentity,NULL);
    double real=0.0; assert(xl6ListSetV1(list,&t,0,&real));
    XL6_STATIC_readFloat_RESULT f=XL6_STATIC_readFloat(list,0); assert(f.ok && f.value==0.0); xrtValueRelease(list);
    t=TUPLE(readText,2,xstrview,xl6StringTypeV1()); list=profile(&t,XL6_VALUE_ADMIT_STRING,false,t.value.elementIdentity,xl6StringConverterV1());
    xstrview text={"a\0\xE4\xBD\xA0",5}; assert(xl6ListSetV1(list,&t,INT64_MIN,&text));
    XL6_STATIC_readText_RESULT s=XL6_STATIC_readText(list,INT64_MIN); assert(s.ok); textExact(s.value,text.Data,text.Size); xrtFree((void*)s.value.Data);
    XL6_STATIC_replaceText_RESULT replacement=XL6_STATIC_replaceText(list,INT64_MIN,(xstrview){"b\0c",3});
    assert(replacement.ok); textExact(replacement.value,"b\0c\0z",5); xrtFree((void*)replacement.value.Data); xrtValueRelease(list);
    t=TUPLE(readChar,1,uint32_t,xl6CharTypeV1()); list=profile(&t,XL6_VALUE_ADMIT_CHAR,false,t.value.elementIdentity,xl6CharConverterV1());
    uint32_t character=0x4F60; assert(xl6ListSetV1(list,&t,0,&character));
    XL6_STATIC_readChar_RESULT c=XL6_STATIC_readChar(list,0); assert(c.ok && c.value==character); xrtValueRelease(list);
    t=TUPLE(readBytes,3,xvalue*,xrtTypeValue()); list=profile(&t,XL6_VALUE_ADMIT_BYTES,false,t.value.elementIdentity,NULL);
    xvalue* payload=xrtValueBytes((xbytesview){(const uint8_t*)"a\0b",3}); assert(payload && xl6ListSetV1(list,&t,0,&payload));
    XL6_STATIC_readBytes_RESULT bytes=XL6_STATIC_readBytes(list,0); xbytesview data={0};
    assert(bytes.ok && xrtValueGetBytes(bytes.value,&data) && data.Size==3 && !memcmp(data.Data,"a\0b",3));
    xrtValueRelease(bytes.value); xrtValueRelease(payload); xrtValueRelease(list);
    t=TUPLE(readOptional,2,xstrview,xl6NullableStringTypeV1());
    list=profile(&t,XL6_VALUE_ADMIT_STRING,true,XL6_STATIC_readText_ELEMENT,xl6NullableStringConverterV1());
    xstrview absent={0},empty={"",0}; assert(xl6ListSetV1(list,&t,0,&absent) && xl6ListSetV1(list,&t,1,&empty));
    XL6_STATIC_readOptional_RESULT opt=XL6_STATIC_readOptional(list,0); assert(opt.ok && !opt.value.Data);
    opt=XL6_STATIC_readOptional(list,1); assert(opt.ok); textExact(opt.value,"",0); xrtFree((void*)opt.value.Data);
    opt=XL6_STATIC_readOptional(list,7); assert(opt.ok && !opt.value.Data);
    XL6_STATIC_replaceOptional_RESULT repl=XL6_STATIC_replaceOptional(list,0,(xstrview){"a\0b",3});
    assert(repl.ok); textExact(repl.value,"a\0b",3); xrtFree((void*)repl.value.Data);
    repl=XL6_STATIC_replaceOptional(list,1,(xstrview){"wrong",5}); assert(repl.ok); textExact(repl.value,"",0); xrtFree((void*)repl.value.Data);
    xrtValueRelease(list);
}
static bool languageAttempt(unsigned operation,size_t point,bool providers)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init());
    XL6_ListLayoutV1 t=TUPLE(readText,2,xstrview,xl6StringTypeV1());
    xvalue* list=profile(&t,XL6_VALUE_ADMIT_STRING,false,t.value.elementIdentity,xl6StringConverterV1());
    xstrview text={"a\0\xE4\xBD\xA0",5}; assert(xl6ListSetV1(list,&t,INT64_MIN,&text));
    int64_t before=XL6_STATIC_CLEANUPS; xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
    if (providers) { providerSeen=0; providerCountdown=(unsigned)point+1; } else assert(xrtMemDebugFailAfter(point));
    xstrview output={0}; bool success;
    if (!operation) { XL6_STATIC_readText_RESULT r=XL6_STATIC_readText(list,INT64_MIN); success=r.ok; output=r.value; }
    else { XL6_STATIC_replaceText_RESULT r=XL6_STATIC_replaceText(list,INT64_MIN,(xstrview){"b\0c",3}); success=r.ok; output=r.value; }
    bool hit=providers ? providerSeen>=providerCountdown : xrtMemDebugFailTriggered();
    providerCountdown=0; xrtMemDebugFailClear(); if (!success) xl6ErrorScopeCaptureFailure(&boundary);
    xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
    assert(success==!hit && outcome.ok==!hit && XL6_STATIC_CLEANUPS==before+1);
    if (output.Data) textExact(output,operation ? "b\0c\0z" : text.Data,5);
    else assert(hit);
    xrtErrorFree(outcome.error); xrtFree((void*)output.Data); xrtValueRelease(list);
    xrtClearError(); xlir_module_fini(); assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("List static language operation=%u point=%zu hit=%d balanced\n",operation,point,hit); return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2);
    if (!strcmp(argv[1],"vectors")) {
        xl6ProductMemoryBegin(SIZE_MAX); assert(xlir_module_init()); languageVectors();
        xlir_module_fini(); assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
        puts("Generated static List functions accept genuine native inputs: balanced"); return 0;
    }
    bool providers=!strcmp(argv[1],"providers"); assert(providers || !strcmp(argv[1],"oom"));
    for (unsigned operation=0;operation<2;++operation) {
        bool closed=false;
        for (size_t point=0;point<4096;++point) if (!languageAttempt(operation,point,providers)) {
            printf("List static closed-batch operation=%u runs=%zu first-no-hit=%zu\n",operation,point+1,point); closed=true; break;
        }
        assert(closed);
    }
    return 0;
}
