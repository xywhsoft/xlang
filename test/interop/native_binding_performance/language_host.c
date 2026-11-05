#include "binding_perf.h"
#include <assert.h>
extern int xl_script_module_init(xvalue*);
extern int xl_script_main_checked(xvalue*,int*);
extern int xl_script_module_unit_checked(void);
extern void xl_script_print_uncaught(void);
int main(void)
{
 int result=-1;
 binding_perf_counter_begin();
 if(!xl_script_module_init(NULL)){
  xl_script_print_uncaught();return 90;
 }
 binding_perf_counter_end(-1);
 if(!xl_script_main_checked(NULL,&result)){xl_script_print_uncaught();return 90;}
 assert(result==0 && xl_script_module_unit_checked());
 assert(binding_perf_verify());binding_perf_counter_end(5);return 0;
}
