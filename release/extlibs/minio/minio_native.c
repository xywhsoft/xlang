/* minio_native.c - S3/MinIO object storage helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static char str_buf[4096];

/* S3 HTTP method name */
const char* xminio_method_name(long long method) {
    switch (method) {
        case 0: return "GET";
        case 1: return "PUT";
        case 2: return "DELETE";
        case 3: return "HEAD";
        case 4: return "POST";
        default: return "UNKNOWN";
    }
}

/* S3 storage class name */
const char* xminio_storage_class(long long cls) {
    switch (cls) {
        case 0: return "STANDARD";
        case 1: return "REDUCED_REDUNDANCY";
        case 2: return "GLACIER";
        case 3: return "STANDARD_IA";
        case 4: return "ONEZONE_IA";
        case 5: return "INTELLIGENT_TIERING";
        case 6: return "DEEP_ARCHIVE";
        default: return "UNKNOWN";
    }
}

/* S3 ACL name */
const char* xminio_acl_name(long long acl) {
    switch (acl) {
        case 0: return "private";
        case 1: return "public-read";
        case 2: return "public-read-write";
        case 3: return "authenticated-read";
        case 4: return "bucket-owner-read";
        case 5: return "bucket-owner-full-control";
        default: return "unknown";
    }
}

/* Validate bucket name (S3 rules) */
long long xminio_valid_bucket(const char *name) {
    int len = (int)strlen(name);
    if (len < 3 || len > 63) return 0;
    if (name[0] == '.' || name[len-1] == '.') return 0;
    if (name[0] == '-' || name[len-1] == '-') return 0;
    for (int i = 0; i < len; i++) {
        char c = name[i];
        if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '.' && c != '-') return 0;
    }
    /* No consecutive dots */
    if (strstr(name, "..") != NULL) return 0;
    /* Not an IP address */
    int dots = 0;
    for (int i = 0; i < len; i++) if (name[i] == '.') dots++;
    if (dots == 3) {
        int all_digits = 1;
        for (int i = 0; i < len; i++) {
            if (name[i] != '.' && !(name[i] >= '0' && name[i] <= '9')) { all_digits = 0; break; }
        }
        if (all_digits) return 0;
    }
    return 1;
}

/* Validate object key */
long long xminio_valid_key(const char *key) {
    int len = (int)strlen(key);
    if (len == 0 || len > 1024) return 0;
    return 1;
}

/* Format S3 endpoint */
const char* xminio_endpoint(const char *host, int port, int tls) {
    if (port == 0) {
        snprintf(str_buf, sizeof(str_buf), "%s://%s", tls ? "https" : "http", host);
    } else {
        snprintf(str_buf, sizeof(str_buf), "%s://%s:%d", tls ? "https" : "http", host, port);
    }
    return str_buf;
}

/* Format S3 path-style URL */
const char* xminio_object_url(const char *endpoint, const char *bucket, const char *key) {
    snprintf(str_buf, sizeof(str_buf), "%s/%s/%s", endpoint, bucket, key);
    return str_buf;
}

/* S3 response code meaning */
const char* xminio_status_name(long long code) {
    switch (code) {
        case 200: return "OK";
        case 204: return "NoContent";
        case 206: return "PartialContent";
        case 301: return "MovedPermanently";
        case 304: return "NotModified";
        case 400: return "BadRequest";
        case 403: return "Forbidden";
        case 404: return "NotFound";
        case 409: return "Conflict";
        case 412: return "PreconditionFailed";
        case 416: return "RangeNotSatisfiable";
        case 500: return "InternalError";
        case 501: return "NotImplemented";
        case 503: return "ServiceUnavailable";
        default: return "Unknown";
    }
}

/* Content type from extension */
const char* xminio_content_type(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot) return "application/octet-stream";
    dot++;
    if (strcmp(dot, "txt") == 0) return "text/plain";
    if (strcmp(dot, "html") == 0) return "text/html";
    if (strcmp(dot, "css") == 0) return "text/css";
    if (strcmp(dot, "js") == 0) return "application/javascript";
    if (strcmp(dot, "json") == 0) return "application/json";
    if (strcmp(dot, "xml") == 0) return "application/xml";
    if (strcmp(dot, "png") == 0) return "image/png";
    if (strcmp(dot, "jpg") == 0 || strcmp(dot, "jpeg") == 0) return "image/jpeg";
    if (strcmp(dot, "gif") == 0) return "image/gif";
    if (strcmp(dot, "svg") == 0) return "image/svg+xml";
    if (strcmp(dot, "pdf") == 0) return "application/pdf";
    if (strcmp(dot, "zip") == 0) return "application/zip";
    if (strcmp(dot, "gz") == 0) return "application/gzip";
    if (strcmp(dot, "mp4") == 0) return "video/mp4";
    if (strcmp(dot, "mp3") == 0) return "audio/mpeg";
    if (strcmp(dot, "csv") == 0) return "text/csv";
    return "application/octet-stream";
}
