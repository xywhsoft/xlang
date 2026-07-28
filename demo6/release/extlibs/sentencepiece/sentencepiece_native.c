/*
 * sentencepiece_native.c - Language-independent tokenizer
 * Tier 5: nlp
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xspm_model_type(long long id) {
    switch (id) {
        case 0: return "unigram";
        case 1: return "bpe";
        case 2: return "word";
        case 3: return "char";
        default: return "Unknown";
    }
}

/* Encode text to pieces with \xe2\x96\x81 (underscore) prefix for word starts */
const char* xspm_encode(const char *text) {
    int i = 0, j = 0;
    int at_start = 1;
    while (text[i] && j < 490) {
        if (text[i] == ' ') {
            at_start = 1;
            i++;
            continue;
        }
        if (at_start) {
            buf[j++] = '_';
            at_start = 0;
        }
        buf[j++] = text[i];
        i++;
    }
    buf[j] = '\0';
    return buf;
}

/* Count pieces (number of underscore-prefixed segments) */
long long xspm_count_pieces(const char *text) {
    long long count = 0;
    int at_start = 1;
    while (*text) {
        if (*text == ' ') { at_start = 1; text++; continue; }
        if (at_start) { count++; at_start = 0; }
        text++;
    }
    return count;
}

const char* xspm_control_symbol(long long id) {
    switch (id) {
        case 0: return "<unk>";
        case 1: return "<s>";
        case 2: return "</s>";
        case 3: return "<pad>";
        default: return "Unknown";
    }
}

/* Vocabulary size for model type */
long long xspm_vocab_size(long long model_type) {
    switch (model_type) {
        case 0: return 32000;  /* unigram typical */
        case 1: return 32000;  /* bpe typical */
        case 2: return 8000;   /* word */
        case 3: return 256;    /* char */
        default: return 0;
    }
}
