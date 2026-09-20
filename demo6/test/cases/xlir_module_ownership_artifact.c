#include "xlir_module_ownership.generated.h"
#include "xlir_module_ownership.generated.c"
static void (*module_graph_capture_hook)(xvalue*);
void module_graph_capture(xvalue* value) { module_graph_capture_hook(value); }
void module_graph_bind_capture(void (*hook)(xvalue*)) { module_graph_capture_hook = hook; }
const XL6_CodeOwner* module_graph_code_owner(void) { return &xlir_code_owner; }
