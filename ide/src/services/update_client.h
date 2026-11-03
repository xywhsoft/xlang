#ifndef XIDE_UPDATE_CLIENT_H
#define XIDE_UPDATE_CLIENT_H
typedef struct xide_app xide_app;
int xideUpdateAction(xide_app*,int);
void xideUpdatePoll(xide_app*);
void xideUpdateDestroy(xide_app*);
int xideUpdateOnQuit(xide_app*);
int xideUpdateIsModal(xide_app*);
int xideUpdatePending(xide_app*);
#endif
