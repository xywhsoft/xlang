#define XRT_MODULE_RANDOM_TEXT_DEFAULT
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* 输出与 xlang 示例一致的检查结果，便于发布门禁直接比较。 */
static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}



/* 安全比较可能为空的拥有型字符串。 */
static int text_eq(const char* text, const char* expected)
{
    return (text != NULL) && (expected != NULL) &&
        (strcmp(text, expected) == 0);
}



/* 演示默认线程状态和调用方持有状态的随机数契约。 */
int main(void)
{
    xrng rngA;
    xrng rngB;
    uint32 seed32 = xrtRand32();
    uint64 seed64 = xrtRand64();
    int64 dice = xrtRandRangeClosed(1, 6);
    str token = xrtRandString(16);
    str fixedText = xrtRandStringFrom(XRT_STR_LITERAL("x"), 4);
    uint32 replayA;
    uint32 replayB;
    uint32 obj32A;
    uint32 obj32B;
    uint64 obj64A;
    uint64 obj64B;
    int64 diceA;
    int64 diceB;
    str objTokenA;
    str objTokenB;
    str objFixed;

    /* 当前线程状态允许使用相同 seed 和 stream 精确重放。 */
    xrtRandSeed(2026, 1);
    replayA = xrtRand32();
    xrtRandSeed(2026, 1);
    replayB = xrtRand32();

    /* 显式状态无分配、由调用方持有，并保持跨平台确定性。 */
    xrtRngSeed(&rngA, 12345, 7);
    xrtRngSeed(&rngB, 12345, 7);
    obj32A = xrtRng32(&rngA);
    obj32B = xrtRng32(&rngB);
    obj64A = xrtRng64(&rngA);
    obj64B = xrtRng64(&rngB);
    diceA = xrtRngRangeClosed(&rngA, 1, 6);
    diceB = xrtRngRangeClosed(&rngB, 1, 6);

    /* 文本生成沿用同一显式状态，也可通过单字符字母表验证边界。 */
    xrtRngSeed(&rngA, 2026, 1);
    xrtRngSeed(&rngB, 2026, 1);
    objTokenA = xrtRngStringFrom(
        &rngA,
        XRT_STR_LITERAL("ab"),
        10
    );
    objTokenB = xrtRngStringFrom(
        &rngB,
        XRT_STR_LITERAL("ab"),
        10
    );
    objFixed = xrtRngStringFrom(
        &rngA,
        XRT_STR_LITERAL("z"),
        5
    );

    require_line(
        "global",
        ((seed32 != 0) || (seed64 != 0)) &&
        (dice >= 1) && (dice <= 6) &&
        (token != NULL) && (strlen(token) == 16) &&
        text_eq(fixedText, "xxxx")
    );

    require_line("seed_replay", replayA == replayB);

    require_line(
        "object_replay",
        (obj32A == obj32B) &&
        (obj64A == obj64B) &&
        (diceA == diceB) &&
        (diceA >= 1) && (diceA <= 6) &&
        text_eq(objTokenA, objTokenB) &&
        (objTokenA != NULL) && (strlen(objTokenA) == 10) &&
        text_eq(objFixed, "zzzzz")
    );

    xrtFree(token);
    xrtFree(fixedText);
    xrtFree(objTokenA);
    xrtFree(objTokenB);
    xrtFree(objFixed);
    return 0;
}
