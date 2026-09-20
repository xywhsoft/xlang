#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct xvalue xvalue;

xvalue* xrtValueInt(long long value);
bool xrtValueGetInt(const xvalue* value, long long* result);
size_t xrtValueCount(const xvalue* value);

static long long g_nc_probe_count = 0;
static long long g_recorded_xvalue = 0;
static unsigned long long g_expect_i64_count = 0;

void expect_i64(long long actual, long long expected)
{
    ++g_expect_i64_count;
    if ( actual != expected ) {
        fprintf(
            stderr,
            "expect_i64 failed: call=%llu actual=%lld expected=%lld\n",
            g_expect_i64_count,
            actual,
            expected
        );
        exit(1);
    }
}

void expect_str(const char* actual, const char* expected)
{
    if ( actual == NULL || expected == NULL || strcmp(actual, expected) != 0 ) {
        fprintf(stderr, "expect_str failed: actual=%s expected=%s\n", actual != NULL ? actual : "(null)", expected != NULL ? expected : "(null)");
        exit(1);
    }
}

void remove_file(const char* path)
{
    if ( path != NULL ) {
        (void)remove(path);
    }
}

void expect_file_contains(
    const char* path,
    const char* first,
    const char* second)
{
    FILE* file;
    char* text;
    long size;

    if ( path == NULL || first == NULL || second == NULL ) {
        fprintf(stderr, "expect_file_contains received null argument\n");
        exit(1);
    }
    file = fopen(path, "rb");
    if ( file == NULL || fseek(file, 0, SEEK_END) != 0 ) {
        fprintf(stderr, "expect_file_contains could not open %s\n", path);
        if ( file != NULL ) { fclose(file); }
        exit(1);
    }
    size = ftell(file);
    if ( size < 0 || fseek(file, 0, SEEK_SET) != 0 ) {
        fprintf(stderr, "expect_file_contains could not size %s\n", path);
        fclose(file);
        exit(1);
    }
    text = (char*)malloc((size_t)size + 1u);
    if ( text == NULL || fread(text, 1u, (size_t)size, file) != (size_t)size ) {
        fprintf(stderr, "expect_file_contains could not read %s\n", path);
        free(text);
        fclose(file);
        exit(1);
    }
    text[size] = 0;
    fclose(file);
    if ( strstr(text, first) == NULL || strstr(text, second) == NULL ) {
        fprintf(stderr, "expect_file_contains mismatch in %s\n", path);
        free(text);
        exit(1);
    }
    free(text);
}

bool file_exists(const char* path)
{
    FILE* file;

    if ( path == NULL ) {
        return false;
    }
    file = fopen(path, "rb");
    if ( file == NULL ) {
        return false;
    }
    fclose(file);
    return true;
}

void nc_probe_reset(void)
{
    g_nc_probe_count = 0;
}

long long nc_probe_next(void)
{
    g_nc_probe_count += 1;
    return 33;
}

xvalue* nc_probe_next_var(void)
{
    g_nc_probe_count += 1;
    return xrtValueInt(33);
}

long long nc_probe_count(void)
{
    return g_nc_probe_count;
}

void record_xvalue(xvalue* value)
{
    (void)xrtValueGetInt(value, &g_recorded_xvalue);
}

long long recorded_xvalue(void)
{
    return g_recorded_xvalue;
}

long long xvalue_vararg_count(const char* tag, xvalue* arg)
{
    (void)tag;
    return (long long)xrtValueCount(arg);
}

long long xvalue_kw_count(const char* tag, xvalue* arg)
{
    (void)tag;
    return (long long)xrtValueCount(arg);
}

void* native_handle(long long value)
{
    static long long slots[8];
    int index = (int)(value & 7);
    slots[index] = value;
    return &slots[index];
}
