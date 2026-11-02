/* 与 review.xl 相同：输入退役后仍拥有两份完整结果。 */
#define XRT_MODULE_STRING
#include "xrt.h"
#include "../include/diff_native.h"
#ifdef __TINYC__
#include "../diff_native.c"
#endif
#include <stdio.h>
typedef struct review_result {char* patch;char* stats;} review_result;
static review_result review(void)
{
    review_result result={0};
    char* old=xrtStrDup("name=旧\r\nport=8080\r\ncache=on\r\ntail=same");
    char* updated=xrtStrDup("name=新\nport=8080\ncache=on\ntail=same\n");
    if(old && updated){result.patch=xdiff_lines(old,updated);if(result.patch)result.stats=xdiff_stats(old,updated);}
    xrtFree(old);xrtFree(updated);return result;
}
int main(void)
{
    review_result result=review();char* repeated=NULL;int code=1;
    if(!result.patch || !result.stats)goto done;
    repeated=xdiff_lines("a\na","a");if(!repeated)goto done;
    puts("configuration review");fputs(result.patch,stdout);
    printf("adds,deletes,same: %s\n",result.stats);puts("repeated lines");fputs(repeated,stdout);code=0;
done:
    xrtFree(repeated);xrtFree(result.stats);xrtFree(result.patch);return code;
}
