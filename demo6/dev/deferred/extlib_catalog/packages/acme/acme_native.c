/* acme_native.c - ACME protocol helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char buf[8192];

/* Build ACME new-account request payload */
const char *xacme_new_account(const char *email, long long terms_agreed) {
    sprintf(buf, "{\"termsOfServiceAgreed\":%s,\"contact\":[\"mailto:%s\"]}",
            terms_agreed ? "true" : "false", email);
    return buf;
}

/* Build new-order request payload */
const char *xacme_new_order(const char *domain) {
    sprintf(buf, "{\"identifiers\":[{\"type\":\"dns\",\"value\":\"%s\"}]}", domain);
    return buf;
}

/* Build HTTP-01 challenge response (token.thumbprint) */
const char *xacme_http01_response(const char *token, const char *thumbprint) {
    sprintf(buf, "%s.%s", token, thumbprint);
    return buf;
}

/* Get challenge type name */
const char *xacme_challenge_type(const char *type) {
    if (strcmp(type, "http-01") == 0) return "HTTP Validation";
    if (strcmp(type, "dns-01") == 0) return "DNS Validation";
    if (strcmp(type, "tls-alpn-01") == 0) return "TLS-ALPN Validation";
    return "Unknown";
}

/* Build ACME directory URLs for known providers */
const char *xacme_directory_url(const char *provider) {
    if (strcmp(provider, "letsencrypt") == 0)
        return "https://acme-v02.api.letsencrypt.org/directory";
    if (strcmp(provider, "letsencrypt-staging") == 0)
        return "https://acme-staging-v02.api.letsencrypt.org/directory";
    if (strcmp(provider, "zerossl") == 0)
        return "https://acme.zerossl.com/v2/DV90";
    if (strcmp(provider, "buypass") == 0)
        return "https://api.buypass.com/acme/directory";
    return "";
}

/* Get ACME status name */
const char *xacme_status_name(const char *status) {
    if (strcmp(status, "pending") == 0) return "Pending";
    if (strcmp(status, "ready") == 0) return "Ready";
    if (strcmp(status, "processing") == 0) return "Processing";
    if (strcmp(status, "valid") == 0) return "Valid";
    if (strcmp(status, "invalid") == 0) return "Invalid";
    if (strcmp(status, "revoked") == 0) return "Revoked";
    if (strcmp(status, "expired") == 0) return "Expired";
    return "Unknown";
}

/* Build DNS-01 TXT record name for a domain */
const char *xacme_dns01_record_name(const char *domain) {
    sprintf(buf, "_acme-challenge.%s", domain);
    return buf;
}

/* Validate domain name (basic check) */
long long xacme_valid_domain(const char *domain) {
    int len = (int)strlen(domain);
    if (len < 3 || len > 253) return 0;
    int i;
    for (i = 0; i < len; i++) {
        char c = domain[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '-'))
            return 0;
    }
    /* Must contain at least one dot */
    if (!strchr(domain, '.')) return 0;
    return 1;
}

/* Build certificate key usage string */
const char *xacme_key_usage(long long digital_sig, long long key_encipherment) {
    buf[0] = '\0';
    if (digital_sig) strcat(buf, "digitalSignature");
    if (digital_sig && key_encipherment) strcat(buf, ",");
    if (key_encipherment) strcat(buf, "keyEncipherment");
    return buf;
}
