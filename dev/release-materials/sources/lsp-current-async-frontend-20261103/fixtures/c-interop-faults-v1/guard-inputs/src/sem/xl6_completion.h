#ifndef XL6_COMPLETION_H
#define XL6_COMPLETION_H
#include "src/ast/xl6_ast.h"
#include "src/type/xl6_type_id.h"
#include "src/compile/xl6_source_report.h"
#define XL6_COMPLETION_MARKER "__xl6_cursor_completion_query__"
#define XL6_COMPLETION_ITEMS 4096u
#define XL6_COMPLETION_BYTES (4u * 1024u * 1024u)
typedef struct XL6_CompletionItem {
 char* name;
 char* canonical;
 const char* kind;
 XL6_SymbolId symbolId;
 unsigned selectionSourceId; /* Resolved before emitting any completion JSON. */
 const XL6_AstNode* selection; /* Borrowed until the unit is freed. */
} XL6_CompletionItem;
typedef struct XL6_CompletionQuery {
 const char* path; /* Absolute, borrowed for one query. */
 size_t offset;
 XL6_AstRange replacement;
 char* prefix;
 XL6_CompletionItem* items;
 size_t count, capacity, bytes;
 int parsed, reached, available;
 const char* error;
} XL6_CompletionQuery;
int xl6CompletionTarget(XL6_CompletionQuery* query,const char* path);
int xl6CompletionAdd(XL6_CompletionQuery* query,const char* name,const char* kind,
 XL6_SymbolId symbolId,const XL6_AstNode* selection,char* ownedCanonical);
void xl6CompletionFree(XL6_CompletionQuery* query);
char* xl6CompletionJson(XL6_CompletionQuery* query,const XL6_SourceReport* report);
#endif
