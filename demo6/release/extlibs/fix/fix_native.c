/*
 * fix_native.c - FIX financial protocol
 * Tier 4: finance
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xfix_msg_type(long long id) {
    switch (id) {
        case 0: return "Heartbeat";
        case 1: return "Logon";
        case 2: return "Logout";
        case 3: return "NewOrderSingle";
        case 4: return "ExecutionReport";
        case 5: return "OrderCancelRequest";
        case 6: return "MarketDataRequest";
        case 7: return "MarketDataSnapshot";
        default: return "Unknown";
    }
}

const char* xfix_side(long long id) {
    switch (id) {
        case 0: return "Buy";
        case 1: return "Sell";
        case 2: return "BuyMinus";
        case 3: return "SellPlus";
        case 4: return "SellShort";
        default: return "Unknown";
    }
}

const char* xfix_ord_type(long long id) {
    switch (id) {
        case 0: return "Market";
        case 1: return "Limit";
        case 2: return "Stop";
        case 3: return "StopLimit";
        default: return "Unknown";
    }
}

const char* xfix_ord_status(long long id) {
    switch (id) {
        case 0: return "New";
        case 1: return "PartiallyFilled";
        case 2: return "Filled";
        case 3: return "DoneForDay";
        case 4: return "Canceled";
        case 5: return "Rejected";
        case 6: return "PendingNew";
        default: return "Unknown";
    }
}

const char* xfix_time_in_force(long long id) {
    switch (id) {
        case 0: return "Day";
        case 1: return "GoodTillCancel";
        case 2: return "AtTheOpening";
        case 3: return "ImmediateOrCancel";
        case 4: return "FillOrKill";
        case 5: return "GoodTillDate";
        default: return "Unknown";
    }
}

const char* xfix_tag_name(long long tag) {
    switch (tag) {
        case 8: return "BeginString";
        case 9: return "BodyLength";
        case 11: return "ClOrdID";
        case 35: return "MsgType";
        case 49: return "SenderCompID";
        case 56: return "TargetCompID";
        case 10: return "CheckSum";
        case 55: return "Symbol";
        case 38: return "OrderQty";
        case 44: return "Price";
        default: return "Unknown";
    }
}

/* Compute FIX checksum (sum of bytes mod 256, formatted as 3 digits) */
const char* xfix_checksum(const char *msg) {
    unsigned int sum = 0;
    const char *p = msg;
    while (*p) {
        sum += (unsigned char)*p;
        p++;
    }
    snprintf(buf, sizeof(buf), "%03d", sum % 256);
    return buf;
}
