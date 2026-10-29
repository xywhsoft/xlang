/* Actual generated checked language entry and actual resident providers. */
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#ifndef XLIR_GENERATED_MODULE
#error An actual emitted module is required
#endif
#define main xl6GeneratedMain
#include XLIR_GENERATED_MODULE
#undef main
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);assert(!xl6ProductMemoryEnd());
    assert(xl_script_module_init(NULL));
    xvalue* arguments=xrtValueArray();assert(arguments);int code=1;
    assert(xl_script_main_checked(arguments,&code) && code==0 && !xrtGetError());
    size_t hits=0;bool complete=false;
    for (size_t point=0;point<4096;point++) {
        xl6ProductMemoryBegin(point);code=-99;
        bool ok=xl_script_main_checked(arguments,&code)!=0;bool hit=xl6ProductMemoryEnd();
        if (hit) {
            if (ok || code!=1) {fprintf(stderr,"silent language success point=%zu ok=%d code=%d\n",point,ok,code);return 1;}
            hits++;
        } else {assert(ok && code==0 && !xrtGetError());complete=true;}
        xrtClearError();xl6ProductMemoryVerify();if (complete) break;
    }
    assert(complete && hits>0);xrtValueRelease(arguments);
    assert(xl_script_module_unit_checked() && !xrtGetError());
    printf("complete actual language fault prefix: %zu failures, checked status and lifetimes balanced\n",hits);return 0;
}
