/*
 * quic_native.c - QUIC protocol helpers
 * Tier 3: QUIC transport protocol utilities (RFC 9000)
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

/* QUIC packet type */
const char* xquic_packet_type(long long type) {
    switch (type) {
        case 0: return "Initial";
        case 1: return "0-RTT";
        case 2: return "Handshake";
        case 3: return "Retry";
        case 4: return "Version Negotiation";
        case 5: return "Short Header (1-RTT)";
        default: return "Unknown";
    }
}

/* QUIC frame type */
const char* xquic_frame_type(long long type) {
    switch (type) {
        case 0x00: return "PADDING";
        case 0x01: return "PING";
        case 0x02: return "ACK";
        case 0x03: return "ACK_ECN";
        case 0x04: return "RESET_STREAM";
        case 0x05: return "STOP_SENDING";
        case 0x06: return "CRYPTO";
        case 0x07: return "NEW_TOKEN";
        case 0x08: return "STREAM";
        case 0x10: return "MAX_DATA";
        case 0x11: return "MAX_STREAM_DATA";
        case 0x12: return "MAX_STREAMS_BIDI";
        case 0x13: return "MAX_STREAMS_UNI";
        case 0x14: return "DATA_BLOCKED";
        case 0x15: return "STREAM_DATA_BLOCKED";
        case 0x16: return "STREAMS_BLOCKED_BIDI";
        case 0x17: return "STREAMS_BLOCKED_UNI";
        case 0x18: return "NEW_CONNECTION_ID";
        case 0x19: return "RETIRE_CONNECTION_ID";
        case 0x1a: return "PATH_CHALLENGE";
        case 0x1b: return "PATH_RESPONSE";
        case 0x1c: return "CONNECTION_CLOSE";
        case 0x1d: return "CONNECTION_CLOSE_APP";
        case 0x1e: return "HANDSHAKE_DONE";
        default: return "UNKNOWN";
    }
}

/* QUIC transport error code */
const char* xquic_error_name(long long code) {
    switch (code) {
        case 0x00: return "NO_ERROR";
        case 0x01: return "INTERNAL_ERROR";
        case 0x02: return "CONNECTION_REFUSED";
        case 0x03: return "FLOW_CONTROL_ERROR";
        case 0x04: return "STREAM_LIMIT_ERROR";
        case 0x05: return "STREAM_STATE_ERROR";
        case 0x06: return "FINAL_SIZE_ERROR";
        case 0x07: return "FRAME_ENCODING_ERROR";
        case 0x08: return "TRANSPORT_PARAMETER_ERROR";
        case 0x09: return "CONNECTION_ID_LIMIT_ERROR";
        case 0x0a: return "PROTOCOL_VIOLATION";
        case 0x0b: return "INVALID_TOKEN";
        case 0x0c: return "APPLICATION_ERROR";
        case 0x0d: return "CRYPTO_BUFFER_EXCEEDED";
        case 0x0e: return "KEY_UPDATE_ERROR";
        case 0x0f: return "AEAD_LIMIT_REACHED";
        case 0x10: return "NO_VIABLE_PATH";
        default: return "UNKNOWN";
    }
}

/* QUIC transport parameter ID */
const char* xquic_param_name(long long id) {
    switch (id) {
        case 0x00: return "original_destination_connection_id";
        case 0x01: return "max_idle_timeout";
        case 0x02: return "stateless_reset_token";
        case 0x03: return "max_udp_payload_size";
        case 0x04: return "initial_max_data";
        case 0x05: return "initial_max_stream_data_bidi_local";
        case 0x06: return "initial_max_stream_data_bidi_remote";
        case 0x07: return "initial_max_stream_data_uni";
        case 0x08: return "initial_max_streams_bidi";
        case 0x09: return "initial_max_streams_uni";
        case 0x0a: return "ack_delay_exponent";
        case 0x0b: return "max_ack_delay";
        case 0x0c: return "disable_active_migration";
        case 0x0d: return "preferred_address";
        case 0x0e: return "active_connection_id_limit";
        case 0x0f: return "initial_source_connection_id";
        case 0x10: return "retry_source_connection_id";
        default: return "unknown";
    }
}

/* QUIC version */
const char* xquic_version_name(long long version) {
    switch ((unsigned int)version) {
        case 0x00000001: return "QUIC v1 (RFC 9000)";
        case 0x6b3343cf: return "QUIC v2 (RFC 9369)";
        case 0xff000000: return "QUIC draft-29";
        case 0x00000000: return "Version Negotiation";
        default: return "Unknown";
    }
}

/* Stream type from stream ID */
const char* xquic_stream_type(long long stream_id) {
    int bidi = (stream_id & 0x02) == 0;
    int client_init = (stream_id & 0x01) == 0;
    if (bidi && client_init) return "client-initiated bidirectional";
    if (bidi && !client_init) return "server-initiated bidirectional";
    if (!bidi && client_init) return "client-initiated unidirectional";
    return "server-initiated unidirectional";
}

/* Encode QUIC variable-length integer (1/2/4/8 bytes) to hex */
const char* xquic_encode_varint(long long value) {
    unsigned long long v = (unsigned long long)value;
    int pos = 0;
    if (v <= 63) {
        sprintf(buf, "%02llx", v);
    } else if (v <= 16383) {
        unsigned short val = (unsigned short)(0x4000 | v);
        sprintf(buf, "%02x%02x", (val >> 8) & 0xff, val & 0xff);
    } else if (v <= 1073741823) {
        unsigned int val = (unsigned int)(0x80000000 | v);
        sprintf(buf, "%02x%02x%02x%02x", (val>>24)&0xff, (val>>16)&0xff, (val>>8)&0xff, val&0xff);
    } else {
        unsigned long long val = 0xC000000000000000ULL | v;
        for (int i = 7; i >= 0; i--) {
            pos += sprintf(buf + pos, "%02x", (int)((val >> (i*8)) & 0xff));
        }
    }
    return buf;
}
