/* proj_native.c - Coordinate projection helpers (simplified PROJ subset) */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static char str_buf[4096];

#define PI 3.14159265358979323846
#define DEG2RAD (PI / 180.0)
#define RAD2DEG (180.0 / PI)
#define WGS84_A 6378137.0
#define WGS84_F (1.0 / 298.257223563)
#define WGS84_E2 (2.0 * WGS84_F - WGS84_F * WGS84_F)

/* Mercator projection: lon/lat (degrees) -> x/y (meters) */
static double merc_y;
double xproj_mercator_x(double lon, double lat) {
    (void)lat;
    return WGS84_A * lon * DEG2RAD;
}
double xproj_mercator_y(double lon, double lat) {
    (void)lon;
    double lat_rad = lat * DEG2RAD;
    return WGS84_A * log(tan(PI / 4.0 + lat_rad / 2.0));
}

/* Inverse Mercator: x/y (meters) -> lon/lat (degrees) */
static double invmerc_lat;
double xproj_invmercator_lon(double x, double y) {
    (void)y;
    return (x / WGS84_A) * RAD2DEG;
}
double xproj_invmercator_lat(double x, double y) {
    (void)x;
    return (2.0 * atan(exp(y / WGS84_A)) - PI / 2.0) * RAD2DEG;
}

/* UTM zone from longitude */
long long xproj_utm_zone(double lon) {
    return (long long)((lon + 180.0) / 6.0) + 1;
}

/* UTM forward (simplified, spherical approx) - returns easting, northing via getter */
static double utm_northing;
double xproj_utm_easting(double lon, double lat) {
    long long zone = (long long)((lon + 180.0) / 6.0) + 1;
    double lon0 = ((zone - 1) * 6.0 - 180.0 + 3.0) * DEG2RAD;
    double lat_rad = lat * DEG2RAD;
    double lon_rad = lon * DEG2RAD;
    double N = WGS84_A / sqrt(1.0 - WGS84_E2 * sin(lat_rad) * sin(lat_rad));
    double T = tan(lat_rad) * tan(lat_rad);
    double C = (WGS84_E2 / (1.0 - WGS84_E2)) * cos(lat_rad) * cos(lat_rad);
    double A = cos(lat_rad) * (lon_rad - lon0);
    double M = WGS84_A * ((1.0 - WGS84_E2/4.0 - 3.0*WGS84_E2*WGS84_E2/64.0) * lat_rad
               - (3.0*WGS84_E2/8.0 + 3.0*WGS84_E2*WGS84_E2/32.0) * sin(2.0*lat_rad)
               + (15.0*WGS84_E2*WGS84_E2/256.0) * sin(4.0*lat_rad));
    double easting = 0.9996 * N * (A + (1.0-T+C)*A*A*A/6.0) + 500000.0;
    utm_northing = 0.9996 * (M + N * tan(lat_rad) * (A*A/2.0 + (5.0-T+9.0*C+4.0*C*C)*A*A*A*A/24.0));
    return easting;
}
double xproj_utm_northing(void) { return utm_northing; }

/* Haversine distance (meters) between two lon/lat points */
double xproj_haversine(double lon1, double lat1, double lon2, double lat2) {
    double dlat = (lat2 - lat1) * DEG2RAD;
    double dlon = (lon2 - lon1) * DEG2RAD;
    double a = sin(dlat/2.0) * sin(dlat/2.0) +
               cos(lat1 * DEG2RAD) * cos(lat2 * DEG2RAD) *
               sin(dlon/2.0) * sin(dlon/2.0);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return WGS84_A * c;
}

/* Bearing (degrees) from point1 to point2 */
double xproj_bearing(double lon1, double lat1, double lon2, double lat2) {
    double dlon = (lon2 - lon1) * DEG2RAD;
    double la1 = lat1 * DEG2RAD, la2 = lat2 * DEG2RAD;
    double y = sin(dlon) * cos(la2);
    double x = cos(la1) * sin(la2) - sin(la1) * cos(la2) * cos(dlon);
    double brng = atan2(y, x) * RAD2DEG;
    if (brng < 0) brng += 360.0;
    return brng;
}

/* EPSG code lookup: returns name for common CRS */
const char* xproj_crs_name(long long epsg) {
    switch (epsg) {
        case 4326: return "WGS 84";
        case 3857: return "WGS 84 / Pseudo-Mercator";
        case 32601: return "WGS 84 / UTM zone 1N";
        case 2154: return "RGF93 / Lambert-93";
        case 4269: return "NAD83";
        case 27700: return "OSGB 1936 / British National Grid";
        default: return "Unknown";
    }
}

/* Check if EPSG is projected (vs geographic) */
long long xproj_is_projected(long long epsg) {
    if (epsg == 4326 || epsg == 4269) return 0;
    return 1;
}

/* Convert degrees to DMS string */
const char* xproj_to_dms(double degrees, int is_lon) {
    char hemi;
    if (is_lon) hemi = degrees >= 0 ? 'E' : 'W';
    else hemi = degrees >= 0 ? 'N' : 'S';
    double abs_deg = fabs(degrees);
    int d = (int)abs_deg;
    double min_f = (abs_deg - d) * 60.0;
    int m = (int)min_f;
    double s = (min_f - m) * 60.0;
    snprintf(str_buf, sizeof(str_buf), "%d %d' %.2f\" %c", d, m, s, hemi);
    return str_buf;
}

/* Web Mercator tile coordinates from lon/lat at zoom level */
static long long tile_y;
long long xproj_tile_x(double lon, double lat, int zoom) {
    (void)lat;
    long long n = 1LL << zoom;
    long long tx = (long long)((lon + 180.0) / 360.0 * n);
    if (tx < 0) tx = 0;
    if (tx >= n) tx = n - 1;
    double lat_rad = lat * DEG2RAD;
    long long ty = (long long)((1.0 - log(tan(lat_rad) + 1.0/cos(lat_rad)) / PI) / 2.0 * n);
    if (ty < 0) ty = 0;
    if (ty >= n) ty = n - 1;
    tile_y = ty;
    return tx;
}
long long xproj_tile_y(void) { return tile_y; }
