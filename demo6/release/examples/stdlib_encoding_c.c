#define XRT_MODULE_CHARSET_DETECT
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* 检查示例步骤并输出与 xlang 示例一致的结果。 */
static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}



/* 使用当前 XRT Unicode 编码接口验证检测、转码和显式端序。 */
int main(void)
{
    const char* text = "A\xE4\xBD\xA0\xE5\xA5\xBD";
    unsigned char invalid = 255u;
    xbytesview source = {(const uint8*)text, strlen(text)};
    xbytesview invalidSource = {&invalid, 1u};
    xstrview invalidText = {(const char*)&invalid, 1u};
    xencodingguess textGuess = xrtEncodingGuess(source);
    xencodingguess invalidGuess = xrtEncodingGuess(invalidSource);
    size_t utf16Size = 0u;
    size_t restoredSize = 0u;
    size_t convertedSize = 0u;
    size_t utf16beSize = 0u;
    size_t utf16againSize = 0u;
    size_t utf32Size = 0u;
    size_t utf32beSize = 0u;
    size_t restored32Size = 0u;
    bytes utf16;
    bytes restored;
    bytes converted;
    bytes utf16be;
    bytes utf16again;
    bytes utf32;
    bytes utf32be;
    bytes restored32;

    require_line(
        "detect",
        (textGuess.Encoding == XENCODING_UTF8) &&
        !xrtUtf8Valid(invalidText, NULL) &&
        (invalidGuess.Encoding == XENCODING_UNKNOWN));

    utf16 = xrtTranscode(source, XENCODING_UTF8, XENCODING_UTF16_LE,
        XUTF_STRICT, false, &utf16Size);
    restored = xrtTranscode(
        (xbytesview){utf16, utf16Size},
        XENCODING_UTF16_LE,
        XENCODING_UTF8,
        XUTF_STRICT,
        false,
        &restoredSize
    );
    require_line(
        "utf16",
        (utf16 != NULL) &&
        (utf16Size == 6u) &&
        (restored != NULL) &&
        (restoredSize == strlen(text)) &&
        (strcmp((const char*)restored, text) == 0));

    converted = xrtTranscode(
        (xbytesview){utf16, utf16Size},
        XENCODING_UTF16_LE,
        XENCODING_UTF8,
        XUTF_STRICT,
        false,
        &convertedSize
    );
    require_line(
        "convert",
        (converted != NULL) &&
        (convertedSize == strlen(text)) &&
        (strcmp((const char*)converted, text) == 0));

    utf16be = xrtTranscode(source, XENCODING_UTF8, XENCODING_UTF16_BE,
        XUTF_STRICT, false, &utf16beSize);
    utf16again = xrtTranscode(
        (xbytesview){utf16be, utf16beSize},
        XENCODING_UTF16_BE,
        XENCODING_UTF16_LE,
        XUTF_STRICT,
        false,
        &utf16againSize
    );
    utf32 = xrtTranscode(source, XENCODING_UTF8, XENCODING_UTF32_LE,
        XUTF_STRICT, false, &utf32Size);
    utf32be = xrtTranscode(source, XENCODING_UTF8, XENCODING_UTF32_BE,
        XUTF_STRICT, false, &utf32beSize);
    restored32 = xrtTranscode(
        (xbytesview){utf32, utf32Size},
        XENCODING_UTF32_LE,
        XENCODING_UTF8,
        XUTF_STRICT,
        false,
        &restored32Size
    );
    require_line(
        "explicit",
        (utf16be != NULL) &&
        (utf16again != NULL) &&
        (utf16againSize == utf16Size) &&
        (memcmp(utf16again, utf16, utf16Size) == 0) &&
        (utf32 != NULL) &&
        (utf32be != NULL) &&
        (restored32 != NULL) &&
        (restored32Size == strlen(text)) &&
        (strcmp((const char*)restored32, text) == 0));

    xrtFree(restored32);
    xrtFree(utf32be);
    xrtFree(utf32);
    xrtFree(utf16again);
    xrtFree(utf16be);
    xrtFree(converted);
    xrtFree(restored);
    xrtFree(utf16);
    return 0;
}
