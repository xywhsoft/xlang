#ifndef FOREIGN_XUI_DOCUMENT_SOURCE_PATCHES_H
#define FOREIGN_XUI_DOCUMENT_SOURCE_PATCHES_H
#include <xge3_runtime.h>
#ifdef __cplusplus
extern "C" {
#endif
void fixtureDocPatchBegin(void);
void fixtureDocPatchBudget(int);
void fixtureDocPatchEndBudget(void);
void fixtureDocPatchBalanced(void);
#ifdef __cplusplus
}
#endif
#endif
