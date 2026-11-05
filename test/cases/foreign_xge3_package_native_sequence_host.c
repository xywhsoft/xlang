#include "test/cases/foreign_xge3_package_native_sequence.h"
#include <assert.h>
#include <stdio.h>
extern int xl_script_module_init(xvalue*);
extern int xl_script_main_checked(xvalue*,int*);
extern int xl_script_module_unit_checked(void);
extern void xl_script_print_uncaught(void);
int main(void)
{
 assert(xrtMemDebugEnable(true));int result=-1;
 if(!xl_script_module_init(NULL) || !xl_script_main_checked(NULL,&result)) { xl_script_print_uncaught();return 90; }
 assert(!result && xl_script_module_unit_checked() && sequence_cpu_verify());
 puts("real owned Shape sequences: source close, copied POD, slices/closure/var, allocation-free typed loops, exact native cleanup");return 0;
}
