#ifndef XLS_FROZEN_INPUTS_H
#define XLS_FROZEN_INPUTS_H
#include "analysis/semantic.h"
typedef struct xls_frozen_inputs xls_frozen_inputs;
#define XLS_FROZEN_INPUT_FILES 512u
/* Private immutable input owner for hypothetical complete native rechecks.
 * Match the complete source report:8MiB/file,64MiB text and512 files.
 *128MiB known owned storage includes normalized paths and prepared native
 * lexical keys. Deep copy each post-BOM body once. Do not put this owner into
 * ordinary64-overlay input history, usage masks or incremental caches. */
xls_frozen_inputs* xlsFrozenInputsCreate(const xls_semantic_input*,size_t,
 int (*cancelled)(void*),void*,char* error,size_t capacity);
xls_frozen_inputs* xlsFrozenInputsRetain(xls_frozen_inputs*);
void xlsFrozenInputsRelease(xls_frozen_inputs*);
size_t xlsFrozenInputsBytes(const xls_frozen_inputs*);
struct XL6_SourceOverlay;
struct XL6_SourceOverlayItem;
int xlsFrozenInputsLookup(const xls_frozen_inputs*,const struct XL6_SourceOverlay*,const char*,const struct XL6_SourceOverlayItem**);
#endif
