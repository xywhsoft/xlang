#ifndef XLS_ANALYSIS_RENAME_POLICY_H
#define XLS_ANALYSIS_RENAME_POLICY_H
#include "analysis/index.h"
enum xls_rename_policy {
 XLS_RENAME_POLICY_FOREIGN=1u, XLS_RENAME_POLICY_COMPILED=2u,
 XLS_RENAME_POLICY_UNSUPPORTED=4u
};
/* Demand-only conservative exclusions from checked native declaration ancestry.
 * No source spelling search, parser, allocation, I/O or retained AST. At is the
 * original zero-based index ordinal. Invalid or >256-deep ancestry fails with
 * UNSUPPORTED. Zero flags does not prove scope/family/freshness/write safety.
 * Foreign aliases are deliberately excluded even with explicit C link names;
 * contracts can contain non-reference names which source rename cannot edit.
 * Ordinary navigation and reference collection do not apply this policy. */
int xlsSemanticIndexRenamePolicy(const xls_semantic_index*,size_t at,unsigned*);
#endif
