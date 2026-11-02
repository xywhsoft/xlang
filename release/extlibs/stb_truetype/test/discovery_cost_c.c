/* 独立的逻辑分配计数，不把它当作延迟/吞吐基准。 */
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_ENVIRONMENT
#define XRT_MODULE_PATH
#define XRT_MODULE_STRING
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/stb_truetype_native.h"
#ifdef __TINYC__
#include "../stb_truetype_native.c"
#endif
#include <stdio.h>
int main(void)
{
    if(!xrtMemDebugEnabled())return 2;
    xrtSetErrorInfo(XERR_STATE,"test.warm",0,"warm");xrtClearError();
    for(unsigned round=0;round<4;++round){
        xmemdebugsnapshot before,after,retired;xrtClearError();xrtMemDebugSnapshot(&before);
        char* result=xstbtt_default_font_path();xrtMemDebugSnapshot(&after);
        if(!result || xrtGetError())return 1;
        bool found=result[0]!=0;xrtFree(result);xrtMemDebugSnapshot(&retired);
        if(before.LiveCount!=retired.LiveCount || before.LiveBytes!=retired.LiveBytes ||
            before.InvalidFreeCount!=retired.InvalidFreeCount || before.DoubleFreeCount!=retired.DoubleFreeCount)return 1;
        printf("discovery cost: found=%d alloc=%llu realloc=%llu\n",found,
            (unsigned long long)(after.AllocCount-before.AllocCount),
            (unsigned long long)(after.ReallocCount-before.ReallocCount));
    }
    return 0;
}
