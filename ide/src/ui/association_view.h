#ifndef XIDE_ASSOCIATION_VIEW_H
#define XIDE_ASSOCIATION_VIEW_H
#include "app/xide_app.h"
int xideAssociationViewAction(xide_app*,int);
int xideAssociationViewOpen(xide_app*);
int xideAssociationViewIsOpen(xide_app*);
int xideAssociationViewBusy(xide_app*);
int xideAssociationStartupSettled(xide_app*);
void xideAssociationViewPoll(xide_app*);
void xideAssociationViewDestroy(xide_app*);
#endif
