/* geos_native.c - Geometry operations (simplified GEOS subset) */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static char str_buf[4096];

/* Point-in-polygon (ray casting). Polygon as "x1,y1;x2,y2;..." */
long long xgeos_point_in_polygon(double px, double py, const char *poly) {
    double xs[128], ys[128]; int n = 0;
    const char *p = poly;
    while (*p && n < 128) {
        xs[n] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
        ys[n] = atof(p);
        while (*p && *p != ';') p++;
        if (*p == ';') p++;
        n++;
    }
    if (n < 3) return 0;
    int inside = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        if ((ys[i] > py) != (ys[j] > py) &&
            px < (xs[j] - xs[i]) * (py - ys[i]) / (ys[j] - ys[i]) + xs[i]) {
            inside = !inside;
        }
    }
    return inside ? 1 : 0;
}

/* Polygon area (shoelace formula). Polygon as "x1,y1;x2,y2;..." */
double xgeos_polygon_area(const char *poly) {
    double xs[128], ys[128]; int n = 0;
    const char *p = poly;
    while (*p && n < 128) {
        xs[n] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
        ys[n] = atof(p);
        while (*p && *p != ';') p++;
        if (*p == ';') p++;
        n++;
    }
    if (n < 3) return 0;
    double area = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        area += (xs[j] + xs[i]) * (ys[j] - ys[i]);
    }
    return fabs(area / 2.0);
}

/* Polygon centroid. Returns x, y via getter */
static double centroid_y;
double xgeos_centroid_x(const char *poly) {
    double xs[128], ys[128]; int n = 0;
    const char *p = poly;
    while (*p && n < 128) {
        xs[n] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
        ys[n] = atof(p);
        while (*p && *p != ';') p++;
        if (*p == ';') p++;
        n++;
    }
    if (n == 0) { centroid_y = 0; return 0; }
    double cx = 0, cy = 0, area = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        double cross = xs[j] * ys[i] - xs[i] * ys[j];
        area += cross;
        cx += (xs[j] + xs[i]) * cross;
        cy += (ys[j] + ys[i]) * cross;
    }
    area /= 2.0;
    if (fabs(area) < 1e-10) { centroid_y = ys[0]; return xs[0]; }
    cx /= (6.0 * area);
    cy /= (6.0 * area);
    centroid_y = cy;
    return cx;
}
double xgeos_centroid_y(void) { return centroid_y; }

/* Polygon perimeter */
double xgeos_perimeter(const char *poly) {
    double xs[128], ys[128]; int n = 0;
    const char *p = poly;
    while (*p && n < 128) {
        xs[n] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
        ys[n] = atof(p);
        while (*p && *p != ';') p++;
        if (*p == ';') p++;
        n++;
    }
    if (n < 2) return 0;
    double perim = 0;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        double dx = xs[j] - xs[i], dy = ys[j] - ys[i];
        perim += sqrt(dx * dx + dy * dy);
    }
    return perim;
}

/* Distance from point to line segment (x1,y1)-(x2,y2) */
double xgeos_point_segment_dist(double px, double py, double x1, double y1, double x2, double y2) {
    double dx = x2 - x1, dy = y2 - y1;
    double len2 = dx * dx + dy * dy;
    if (len2 < 1e-10) { double ddx = px - x1, ddy = py - y1; return sqrt(ddx*ddx + ddy*ddy); }
    double t = ((px - x1) * dx + (py - y1) * dy) / len2;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    double cx = x1 + t * dx, cy = y1 + t * dy;
    double ddx = px - cx, ddy = py - cy;
    return sqrt(ddx * ddx + ddy * ddy);
}

/* Bounding box of polygon: returns minx; miny/maxx/maxy via getters */
static double bbox_miny, bbox_maxx, bbox_maxy;
double xgeos_bbox_minx(const char *poly) {
    double xs[128], ys[128]; int n = 0;
    const char *p = poly;
    while (*p && n < 128) {
        xs[n] = atof(p);
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
        ys[n] = atof(p);
        while (*p && *p != ';') p++;
        if (*p == ';') p++;
        n++;
    }
    if (n == 0) { bbox_miny = bbox_maxx = bbox_maxy = 0; return 0; }
    double mnx = xs[0], mny = ys[0], mxx = xs[0], mxy = ys[0];
    for (int i = 1; i < n; i++) {
        if (xs[i] < mnx) mnx = xs[i];
        if (ys[i] < mny) mny = ys[i];
        if (xs[i] > mxx) mxx = xs[i];
        if (ys[i] > mxy) mxy = ys[i];
    }
    bbox_miny = mny; bbox_maxx = mxx; bbox_maxy = mxy;
    return mnx;
}
double xgeos_bbox_miny(void) { return bbox_miny; }
double xgeos_bbox_maxx(void) { return bbox_maxx; }
double xgeos_bbox_maxy(void) { return bbox_maxy; }

/* Check if two line segments intersect */
static double cross2d(double ox, double oy, double ax, double ay, double bx, double by) {
    return (ax - ox) * (by - oy) - (ay - oy) * (bx - ox);
}
long long xgeos_segments_intersect(double ax, double ay, double bx, double by,
                                    double cx, double cy, double dx, double dy) {
    double d1 = cross2d(cx, cy, dx, dy, ax, ay);
    double d2 = cross2d(cx, cy, dx, dy, bx, by);
    double d3 = cross2d(ax, ay, bx, by, cx, cy);
    double d4 = cross2d(ax, ay, bx, by, dx, dy);
    if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) &&
        ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0))) return 1;
    return 0;
}

/* Geometry type name */
const char* xgeos_type_name(int code) {
    switch (code) {
        case 0: return "Point";
        case 1: return "LineString";
        case 2: return "Polygon";
        case 3: return "MultiPoint";
        case 4: return "MultiLineString";
        case 5: return "MultiPolygon";
        case 6: return "GeometryCollection";
        default: return "Unknown";
    }
}
