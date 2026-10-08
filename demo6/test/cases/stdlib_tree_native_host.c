#include "src/runtime/xl6_tree.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_reference_value.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TREE_ID UINT64_C(0xD350)
#define KEY_ARRAY UINT64_C(0xD351)
#define KEY_ITEM UINT64_C(0xD352)
#define VALUE_ARRAY UINT64_C(0xD353)
#define VALUE_ITEM UINT64_C(0xD354)
static XL6_TreeLayoutV1 layout(unsigned transfer,const xrttype* type)
{
    return (XL6_TreeLayoutV1){TREE_ID,2,
        {KEY_ARRAY,KEY_ITEM,1,sizeof(int64_t),xrtTypeInt64()},
        {VALUE_ARRAY,VALUE_ITEM,transfer,type->Size,type}};
}
static xvalue* seed(const XL6_SequenceLayoutV1* field,unsigned rule)
{
    return field->transfer==3 ? xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,
        field->identity,field->elementIdentity,rule,false,field->elementIdentity) :
        xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,field->identity,
            field->elementIdentity,field->transfer,field->size,field->type,rule,false,field->elementIdentity);
}
static xvalue* create(const XL6_TreeLayoutV1* tuple,unsigned rule)
{
    /* Explicit order is important to preserve the first failed witness. */
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    xvalue* keys=seed(&tuple->key,XL6_VALUE_ADMIT_I64);
    if (!keys) xl6ErrorScopeCaptureFailure(&scope);
    xvalue* values=seed(&tuple->value,rule);
    if (!values) xl6ErrorScopeCaptureFailure(&scope);
    xvalue* result=xl6TreeCreateTakeV1(tuple,keys,values);
    if (!result) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return result;
}
typedef struct Edges { unsigned count; xrtownershipref first; bool same; } Edges;
static bool edge(xrtownershipref ref,void* context)
{
    Edges* edges=context;
    if (!edges->count) { edges->first=ref; edges->same=true; }
    else edges->same=edges->same && edges->first.Data==ref.Data && edges->first.Ops==ref.Ops;
    ++edges->count; return true;
}
static void integers(void)
{
    XL6_TreeLayoutV1 tuple=layout(1,xrtTypeInt64());
    xvalue* tree=create(&tuple,XL6_VALUE_ADMIT_I64); assert(tree);
    int64_t key=2,item=12,output=99; bool found=true,equal=false;
    assert(xl6TreeGetV1(tree,&tuple,&key,&output,false,&found) && !found && output==99);
    assert(xl6TreeKeyV1(tree,&tuple,&key,true,&found) && !found);
    for (key=3;key>0;--key) { item=key+10; assert(xl6TreeSetV1(tree,&tuple,&key,&item)); }
    assert(xl6TreeCountV1(tree,&tuple)==3);
    assert(xl6TreeEqualsV1(tree,tree,&tuple,&equal) && equal);
    xvalue* copy=xl6TreeCloneV1(tree,&tuple); assert(copy);
    assert(xl6TreeEqualsV1(tree,copy,&tuple,&equal) && equal);
    key=2; item=17; assert(xl6TreeSetV1(copy,&tuple,&key,&item));
    assert(xl6TreeEqualsV1(tree,copy,&tuple,&equal) && !equal);
    assert(xl6TreeMergeV1(tree,copy,&tuple,false));
    assert(xl6TreeGetV1(tree,&tuple,&key,&output,false,&found) && found && output==12);
    assert(xl6TreeMergeV1(tree,copy,&tuple,true));
    assert(xl6TreeGetV1(tree,&tuple,&key,&output,true,&found) && found && output==17);
    assert(xl6TreeCountV1(tree,&tuple)==2);
    int64_t first=-1,value=-1;
    assert(xl6TreeEndpointV1(tree,&tuple,0,NULL,&first,&value,&found) && found && first==1 && value==11);
    assert(xl6TreeEndpointV1(tree,&tuple,1,NULL,&first,&value,&found) && found && first==3 && value==13);
    key=2;
    assert(xl6TreeEndpointV1(tree,&tuple,2,&key,&first,&value,&found) && found && first==3);
    assert(xl6TreeEndpointV1(tree,&tuple,3,&key,&first,&value,&found) && found && first==3);
    assert(xl6TreeEndpointV1(tree,&tuple,0,NULL,&first,&first,&found) && !found && first==3);
    assert(xl6TreeClearV1(tree,&tuple));
    assert(xl6TreeTrimV1(tree,&tuple,0)>=0 && xl6TreeTrimV1(tree,&tuple,1)>=0);
    assert(xl6TreeEndpointV1(tree,&tuple,0,NULL,&first,&value,&found) && !found);
    xrtValueRelease(copy); xrtValueRelease(tree);
}
static void references(void)
{
    XL6_TreeLayoutV1 tuple=layout(3,xrtTypeValue());
    xvalue* tree=create(&tuple,XL6_VALUE_ADMIT_ANY); assert(tree);
    xvalue* item=xrtValueArray(); assert(item);
    assert(xrtValueArrayAppendNew(item,xrtValueInt(42)));
    int64_t key=1; assert(xl6TreeSetV1(tree,&tuple,&key,&item));
    key=2; assert(xl6TreeSetV1(tree,&tuple,&key,&item));
    xvalue* output=NULL; bool found=false,equal=false;
    assert(xl6TreeGetV1(tree,&tuple,&key,&output,false,&found) && found && output==item);
    xrtValueRelease(output);
    xvalue* copy=xl6TreeCloneV1(tree,&tuple); assert(copy);
    assert(xl6TreeEqualsV1(tree,copy,&tuple,&equal) && equal);
    /* Handle assignment retains the existing shell, unlike Array COW.
     * Its two language aliases still own only one wrapper edge. */
    xvalue* shell=xrtValueClone(tree); assert(shell==tree);
    xrtownershipref root=xrtValueOwnership(tree),other=xrtValueOwnership(shell);
    Edges a={0},b={0},slots={0};
    assert(root.Ops->Trace(root.Data,edge,&a) && other.Ops->Trace(other.Data,edge,&b));
    assert(a.count==1 && b.count==1 && a.first.Data==b.first.Data && a.first.Ops==b.first.Ops);
    size_t count=0; assert(a.first.Ops->Count(a.first.Data,&count) && count==1);
    assert(xl6TreeOwnershipAdapterV1(a.first));
    assert(a.first.Ops->Trace(a.first.Data,edge,&slots) && slots.count==2 && slots.same);
    xrtownershipref actual=xrtValueOwnership(item);
    assert(slots.first.Data==actual.Data && slots.first.Ops==actual.Ops);
    xrtValueRelease(shell); xrtValueRelease(copy); xrtValueRelease(tree); xrtValueRelease(item);
}
static void phased(void)
{
    XL6_TreeLayoutV1 tuple=layout(1,xrtTypeInt64());
    xvalue* tree=create(&tuple,XL6_VALUE_ADMIT_I64); assert(tree);
    int64_t key=1,item=7; assert(xl6TreeSetV1(tree,&tuple,&key,&item));
    xrtownershipref root=xrtValueOwnership(tree); Edges edges={0};
    assert(root.Ops->Trace(root.Data,edge,&edges) && edges.count==1);
    xrtownershipref owner=edges.first;
    const xrtownershipadapterv1* adapter=xl6TreeOwnershipAdapterV1(owner); assert(adapter);
    unsigned token=0; xrtownershipscope freeze={0};
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(adapter->Hold(owner.Data) && adapter->Claim(owner.Data,&token));
    assert(xrtOwnershipScopeEnd(&freeze));
    /* Finalize runs outside freeze, before Clear: real fields stay readable. */
    assert(xl6TreeCountV1(tree,&tuple)==1);
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    adapter->Clear(owner.Data,&token); assert(xrtOwnershipScopeEnd(&freeze));
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* prior=xrtGetError(); assert(prior);
    assert(adapter->Finish(owner.Data,&token) && xrtGetError()==prior);
    /* An idempotent terminal duty must also distinguish inherited failure. */
    assert(adapter->Finish(owner.Data,&token) && xrtGetError()==prior); xrtClearError();
    adapter->Drop(owner.Data); xrtValueRelease(tree);
}
static void rejected(void)
{
    XL6_TreeLayoutV1 tuple=layout(3,xrtTypeValue());
    xvalue* bytes=create(&tuple,XL6_VALUE_ADMIT_BYTES);
    xvalue* any=create(&tuple,XL6_VALUE_ADMIT_ANY); assert(bytes && any);
    bool equal=true;
    assert(!xl6TreeEqualsV1(bytes,any,&tuple,&equal) && equal); xrtClearError();
    assert(!xl6TreeMergeV1(bytes,any,&tuple,true)); xrtClearError();
    int64_t key=1; xvalue* item=xrtValueString((xstrview)XRT_STR_INIT("not bytes")); assert(item);
    assert(!xl6TreeSetV1(bytes,&tuple,&key,&item)); xrtClearError(); xrtValueRelease(item);
    assert(xl6TreeCountV1(bytes,&tuple)==0);
    XL6_TreeLayoutV1 bad=tuple; ++bad.value.elementIdentity;
    assert(xl6TreeCountV1(bytes,&bad)==-1); xrtClearError();
    bad=tuple; bad.keyOrder=3;
    assert(!create(&bad,XL6_VALUE_ADMIT_ANY)); xrtClearError();
    xvalue* wire=xrtValueArray(); assert(wire && xrtValueTypeIdBind(wire,TREE_ID));
    assert(xl6TreeCountV1(wire,&tuple)==-1); xrtClearError(); xrtValueRelease(wire);
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    assert(!xl6TreeCreateTakeV1(&tuple,NULL,NULL));
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok); xrtErrorFree(outcome.error);
    xrtValueRelease(bytes); xrtValueRelease(any);
}
typedef struct Reentrant { xvalue* tree; XL6_TreeLayoutV1 tuple; unsigned drops; } Reentrant;
static bool callbackRetain(void* data,void** out,void* context)
{ (void)context; *out=data; return true; }
static void callbackDrop(void* data,void* context)
{
    Reentrant* state=data; assert(state==context); ++state->drops;
    /* Reject reentry before storage access, then release the caller's last
     * shell. The outer operation's wrapper pin is now its only lifetime. */
    assert(xl6TreeCountV1(state->tree,&state->tuple)==-1);
    xrtValueRelease(state->tree); state->tree=NULL;
}
static const xvaluehandleops callbackOps={callbackRetain,callbackDrop,NULL,NULL};
static void reentrant(void)
{
    Reentrant state={.tuple=layout(3,xrtTypeValue())};
    state.tree=create(&state.tuple,XL6_VALUE_ADMIT_ANY); assert(state.tree);
    void* raw=&state; xvalue* item=xrtValueHandleTake(&raw,&callbackOps,&state); assert(item);
    int64_t key=1; assert(xl6TreeSetV1(state.tree,&state.tuple,&key,&item)); xrtValueRelease(item);
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    assert(!xl6TreeClearV1(state.tree,&state.tuple));
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(!outcome.ok && state.drops==1 && !state.tree);
    assert(outcome.error && xrtErrorKind(outcome.error)==XERR_STATE); xrtErrorFree(outcome.error);
}
static void fault(const char* operation,size_t point)
{
    XL6_TreeLayoutV1 tuple=layout(2,xl6StringTypeV1());
    xvalue* tree=NULL; xvalue* result=NULL;
    int64_t key=7,keyOut=99;
    xstrview item={"a\0\xE4\xBD\xA0",5};
    xstrview output={0}; assert(xrtTypeInitValue(tuple.value.type,&output));
    xstrview original=XRT_STR_INIT("original");
    assert(xrtTypeCopyValue(tuple.value.type,&output,&original));
    if (strcmp(operation,"create")) {
        tree=create(&tuple,XL6_VALUE_ADMIT_STRING); assert(tree);
        assert(xl6TreeSetV1(tree,&tuple,&key,&item));
    }
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    assert(xrtMemDebugFailAfter(point)); bool success=false,found=false;
    if (!strcmp(operation,"create")) { result=create(&tuple,XL6_VALUE_ADMIT_STRING); success=result!=NULL; }
    else if (!strcmp(operation,"clone")) { result=xl6TreeCloneV1(tree,&tuple); success=result!=NULL; }
    else if (!strcmp(operation,"endpoint")) success=xl6TreeEndpointV1(tree,&tuple,0,NULL,&keyOut,&output,&found);
    else if (!strcmp(operation,"projection")) {
        result=xl6TreeProjectTakeV1(tree,&tuple,false,seed(&tuple.value,XL6_VALUE_ADMIT_STRING),xl6StringConverterV1());
        success=result!=NULL;
    } else assert(0);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(hit ? !success && !outcome.ok : success && outcome.ok);
    if (!strcmp(operation,"endpoint")) {
        assert(hit ? !found && keyOut==99 && output.Size==original.Size && !memcmp(output.Data,original.Data,original.Size) :
            found && keyOut==key && output.Size==item.Size && !memcmp(output.Data,item.Data,item.Size));
    }
    if (tree) {
        assert(xl6TreeCountV1(tree,&tuple)==1);
        xstrview read={0}; assert(xrtTypeInitValue(tuple.value.type,&read));
        assert(xl6TreeGetV1(tree,&tuple,&key,&read,false,&found) && found && read.Size==item.Size && !memcmp(read.Data,item.Data,item.Size));
        xrtTypeDropValue(tuple.value.type,&read);
    }
    xrtTypeDropValue(tuple.value.type,&output); xrtErrorFree(outcome.error);
    xrtValueRelease(result); xrtValueRelease(tree); xrtClearError();
    printf("native tree %s point=%zu hit=%d complete-prefix balanced\n",operation,point,hit);
}
int main(int argc,char** argv)
{
    assert(argc==1 || argc==3); xl6ProductMemoryBegin(SIZE_MAX);
    if (argc==3) fault(argv[1],(size_t)strtoull(argv[2],NULL,10));
    else { integers(); references(); phased(); rejected(); reentrant(); }
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    if (argc==1) puts("native Tree results, shared shells, actual duplicate edges and reentrant last-shell retirement balanced");
    return 0;
}
