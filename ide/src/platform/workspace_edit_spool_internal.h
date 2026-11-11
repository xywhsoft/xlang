#ifndef XIDE_WORKSPACE_EDIT_SPOOL_INTERNAL_H
#define XIDE_WORKSPACE_EDIT_SPOOL_INTERNAL_H
#include "platform/workspace_edit_spool.h"
/* Private worker-only journal binding. Borrow directory until Spool destruction;
 * hash its already pinned immutable marker, never recapture sources or expose a
 * write handle. Spool and its exclusive recovery guard must outlive the journal. */
int xideEditSpoolJournalIdentity(xide_edit_spool*,const char** directory,unsigned char sha256[32],xide_model_error*);
#endif
