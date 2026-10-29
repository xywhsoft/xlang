#include "../../build/test/xlir_export_callback_public.h"
#include <assert.h>
#include <stdio.h>

extern void callback_public_unit(void);
static int observed;
static int triple(int value) { return value * 3; }
static double mixed(bool enabled, double value, void* token, const char* text)
{
    return enabled && token != NULL && text != NULL && text[0] ? value * 2 : -1;
}
static void remember(int value) { observed = value; }

int main(void)
{
    int i, marker;
    assert(xl_apply_callback(triple, 14) == 42);
    for (i = 0; i < 1000; ++i) xl_set_callback(triple);
    assert(xl_apply_saved_callback(14) == 42);
    assert(xl_apply_mixed_callback(mixed, true, 21.0, &marker, "ok") == 42.0);
    xl_fire_callback(remember, 42);
    assert(observed == 42);
    callback_public_unit();
    callback_public_unit();
    /* The saved callback has module lifetime, not the export call lifetime. */
    assert(xl_apply_saved_callback(14) == 14);
    callback_public_unit();
    puts("public raw callback header and separate C linkage ok");
    return 0;
}
