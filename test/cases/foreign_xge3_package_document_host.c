#include <assert.h>
#include <stdio.h>
#include <xrt_decl.h>
extern int xl_script_module_init(xvalue*);
extern int xl_script_main_checked(xvalue*,int*);
extern int xl_script_module_unit_checked(void);
extern void xl_script_print_uncaught(void);
int main(void)
{
    assert(xrtMemDebugEnable(true));int result=-1;
    if(!xl_script_module_init(NULL) || !xl_script_main_checked(NULL,&result)){
        xl_script_print_uncaught();return 90;
    }
    if(result || !xl_script_module_unit_checked())return 91;
    puts("seven actual default Model roots: safe POD projections, callback shares, immutable reads, unique transaction shells");
    return 0;
}
