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
    XL6_Sequence* a=NULL; XL6_SequenceLayoutV1 layout; const xvalueconverter* converter=NULL;
    assert(xl6SequenceBeginErasedArrayV1(left,&a,&layout,&converter)==1);
    assert(!xl6ArrayConcatV1(left,right)); error(XERR_STATE);
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
}

static int faultAlgebra(const char* operation, size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* source=wire(false),*left=xl6ArrayImportTakeV1(seed(true),source);
    xvalue* right=xl6ArrayImportTakeV1(seed(true),source); assert(left && right);
    const xrttype* type; unsigned int transfer;
    xvalue* shape=factory(XL6_SEQUENCE_ARRAY_V1,&type,&transfer); assert(shape); xrtValueRelease(shape);
    bool wireMode=!strcmp(operation,"wire"), selfMode=!strcmp(operation,"self"), copyMode=!strcmp(operation,"copy");
    assert(wireMode || selfMode || copyMode || !strcmp(operation,"native"));
    assert(xrtMemDebugFailAfter(point));
    xvalue* result=copyMode ? xl6SequenceCloneRoleV1(left,XL6_SEQUENCE_ARRAY_V1,1001,3001,
        transfer,type->Size,type) : xl6ArrayConcatV1(wireMode ? source : left,wireMode ? source : selfMode ? left : right);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) { assert(!result); error(XERR_MEMORY); }
    else { assert(result); if (!wireMode) values(result,copyMode ? 2 : 4); }
    observed(left); observed(right); assert(xrtValueCount(source)==2);
    xrtValueRelease(result); xrtValueRelease(left); xrtValueRelease(right); xrtValueRelease(source);
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
