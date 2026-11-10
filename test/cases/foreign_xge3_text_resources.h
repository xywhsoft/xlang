#ifndef FIXTURE_XGE3_TEXT_RESOURCES_H
#define FIXTURE_XGE3_TEXT_RESOURCES_H
#include <xge.h>
const char* fixtureXgeTextPath(void);
int fixtureXgeTextHotBegin(void);
int fixtureXgeTextHotEnd(void);
void fixtureXgeTextCostBegin(void);
void fixtureXgeTextCostReport(int stage);
int fixtureXgeTextNativeCosts(xge_font font);
#endif
