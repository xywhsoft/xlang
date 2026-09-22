/* langdetect C smoke: strict UTF-8, script detection and bounded Latin scoring. */
#define XRT_MODULE_ALL
#include "xrt.h"

#include <stdio.h>
#include <string.h>

cstr xlangdetect_detect(cstr sText);
cstr xlangdetect_code(cstr sText);
cstr xlangdetect_script(cstr sText);
long long xlangdetect_confidence(cstr sText);
bool xlangdetect_valid_utf8(cstr sText);



static int fail(cstr sMessage)
{
    fprintf(stderr, "langdetect C smoke failed: %s\n", sMessage);
    return 1;
}



int main(void)
{
    static const char Chinese[] = "\xE4\xBB\x8A\xE5\xA4\xA9\xE5\xA4\xA9\xE6\xB0\x94\xE5\xBE\x88\xE5\xA5\xBD";
    static const char BadUtf8[] = "\xE4\xB8";
    cstr sEnglish = "The quick brown fox is in the garden and this is a test.";

    if (strcmp(xlangdetect_detect(sEnglish), "english") != 0 ||
        strcmp(xlangdetect_code(sEnglish), "en") != 0 ||
        strcmp(xlangdetect_script(sEnglish), "Latin") != 0 ||
        xlangdetect_confidence(sEnglish) < 60) {
        return fail("English result");
    }
    if (strcmp(xlangdetect_detect(Chinese), "chinese") != 0 ||
        strcmp(xlangdetect_code(Chinese), "zh") != 0 ||
        strcmp(xlangdetect_script(Chinese), "Han") != 0) {
        return fail("Chinese script result");
    }
    if (!xlangdetect_valid_utf8(Chinese) || xlangdetect_valid_utf8(BadUtf8) ||
        strcmp(xlangdetect_detect("123 !?"), "unknown") != 0) {
        return fail("UTF-8 and unknown result");
    }
    puts("langdetect C smoke: PASS");
    return 0;
}
