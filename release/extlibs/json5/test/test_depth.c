#define XRT_MODULE_ALL
#include "xrt.h"
#include "../json5_native.h"
#include <stdio.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "JSON5 depth failed at %d: mode=%u wraps=%u leaf=%s: %s\n", \
        __LINE__, mode, wraps, leaves[leaf].source, #test); return 1; \
} } while (0)

int main(void)
{
    static const struct { const char *source; bool container; } leaves[] = {
        {"null", false}, {"true", false}, {"42", false}, {"1.25", false},
        {"'x'", false}, {"[]", true}, {"{}", true}
    };
    static const unsigned counts[] = {0, 1, 63, 64, 65};
    unsigned mode, wraps, leaf, count, i;

    /* 容器本身计一层，标量不多占层；空容器与有内容的容器采用相同预算。 */
    for (mode = 0; mode < 4; ++mode) {
        for (count = 0; count < sizeof(counts) / sizeof(counts[0]); ++count) {
            wraps = counts[count];
            for (leaf = 0; leaf < sizeof(leaves) / sizeof(leaves[0]); ++leaf) {
                char source[512], close[65];
                size_t size = 0;
                unsigned depth = wraps + (leaves[leaf].container ? 1u : 0u);
                bool valid = depth <= 64;
                xvalue *value;
                char *text;
                for (i = 0; i < wraps; ++i) {
                    bool object = mode == 1 || (mode >= 2 && ((i + mode) & 1));
                    if (object) { memcpy(source + size, "{a:", 3); size += 3; }
                    else source[size++] = '[';
                    close[i] = object ? '}' : ']';
                }
                memcpy(source + size, leaves[leaf].source, strlen(leaves[leaf].source));
                size += strlen(leaves[leaf].source);
                for (i = wraps; i > 0; --i) source[size++] = close[i - 1];
                source[size] = 0;
                value = xjson5_parse_len(source, size);
                CHECK(value != NULL);
                CHECK(valid ? xrtGetError() == NULL : xrtGetError() &&
                    xrtErrorKind(xrtGetError()) == XERR_RANGE);
                xrtValueRelease(value);
                CHECK(xjson5_valid_len(source, size) == valid && !xrtGetError());
                value = xjson5_get_value_len(source, size, "");
                CHECK(value != NULL);
                CHECK(valid ? xrtGetError() == NULL : xrtGetError() &&
                    xrtErrorKind(xrtGetError()) == XERR_RANGE);
                xrtValueRelease(value);
                text = xjson5_get_text_len(source, size, "");
                CHECK(!valid ? !text && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_RANGE :
                    depth ? !text && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE :
                    text && !xrtGetError());
                xrtFree(text);
            }
        }
    }
    xrtClearError();
    puts("json5 container depth: PASS");
    return 0;
}
