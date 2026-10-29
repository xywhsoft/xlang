/* Compile the actual host TU, not a copied resolver or a test adapter list.
 * The graph is real native Dict/Set storage and genuine XRT Value shells.
 * This validates host admission + phased plan retirement, not TCC deletion. */
#include "src/api/xl_api.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const xrttype* valueType(void) { return xrtTypeValue(); }
static xvalue* witness(uint64_t owner,uint64_t element)
{ return xl6SequenceReferenceCreateV1(1,owner,element,XL6_VALUE_ADMIT_ANY,true,element); }
static xvalue* dictCreate(void)
{
    XL6_DictLayoutV1 t={0xF910,{0xF911,0xF912,3,valueType()->Size,valueType()}};
    xvalue* seed=witness(t.value.identity,t.value.elementIdentity); assert(seed);
    xvalue* dict=xl6DictProjectionTakeV1(xl6DictCreateTakeV1(&t,seed),&t,NULL); assert(dict); return dict;
}
static xvalue* setCreate(void)
{
    XL6_SetLayoutV1 t={0xF920,{0xF921,0xF922,3,valueType()->Size,valueType()}};
    xvalue* seed=witness(t.value.identity,t.value.elementIdentity); assert(seed);
    xvalue* set=xl6SetProjectionTakeV1(xl6SetCreateTakeV1(&t,seed),&t,NULL); assert(set); return set;
}
static bool single(xrtownershipref ref,void* output)
{ xrtownershipref* edge=output; assert(!edge->Data); *edge=ref; return true; }
static void admitted(xvalue* shell,bool dict)
{
    xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    xrtownershipref owner={0},root=xrtValueOwnership(shell);
    assert(root.Ops->Trace(root.Data,single,&owner) && owner.Data);
    const xrtownershipadapterv1* expected=dict ? xl6DictOwnershipAdapterV1(owner) : xl6SetOwnershipAdapterV1(owner);
    assert(expected && xlModuleResolveOwnership(owner,NULL)==expected);
    const xrtownershipadapterv1* lifecycle=NULL; const xrtownershippreparationv1* preparation=NULL;
    const XL6_OwnershipFinishV1* completion=NULL;
    assert(xlModuleResolveOwnershipV3(owner,NULL,&lifecycle,&preparation,&completion) &&
        lifecycle==expected && !preparation && !completion);
    assert(xrtOwnershipScopeEnd(&freeze));
}
static bool attempt(size_t point,bool faults)
{
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    xvalue* dict=dictCreate(),*set=setCreate(); bool added=false;
    assert(xl6DictErasedSetV1(dict,(xstrview){"self\0a",6},dict) &&
        xl6DictErasedSetV1(dict,XRT_STR_LITERAL("same"),dict) &&
        xl6DictErasedSetV1(dict,XRT_STR_LITERAL("set"),set));
    assert(xl6SetErasedKeyV1(set,dict,XL6_SET_ADD_V1,&added) && added);
    assert(xl6SetErasedKeyV1(set,set,XL6_SET_ADD_V1,&added) && added);
    admitted(dict,true); admitted(set,false);
    /* Root is a live actual shell kept by genuine cycle edges. No fabricated
     * root node or reference-count subtraction authorizes collection. */
    xrtownershipref root=xrtValueOwnership(dict);
    xrtValueRelease(set); xrtValueRelease(dict);
    XL6_OwnershipPlan* plan=NULL;
    if (faults) assert(xrtMemDebugFailAfter(point));
    XL6_OwnershipPlanResult status=xl6OwnershipPlanTryCreateV3(root,xlModuleResolveOwnershipV3,NULL,&plan);
    bool hit=faults && xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) {
        assert(status==XL6_OWNERSHIP_PLAN_ERROR && !plan); xrtClearError();
        status=xl6OwnershipPlanTryCreateV3(root,xlModuleResolveOwnershipV3,NULL,&plan);
    }
    assert(status==XL6_OWNERSHIP_PLAN_READY && plan);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY); xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    if (faults) printf("host Dict/Set resolver point=%zu hit=%d balanced\n",point,hit);
    return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2);
    assert(xrtMemDebugEnable(true));
    if (!strcmp(argv[1],"vectors")) assert(!attempt(0,false));
    else {
        assert(!strcmp(argv[1],"oom")); size_t point=0;
        while(point<4096 && attempt(point,true)) ++point;
        assert(point<4096); printf("host Dict/Set resolver closed runs=%zu first-no-hit=%zu\n",point+1,point);
    }
    puts("[PASS] actual host Dict/Set resolver and cycle retirement"); return 0;
}
