#ifndef FIXTURE_XGE3_FONT_RESOURCES_H
#define FIXTURE_XGE3_FONT_RESOURCES_H
#include "xge.h"
const char* fixtureXgeFontPath(void);
const char* fixtureXgeFontTemp(void);
int fixtureXgeFontHotBegin(void);
int fixtureXgeFontHotEnd(void);
void fixtureXgeFontShareSampleBegin(void);
void fixtureXgeFontShareSampleEnd(int language);
void fixtureXgeFontShareSampleC(xge_font_face face, xge_font font);
#endif
