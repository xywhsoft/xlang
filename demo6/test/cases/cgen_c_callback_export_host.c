#include "cgen_c_callback_export.h"

static int c_triple(int value)
{
    return value * 3;
}

static double c_mixed(bool enabled, double value, void* token, const char* text)
{
    return enabled && token != NULL && text != NULL && text[0] != '\0' ? value * 2.0 : -1.0;
}

static int fired_value;

static void c_remember(int value)
{
    fired_value = value;
}

int main(void)
{
    XL_InitOptions options = xlInitOptions(0, 0);
    int i;
    int result;

    if ( !xlInit(&options) ) {
        return 1;
    }
    result = xl_apply_callback(c_triple, 14) == 42 ? 0 : 2;
    if ( result == 0 ) {
        xl_set_callback(c_triple);
        result = xl_apply_saved_callback(14) == 42 ? 0 : 3;
    }
    if ( result == 0 ) {
        for ( i = 0; i < 1000; ++i ) {
            xl_set_callback(c_triple);
        }
        result = xl_apply_saved_callback(14) == 42 ? 0 : 6;
    }
    if ( result == 0 ) {
        int marker;
        result = xl_apply_mixed_callback(c_mixed, true, 21.0, &marker, "ok") == 42.0 ? 0 : 4;
    }
    if ( result == 0 ) {
        xl_fire_callback(c_remember, 42);
        result = fired_value == 42 ? 0 : 5;
    }
    xlUnit();
    return result;
}
