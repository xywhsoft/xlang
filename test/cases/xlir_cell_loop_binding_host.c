#include "xlir_cell_loop_binding.c"
#include "xlir_cell_loop_binding.h.bind.h"
int loop_binding_init(void) { return xlir_module_init(); }
void loop_binding_finish(void) { xlir_module_fini(); }
int loop_binding_run(int shape, long long* value)
{
    xlir_result_i64 result = {0};
    switch (shape) {
    case 0: result = LOOP_BINDING_0(); break;
    case 1: result = LOOP_BINDING_1(); break;
    case 2: result = LOOP_BINDING_2(); break;
    case 3: result = LOOP_BINDING_3(); break;
    case 4: result = LOOP_BINDING_4(); break;
    case 5: result = LOOP_BINDING_5(); break;
    case 6: result = LOOP_BINDING_6(); break;
    case 7: result.value = LOOP_BINDING_7(); result.ok = true; break;
    case 8: result = LOOP_BINDING_8(); break;
    case 9: result = LOOP_BINDING_9(); break;
    case 10: result = LOOP_BINDING_10(); break;
    }
    *value = (long long)result.value; return result.ok;
}
