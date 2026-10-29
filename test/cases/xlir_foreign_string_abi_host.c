#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char xext_text_buffer[64];
static int64_t xext_text_host_count;

const char* xext_text_static(int64_t version)
{
    const char* text = version == 1 ? "first" : "second";
    memcpy(xext_text_buffer, text, strlen(text) + 1);
    return xext_text_buffer;
}

const char* xext_text_null(void)
{
    return NULL;
}

const char* xext_text_alias(const char* text)
{
    return text;
}

const char* xext_text_join(const char* left, const char* right)
{
    static char joined[128];
    size_t left_size = strlen(left);
    size_t right_size = strlen(right);
    if ( left_size + right_size + 2 > sizeof(joined) ) return NULL;
    memcpy(joined, left, left_size);
    joined[left_size] = ':';
    memcpy(joined + left_size + 1, right, right_size + 1);
    return joined;
}

void* xext_text_create(void)
{
    char* text = (char*)malloc(16);
    if ( text != NULL ) {
        memcpy(text, "native", sizeof("native"));
        ++xext_text_host_count;
    }
    return text;
}

void xext_text_destroy(void* handle)
{
    if ( handle == NULL ) return;
    /* 销毁前覆盖内容，暴露从临时接收者借出字符串后才复制的错误顺序。 */
    memset(handle, '!', 15);
    free(handle);
    --xext_text_host_count;
}

const char* xext_text_get(void* handle)
{
    return (const char*)handle;
}

const char* xext_text_read(void* handle)
{
    return (const char*)handle;
}

int64_t xext_text_alive(void)
{
    return xext_text_host_count;
}

#ifdef XLIR_GENERATED_MODULE_PREINCLUDED
int main(void)
{
    xvalue* arguments = xrtValueArray();
    int result;
    if ( arguments == NULL ) return 2;
    result = xlang_main(arguments);
    xrtValueRelease(arguments);
    if ( xl_script_has_error() ) {
        xl_script_print_uncaught();
        result = 1;
    }
    xl_script_module_unit();
    xl_script_error_unit();
    return result;
}
#endif
