#ifndef FOREIGN_XUI_TEXT_READS_H
#define FOREIGN_XUI_TEXT_READS_H
#include <xge3_runtime.h>
void fixtureTextReadBegin(void);
int fixtureTextReadAllocations(void);
int fixtureTextReadWarm(xui_widget editor);
int fixtureTextReadSnapshot(xui_widget editor,xstrview snapshot);
#endif
