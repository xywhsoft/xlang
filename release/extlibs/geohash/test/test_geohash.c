#define XRT_MODULE_STRING
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/geohash_native.h"
#include <locale.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "geohash C test failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (0)

static int same(void* text, const char* expected)
{
    int equal = text != NULL && strcmp((const char*)text, expected) == 0;
    xrtFree(text);
    return equal;
}

/* 三条文本结果路径分别验证分配失败诊断和下一次调用恢复。 */
static void* allocated_result(int operation)
{
    if (operation == 0) return xgeohash_encode(42.6, -5.6, 5);
    if (operation == 1) return xgeohash_bbox("ezs42");
    return xgeohash_neighbor("ezs42", 2);
}

static int test_allocation_errors(void)
{
    int operation, failure;
    for (operation = 0; operation < 3; ++operation) {
        int failures = 0, successes = 0;
        for (failure = 0; failure < 16; ++failure) {
            void* text;
            int triggered;
            CHECK(xrtMemDebugFailAfter((uint64)failure));
            text = allocated_result(operation);
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                CHECK(text == NULL && xrtGetError() != NULL &&
                      xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                ++failures;
            } else {
                CHECK(text != NULL && xrtGetError() == NULL);
                ++successes;
            }
            xrtFree(text);
            text = allocated_result(operation);
            CHECK(text != NULL && xrtGetError() == NULL);
            xrtFree(text);
        }
        CHECK(failures > 0 && successes > 0);
    }
    return 0;
}

static int test_short_cells(void)
{
    static const char alphabet[] = "0123456789bcdefghjkmnpqrstuvwxyz";
    char hash[3] = {0};
    size_t first, second;
    int precision;
    for (precision = 1; precision <= 2; ++precision) {
        for (first = 0; first < 32; ++first) {
            for (second = 0; second < (precision == 1 ? 1u : 32u); ++second) {
                xgeohash_bounds bounds;
                char *east, *west, *north, *south;
                hash[0] = alphabet[first];
                hash[1] = precision == 1 ? '\0' : alphabet[second];
                hash[2] = '\0';
                CHECK(xgeohash_decode_bounds(hash, &bounds));
                CHECK(same(xgeohash_encode((bounds.min_lat + bounds.max_lat) / 2.0,
                    (bounds.min_lon + bounds.max_lon) / 2.0, precision), hash));
                east = (char *)xgeohash_neighbor(hash, 2);
                west = east ? (char *)xgeohash_neighbor(east, 3) : NULL;
                CHECK(east != NULL && west != NULL && strcmp(west, hash) == 0);
                xrtFree(west);
                xrtFree(east);
                north = (char *)xgeohash_neighbor(hash, 0);
                if (bounds.max_lat == 90.0) CHECK(north == NULL);
                else {
                    south = north ? (char *)xgeohash_neighbor(north, 1) : NULL;
                    CHECK(north != NULL && south != NULL && strcmp(south, hash) == 0);
                    xrtFree(south);
                }
                xrtFree(north);
            }
        }
    }
    return 0;
}

/* 固定种子覆盖不同纬度、经线和精度，检查跨 API 的格子不变量。 */
static int test_coordinate_grid(void)
{
    uint32_t seed = UINT32_C(0x61c88647);
    int sample, precision;
    for (sample = 0; sample < 512; ++sample) {
        double lat, lon;
        char previous[13] = {0};
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        lat = -90.0 + 180.0 * ((double)seed / 4294967295.0);
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        lon = -180.0 + 360.0 * ((double)seed / 4294967295.0);
        for (precision = 1; precision <= 12; ++precision) {
            xgeohash_bounds bounds;
            char *hash = (char *)xgeohash_encode(lat, lon, precision);
            char *center, *east, *west;
            CHECK(hash != NULL && xgeohash_decode_bounds(hash, &bounds));
            CHECK(bounds.min_lat <= lat && lat <= bounds.max_lat);
            CHECK(bounds.min_lon <= lon && lon <= bounds.max_lon);
            if (precision > 1) CHECK(strncmp(hash, previous, (size_t)precision - 1u) == 0);
            center = (char *)xgeohash_encode(
                (bounds.min_lat + bounds.max_lat) / 2.0,
                (bounds.min_lon + bounds.max_lon) / 2.0, precision);
            CHECK(center != NULL && strcmp(center, hash) == 0);
            xrtFree(center);
            east = (char *)xgeohash_neighbor(hash, 2);
            west = east ? (char *)xgeohash_neighbor(east, 3) : NULL;
            CHECK(east != NULL && west != NULL && strcmp(west, hash) == 0);
            xrtFree(west);
            xrtFree(east);
            memcpy(previous, hash, (size_t)precision + 1u);
            xrtFree(hash);
        }
    }
    return 0;
}

/* 坐标之间固定用逗号，小数点不能受进程区域设置影响。 */
static int test_bbox_locale(void)
{
    static const char *locales[] = {
        "French_France.1252", "fr-FR", "German_Germany.1252", "de-DE",
        "fr_FR.UTF-8", "de_DE.UTF-8"
    };
    const char *current = setlocale(LC_NUMERIC, NULL);
    char saved[128];
    size_t i;
    int tested = 0;
    if (current == NULL || strlen(current) >= sizeof(saved)) return 1;
    strcpy(saved, current);
    for (i = 0; i < sizeof(locales) / sizeof(locales[0]); ++i) {
        char *bbox;
        size_t commas = 0;
        const char *p;
        if (setlocale(LC_NUMERIC, locales[i]) == NULL ||
            strcmp(localeconv()->decimal_point, ",") != 0) continue;
        bbox = (char *)xgeohash_bbox("ezs42");
        if (bbox == NULL) { setlocale(LC_NUMERIC, saved); return 1; }
        for (p = bbox; *p; ++p) if (*p == ',') ++commas;
        tested = commas == 3u && strchr(bbox, '.') != NULL;
        xrtFree(bbox);
        break;
    }
    setlocale(LC_NUMERIC, saved);
    return tested || i == sizeof(locales) / sizeof(locales[0]);
}

int main(void)
{
    xgeohash_bounds bounds;
    xgeohash_bounds old_bounds;
    char* held;
    char* east;
    char* back;
    char* hash;
    int precision, i;

    CHECK(same(xgeohash_encode(0.0, 0.0, 1), "s"));
    CHECK(same(xgeohash_encode(42.6, -5.6, 5), "ezs42"));
    CHECK(same(xgeohash_bbox("s"), "0,0,45,45"));
    CHECK(xgeohash_decode_bounds("s", &bounds) == 1);
    CHECK(bounds.min_lat == 0.0 && bounds.max_lat == 45.0);
    CHECK(bounds.min_lon == 0.0 && bounds.max_lon == 45.0);
    CHECK(xgeohash_decode_lat("s") == 22.5);
    CHECK(xgeohash_decode_lon("s") == 22.5);
    CHECK(xgeohash_precision("s") == 1);

    /* 失败时不污染调用方的输出结构。 */
    old_bounds = bounds;
    CHECK(xgeohash_decode_bounds("s!", &bounds) == 0);
    CHECK(memcmp(&bounds, &old_bounds, sizeof(bounds)) == 0);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xgeohash_precision("s!") == -1);
    CHECK(xgeohash_precision("S") == -1);
    CHECK(xgeohash_precision("0123456789bcd") == -1);
    CHECK(xgeohash_encode(91.0, 0.0, 6) == NULL);
    CHECK(xgeohash_encode(0.0, 181.0, 6) == NULL);
    CHECK(xgeohash_encode(0.0, 0.0, 0) == NULL);
    CHECK(xgeohash_encode(0.0, 0.0, 13) == NULL);
    CHECK(xgeohash_encode(NAN, 0.0, 6) == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    CHECK(xgeohash_encode(0.0, INFINITY, 6) == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    CHECK(xgeohash_encode(-INFINITY, 0.0, 6) == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    CHECK(xgeohash_decode_bounds("s", NULL) == 0);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    CHECK(xgeohash_decode_bounds(NULL, &bounds) == 0);
    CHECK(memcmp(&bounds, &old_bounds, sizeof(bounds)) == 0);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xgeohash_neighbor("s", 4) == NULL);

    /* 成功调用不能继承上一次失败留下的线程错误。 */
    CHECK(same(xgeohash_encode(0.0, 0.0, 1), "s"));
    CHECK(xrtGetError() == NULL);
    CHECK(xgeohash_precision("!") == -1);
    CHECK(xgeohash_precision("s") == 1);
    CHECK(xrtGetError() == NULL);
    CHECK(xgeohash_decode_bounds("!", &bounds) == 0);
    CHECK(xgeohash_decode_bounds("s", &bounds) == 1);
    CHECK(xrtGetError() == NULL);
    CHECK(xgeohash_neighbor("s", 4) == NULL);
    CHECK(same(xgeohash_neighbor("s", 2), "t"));
    CHECK(xrtGetError() == NULL);

    /* 经度环绕，纬度到极点为止，不复制原单元格。 */
    hash = (char*)xgeohash_encode(0.0, 180.0, 6);
    CHECK(hash != NULL);
    east = (char*)xgeohash_neighbor(hash, 2);
    CHECK(east != NULL && xgeohash_decode_lon(east) < 0.0);
    back = (char*)xgeohash_neighbor(east, 3);
    CHECK(back != NULL && strcmp(back, hash) == 0);
    xrtFree(back);
    xrtFree(east);
    xrtFree(hash);
    hash = (char*)xgeohash_encode(90.0, 0.0, 6);
    CHECK(hash != NULL && xgeohash_neighbor(hash, 0) == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtFree(hash);

    /* 最高精度下仍应保留经线环绕和极点终止。 */
    hash = (char*)xgeohash_encode(90.0, 180.0, 12);
    CHECK(hash != NULL && xgeohash_neighbor(hash, 0) == NULL);
    east = (char*)xgeohash_neighbor(hash, 2);
    back = east ? (char*)xgeohash_neighbor(east, 3) : NULL;
    CHECK(east != NULL && back != NULL && strcmp(back, hash) == 0);
    xrtFree(back);
    xrtFree(east);
    xrtFree(hash);

    for (precision = 1; precision <= 12; ++precision) {
        hash = (char*)xgeohash_encode(39.9042, 116.4074, precision);
        CHECK(hash != NULL && xgeohash_precision(hash) == precision);
        CHECK(xgeohash_decode_bounds(hash, &bounds) == 1);
        CHECK(bounds.min_lat <= 39.9042 && 39.9042 <= bounds.max_lat);
        CHECK(bounds.min_lon <= 116.4074 && 116.4074 <= bounds.max_lon);
        east = (char*)xgeohash_neighbor(hash, 2);
        CHECK(east != NULL);
        back = (char*)xgeohash_neighbor(east, 3);
        CHECK(back != NULL && strcmp(back, hash) == 0);
        xrtFree(back);
        xrtFree(east);
        xrtFree(hash);
    }

    held = (char*)xgeohash_encode(42.6, -5.6, 5);
    CHECK(held != NULL);
    for (i = 0; i < 128; ++i) {
        char* other = (char*)xgeohash_encode(39.9, 116.4, 8);
        CHECK(other != NULL && strcmp(held, "ezs42") == 0);
        xrtFree(other);
    }
    xrtFree(held);
    if (test_short_cells()) return 1;
    if (test_coordinate_grid()) return 1;
    CHECK(test_bbox_locale());
    if (test_allocation_errors()) return 1;
    puts("geohash C boundaries: PASS");
    return 0;
}
