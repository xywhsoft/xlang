#ifndef XL6_TEST_XUI_CODE_ANNOTATIONS_H
#define XL6_TEST_XUI_CODE_ANNOTATIONS_H
#include <xui3_code_annotations.h>
void fixtureAnnBudget(int);
void fixtureAnnPrefixBegin(void);
void fixtureAnnPrefixEnd(void);
void fixtureAnnHeapEqual(void);
void fixtureAnnHotBegin(void);
void fixtureAnnHotReport(int);
void fixtureAnnCheck(xui_code_annotation_store,int);
void fixtureAnnNativeHot(xui_code_annotation_store,uint32_t);
void fixtureAnnNativeCreate(void);
void fixtureAnnNativeSet(xui_code_annotation_store);
int fixtureAnnDuringCall(xui_code_annotation_store,void (*)(void*),void*);
#endif
