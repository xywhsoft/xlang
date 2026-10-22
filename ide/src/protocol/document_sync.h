#ifndef XLANG_DOCUMENT_SYNC_H
#define XLANG_DOCUMENT_SYNC_H
#include <stdint.h>
#include <stddef.h>
#include <xrt.h>
#include "analysis/text.h"

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
/* Owner-thread lease of the exact successfully emitted URI/wire version.
 * Constant-time body ownership: no scan/copy. Release with xlsTextRelease;
 * the immutable text remains valid after edits, close or sync destruction.
 * Lookup must run on sync's owner; the returned lease may cross threads. */
xls_text* xlsSyncRetainText(const xls_sync* sync,const char* uri,int version);
/* Receipt of successfully emitted online URI/version bindings. No body scan.
 * Not a transport acknowledgement: compare with server inputState response,
 * then separately verify local model identities/revisions and file proofs. */
int xlsSyncInputState(const xls_sync* sync,char digest[65],size_t* documents);
void xlsSyncStats(const xls_sync* sync,xls_sync_stats* stats);
#endif
