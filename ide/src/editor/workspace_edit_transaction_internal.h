#ifndef XIDE_WORKSPACE_EDIT_TRANSACTION_INTERNAL_H
#define XIDE_WORKSPACE_EDIT_TRANSACTION_INTERNAL_H
#include "editor/workspace_edit_transaction.h"
#include "platform/workspace_edit_aliases_internal.h"
/* Private native host adapters; one dispatcher, no concurrent transaction calls.
 * Never expose write handles or GUI objects. Verify may run from Apply's same
 * worker fence; all other accesses are sequential with Stage/Apply/Finish. */
int xideEditTransactionAllNamed(xide_edit_transaction*);
const xide_edit_aliases* xideEditTransactionAliases(const xide_edit_transaction*);
size_t xideEditTransactionClosed(const xide_edit_transaction*);
int xideEditTransactionFileAt(const xide_edit_transaction*,size_t,xide_edit_file_status*);
int xideEditTransactionVerifyPinned(xide_edit_transaction*,int content,int (*cancelled)(void*),void*,xide_model_error*);
#endif
