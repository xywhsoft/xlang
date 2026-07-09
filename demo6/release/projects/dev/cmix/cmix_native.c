/* cmix_native.c - C companion for the cmix C-mixing verification tool.
 *
 * Defines C functions, global variables and an opaque counter handle that the
 * xlang side (cmix.xl) reaches through `extern`, `export extern`, and a
 * native-backed class. Also calls back into xlang-exported functions to
 * verify the reverse direction (C -> xlang).
 *
 * IMPORTANT type mapping (verified via `xl cgen --emit-header`):
 *   xlang `int`    -> C int64_t / long long   (64-bit)
 *   xlang `int32`  -> C int                    (32-bit, native)
 *   xlang `float`  -> C double                 (64-bit, xlang float is f64)
 *   xlang `point`  -> C void*
 * So functions reached through `extern ... as int` use 64-bit params/returns,
 * and functions reached through `... as int32` use native 32-bit int.
 *
 * Build / run together with cmix.xl:
 *   xl run   cmix.xl cmix_native.c
 *   xl build cmix.xl cmix_native.c -o cmix.exe
 */

#include <stdlib.h>

/* ---- Global variables --------------------------------------------------
 * cmix_counter is read/written from xlang via `export extern` (storage owned
 *              here; the xlang symbol is an alias onto this C global).
 * cmix_magic   is read from xlang via plain `extern`.
 * ----------------------------------------------------------------------- */
long long cmix_counter = 100;
const long long cmix_magic = 7;

/* ---- Simple math: xlang `int` (int64_t) path --------------------------- */
long long cmix_add(long long a, long long b)
{
    return a + b;
}

long long cmix_mul(long long a, long long b)
{
    return a * b;
}

long long cmix_max(long long a, long long b)
{
    return a > b ? a : b;
}

double cmix_average(long long a, long long b)
{
    return ((double)a + (double)b) / 2.0;
}

long long cmix_square_ll(long long x)
{
    return x * x;
}

/* ---- Native-int path: xlang `int32` / `c_int` (32-bit) ----------------- */
/* These prove that C ABI types map to native C int, not int64_t. */
int cmix_native_add(int a, int b)
{
    return a + b;
}

int cmix_native_mul(int a, int b)
{
    return a * b;
}

/* ---- Opaque counter handle (native-backed class) -----------------------
 * All int-valued params/returns use long long to match xlang `int`.
 * ----------------------------------------------------------------------- */
typedef struct cmix_counter_s {
    long long value;
    long long ops;
} cmix_counter_s;

void* cmix_counter_open(long long seed)
{
    cmix_counter_s* c = (cmix_counter_s*)malloc(sizeof(cmix_counter_s));
    if (c != NULL) {
        c->value = seed;
        c->ops = 0;
    }
    return (void*)c;
}

void cmix_counter_close(void* handle)
{
    if (handle != NULL) {
        free(handle);
    }
}

long long cmix_counter_add(void* handle, long long value)
{
    cmix_counter_s* c = (cmix_counter_s*)handle;
    if (c == NULL) {
        return 0;
    }
    c->value += value;
    c->ops += 1;
    return c->value;
}

long long cmix_counter_value(void* handle)
{
    cmix_counter_s* c = (cmix_counter_s*)handle;
    return (c != NULL) ? c->value : 0;
}

long long cmix_counter_ops(void* handle)
{
    cmix_counter_s* c = (cmix_counter_s*)handle;
    return (c != NULL) ? c->ops : 0;
}

void cmix_counter_reset(void* handle, long long seed)
{
    cmix_counter_s* c = (cmix_counter_s*)handle;
    if (c != NULL) {
        c->value = seed;
        c->ops = 0;
    }
}

/* ---- C calling back into xlang -----------------------------------------
 * cmix_xlang_double / cmix_xlang_triple are xlang fns exported with
 * `export "cmix_xlang_double"` etc. The xl csrc wrapper includes the
 * generated module.h before this file, which declares them as
 * `int64_t cmix_xlang_double(int64_t x)`. We provide matching `long long`
 * forward declarations here (long long == int64_t on this platform, so this
 * is a compatible redeclaration, not a conflict). The symbols resolve at link
 * time against the generated module.c.
 * ----------------------------------------------------------------------- */
extern long long cmix_xlang_double(long long x);
extern long long cmix_xlang_triple(long long x);

long long cmix_callback_demo(long long x)
{
    /* call into xlang, then add 1 in C to prove the round trip */
    return cmix_xlang_double(x) + 1;
}

long long cmix_callback_chain(long long x)
{
    long long once = cmix_xlang_double(x);   /* x*2      */
    long long twice = cmix_xlang_triple(once); /* once*3 */
    return once + twice;
}
