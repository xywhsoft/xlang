#ifndef XIDE_NATIVE_EDIT_PROTECTION_H
#define XIDE_NATIVE_EDIT_PROTECTION_H
#include "platform/workspace_edit_aliases.h"
typedef struct xide_native_protection xide_native_protection;
typedef struct xide_native_protection_seed {
 const char *runtimeDirectory,*compiler,*storageDirectory,*sessionDirectory;
 const char *preferences,*workspace,*projectSession;
 const char* const* manifests;size_t projects;
} xide_native_protection_seed;
/* Immutable host metadata, not source/write authority. Prepare lexical absolute
 * paths only: no file open/stat/body/hash/manifest read. Protect the bundled
 * data/extlibs/include/notices/option roots for IDE and selected compiler,
 * session/recovery storage, exact preferences/workspace/project manifests.
 * User project directories, include paths and examples remain source-capable.
 * A caller-designated session storage root is IDE-owned data, not a source root.
 * Deduplicate with the existing Unicode ordinal path comparison. Max255 paths,
 *1MiB copied UTF8 text and4MiB known metadata, no silent prefix on exhaustion.
 * The actual alias gate later pins physical identities and explicit absence. */
xide_native_protection* xideNativeProtectionCreate(const xide_native_protection_seed*,xide_model_error*);
void xideNativeProtectionRelease(xide_native_protection*);
size_t xideNativeProtectionCount(const xide_native_protection*);
const xide_edit_alias_exclusion* xideNativeProtectionPaths(const xide_native_protection*);
int xideNativeProtectionEqual(const xide_native_protection*,const xide_native_protection*);
#endif
