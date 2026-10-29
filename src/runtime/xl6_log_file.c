#include "src/runtime/xl6_log.h"

xstrview xl6LogFilePathV1(void* sink)
{
    const char* path=xrtLogFilePath(sink);
    if (!path) return (xstrview){0};
    xstrview source=xrtStrView(path);
    return (xstrview){xrtStrDupN(source.Data,source.Size),source.Size};
}
bool xl6LogFileRotateV1(void* sink) { return xrtLogFileRotate(sink); }
bool xl6LogFileReopenV1(void* sink) { return xrtLogFileReopen(sink); }
bool xl6LogFileCloseV1(void* sink) { return xrtLogFileClose(sink); }
void* xl6LogFileStatsV1(void* sink)
{
    xlogfilestats* stats=xrtMalloc(sizeof(*stats));
    if (!stats) return NULL;
    if (!xrtLogFileStats(sink,stats)) { xrtFree(stats); return NULL; }
    return stats;
}
void* xl6LogFileStatsTakeV1(void* stats)
{ if (!stats) xrtSetErrorKind(XERR_ARGUMENT); return stats; }
void xl6LogFileStatsDestroyV1(void* stats) { xrtFree(stats); }
uint64_t xl6LogFileCounterV1(void* snapshot,int64_t index)
{
    if (!snapshot) { xrtSetErrorKind(XERR_ARGUMENT); return 0; }
    const xlogfilestats* stats=snapshot;
    switch (index) {
    case 0:return stats->CurrentBytes; case 1:return stats->WrittenBytes;
    case 2:return stats->Records; case 3:return stats->Rotations;
    case 4:return stats->Reopens; case 5:return stats->Syncs;
    default:xrtSetErrorKind(XERR_RANGE); return 0;
    }
}
