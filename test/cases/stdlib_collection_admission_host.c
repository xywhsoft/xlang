#include "include/xruntime_decl.h"
#include <assert.h>
#include <stdio.h>
int xl_script_main_checked(xvalue*, int*);
int xl_script_module_unit_checked(void);
int main(void)
{
    xvalue* arguments=xrtValueArray(); assert(arguments);
    int code=97; int ok=xl_script_main_checked(arguments,&code);
    if (!ok || code || xrtGetError()) fprintf(stderr,"collection admission: ok=%d code=%d error=%s\n",
        ok,code,xrtErrorMessage(xrtGetError()));
    assert(ok && !code && !xrtGetError()); xrtValueRelease(arguments);
    assert(xl_script_module_unit_checked() && !xrtGetError());
    puts("collection admission language passed"); return 0;
}
