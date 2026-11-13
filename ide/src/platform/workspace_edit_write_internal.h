#ifndef XIDE_WORKSPACE_EDIT_WRITE_INTERNAL_H
#define XIDE_WORKSPACE_EDIT_WRITE_INTERNAL_H
#include "platform/workspace_edit_write.h"
/* Only while the enclosing transaction retains exclusive reader locks. Match
 * the last fully verified writer state and pinned names without reopening or
 * hashing data. Not a replacement for the writer's before/after full checks. */
int xideEditFileWriteVerifyMetadata(xide_edit_file_write*,xide_model_error*);
#endif
