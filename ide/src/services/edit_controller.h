#ifndef XIDE_EDIT_CONTROLLER_H
#define XIDE_EDIT_CONTROLLER_H
#include "services/edit_candidate.h"
typedef struct xide_edit_controller xide_edit_controller;
enum xide_edit_controller_state {XIDE_EDIT_IDLE,XIDE_EDIT_PREPARING,XIDE_EDIT_PREPARED,
 XIDE_EDIT_WAITING,XIDE_EDIT_PROCESSING,XIDE_EDIT_READY,XIDE_EDIT_REMOTE_ERROR,
 XIDE_EDIT_REJECTED,XIDE_EDIT_CANCELLED,XIDE_EDIT_RETIRING};
typedef struct xide_edit_controller_status {
 enum xide_edit_controller_state state;
 const xide_edit_request* request;
 xide_edit_call* call;
 const xide_edit_candidate* candidate;
 uint64_t workerThread,retiredThread;
 int active,paused,remoteCode;
 char error[256];
} xide_edit_controller_status;
/* One UI owner, one outstanding operation. A persistent XRT thread prepares
 * calls, routes/validates replies and retires all large assets. Transport Receive
 * only transfers one same-connection, typed-ID root, without decoding proofs.
 * Wake never touches UI state; its user outlives Destroy. No source writes. */
xide_edit_controller* xideEditControllerCreate(void (*wake)(void*),void*);
/* Owner metadata copy <=262144 URI +4096 name bytes, retained immutable request.
 * Host captures/validates full scope, capability and source-job admission first.
 * Failure takes nothing. No transport, body scan, path I/O or compiler. */
int xideEditControllerSubmit(xide_edit_controller*,xide_edit_request*,uint64_t connection,
 const char* uri,int line,int character,xstrview name);
/* Owner borrowed pointers remain alive until Drop/Destroy, never carry them
 * across another mutating owner call/event dispatch. Repeat full host Current
 * before Send and candidate publication/consent; READY is data admission only. */
int xideEditControllerStatus(xide_edit_controller*,xide_edit_controller_status*);
int xideEditControllerSend(xide_edit_controller*,xide_language_client*);
/* Transport callback; success TAKES exactly the supplied root, failure nothing.
 * Late/unmatched/duplicate responses use the host's existing background discard. */
int xideEditControllerReceive(xide_edit_controller*,uint64_t peer,xvalue*,size_t);
/* Local cancellation is immediate; queue the preencoded cancel on the original
 * client with CancelSend (retry after queue backpressure). Neither waits, joins
 * nor encodes JSON. Drop schedules worker retirement, invalidating borrows. */
void xideEditControllerCancel(xide_edit_controller*);
int xideEditControllerCancelSend(xide_edit_controller*,xide_language_client*);
void xideEditControllerDrop(xide_edit_controller*);
/* Close admission before the source barrier probes physical quiescence. An
 * already READY candidate is preserved for a later transaction; in-flight work
 * is cancelled. Quiet proves paused + no queued/running work, not a future flag.
 * No new Receive/Submit can start reads while paused. Resume only after writes
 * and all host recovery duties settle. It never validates a stale candidate. */
void xideEditControllerSuspend(xide_edit_controller*);
int xideEditControllerQuiet(void*);
int xideEditControllerResume(xide_edit_controller*);
/* Final shutdown only. Stop/join transport callbacks first, unlink source-job
 * probe, then Destroy joins this worker and frees its small synchronization. */
void xideEditControllerDestroy(xide_edit_controller*);
#endif
