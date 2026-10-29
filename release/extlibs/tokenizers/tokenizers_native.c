/*
 * tokenizers_native.c - HuggingFace tokenizers
 * Tier 5: nlp
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xtok_model(long long id) {
    switch (id) {
        case 0: return "BPE";
        case 1: return "WordPiece";
        case 2: return "Unigram";
        case 3: return "WordLevel";
        default: return "Unknown";
    }
}

const char* xtok_normalizer(long long id) {
    switch (id) {
        case 0: return "NFC";
        case 1: return "NFD";
        case 2: return "NFKC";
        case 3: return "NFKD";
        case 4: return "Lowercase";
        case 5: return "Strip";
        default: return "Unknown";
    }
}

const char* xtok_truncation(long long id) {
    switch (id) {
        case 0: return "longest_first";
        case 1: return "only_first";
        case 2: return "only_second";
        default: return "Unknown";
    }
}

const char* xtok_padding(long long id) {
    switch (id) {
        case 0: return "right";
        case 1: return "left";
        default: return "Unknown";
    }
}

/* Count tokens in simple whitespace-separated text */
long long xtok_count_tokens(const char *text) {
    long long count = 0;
    int in_word = 0;
    while (*text) {
        if (*text == ' ' || *text == '\t' || *text == '\n') {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            count++;
        }
        text++;
    }
    return count;
}

/* Simple BPE-like: count characters as tokens */
long long xtok_char_count(const char *text) {
    return (long long)strlen(text);
}

const char* xtok_special_token(long long id) {
    switch (id) {
        case 0: return "[CLS]";
        case 1: return "[SEP]";
        case 2: return "[PAD]";
        case 3: return "[UNK]";
        case 4: return "[MASK]";
        case 5: return "<s>";
        case 6: return "</s>";
        case 7: return "<pad>";
        default: return "Unknown";
    }
}
