#define XL6_IMPORT_FIXTURE_MAIN xl6ArrayAlgebraImportFixtureMain
#include "test/cases/stdlib_array_import_native_host.c"
#include "src/runtime/xl6_array_algebra.h"
#include "src/api/xl_foreign_error.h"

static void values(const xvalue* value, size_t size)
{
    assert(xl6ArrayErasedSizeV1(value)==(int64_t)size);
    for (size_t i=0;i<size;++i) {
        xvalue* current=xl6ArrayErasedReadV1(value,(int64_t)i),*expected=item(false);
        assert(current && expected);
#if XL6_FACTORY_PROFILE == 4 || XL6_FACTORY_PROFILE == 7 || XL6_FACTORY_PROFILE == 8
        /* Independently created reference objects need not be equal. */
        assert(xrtValueType(current)==xrtValueType(expected));
#else
        assert(xrtValueEqual(current,expected));
#endif
        xrtValueRelease(current); xrtValueRelease(expected);
    }
}

static void algebraVectors(void)
{
    const char aText[]="a\0b", bText[]="\0\xE4\xBD\xA0";
    const char expectedText[]="a\0b\0\xE4\xBD\xA0";
    xvalue* aString=xrtValueString((xstrview){aText,sizeof(aText)-1});
    xvalue* bString=xrtValueString((xstrview){bText,sizeof(bText)-1}); assert(aString && bString);
    xrtSetErrorKind(XERR_IO); const xerror* stringIncoming=xrtGetError(); assert(stringIncoming);
    xvalue* text=xl6ValueStringConcatV1(aString,bString); xstrview actualText={0};
    assert(text && xrtGetError()==stringIncoming); xrtClearError();
    assert(xrtValueGetString(text,&actualText) && actualText.Size==sizeof(expectedText)-1 &&
        !memcmp(actualText.Data,expectedText,sizeof(expectedText)-1));
    assert(!xl6ValueStringConcatV1(aString,xrtValueBool(true))); error(XERR_TYPE);
    xrtValueRelease(text); xrtValueRelease(bString); xrtValueRelease(aString);
    xvalue* source=wire(false);
    xvalue* left=xl6ArrayImportTakeV1(seed(true),source);
    xvalue* right=xl6ArrayImportTakeV1(seed(true),source); assert(left && right);
    xvalue* joined=xl6ArrayConcatV1(left,right); assert(joined && joined!=left && joined!=right);
    values(joined,4); observed(left); observed(right);
    xvalue* self=xl6ArrayConcatV1(left,left); assert(self); values(self,4);
    xvalue* first=xl6ArrayErasedReadV1(left,0),*joinedFirst=xl6ArrayErasedReadV1(joined,0);
    assert(first && joinedFirst);
#if XL6_FACTORY_PROFILE == 4 || XL6_FACTORY_PROFILE == 7
    assert(first==joinedFirst);
#elif XL6_FACTORY_PROFILE == 8
    assert(xrtValueGetFuture(first)==xrtValueGetFuture(joinedFirst));
#else
    assert(xrtValueEqual(first,joinedFirst));
#endif
    xrtValueRelease(first); xrtValueRelease(joinedFirst);
#if XL6_FACTORY_PROFILE == 2
    xvalue* wrong=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,1001,3001,1,
        sizeof(int64_t),xrtTypeInt64(),XL6_VALUE_ADMIT_I64,false,3001); assert(wrong);
    xvalue* wrongAlias=xl6ArrayEraseV1(wrong,1001,3001,1,sizeof(int64_t),xrtTypeInt64(),NULL);
    assert(wrongAlias); xrtValueRelease(wrongAlias);
    assert(!xl6ArrayConcatV1(left,wrong)); error(XERR_TYPE); xrtValueRelease(wrong);
#elif XL6_FACTORY_PROFILE == 4
    xvalue* child=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,3001,4001,1,
        sizeof(int64_t),xrtTypeInt64(),XL6_VALUE_ADMIT_I64,false,4001); assert(child);
    xvalue* wrong=xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_ARRAY_V1,1001,3001,child,false); assert(wrong);
    xvalue* wrongAlias=xl6ArrayEraseV1(wrong,1001,3001,3,sizeof(xvalue*),xrtTypeValue(),NULL);
    assert(wrongAlias); xrtValueRelease(wrongAlias);
    assert(!xl6ArrayConcatV1(left,wrong)); error(XERR_TYPE); xrtValueRelease(wrong);
#endif
    const xrttype* type; unsigned int transfer;
    xvalue* shape=factory(XL6_SEQUENCE_ARRAY_V1,&type,&transfer); assert(shape); xrtValueRelease(shape);
    xvalue* copy=xl6SequenceCloneRoleV1(left,XL6_SEQUENCE_ARRAY_V1,1001,3001,transfer,type->Size,type);
    assert(copy && copy!=left); observed(copy); /* Projection must survive copy. */
    xvalue* languageCopy=xl6ValueCloneV1(left);
    assert(languageCopy && languageCopy!=left); observed(languageCopy);
    assert(xl6ArrayIsValueV1(left) && xl6ArrayIsValueV1(source));
    assert(!xl6ArrayIsValueV1(NULL) && !xl6ArrayIsValueV1(xrtValueBool(true)));
    xvalue* dynamic=xl6ArrayConcatDynamicV1(left,source);
    assert(dynamic && xrtValueType(dynamic)==XVALUE_ARRAY && xrtValueCount(dynamic)==4);
    xvalue* dynamicReverse=xl6ArrayConcatDynamicV1(source,left);
    assert(dynamicReverse && xrtValueType(dynamicReverse)==XVALUE_ARRAY && xrtValueCount(dynamicReverse)==4);
    XL6_Sequence* a=NULL; XL6_SequenceLayoutV1 layout; const xvalueconverter* converter=NULL;
    assert(xl6SequenceBeginErasedArrayV1(left,&a,&layout,&converter)==1);
    assert(!xl6ArrayConcatV1(left,right)); error(XERR_STATE);
    assert(!xl6ValueCloneV1(left)); error(XERR_STATE);
    assert(!xl6ArrayConcatDynamicV1(left,right)); error(XERR_STATE);
    xl6SequenceEndV1(a);
    assert(!xl6ArrayConcatV1(source,left)); error(XERR_TYPE);
    assert(!xl6ArrayConcatV1(left,source)); xrtClearError();
    shape=seed(false); assert(shape);
    assert(!xl6ArrayConcatV1(shape,right)); error(XERR_STATE); xrtValueRelease(shape);
    shape=factory(XL6_SEQUENCE_STACK_V1,&type,&transfer); assert(shape);
    assert(!xl6ArrayConcatV1(shape,right)); error(XERR_TYPE); xrtValueRelease(shape);
    assert(!xl6ArrayConcatV1(NULL,right)); error(XERR_TYPE);
    xrtSetErrorKind(XERR_IO); const xerror* incoming=xrtGetError(); assert(incoming);
    xvalue* wireJoined=xl6ArrayConcatV1(source,source); assert(wireJoined && xrtGetError()==incoming);
    xrtClearError(); assert(xrtValueCount(wireJoined)==4);
    xrtValueRelease(left); xrtValueRelease(right); xrtValueRelease(source);
    values(joined,4); values(self,4); observed(copy);
    xrtValueRelease(joined); xrtValueRelease(self); xrtValueRelease(copy); xrtValueRelease(wireJoined);
    xrtValueRelease(languageCopy); xrtValueRelease(dynamic); xrtValueRelease(dynamicReverse);
}

static int faultAlgebra(const char* operation, size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* source=wire(false),*left=xl6ArrayImportTakeV1(seed(true),source);
    xvalue* right=xl6ArrayImportTakeV1(seed(true),source); assert(left && right);
    const xrttype* type; unsigned int transfer;
    xvalue* shape=factory(XL6_SEQUENCE_ARRAY_V1,&type,&transfer); assert(shape); xrtValueRelease(shape);
    bool wireMode=!strcmp(operation,"wire"), selfMode=!strcmp(operation,"self"), copyMode=!strcmp(operation,"copy");
    bool languageCopyMode=!strcmp(operation,"language-copy"), dynamicMode=!strcmp(operation,"dynamic");
    bool stringMode=!strcmp(operation,"dynamic-string");
    xvalue* aString=NULL; xvalue* bString=NULL;
    if (stringMode) {
        aString=xrtValueString((xstrview){"a\0b",3});
        bString=xrtValueString((xstrview){"\0c",2}); assert(aString && bString);
    }
    assert(wireMode || selfMode || copyMode || languageCopyMode || dynamicMode || stringMode || !strcmp(operation,"native"));
    assert(xrtMemDebugFailAfter(point));
    xvalue* result=stringMode ? xl6ValueStringConcatV1(aString,bString) :
        languageCopyMode ? xl6ValueCloneV1(left) : dynamicMode ? xl6ArrayConcatDynamicV1(left,source) :
        copyMode ? xl6SequenceCloneRoleV1(left,XL6_SEQUENCE_ARRAY_V1,1001,3001,
        transfer,type->Size,type) : xl6ArrayConcatV1(wireMode ? source : left,wireMode ? source : selfMode ? left : right);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) { assert(!result); error(XERR_MEMORY); }
    else {
        assert(result);
        if (stringMode) {
            xstrview text={0}; assert(xrtValueGetString(result,&text));
            assert(text.Size==5 && !memcmp(text.Data,"a\0b\0c",5));
        } else if (dynamicMode) assert(xrtValueType(result)==XVALUE_ARRAY && xrtValueCount(result)==4);
        else if (!wireMode) values(result,copyMode || languageCopyMode ? 2 : 4);
    }
    observed(left); observed(right); assert(xrtValueCount(source)==2);
    xrtValueRelease(result); xrtValueRelease(left); xrtValueRelease(right); xrtValueRelease(source);
    xrtValueRelease(bString); xrtValueRelease(aString);
    balanced(&before);
    printf("array-algebra profile=%u operation=%s point=%zu hit=%u balanced\n",XL6_FACTORY_PROFILE,operation,point,hit);
    return 0;
}

int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc==3) return faultAlgebra(argv[1],(size_t)strtoull(argv[2],NULL,10));
    assert(argc==1); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    for (size_t i=0;i<100;++i) algebraVectors();
    balanced(&before); printf("array-algebra profile=%u: 100 source/destination cycles balanced\n",XL6_FACTORY_PROFILE);
    return 0;
}
