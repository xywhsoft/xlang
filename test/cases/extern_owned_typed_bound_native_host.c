#include <__xl_runtime/xruntime_decl.h>
#include <stdint.h>
char* owned_text(void);
bool owned_original(const char*);
static int64_t handles;
void* typed_bound_open(void) {
    uint64_t* state=xrtMalloc(sizeof(*state));
    if(state){*state=1234567;++handles;}
    return state;
}
void typed_bound_close(void* object) {
    if(object){--handles;xrtFree(object);}
}
int64_t typed_bound_handles(void){return handles;}
char* typed_bound_text(void* object,int mode) {
    if(!object || *(uint64_t*)object!=1234567 || mode!=7){xrtSetErrorKind(XERR_VALUE);return NULL;}
    return owned_text();
}
char* typed_bound_roundtrip(void* object,const char* input,int mode) {
    /* Compare before producing the next owned string; input must be borrowed. */
    if(!owned_original(input)){xrtSetErrorKind(XERR_TYPE);return NULL;}
    return typed_bound_text(object,mode);
}
