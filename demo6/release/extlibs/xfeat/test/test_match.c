#if !defined(_WIN32) && !defined(_WIN64) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <string.h>

/* 不依赖模型资产，直接验证匹配与仿射估计核心。 */
#include "../src/xfeat_artifact.c"

static int require(int condition, const char* what)
{
    if (condition) return 0;
    fprintf(stderr, "xfeat: %s\n", what);
    return 1;
}

int main(void)
{
    float query_xy[8] = {10, 10, 80, 10, 10, 80, 80, 80};
    float target_xy[8] = {30, 40, 100, 40, 30, 110, 100, 110};
    float query_desc[4 * XFEAT_DESC_DIM] = {0};
    float target_desc[4 * XFEAT_DESC_DIM] = {0};
    XFeatFeatures query = {0};
    XFeatFeatures target = {0};
    XFeatLocation* location;
    char* json;
    int i;

    for (i = 0; i < 4; ++i) {
        query_desc[i * XFEAT_DESC_DIM + i] = 1.0f;
        target_desc[i * XFEAT_DESC_DIM + i] = 1.0f;
    }
    query.count = target.count = 4;
    query.image_w = query.image_h = 100;
    target.image_w = target.image_h = 300;
    query.xy = query_xy;
    target.xy = target_xy;
    query.desc = query_desc;
    target.desc = target_desc;

    location = (XFeatLocation*)xfeat_match_native(&query, &target, 0.8, 3, 2.0, 1.0);
    if (require(location != NULL && location->found, "translated features should match")) return 1;
    if (require(location->matches == 4 && location->inliers == 4, "match/inlier count")) return 1;
    if (require(location->bbox[0] == 20 && location->bbox[1] == 30 &&
                location->bbox[2] == 99 && location->bbox[3] == 99, "translated bounding box")) return 1;
    json = xfeat_location_json_native(location, "target.png", "result.png");
    if (require(json != NULL && strstr(json, "\"found\":true") != NULL &&
                xrtJsonValid(xrtStrView(json)), "location JSON")) return 1;
    xrtFree(json);
    xfeat_location_release_native(location);

    location = (XFeatLocation*)xfeat_match_native(&query, &target, 1.1, 3, 2.0, 1.0);
    if (require(location != NULL && !location->found && location->matches == 0,
                "high similarity threshold should reject all matches")) return 1;
    xfeat_location_release_native(location);
    if (require(xfeat_match_native(&query, &target, 0.8, 0, 2.0, 1.0) == NULL &&
                strcmp(xfeat_last_error_native(), "invalid XFeat match parameters") == 0,
                "minimum inliers must be at least three")) return 1;
    if (require(xfeat_match_native(&query, &target, 0.8, 3, 0.0, 1.0) == NULL,
                "reprojection threshold must be positive")) return 1;
    puts("xfeat matching core: PASS");
    return 0;
}
