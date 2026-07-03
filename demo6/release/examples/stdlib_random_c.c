#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>

static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}

int main(void)
{
    xrtInit();

    uint32 seed32 = xrtRand32();
    uint64 seed64 = xrtRand64();
    int dice = xrtRandRange(1, 6);
    str token = xrtRandStr(NULL, 0, 16);
    str fixedText = xrtRandStr("x", 0, 4);

    xrtRandSeedThread(2026, 1);
    uint32 replayA = xrtRand32();
    xrtRandSeedThread(2026, 1);
    uint32 replayB = xrtRand32();

    ptr rngA = xrtRandCreate(12345, 7);
    ptr rngB = xrtRandCreate(12345, 7);
    uint32 obj32A = xrtRand32Obj(rngA);
    uint32 obj32B = xrtRand32Obj(rngB);
    uint64 obj64A = xrtRand64Obj(rngA);
    uint64 obj64B = xrtRand64Obj(rngB);
    int diceA = xrtRandRangeObj(rngA, 1, 6);
    int diceB = xrtRandRangeObj(rngB, 1, 6);

    xrtRandSeedObj(rngA, 2026, 1);
    xrtRandSeedObj(rngB, 2026, 1);
    str objTokenA = xrtRandStrObj(rngA, "ab", 0, 10);
    str objTokenB = xrtRandStrObj(rngB, "ab", 0, 10);
    str objFixed = xrtRandStrObj(rngA, "z", 0, 5);

    require_line("global",
        seed32 >= 0 &&
        seed64 >= 0 &&
        dice >= 1 &&
        dice <= 6 &&
        xrtStrByteLen(token, 0) == 16 &&
        xrtStrComp(fixedText, "xxxx", 0, FALSE) == 0);

    require_line("seed_replay", replayA == replayB);

    require_line("object_replay",
        obj32A == obj32B &&
        obj64A == obj64B &&
        diceA == diceB &&
        diceA >= 1 &&
        diceA <= 6 &&
        xrtStrComp(objTokenA, objTokenB, 0, FALSE) == 0 &&
        xrtStrByteLen(objTokenA, 0) == 10 &&
        xrtStrComp(objFixed, "zzzzz", 0, FALSE) == 0);

    xrtFree(token);
    xrtFree(fixedText);
    xrtFree(objTokenA);
    xrtFree(objTokenB);
    xrtFree(objFixed);
    xrtRandDestroy(rngA);
    xrtRandDestroy(rngB);

    xrtUnit();
    return 0;
}
