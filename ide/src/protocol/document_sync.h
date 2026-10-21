#ifndef XLANG_DOCUMENT_SYNC_H
#define XLANG_DOCUMENT_SYNC_H
#include <stdint.h>
#include <stddef.h>
#include <xrt.h>

typedef struct xls_sync xls_sync;
typedef struct xls_sync_document {
    const char *uri,*text;
    size_t length;
    uint64_t identity,revision;
} xls_sync_document;
typedef struct xls_sync_stats { uint64_t opens,changes,closes,textBytes; } xls_sync_stats;
/* Callback borrows params for this call; retain it if an asynchronous sender
 * needs it. Returning false poisons the connection: destroy sync and reconnect. */
typedef int (*xls_sync_emit)(void* user,const char* method,const xvalue* params);
/* changeKind is 1 (full) or 2 (incremental). One owner thread per instance. */
xls_sync* xlsSyncCreate(int changeKind);
void xlsSyncDestroy(xls_sync* sync);
/* identity/revision must identify exact text: unchanged revisions reuse cached
 * snapshots without scanning or copying bodies. All inputs are validated and
 * notifications prepared before any emission. Successful update owns no inputs. */
int xlsSyncUpdate(xls_sync* sync,const xls_sync_document* documents,size_t count,
                  xls_sync_emit emit,void* user);
int xlsSyncVersion(const xls_sync* sync,const char* uri);
void xlsSyncStats(const xls_sync* sync,xls_sync_stats* stats);
#endif
