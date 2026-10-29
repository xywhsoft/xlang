#define XRT_MODULE_UNICODE
#include "xrt.h"
#include "../include/langdetect_native.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    // 编译期长度视图统一处理普通文字和内嵌NUL，不依赖样本所在的位置。
#define LANG_SAMPLE(value) {value, sizeof(value)-1u}
    static const xstrview messages[] = {LANG_SAMPLE("the and is"), LANG_SAMPLE("le est et une"),
        LANG_SAMPLE("こんにちは"), LANG_SAMPLE("你好世界"), LANG_SAMPLE("한글"), LANG_SAMPLE("Привет"),
        LANG_SAMPLE("12345"), LANG_SAMPLE("the"), LANG_SAMPLE("the\0and")};
#undef LANG_SAMPLE
    for (size_t i = 0; i < sizeof(messages)/sizeof(*messages); ++i) {
        const char* code = xlangdetect_code(messages[i].Data, messages[i].Size);
        if (xrtGetError()) { fprintf(stderr, "%s\n", xlangdetect_last_error()); return 1; }
        int64_t score = xlangdetect_confidence(messages[i].Data, messages[i].Size);
        if (xrtGetError()) { fprintf(stderr, "%s\n", xlangdetect_last_error()); return 1; }
        // code借用静态结果，不释放、不交给拥有型字符串接管。
        if (!strcmp(code, "und") || score < 60) code = "manual";
        printf("route: %s\n", code);
    }
    puts("langdetect review queue: PASS");
    return 0;
}

#ifdef __TINYC__
#include "../langdetect_native.c"
#endif
