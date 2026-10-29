#include "../../extlibs/pinyin/include/pinyin_native.h"
#include "../../extlibs/pinyin/pinyin_native.c"
#include "xrt.h"
#include <stdio.h>

int main(void) {
    const char *samples[] = {"中国人民", "你好世界"};
    int index;
    for (index = 0; index < 2; ++index) {
        char *full = (char *)xpinyin_convert(samples[index]);
        char *short_name = (char *)xpinyin_initials(samples[index]);
        if (full == NULL || short_name == NULL) {
            xrtFree(full);
            xrtFree(short_name);
            return 1;
        }
        printf("pinyin: %s\ninitials: %s\n", full, short_name);
        xrtFree(full);
        xrtFree(short_name);
    }
    puts("pinyin example: PASS");
    return 0;
}
