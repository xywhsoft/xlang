#include "public.h"
#include <assert.h>
#include <stdio.h>
typedef struct xvalue xvalue;
extern int xl_script_module_init(xvalue *);
extern int xl_script_main_checked(xvalue *,int *);
extern int xl_script_module_unit_checked(void);
extern void xl_script_print_uncaught(void);
_Static_assert(_Generic(&scalar_f32_step,float (*)(float):1,default:0),"float32 export ABI");
_Static_assert(_Generic(&scalar_c_float_step,float (*)(float):1,default:0),"c_float export ABI");
_Static_assert(_Generic(&scalar_c_double_step,double (*)(double):1,default:0),"c_double export ABI");
_Static_assert(_Generic(&scalar_f64_step,double (*)(double):1,default:0),"float export ABI");
int main(void)
{
    int code=-1;
    assert(xl_script_module_init(NULL));
    assert(scalar_f32_step(8.0f)==14.0f);
    assert(scalar_c_float_step(8.0f)==14.0f);
    assert(scalar_c_double_step(8.0)==14.0);
    assert(scalar_f64_step(8.0)==14.0);
    if(!xl_script_main_checked(NULL,&code)) {
        xl_script_print_uncaught();return 90;
    }
    assert(code==0 && xl_script_module_unit_checked());
    puts("C caller: exact scalar export ABIs and checked execution passed");
    return 0;
}
