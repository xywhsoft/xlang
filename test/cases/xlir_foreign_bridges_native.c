#include <__xl_runtime/xruntime_decl.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
static int allocations, releases;
void* foreign_bridge_create(void) {
    int* raw=malloc(sizeof(*raw));assert(raw);*raw=42;++allocations;return raw;
}
void foreign_bridge_destroy(void* raw) {
    assert(raw && *(int*)raw==42);*(int*)raw=0;free(raw);++releases;
    if(releases==allocations)fprintf(stderr,"foreign resources balanced\n");
}
int64_t foreign_bridge_read(void* raw) { assert(raw);return *(int*)raw; }
int64_t foreign_bridge_released(void) { return releases; }
bool foreign_bridge_check_data(xvalue* value) {
    if(xrtValueType(value)!=XVALUE_OBJECT)return false;
    xvalue* items=xrtValueObjectGet(value,xrtStrView("items"));
    xvalue* nested=xrtValueObjectGet(value,xrtStrView("nested"));
    if(xrtValueType(items)!=XVALUE_ARRAY || xrtValueCount(items)!=2 ||
       xrtValueType(nested)!=XVALUE_OBJECT)return false;
    int64_t first=0;
    if(!xrtValueGetInt(xrtValueArrayGet(items,0),&first) || first!=1)return false;
    /* Snapshot changes must not mutate the language source. */
    return xrtValueArraySetNew(items,0,xrtValueInt(99));
}
bool foreign_bridge_check_list(xvalue* value) {
    int64_t left=0,right=0;
    return xrtValueType(value)==XVALUE_INT_MAP && xrtValueCount(value)==2 &&
        xrtValueGetInt(xrtValueIntMapGet(value,-9),&left) && left==7 &&
        xrtValueGetInt(xrtValueIntMapGet(value,4),&right) && right==11 &&
        xrtValueIntMapSetNew(value,-9,xrtValueInt(99));
}

bool foreign_bridge_debug(void) {
    if(xrtMemDebugEnabled())return true;
    return xrtMemDebugEnable(true);
}
void foreign_bridge_fail(int64_t after) { assert(xrtMemDebugFailAfter(after)); }
void foreign_bridge_clear(void) { xrtMemDebugFailClear();xrtClearError(); }
int64_t foreign_bridge_outstanding(void) { return allocations-releases; }
