#ifndef XIDE_WORKSPACE_EDIT_ALIASES_H
#define XIDE_WORKSPACE_EDIT_ALIASES_H
#include "protocol/workspace_edit.h"
#include "model/workspace.h"
typedef struct xide_edit_aliases xide_edit_aliases;
typedef struct xide_edit_alias_model {const char* path;uint64_t identity;} xide_edit_alias_model;
typedef struct xide_edit_alias_exclusion {const char* path;int directory;} xide_edit_alias_exclusion;
/* Worker-only physical policy for the complete target set and ALL named open
 * models, not just edited overlays. Model identity/path metadata must be captured
 * on the owner and independently kept current by the UI host. Inputs borrowed
 * during Pin only; no plan/body/model pointer survives construction.
 * Target paths require ordinary local fixed-drive components. Target files are
 * single-link; overlays may be readonly or absent. Existing target ancestors
 * stay pinned without delete sharing. Unrelated models may resolve aliases but
 * any physical collision involving a target rejects the group. Protected roots
 * reject lexical descendants AND physically identical target ancestors (e.g.
 * a different drive/volume spelling); protected files reject physical aliases.
 * Missing named overlays/models/protected paths are retained as absence claims,
 * not silently skipped; all names and handles are revalidated.
 * Max512 targets,4096 models,256 exclusions,4096 unique target ancestors from
 *32768 candidates,8MiB native path bytes,16MiB known retained storage. Bounded
 *32KiB ID-sort scratch, allocator/OS resources are additional. No body reads,
 * hashes, writes, locks against byte writers, source closure or edit approval. */
xide_edit_aliases* xideEditAliasesPin(const xide_workspace_edit*,const xide_edit_alias_model*,size_t,
 const xide_edit_alias_exclusion*,size_t,int (*cancelled)(void*),void*,xide_model_error*);
int xideEditAliasesValidate(xide_edit_aliases*,int (*cancelled)(void*),void*,xide_model_error*);
size_t xideEditAliasesStorage(const xide_edit_aliases*);
void xideEditAliasesDestroy(xide_edit_aliases*);
#endif
