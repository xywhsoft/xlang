#include <__xl_runtime/xruntime_decl.h>
char* native_bound_text(void* self,int mode) {
 if (!self || mode!=7) {xrtSetErrorKind(XERR_VALUE);return NULL;}
 return xrtStrDup("native bound");
}
