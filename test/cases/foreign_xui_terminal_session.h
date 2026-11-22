#ifndef XL6_TEST_XUI_TERMINAL_SESSION_H
#define XL6_TEST_XUI_TERMINAL_SESSION_H
#include <xui3_terminal_session.h>

void fixtureTerminalSessionBudget(int allowance);
void fixtureTerminalSessionRollback(void);
void fixtureTerminalSessionBegin(void);
void fixtureTerminalSessionEnd(int native);
void fixtureTerminalSessionNativeHot(xui_terminal_session_t* session);
int fixtureTerminalSessionEdges(xui_terminal_session_t* session);

#endif
