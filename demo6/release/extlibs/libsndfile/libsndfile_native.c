/*
 * libsndfile_native.c - libsndfile helpers
 * Tier 3: audio file I/O utilities
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

/* File format major type */
const char* xsf_format_major(long long fmt) {
    switch (fmt) {
        case 0x010000: return "WAV";
        case 0x020000: return "AIFF";
        case 0x030000: return "AU";
        case 0x040000: return "RAW";
        case 0x050000: return "PAF";
        case 0x060000: return "SVX";
        case 0x070000: return "NIST";
        case 0x080000: return "VOC";
        case 0x0A0000: return "IRCAM";
        case 0x0B0000: return "W64";
        case 0x0C0000: return "MAT4";
        case 0x0D0000: return "MAT5";
        case 0x0E0000: return "PVF";
        case 0x0F0000: return "XI";
        case 0x100000: return "HTK";
        case 0x110000: return "SDS";
        case 0x120000: return "AVR";
        case 0x130000: return "WAVEX";
        case 0x140000: return "SD2";
        case 0x150000: return "FLAC";
        case 0x160000: return "CAF";
        case 0x170000: return "WVE";
        case 0x180000: return "OGG";
        case 0x200000: return "MPC2K";
        case 0x210000: return "RF64";
        case 0x220000: return "MPEG";
        default: return "Unknown";
    }
}

/* Subtype (encoding) */
const char* xsf_format_subtype(long long sub) {
    switch (sub) {
        case 0x0001: return "PCM_S8";
        case 0x0002: return "PCM_16";
        case 0x0003: return "PCM_24";
        case 0x0004: return "PCM_32";
        case 0x0005: return "PCM_U8";
        case 0x0006: return "FLOAT";
        case 0x0007: return "DOUBLE";
        case 0x0010: return "ULAW";
        case 0x0011: return "ALAW";
        case 0x0020: return "IMA_ADPCM";
        case 0x0021: return "MS_ADPCM";
        case 0x0030: return "GSM610";
        case 0x0040: return "VOX_ADPCM";
        case 0x0060: return "G721_32";
        case 0x0061: return "G723_24";
        case 0x0062: return "G723_40";
        case 0x0080: return "DWVW_12";
        case 0x0081: return "DWVW_16";
        case 0x0082: return "DWVW_24";
        case 0x0083: return "DWVW_N";
        case 0x00A0: return "DPCM_8";
        case 0x00A1: return "DPCM_16";
        case 0x00B0: return "VORBIS";
        case 0x00C0: return "OPUS";
        case 0x00D0: return "ALAC_16";
        case 0x00D1: return "ALAC_20";
        case 0x00D2: return "ALAC_24";
        case 0x00D3: return "ALAC_32";
        case 0x00E0: return "MPEG_LAYER_I";
        case 0x00E1: return "MPEG_LAYER_II";
        case 0x00E2: return "MPEG_LAYER_III";
        default: return "Unknown";
    }
}

/* Seek mode */
const char* xsf_seek_mode(long long mode) {
    switch (mode) {
        case 0: return "SET";
        case 1: return "CUR";
        case 2: return "END";
        default: return "Unknown";
    }
}

/* Error code */
const char* xsf_error_name(long long code) {
    switch (code) {
        case 0: return "No error";
        case 1: return "Unrecognised format";
        case 2: return "System error";
        case 3: return "Malformed file";
        case 4: return "Unsupported encoding";
        default: return "Unknown";
    }
}

/* Detect format from file extension */
long long xsf_format_from_ext(const char *ext) {
    if (strcmp(ext, "wav") == 0) return 0x010000;
    if (strcmp(ext, "aif") == 0 || strcmp(ext, "aiff") == 0) return 0x020000;
    if (strcmp(ext, "au") == 0) return 0x030000;
    if (strcmp(ext, "raw") == 0) return 0x040000;
    if (strcmp(ext, "flac") == 0) return 0x150000;
    if (strcmp(ext, "ogg") == 0) return 0x180000;
    if (strcmp(ext, "caf") == 0) return 0x160000;
    if (strcmp(ext, "mp3") == 0) return 0x220000;
    return 0;
}
