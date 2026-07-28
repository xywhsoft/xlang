/*
 * dns_native.c - DNS protocol helpers
 * Tier 3: Domain Name System protocol utilities
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

/* DNS record type name */
const char* xdns_type_name(long long type) {
    switch (type) {
        case 1: return "A";
        case 2: return "NS";
        case 5: return "CNAME";
        case 6: return "SOA";
        case 12: return "PTR";
        case 13: return "HINFO";
        case 15: return "MX";
        case 16: return "TXT";
        case 28: return "AAAA";
        case 33: return "SRV";
        case 35: return "NAPTR";
        case 39: return "DNAME";
        case 41: return "OPT";
        case 43: return "DS";
        case 44: return "SSHFP";
        case 46: return "RRSIG";
        case 47: return "NSEC";
        case 48: return "DNSKEY";
        case 50: return "NSEC3";
        case 51: return "NSEC3PARAM";
        case 52: return "TLSA";
        case 53: return "SMIMEA";
        case 55: return "HIP";
        case 59: return "CDS";
        case 60: return "CDNSKEY";
        case 61: return "OPENPGPKEY";
        case 62: return "CSYNC";
        case 64: return "SVCB";
        case 65: return "HTTPS";
        case 99: return "SPF";
        case 249: return "TKEY";
        case 250: return "TSIG";
        case 251: return "IXFR";
        case 252: return "AXFR";
        case 255: return "ANY";
        case 256: return "URI";
        case 257: return "CAA";
        case 32768: return "TA";
        case 32769: return "DLV";
        default: return "UNKNOWN";
    }
}

/* DNS class name */
const char* xdns_class_name(long long cls) {
    switch (cls) {
        case 1: return "IN";
        case 3: return "CH";
        case 4: return "HS";
        case 254: return "NONE";
        case 255: return "ANY";
        default: return "UNKNOWN";
    }
}

/* DNS response code */
const char* xdns_rcode_name(long long rcode) {
    switch (rcode) {
        case 0: return "NOERROR";
        case 1: return "FORMERR";
        case 2: return "SERVFAIL";
        case 3: return "NXDOMAIN";
        case 4: return "NOTIMP";
        case 5: return "REFUSED";
        case 6: return "YXDOMAIN";
        case 7: return "YXRRSET";
        case 8: return "NXRRSET";
        case 9: return "NOTAUTH";
        case 10: return "NOTZONE";
        case 16: return "BADSIG";
        case 17: return "BADKEY";
        case 18: return "BADTIME";
        case 19: return "BADMODE";
        case 20: return "BADNAME";
        case 21: return "BADALG";
        case 22: return "BADTRUNC";
        case 23: return "BADCOOKIE";
        default: return "UNKNOWN";
    }
}

/* DNS opcode */
const char* xdns_opcode_name(long long opcode) {
    switch (opcode) {
        case 0: return "QUERY";
        case 1: return "IQUERY";
        case 2: return "STATUS";
        case 4: return "NOTIFY";
        case 5: return "UPDATE";
        case 6: return "DSO";
        default: return "UNKNOWN";
    }
}

/* Encode a domain name into DNS wire format (hex) */
const char* xdns_encode_name(const char *domain) {
    int pos = 0;
    const char *p = domain;
    while (*p) {
        const char *dot = strchr(p, '.');
        int label_len;
        if (dot) label_len = (int)(dot - p);
        else label_len = (int)strlen(p);
        if (label_len > 63) label_len = 63;
        pos += sprintf(buf + pos, "%02x", label_len);
        for (int i = 0; i < label_len; i++) {
            pos += sprintf(buf + pos, "%02x", (unsigned char)p[i]);
        }
        if (dot) p = dot + 1;
        else break;
    }
    pos += sprintf(buf + pos, "00");
    buf[pos] = '\0';
    return buf;
}

/* Validate domain name */
long long xdns_valid_name(const char *name) {
    int len = (int)strlen(name);
    if (len < 1 || len > 253) return 0;
    int label_len = 0;
    for (int i = 0; i < len; i++) {
        char c = name[i];
        if (c == '.') {
            if (label_len == 0) return 0;
            label_len = 0;
        } else {
            label_len++;
            if (label_len > 63) return 0;
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '-' || c == '_')) return 0;
        }
    }
    return 1;
}

/* Is IPv4 address */
long long xdns_is_ipv4(const char *addr) {
    int a, b, c, d;
    if (sscanf(addr, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) return 0;
    if (a < 0 || a > 255 || b < 0 || b > 255 || c < 0 || c > 255 || d < 0 || d > 255) return 0;
    return 1;
}
