#ifndef XL6_FS_FILE_H
#define XL6_FS_FILE_H
void xl6_xrt_fs_file_destroy(void* handle);
/* Fixed File resource projection, shared by generated filesystem operations
 * and resident consumers. This owns only an XRT resource, not a language object.
 * References protect lifetime; cursor/IO/explicit close still require caller
 * serialization, exactly as the public File contract specifies. */
typedef struct xlir_fs_file {
    xfile file;
    xatomic32 refs;
    xatomic32 lock;
    char* tempPath;
    bool deleteOnClose;
} xlir_fs_file;
static inline xlir_fs_file* xlir_fs_file_alloc(void)
{
    xlir_fs_file* owner=xrtCalloc(1,sizeof(*owner));
    if(owner){xrtAtomic32Init(&owner->refs,1);xrtAtomic32Init(&owner->lock,0);}
    return owner;
}
static inline xfile xlir_fs_file_get(void* handle)
{
    xlir_fs_file* owner=handle;
    if(!owner || !owner->file){xrtSetErrorKind(XERR_CLOSED);return NULL;}
    return owner->file;
}
static inline bool xlir_fs_temp_remove(xlir_fs_file* owner)
{
    if(!owner->deleteOnClose || !owner->tempPath)return true;
    if(!owner->file){xrtSetErrorKind(XERR_STATE);return false;}
    if(!xrtFileDeleteOpen(owner->file,owner->tempPath))return false;
    owner->deleteOnClose=false;return true;
}
static inline void xlir_fs_file_drop(xlir_fs_file* owner)
{
    if(owner && xrtAtomic32FetchSub(&owner->refs,1,XMEMORY_ACQ_REL)==1){
        (void)xlir_fs_temp_remove(owner);
        if(owner->file)(void)xrtClose(owner->file);
        xrtFree(owner->tempPath);xrtFree(owner);
    }
}
static inline bool xlir_fs_file_ref(xlir_fs_file* owner)
{
    uint32_t refs=xrtAtomic32Load(&owner->refs,XMEMORY_RELAXED);
    for(;;){
        if(!refs || refs==UINT32_MAX){xrtSetErrorKind(XERR_RANGE);return false;}
        if(xrtAtomic32CompareExchange(&owner->refs,&refs,refs+1,XMEMORY_ACQ_REL,XMEMORY_RELAXED))return true;
    }
}
#endif
