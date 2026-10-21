#ifndef XLANG_IDE_RESOURCE_TEXT_H
#define XLANG_IDE_RESOURCE_TEXT_H
#include "model/workspace.h"
/* An immutable original plus a JSON projection of editable resource fields.
   Extension values stay in their original XLON spelling, outside the old DOM. */
typedef struct xide_resource_text { char* original; xvalue* projection; } xide_resource_text;
void xideResourceTextClear(xide_resource_text* text);
int xideResourceTextParse(xide_resource_text* text,const char* body,xide_model_error* error);
/* Reject changes to uneditable DOM extensions, return an owned field projection. */
xvalue* xideResourceTextDraft(const xvalue* original,const xvalue* draft,xide_model_error* error);
/* xrtFree result. Only projected fields may change; no filesystem mutation. */
char* xideResourceTextSerialize(const xide_resource_text* text,const xvalue* draft,xide_model_error* error);
#endif
