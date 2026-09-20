#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

typedef int (*CIntCallback)(int value);
typedef int (*CTextCallback)(const char* text);
typedef double (*CMixedCallback)(bool enabled, double value, void* token, const char* text);
typedef void (*CVoidCallback)(int value);

typedef struct CallbackBox {
    CIntCallback callback;
} CallbackBox;

int c_apply_i32(CIntCallback callback, int value)
{
    return callback != NULL ? callback(value) : -1;
}

int c_apply_text(CTextCallback callback, const char* text)
{
    return callback != NULL ? callback(text) : -1;
}

double c_apply_mixed(CMixedCallback callback)
{
    static int marker;
    return callback != NULL ? callback(true, 21.0, &marker, "你好") : -1.0;
}

void c_fire_i32(CVoidCallback callback, int value)
{
    if ( callback != NULL ) {
        callback(value);
    }
}

bool c_text_is_hello(const char* text)
{
    return text != NULL && strcmp(text, "你好") == 0;
}

void expect_i64(long long actual, long long expected)
{
    if ( actual != expected ) {
        exit(10);
    }
}

void expect_f64(double actual, double expected)
{
    if ( actual != expected ) {
        exit(11);
    }
}

void* callback_box_create(CIntCallback callback)
{
    CallbackBox* box = (CallbackBox*)malloc(sizeof(CallbackBox));
    if ( box != NULL ) {
        box->callback = callback;
    }
    return box;
}

long long callback_box_destroy(void* self)
{
    free(self);
    return 0;
}

void callback_box_set(void* self, CIntCallback callback)
{
    CallbackBox* box = (CallbackBox*)self;
    if ( box != NULL ) {
        box->callback = callback;
    }
}

int callback_box_apply(void* self, int value)
{
    CallbackBox* box = (CallbackBox*)self;
    return box != NULL && box->callback != NULL ? box->callback(value) : -1;
}
