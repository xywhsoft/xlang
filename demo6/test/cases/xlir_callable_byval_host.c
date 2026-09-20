#include "../../build/test/xlir_callable_byval.c"
#include "../../build/test/xlir_callable_byval.h.bind.h"
int bridge_init(void) { return xlir_module_init(); }
void bridge_fini(void) { xlir_module_fini(); }
xvalue* bridge_error_value(xerror* error) { return xlir_xvalue_error_ref(error, BRIDGE_ERROR_TYPE); }
bool bridge_forward(int kind, xvalue** arguments, size_t count, int64_t* result)
{
    xrtcallframe frame = {0}; xrtcallresult results = {0}; bool ok;
    frame.ArgumentCount = count; frame.Arguments = arguments;
    ok = xrtValueInvoke(kind == 1 ? BRIDGE_ERASED_BAD : BRIDGE_ERASED_GOOD, &frame, &results);
    if (ok) ok = xrtCallResultCount(&results) == 1 && xrtValueGetInt(xrtCallResultGet(&results, 0), result);
    xrtCallResultUnit(&results); return ok;
}
bool bridge_reverse(int kind, char* text, xvalue* value, xerror* issue, int64_t* result)
{
    xlir_closure* closure = kind == 3 || kind == 9 ? BRIDGE_BAD : BRIDGE_GOOD;
    if (kind == 4 || kind == 10) return BRIDGE_ENTRY(NULL, result, text, value, issue);
    if (kind == 5 || kind == 11) return BRIDGE_ENTRY(closure->environment, NULL, text, value, issue);
    return ((bool (*)(void*, int64_t*, char*, xvalue*, xerror*))closure->invoke)(
        closure->environment, result, text, value, issue);
}
