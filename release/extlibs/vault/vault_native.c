/* vault_native.c - HashiCorp Vault KV helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char buf[8192];

/* Build KV v2 read path */
const char *xvault_kv2_read_path(const char *mount, const char *path) {
    sprintf(buf, "/v1/%s/data/%s", mount, path);
    return buf;
}

/* Build KV v2 write path */
const char *xvault_kv2_write_path(const char *mount, const char *path) {
    sprintf(buf, "/v1/%s/data/%s", mount, path);
    return buf;
}

/* Build KV v1 read path */
const char *xvault_kv1_read_path(const char *mount, const char *path) {
    sprintf(buf, "/v1/%s/%s", mount, path);
    return buf;
}

/* Build KV v2 write request body */
const char *xvault_kv2_write_body(const char *json_data) {
    sprintf(buf, "{\"data\":%s}", json_data);
    return buf;
}

/* Build KV v2 delete path */
const char *xvault_kv2_delete_path(const char *mount, const char *path) {
    sprintf(buf, "/v1/%s/data/%s", mount, path);
    return buf;
}

/* Build metadata path */
const char *xvault_kv2_metadata_path(const char *mount, const char *path) {
    sprintf(buf, "/v1/%s/metadata/%s", mount, path);
    return buf;
}

/* Build auth token request body */
const char *xvault_token_create_body(const char *policies, long long ttl_seconds) {
    sprintf(buf, "{\"policies\":[\"%s\"],\"ttl\":\"%llds\"}", policies, ttl_seconds);
    return buf;
}

/* Build AppRole login body */
const char *xvault_approle_login(const char *role_id, const char *secret_id) {
    sprintf(buf, "{\"role_id\":\"%s\",\"secret_id\":\"%s\"}", role_id, secret_id);
    return buf;
}

/* Build userpass login path */
const char *xvault_userpass_login_path(const char *username) {
    sprintf(buf, "/v1/auth/userpass/login/%s", username);
    return buf;
}

/* Build userpass login body */
const char *xvault_userpass_login_body(const char *password) {
    sprintf(buf, "{\"password\":\"%s\"}", password);
    return buf;
}

/* Validate mount path (no leading/trailing slash, alphanumeric + dash/underscore) */
long long xvault_valid_mount(const char *mount) {
    int len = (int)strlen(mount);
    if (len < 1 || len > 128) return 0;
    if (mount[0] == '/' || mount[len-1] == '/') return 0;
    int i;
    for (i = 0; i < len; i++) {
        char c = mount[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '/'))
            return 0;
    }
    return 1;
}

/* Get auth method name */
const char *xvault_auth_method_name(const char *method) {
    if (strcmp(method, "token") == 0) return "Token";
    if (strcmp(method, "approle") == 0) return "AppRole";
    if (strcmp(method, "userpass") == 0) return "Username & Password";
    if (strcmp(method, "kubernetes") == 0) return "Kubernetes";
    if (strcmp(method, "ldap") == 0) return "LDAP";
    if (strcmp(method, "github") == 0) return "GitHub";
    if (strcmp(method, "aws") == 0) return "AWS IAM";
    return "Unknown";
}

/* Build health check path */
const char *xvault_health_path(void) {
    return "/v1/sys/health";
}

/* Build seal status path */
const char *xvault_seal_status_path(void) {
    return "/v1/sys/seal-status";
}
