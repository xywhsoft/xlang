#include <__xl_runtime/xrt_decl.h>
#include <assert.h>
#include <stdio.h>
extern int xl_script_module_init(xvalue*);
extern int xl_script_main_checked(xvalue*,int*);
extern int xl_script_module_unit_checked(void);
extern void xl_script_print_uncaught(void);
int main(void)
{
    assert(xrtMemDebugEnable(true));int code=-1;
    if(!xl_script_module_init(NULL) || !xl_script_main_checked(NULL,&code)){
        xl_script_print_uncaught();return 90;
    }
    if(!xl_script_module_unit_checked())return 91;
    return code;
}
