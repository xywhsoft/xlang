#define XRT_MODULE_STRING
#define XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_NUMBER_FLOAT
#include "xrt.h"
#include "../include/geohash_native.h"
#include <stdio.h>

int main(void)
{
    char* hash = (char*)xgeohash_encode(39.9042, 116.4074, 7);
    char* bounds;
    char* north;
    char* south;
    char* east;
    char* west;
    double lat, lon;
    int result = 1;
    if (hash == NULL) return 1;
    bounds = (char*)xgeohash_bbox(hash);
    north = (char*)xgeohash_neighbor(hash, 0);
    south = (char*)xgeohash_neighbor(hash, 1);
    east = (char*)xgeohash_neighbor(hash, 2);
    west = (char*)xgeohash_neighbor(hash, 3);
    if (bounds == NULL || north == NULL || south == NULL ||
        east == NULL || west == NULL) goto fail;
    lat = xgeohash_decode_lat(hash);
    lon = xgeohash_decode_lon(hash);
    printf("Beijing geohash: %s\n", hash);
    printf("precision: %d\n", xgeohash_precision(hash));
    printf("center near Beijing: %s\n",
        lat > 39.9 && lat < 39.91 && lon > 116.4 && lon < 116.41 ?
        "true" : "false");
    printf("bbox: %s\n", bounds);
    printf("north: %s\n", north);
    printf("south: %s\n", south);
    printf("east: %s\n", east);
    printf("west: %s\n", west);
    puts("geohash example: PASS");
    result = 0;
fail:
    xrtFree(west); xrtFree(east); xrtFree(south); xrtFree(north);
    xrtFree(bounds); xrtFree(hash);
    return result;
}

/* 单文件 TCC 运行包含实现；普通 C 构建仍单独链接。 */
#ifdef __TINYC__
#include "../geohash_native.c"
#endif
