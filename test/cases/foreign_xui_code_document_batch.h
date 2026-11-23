#ifndef FIXTURE_XUI_CODE_DOCUMENT_BATCH_H
#define FIXTURE_XUI_CODE_DOCUMENT_BATCH_H
#include <xge3_runtime.h>

const xui_code_document_batch_callbacks_t* fixtureBatchHooks(int refuse);
int fixtureBatchPrepared(void);
int fixtureBatchPublished(void);
void fixtureBatchWrongThread(xui_code_document_batch batch);
void fixtureBatchFailAfter(int allowance);

#endif
