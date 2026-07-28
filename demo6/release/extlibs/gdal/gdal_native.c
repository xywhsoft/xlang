/*
 * gdal_native.c - Geospatial Data Abstraction Library
 * Tier 4: geospatial
 */
#include <string.h>
#include <stdio.h>
#include <math.h>

static char buf[512];

const char* xgdal_driver(long long id) {
    switch (id) {
        case 0: return "GTiff";
        case 1: return "PNG";
        case 2: return "JPEG";
        case 3: return "GeoJSON";
        case 4: return "Shapefile";
        case 5: return "GPKG";
        case 6: return "KML";
        case 7: return "CSV";
        default: return "Unknown";
    }
}

const char* xgdal_datatype(long long id) {
    switch (id) {
        case 0: return "Byte";
        case 1: return "UInt16";
        case 2: return "Int16";
        case 3: return "UInt32";
        case 4: return "Int32";
        case 5: return "Float32";
        case 6: return "Float64";
        default: return "Unknown";
    }
}

const char* xgdal_projection(long long id) {
    switch (id) {
        case 0: return "EPSG:4326";
        case 1: return "EPSG:3857";
        case 2: return "EPSG:32633";
        case 3: return "EPSG:2154";
        case 4: return "EPSG:27700";
        default: return "Unknown";
    }
}

const char* xgdal_resampling(long long id) {
    switch (id) {
        case 0: return "nearest";
        case 1: return "bilinear";
        case 2: return "cubic";
        case 3: return "cubicspline";
        case 4: return "lanczos";
        case 5: return "average";
        case 6: return "mode";
        default: return "Unknown";
    }
}

/* Haversine distance in meters between two lat/lon points */
double xgdal_haversine(double lat1, double lon1, double lat2, double lon2) {
    double R = 6371000.0;
    double dlat = (lat2 - lat1) * 3.14159265358979 / 180.0;
    double dlon = (lon2 - lon1) * 3.14159265358979 / 180.0;
    double a = sin(dlat/2) * sin(dlat/2) +
               cos(lat1 * 3.14159265358979 / 180.0) * cos(lat2 * 3.14159265358979 / 180.0) *
               sin(dlon/2) * sin(dlon/2);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return R * c;
}

/* Convert WGS84 to Web Mercator (EPSG:3857) - returns "x,y" */
const char* xgdal_to_mercator(double lon, double lat) {
    double x = lon * 20037508.34 / 180.0;
    double y = log(tan((90.0 + lat) * 3.14159265358979 / 360.0)) / (3.14159265358979 / 180.0);
    y = y * 20037508.34 / 180.0;
    snprintf(buf, sizeof(buf), "%.2f,%.2f", x, y);
    return buf;
}

const char* xgdal_geometry_type(long long id) {
    switch (id) {
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
