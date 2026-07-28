/*
 * opentelemetry_native.c - Distributed tracing
 * Tier 4: observability
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xotel_signal_type(long long id) {
    switch (id) {
        case 0: return "traces";
        case 1: return "metrics";
        case 2: return "logs";
        default: return "Unknown";
    }
}

const char* xotel_span_kind(long long id) {
    switch (id) {
        case 0: return "internal";
        case 1: return "server";
        case 2: return "client";
        case 3: return "producer";
        case 4: return "consumer";
        default: return "Unknown";
    }
}

const char* xotel_span_status(long long id) {
    switch (id) {
        case 0: return "unset";
        case 1: return "ok";
        case 2: return "error";
        default: return "Unknown";
    }
}

const char* xotel_exporter(long long id) {
    switch (id) {
        case 0: return "otlp";
        case 1: return "jaeger";
        case 2: return "zipkin";
        case 3: return "prometheus";
        case 4: return "console";
        case 5: return "none";
        default: return "Unknown";
    }
}

const char* xotel_sampler(long long id) {
    switch (id) {
        case 0: return "always_on";
        case 1: return "always_off";
        case 2: return "trace_id_ratio";
        case 3: return "parent_based";
        default: return "Unknown";
    }
}

const char* xotel_propagator(long long id) {
    switch (id) {
        case 0: return "tracecontext";
        case 1: return "baggage";
        case 2: return "b3";
        case 3: return "b3multi";
        case 4: return "jaeger";
        case 5: return "xray";
        case 6: return "ottrace";
        default: return "Unknown";
    }
}

/* Generate a simple trace ID (32 hex chars) from seed */
const char* xotel_gen_trace_id(long long seed) {
    unsigned long long h1 = (unsigned long long)seed * 6364136223846793005ULL + 1442695040888963407ULL;
    unsigned long long h2 = h1 * 6364136223846793005ULL + 1442695040888963407ULL;
    int i;
    for (i = 0; i < 16; i++) {
        int nibble = (int)((h1 >> (i * 4)) & 0xF);
        buf[i] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
    }
    for (i = 0; i < 16; i++) {
        int nibble = (int)((h2 >> (i * 4)) & 0xF);
        buf[16 + i] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
    }
    buf[32] = '\0';
    return buf;
}

const char* xotel_semconv(long long id) {
    switch (id) {
        case 0: return "http.method";
        case 1: return "http.url";
        case 2: return "http.status_code";
        case 3: return "db.system";
        case 4: return "db.statement";
        case 5: return "rpc.system";
        case 6: return "rpc.method";
        case 7: return "messaging.system";
        default: return "Unknown";
    }
}
