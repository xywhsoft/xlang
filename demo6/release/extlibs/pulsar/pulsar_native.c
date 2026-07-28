/*
 * pulsar_native.c - Apache Pulsar protocol helpers
 * Tier 3: distributed messaging/streaming platform protocol utilities
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

/* Pulsar command type */
const char* xpulsar_cmd_name(long long type) {
    switch (type) {
        case 2: return "CONNECT";
        case 3: return "CONNECTED";
        case 4: return "SUBSCRIBE";
        case 5: return "PRODUCER";
        case 6: return "SEND";
        case 7: return "SEND_RECEIPT";
        case 8: return "SEND_ERROR";
        case 9: return "MESSAGE";
        case 10: return "ACK";
        case 11: return "FLOW";
        case 12: return "UNSUBSCRIBE";
        case 13: return "SUCCESS";
        case 14: return "ERROR";
        case 15: return "CLOSE_PRODUCER";
        case 16: return "CLOSE_CONSUMER";
        case 17: return "PRODUCER_SUCCESS";
        case 18: return "PING";
        case 19: return "PONG";
        case 20: return "REDELIVER_UNACKNOWLEDGED_MESSAGES";
        case 21: return "PARTITIONED_METADATA";
        case 22: return "PARTITIONED_METADATA_RESPONSE";
        case 23: return "LOOKUP";
        case 24: return "LOOKUP_RESPONSE";
        case 25: return "CONSUMER_STATS";
        case 26: return "CONSUMER_STATS_RESPONSE";
        case 27: return "REACHED_END_OF_TOPIC";
        case 28: return "SEEK";
        case 29: return "GET_LAST_MESSAGE_ID";
        case 30: return "GET_LAST_MESSAGE_ID_RESPONSE";
        case 31: return "GET_TOPICS_OF_NAMESPACE";
        case 32: return "GET_TOPICS_OF_NAMESPACE_RESPONSE";
        case 33: return "GET_SCHEMA";
        case 34: return "GET_SCHEMA_RESPONSE";
        case 35: return "AUTH_CHALLENGE";
        case 36: return "AUTH_RESPONSE";
        default: return "UNKNOWN";
    }
}

/* Server error codes */
const char* xpulsar_error_name(long long code) {
    switch (code) {
        case 0: return "UnknownError";
        case 1: return "MetadataError";
        case 2: return "PersistenceError";
        case 3: return "AuthenticationError";
        case 4: return "AuthorizationError";
        case 5: return "ConsumerBusy";
        case 6: return "ServiceNotReady";
        case 7: return "ProducerBlockedQuotaExceededError";
        case 8: return "ProducerBlockedQuotaExceededException";
        case 9: return "ChecksumError";
        case 10: return "UnsupportedVersionError";
        case 11: return "TopicNotFound";
        case 12: return "SubscriptionNotFound";
        case 13: return "ConsumerNotFound";
        case 14: return "TooManyRequests";
        case 15: return "TopicTerminatedError";
        case 16: return "ProducerBusy";
        case 17: return "InvalidTopicName";
        case 18: return "IncompatibleSchema";
        case 19: return "ConsumerAssignError";
        case 20: return "TransactionCoordinatorNotFound";
        case 21: return "InvalidTxnStatus";
        default: return "Unknown";
    }
}

/* Subscription type */
const char* xpulsar_sub_type(long long type) {
    switch (type) {
        case 0: return "Exclusive";
        case 1: return "Shared";
        case 2: return "Failover";
        case 3: return "Key_Shared";
        default: return "Unknown";
    }
}

/* Compression type */
const char* xpulsar_compression(long long type) {
    switch (type) {
        case 0: return "NONE";
        case 1: return "LZ4";
        case 2: return "ZLIB";
        case 3: return "ZSTD";
        case 4: return "SNAPPY";
        default: return "UNKNOWN";
    }
}

/* Schema type */
const char* xpulsar_schema_type(long long type) {
    switch (type) {
        case 0: return "NONE";
        case 1: return "STRING";
        case 2: return "JSON";
        case 3: return "PROTOBUF";
        case 4: return "AVRO";
        case 5: return "BOOLEAN";
        case 6: return "INT8";
        case 7: return "INT16";
        case 8: return "INT32";
        case 9: return "INT64";
        case 10: return "FLOAT";
        case 11: return "DOUBLE";
        case 12: return "BYTES";
        case 15: return "KEY_VALUE";
        default: return "UNKNOWN";
    }
}

/* Topic domain */
const char* xpulsar_topic_domain(long long domain) {
    switch (domain) {
        case 0: return "persistent";
        case 1: return "non-persistent";
        default: return "unknown";
    }
}

/* Parse topic name: tenant/namespace/topic */
const char* xpulsar_topic_tenant(const char *topic) {
    /* Format: persistent://tenant/namespace/topic */
    const char *p = strstr(topic, "://");
    if (p) p += 3; else p = topic;
    const char *slash = strchr(p, '/');
    if (!slash) return "";
    int len = (int)(slash - p);
    if (len > 255) len = 255;
    memcpy(buf, p, len);
    buf[len] = '\0';
    return buf;
}

const char* xpulsar_topic_namespace(const char *topic) {
    const char *p = strstr(topic, "://");
    if (p) p += 3; else p = topic;
    const char *s1 = strchr(p, '/');
    if (!s1) return "";
    s1++;
    const char *s2 = strchr(s1, '/');
    if (!s2) return "";
    int len = (int)(s2 - s1);
    if (len > 255) len = 255;
    memcpy(buf, s1, len);
    buf[len] = '\0';
    return buf;
}
