#if !defined(XLIR_GENERATED_MODULE_PREINCLUDED)
    #ifndef XLIR_GENERATED_MODULE
        #error "XLIR_GENERATED_MODULE must name the generated C translation unit"
    #endif
    #include XLIR_GENERATED_MODULE
#endif

#include <string.h>

bool xl6_test_point_is_null(void* value)
{
    return value == NULL;
}

static xvalue* xl6_test_invoke(
    XL6_Function callback, int argc, xvalue** arguments)
{
    xvalue* result;
    if ( callback.invoke_var == NULL || callback.env == NULL ) return NULL;
    if ( callback.retain != NULL ) callback.retain(callback.env);
    result = callback.invoke_var(callback.env, argc, arguments);
    if ( callback.release != NULL ) callback.release(callback.env);
    return result;
}

bool xl6_test_invoke_void(XL6_Function callback)
{
    xvalue* result = xl6_test_invoke(callback, 0, NULL);
    bool ok = result != NULL && xrtValueType(result) == 0;
    xrtValueRelease(result);
    return ok;
}

bool xl6_test_invoke_scalars(XL6_Function callback)
{
    xvalue* arguments[5];
    xvalue* result;
    double value = 0.0;
    bool ok;
    arguments[0] = xrtValueRetain(xrtValueBool(true));
    arguments[1] = xrtValueInt(-7);
    arguments[2] = xrtValueUInt(UINT64_C(9));
    arguments[3] = xrtValueUInt(UINT64_C(1));
    arguments[4] = xrtValueFloat(4.5);
    if ( arguments[0] == NULL || arguments[1] == NULL ||
         arguments[2] == NULL || arguments[3] == NULL ||
         arguments[4] == NULL ) {
        int i;
        for ( i = 0; i < 5; ++i ) xrtValueRelease(arguments[i]);
        return false;
    }
    result = xl6_test_invoke(callback, 5, arguments);
    for ( int i = 0; i < 5; ++i ) xrtValueRelease(arguments[i]);
    ok = result != NULL && xrtValueGetFloat(result, &value) && value == 4.5;
    xrtValueRelease(result);
    return ok;
}

bool xl6_test_invoke_string(XL6_Function callback)
{
    xvalue* arguments[2];
    xvalue* result;
    xlir_string_view text = {0};
    bool ok;
    arguments[0] = xrtValueRetain(xrtValueNull());
    arguments[1] = xrtValueString((xstrview){"ma\0trix", 7});
    if ( arguments[0] == NULL || arguments[1] == NULL ) {
        xrtValueRelease(arguments[0]); xrtValueRelease(arguments[1]);
        return false;
    }
    result = xl6_test_invoke(callback, 2, arguments);
    xrtValueRelease(arguments[0]); xrtValueRelease(arguments[1]);
    ok = result != NULL && xrtValueGetString(result, &text) &&
         text.Size == 7 && memcmp(text.Data, "ma\0trix", 7) == 0;
    xrtValueRelease(result);
    return ok;
}

bool xl6_test_invoke_value(XL6_Function callback)
{
    xvalue* argument = xrtValueInt(17);
    xvalue* result;
    int64_t value = 0;
    bool ok;
    if ( argument == NULL ) return false;
    result = xl6_test_invoke(callback, 1, &argument);
    xrtValueRelease(argument);
    ok = result != NULL && xrtValueGetInt(result, &value) && value == 17;
    xrtValueRelease(result);
    return ok;
}

bool xl6_test_invoke_predicate(void* context, XL6_Function callback)
{
    xvalue* argument = xrtValueInt(17);
    xvalue* result;
    bool value = false;
    bool ok;
    if ( context != NULL || argument == NULL ) {
        xrtValueRelease(argument);
        return false;
    }
    result = xl6_test_invoke(callback, 1, &argument);
    xrtValueRelease(argument);
    ok = result != NULL && xrtValueGetBool(result, &value) && value;
    xrtValueRelease(result);
    return ok;
}

bool xl6_test_invoke_captured(XL6_Function callback)
{
    xvalue* argument = xrtValueInt(11);
    xvalue* result;
    bool value = false;
    bool ok;
    if ( argument == NULL ) return false;
    result = xl6_test_invoke(callback, 1, &argument);
    xrtValueRelease(argument);
    ok = result != NULL && xrtValueGetBool(result, &value) && value;
    xrtValueRelease(result);
    return ok;
}

int main(void)
{
    return xlir_module_init() ? 0 : 1;
}
