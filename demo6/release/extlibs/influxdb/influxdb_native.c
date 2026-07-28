/* influxdb_native.c - InfluxDB line protocol helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char hex_buf[8192];
static char str_buf[4096];

/* Build line protocol: measurement,tag1=v1,tag2=v2 field1=v1,field2=v2 timestamp */
const char *xinfluxdb_line(const char *measurement, const char *tags, const char *fields, long long timestamp) {
    if (timestamp > 0) {
        sprintf(str_buf, "%s,%s %s %lld", measurement, tags, fields, timestamp);
    } else {
        sprintf(str_buf, "%s,%s %s", measurement, tags, fields);
    }
    return str_buf;
}

/* Build line without tags */
const char *xinfluxdb_line_no_tags(const char *measurement, const char *fields, long long timestamp) {
    if (timestamp > 0) {
        sprintf(str_buf, "%s %s %lld", measurement, fields, timestamp);
    } else {
        sprintf(str_buf, "%s %s", measurement, fields);
    }
    return str_buf;
}

/* Format field value as string */
const char *xinfluxdb_field_str(const char *key, const char *value) {
    sprintf(str_buf, "%s=\"%s\"", key, value);
    return str_buf;
}

/* Format field value as float */
const char *xinfluxdb_field_float(const char *key, double value) {
    sprintf(str_buf, "%s=%f", key, value);
    return str_buf;
}

/* Format field value as integer */
const char *xinfluxdb_field_int(const char *key, long long value) {
    sprintf(str_buf, "%s=%lldi", key, value);
    return str_buf;
}

/* Format field value as boolean */
const char *xinfluxdb_field_bool(const char *key, long long value) {
    sprintf(str_buf, "%s=%s", key, value ? "true" : "false");
    return str_buf;
}

/* Escape measurement name (spaces and commas) */
const char *xinfluxdb_escape_measurement(const char *name) {
    int pos = 0, i, len = (int)strlen(name);
    for (i = 0; i < len && pos < 4090; i++) {
        if (name[i] == ',' || name[i] == ' ') {
            str_buf[pos++] = '\\';
        }
        str_buf[pos++] = name[i];
    }
    str_buf[pos] = '\0';
    return str_buf;
}

/* Escape tag key/value (spaces, commas, equals) */
const char *xinfluxdb_escape_tag(const char *s) {
    int pos = 0, i, len = (int)strlen(s);
    for (i = 0; i < len && pos < 4090; i++) {
        if (s[i] == ',' || s[i] == ' ' || s[i] == '=') {
            str_buf[pos++] = '\\';
        }
        str_buf[pos++] = s[i];
    }
    str_buf[pos] = '\0';
    return str_buf;
}

/* Validate measurement name */
long long xinfluxdb_valid_measurement(const char *name) {
    int len = (int)strlen(name);
    if (len < 1 || len > 255) return 0;
    return 1;
}

/* Validate tag key */
long long xinfluxdb_valid_tag_key(const char *key) {
    int len = (int)strlen(key);
    if (len < 1 || len > 255) return 0;
    if (strchr(key, ' ') || strchr(key, ',') || strchr(key, '=')) return 0;
    return 1;
}

/* Default HTTP port */
long long xinfluxdb_http_port(void) { return 8086; }

/* Default write endpoint */
const char *xinfluxdb_write_endpoint(const char *db) {
    sprintf(str_buf, "/api/v2/write?org=my-org&bucket=%s&precision=ns", db);
    return str_buf;
}

/* Default query endpoint */
const char *xinfluxdb_query_endpoint(void) {
    return "/api/v2/query?org=my-org";
}

/* Precision names */
const char *xinfluxdb_precision_name(long long prec) {
    switch(prec) {
        case 0: return "ns";
        case 1: return "us";
        case 2: return "ms";
        case 3: return "s";
        default: return "UNKNOWN";
    }
}
