#include "src/runtime/xl6_tree.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_reference_value.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { TREE=0xF600,KEYS=0xF601,KEY=0xF602,VALUES=0xF603,VALUE=0xF604 };
static XL6_TreeLayoutV2 layout(void)
{
    return (XL6_TreeLayoutV2){TREE,XL6_TREE_KEY_BYTES_V1,
        {KEYS,KEY,3,sizeof(xvalue*),xrtTypeValue()},
        {VALUES,VALUE,1,sizeof(int64_t),xrtTypeInt64()},xl6TreeBytesKeyCodecV1()};
}
static xvalue* keySeed(unsigned rule,bool nullable)
{ return xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,KEYS,KEY,rule,nullable,KEY); }
static xvalue* valueSeed(void)
{ return xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,VALUES,VALUE,1,sizeof(int64_t),xrtTypeInt64(),XL6_VALUE_ADMIT_I64,false,VALUE); }
static xvalue* create(const XL6_TreeLayoutV2* tuple,unsigned rule,bool nullable)
{
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
    xvalue* keys=keySeed(rule,nullable);if(!keys)xl6ErrorScopeCaptureFailure(&scope);
    xvalue* values=valueSeed();if(!values)xl6ErrorScopeCaptureFailure(&scope);
    xvalue* tree=xl6TreeCreateTakeV1(tuple,keys,values);if(!tree)xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    if(!result.ok)xl6ErrorScopePropagateFailure(result);
    return tree;
}
static void refused(const XL6_TreeLayoutV2* tuple,unsigned rule,bool nullable)
{ assert(!create(tuple,rule,nullable) && xrtGetError());xrtClearError(); }
typedef struct Edges { unsigned count;xrtownershipref refs[8]; } Edges;
static bool edge(xrtownershipref ref,void* context)
{ Edges* out=context;assert(out->count<8);out->refs[out->count++]=ref;return true; }
static void ordinary(void)
{
    XL6_TreeLayoutV2 tuple=layout();xvalue* tree=create(&tuple,XL6_VALUE_ADMIT_BYTES,false);assert(tree);
    const unsigned char bytes[]={0,255};
    xvalue* a=xrtValueBytes((xbytesview){bytes,1}),*b=xrtValueBytes((xbytesview){bytes,2});assert(a && b);
    int64_t first=7,last=9;assert(xl6TreeSetV1(tree,&tuple,&b,&last) && xl6TreeSetV1(tree,&tuple,&a,&first));
    assert(xl6TreeCountV1(tree,&tuple)==2);
    xvalue* key=NULL;int64_t item=0;bool found=false;
    assert(xl6TreeQueryV2(tree,&tuple,XL6_TREE_QUERY_FIRST_V2,NULL,true,
        (XL6_TreeQuerySlotV2){&key,sizeof(key)},(XL6_TreeQuerySlotV2){&item,sizeof(item)},&found));
    /* Immutable byte atoms are shared, not unnecessarily duplicated. */
    assert(found && key==a && item==first);xrtValueRelease(key);
    xrtownershipref root=xrtValueOwnership(tree);Edges owners={0},slots={0};
    assert(root.Ops->Trace(root.Data,edge,&owners) && owners.count==1);
    assert(owners.refs[0].Ops->Trace(owners.refs[0].Data,edge,&slots) && slots.count==2);
    assert(slots.refs[0].Data==xrtValueOwnership(a).Data && slots.refs[1].Data==xrtValueOwnership(b).Data);
    xvalue* projected=xl6TreeProjectTakeV1(tree,&tuple,true,keySeed(XL6_VALUE_ADMIT_BYTES,false),xl6ArrayReferenceConverterV1());assert(projected);
    xvalue* clone=xl6TreeCloneV1(tree,&tuple);assert(clone);bool equal=false;
    assert(xl6TreeEqualsV1(tree,clone,&tuple,&equal) && equal);
    xvalue* invalid=xrtValueString((xstrview)XRT_STR_INIT("wrong"));assert(invalid);
    assert(!xl6TreeSetV1(tree,&tuple,&invalid,&first));xrtClearError();xrtValueRelease(invalid);
    invalid=xrtValueBytes((xbytesview){bytes,1});assert(invalid && xrtValueTypeIdBind(invalid,KEY+1));
    assert(!xl6TreeSetV1(tree,&tuple,&invalid,&first));xrtClearError();xrtValueRelease(invalid);
    XL6_TreeLayoutV2 bad=tuple;bad.keyCodec=NULL;refused(&bad,XL6_VALUE_ADMIT_BYTES,false);
    bad=tuple;bad.keyOrder=XL6_TREE_KEY_SIGNED_I64_V1;refused(&bad,XL6_VALUE_ADMIT_BYTES,false);
    XL6_TreeKeyCodecV1 codec=*tuple.keyCodec;codec.type=xrtTypeValue();bad=tuple;bad.keyCodec=&codec;
    refused(&bad,XL6_VALUE_ADMIT_BYTES,false);
    refused(&tuple,XL6_VALUE_ADMIT_ANY,false);refused(&tuple,XL6_VALUE_ADMIT_BYTES,true);
    xrtValueRelease(clone);xrtValueRelease(projected);xrtValueRelease(a);xrtValueRelease(b);xrtValueRelease(tree);
}
static void fault(const char* operation,size_t point)
{
    XL6_TreeLayoutV2 tuple=layout();bool creating=!strcmp(operation,"create");
    xvalue* tree=creating ? NULL : create(&tuple,XL6_VALUE_ADMIT_BYTES,false);assert(creating || tree);
    const unsigned char bytes[]={97,0,255};xvalue* key=xrtValueBytes((xbytesview){bytes,sizeof(bytes)});assert(key);
    int64_t item=17;bool setting=!strcmp(operation,"set");
    if(tree && !setting)assert(xl6TreeSetV1(tree,&tuple,&key,&item));
    xvalue* result=NULL,*output=NULL;int64_t read=0;bool found=false,success=false;
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));assert(xrtMemDebugFailAfter(point));
    if(creating) {result=create(&tuple,XL6_VALUE_ADMIT_BYTES,false);success=result!=NULL;}
    else if(setting) success=xl6TreeSetV1(tree,&tuple,&key,&item);
    else if(!strcmp(operation,"clone")) {result=xl6TreeCloneV1(tree,&tuple);success=result!=NULL;}
    else if(!strcmp(operation,"keys")) {
        result=xl6TreeProjectTakeV1(tree,&tuple,true,keySeed(XL6_VALUE_ADMIT_BYTES,false),xl6ArrayReferenceConverterV1());success=result!=NULL;
    } else if(!strcmp(operation,"first")) success=xl6TreeQueryV2(tree,&tuple,XL6_TREE_QUERY_FIRST_V2,NULL,true,
        (XL6_TreeQuerySlotV2){&output,sizeof(output)},(XL6_TreeQuerySlotV2){&read,sizeof(read)},&found);
    else assert(false);
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    if(!success || xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(hit ? !success && !outcome.ok : success && outcome.ok);
    if(!strcmp(operation,"first"))assert(hit ? !found && output==NULL : found && output==key && read==item);
    xrtErrorFree(outcome.error);xrtClearError();
    if(tree)assert(xl6TreeCountV1(tree,&tuple)==(setting && hit ? 0 : 1));
    xrtValueRelease(output);xrtValueRelease(result);xrtValueRelease(tree);xrtValueRelease(key);xrtClearError();
    printf("tree-key %s point=%zu hit=%d complete-prefix balanced\n",operation,point,hit);
}
int main(int argc,char** argv)
{
    assert(argc==1 || argc==3);xl6ProductMemoryBegin(SIZE_MAX);
    if(argc==1)ordinary();else fault(argv[1],(size_t)strtoull(argv[2],NULL,10));
    assert(!xrtGetError() && !xl6ProductMemoryEnd());xl6ProductMemoryVerify();
    if(argc==1)puts("native byte-key ordering, exact admission, atom copy and physical ownership edges balanced");
    return 0;
}
