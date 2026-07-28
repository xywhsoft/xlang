/*
 * kafka_native.c - Apache Kafka protocol helpers
 * Tier 3: distributed streaming platform protocol utilities
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

/* API key to name */
const char* xkafka_api_name(long long api_key) {
    switch (api_key) {
        case 0: return "Produce";
        case 1: return "Fetch";
        case 2: return "ListOffsets";
        case 3: return "Metadata";
        case 8: return "OffsetCommit";
        case 9: return "OffsetFetch";
        case 10: return "FindCoordinator";
        case 11: return "JoinGroup";
        case 12: return "Heartbeat";
        case 13: return "LeaveGroup";
        case 14: return "SyncGroup";
        case 15: return "DescribeGroups";
        case 16: return "ListGroups";
        case 17: return "SaslHandshake";
        case 18: return "ApiVersions";
        case 19: return "CreateTopics";
        case 20: return "DeleteTopics";
        case 21: return "DeleteRecords";
        case 22: return "InitProducerId";
        case 23: return "OffsetForLeaderEpoch";
        case 24: return "AddPartitionsToTxn";
        case 25: return "AddOffsetsToTxn";
        case 26: return "EndTxn";
        case 28: return "TxnOffsetCommit";
        case 32: return "DescribeConfigs";
        case 33: return "AlterConfigs";
        case 36: return "SaslAuthenticate";
        case 37: return "CreatePartitions";
        case 42: return "DeleteGroups";
        case 50: return "DescribeAcls";
        case 51: return "CreateAcls";
        case 52: return "DeleteAcls";
        case 56: return "AlterPartitionReassignments";
        case 60: return "DescribeClientQuotas";
        case 67: return "AlterClientQuotas";
        default: return "Unknown";
    }
}

/* Error code to name */
const char* xkafka_error_name(long long code) {
    switch (code) {
        case 0: return "NONE";
        case 1: return "OFFSET_OUT_OF_RANGE";
        case 2: return "CORRUPT_MESSAGE";
        case 3: return "UNKNOWN_TOPIC_OR_PARTITION";
        case 5: return "LEADER_NOT_AVAILABLE";
        case 6: return "NOT_LEADER_FOR_PARTITION";
        case 7: return "REQUEST_TIMED_OUT";
        case 8: return "BROKER_NOT_AVAILABLE";
        case 9: return "REPLICA_NOT_AVAILABLE";
        case 10: return "MESSAGE_TOO_LARGE";
        case 11: return "STALE_CONTROLLER_EPOCH";
        case 12: return "OFFSET_METADATA_TOO_LARGE";
        case 13: return "NETWORK_EXCEPTION";
        case 14: return "COORDINATOR_LOAD_IN_PROGRESS";
        case 15: return "COORDINATOR_NOT_AVAILABLE";
        case 16: return "NOT_COORDINATOR";
        case 17: return "INVALID_TOPIC_EXCEPTION";
        case 19: return "NOT_ENOUGH_REPLICAS";
        case 22: return "ILLEGAL_GENERATION";
        case 25: return "UNKNOWN_MEMBER_ID";
        case 27: return "REBALANCE_IN_PROGRESS";
        case 29: return "TOPIC_AUTHORIZATION_FAILED";
        case 30: return "GROUP_AUTHORIZATION_FAILED";
        case 36: return "UNSUPPORTED_SASL_MECHANISM";
        case 48: return "UNSUPPORTED_FOR_MESSAGE_FORMAT";
        case 51: return "DUPLICATE_SEQUENCE_NUMBER";
        case 59: return "UNKNOWN_PRODUCER_ID";
        default: return "UNKNOWN_ERROR";
    }
}

/* Compression codec */
const char* xkafka_compression(long long codec) {
    switch (codec) {
        case 0: return "none";
        case 1: return "gzip";
        case 2: return "snappy";
        case 3: return "lz4";
        case 4: return "zstd";
        default: return "unknown";
    }
}

/* Message magic version info */
const char* xkafka_magic_info(long long magic) {
    switch (magic) {
        case 0: return "v0.8-0.9: no timestamp";
        case 1: return "v0.10.0: CreateTime/LogAppendTime";
        case 2: return "v0.11+: record batch, transactions";
        default: return "unknown";
    }
}

/* Isolation level */
const char* xkafka_isolation(long long level) {
    switch (level) {
        case 0: return "READ_UNCOMMITTED";
        case 1: return "READ_COMMITTED";
        default: return "UNKNOWN";
    }
}

/* Acks setting description */
const char* xkafka_acks_desc(long long acks) {
    switch (acks) {
        case 0: return "fire-and-forget";
        case 1: return "leader-ack";
        case -1: return "all-replicas-ack";
        default: return "unknown";
    }
}

/* Group state */
const char* xkafka_group_state(long long state) {
    switch (state) {
        case 0: return "Empty";
        case 1: return "Stable";
        case 2: return "PreparingRebalance";
        case 3: return "CompletingRebalance";
        case 4: return "Dead";
        default: return "Unknown";
    }
}

/* Transaction state */
const char* xkafka_txn_state(long long state) {
    switch (state) {
        case 0: return "Ongoing";
        case 1: return "PrepareCommit";
        case 2: return "PrepareAbort";
        case 3: return "CompleteCommit";
        case 4: return "CompleteAbort";
        case 5: return "Empty";
        case 6: return "PrepareEpochFence";
        default: return "Unknown";
    }
}
