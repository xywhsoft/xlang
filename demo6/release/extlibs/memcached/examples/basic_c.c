/* C counterpart to basic.xl; a distinct basename avoids implicit co-build. */
#define XRT_MODULE_VALUE_CONTAINER
#include "../include/memcached_native.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const uint8 response[] = "VALUE key 0 3\r\na\0b\r\nEND\r\n";
    char *command = xmc_build_set((xstrview){"key", 3}, 0, 0, 3);
    xvalue *result = xmc_parse_response_span(
        (xbytesview){response, sizeof(response) - 1});
    xvalue *items = result != NULL ?
        xrtValueObjectGet(result, XRT_STR_LITERAL("items")) : NULL;
    xvalue *item = items != NULL ? xrtValueArrayAt(items, 0) : NULL;
    xvalue *data = item != NULL ?
        xrtValueObjectGet(item, XRT_STR_LITERAL("data")) : NULL;
    xstrview value;
    if (command == NULL || data == NULL || !xrtValueGetString(data, &value)) {
        xrtFree(command);
        xrtValueRelease(result);
        return 1;
    }
    printf("set command: %.*s\n", (int)strlen(command) - 2, command);
    printf("value bytes: %zu\n", value.Size);
    printf("value middle byte: %u\n", (unsigned)(uint8)value.Data[1]);
    printf("missing status: end\n");
    xrtFree(command);
    xrtValueRelease(result);
    return 0;
}
