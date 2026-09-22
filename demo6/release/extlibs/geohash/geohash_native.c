/*
 * geohash_native.c - Geohash encode/decode/neighbor
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
static char geo_buf[4096];

static const char GEO_BASE32[] = "0123456789bcdefghjkmnpqrstuvwxyz";

static int geo_base32_index(char c) {
    const char *p = strchr(GEO_BASE32, c);
    if (!p) return -1;
    return (int)(p - GEO_BASE32);
}

/* encode lat/lon to geohash string with given precision (1-12) */
const char *xgeohash_encode(double lat, double lon, int precision) {
    double lat_min = -90.0, lat_max = 90.0;
    double lon_min = -180.0, lon_max = 180.0;
    int is_lon = 1;
    int bit = 0, ch = 0;
    int i = 0;

    if (precision < 1) precision = 1;
    if (precision > 12) precision = 12;

    while (i < precision) {
        if (is_lon) {
            double mid = (lon_min + lon_max) / 2.0;
            if (lon >= mid) { ch = ch * 2 + 1; lon_min = mid; }
            else { ch = ch * 2; lon_max = mid; }
        } else {
            double mid = (lat_min + lat_max) / 2.0;
            if (lat >= mid) { ch = ch * 2 + 1; lat_min = mid; }
            else { ch = ch * 2; lat_max = mid; }
        }
        is_lon = !is_lon;
        bit++;
        if (bit == 5) {
            geo_buf[i++] = GEO_BASE32[ch];
            bit = 0;
            ch = 0;
        }
    }
    geo_buf[i] = '\0';
    return geo_buf;
}

/* decode geohash -> center lat (returned as double via sprintf trick) */
static void geohash_decode(const char *hash, double *lat_out, double *lon_out,
                           double *lat_err, double *lon_err) {
    double lat_min = -90.0, lat_max = 90.0;
    double lon_min = -180.0, lon_max = 180.0;
    int is_lon = 1;
    const char *p = hash;

    while (*p) {
        int idx = geo_base32_index(*p);
        int b;
        if (idx < 0) { p++; continue; }
        for (b = 4; b >= 0; b--) {
            int bit = (idx >> b) & 1;
            if (is_lon) {
                double mid = (lon_min + lon_max) / 2.0;
                if (bit) lon_min = mid; else lon_max = mid;
            } else {
                double mid = (lat_min + lat_max) / 2.0;
                if (bit) lat_min = mid; else lat_max = mid;
            }
            is_lon = !is_lon;
        }
        p++;
    }
    *lat_out = (lat_min + lat_max) / 2.0;
    *lon_out = (lon_min + lon_max) / 2.0;
    *lat_err = (lat_max - lat_min) / 2.0;
    *lon_err = (lon_max - lon_min) / 2.0;
}

double xgeohash_decode_lat(const char *hash) {
    double lat, lon, lat_err, lon_err;
    if (!hash || !hash[0]) return 0.0;
    geohash_decode(hash, &lat, &lon, &lat_err, &lon_err);
    return lat;
}

double xgeohash_decode_lon(const char *hash) {
    double lat, lon, lat_err, lon_err;
    if (!hash || !hash[0]) return 0.0;
    geohash_decode(hash, &lat, &lon, &lat_err, &lon_err);
    return lon;
}

/* return bounding box as "min_lat,min_lon,max_lat,max_lon" */
const char *xgeohash_bbox(const char *hash) {
    double lat, lon, lat_err, lon_err;
    if (!hash || !hash[0]) return "";
    geohash_decode(hash, &lat, &lon, &lat_err, &lon_err);
    snprintf(geo_buf, sizeof(geo_buf), "%.8f,%.8f,%.8f,%.8f",
             lat - lat_err, lon - lon_err, lat + lat_err, lon + lon_err);
    return geo_buf;
}

/* neighbor: direction 0=N, 1=S, 2=E, 3=W */
const char *xgeohash_neighbor(const char *hash, int direction) {
    double lat, lon, lat_err, lon_err;
    int len;
    if (!hash || !hash[0]) return "";
    geohash_decode(hash, &lat, &lon, &lat_err, &lon_err);
    len = (int)strlen(hash);

    switch (direction) {
        case 0: lat += lat_err * 2.0; break;  /* N */
        case 1: lat -= lat_err * 2.0; break;  /* S */
        case 2: lon += lon_err * 2.0; break;  /* E */
        case 3: lon -= lon_err * 2.0; break;  /* W */
        default: return "";
    }
    /* clamp */
    if (lat > 90.0) lat = 90.0;
    if (lat < -90.0) lat = -90.0;
    if (lon > 180.0) lon -= 360.0;
    if (lon < -180.0) lon += 360.0;

    return xgeohash_encode(lat, lon, len);
}

int xgeohash_precision(const char *hash) {
    if (!hash) return 0;
    return (int)strlen(hash);
}
