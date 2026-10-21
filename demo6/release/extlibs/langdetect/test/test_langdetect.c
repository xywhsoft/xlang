/* langdetect C smoke: strict UTF-8, script detection and bounded Latin scoring. */
#define XRT_MODULE_UNICODE
#include "xrt.h"
#include "langdetect_native.h"

#include <stdio.h>
#include <string.h>

static int fail(cstr sMessage)
{
    fprintf(stderr, "langdetect C smoke failed: %s\n", sMessage);
    return 1;
}



int main(void)
{
    static const char Chinese[] = "\xE4\xBB\x8A\xE5\xA4\xA9\xE5\xA4\xA9\xE6\xB0\x94\xE5\xBE\x88\xE5\xA5\xBD";
    static const char BadUtf8[] = "\xE4\xB8";
    static const char Mixed[] = "The garden is nice and the sky is blue. \xE4\xB8\xAD";
    static const char Cyrillic[] = "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82";
    static const char TiedScripts[] = "\xE4\xBD\xA0\xE5\xA5\xBD" "\xED\x95\x9C\xEA\xB8\x80";
    static const char TiedJapaneseKorean[] = "\xE3\x81\x8B\xE3\x81\xAA" "\xED\x95\x9C\xEA\xB8\x80";
    static const char TiedHanCyrillic[] = "\xE4\xBD\xA0\xE5\xA5\xBD" "\xD0\x9F\xD1\x80";
    static const char TiedCyrillicArabic[] = "\xD0\x9F\xD1\x80" "\xD9\x85\xD8\xB1";
    static const char TiedHanLatin[] = "\xE4\xBD\xA0\xE5\xA5\xBD" "is";
    static const char KoreanMajority[] = "\xE4\xBD\xA0\xE5\xA5\xBD" "\xED\x95\x9C\xEA\xB8\x80\xEC\x96\xB4";
    static const char WithNul[] = {'t', 'h', 'e', 0, 'a', 'n', 'd'};
    char *tooLong;
    cstr sEnglish = "The quick brown fox is in the garden and this is a test.";

    if (strcmp(xlangdetect_detect(sEnglish, strlen(sEnglish)), "english") != 0 ||
        strcmp(xlangdetect_code(sEnglish, strlen(sEnglish)), "en") != 0 ||
        strcmp(xlangdetect_script(sEnglish, strlen(sEnglish)), "Latin") != 0 ||
        xlangdetect_confidence(sEnglish, strlen(sEnglish)) < 60) {
        return fail("English result");
    }
    if (strcmp(xlangdetect_detect(Chinese, sizeof(Chinese) - 1), "chinese") != 0 ||
        strcmp(xlangdetect_code(Chinese, sizeof(Chinese) - 1), "zh") != 0 ||
        strcmp(xlangdetect_script(Chinese, sizeof(Chinese) - 1), "Han") != 0) {
        return fail("Chinese script result");
    }
    if (!xlangdetect_valid_utf8(Chinese, sizeof(Chinese) - 1) ||
        xlangdetect_valid_utf8(BadUtf8, sizeof(BadUtf8) - 1) ||
        strcmp(xlangdetect_detect("123 !?", 6), "unknown") != 0) {
        return fail("UTF-8 and unknown result");
    }
    if (strcmp(xlangdetect_detect(Mixed, sizeof(Mixed) - 1), "english") != 0 ||
        strcmp(xlangdetect_detect(WithNul, sizeof(WithNul)), "english") != 0 ||
        strcmp(xlangdetect_detect(Cyrillic, sizeof(Cyrillic) - 1), "unknown") != 0 ||
        strcmp(xlangdetect_script(Cyrillic, sizeof(Cyrillic) - 1), "Cyrillic") != 0) {
        return fail("mixed script and explicit length");
    }
    /* 两种脚本各占一半不构成主导；三对二仍应识别。 */
    if (strcmp(xlangdetect_detect(TiedScripts, sizeof(TiedScripts) - 1), "unknown") != 0 ||
        strcmp(xlangdetect_script(TiedScripts, sizeof(TiedScripts) - 1), "Unknown") != 0 ||
        strcmp(xlangdetect_detect(TiedJapaneseKorean, sizeof(TiedJapaneseKorean) - 1), "unknown") != 0 ||
        strcmp(xlangdetect_detect(TiedHanCyrillic, sizeof(TiedHanCyrillic) - 1), "unknown") != 0 ||
        strcmp(xlangdetect_detect(TiedCyrillicArabic, sizeof(TiedCyrillicArabic) - 1), "unknown") != 0 ||
        strcmp(xlangdetect_detect(KoreanMajority, sizeof(KoreanMajority) - 1), "korean") != 0) {
        return fail("script majority and tie");
    }
    if (strcmp(xlangdetect_detect(TiedHanLatin, sizeof(TiedHanLatin) - 1), "unknown") != 0 ||
        strcmp(xlangdetect_script(TiedHanLatin, sizeof(TiedHanLatin) - 1), "Unknown") != 0) {
        return fail("Latin word cannot win a script tie");
    }
    if (strcmp(xlangdetect_detect("the\xC3\xA9", 5), "unknown") != 0 ||
        strcmp(xlangdetect_detect("\xC3\xA9the", 5), "unknown") != 0 ||
        strcmp(xlangdetect_detect("the\xF0\x9F\x98\x80", 7), "english") != 0) {
        return fail("UTF-8 word boundaries");
    }
    if (strcmp(xlangdetect_detect("the\xC3\x97", 5), "english") != 0 ||
        strcmp(xlangdetect_detect("\xC3\x97the", 5), "english") != 0 ||
        strcmp(xlangdetect_detect("the\xC3\xB7", 5), "english") != 0 ||
        strcmp(xlangdetect_script("\xC3\x97\xC3\xB7", 4), "Unknown") != 0) {
        return fail("Latin-1 mathematical symbols are not letters");
    }
    if (strcmp(xlangdetect_detect(BadUtf8, sizeof(BadUtf8) - 1), "unknown") != 0 ||
        strstr(xlangdetect_last_error(), "UTF-8") == NULL) {
        return fail("malformed UTF-8 error");
    }
    tooLong = xrtMalloc(1024u * 1024u + 1u);
    if (tooLong == NULL) return fail("allocation");
    memset(tooLong, 'a', 1024u * 1024u + 1u);
    if (strcmp(xlangdetect_detect(tooLong, 1024u * 1024u + 1u), "unknown") != 0 ||
        strstr(xlangdetect_last_error(), "1 MiB") == NULL) {
        xrtFree(tooLong);
        return fail("analysis size limit");
    }
    xrtFree(tooLong);
    if (!xlangdetect_valid_utf8(Chinese, sizeof(Chinese) - 1) ||
        xlangdetect_last_error()[0] != '\0') {
        return fail("valid UTF-8 must clear previous analysis error");
    }
    xrtSetErrorInfo(XERR_VALUE, "langdetect.test", 0, "stale error");
    if (xlangdetect_valid_utf8(BadUtf8, sizeof(BadUtf8) - 1) ||
        xlangdetect_last_error()[0] != '\0') {
        return fail("invalid UTF-8 boolean result must clear previous error");
    }
    if (xlangdetect_valid_utf8(NULL, 0) ||
        strstr(xlangdetect_last_error(), "required") == NULL) {
        return fail("null UTF-8 input must report an argument error");
    }
    puts("langdetect C smoke: PASS");
    return 0;
}
