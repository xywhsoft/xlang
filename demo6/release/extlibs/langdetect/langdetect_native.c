/* langdetect_native.c - Simplified language detection via n-gram/unicode analysis */
#include <string.h>
#include <stdlib.h>

static char lang_buf[64];

/* Decode one UTF-8 char, return codepoint and advance pointer */
static unsigned int utf8_decode(const unsigned char *s, int *advance) {
    unsigned char c = s[0];
    if (c < 0x80) { *advance = 1; return c; }
    if ((c & 0xE0) == 0xC0) { *advance = 2; return ((c & 0x1F) << 6) | (s[1] & 0x3F); }
    if ((c & 0xF0) == 0xE0) { *advance = 3; return ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); }
    if ((c & 0xF8) == 0xF0) { *advance = 4; return ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F); }
    *advance = 1; return c;
}

/* Count scripts in text */
static void count_scripts(const char *text, int *cjk, int *hiragana, int *katakana,
                          int *hangul, int *cyrillic, int *arabic, int *latin, int *total) {
    const unsigned char *s = (const unsigned char *)text;
    *cjk = *hiragana = *katakana = *hangul = *cyrillic = *arabic = *latin = *total = 0;
    while (*s) {
        int adv;
        unsigned int cp = utf8_decode(s, &adv);
        s += adv;
        (*total)++;
        if (cp >= 0x4E00 && cp <= 0x9FFF) (*cjk)++;
        else if (cp >= 0x3400 && cp <= 0x4DBF) (*cjk)++;
        else if (cp >= 0x3040 && cp <= 0x309F) (*hiragana)++;
        else if (cp >= 0x30A0 && cp <= 0x30FF) (*katakana)++;
        else if (cp >= 0xAC00 && cp <= 0xD7AF) (*hangul)++;
        else if (cp >= 0x1100 && cp <= 0x11FF) (*hangul)++;
        else if (cp >= 0x0400 && cp <= 0x04FF) (*cyrillic)++;
        else if (cp >= 0x0600 && cp <= 0x06FF) (*arabic)++;
        else if ((cp >= 0x41 && cp <= 0x5A) || (cp >= 0x61 && cp <= 0x7A)) (*latin)++;
        else if (cp >= 0xC0 && cp <= 0x024F) (*latin)++;
    }
}

/* Common words for European language detection */
static int has_word(const char *text, const char *word) {
    return strstr(text, word) != NULL;
}

static int score_english(const char *t) {
    int s = 0;
    if (has_word(t, "the ")) s += 3;
    if (has_word(t, " is ")) s += 2;
    if (has_word(t, " and ")) s += 3;
    if (has_word(t, " of ")) s += 2;
    if (has_word(t, " to ")) s += 2;
    if (has_word(t, " in ")) s += 2;
    if (has_word(t, " that ")) s += 2;
    if (has_word(t, " it ")) s += 1;
    if (has_word(t, " for ")) s += 2;
    if (has_word(t, " was ")) s += 2;
    if (has_word(t, " are ")) s += 2;
    if (has_word(t, " this ")) s += 2;
    if (has_word(t, " have ")) s += 2;
    if (has_word(t, " not ")) s += 1;
    if (has_word(t, " you ")) s += 2;
    return s;
}

static int score_french(const char *t) {
    int s = 0;
    if (has_word(t, " le ")) s += 3;
    if (has_word(t, " la ")) s += 2;
    if (has_word(t, " les ")) s += 2;
    if (has_word(t, " des ")) s += 2;
    if (has_word(t, " est ")) s += 2;
    if (has_word(t, " et ")) s += 2;
    if (has_word(t, " une ")) s += 2;
    if (has_word(t, " dans ")) s += 2;
    if (has_word(t, " pour ")) s += 2;
    if (has_word(t, " pas ")) s += 1;
    if (has_word(t, " que ")) s += 2;
    if (has_word(t, " vous ")) s += 2;
    if (has_word(t, " nous ")) s += 2;
    if (has_word(t, " avec ")) s += 2;
    if (strstr(t, "\xc3\xa9")) s += 2; /* é */
    if (strstr(t, "\xc3\xa8")) s += 2; /* è */
    if (strstr(t, "\xc3\xa7")) s += 2; /* ç */
    return s;
}

static int score_german(const char *t) {
    int s = 0;
    if (has_word(t, " der ")) s += 3;
    if (has_word(t, " die ")) s += 3;
    if (has_word(t, " das ")) s += 3;
    if (has_word(t, " und ")) s += 3;
    if (has_word(t, " ist ")) s += 2;
    if (has_word(t, " ein ")) s += 2;
    if (has_word(t, " eine ")) s += 2;
    if (has_word(t, " nicht ")) s += 2;
    if (has_word(t, " mit ")) s += 1;
    if (has_word(t, " den ")) s += 2;
    if (has_word(t, " von ")) s += 1;
    if (has_word(t, " zu ")) s += 1;
    if (strstr(t, "\xc3\xa4")) s += 2; /* ä */
    if (strstr(t, "\xc3\xb6")) s += 2; /* ö */
    if (strstr(t, "\xc3\xbc")) s += 2; /* ü */
    if (strstr(t, "\xc3\x9f")) s += 3; /* ß */
    return s;
}

static int score_spanish(const char *t) {
    int s = 0;
    if (has_word(t, " el ")) s += 2;
    if (has_word(t, " la ")) s += 2;
    if (has_word(t, " los ")) s += 2;
    if (has_word(t, " las ")) s += 2;
    if (has_word(t, " es ")) s += 1;
    if (has_word(t, " en ")) s += 2;
    if (has_word(t, " que ")) s += 2;
    if (has_word(t, " de ")) s += 2;
    if (has_word(t, " del ")) s += 2;
    if (has_word(t, " una ")) s += 2;
    if (has_word(t, " para ")) s += 2;
    if (has_word(t, " con ")) s += 1;
    if (strstr(t, "\xc3\xb1")) s += 3; /* ñ */
    if (strstr(t, "\xc2\xbf")) s += 3; /* ¿ */
    if (strstr(t, "\xc2\xa1")) s += 3; /* ¡ */
    return s;
}

static int score_portuguese(const char *t) {
    int s = 0;
    if (has_word(t, " o ")) s += 1;
    if (has_word(t, " os ")) s += 2;
    if (has_word(t, " as ")) s += 2;
    if (has_word(t, " um ")) s += 2;
    if (has_word(t, " uma ")) s += 2;
    if (has_word(t, " para ")) s += 2;
    if (has_word(t, " com ")) s += 2;
    if (has_word(t, " nao ")) s += 2;
    if (has_word(t, " dos ")) s += 2;
    if (has_word(t, " das ")) s += 2;
    if (strstr(t, "\xc3\xa3")) s += 3; /* ã */
    if (strstr(t, "\xc3\xb5")) s += 3; /* õ */
    if (strstr(t, "\xc3\xa7")) s += 1; /* ç */
    return s;
}

static int score_italian(const char *t) {
    int s = 0;
    if (has_word(t, " il ")) s += 2;
    if (has_word(t, " la ")) s += 2;
    if (has_word(t, " gli ")) s += 2;
    if (has_word(t, " che ")) s += 2;
    if (has_word(t, " di ")) s += 2;
    if (has_word(t, " del ")) s += 2;
    if (has_word(t, " della ")) s += 2;
    if (has_word(t, " una ")) s += 2;
    if (has_word(t, " per ")) s += 2;
    if (has_word(t, " con ")) s += 1;
    if (has_word(t, " sono ")) s += 2;
    if (has_word(t, " questo ")) s += 2;
    return s;
}

static int score_dutch(const char *t) {
    int s = 0;
    if (has_word(t, " de ")) s += 2;
    if (has_word(t, " het ")) s += 3;
    if (has_word(t, " een ")) s += 3;
    if (has_word(t, " van ")) s += 2;
    if (has_word(t, " en ")) s += 2;
    if (has_word(t, " is ")) s += 1;
    if (has_word(t, " dat ")) s += 2;
    if (has_word(t, " niet ")) s += 2;
    if (has_word(t, " voor ")) s += 2;
    if (has_word(t, " met ")) s += 2;
    if (has_word(t, " zijn ")) s += 2;
    if (strstr(t, "ij")) s += 1;
    return s;
}

static int score_russian(const char *t) {
    int s = 0;
    if (strstr(t, "\xd0\xb5")) s += 1; /* е */
    if (strstr(t, "\xd0\xbe")) s += 1; /* о */
    if (strstr(t, "\xd0\xb0")) s += 1; /* а */
    if (strstr(t, "\xd0\xb8")) s += 1; /* и */
    if (strstr(t, "\xd0\xbd")) s += 1; /* н */
    if (strstr(t, "\xd1\x82")) s += 1; /* т */
    if (strstr(t, "\xd1\x81")) s += 1; /* с */
    if (strstr(t, "\xd1\x80")) s += 1; /* р */
    if (strstr(t, "\xd0\xb2")) s += 1; /* в */
    if (strstr(t, "\xd0\xbb")) s += 1; /* л */
    return s;
}

const char *xlangdetect_detect(const char *text) {
    int cjk, hiragana, katakana, hangul, cyrillic, arabic, latin, total;
    count_scripts(text, &cjk, &hiragana, &katakana, &hangul, &cyrillic, &arabic, &latin, &total);

    if (total == 0) { strcpy(lang_buf, "unknown"); return lang_buf; }

    /* CJK family detection */
    if (hiragana + katakana > 0 && (hiragana + katakana) * 3 > total) {
        strcpy(lang_buf, "japanese"); return lang_buf;
    }
    if (hangul > 0 && hangul * 3 > total) {
        strcpy(lang_buf, "korean"); return lang_buf;
    }
    if (cjk > 0 && cjk * 3 > total) {
        /* If mixed with hiragana/katakana, still japanese */
        if (hiragana + katakana > 0) { strcpy(lang_buf, "japanese"); return lang_buf; }
        strcpy(lang_buf, "chinese"); return lang_buf;
    }
    if (cyrillic > 0 && cyrillic * 2 > total) {
        strcpy(lang_buf, "russian"); return lang_buf;
    }
    if (arabic > 0 && arabic * 2 > total) {
        strcpy(lang_buf, "arabic"); return lang_buf;
    }

    /* European language scoring */
    int best = 0;
    const char *best_lang = "english";
    int s;

    s = score_english(text); if (s > best) { best = s; best_lang = "english"; }
    s = score_french(text); if (s > best) { best = s; best_lang = "french"; }
    s = score_german(text); if (s > best) { best = s; best_lang = "german"; }
    s = score_spanish(text); if (s > best) { best = s; best_lang = "spanish"; }
    s = score_portuguese(text); if (s > best) { best = s; best_lang = "portuguese"; }
    s = score_italian(text); if (s > best) { best = s; best_lang = "italian"; }
    s = score_dutch(text); if (s > best) { best = s; best_lang = "dutch"; }
    s = score_russian(text); if (s > best) { best = s; best_lang = "russian"; }

    strcpy(lang_buf, best_lang);
    return lang_buf;
}
