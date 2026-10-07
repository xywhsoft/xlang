/* Preinclude exactly one repository XRT/xruntime implementation. This probe
 * deliberately uses the public ABI, not test-local runtime replacements. */
#include <stdio.h>
#include <stdlib.h>

typedef struct KeyBoundaryState {
    unsigned int mode, drops;
    xerror* failure;
    bool dropFailure;
} KeyBoundaryState;
static void require(bool ready, const char* message)
{ if (!ready) { fprintf(stderr,"key callback boundary: %s\n",message); exit(1); } }
static uint64 keyHash(const void* key, ptr data)
{
    KeyBoundaryState* state=data; (void)key;
    if (state->mode==1) { xrtSetErrorTake(state->failure); state->failure=NULL; }
    return 0;
}
static bool keyEqual(const void* left, const void* right, ptr data)
{
    KeyBoundaryState* state=data;
    if (state->mode==2) {
        xrtSetErrorTake(state->failure); state->failure=NULL; return true;
    }
    return *(const int*)left==*(const int*)right;
}
static uint64 handleHash(ptr key,ptr data) { return keyHash(key,data); }
static bool handleEqual(ptr left,ptr right,ptr data) { return keyEqual(left,right,data); }
static void handleDrop(ptr key,ptr data)
{
    KeyBoundaryState* state=data; ++state->drops; xrtFree(key);
    if(state->dropFailure)xrtSetErrorKind(XERR_ARGUMENT);
}
static const xvaluehandleops keyOps={NULL,handleDrop,handleHash,handleEqual};
static xvalue* makeKey(int number,KeyBoundaryState* state)
{
    ptr raw=xrtMalloc(sizeof(int)); xvalue* value;
    require(raw!=NULL,"allocate key"); *(int*)raw=number;
    value=xrtValueHandleTake(&raw,&keyOps,state);
    require(value!=NULL && raw==NULL,"publish key"); return value;
}
static const xerror* arm(KeyBoundaryState* state,unsigned int mode)
{
    xrtClearError(); xrtSetErrorKind(XERR_VALUE);
    state->failure=xrtTakeError(); state->mode=mode; return state->failure;
}
static void failed(KeyBoundaryState* state,const xerror* original)
{
    require(!state->failure && xrtGetError()==original,"original error identity");
    state->mode=0; xrtClearError();
}
int main(void)
{
    KeyBoundaryState state={0}; xset native;
    int first=1,second=2,output=19; bool added=true;
    uint64 version,hash=19; const xerror* original;
    xvalue *key,*other,*set,*alias,*source,*merged,*left,*right;
    require(xrtSetInit(&native,sizeof(int)) &&
        xrtSetSetKeyPolicy(&native,keyHash,keyEqual,&state) &&
        xrtSetAdd(&native,&first),"native zero-hash insertion");
    version=native.Version;
    original=arm(&state,1);
    require(!xrtSetGetOrAdd(&native,&second,&added) && !added,"hash status/added");
    failed(&state,original);
    original=arm(&state,2);
    require(!xrtSetAdd(&native,&second),"true payload is not duplicate success");
    failed(&state,original);
    original=arm(&state,2);
    require(!xrtSetTake(&native,&first,&output) && output==19,"take output");
    failed(&state,original);
    require(native.Count==1 && native.Version==version,"failure atomicity");
    xrtSetErrorKind(XERR_STATE);original=xrtGetError();
    require(xrtSetHas(&native,&first) && !xrtSetHas(&native,&second) &&
        xrtGetError()==original,"successful present/absent with prior error");
    xrtClearError(); xrtSetUnit(&native);
    key=makeKey(1,&state);other=makeKey(2,&state);set=xrtValueSet();
    require(set && xrtValueSetAdd(set,key),"value set");
    alias=xrtValueClone(set);require(alias!=NULL,"COW alias");
    original=arm(&state,1);
    require(!xrtValueHash(key,&hash) && hash==19,"hash output unchanged");
    failed(&state,original);
    original=arm(&state,2);
    require(!xrtValueScalarEqual(key,other),"checked scalar equality");
    failed(&state,original);
    original=arm(&state,2);
    require(!xrtValueEqual(key,other),"checked graph equality");
    failed(&state,original);
    left=xrtValueArray();right=xrtValueArray();
    require(left && right && xrtValueArrayAppend(left,key) && xrtValueArrayAppend(left,key) &&
        xrtValueArrayAppend(right,other) && xrtValueArrayAppend(right,other),"nested graph fixture");
    original=arm(&state,2);
    require(!xrtValueEqual(left,right),"nested graph stops at first callback failure");
    failed(&state,original);xrtValueRelease(right);xrtValueRelease(left);
    original=arm(&state,1);source=other;
    require(!xrtValueSetAddTake(set,&source) && source==other,"source retained");
    failed(&state,original);
    original=arm(&state,2);
    require(!xrtValueSetAdd(set,other),"failed preflight does not insert");
    failed(&state,original);
    source=makeKey(3,&state);original=arm(&state,1);state.dropFailure=true;
    require(!xrtValueSetAddNew(set,source) && state.drops==1,"temporary retirement");
    failed(&state,original);state.dropFailure=false;
    source=xrtValueSet();require(source && xrtValueSetAdd(source,other),"merge source");
    original=arm(&state,1);
    require(!xrtValueSetMerge(set,source) && xrtValueCount(set)==1,"failed subset stops merge");
    failed(&state,original);
    original=arm(&state,2);merged=xrtValueSetUnion(set,source);
    require(merged==NULL,"failed algebra cannot publish result");
    failed(&state,original);xrtValueRelease(source);
    require(xrtValueCount(set)==1 && xrtValueCount(alias)==1 &&
        !xrtValueSetHas(set,other) && xrtValueSetAdd(set,other) &&
        xrtValueCount(set)==2 && xrtValueCount(alias)==1,"recovery and COW isolation");
    xrtValueRelease(alias);xrtValueRelease(set);xrtValueRelease(other);xrtValueRelease(key);
    require(state.drops==3 && !xrtGetError(),"balanced owners and errors");
    puts("XRT key callback boundary ABI ok");return 0;
}
