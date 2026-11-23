#ifndef XL6_FIXTURE_XGE3_CLIPBOARD_BATCH_H
#define XL6_FIXTURE_XGE3_CLIPBOARD_BATCH_H
#include <xge3_runtime.h>

int clipboardBatchProxy(xui_proxy* out);
void clipboardBatchRemember(const void* payload, size_t size);
int clipboardBatchCalls(void);
int clipboardBatchReferences(void);
void clipboardBatchReject(int enabled);
int clipboardBatchIsolatedDisplay(void);
void clipboardBatchFailAfter(int allowance);

#endif
