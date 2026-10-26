/* Geohash 的位序以经度开头；经纬网格整数用于精确计算邻格。 */
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#ifndef XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_STRING_FORMAT
#endif
#ifndef XRT_MODULE_NUMBER_FLOAT
#define XRT_MODULE_NUMBER_FLOAT
#endif
#include "xrt.h"
#include "include/geohash_native.h"
#include <string.h>

#define XGEO_DOMAIN "xlang.extlib.geohash"

static const char XGEO_ALPHABET[] = "0123456789bcdefghjkmnpqrstuvwxyz";

typedef struct xgeo_cell {
    uint32 lon;
    uint32 lat;
    int lon_bits;
    int lat_bits;
    int precision;
} xgeo_cell;

static void xgeo_error(xerrkind kind, const char* message)
{
    xrtSetErrorInfo(kind, XGEO_DOMAIN, 0, message);
}

static int xgeo_digit(char value)
{
    const char* found = strchr(XGEO_ALPHABET, value);
    return found != NULL ? (int)(found - XGEO_ALPHABET) : -1;
}

static bool xgeo_parse(const char* hash, xgeo_cell* cell)
{
    int i, bit, position = 0;
    if (hash == NULL || hash[0] == '\0') {
        xgeo_error(XERR_VALUE, "geohash must contain 1..12 characters");
        return false;
    }
    memset(cell, 0, sizeof(*cell));
    for (i = 0; i < 12 && hash[i] != '\0'; ++i) {
        int digit = xgeo_digit(hash[i]);
        if (digit < 0) {
            xgeo_error(XERR_VALUE, "geohash contains an invalid character");
            return false;
        }
        for (bit = 4; bit >= 0; --bit, ++position) {
            if ((position & 1) == 0) {
                cell->lon = (cell->lon << 1) | (uint32)((digit >> bit) & 1);
                ++cell->lon_bits;
            } else {
                cell->lat = (cell->lat << 1) | (uint32)((digit >> bit) & 1);
                ++cell->lat_bits;
            }
        }
    }
    if (i == 12 && hash[i] != '\0') {
        xgeo_error(XERR_RANGE, "geohash exceeds 12 characters");
        return false;
    }
    cell->precision = i;
    return true;
}

/* 交错网格位，不经浮点数往返，跨经线邻格也不会跳格。 */
static void xgeo_cell_text(const xgeo_cell* cell, char output[13])
{
    int i, lon_at = 0, lat_at = 0, digit = 0;
    for (i = 0; i < cell->precision * 5; ++i) {
        uint32 bit;
        if ((i & 1) == 0) {
            bit = (cell->lon >> (cell->lon_bits - ++lon_at)) & 1u;
        } else {
            bit = (cell->lat >> (cell->lat_bits - ++lat_at)) & 1u;
        }
        digit = (digit << 1) | (int)bit;
        if (i % 5 == 4) {
            output[i / 5] = XGEO_ALPHABET[digit];
            digit = 0;
        }
    }
    output[cell->precision] = '\0';
}

char* xgeohash_encode(double lat, double lon, int precision)
{
    double lat_min = -90.0, lat_max = 90.0;
    double lon_min = -180.0, lon_max = 180.0;
    char output[13];
    int i, digit = 0;
    xrtClearError();
    if (!(lat >= -90.0 && lat <= 90.0) ||
        !(lon >= -180.0 && lon <= 180.0)) {
        xgeo_error(XERR_RANGE, "latitude or longitude is out of range");
        return NULL;
    }
    if (precision < 1 || precision > 12) {
        xgeo_error(XERR_RANGE, "precision must be between 1 and 12");
        return NULL;
    }
    for (i = 0; i < precision * 5; ++i) {
        double midpoint;
        int bit;
        if ((i & 1) == 0) {
            midpoint = (lon_min + lon_max) / 2.0;
            bit = lon >= midpoint;
            if (bit) lon_min = midpoint; else lon_max = midpoint;
        } else {
            midpoint = (lat_min + lat_max) / 2.0;
            bit = lat >= midpoint;
            if (bit) lat_min = midpoint; else lat_max = midpoint;
        }
        digit = (digit << 1) | bit;
        if (i % 5 == 4) {
            output[i / 5] = XGEO_ALPHABET[digit];
            digit = 0;
        }
    }
    output[precision] = '\0';
    return xrtStrDup(output);
}

int xgeohash_decode_bounds(const char* hash, xgeohash_bounds* output)
{
    xgeo_cell cell;
    xgeohash_bounds bounds;
    double lat_size, lon_size;
    xrtClearError();
    if (output == NULL) {
        xgeo_error(XERR_ARGUMENT, "bounds output is null");
        return 0;
    }
    if (!xgeo_parse(hash, &cell)) return 0;
    lat_size = 180.0 / (double)(1u << cell.lat_bits);
    lon_size = 360.0 / (double)(1u << cell.lon_bits);
    bounds.min_lat = -90.0 + (double)cell.lat * lat_size;
    bounds.min_lon = -180.0 + (double)cell.lon * lon_size;
    bounds.max_lat = bounds.min_lat + lat_size;
    bounds.max_lon = bounds.min_lon + lon_size;
    *output = bounds;
    return 1;
}

double xgeohash_decode_lat(const char* hash)
{
    xgeohash_bounds bounds;
    if (!xgeohash_decode_bounds(hash, &bounds)) return 0.0;
    return (bounds.min_lat + bounds.max_lat) / 2.0;
}

double xgeohash_decode_lon(const char* hash)
{
    xgeohash_bounds bounds;
    if (!xgeohash_decode_bounds(hash, &bounds)) return 0.0;
    return (bounds.min_lon + bounds.max_lon) / 2.0;
}

char* xgeohash_bbox(const char* hash)
{
    xgeohash_bounds bounds;
    char numbers[4][40];
    double values[4];
    size_t length, i;
    if (!xgeohash_decode_bounds(hash, &bounds)) return NULL;
    values[0] = bounds.min_lat;
    values[1] = bounds.min_lon;
    values[2] = bounds.max_lat;
    values[3] = bounds.max_lon;
    for (i = 0; i < 4; ++i)
        if (!xrtNumWrite(values[i], numbers[i], sizeof(numbers[i]),
            &length, XNUMBER_FLOAT_COMPACT)) return NULL;
    return xrtFormat("%s,%s,%s,%s", numbers[0], numbers[1],
        numbers[2], numbers[3]);
}

char* xgeohash_neighbor(const char* hash, int direction)
{
    xgeo_cell cell;
    uint32 lat_count, lon_count;
    char output[13];
    xrtClearError();
    if (!xgeo_parse(hash, &cell)) return NULL;
    lat_count = 1u << cell.lat_bits;
    lon_count = 1u << cell.lon_bits;
    switch (direction) {
    case 0:
        if (cell.lat + 1u == lat_count) goto pole;
        ++cell.lat;
        break;
    case 1:
        if (cell.lat == 0u) goto pole;
        --cell.lat;
        break;
    case 2: cell.lon = (cell.lon + 1u) % lon_count; break;
    case 3: cell.lon = (cell.lon + lon_count - 1u) % lon_count; break;
    default:
        xgeo_error(XERR_ARGUMENT, "direction must be north, south, east or west");
        return NULL;
    }
    xgeo_cell_text(&cell, output);
    return xrtStrDup(output);
pole:
    xgeo_error(XERR_RANGE, "no geohash neighbor beyond a pole");
    return NULL;
}

int xgeohash_precision(const char* hash)
{
    xgeo_cell cell;
    xrtClearError();
    return xgeo_parse(hash, &cell) ? cell.precision : -1;
}
