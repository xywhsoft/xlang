#ifndef XIDE_DIAGNOSTIC_NOTICE_H
#define XIDE_DIAGNOSTIC_NOTICE_H
#include "services/diagnostic_dependencies.h"
#include "analysis/text.h"
#include <stdint.h>

typedef struct xide_diagnostic_item {
 int line,character,endLine,endCharacter,severity,start,end;
 const char *message,*source;char* title;
} xide_diagnostic_item;
typedef struct xide_diagnostic_binding {
 const char* uri;char* path;wchar_t* key;int version;
 uint64_t identity,revision; /* Filled only by exact sent-sync matching. */
} xide_diagnostic_binding;
typedef struct xide_diagnostic_notice {
 uint64_t generation,inputSerial;int hasSerial,cleared,overlay,version,versionPresent,mapped,sourceVerified;
 size_t normalizedSize,count;char normalizedSha256[65];
 char* path;wchar_t* key;const char* uri;const xvalue* input;
 xide_diagnostic_item* items;
 int hasOverlays,overlaysMatched,diskValidated;size_t bindingCount,wireBytes;
 xide_diagnostic_binding* bindings;
 struct xide_diagnostic_notice* retiredNext; /* Host-owned retirement list. */
 /* Read-only backing storage and complete dependency lease, owned here. */
 xvalue* params;xide_diagnostic_dependency* dependency;
} xide_diagnostic_notice;

/* Worker-only decoder for the 0.26 dependency/file-proof contract and optional
 * 0.27 causal overlay metadata.
 * Caller retains the existing bounded transport/frame admission. Borrows params,
 * retains it only on success; no deep clone, disk read or UI.
 * Wire references change only after the entire notice passes validation.
 * Cleared is withdrawal, including an unknown path; a clean analyzed file is
 * a non-cleared notice with zero items and full proof/dependency metadata. */
xide_diagnostic_notice* xideDiagnosticNoticeDecode(xide_diagnostic_dependencies* dependencies,
 const xvalue* params,int (*cancelled)(void*),void* user,char* error,size_t capacity);
/* Worker-only, before handing the notice to readers. Validates the exact
 * normalized digest (post-BOM, LF), then maps all UTF-16 ranges to source byte
 * offsets through the existing strict batch adapter. Original source CRLF/BOM
 * bytes remain in offsets. Failure/cancellation leaves every offset unchanged.
 * sourceVerified distinguishes an invalid range in verified current text from
 * stale/mismatched text. A notice maps once. Host must ALSO gate connection,
 * input receipt, model
 * identity/revision, origin/version, dependency changes and configuration. */
int xideDiagnosticNoticeMap(xide_diagnostic_notice* notice,const xls_text* source,
 int (*cancelled)(void*),void* user);
/* Large payloads/titles/paths are freed on the worker or at final shutdown. */
void xideDiagnosticNoticeRelease(xide_diagnostic_notice* notice);
#endif
