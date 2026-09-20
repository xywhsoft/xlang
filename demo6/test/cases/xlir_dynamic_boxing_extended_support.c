#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct xvalue xvalue;
typedef struct xerror xerror;
typedef struct xvaluehandleops xvaluehandleops;

extern bool xrtValueGetHandle(
    const xvalue* value, void** handle,
    const xvaluehandleops** operations, void** user);
extern xvalue* xrtValueClone(const xvalue* value);
extern void xrtValueRelease(xvalue* value);
extern const char* xrtErrorMessage(const xerror* error);

void expect_i64(int64_t actual, int64_t expected)
{
    if ( actual != expected ) {
        fprintf(stderr, "dynamic boxing mismatch: %lld != %lld\n",
                (long long)actual, (long long)expected);
        exit(1);
    }
}

static void inspect_error_payload(const xvalue* value)
{
    void* handle = NULL;
    const xvaluehandleops* operations = NULL;
    void* user = NULL;
    const char* message;
    bool hasHandle = value != NULL &&
        xrtValueGetHandle(value, &handle, &operations, &user);
    if ( value == NULL || !hasHandle ||
         handle == NULL || operations == NULL ) {
        fprintf(stderr,
                "dynamic error value has no retained handle ABI: handle=%d payload=%p operations=%p\n",
                hasHandle ? 1 : 0,
                handle, (const void*)operations);
        exit(1);
    }
    message = xrtErrorMessage((const xerror*)handle);
    if ( message == NULL || strcmp(message, "invalid integer string") != 0 ) {
        fprintf(stderr, "dynamic error handle lost its payload: %s\n",
                message != NULL ? message : "(null)");
        exit(1);
    }
}

void inspect_error_value(xvalue* value)
{
    xvalue* clone;
    inspect_error_payload(value);
    clone = xrtValueClone(value);
    if ( clone == NULL ) {
        fprintf(stderr, "dynamic error handle cannot be cloned\n");
        exit(1);
    }
    inspect_error_payload(clone);
    xrtValueRelease(clone);
}
