/*
 * nats_native.c - NATS messaging protocol helpers
 * Tier 3: cloud-native messaging system protocol utilities
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

/* NATS protocol operation */
const char* xnats_op_name(long long op) {
    switch (op) {
        case 0: return "INFO";
        case 1: return "CONNECT";
        case 2: return "PUB";
        case 3: return "HPUB";
        case 4: return "SUB";
        case 5: return "UNSUB";
        case 6: return "MSG";
        case 7: return "HMSG";
        case 8: return "PING";
        case 9: return "PONG";
        case 10: return "OK";
        case 11: return "ERR";
        default: return "UNKNOWN";
    }
}

/* NATS error messages */
const char* xnats_error_name(long long code) {
    switch (code) {
        case 0: return "Unknown Protocol Operation";
        case 1: return "Attempted To Connect To Route Port";
        case 2: return "Authorization Violation";
        case 3: return "Authorization Timeout";
        case 4: return "Invalid Client Protocol";
        case 5: return "Maximum Control Line Exceeded";
        case 6: return "Parser Error";
        case 7: return "Secure Connection Required";
        case 8: return "Stale Connection";
        case 9: return "Maximum Connections Exceeded";
        case 10: return "Slow Consumer";
        case 11: return "Maximum Payload Violation";
        case 12: return "Invalid Subject";
        case 13: return "Permissions Violation for Subscription";
        case 14: return "Permissions Violation for Publish";
        default: return "Unknown Error";
    }
}

/* Subject token match: supports * (single token) and > (multi-token tail) */
long long xnats_subject_match(const char *pattern, const char *subject) {
    const char *p = pattern;
    const char *s = subject;
    while (*p && *s) {
        if (*p == '>') {
            /* > matches rest of subject (must be last token) */
            return 1;
        }
        if (*p == '*') {
            /* * matches one token */
            while (*s && *s != '.') s++;
            p++;
            if (*p == '.' && *s == '.') { p++; s++; }
            else if (*p == '\0' && *s == '\0') return 1;
            else if (*p == '\0' && *s == '.') return 0;
            continue;
        }
        if (*p != *s) return 0;
        p++;
        s++;
    }
    if (*p == '>' && *s == '\0') return 1;
    return (*p == '\0' && *s == '\0') ? 1 : 0;
}

/* JetStream stream storage type */
const char* xnats_storage_type(long long type) {
    switch (type) {
        case 0: return "file";
        case 1: return "memory";
        default: return "unknown";
    }
}

/* JetStream consumer ack policy */
const char* xnats_ack_policy(long long policy) {
    switch (policy) {
        case 0: return "none";
        case 1: return "all";
        case 2: return "explicit";
        default: return "unknown";
    }
}

/* JetStream consumer deliver policy */
const char* xnats_deliver_policy(long long policy) {
    switch (policy) {
        case 0: return "all";
        case 1: return "last";
        case 2: return "new";
        case 3: return "by_start_sequence";
        case 4: return "by_start_time";
        case 5: return "last_per_subject";
        default: return "unknown";
    }
}

/* JetStream consumer replay policy */
const char* xnats_replay_policy(long long policy) {
    switch (policy) {
        case 0: return "instant";
        case 1: return "original";
        default: return "unknown";
    }
}

/* NATS URL validation (simplified) */
long long xnats_valid_url(const char *url) {
    if (strncmp(url, "nats://", 7) == 0) return 1;
    if (strncmp(url, "tls://", 6) == 0) return 1;
    if (strncmp(url, "ws://", 5) == 0) return 1;
    if (strncmp(url, "wss://", 6) == 0) return 1;
    return 0;
}
