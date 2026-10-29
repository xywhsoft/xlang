#ifndef XL6_FS_SNAPSHOT_H
#define XL6_FS_SNAPSHOT_H
#include <string.h>
/* Physical layouts shared by resident VFS creation and reached FS adapters.
 * Metadata owns copied native text; directory owns an XRT directory cursor. */
typedef struct xlir_fs_info { xfileinfo info; char* path; char* name; size_t nameSize; uint32_t flags; } xlir_fs_info;
typedef struct xlir_fs_dir { xdir dir; xdirentry entry; bool current; } xlir_fs_dir;
static inline void xlir_fs_info_drop(xlir_fs_info* info)
{ if(info){xrtFree(info->path);xrtFree(info->name);xrtFree(info);} }
static inline xlir_fs_info* xlir_fs_info_alloc(const char* path)
{
    const char* name=path;xlir_fs_info* info=xrtCalloc(1,sizeof(*info));if(!info)return NULL;
    for(const char* at=path;*at;++at)if(*at=='/'
#if defined(_WIN32)
        || *at=='\\'
#endif
        )name=at+1;
    info->path=xrtStrDup(path);info->nameSize=strlen(name);info->name=xrtStrDupN(name,info->nameSize);
    if(!info->path || !info->name){xlir_fs_info_drop(info);return NULL;}
    info->flags=1u;return info;
}
#endif
