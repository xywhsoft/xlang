#include "../include/pinyin_native.h"
#include "xrt.h"
#include <stdio.h>

#ifdef __TINYC__
#include "../pinyin_native.c"
#endif

int main(void) {
    /* 编译期字节长度；调用_n入口不额外扫描输入长度。 */
#define LABEL(text) {text, sizeof(text)-1}
    static const xstrview labels[] = {LABEL("中国"), LABEL("上海"), LABEL("北京"),
        LABEL("伉"), LABEL("女"), LABEL("佛"), LABEL("龘😀")};
#undef LABEL
    unsigned matches = 0;
    for (size_t i = 0; i < sizeof(labels)/sizeof(labels[0]); ++i) {
        char *full = xpinyin_convert_n(labels[i].Data, labels[i].Size);
        if (!full || xrtGetError()) { xrtFree(full); return 1; }
        char *short_key = xpinyin_initials_n(labels[i].Data, labels[i].Size);
        if (!short_key || xrtGetError()) { xrtFree(short_key); xrtFree(full); return 1; }
        printf("%s | %s | %s\n", labels[i].Data, full, short_key);
        if (full[0] == 'b') { printf("query b: %s\n", labels[i].Data); ++matches; }
        /* 两个结果各自独立拥有；不能用CRT free释放XRT指针。 */
        xrtFree(short_key); xrtFree(full);
    }
    if (matches != 1) return 1;
    puts("pinyin search keys: PASS");
    return 0;
}
