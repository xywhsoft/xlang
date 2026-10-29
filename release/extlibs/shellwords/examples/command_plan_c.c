/* 与 command_plan.xl 使用相同输入、四个 API 与逐字输出；不启动命令。 */
#include "../include/xrt_shellwords.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char* filename = "report 'final'.txt";
    const char* label = "你好 世界";
    str quoted_file = xrtShellwordsQuote(filename);
    str quoted_label = xrtShellwordsQuote(label);
    str windows = NULL, canonical = NULL;
    xvalue* arguments = NULL;
    xvalue* restored = NULL;
    char line[256];
    int status = 1;
    if (quoted_file == NULL || quoted_label == NULL) goto done;
    /* 固定教学输入也检查 snprintf 边界，不把截断后的命令当成成功。 */
    int length = snprintf(line, sizeof(line), "agent --output %s --label %s", quoted_file, quoted_label);
    if (length < 0 || (size_t)length >= sizeof(line)) goto done;
    arguments = xrtShellwordsSplit(line);
    if (xrtValueCount(arguments) != 5) goto done;
    canonical = xrtShellwordsJoin(arguments);
    if (canonical == NULL || strcmp(canonical, line) != 0) goto done;
    restored = xrtShellwordsSplit(canonical);
    if (xrtValueCount(restored) != 5) goto done;
    for (size_t i = 0; i != 5; ++i) {
        xstrview original, again;
        if (!xrtValueGetString(xrtValueArrayGet(arguments, i), &original) ||
            !xrtValueGetString(xrtValueArrayGet(restored, i), &again) ||
            original.Size != again.Size || memcmp(original.Data, again.Data, original.Size) != 0) goto done;
    }
    windows = xrtShellwordsQuoteWindows("C:\\saved files\\");
    if (windows == NULL || strcmp(windows, "\"C:\\saved files\\\\\"") != 0) goto done;
    printf("argc: 5\noutput: %s\nlabel: %s\ncanonical: %s\nwindows: %s\nroundtrip: true\n",
        filename, label, canonical, windows);
    status = 0;
done:
    if (status != 0) fputs("shellwords command plan failed\n", stderr);
    xrtValueRelease(restored);
    xrtValueRelease(arguments);
    xrtFree(windows);
    xrtFree(canonical);
    xrtFree(quoted_file);
    xrtFree(quoted_label);
    return status;
}

#include "../shellwords_native.c"
