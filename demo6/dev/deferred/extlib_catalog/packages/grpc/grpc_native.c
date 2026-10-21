/*
 * grpc_native.c - gRPC remote procedure calls
 * Tier 5: network
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xgrpc_status(long long id) {
    switch (id) {
        case 0: return "OK";
        case 1: return "CANCELLED";
        case 2: return "UNKNOWN";
        case 3: return "INVALID_ARGUMENT";
        case 4: return "DEADLINE_EXCEEDED";
        case 5: return "NOT_FOUND";
        case 6: return "ALREADY_EXISTS";
        case 7: return "PERMISSION_DENIED";
        case 8: return "RESOURCE_EXHAUSTED";
        case 9: return "UNAVAILABLE";
        case 10: return "INTERNAL";
        default: return "Unknown";
    }
}

const char* xgrpc_compression(long long id) {
    switch (id) {
        case 0: return "none";
        case 1: return "gzip";
        case 2: return "deflate";
        default: return "Unknown";
    }
}

const char* xgrpc_channel_state(long long id) {
    switch (id) {
        case 0: return "IDLE";
        case 1: return "CONNECTING";
        case 2: return "READY";
        case 3: return "TRANSIENT_FAILURE";
        case 4: return "SHUTDOWN";
        default: return "Unknown";
    }
}

const char* xgrpc_metadata_key(long long id) {
    switch (id) {
        case 0: return "content-type";
        case 1: return "authorization";
        case 2: return "user-agent";
        case 3: return "grpc-encoding";
        case 4: return "grpc-accept-encoding";
        default: return "Unknown";
    }
}

/* Deadline in ms from seconds */
long long xgrpc_deadline_ms(long long seconds) {
    return seconds * 1000;
}

const char* xgrpc_call_type(long long id) {
    switch (id) {
        case 0: return "unary";
        case 1: return "server_streaming";
        case 2: return "client_streaming";
        case 3: return "bidirectional";
        default: return "Unknown";
    }
}
