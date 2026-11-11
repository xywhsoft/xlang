#ifndef XIDE_WORKSPACE_EDIT_SPOOL_INTERNAL_H
#define XIDE_WORKSPACE_EDIT_SPOOL_INTERNAL_H
#include "platform/workspace_edit_spool.h"
/* Private worker-only journal binding. Borrow directory until Spool destruction;
 * hash its already pinned immutable marker, never recapture sources or expose a
 * write handle. Spool and its exclusive recovery guard must outlive the journal. */
int xideEditSpoolJournalIdentity(xide_edit_spool*,const char** directory,unsigned char sha256[32],xide_model_error*);
/* Conservative component reservation, including capture/codec scratch and
 * borrowed input capacities charged by Prepare; not retained RSS. */
size_t xideEditSpoolReservation(const xide_edit_spool*);
#endif
