#include <stdio.h>
#include <string.h>

/* bytes 由记录布局提供；strlen 只读取首个 NUL 前的文本，不保留指针。 */
static void describe(const char* name, const char* text, size_t bytes)
{
    printf("%s: bytes=%zu, c=%zu\n", name, bytes, strlen(text));
}

int main(void)
{
    const char ascii[] = "alpha", unicode[] = "你好", payload[] = "name\0payload";
    const char leading[] = "\0payload", trailing[] = "后\0", empty[] = "";
    describe("ascii", ascii, sizeof(ascii) - 1);
    describe("unicode", unicode, sizeof(unicode) - 1);
    describe("payload", payload, sizeof(payload) - 1);
    describe("leading", leading, sizeof(leading) - 1);
    describe("trailing", trailing, sizeof(trailing) - 1);
    describe("empty", empty, sizeof(empty) - 1);
    return ferror(stdout) ? 1 : 0;
}
