#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_iterator.h"
#include "src/runtime/xl6_array_methods.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#define LAYOUT 1001,2001,1,sizeof(int32_t),xrtTypeInt32()
#define CURSOR_LAYOUT 3001,2001,1,sizeof(int32_t),xrtTypeInt32()
static unsigned blocked;
static void refused(bool result) { assert(!result && xrtGetError());xrtClearError();++blocked; }
typedef struct Metadata { unsigned drops,mode; } Metadata;
static bool hold(const void* data) { return data!=NULL; }
static void dropMetadata(const void* raw)
{
 Metadata* state=(Metadata*)raw;++state->drops;
 if(state->mode==1)xrtSetErrorKind(XERR_VALUE);
 if(state->mode==2)xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
}
static bool validSlot(const void* data,const void* slot) { return data && slot; }
static bool validValue(const void* data,const xvalue* value) { return data && value; }
static bool equivalent(const void* left,const void* right) { return left==right; }
static bool defaultable(const void* data) { return data!=NULL; }
static const XL6_SequenceAdmissionOpsV2 metadataOps={hold,dropMetadata,validSlot,validValue,equivalent,defaultable,NULL,NULL};
static void lastOwner(void)
{
 for(unsigned mode=0;mode<3;++mode){
  Metadata state={0,mode};XL6_SequenceAdmissionV3 policy={.ops=&metadataOps,.data=&state,.elementIdentity=2001,.transfer=1,.size=sizeof(int32_t),.type=xrtTypeInt32(),.boxedIdentity=2001};
  xvalue* source=xl6SequenceCreateAdmittedV1(XL6_SEQUENCE_ARRAY_V1,1001,&policy);assert(source);
  XL6_SequenceReadPinV1 pin={0};assert(xl6SequenceReadPinBeginV1(source,LAYOUT,&pin));
  xrtValueRelease(source);assert(!state.drops); /* Native callback clears last language owner. */
  if(mode==2)assert(xrtMemDebugFailAfter(0));
  bool ok=xl6SequenceReadPinEndV1(&pin);assert(ok==(mode==0));assert(state.drops==1 && !pin.self && !pin.owner);
  if(mode==1){assert(xrtGetError() && xrtErrorKind(xrtGetError())==XERR_VALUE);xrtClearError();}
  if(mode==2){assert(!xrtGetError());assert(xrtMemDebugFailAfter(-1));}
 }
}
int main(void)
{
 xvalue* array=xl6ArrayCreateV1(LAYOUT);assert(array);int32_t a=7,b=9,out=0;
 assert(xl6ArrayPushV1(array,LAYOUT,&a));assert(xl6ArrayPushV1(array,LAYOUT,&b));
 xvalue* alias=xl6ArrayEraseV1(array,LAYOUT,NULL);assert(alias);
 XL6_SequenceReadPinV1 first={0},second={0};assert(xl6SequenceReadPinBeginV1(array,LAYOUT,&first));
 assert(xl6SequenceReadPinBeginV1(alias,LAYOUT,&second));
 const void* data=xrtTypedArrayConstData(first.storage);assert(data && data==xrtTypedArrayConstData(second.storage));
 assert(*(const int32_t*)xl6SequenceReadPinSlotV1(&first,0)==7);
 assert(xl6ArrayReadV1(array,LAYOUT,0,&out) && out==7);
 assert(xl6ArraySizeV1(array,LAYOUT,false)==2 && xl6ArrayErasedSizeV1(alias)==2);
 assert(xl6ArrayMethodCapacityV1(alias)>=2);
 xvalue* clone=xl6ArrayCloneV1(array,LAYOUT);assert(clone);xrtValueRelease(clone);
 clone=xl6ArrayErasedCloneV1(alias);assert(clone);xrtValueRelease(clone);
 clone=xl6ArrayIteratorCreateV1(array,LAYOUT,3001);assert(clone);
 assert(xl6ArrayIteratorAdvanceV1(clone,CURSOR_LAYOUT));assert(xl6ArrayIteratorReadV1(clone,CURSOR_LAYOUT,&out) && out==7);xrtValueRelease(clone);
 xvalue* item=xrtValueInt(17);assert(item);
 refused(xl6ArraySetV1(array,LAYOUT,0,&b));refused(xl6ArrayPushV1(array,LAYOUT,&b));
 refused(xl6ArrayInsertV1(array,LAYOUT,0,&b));refused(xl6ArrayRemoveV1(array,LAYOUT,0,1));
 refused(xl6ArrayReserveV1(array,LAYOUT,100));refused(xl6ArrayResizeV1(array,LAYOUT,0));
 refused(xl6ArrayTrimV1(array,LAYOUT));refused(xl6ArrayClearV1(array,LAYOUT));
 refused(xl6ArraySwapV1(array,LAYOUT,0,1));refused(xl6ArrayReverseV1(array,LAYOUT));
 refused(xl6ArrayAppendV1(array,array,LAYOUT));refused(xl6ArrayErasedSetV1(alias,0,item));
 refused(xl6ArrayErasedPushV1(alias,item));refused(xl6ArrayErasedInsertV1(alias,0,item));
 assert(!xl6ArrayErasedTakeV1(alias,0));refused(false);refused(xl6ArrayErasedClearV1(alias));
 refused(xl6ArrayMethodClearV1(alias));refused(xl6ArrayMethodReserveV1(alias,100));
 refused(xl6ArrayMethodTrimV1(alias));refused(xl6ArrayMethodSwapV1(alias,0,1));refused(xl6ArrayMethodReverseV1(alias));
 refused(xl6ArrayMethodRemoveValueV1(alias,item));refused(xl6ArrayMethodRemoveV1(alias,0,1));
 assert(!xl6ArrayMethodTakeV1(alias,0,false));refused(false);
 assert(xrtTypedArrayConstData(first.storage)==data && first.count==2 && ((const int32_t*)data)[0]==7 && ((const int32_t*)data)[1]==9);
 XL6_SequenceReadPinV1 copied=first;assert(!xl6SequenceReadPinSlotV1(&copied,0));refused(false);
 assert(xl6SequenceReadPinEndV1(&second));refused(xl6ArrayPushV1(alias,LAYOUT,&b));
 assert(xl6SequenceReadPinEndV1(&first));assert(xl6ArrayPushV1(alias,LAYOUT,&b));
 assert(!xl6SequenceReadPinEndV1(&first));refused(false);
 xrtValueRelease(item);xrtValueRelease(alias);xrtValueRelease(array);assert(!xrtGetError());
 lastOwner();printf("Array read-pin mutation gates: %u + last-owner cleanup\n",blocked);return 0;
}
