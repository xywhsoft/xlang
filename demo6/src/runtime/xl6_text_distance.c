#include "src/runtime/xl6_text_internal.h"

int64_t xl6TextDistanceV1(xstrview left, xstrview right, int64_t limit, int64_t leftSize, int64_t rightSize)
{
    if (limit < -1 || (limit >= 0 && (uint64_t)limit > SIZE_MAX)) { xrtSetErrorKind(XERR_RANGE); return -1; }
    if (!xl6TextSelect(&left, leftSize) || !xl6TextSelect(&right, rightSize)) return -1;
    size_t result = xrtUtf8Distance(left, right, limit < 0 ? SIZE_MAX : (size_t)limit);
    return result == XRT_NPOS ? -1 : (int64_t)result;
}
double xl6TextSimilarityV1(xstrview left, xstrview right, int64_t leftSize, int64_t rightSize)
{
    if (!xl6TextSelect(&left, leftSize) || !xl6TextSelect(&right, rightSize)) return -1;
    return xrtUtf8Similarity(left, right);
}
bool xl6TextApproxV1(xstrview left, xstrview right, double tolerance, int64_t leftSize, int64_t rightSize)
{
    if (!(tolerance >= 0 && tolerance <= 1)) { xrtSetErrorKind(XERR_RANGE); return false; }
    double result = xl6TextSimilarityV1(left, right, leftSize, rightSize);
    return result >= 0 && result >= tolerance;
}
