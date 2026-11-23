#ifndef FOREIGN_XUI_STANDALONE_CODE_COMMANDS_H
#define FOREIGN_XUI_STANDALONE_CODE_COMMANDS_H

#include <xge3_runtime.h>

void fixtureCodeCommandArmOom(void);
bool fixtureCodeCommandClearOom(void);
void fixtureCodeCommandBeginMeasure(void);
void fixtureCodeCommandEndMeasure(int language);
void fixtureCodeCommandMeasureNative(xui_code_document document,xui_code_selection_model selection);

#endif
