#ifndef XIDE_WORKSPACE_EDIT_ALIASES_INTERNAL_H
#define XIDE_WORKSPACE_EDIT_ALIASES_INTERNAL_H
#include "platform/workspace_edit_aliases.h"
#include <wchar.h>
typedef struct xide_edit_alias_target {const wchar_t* path;uint32_t volume;uint64_t index;int missing;} xide_edit_alias_target;
/* Immutable worker-only identity view of the actual gate. At indexes its rows:
 * 1 target, 0 other role, -1 invalid index. Borrow while the gate remains alive;
 * no new path lookup, allocation, body read or authorization. */
size_t xideEditAliasesRows(const xide_edit_aliases*);
int xideEditAliasesTargetAt(const xide_edit_aliases*,size_t,xide_edit_alias_target*);
#endif
