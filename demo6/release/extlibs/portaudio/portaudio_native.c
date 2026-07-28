/*
 * portaudio_native.c - PortAudio helpers
 * Tier 3: cross-platform audio I/O utilities
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

/* Host API type */
const char* xpa_host_api(long long type) {
    switch (type) {
        case 0: return "In Development";
        case 1: return "DirectSound";
        case 2: return "MME";
        case 3: return "ASIO";
        case 4: return "SoundManager";
        case 5: return "CoreAudio";
        case 7: return "OSS";
        case 8: return "ALSA";
        case 9: return "AL";
        case 10: return "BeOS";
        case 11: return "WDMKS";
        case 12: return "JACK";
        case 13: return "WASAPI";
        case 14: return "AudioScienceHPI";
        default: return "Unknown";
    }
}

/* Sample format */
const char* xpa_sample_format(long long fmt) {
    switch (fmt) {
        case 1: return "Float32";
        case 2: return "Int32";
        case 4: return "Int24";
        case 8: return "Int16";
        case 16: return "Int8";
        case 32: return "UInt8";
        default: return "Unknown";
    }
}

/* Stream flags */
const char* xpa_stream_flag(long long flag) {
    switch (flag) {
        case 0: return "No flags";
        case 1: return "Clip off";
        case 2: return "Dither off";
        case 4: return "Never drop input";
        case 8: return "Prime output";
        case 16: return "Platform-specific flags";
        default: return "Unknown";
    }
}

/* Error code */
const char* xpa_error_name(long long code) {
    switch (code) {
        case 0: return "No error";
        case -10000: return "Not initialized";
        case -9999: return "Unanticipated host error";
        case -9998: return "Invalid channel count";
        case -9997: return "Invalid sample rate";
        case -9996: return "Invalid device";
        case -9995: return "Invalid flag";
        case -9994: return "Unsupported combination";
        case -9993: return "Buffer too big";
        case -9992: return "Buffer too small";
        case -9991: return "Null callback";
        case -9990: return "Bad stream pointer";
        case -9989: return "Timed out";
        case -9988: return "Internal error";
        case -9987: return "Device unavailable";
        case -9986: return "Incompatible host API";
        case -9985: return "Stream is stopped";
        case -9984: return "Stream is not stopped";
        case -9983: return "Input overflowed";
        case -9982: return "Output underflowed";
        case -9981: return "Host API not found";
        case -9980: return "Invalid host API";
        case -9979: return "Can't read from callback stream";
        case -9978: return "Can't write to callback stream";
        case -9977: return "Can't read from non-callback stream";
        case -9976: return "Can't write to non-callback stream";
        default: return "Unknown";
    }
}

/* Standard sample rates */
long long xpa_valid_rate(long long rate) {
    if (rate == 8000) return 1;
    if (rate == 11025) return 1;
    if (rate == 16000) return 1;
    if (rate == 22050) return 1;
    if (rate == 32000) return 1;
    if (rate == 44100) return 1;
    if (rate == 48000) return 1;
    if (rate == 88200) return 1;
    if (rate == 96000) return 1;
    if (rate == 176400) return 1;
    if (rate == 192000) return 1;
    return 0;
}

/* Bytes per sample for format */
long long xpa_bytes_per_sample(long long fmt) {
    switch (fmt) {
        case 1: return 4;  /* Float32 */
        case 2: return 4;  /* Int32 */
        case 4: return 3;  /* Int24 */
        case 8: return 2;  /* Int16 */
        case 16: return 1; /* Int8 */
        case 32: return 1; /* UInt8 */
        default: return 0;
    }
}
