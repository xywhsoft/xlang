#include "xl6_vfs_private.h"
#include "xl6_fs_file.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
static inline xvfspack xlir_fs_pack_create(xfile source, const xvfspackoptions* options,
    uint64_t offset, int64_t length)
{ uint64_t size;
  if (length < -1) { xrtSetErrorKind(XERR_RANGE); return NULL; }
  if ((xrtFileCapabilities(source) & (XFILE_CAP_READ_AT | XFILE_CAP_STAT)) !=
      (XFILE_CAP_READ_AT | XFILE_CAP_STAT)) { xrtSetErrorKind(XERR_UNSUPPORTED); return NULL; }
  if (!xrtFileSize(source,&size)) { return NULL; }
  if (offset > size || (length >= 0 && (uint64_t)length > size - offset))
    { xrtSetErrorKind(XERR_RANGE); return NULL; }
  return xrtVfsPackCreate(source,offset,length < 0 ? size - offset : (uint64_t)length,options); }

XL6_TEXT_API void* xl6_xrt_fs_pack_options_create(int64_t format, uint64_t maxEntries,
    uint64_t maxIndexBytes, uint64_t maxEntryBytes, uint64_t maxDecodedBytes, uint64_t cacheBytes)
{ xvfspackoptions* options;
  if (format < 0 || format > 2 || maxEntries == 0 || maxEntries > UINT32_MAX ||
      maxIndexBytes == 0 || maxEntryBytes == 0 || maxDecodedBytes == 0)
    { xrtSetErrorKind(XERR_RANGE); return NULL; }
  options = (xvfspackoptions*)xrtMalloc(sizeof(*options)); if (options == NULL) { return NULL; }
  xrtVfsPackOptionsInit(options); options->Format = (xvfspackformat)format;
  options->MaxEntries = (uint32_t)maxEntries; options->MaxIndexBytes = maxIndexBytes;
  options->MaxEntryBytes = maxEntryBytes; options->MaxDecodedBytes = maxDecodedBytes;
  options->CacheBytes = cacheBytes; return options; }

XL6_TEXT_API void xl6_xrt_fs_pack_options_destroy(void* handle) { xrtFree(handle); }

XL6_TEXT_API uint64_t xl6_xrt_fs_pack_options_value(void* handle, int64_t field)
{ const xvfspackoptions* options = (const xvfspackoptions*)handle;
  if (options == NULL) { xrtSetErrorKind(XERR_CLOSED); return 0; }
  switch (field) {
    case 0: return options->Format; case 1: return options->MaxEntries;
    case 2: return options->MaxIndexBytes; case 3: return options->MaxEntryBytes;
    case 4: return options->MaxDecodedBytes; case 5: return options->CacheBytes;
    default: xrtSetErrorKind(XERR_RANGE); return 0;
  } }

XL6_TEXT_API void* xl6_xrt_fs_pack_path_create(xstrview path, void* options, uint64_t offset, int64_t length)
{ const char* name; xfile source; xvfspack pack; xerror* error;
  if (options == NULL) { xrtSetErrorKind(XERR_CLOSED); return NULL; }
  if (length < -1) { xrtSetErrorKind(XERR_RANGE); return NULL; }
  name = xl6VfsPath(path); if (name == NULL) { return NULL; }
  source = xrtOpen(name,XFILE_READ); if (source == NULL) { return NULL; }
  pack = xlir_fs_pack_create(source,options,offset,length);
  if (pack == NULL) {
    error = xrtTakeError(); (void)xrtClose(source); xrtClearError(); xrtSetErrorTake(error);
  }
  return pack; }

XL6_TEXT_API void* xl6_xrt_fs_pack_take_create(void* handle, void* options, uint64_t offset, int64_t length)
{ xlir_fs_file* owner = (xlir_fs_file*)handle; xvfspack pack;
  xfile source = xlir_fs_file_get(handle); if (source == NULL) { return NULL; }
  if (options == NULL) { xrtSetErrorKind(XERR_CLOSED); return NULL; }
  if (xrtAtomic32Load(&owner->refs,XMEMORY_ACQUIRE) != 1 || owner->deleteOnClose)
    { xrtSetErrorKind(XERR_STATE); return NULL; }
  pack = xlir_fs_pack_create(source,options,offset,length);
  if (pack != NULL) { owner->file = NULL; }
  return pack; }

XL6_TEXT_API void* xl6_xrt_fs_pack_stats_create(void* handle)
{ xvfspackstats* stats;
  if (handle == NULL) { xrtSetErrorKind(XERR_CLOSED); return NULL; }
  stats = (xvfspackstats*)xrtMalloc(sizeof(*stats)); if (stats == NULL) { return NULL; }
  if (!xrtVfsPackStats((xvfspack)handle,stats)) { xrtFree(stats); return NULL; }
  return stats; }

XL6_TEXT_API void xl6_xrt_fs_pack_stats_destroy(void* handle) { xrtFree(handle); }

XL6_TEXT_API uint64_t xl6_xrt_fs_pack_stats_value(void* handle, int64_t field)
{ const xvfspackstats* s = (const xvfspackstats*)handle;
  if (s == NULL) { xrtSetErrorKind(XERR_CLOSED); return 0; }
  switch (field) {
    case 0: return s->Hits; case 1: return s->Misses; case 2: return s->Loads;
    case 3: return s->Failures; case 4: return s->ResidentBytes; case 5: return s->Evictions;
    default: xrtSetErrorKind(XERR_RANGE); return 0;
  } }

XL6_TEXT_API void* xl6_xrt_vfs_pack_create(xstrview path, int64_t offset, int64_t length,
    int64_t format, int64_t cacheBytes, int64_t maxEntryBytes, int64_t maxDecodedBytes)
{
    const char* name;
    xfile source;
    xvfspack pack;
    xvfspackoptions options;
    uint64_t size;
    xerror* error;
    if (offset < 0 || length < -1 || format < 0 || format > 2 ||
        cacheBytes < 0 || maxEntryBytes <= 0 || maxDecodedBytes <= 0) {
        xrtSetErrorKind(XERR_RANGE); return NULL;
    }
    name = xl6VfsPath(path); if (name == NULL) return NULL;
    source = xrtOpen(name, XFILE_READ); if (source == NULL) return NULL;
    if (!xrtFileSize(source, &size) || (uint64_t)offset > size ||
        (length >= 0 && (uint64_t)length > size - (uint64_t)offset)) {
        if (xrtGetError() == NULL) xrtSetErrorKind(XERR_RANGE);
        error = xrtTakeError(); (void)xrtClose(source);
        xrtClearError(); if (error != NULL) xrtSetErrorTake(error); return NULL;
    }
    xrtVfsPackOptionsInit(&options);
    options.Format = (xvfspackformat)format;
    options.CacheBytes = (uint64_t)cacheBytes;
    options.MaxEntryBytes = (uint64_t)maxEntryBytes;
    options.MaxDecodedBytes = (uint64_t)maxDecodedBytes;
    pack = xrtVfsPackCreate(source, (uint64_t)offset,
        length < 0 ? size - (uint64_t)offset : (uint64_t)length, &options);
    if (pack == NULL) {
        error = xrtTakeError(); (void)xrtClose(source);
        xrtClearError(); if (error != NULL) xrtSetErrorTake(error);
    }
    return pack;
}

XL6_TEXT_API void xl6_xrt_vfs_pack_destroy(void* handle)
{
    xerror* error = xrtTakeError();
    xrtVfsPackDestroy((xvfspack)handle);
    xrtClearError(); if (error != NULL) xrtSetErrorTake(error);
}

XL6_TEXT_API int64_t xl6_xrt_vfs_pack_format(void* pack) { return (int64_t)xrtVfsPackFormat((xvfspack)pack); }

XL6_TEXT_API void* xl6_xrt_vfs_mount_pack(void* handle,void* provider,xstrview prefix,int64_t priority,int64_t mode)
{
    if(priority<INT32_MIN || priority>INT32_MAX || mode<0 || mode>1){xrtSetErrorKind(XERR_RANGE);return NULL;}
    const char* path=xl6VfsPath(prefix);if(!path)return NULL;xvfs fs=xl6VfsGet(handle);if(!fs)return NULL;
    return xrtVfsPackMount(fs,path,(int32_t)priority,(xvfscase)mode,provider,0u);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6VfsPackHandlePolicyV1Get,xl6_xrt_vfs_pack_destroy)
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6PackOptionsHandlePolicyV1Get,xl6_xrt_fs_pack_options_destroy)
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6PackStatsHandlePolicyV1Get,xl6_xrt_fs_pack_stats_destroy)
