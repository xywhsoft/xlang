#define XRT_MODULE_ALL
#include "json_patch.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    char line[65536];
    size_t case_number = 0;

    /* 每行包含 [原文档, 补丁, 独立实现的预期结果]。 */
    while (fgets(line, sizeof(line), stdin) != NULL) {
        xvalue *input, *document, *patch, *expected, *actual;
        size_t length = strlen(line);
        ++case_number;
        if (length == 0 || line[length - 1] != '\n') {
            fprintf(stderr, "case %zu: input line is too long\n", case_number);
            return 1;
        }
        input = xrtJsonParse((xstrview){line, length});
        document = input ? xrtValueArrayGet(input, 0) : NULL;
        patch = input ? xrtValueArrayGet(input, 1) : NULL;
        expected = input ? xrtValueArrayGet(input, 2) : NULL;
        if (document == NULL || patch == NULL || expected == NULL) {
            fprintf(stderr, "case %zu: invalid oracle input\n", case_number);
            xrtValueRelease(input);
            return 1;
        }
        actual = xjsonpatch_apply(document, patch);
        if (actual == NULL || xjsonpatch_has_error() ||
            !xrtValueEqual(actual, expected)) {
            fprintf(stderr, "case %zu: patch differs from oracle\n", case_number);
            xrtValueRelease(actual);
            xrtValueRelease(input);
            return 1;
        }
        xrtValueRelease(actual);
        xrtValueRelease(input);
    }
    if (ferror(stdin) || case_number == 0) return 1;
    printf("json.patch oracle: %zu cases passed\n", case_number);
    return 0;
}
