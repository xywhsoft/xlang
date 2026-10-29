#include "src/runtime/xl6_tree.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_string_type.h"
#include "src/runtime/xl6_scalar_type_abi.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned failCopy,copyCalls;
static bool reportCopy,partialCopy,failDrop;
bool xl6TreeQueryProbeCopy(const xrttype* type,void* target,const void* source)
{
    if (!failCopy || ++copyCalls!=failCopy) return xrtTypeCopyValue(type,target,source);
    if (partialCopy) assert(xrtTypeCopyValue(type,target,source));
    if (reportCopy) xrtSetErrorKind(XERR_RANGE);
    return false;
}
void xl6TreeQueryProbeDrop(const xrttype* type,void* slot)
{
    xrtTypeDropValue(type,slot);
    if (failDrop) xrtSetErrorKind(XERR_IO);
}
static xvalue* seed(const XL6_SequenceLayoutV1* field,unsigned rule,bool nullable)
{
    return field->transfer==3 ? xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,
        field->identity,field->elementIdentity,rule,nullable,field->elementIdentity) :
        xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,field->identity,field->elementIdentity,
            field->transfer,field->size,field->type,rule,nullable,field->elementIdentity);
}
static XL6_TreeLayoutV2 tuple(unsigned keyTransfer,const xrttype* key,unsigned order,
    unsigned transfer,const xrttype* value)
{
    return (XL6_TreeLayoutV2){UINT64_C(0xD370),order,
        {UINT64_C(0xD371),UINT64_C(0xD372),keyTransfer,key->Size,key},
        {UINT64_C(0xD373),UINT64_C(0xD374),transfer,value->Size,value},NULL};
}
static xvalue* create(const XL6_TreeLayoutV2* layout,unsigned keyRule,unsigned rule,bool nullable)
{
    xvalue* keys=seed(&layout->key,keyRule,false); assert(keys);
    xvalue* values=seed(&layout->value,rule,nullable); assert(values);
    xvalue* tree=xl6TreeCreateTakeV1(layout,keys,values); assert(tree); return tree;
}
static bool query(const xvalue* tree,const XL6_TreeLayoutV2* layout,XL6_TreeQueryV2 op,
    const void* search,bool required,void* key,void* value,bool* found)
{
    return xl6TreeQueryV2(tree,layout,op,search,required,
        (XL6_TreeQuerySlotV2){key,key ? layout->key.size : 0},
        (XL6_TreeQuerySlotV2){value,layout->value.size},found);
}
static void integers(void)
{
    XL6_TreeLayoutV2 layout=tuple(1,xrtTypeInt64(),2,1,xrtTypeInt64());
    xvalue* tree=create(&layout,XL6_VALUE_ADMIT_I64,XL6_VALUE_ADMIT_I64,false);
    int64_t search=2,key=99,value=99; bool found=false;
    for (unsigned op=0;op<=XL6_TREE_QUERY_UPPER_V2;++op) {
        bool pair=op>=XL6_TREE_QUERY_FIRST_V2;
        const void* input=op<2 || op>=4 ? &search : NULL;
        key=99; value=99;
        assert(query(tree,&layout,(XL6_TreeQueryV2)op,input,false,pair ? &key : NULL,&value,&found));
        assert(!found && value==0 && (!pair || key==0));
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        assert(!query(tree,&layout,(XL6_TreeQueryV2)op,input,true,pair ? &key : NULL,&value,&found));
        xl6_error_result result=xl6ErrorScopeEnd(&scope);
        assert(!result.ok && result.error && xrtErrorKind(result.error)==XERR_RANGE);
        assert(!found && value==0 && (!pair || key==0)); xrtErrorFree(result.error);
    }
    for (int64_t at=1;at<=3;++at) { value=at+10; assert(xl6TreeSetV1(tree,&layout,&at,&value)); }
    for (unsigned op=0;op<=XL6_TREE_QUERY_UPPER_V2;++op) {
        bool pair=op>=XL6_TREE_QUERY_FIRST_V2;
        const void* input=op<2 || op>=4 ? &search : NULL;
        assert(xrtMemDebugFailAfter(0));
        assert(query(tree,&layout,(XL6_TreeQueryV2)op,input,true,pair ? &key : NULL,&value,&found));
        assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
        assert(found && value==(op==2 ? 11 : op==0 || op==1 ? 12 : 13));
        if (pair) assert(key==(op==2 ? 1 : 3));
        assert(xl6TreeCountV1(tree,&layout)==(op ? 2 : 3));
    }
    /* A query is independently successful in the presence of an older error. */
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* prior=xrtGetError(); assert(prior);
    assert(query(tree,&layout,XL6_TREE_QUERY_FIRST_V2,NULL,false,&key,&value,&found) && found);
    assert(xrtGetError()==prior); xrtClearError(); xrtValueRelease(tree);
}
static void text(void)
{
    XL6_TreeLayoutV2 layout=tuple(2,xl6StringTypeV1(),5,2,xl6StringTypeV1());
    xvalue* tree=create(&layout,XL6_VALUE_ADMIT_STRING,XL6_VALUE_ADMIT_STRING,false);
    xstrview key={"a\0b",3},value={"\xE4\xBD\xA0\0\xE5\xA5\xBD",7},keyOut={0},valueOut={0};
    assert(xl6TreeSetV1(tree,&layout,&key,&value)); bool found=false;
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    assert(query(tree,&layout,XL6_TREE_QUERY_FIRST_V2,NULL,true,&keyOut,&valueOut,&found) && found);
    xrtMemDebugSnapshot(&after); assert(after.AllocCount-before.AllocCount==2);
    assert(keyOut.Size==key.Size && !memcmp(keyOut.Data,key.Data,key.Size));
    assert(valueOut.Size==value.Size && !memcmp(valueOut.Data,value.Data,value.Size));
    xrtValueRelease(tree);
    assert(!memcmp(keyOut.Data,key.Data,key.Size) && !memcmp(valueOut.Data,value.Data,value.Size));
    xrtTypeDropValue(layout.key.type,&keyOut); xrtTypeDropValue(layout.value.type,&valueOut);
    tree=create(&layout,XL6_VALUE_ADMIT_STRING,XL6_VALUE_ADMIT_STRING,false);
    assert(query(tree,&layout,XL6_TREE_QUERY_FIRST_V2,NULL,false,&keyOut,&valueOut,&found));
    assert(!found && !keyOut.Data && !valueOut.Data); xrtValueRelease(tree);
}
static void reference(void)
{
    XL6_TreeLayoutV2 layout=tuple(1,xrtTypeInt64(),2,3,xrtTypeValue());
    xvalue* tree=create(&layout,XL6_VALUE_ADMIT_I64,XL6_VALUE_ADMIT_ANY,true);
    int64_t key=1,keyOut=0; xvalue* item=NULL; xvalue* output=NULL; bool found=false;
    assert(xl6TreeSetV1(tree,&layout,&key,&item));
    assert(query(tree,&layout,XL6_TREE_QUERY_GET_V2,&key,false,NULL,&output,&found));
    assert(found && output==NULL);
    key=2; assert(query(tree,&layout,XL6_TREE_QUERY_GET_V2,&key,false,NULL,&output,&found));
    assert(!found && output==NULL);
    item=xrtValueArray(); assert(item); assert(xrtValueArrayAppendNew(item,xrtValueInt(42)));
    assert(xl6TreeSetV1(tree,&layout,&key,&item));
    assert(query(tree,&layout,XL6_TREE_QUERY_LAST_V2,NULL,true,&keyOut,&output,&found) && found);
    assert(keyOut==key && output==item); xrtValueRelease(output); output=NULL;
    assert(query(tree,&layout,XL6_TREE_QUERY_TAKE_V2,&key,true,NULL,&output,&found) && found);
    assert(output==item && xl6TreeCountV1(tree,&layout)==1);
    xrtValueRelease(tree); xrtValueRelease(item);
    assert(xrtValueCount(output)==1); xrtValueRelease(output);
}
static void nullable(void)
{
    XL6_TreeLayoutV2 layout=tuple(1,xrtTypeInt64(),2,1,xl6NullableI64TypeV1());
    xvalue* tree=create(&layout,XL6_VALUE_ADMIT_I64,XL6_VALUE_ADMIT_I64,true);
    int64_t key=1; XL6_NullableI64V1 empty={0},out={.hasValue=true,.value=8}; bool found=false;
    assert(xl6TreeSetV1(tree,&layout,&key,&empty));
    assert(query(tree,&layout,XL6_TREE_QUERY_GET_V2,&key,false,NULL,&out,&found));
    assert(found && !out.hasValue);
    key=2; assert(query(tree,&layout,XL6_TREE_QUERY_GET_V2,&key,false,NULL,&out,&found));
    assert(!found && !out.hasValue); xrtValueRelease(tree);

    layout=tuple(1,xrtTypeInt64(),2,2,xl6NullableStringTypeV1());
    tree=create(&layout,XL6_VALUE_ADMIT_I64,XL6_VALUE_ADMIT_STRING,true);
    xstrview nullText={0},textOut={0}; key=1;
    assert(xl6TreeSetV1(tree,&layout,&key,&nullText));
    assert(query(tree,&layout,XL6_TREE_QUERY_GET_V2,&key,false,NULL,&textOut,&found));
    assert(found && !textOut.Data && textOut.Size==0);
    key=2; assert(query(tree,&layout,XL6_TREE_QUERY_GET_V2,&key,false,NULL,&textOut,&found));
    assert(!found && !textOut.Data && textOut.Size==0); xrtValueRelease(tree);
}
static void rejected(void)
{
    XL6_TreeLayoutV2 layout=tuple(1,xrtTypeInt64(),2,1,xrtTypeInt64());
    xvalue* tree=create(&layout,XL6_VALUE_ADMIT_I64,XL6_VALUE_ADMIT_I64,false);
    int64_t search=1,key=71,value=72; bool found=true;
    assert(xl6TreeSetV1(tree,&layout,&search,&value));
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
#define REJECT(OP,INPUT,K,V,STATUS) do { \
    assert(!xl6TreeQueryV2(tree,&layout,OP,INPUT,false,K,V,STATUS)); \
    assert(key==71 && value==72 && found); xrtClearError(); } while(0)
    XL6_TreeQuerySlotV2 k={&key,sizeof(key)},v={&value,sizeof(value)},none={0};
    REJECT((XL6_TreeQueryV2)99,NULL,k,v,&found);
    REJECT(XL6_TREE_QUERY_GET_V2,&search,k,v,&found);
    REJECT(XL6_TREE_QUERY_FIRST_V2,&search,k,v,&found);
    REJECT(XL6_TREE_QUERY_FIRST_V2,NULL,k,k,&found);
    REJECT(XL6_TREE_QUERY_GET_V2,&value,none,v,&found);
    REJECT(XL6_TREE_QUERY_FIRST_V2,NULL,k,v,(bool*)&value);
    REJECT(XL6_TREE_QUERY_GET_V2,&search,none,((XL6_TreeQuerySlotV2){&value,sizeof(value)-1}),&found);
    _Alignas(int64_t) unsigned char raw[sizeof(int64_t)+1]={0};
    REJECT(XL6_TREE_QUERY_GET_V2,&search,none,((XL6_TreeQuerySlotV2){raw+1,sizeof(value)}),&found);
    REJECT(XL6_TREE_QUERY_GET_V2,&search,none,((XL6_TreeQuerySlotV2){(void*)(UINTPTR_MAX-3),sizeof(value)}),&found);
#undef REJECT
    xl6_error_result result=xl6ErrorScopeEnd(&scope); assert(!result.ok); xrtErrorFree(result.error);
    xrtValueRelease(tree);
    layout=tuple(1,xrtTypeInt64(),2,2,xl6StringTypeV1());
    tree=create(&layout,XL6_VALUE_ADMIT_I64,XL6_VALUE_ADMIT_STRING,false);
    xstrview item=XRT_STR_INIT("value"),out={0}; assert(xl6TreeSetV1(tree,&layout,&search,&item));
    assert(xrtTypeCopyValue(layout.value.type,&out,&item)); const char* old=out.Data;
    assert(!query(tree,&layout,XL6_TREE_QUERY_GET_V2,&search,false,NULL,&out,&found));
    assert(out.Data==old && out.Size==item.Size); xrtClearError();
    xrtTypeDropValue(layout.value.type,&out); xrtValueRelease(tree);
}
static void fault(const char* mode,size_t point)
{
    XL6_TreeLayoutV2 layout=tuple(2,xl6StringTypeV1(),5,2,xl6StringTypeV1());
    xvalue* tree=create(&layout,XL6_VALUE_ADMIT_STRING,XL6_VALUE_ADMIT_STRING,false);
    xstrview key={"a\0b",3},item={"\xE4\xBD\xA0\0b",5},keyOut={0},out={0};
    assert(xl6TreeSetV1(tree,&layout,&key,&item));
    bool pair=!strcmp(mode,"pair"),take=!strcmp(mode,"take"); bool found=false;
    XL6_TreeQueryV2 operation=pair ? XL6_TREE_QUERY_FIRST_V2 : take ? XL6_TREE_QUERY_TAKE_V2 : XL6_TREE_QUERY_GET_V2;
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    assert(xrtMemDebugFailAfter(point));
    bool ok=query(tree,&layout,operation,pair ? NULL : &key,false,pair ? &keyOut : NULL,&out,&found);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    assert(hit ? !ok && !result.ok && !found && !keyOut.Data && !out.Data : ok && result.ok && found);
    assert(xl6TreeCountV1(tree,&layout)==(take && ok ? 0 : 1));
    if (hit) {
        assert(query(tree,&layout,operation,pair ? NULL : &key,false,pair ? &keyOut : NULL,&out,&found) && found);
    }
    assert(out.Size==item.Size && !memcmp(out.Data,item.Data,item.Size));
    xrtTypeDropValue(layout.key.type,&keyOut); xrtTypeDropValue(layout.value.type,&out);
    xrtErrorFree(result.error); xrtValueRelease(tree);
    printf("query %s point=%zu hit=%d complete-prefix balanced\n",mode,point,hit);
}
static void copyFaults(void)
{
    for (unsigned at=1;at<=2;++at) for(unsigned reported=0;reported<=1;++reported)
    for(unsigned partial=0;partial<=1;++partial) {
        XL6_TreeLayoutV2 layout=tuple(2,xl6StringTypeV1(),5,2,xl6StringTypeV1());
        xvalue* tree=create(&layout,XL6_VALUE_ADMIT_STRING,XL6_VALUE_ADMIT_STRING,false);
        xstrview key={"a\0b",3},item={"v\0w",3},keyOut={0},out={0}; bool found=false;
        assert(xl6TreeSetV1(tree,&layout,&key,&item));
        failCopy=at; copyCalls=0; reportCopy=reported!=0; partialCopy=partial!=0; failDrop=true;
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        assert(!query(tree,&layout,XL6_TREE_QUERY_FIRST_V2,NULL,false,&keyOut,&out,&found));
        xl6_error_result result=xl6ErrorScopeEnd(&scope);
        assert(!result.ok && !found && !keyOut.Data && !out.Data);
        assert(reported ? result.error && xrtErrorKind(result.error)==XERR_RANGE : result.error==NULL);
        failCopy=0; failDrop=false; xrtErrorFree(result.error);
        assert(query(tree,&layout,XL6_TREE_QUERY_FIRST_V2,NULL,false,&keyOut,&out,&found) && found);
        xrtTypeDropValue(layout.key.type,&keyOut); xrtTypeDropValue(layout.value.type,&out); xrtValueRelease(tree);
    }
}
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    if (argc==3) fault(argv[1],(size_t)strtoull(argv[2],NULL,10));
    else if (argc==2 && !strcmp(argv[1],"copy-faults")) copyFaults();
    else if (argc==2 && !strcmp(argv[1],"repeat")) {
        for (unsigned i=0;i<1000;++i) { integers(); text(); reference(); nullable(); rejected(); }
    }
    else { assert(argc==1); integers(); text(); reference(); nullable(); rejected(); }
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("fresh query slots balanced"); return 0;
}
