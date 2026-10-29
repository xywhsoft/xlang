/*
 * rtmp_native.c - RTMP protocol helpers
 * Tier 3: Real-Time Messaging Protocol utilities
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

/* RTMP message type */
const char* xrtmp_msg_type(long long type) {
    switch (type) {
        case 1: return "Set Chunk Size";
        case 2: return "Abort Message";
        case 3: return "Acknowledgement";
        case 4: return "User Control Message";
        case 5: return "Window Acknowledgement Size";
        case 6: return "Set Peer Bandwidth";
        case 8: return "Audio Message";
        case 9: return "Video Message";
        case 15: return "AMF3 Data Message";
        case 16: return "AMF3 Shared Object Message";
        case 17: return "AMF3 Command Message";
        case 18: return "AMF0 Data Message";
        case 19: return "AMF0 Shared Object Message";
        case 20: return "AMF0 Command Message";
        case 22: return "Aggregate Message";
        default: return "Unknown";
    }
}

/* RTMP chunk stream ID (protocol control) */
const char* xrtmp_csid_name(long long csid) {
    switch (csid) {
        case 2: return "Protocol Control";
        case 3: return "Command (invoke)";
        case 4: return "Audio";
        case 5: return "Video";
        case 6: return "Data";
        default: return "Application";
    }
}

/* User control event type */
const char* xrtmp_ucm_event(long long event) {
    switch (event) {
        case 0: return "Stream Begin";
        case 1: return "Stream EOF";
        case 2: return "Stream Dry";
        case 3: return "Set Buffer Length";
        case 4: return "Stream Is Recorded";
        case 6: return "Ping Request";
        case 7: return "Ping Response";
        case 31: return "Buffer Empty";
        case 32: return "Buffer Ready";
        default: return "Unknown";
    }
}

/* Audio codec ID */
const char* xrtmp_audio_codec(long long codec) {
    switch (codec) {
        case 0: return "Linear PCM (platform endian)";
        case 1: return "ADPCM";
        case 2: return "MP3";
        case 3: return "Linear PCM (little endian)";
        case 4: return "Nellymoser 16kHz";
        case 5: return "Nellymoser 8kHz";
        case 6: return "Nellymoser";
        case 7: return "G.711 A-law";
        case 8: return "G.711 mu-law";
        case 10: return "AAC";
        case 11: return "Speex";
        case 14: return "MP3 8kHz";
        case 15: return "Device-specific";
        default: return "Unknown";
    }
}

/* Video codec ID */
const char* xrtmp_video_codec(long long codec) {
    switch (codec) {
        case 1: return "JPEG";
        case 2: return "Sorenson H.263";
        case 3: return "Screen Video";
        case 4: return "On2 VP6";
        case 5: return "On2 VP6 with Alpha";
        case 6: return "Screen Video v2";
        case 7: return "AVC (H.264)";
        case 12: return "HEVC (H.265)";
        case 13: return "AV1";
        default: return "Unknown";
    }
}

/* Video frame type */
const char* xrtmp_frame_type(long long type) {
    switch (type) {
        case 1: return "Keyframe";
        case 2: return "Inter frame";
        case 3: return "Disposable inter frame";
        case 4: return "Generated keyframe";
        case 5: return "Video info/command frame";
        default: return "Unknown";
    }
}

/* RTMP command name */
const char* xrtmp_command_name(long long cmd) {
    switch (cmd) {
        case 0: return "connect";
        case 1: return "createStream";
        case 2: return "play";
        case 3: return "publish";
        case 4: return "deleteStream";
        case 5: return "closeStream";
        case 6: return "receiveAudio";
        case 7: return "receiveVideo";
        case 8: return "pause";
        case 9: return "seek";
        case 10: return "getStreamLength";
        case 11: return "FCPublish";
        case 12: return "FCUnpublish";
        case 13: return "releaseStream";
        case 14: return "FCSubscribe";
        default: return "Unknown";
    }
}

/* Validate RTMP URL */
long long xrtmp_valid_url(const char *url) {
    if (strncmp(url, "rtmp://", 7) == 0) return 1;
    if (strncmp(url, "rtmps://", 8) == 0) return 1;
    if (strncmp(url, "rtmpt://", 8) == 0) return 1;
    if (strncmp(url, "rtmpe://", 8) == 0) return 1;
    if (strncmp(url, "rtmpte://", 9) == 0) return 1;
    return 0;
}
