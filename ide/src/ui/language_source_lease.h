#ifndef XIDE_LANGUAGE_SOURCE_LEASE_H
#define XIDE_LANGUAGE_SOURCE_LEASE_H
#include "app/xide_app.h"
typedef struct xide_language_sources_status {
 int quiet,invalidated,failed,resuming,resumed;
} xide_language_sources_status;
/* One UI owner; tokens belong to the live language/completion views and may
 * not outlive either view. Freezes both connections, creation/rebinding and
 * local language-feature disk readers, leaving model input/undo available.
 * Nonzero is an admitted pending lease, NOT permission to write source. */
uint64_t xideLanguageSourcesSuspend(xide_app*,xide_model_error*);
/* Metadata/kernel probes only. Writing requires quiet and neither invalidated
 * nor failed/resuming; host still proves all other source readers/policies. */
int xideLanguageSourcesState(xide_app*,uint64_t,xide_language_sources_status*);
/* Caller has settled writes/rollback/recovery before requesting resume. No
 * auto-unlock, restart or timeout fallback while a source operation is active. */
int xideLanguageSourcesResume(xide_app*,uint64_t);
int xideLanguageSourcesRelease(xide_app*,uint64_t);

/* Completion participant, only for the primary coordinator above. */
int xideCompletionSourcePreflight(xide_app*);
int xideCompletionSourceSuspend(xide_app*,uint64_t);
int xideCompletionSourceState(xide_app*,uint64_t,xide_language_sources_status*);
int xideCompletionSourceResume(xide_app*,uint64_t);
int xideCompletionSourceRelease(xide_app*,uint64_t);
#endif
