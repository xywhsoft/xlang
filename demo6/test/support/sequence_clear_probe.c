/* Shared actual native/language callback fixture. No replacement container,
 * ownership adapter, error bridge, or generated-code patch. */
#include "src/runtime/xl6_stack.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include <assert.h>
#include <stdint.h>

static xvalue* ClearReceiver;
static xerror* ClearErrors[3];
static unsigned int ClearMode, ClearMask, ClearDrops, ClearOrder[3];
static uint64_t ClearIdentity, ClearElement;
static int ClearItems[3]={1,2,3};
static bool ClearProbe, ClearRelease;
#define CLEAR_LAYOUT ClearIdentity,ClearElement,3,sizeof(xvalue*),xrtTypeValue()

static bool clearNow(void)
{
#if defined(XL6_SEQUENCE_CLEAR_NATIVE)
    if (ClearMode==0) return xl6StackClearV1(ClearReceiver,CLEAR_LAYOUT);
    if (ClearMode==1) return xl6ArrayClearV1(ClearReceiver,CLEAR_LAYOUT);
    return xl6ArrayErasedClearV1(ClearReceiver);
#else
    return xl6StackClearV1(ClearReceiver,CLEAR_LAYOUT);
#endif
}
static void clearDrop(void* opaque, void* context)
{
    (void)context;
    unsigned int item=(unsigned int)*(int*)opaque;
    assert(item>=1 && item<=3 && ClearDrops<3 && !xrtGetError());
    ClearOrder[ClearDrops++]=item;
    if (ClearProbe) {
        assert(!clearNow() && xrtErrorKind(xrtGetError())==XERR_STATE);
        xrtClearError();
    }
    if (ClearRelease && item==3) {
        xvalue* prior=ClearReceiver; ClearReceiver=NULL; xrtValueRelease(prior);
        assert(!xrtGetError());
    }
    if (ClearMask & (1u<<(item-1))) xrtSetErrorTake(xrtErrorRef(ClearErrors[item-1]));
}
static const xvaluehandleops ClearOps={NULL,clearDrop,NULL,NULL};
static void install(unsigned int mode, xvalue* receiver, uint64_t identity, uint64_t element)
{
    assert(!ClearReceiver && !xrtGetError() && mode<3);
    ClearReceiver=receiver; ClearIdentity=identity; ClearElement=element;
    ClearMode=mode; ClearDrops=0; ClearProbe=true; ClearRelease=false; ClearMask=7;
    for (unsigned int i=0;i<3;++i) {
        ClearErrors[i]=xrtErrorCreate(i==2 ? XERR_RANGE : XERR_STATE,"test.clear.drop",i+1,"drop failed");
        assert(ClearErrors[i]);
        void* raw=&ClearItems[i]; xvalue* item=xrtValueHandleTake(&raw,&ClearOps,NULL); assert(item);
#if defined(XL6_SEQUENCE_CLEAR_NATIVE)
        bool ok=mode==0 ? xl6StackPushV1(receiver,CLEAR_LAYOUT,&item) : xl6ArrayPushV1(receiver,CLEAR_LAYOUT,&item);
#else
        bool ok=xl6StackPushV1(receiver,CLEAR_LAYOUT,&item);
#endif
        assert(ok && !xrtGetError()); xrtValueRelease(item);
    }
}
static void finish(void)
{
    assert(ClearDrops==3 && ClearOrder[0]==3 && ClearOrder[1]==2 && ClearOrder[2]==1);
    ClearReceiver=NULL;
    for (unsigned int i=0;i<3;++i) { xrtErrorFree(ClearErrors[i]); ClearErrors[i]=NULL; }
}
/* Sidecar entry points. Borrow the exact already-constructed language Stack;
 * its public type identity is checked against Typed IR by the acceptance gate. */
void clearProbeInstallV1(xvalue* receiver)
{
    /* XLIR's public `var` identity, not xrtTypeId's distinct descriptor hash.
     * The gate recomputes this value and compares it with the current IR. */
    uint64_t element=UINT64_C(0x3594F07E9DD4DB58);
    install(0,receiver,xrtValueTypeId(receiver),element);
}
int64_t clearProbeDropsV1(void) { return ClearDrops; }
void clearProbeFinishV1(void) { finish(); }

/* Native vectors cover every facade, pending-error priority, postcommit count,
 * capacity/address stability, idempotency, busy rejection, and last-owner
 * release. Fault mode omits the deliberately allocating reentry diagnostic. */
#if defined(XL6_SEQUENCE_CLEAR_NATIVE)
void clearProbeNativeV1(void)
{
    for (unsigned int mode=0;mode<3;++mode) for (unsigned int pending=0;pending<2;++pending)
    for (unsigned int fault=0;fault<2;++fault) for (unsigned int mask=0;mask<8;++mask) {
        uint64_t identity=301+mode, element=401;
        xvalue* receiver=mode==0 ? xl6StackCreateV1(identity,element,3,sizeof(xvalue*),xrtTypeValue()) :
            xl6ArrayCreateV1(identity,element,3,sizeof(xvalue*),xrtTypeValue());
        assert(receiver); install(mode,receiver,identity,element);
        if (mode==2) {
            xvalue* alias=xl6ArrayEraseV1(receiver,CLEAR_LAYOUT,NULL); assert(alias==receiver); xrtValueRelease(alias);
        }
        XL6_Sequence* pin=xl6SequenceBeginRoleV1(receiver,mode==0 ? XL6_SEQUENCE_STACK_V1 : XL6_SEQUENCE_ARRAY_V1,CLEAR_LAYOUT);
        assert(pin); xarray before=xl6SequenceStorageV1(pin)->Storage; xl6SequenceEndV1(pin);
        xerror* prior=xrtErrorCreate(XERR_ARGUMENT,"test.clear.pending",9,"original error"); assert(prior);
        ClearProbe=!fault; ClearMask=mask;
        if (pending) xrtSetErrorTake(xrtErrorRef(prior));
        if (fault) assert(xrtMemDebugFailAfter(0));
        bool ok=clearNow();
        assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
        const xerror* expected=pending ? prior : NULL;
        for (unsigned int i=3;i && !expected;--i) if(mask & (1u<<(i-1))) expected=ClearErrors[i-1];
        assert(ok==(expected==NULL) && xrtGetError()==expected && ClearDrops==3);
        xrtClearError();
        pin=xl6SequenceBeginRoleV1(receiver,mode==0 ? XL6_SEQUENCE_STACK_V1 : XL6_SEQUENCE_ARRAY_V1,CLEAR_LAYOUT);
        assert(pin); const xarray* after=&xl6SequenceStorageV1(pin)->Storage;
        assert(!after->Count && after->Data==before.Data && after->Allocation==before.Allocation && after->Capacity==before.Capacity);
        xl6SequenceEndV1(pin); assert(clearNow() && !xrtGetError());
        finish(); xrtErrorFree(prior); xrtValueRelease(receiver); assert(!xrtGetError());
    }
    for (unsigned int mode=0;mode<3;++mode) {
        xvalue* receiver=mode==0 ? xl6StackCreateV1(501,601,3,sizeof(xvalue*),xrtTypeValue()) :
            xl6ArrayCreateV1(501,601,3,sizeof(xvalue*),xrtTypeValue());
        assert(receiver); install(mode,receiver,501,601);
        if(mode==2) { xvalue* alias=xl6ArrayEraseV1(receiver,CLEAR_LAYOUT,NULL); assert(alias); xrtValueRelease(alias); }
        ClearRelease=true; ClearProbe=false;
        assert(!clearNow() && !ClearReceiver && xrtGetError()==ClearErrors[2]);
        xrtClearError(); finish(); assert(!xrtGetError());
    }
}
#endif
