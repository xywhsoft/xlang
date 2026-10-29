/*
 * taglib_native.c - TagLib audio metadata helpers
 * Tier 3: audio file metadata utilities
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

/* Tag frame ID (ID3v2) */
const char* xtag_frame_name(const char *id) {
    if (strcmp(id, "TIT2") == 0) return "Title";
    if (strcmp(id, "TPE1") == 0) return "Artist";
    if (strcmp(id, "TALB") == 0) return "Album";
    if (strcmp(id, "TRCK") == 0) return "Track Number";
    if (strcmp(id, "TYER") == 0) return "Year";
    if (strcmp(id, "TDRC") == 0) return "Recording Date";
    if (strcmp(id, "TCON") == 0) return "Genre";
    if (strcmp(id, "COMM") == 0) return "Comment";
    if (strcmp(id, "TPOS") == 0) return "Disc Number";
    if (strcmp(id, "TCOM") == 0) return "Composer";
    if (strcmp(id, "TPE2") == 0) return "Album Artist";
    if (strcmp(id, "TBPM") == 0) return "BPM";
    if (strcmp(id, "TKEY") == 0) return "Initial Key";
    if (strcmp(id, "TLEN") == 0) return "Length";
    if (strcmp(id, "TSRC") == 0) return "ISRC";
    if (strcmp(id, "TCOP") == 0) return "Copyright";
    if (strcmp(id, "TPUB") == 0) return "Publisher";
    if (strcmp(id, "TENC") == 0) return "Encoded By";
    if (strcmp(id, "APIC") == 0) return "Attached Picture";
    if (strcmp(id, "USLT") == 0) return "Lyrics";
    return "Unknown";
}

/* ID3v1 genre ID to name (subset) */
const char* xtag_genre_name(long long id) {
    switch (id) {
        case 0: return "Blues";
        case 1: return "Classic Rock";
        case 2: return "Country";
        case 3: return "Dance";
        case 4: return "Disco";
        case 5: return "Funk";
        case 6: return "Grunge";
        case 7: return "Hip-Hop";
        case 8: return "Jazz";
        case 9: return "Metal";
        case 10: return "New Age";
        case 11: return "Oldies";
        case 12: return "Other";
        case 13: return "Pop";
        case 14: return "R&B";
        case 15: return "Rap";
        case 16: return "Reggae";
        case 17: return "Rock";
        case 18: return "Techno";
        case 19: return "Industrial";
        case 20: return "Alternative";
        case 21: return "Ska";
        case 22: return "Death Metal";
        case 23: return "Pranks";
        case 24: return "Soundtrack";
        case 25: return "Euro-Techno";
        case 26: return "Ambient";
        case 27: return "Trip-Hop";
        case 28: return "Vocal";
        case 29: return "Jazz+Funk";
        case 30: return "Fusion";
        case 31: return "Trance";
        case 32: return "Classical";
        case 33: return "Instrumental";
        case 34: return "Acid";
        case 35: return "House";
        case 36: return "Game";
        case 37: return "Sound Clip";
        case 38: return "Gospel";
        case 39: return "Noise";
        default: return "Unknown";
    }
}

/* File format from extension */
const char* xtag_format_name(const char *ext) {
    if (strcmp(ext, "mp3") == 0) return "MPEG Audio";
    if (strcmp(ext, "flac") == 0) return "FLAC";
    if (strcmp(ext, "ogg") == 0) return "Ogg Vorbis";
    if (strcmp(ext, "opus") == 0) return "Ogg Opus";
    if (strcmp(ext, "m4a") == 0) return "MP4";
    if (strcmp(ext, "aac") == 0) return "AAC";
    if (strcmp(ext, "wav") == 0) return "RIFF WAVE";
    if (strcmp(ext, "wma") == 0) return "ASF/WMA";
    if (strcmp(ext, "aiff") == 0 || strcmp(ext, "aif") == 0) return "AIFF";
    if (strcmp(ext, "ape") == 0) return "Monkey's Audio";
    if (strcmp(ext, "wv") == 0) return "WavPack";
    if (strcmp(ext, "mpc") == 0) return "Musepack";
    return "Unknown";
}

/* Validate ISRC format (CC-XXX-YY-NNNNN) */
long long xtag_valid_isrc(const char *isrc) {
    int len = (int)strlen(isrc);
    if (len != 12) return 0;
    /* First 2 chars: country code (letters) */
    if (!((isrc[0] >= 'A' && isrc[0] <= 'Z') || (isrc[0] >= 'a' && isrc[0] <= 'z'))) return 0;
    if (!((isrc[1] >= 'A' && isrc[1] <= 'Z') || (isrc[1] >= 'a' && isrc[1] <= 'z'))) return 0;
    /* Next 3 chars: registrant (alphanumeric) */
    for (int i = 2; i < 5; i++) {
        char c = isrc[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) return 0;
    }
    /* Next 2 chars: year (digits) */
    if (!(isrc[5] >= '0' && isrc[5] <= '9')) return 0;
    if (!(isrc[6] >= '0' && isrc[6] <= '9')) return 0;
    /* Last 5 chars: designation (digits) */
    for (int i = 7; i < 12; i++) {
        if (!(isrc[i] >= '0' && isrc[i] <= '9')) return 0;
    }
    return 1;
}
