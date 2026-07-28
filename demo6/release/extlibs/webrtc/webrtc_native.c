/*
 * webrtc_native.c - WebRTC protocol helpers
 * Tier 3: Real-time communication protocol utilities
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

/* ICE candidate type */
const char* xrtc_candidate_type(long long type) {
    switch (type) {
        case 0: return "host";
        case 1: return "srflx";
        case 2: return "prflx";
        case 3: return "relay";
        default: return "unknown";
    }
}

/* ICE connection state */
const char* xrtc_ice_state(long long state) {
    switch (state) {
        case 0: return "new";
        case 1: return "checking";
        case 2: return "connected";
        case 3: return "completed";
        case 4: return "failed";
        case 5: return "disconnected";
        case 6: return "closed";
        default: return "unknown";
    }
}

/* Peer connection state */
const char* xrtc_pc_state(long long state) {
    switch (state) {
        case 0: return "new";
        case 1: return "connecting";
        case 2: return "connected";
        case 3: return "disconnected";
        case 4: return "failed";
        case 5: return "closed";
        default: return "unknown";
    }
}

/* Signaling state */
const char* xrtc_signaling_state(long long state) {
    switch (state) {
        case 0: return "stable";
        case 1: return "have-local-offer";
        case 2: return "have-remote-offer";
        case 3: return "have-local-pranswer";
        case 4: return "have-remote-pranswer";
        case 5: return "closed";
        default: return "unknown";
    }
}

/* SDP type */
const char* xrtc_sdp_type(long long type) {
    switch (type) {
        case 0: return "offer";
        case 1: return "pranswer";
        case 2: return "answer";
        case 3: return "rollback";
        default: return "unknown";
    }
}

/* RTP payload type name (common ones) */
const char* xrtc_payload_name(long long pt) {
    switch (pt) {
        case 0: return "PCMU (G.711 μ-law)";
        case 3: return "GSM";
        case 4: return "G723";
        case 8: return "PCMA (G.711 A-law)";
        case 9: return "G722";
        case 10: return "L16 Stereo";
        case 11: return "L16 Mono";
        case 18: return "G729";
        case 26: return "JPEG";
        case 31: return "H261";
        case 32: return "MP2T (MPEG Transport Stream)";
        case 33: return "MP2V (MPEG Video)";
        case 34: return "H263";
        case 96: return "Dynamic (96)";
        case 100: return "Dynamic (100)";
        case 111: return "Dynamic (111)";
        case 127: return "Dynamic (127)";
        default: return "Unknown";
    }
}

/* DTLS fingerprint hash algorithm */
const char* xrtc_hash_algo(long long algo) {
    switch (algo) {
        case 0: return "sha-1";
        case 1: return "sha-224";
        case 2: return "sha-256";
        case 3: return "sha-384";
        case 4: return "sha-512";
        default: return "unknown";
    }
}

/* SCTP data channel reliability */
const char* xrtc_dc_reliability(long long mode) {
    switch (mode) {
        case 0: return "reliable";
        case 1: return "partial-reliable-ttl";
        case 2: return "partial-reliable-rexmit";
        default: return "unknown";
    }
}

/* Bundle policy */
const char* xrtc_bundle_policy(long long policy) {
    switch (policy) {
        case 0: return "balanced";
        case 1: return "max-compat";
        case 2: return "max-bundle";
        default: return "unknown";
    }
}
