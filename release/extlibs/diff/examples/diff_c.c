#ifdef __TINYC__
#include "../diff_native.c"
#else
#define XRT_MODULE_STRING_FORMAT
#include "xrt.h"
#include "../include/diff_native.h"
#endif
#include <stdio.h>

int main(void)
{
    const char* old_text = "func main()\n    println(hello)\n    return 0\nend";
    const char* new_text = "func main(...) as int\n    println(hello world)\n    return 0\nend";
    char* lines = (char*)xdiff_lines(old_text, new_text);
    char* stats = (char*)xdiff_stats(old_text, new_text);
    if (lines == NULL || stats == NULL) {
        xrtFree(lines);
        xrtFree(stats);
        return 1;
    }
    puts("=== Diff ===");
    fputs(lines, stdout);
    putchar('\n');
    puts("=== Stats (adds,dels,same) ===");
    puts(stats);
    puts("diff example: PASS");
    xrtFree(stats);
    xrtFree(lines);
    return 0;
}
