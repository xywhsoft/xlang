#ifndef XL6_TEST_XUI_CODE_COMMANDS_H
#define XL6_TEST_XUI_CODE_COMMANDS_H
#include <xui3_code_command_map.h>
void fixtureCommandBudget(int);
void fixtureCommandPrefixBegin(void);
void fixtureCommandPrefixEnd(void);
void fixtureCommandHeapEqual(void);
void fixtureCommandHotBegin(void);
void fixtureCommandHotReport(int);
void fixtureCommandNativeHot(xui_code_command_map);
void fixtureCommandNativeCreate(void);
int fixtureCommandDuringCall(xui_code_command_map,void (*)(void*),void*);
#endif
