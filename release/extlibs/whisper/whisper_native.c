/*
 * whisper_native.c - Speech recognition
 * Tier 5: ai_inference
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xwhisper_model(long long id) {
    switch (id) {
        case 0: return "tiny";
        case 1: return "base";
        case 2: return "small";
        case 3: return "medium";
        case 4: return "large";
        case 5: return "tiny.en";
        case 6: return "base.en";
        case 7: return "small.en";
        default: return "Unknown";
    }
}

const char* xwhisper_language(long long id) {
    switch (id) {
        case 0: return "en";
        case 1: return "zh";
        case 2: return "ja";
        case 3: return "ko";
        case 4: return "fr";
        case 5: return "de";
        case 6: return "es";
        case 7: return "ru";
        default: return "Unknown";
    }
}

const char* xwhisper_task(long long id) {
    switch (id) {
        case 0: return "transcribe";
        case 1: return "translate";
        default: return "Unknown";
    }
}

/* Convert timestamp in ms to "HH:MM:SS.mmm" format */
const char* xwhisper_timestamp(long long ms) {
    long long h = ms / 3600000;
    long long m = (ms % 3600000) / 60000;
    long long s = (ms % 60000) / 1000;
    long long millis = ms % 1000;
    snprintf(buf, sizeof(buf), "%02lld:%02lld:%02lld.%03lld", h, m, s, millis);
    return buf;
}

/* Model parameters count in millions */
long long xwhisper_params(long long model_id) {
    switch (model_id) {
        case 0: return 39;    /* tiny */
        case 1: return 74;    /* base */
        case 2: return 244;   /* small */
        case 3: return 769;   /* medium */
        case 4: return 1550;  /* large */
        default: return 0;
    }
}

const char* xwhisper_strategy(long long id) {
    switch (id) {
        case 0: return "greedy";
        case 1: return "beam_search";
        default: return "Unknown";
    }
}
