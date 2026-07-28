/* s2_native.c - Simplified S2-like Cell ID encode/decode using Z-order curve */
#include <math.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_LEVEL 30

static char token_buf[32];

static double deg2rad(double d) { return d * M_PI / 180.0; }
static double rad2deg(double r) { return r * 180.0 / M_PI; }

/* Determine cube face from XYZ */
static int xyz_to_face(double x, double y, double z) {
    double ax = fabs(x), ay = fabs(y), az = fabs(z);
    if (ax > ay && ax > az) return x > 0 ? 0 : 3;
    if (ay > az) return y > 0 ? 1 : 4;
    return z > 0 ? 2 : 5;
}

/* Get UV on face from XYZ */
static void face_xyz_to_uv(int face, double x, double y, double z, double *u, double *v) {
    switch (face) {
        case 0: *u = y / x; *v = z / x; break;
        case 1: *u = -x / y; *v = z / y; break;
        case 2: *u = -x / z; *v = -y / z; break;
        case 3: *u = z / x; *v = y / x; break;
        case 4: *u = z / y; *v = -x / y; break;
        default: *u = -y / z; *v = -x / z; break;
    }
}

/* UV to XYZ for a face */
static void face_uv_to_xyz(int face, double u, double v, double *x, double *y, double *z) {
    switch (face) {
        case 0: *x = 1; *y = u; *z = v; break;
        case 1: *x = -u; *y = 1; *z = v; break;
        case 2: *x = -u; *y = -v; *z = 1; break;
        case 3: *x = -1; *y = -v; *z = u; break;
        case 4: *x = v; *y = -1; *z = u; break;
        default: *x = v; *y = u; *z = -1; break;
    }
}

/* Interleave bits of i and j (Morton/Z-order): result has i in even bits, j in odd bits */
static unsigned long long interleave_bits(unsigned int val, int nbits) {
    unsigned long long result = 0;
    int k;
    for (k = 0; k < nbits; k++) {
        if (val & (1u << k))
            result |= (1ULL << (2 * k));
    }
    return result;
}

/* De-interleave: extract even bits */
static unsigned int deinterleave_even(unsigned long long val, int nbits) {
    unsigned int result = 0;
    int k;
    for (k = 0; k < nbits; k++) {
        if (val & (1ULL << (2 * k)))
            result |= (1u << k);
    }
    return result;
}

/* De-interleave: extract odd bits */
static unsigned int deinterleave_odd(unsigned long long val, int nbits) {
    unsigned int result = 0;
    int k;
    for (k = 0; k < nbits; k++) {
        if (val & (1ULL << (2 * k + 1)))
            result |= (1u << k);
    }
    return result;
}

/*
 * Cell ID layout (64 bits):
 *   [63..61] face (3 bits)
 *   [60..1]  position bits (2*level bits used, rest zero)
 *   [0]      always 1 (marker)
 *
 * For a cell at level L:
 *   face << 61 | (pos << (61 - 2*L)) | (1 << (60 - 2*L))
 *
 * Simpler: we store as face*2^61 + pos*2 + 1, where pos is 2*L bits
 * padded to fill 60 bits total.
 */

unsigned long long xs2_from_latlng(double lat, double lng, int level) {
    if (level < 0) level = 0;
    if (level > MAX_LEVEL) level = MAX_LEVEL;

    double phi = deg2rad(lat);
    double theta = deg2rad(lng);
    double cosphi = cos(phi);
    double x = cosphi * cos(theta);
    double y = cosphi * sin(theta);
    double z = sin(phi);

    int face = xyz_to_face(x, y, z);
    double u, v;
    face_xyz_to_uv(face, x, y, z, &u, &v);

    /* UV [-1,1] -> [0,1] */
    double s = 0.5 * (u + 1.0);
    double t = 0.5 * (v + 1.0);

    /* Discrete IJ at given level */
    unsigned int max_ij = 1u << level;
    unsigned int i = (unsigned int)(s * max_ij);
    unsigned int j = (unsigned int)(t * max_ij);
    if (i >= max_ij) i = max_ij - 1;
    if (j >= max_ij) j = max_ij - 1;

    /* Interleave i (even bits) and j (odd bits) */
    unsigned long long pos = interleave_bits(i, level) | (interleave_bits(j, level) << 1);

    /* Build cell ID: face in top 3 bits, then pos in next 2*level bits, then marker */
    unsigned long long id = (unsigned long long)face << 61;
    id |= pos << (61 - 2 * level);
    id |= 1ULL << (60 - 2 * level);

    return id;
}

/* Get level from cell ID */
long long xs2_level(unsigned long long id) {
    if (id == 0) return -1;
    /* Find lowest set bit position */
    int shift = 0;
    unsigned long long tmp = id;
    while ((tmp & 1) == 0) { tmp >>= 1; shift++; }
    /* marker is at bit (60 - 2*level), so level = (60 - shift) / 2 */
    return (60 - shift) / 2;
}

/* Internal decode: get face, i, j, level from cell ID */
static void decode_cell(unsigned long long id, int *face, unsigned int *pi, unsigned int *pj, int *level) {
    *face = (int)(id >> 61);
    *level = (int)xs2_level(id);
    int lv = *level;

    /* Extract position bits: bits [60 .. 61-2*level] */
    unsigned long long pos_mask = ((1ULL << (2 * lv)) - 1);
    unsigned long long pos = (id >> (61 - 2 * lv)) & pos_mask;

    *pi = deinterleave_even(pos, lv);
    *pj = deinterleave_odd(pos, lv);
}

double xs2_to_lat(unsigned long long id) {
    if (id == 0) return 0.0;
    int face, level;
    unsigned int i, j;
    decode_cell(id, &face, &i, &j, &level);

    unsigned int max_ij = 1u << level;
    double s = ((double)i + 0.5) / max_ij;
    double t = ((double)j + 0.5) / max_ij;
    double u = 2.0 * s - 1.0;
    double v = 2.0 * t - 1.0;

    double x, y, z;
    face_uv_to_xyz(face, u, v, &x, &y, &z);
    double norm = sqrt(x*x + y*y + z*z);
    return rad2deg(asin(z / norm));
}

double xs2_to_lng(unsigned long long id) {
    if (id == 0) return 0.0;
    int face, level;
    unsigned int i, j;
    decode_cell(id, &face, &i, &j, &level);

    unsigned int max_ij = 1u << level;
    double s = ((double)i + 0.5) / max_ij;
    double t = ((double)j + 0.5) / max_ij;
    double u = 2.0 * s - 1.0;
    double v = 2.0 * t - 1.0;

    double x, y, z;
    face_uv_to_xyz(face, u, v, &x, &y, &z);
    double norm = sqrt(x*x + y*y + z*z);
    return rad2deg(atan2(y / norm, x / norm));
}

/* Get parent cell (one level up) */
unsigned long long xs2_parent(unsigned long long id) {
    long long level = xs2_level(id);
    if (level <= 0) return id;
    int new_level = (int)level - 1;
    int face = (int)(id >> 61);

    /* Get current i,j and truncate to parent level */
    unsigned int i, j;
    int lv;
    decode_cell(id, &face, &i, &j, &lv);
    i >>= 1;
    j >>= 1;

    unsigned long long pos = interleave_bits(i, new_level) | (interleave_bits(j, new_level) << 1);
    unsigned long long pid = (unsigned long long)face << 61;
    pid |= pos << (61 - 2 * new_level);
    pid |= 1ULL << (60 - 2 * new_level);
    return pid;
}

/* Token: hex representation */
const char *xs2_token(unsigned long long id) {
    if (id == 0) { strcpy(token_buf, "X"); return token_buf; }
    sprintf(token_buf, "%llx", id);
    int len = (int)strlen(token_buf);
    while (len > 1 && token_buf[len-1] == '0') len--;
    token_buf[len] = '\0';
    return token_buf;
}

/* Get face number */
long long xs2_face(unsigned long long id) {
    return (long long)(id >> 61);
}
