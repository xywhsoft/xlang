/*
 * jieba_native.c - Chinese text segmentation
 * Tier 4: NLP
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xjieba_mode(long long id) {
    switch (id) {
        case 0: return "accurate";
        case 1: return "full";
        case 2: return "search engine";
        default: return "Unknown";
    }
}

const char* xjieba_pos_tag(long long id) {
    switch (id) {
        case 0: return "n (noun)";
        case 1: return "v (verb)";
        case 2: return "a (adjective)";
        case 3: return "d (adverb)";
        case 4: return "p (preposition)";
        case 5: return "c (conjunction)";
        case 6: return "u (auxiliary)";
        case 7: return "r (pronoun)";
        case 8: return "m (numeral)";
        case 9: return "q (classifier)";
        case 10: return "nr (person name)";
        case 11: return "ns (place name)";
        case 12: return "nt (org name)";
        case 13: return "eng (english)";
        case 14: return "x (unknown)";
        default: return "Unknown";
    }
}

const char* xjieba_dict_type(long long id) {
    switch (id) {
        case 0: return "main dictionary";
        case 1: return "user dictionary";
        case 2: return "HMM model";
        case 3: return "stop words";
        case 4: return "idf dictionary";
        default: return "Unknown";
    }
}

/* Simple word frequency hash for demonstration */
long long xjieba_word_freq(const char *word) {
    unsigned long long h = 0;
    const char *p = word;
    while (*p) {
        h = h * 31 + (unsigned char)*p;
        p++;
    }
    return (long long)(h % 100000) + 1;
}

const char* xjieba_tokenizer(long long id) {
    switch (id) {
        case 0: return "jieba";
        case 1: return "pkuseg";
        case 2: return "thulac";
        case 3: return "ltp";
        case 4: return "hanlp";
        default: return "Unknown";
    }
}

const char* xjieba_encoding(long long id) {
    switch (id) {
        case 0: return "UTF-8";
        case 1: return "GBK";
        case 2: return "GB2312";
        case 3: return "BIG5";
        default: return "Unknown";
    }
}

/* Count characters in a UTF-8 string */
long long xjieba_char_count(const char *text) {
    long long count = 0;
    const unsigned char *p = (const unsigned char *)text;
    while (*p) {
        if (*p < 0x80) { p += 1; }
        else if (*p < 0xE0) { p += 2; }
        else if (*p < 0xF0) { p += 3; }
        else { p += 4; }
        count++;
    }
    return count;
}
