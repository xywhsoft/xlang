/*
 * opus_native.c - Opus audio codec helpers
 * Tier 3: audio codec protocol utilities
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

/* Opus application mode */
const char* xopus_application(long long mode) {
    switch (mode) {
        case 2048: return "VOIP";
        case 2049: return "AUDIO";
        case 2051: return "LOWDELAY";
        default: return "UNKNOWN";
    }
}

/* Opus bandwidth */
const char* xopus_bandwidth(long long bw) {
    switch (bw) {
        case 1101: return "NARROWBAND (4kHz)";
        case 1102: return "MEDIUMBAND (6kHz)";
        case 1103: return "WIDEBAND (8kHz)";
        case 1104: return "SUPERWIDEBAND (12kHz)";
        case 1105: return "FULLBAND (20kHz)";
        default: return "UNKNOWN";
    }
}

/* Opus signal type */
const char* xopus_signal(long long signal) {
    switch (signal) {
        case -1000: return "AUTO";
        case 3001: return "VOICE";
        case 3002: return "MUSIC";
        default: return "UNKNOWN";
    }
}

/* Opus frame size (in samples at 48kHz) */
const char* xopus_frame_size(long long samples) {
    switch (samples) {
        case 120: return "2.5ms";
        case 240: return "5ms";
        case 480: return "10ms";
        case 960: return "20ms";
        case 1920: return "40ms";
        case 2880: return "60ms";
        default: return "UNKNOWN";
    }
}

/* Sample rate to name */
const char* xopus_sample_rate(long long rate) {
    switch (rate) {
        case 8000: return "8kHz (narrowband)";
        case 12000: return "12kHz (mediumband)";
        case 16000: return "16kHz (wideband)";
        case 24000: return "24kHz (super-wideband)";
        case 48000: return "48kHz (fullband)";
        default: return "unsupported";
    }
}

/* Channels configuration */
const char* xopus_channels(long long ch) {
    switch (ch) {
        case 1: return "Mono";
        case 2: return "Stereo";
        default: return "Unknown";
    }
}

/* Bitrate recommendation based on application */
long long xopus_recommended_bitrate(long long app, long long channels) {
    if (app == 2048) { /* VOIP */
        return channels == 1 ? 24000 : 32000;
    }
    if (app == 2049) { /* AUDIO */
        return channels == 1 ? 96000 : 128000;
    }
    return 64000;
}

/* Opus error code */
const char* xopus_error_name(long long code) {
    switch (code) {
        case 0: return "OK";
        case -1: return "BAD_ARG";
        case -2: return "BUFFER_TOO_SMALL";
        case -3: return "INTERNAL_ERROR";
        case -4: return "INVALID_PACKET";
        case -5: return "UNIMPLEMENTED";
        case -6: return "INVALID_STATE";
        case -7: return "ALLOC_FAIL";
        default: return "UNKNOWN";
    }
}
