#ifndef XIDE_WORKSPACE_EDIT_JOURNAL_H
#define XIDE_WORKSPACE_EDIT_JOURNAL_H
#include "platform/workspace_edit_write.h"
typedef struct xide_edit_journal xide_edit_journal;
enum xide_edit_journal_phase {XIDE_EDIT_TX_PREPARED=1,XIDE_EDIT_TX_APPLYING,XIDE_EDIT_TX_READY,XIDE_EDIT_TX_PUBLISHING,XIDE_EDIT_TX_COMMITTED,XIDE_EDIT_TX_ROLLING_BACK,XIDE_EDIT_TX_ROLLED_BACK,XIDE_EDIT_TX_IN_DOUBT};
enum xide_edit_journal_file {XIDE_EDIT_TX_FILE_BEFORE=1,XIDE_EDIT_TX_FILE_APPLY_INTENT,XIDE_EDIT_TX_FILE_AFTER,XIDE_EDIT_TX_FILE_PARTIAL,XIDE_EDIT_TX_FILE_RESTORE_INTENT,XIDE_EDIT_TX_FILE_RESTORED,XIDE_EDIT_TX_FILE_UNKNOWN};
typedef struct xide_edit_journal_status {int phase,frozen,recovered,incompleteTail;size_t documents,closed,records,bytes;} xide_edit_journal_status;
typedef struct xide_edit_journal_row {int state,observed;xide_edit_file_status file;} xide_edit_journal_row;
/* Fresh writer requires live source pins, CREATE_NEW ordinary single-link log,
 * exact pinned marker binding. No existing file is truncated/replaced/deleted.
 * Spool, guard and bodies MUST outlive journal. No GUI pointers or source writes.
 * Append-only binary framing of bounded XRT JSON, chained XRT SHA-256; max8MiB,
 *4096 records,64KiB JSON/record,512 targets. Memory retains only current per-file
 * observations, not all historical JSON; generic allocator overhead additional. */
xide_edit_journal* xideEditJournalCreate(xide_edit_spool*,xide_model_error*);
/* Inspection only, ALWAYS frozen even if complete and sources still pinned.
 * A torn final frame after a valid begin yields incompleteTail; hash/sequence/
 * schema/binding violations refuse the log. Never treats a valid prefix as a
 * complete source state or automatically resumes/replays writes after restart. */
xide_edit_journal* xideEditJournalOpen(xide_edit_spool*,xide_model_error*);
int xideEditJournalStatus(const xide_edit_journal*,xide_edit_journal_status*);
int xideEditJournalRow(const xide_edit_journal*,size_t document,xide_edit_journal_row*);
/* Caller establishes full host/request/physical/consent policy before intent.
 * Journal acknowledgement is not user consent. Suitable for the required file
 * writer fence only when combined with that policy. Every acknowledgement has
 * written, flushed and read back its exact frame. An uncertain append freezes
 * the object; do not retry an operation using a previous acknowledged prefix. */
int xideEditJournalFileIntent(xide_edit_journal*,size_t document,int action,xide_model_error*);
int xideEditJournalRecordFile(xide_edit_journal*,const xide_edit_file_write*,xide_model_error*);
/* Forward transitions; READY requires all closed rows AFTER, ROLLED_BACK all
 * BEFORE/RESTORED. PUBLISHING is write-ahead of GUI publication. Rollback from
 * PUBLISHING additionally requires an explicit guiNotCommitted=true outcome;
 * never infer that result from a missing COMMITTED event or a failed append.
 * COMMITTED/ROLLED_BACK are terminal. These describe acknowledged observations,
 * not fresh physical proof; multi-file actor and recovery policy remain required. */
int xideEditJournalSetPhase(xide_edit_journal*,int phase,int guiNotCommitted,xide_model_error*);
/* Only closes/releases; preserves all log/package bytes, even failed creations. */
void xideEditJournalDestroy(xide_edit_journal*);
#endif
