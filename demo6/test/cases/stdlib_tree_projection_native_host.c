#include "src/runtime/xl6_tree_projection.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_reference_value.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ID UINT64_C(0x9988)
#define ITEM UINT64_C(0x7766)
static xvalue* seed(unsigned rule,const xrttype* type,unsigned transfer)
{
    return xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,ID,ITEM,transfer,
        type->Size,type,rule,false,ITEM);
}
static xvalue* project(xtypedtree* tree,bool keys,xvalue* array,const xrttype* type,
    unsigned transfer,const xvalueconverter* converter)
{ return xl6TypedTreeProjectTakeV1(tree,keys,array,ID,ITEM,transfer,type->Size,type,converter); }
static int64_t integerAt(xvalue* array,int64_t index)
{
    xvalue* value=xl6ArrayErasedReadV1(array,index); int64_t item=0;
    assert(value && xrtValueGetInt(value,&item)); xrtValueRelease(value); return item;
}
static xtypedtree* integers(void)
{
    xtypedtree* tree=xrtTypedTreeCreate(xrtTypeInt64(),xrtTypeInt64()); assert(tree);
    for (int64_t key=3;key>0;--key) { int64_t item=key+10; assert(xrtTypedTreeSet(tree,&key,&item)); }
    return tree;
}
static void vectors(void)
{
    xtypedtree* tree=integers();
    xvalue* values=project(tree,false,seed(XL6_VALUE_ADMIT_I64,xrtTypeInt64(),1),xrtTypeInt64(),1,NULL);
    xvalue* keys=project(tree,true,seed(XL6_VALUE_ADMIT_I64,xrtTypeInt64(),1),xrtTypeInt64(),1,NULL);
    assert(values && keys && xl6SequenceAdmitsArrayV1(values,ID));
    for (int64_t i=0;i<3;++i) assert(integerAt(values,i)==i+11 && integerAt(keys,i)==i+1);
    int64_t key=2,item=99; assert(xrtTypedTreeSet(tree,&key,&item));
    xrtTypedTreeDestroy(tree); assert(integerAt(values,1)==12);
    xrtValueRelease(values); xrtValueRelease(keys);

    tree=xrtTypedTreeCreate(xrtTypeInt64(),xl6StringTypeV1()); assert(tree);
    const xstrview texts[]={XRT_STR_INIT(""),{"a\0b",3},{"\xE4\xBD\xA0\0b",5}};
    for (size_t i=0;i<3;++i) { key=(int64_t)i; assert(xrtTypedTreeSet(tree,&key,&texts[i])); }
    values=project(tree,false,seed(XL6_VALUE_ADMIT_STRING,xl6StringTypeV1(),2),xl6StringTypeV1(),2,xl6StringConverterV1());
    assert(values); xrtTypedTreeDestroy(tree);
    for (size_t i=0;i<3;++i) { xvalue* boxed=xl6ArrayErasedReadV1(values,(int64_t)i); xstrview text={0};
        assert(boxed && xrtValueGetString(boxed,&text) && text.Size==texts[i].Size && !memcmp(text.Data,texts[i].Data,text.Size));
        xrtValueRelease(boxed); }
    xrtValueRelease(values);

    tree=xrtTypedTreeCreate(xrtTypeInt64(),xl6SequenceStorageTypeV1(3,xrtTypeValue())); assert(tree);
    xvalue* value=xrtValueArray(); assert(value); assert(xrtValueArrayAppendNew(value,xrtValueInt(19)));
    key=1; assert(xrtTypedTreeSet(tree,&key,&value)); xrtValueRelease(value);
    xvalue* stored=*(xvalue* const*)xrtTypedTreeConstGet(tree,&key);
    values=project(tree,false,xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,ID,ITEM,
        XL6_VALUE_ADMIT_ANY,false,ITEM),xrtTypeValue(),3,xl6ArrayReferenceConverterV1());
    assert(values); value=xl6ArrayErasedReadV1(values,0); assert(value==stored); xrtValueRelease(value);
    xrtTypedTreeDestroy(tree); value=xl6ArrayErasedReadV1(values,0);
    assert(value && xrtValueCount(value)==1); xrtValueRelease(value); xrtValueRelease(values);
}
static void rejected(void)
{
    xtypedtree* tree=integers();
    xvalue* wire=xrtValueArray(); assert(wire && xrtValueTypeIdBind(wire,ID));
    assert(!project(tree,false,wire,xrtTypeInt64(),1,NULL)); xrtClearError();
    assert(!project(NULL,false,seed(XL6_VALUE_ADMIT_I64,xrtTypeInt64(),1),xrtTypeInt64(),1,NULL)); xrtClearError();
    assert(!project(tree,false,seed(XL6_VALUE_ADMIT_U64,xrtTypeUInt64(),1),xrtTypeUInt64(),1,NULL)); xrtClearError();
    /* A logical narrowing failure after a valid prefix cannot leak an owner
     * or alter the source. This is not a coercing wire/native fallback. */
    int64_t key=3,item=256; assert(xrtTypedTreeSet(tree,&key,&item));
    assert(!project(tree,false,seed(XL6_VALUE_ADMIT_U8,xrtTypeInt64(),1),xrtTypeInt64(),1,NULL)); xrtClearError();
    assert(xrtTypedTreeCount(tree)==3 && *(const int64_t*)xrtTypedTreeConstGet(tree,&key)==256);
    xerror* first=xrtErrorCreate(XERR_MEMORY,"test",31,"seed construction failure"); assert(first);
    xrtSetErrorTake(first);
    assert(!project(tree,false,NULL,xrtTypeInt64(),1,NULL) && xrtGetError()==first); xrtClearError();
    xl6_error_scope outer={0}; assert(xl6ErrorScopeBegin(&outer));
    assert(!project(tree,false,NULL,xrtTypeInt64(),1,NULL));
    xl6_error_result outcome=xl6ErrorScopeEnd(&outer); assert(!outcome.ok); xrtErrorFree(outcome.error);
    xrtTypedTreeDestroy(tree);
}
static void fault(size_t point)
{
    xtypedtree* tree=xrtTypedTreeCreate(xrtTypeInt64(),xl6StringTypeV1()); assert(tree);
    for (int64_t key=0;key<4;++key) { xstrview text={"a\0b",3}; assert(xrtTypedTreeSet(tree,&key,&text)); }
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    assert(xrtMemDebugFailAfter(point));
    xvalue* values=project(tree,false,seed(XL6_VALUE_ADMIT_STRING,xl6StringTypeV1(),2),
        xl6StringTypeV1(),2,xl6StringConverterV1());
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(hit ? !values && !outcome.ok : values && outcome.ok);
    assert(xrtTypedTreeCount(tree)==4);
    for (int64_t key=0;key<4;++key) { const xstrview* text=xrtTypedTreeConstGet(tree,&key);
        assert(text && text->Size==3 && !memcmp(text->Data,"a\0b",3)); }
    xrtErrorFree(outcome.error); xrtValueRelease(values); xrtTypedTreeDestroy(tree); xrtClearError();
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    printf("tree projection point=%zu hit=%d complete-prefix balanced\n",point,hit);
}
int main(int argc,char** argv)
{
    assert(argc==1 || argc==2); xl6ProductMemoryBegin(SIZE_MAX);
    if (argc==2) { fault((size_t)strtoull(argv[1],NULL,10)); return 0; }
    vectors(); rejected(); assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("tree projection ordered physical snapshots, owned references and rejected schemas balanced"); return 0;
}
