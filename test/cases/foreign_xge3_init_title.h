#ifndef FIXTURE_XGE3_INIT_TITLE_H
#define FIXTURE_XGE3_INIT_TITLE_H
#include <xge.h>
#include <string.h>
/* Test-only observation; not a public language adaptation. */
static inline int fixtureInitTitleMatches(const char* expected)
{const char* title=xgeGetTitle();return title && !strcmp(title,expected);}
static inline int fixtureInitTitleAvailable(void){return xgeGetTitle()!=NULL;}
#endif
