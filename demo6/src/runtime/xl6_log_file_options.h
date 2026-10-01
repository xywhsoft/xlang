#ifndef XL6_LOG_FILE_OPTIONS_H
#define XL6_LOG_FILE_OPTIONS_H
#include "src/runtime/xl6_log_record.h"
#include <limits.h>
#include <string.h>
static bool log_file_options(xlogfileoptions* options,xstrview path,char** owned,
    int64_t level,int64_t mode,int64_t sync,uint64_t maxBytes,int64_t backups,
    int64_t recordLimit,int64_t bufferLimit,uint64_t interval)
{
    *owned=NULL;
    if (level<0 || level>6 || mode<0 || mode>1 || sync<0 || sync>2 ||
        backups<0 || (uint64_t)backups>UINT32_MAX || recordLimit<=0 ||
        (uint64_t)recordLimit>SIZE_MAX || bufferLimit<0 || (uint64_t)bufferLimit>SIZE_MAX) {
        xrtSetErrorKind(XERR_RANGE); return false;
    }
    if (!path.Data || !path.Size || memchr(path.Data,0,path.Size)) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    *owned=xrtStrDupN(path.Data,path.Size); if (!*owned) return false;
    if (!xrtLogFileOptionsInit(options,*owned)) { xrtFree(*owned); *owned=NULL; return false; }
    options->Level=(xloglevel)level; options->Mode=(xlogfilemode)mode; options->Sync=(xlogfilesync)sync;
    options->MaxBytes=maxBytes; options->BackupCount=(uint32)backups;
    options->RecordLimit=(size_t)recordLimit; options->BufferLimit=(size_t)bufferLimit; options->SyncInterval=interval;
    return true;
}
#endif
