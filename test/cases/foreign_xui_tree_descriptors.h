#ifndef XL6_TEST_XUI_TREE_DESCRIPTORS_H
#define XL6_TEST_XUI_TREE_DESCRIPTORS_H
#include <xge3_runtime.h>
#ifdef __cplusplus
extern "C" {
#endif
void treeDescriptorsHeapBegin(void);
void treeDescriptorsHeapEnd(int mode);
void treeDescriptorsHeapBudget(int allowance);
void treeDescriptorsNativeHot(xui_widget tree);
#ifdef __cplusplus
}
#endif
#endif
