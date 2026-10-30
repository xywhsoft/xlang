#ifndef XL_GEOHASH_NATIVE_H
#define XL_GEOHASH_NATIVE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xgeohash_bounds {
    double min_lat;
    double min_lon;
    double max_lat;
    double max_lon;
} xgeohash_bounds;

/* 文本由 XRT 独立分配，调用方用 xrtFree；NULL 表示失败并伴随 XRT 错误。 */
char* xgeohash_encode(double lat, double lon, int precision);
char* xgeohash_bbox(const char* hash);
/* direction：0 北、1 南、2 东、3 西；极点方向失败，经度方向环绕。 */
char* xgeohash_neighbor(const char* hash, int direction);

/* 成功为 1，失败为 0；失败不修改 output。 */
int xgeohash_decode_bounds(const char* hash, xgeohash_bounds* output);
double xgeohash_decode_lat(const char* hash);
double xgeohash_decode_lon(const char* hash);
/* 严格校验整个 hash；失败返回 -1。 */
int xgeohash_precision(const char* hash);

#ifdef __cplusplus
}
#endif

#endif
