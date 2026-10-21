#ifndef XIDE_MESSAGE_BOX_H
#define XIDE_MESSAGE_BOX_H
#include "xui.h"
#include <stdint.h>
typedef struct xide_app xide_app;
typedef struct xide_document xide_document;
typedef struct xide_messages xide_messages;
void xideMessageShow(xide_app*,const char*,const char*,int);
int xideMessageConfirm(xide_app*,const char*,const char*,int,uint64_t);
uint64_t xideMessageDocumentKey(xide_document*,int,int);
uint64_t xideMessageWorkspaceKey(xide_app*,int);
void xideMessageBeginAction(xide_app*,int);
void xideMessageBeginClose(xide_app*,xide_document*);
void xideMessageBeginQuit(xide_app*);
void xideMessageEndOperation(xide_app*);
int xideMessagePending(xide_app*);
int xideMessageBusy(xide_app*);
void xideMessagePoll(xide_app*);
void xideMessageDestroy(xide_app*);
/* Inspection for native regression drivers; the SDK owns all box widgets. */
xui_msgbox xideMessageCurrent(xide_app*);
char* xideMessageSavePath(xide_app*,uint64_t);
char* xideMessageChoosePath(xide_app*,int,const char*,const char*,const char*,const char*,uint64_t,uint64_t);
int xideMessageRememberSavePath(xide_app*,uint64_t,const char*);
#endif
