#ifdef __TINYC__
#include "../langdetect_native.c"
#else
#include "../include/langdetect_native.h"
#endif
#include "xrt.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *english = "the cat is on the table and it is not bad";
    const char *french = " le chat est dans la maison avec une fille ";
    const char *german = " der Hund und die Katze sind nicht das Haus ";
    const char chinese[] = "\xE4\xBB\x8A\xE5\xA4\xA9\xE5\xA4\xA9\xE6\xB0\x94\xE5\xBE\x88\xE5\xA5\xBD";
    const char supplementaryHan[] = "\xF0\xA0\x80\x80\xF0\xA0\x80\x80"; /* U+20000 x 2 */
    const char japanese[] = "\xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF";
    if (strcmp(xlangdetect_detect(english, strlen(english)), "english") != 0 ||
        strcmp(xlangdetect_code(english, strlen(english)), "en") != 0 ||
        strcmp(xlangdetect_script(english, strlen(english)), "Latin") != 0 ||
        strcmp(xlangdetect_detect(french, strlen(french)), "french") != 0 ||
        strcmp(xlangdetect_detect(german, strlen(german)), "german") != 0 ||
        strcmp(xlangdetect_detect(chinese, sizeof(chinese) - 1), "chinese") != 0 ||
        strcmp(xlangdetect_detect(supplementaryHan, sizeof(supplementaryHan) - 1), "chinese") != 0 ||
        strcmp(xlangdetect_detect(japanese, sizeof(japanese) - 1), "japanese") != 0 ||
        strcmp(xlangdetect_detect("12345 !!!", 9), "unknown") != 0) return 1;
    printf("detect('the cat is on the table and it is not bad') =  %s\n",
        xlangdetect_detect(english, strlen(english)));
    printf("  code/script/confidence =  %s  /  %s  /  %" PRId64 "\n",
        xlangdetect_code(english, strlen(english)),
        xlangdetect_script(english, strlen(english)),
        xlangdetect_confidence(english, strlen(english)));
    printf("detect(' le chat est dans la maison avec une fille ') =  %s\n",
        xlangdetect_detect(french, strlen(french)));
    printf("detect(' der Hund und die Katze sind nicht das Haus ') =  %s\n",
        xlangdetect_detect(german, strlen(german)));
    printf("detect(Chinese text) =  %s\n",
        xlangdetect_detect(chinese, sizeof(chinese) - 1));
    printf("detect(Supplementary Han text) =  %s\n",
        xlangdetect_detect(supplementaryHan, sizeof(supplementaryHan) - 1));
    printf("detect(Japanese text) =  %s\n",
        xlangdetect_detect(japanese, sizeof(japanese) - 1));
    printf("detect('12345 !!!') =  %s\n", xlangdetect_detect("12345 !!!", 9));
    puts("langdetect example: PASS");
    return 0;
}
