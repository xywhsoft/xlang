/*
 * ffmpeg_native.c - Audio/video codec helpers
 * Tier 4: multimedia
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xffmpeg_codec(long long id) {
    switch (id) {
        case 0: return "h264";
        case 1: return "h265";
        case 2: return "vp9";
        case 3: return "av1";
        case 4: return "aac";
        case 5: return "mp3";
        case 6: return "opus";
        case 7: return "flac";
        case 8: return "vorbis";
        case 9: return "pcm_s16le";
        default: return "Unknown";
    }
}

const char* xffmpeg_container(long long id) {
    switch (id) {
        case 0: return "mp4";
        case 1: return "mkv";
        case 2: return "webm";
        case 3: return "avi";
        case 4: return "flv";
        case 5: return "ts";
        case 6: return "ogg";
        case 7: return "wav";
        case 8: return "flac";
        case 9: return "hls";
        default: return "Unknown";
    }
}

const char* xffmpeg_pixel_format(long long id) {
    switch (id) {
        case 0: return "yuv420p";
        case 1: return "yuv422p";
        case 2: return "yuv444p";
        case 3: return "rgb24";
        case 4: return "bgr24";
        case 5: return "rgba";
        case 6: return "nv12";
        case 7: return "gray";
        default: return "Unknown";
    }
}

const char* xffmpeg_filter(long long id) {
    switch (id) {
        case 0: return "scale";
        case 1: return "crop";
        case 2: return "overlay";
        case 3: return "transpose";
        case 4: return "hflip";
        case 5: return "vflip";
        case 6: return "fps";
        case 7: return "trim";
        case 8: return "volume";
        case 9: return "aresample";
        default: return "Unknown";
    }
}

/* Calculate bitrate in kbps from file size (bytes) and duration (seconds) */
long long xffmpeg_calc_bitrate(long long file_size, long long duration_sec) {
    if (duration_sec <= 0) return 0;
    return (file_size * 8) / (duration_sec * 1000);
}

const char* xffmpeg_profile(long long codec, long long profile) {
    if (codec == 0) { /* h264 */
        switch (profile) {
            case 0: return "baseline";
            case 1: return "main";
            case 2: return "high";
            case 3: return "high 10";
            case 4: return "high 4:2:2";
            default: return "Unknown";
        }
    }
    if (codec == 1) { /* h265 */
        switch (profile) {
            case 0: return "main";
            case 1: return "main 10";
            case 2: return "main still picture";
            default: return "Unknown";
        }
    }
    return "Unknown";
}

const char* xffmpeg_color_space(long long id) {
    switch (id) {
        case 0: return "bt709";
        case 1: return "bt601";
        case 2: return "bt2020";
        case 3: return "smpte170m";
        case 4: return "srgb";
        default: return "Unknown";
    }
}
