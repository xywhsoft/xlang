/*
 * rabbitmq_native.c - RabbitMQ AMQP protocol helpers
 * Tier 3: message broker protocol utilities
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

/* AMQP class name */
const char* xamqp_class_name(long long class_id) {
    switch (class_id) {
        case 10: return "Connection";
        case 20: return "Channel";
        case 30: return "Exchange";
        case 40: return "Queue";
        case 50: return "Basic";
        case 60: return "Tx";
        case 85: return "Confirm";
        default: return "Unknown";
    }
}

/* AMQP method name by class+method */
const char* xamqp_method_name(long long class_id, long long method_id) {
    if (class_id == 10) {
        switch (method_id) {
            case 10: return "Connection.Start";
            case 11: return "Connection.StartOk";
            case 20: return "Connection.Secure";
            case 21: return "Connection.SecureOk";
            case 30: return "Connection.Tune";
            case 31: return "Connection.TuneOk";
            case 40: return "Connection.Open";
            case 41: return "Connection.OpenOk";
            case 50: return "Connection.Close";
            case 51: return "Connection.CloseOk";
            case 60: return "Connection.Blocked";
            case 61: return "Connection.Unblocked";
            default: return "Connection.Unknown";
        }
    }
    if (class_id == 50) {
        switch (method_id) {
            case 10: return "Basic.Qos";
            case 11: return "Basic.QosOk";
            case 20: return "Basic.Consume";
            case 21: return "Basic.ConsumeOk";
            case 30: return "Basic.Cancel";
            case 31: return "Basic.CancelOk";
            case 40: return "Basic.Publish";
            case 50: return "Basic.Return";
            case 60: return "Basic.Deliver";
            case 70: return "Basic.Get";
            case 71: return "Basic.GetOk";
            case 72: return "Basic.GetEmpty";
            case 80: return "Basic.Ack";
            case 90: return "Basic.Reject";
            case 100: return "Basic.RecoverAsync";
            case 110: return "Basic.Recover";
            case 111: return "Basic.RecoverOk";
            case 120: return "Basic.Nack";
            default: return "Basic.Unknown";
        }
    }
    if (class_id == 40) {
        switch (method_id) {
            case 10: return "Queue.Declare";
            case 11: return "Queue.DeclareOk";
            case 20: return "Queue.Bind";
            case 21: return "Queue.BindOk";
            case 30: return "Queue.Purge";
            case 31: return "Queue.PurgeOk";
            case 40: return "Queue.Delete";
            case 41: return "Queue.DeleteOk";
            case 50: return "Queue.Unbind";
            case 51: return "Queue.UnbindOk";
            default: return "Queue.Unknown";
        }
    }
    if (class_id == 30) {
        switch (method_id) {
            case 10: return "Exchange.Declare";
            case 11: return "Exchange.DeclareOk";
            case 20: return "Exchange.Delete";
            case 21: return "Exchange.DeleteOk";
            case 30: return "Exchange.Bind";
            case 31: return "Exchange.BindOk";
            case 40: return "Exchange.Unbind";
            case 51: return "Exchange.UnbindOk";
            default: return "Exchange.Unknown";
        }
    }
    return "Unknown";
}

/* Exchange type */
const char* xamqp_exchange_type(long long type) {
    switch (type) {
        case 0: return "direct";
        case 1: return "fanout";
        case 2: return "topic";
        case 3: return "headers";
        default: return "unknown";
    }
}

/* Reply code description */
const char* xamqp_reply_text(long long code) {
    switch (code) {
        case 311: return "ContentTooLarge";
        case 313: return "NoConsumers";
        case 320: return "ConnectionForced";
        case 402: return "InvalidPath";
        case 403: return "AccessRefused";
        case 404: return "NotFound";
        case 405: return "ResourceLocked";
        case 406: return "PreconditionFailed";
        case 501: return "FrameError";
        case 502: return "SyntaxError";
        case 503: return "CommandInvalid";
        case 504: return "ChannelError";
        case 505: return "UnexpectedFrame";
        case 506: return "ResourceError";
        case 530: return "NotAllowed";
        case 540: return "NotImplemented";
        case 541: return "InternalError";
        default: return "Unknown";
    }
}

/* Frame type */
const char* xamqp_frame_type(long long type) {
    switch (type) {
        case 1: return "METHOD";
        case 2: return "HEADER";
        case 3: return "BODY";
        case 4: return "HEARTBEAT";
        case 8: return "PROTOCOL_HEADER";
        default: return "UNKNOWN";
    }
}

/* Delivery mode */
const char* xamqp_delivery_mode(long long mode) {
    switch (mode) {
        case 1: return "non-persistent";
        case 2: return "persistent";
        default: return "unknown";
    }
}

/* Topic routing key match (simplified: * = one word, # = zero+ words) */
long long xamqp_topic_match(const char *pattern, const char *routing_key) {
    /* Simple implementation: exact match or wildcard */
    if (strcmp(pattern, "#") == 0) return 1;
    if (strcmp(pattern, routing_key) == 0) return 1;
    /* Single wildcard * matches exactly one word */
    if (strcmp(pattern, "*") == 0) {
        /* routing_key must have no dots (single word) */
        return strchr(routing_key, '.') == NULL ? 1 : 0;
    }
    return 0;
}
