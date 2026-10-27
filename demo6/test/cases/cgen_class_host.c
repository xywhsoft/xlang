#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

typedef struct xvalue xvalue;

xvalue* xrtValueInt(int64_t value);
bool xrtValueGetInt(const xvalue* value, int64_t* result);
size_t xrtValueCount(const xvalue* value);

static int64_t g_nc_probe_count = 0;
static int64_t g_recorded_xvalue = 0;
static uint64_t g_expect_i64_count = 0;

void expect_i64(int64_t actual, int64_t expected)
{
    ++g_expect_i64_count;
    if ( actual != expected ) {
        fprintf(
            stderr,
            "expect_i64 failed: call=%" PRIu64 " actual=%" PRId64 " expected=%" PRId64 "\n",
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

int64_t nc_probe_next(void)
{
    g_nc_probe_count += 1;
    return 33;
}

xvalue* nc_probe_next_var(void)
{
    g_nc_probe_count += 1;
    return xrtValueInt(33);
}

int64_t nc_probe_count(void)
{
    return g_nc_probe_count;
}

void record_xvalue(xvalue* value)
{
    (void)xrtValueGetInt(value, &g_recorded_xvalue);
}

int64_t recorded_xvalue(void)
{
    return g_recorded_xvalue;
}

int64_t xvalue_vararg_count(const char* tag, xvalue* arg)
{
    (void)tag;
    return (int64_t)xrtValueCount(arg);
}

int64_t xvalue_kw_count(const char* tag, xvalue* arg)
{
    (void)tag;
    return (int64_t)xrtValueCount(arg);
}

void* native_handle(int64_t value)
{
    static int64_t slots[8];
    int index = (int)(value & 7);
    slots[index] = value;
    return &slots[index];
}
