#ifndef XLANG_IDE_FILE_RENAME_H
#define XLANG_IDE_FILE_RENAME_H
#include "platform/text_file.h"
/* Same-volume handle rename, preserving the file identity and byte baseline.
   Never replaces another file or falls back to copy/delete. */
typedef struct xide_file_snapshot {
 uint64_t length,index,lastWrite; uint32_t volume,attributes; unsigned char sha256[32];
} xide_file_snapshot;
/* Streaming snapshots support binary resources without a text-size limit. */
int xideFileRenameSourceValid(const char* source,xide_model_error* error);
int xideFileSnapshotRead(const char* source,xide_file_snapshot* snapshot,xide_model_error* error);
/* Read-only inspection accepts read-only regular files, with a 512 MiB bound. */
int xideFileSnapshotInspect(const char* source,xide_file_snapshot* snapshot,xide_model_error* error);
/* Inspect the caller's already pinned read handle; neither closes nor reopens it. */
int xideFileSnapshotInspectHandle(void* handle,xide_file_snapshot* snapshot,xide_model_error* error);
int xideFileSnapshotEqual(const xide_file_snapshot* a,const xide_file_snapshot* b);
int xideFileRenameChecked(const char* source,const char* target,const xide_text_file* textBaseline,const xide_file_snapshot* snapshot,xide_model_error* error);
int xideFileRenameTargetValid(const char* target,xide_model_error* error);
int xideFileRename(const char* source,const char* target,const xide_text_file* baseline,xide_model_error* error);
/* Directory moves preserve all children and only validate the root identity.
   A replaced root, reparse point or existing target is never accepted. */
typedef struct xide_directory_identity { uint64_t index; uint32_t volume; } xide_directory_identity;
int xideDirectoryIdentityRead(const char* source,xide_directory_identity* identity,xide_model_error* error);
int xideDirectoryRenameChecked(const char* source,const char* target,const xide_directory_identity* identity,xide_model_error* error);
#endif
