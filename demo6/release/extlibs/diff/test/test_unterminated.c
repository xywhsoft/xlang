#include "../include/diff_native.h"
#include "xrt.h"
/* 边界调用写在 assert 中；发布配置仍须执行。 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define TEST_LIMIT (8u * 1024u * 1024u)

int main(void)
{
    char* text = (char*)malloc(TEST_LIMIT + 1u);
    void* result;
    assert(text != NULL);
    memset(text, 'a', TEST_LIMIT + 1u);

    text[TEST_LIMIT] = '\0';
    result = xdiff_stats("", text);
    assert(result != NULL);
    assert(strcmp((const char*)result, "1,0,0") == 0);
    xrtFree(result);
    text[TEST_LIMIT] = 'a';

    /* 超过输入预算时不能继续扫描到缓冲区之外。 */
    assert(xdiff_lines(text, "") == NULL);
    assert(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    assert(xdiff_stats("", text) == NULL);
    assert(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);

    result = xdiff_stats("a", "a");
    assert(result != NULL);
    assert(strcmp((const char*)result, "0,0,1") == 0);
    assert(xrtGetError() == NULL);
    xrtFree(result);
    free(text);
    return 0;
}
