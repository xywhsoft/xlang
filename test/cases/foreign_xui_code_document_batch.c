#include "test/cases/foreign_xui_code_document_batch.h"
#include <assert.h>

static int prepared;
static int published;
static int refuse;

static int before_publish(void* user)
{
    assert(user == &refuse);
    ++prepared;
    return refuse ? XUI_ERROR_INVALID_STATE : XUI_OK;
}

static void after_publish(void* user)
{
    assert(user == &refuse);
    ++published;
}

const xui_code_document_batch_callbacks_t* fixtureBatchHooks(int reject)
{
    static const xui_code_document_batch_callbacks_t hooks = {
        sizeof(hooks), before_publish, after_publish, &refuse
    };
    refuse = reject;
    prepared = published = 0;
    return &hooks;
}

int fixtureBatchPrepared(void) { return prepared; }
int fixtureBatchPublished(void) { return published; }

static int32_t other_thread(void* raw)
{
    xui_code_document_batch batch = raw;
    bool committed = true;
    assert(xuiCodeDocumentBatchOwnershipBusy(batch));
    assert(xuiCodeDocumentBatchGetCommitted(batch, &committed) == XUI_ERROR_BUSY && committed);
    assert(xuiCodeDocumentBatchCommit(batch, &committed) == XUI_ERROR_BUSY && !committed);
    assert(xuiCodeDocumentBatchDestroy(batch) == XUI_ERROR_BUSY);
    return 0;
}

void fixtureBatchWrongThread(xui_code_document_batch batch)
{
    xthread* worker = xrtThreadCreate(other_thread, batch, 0);
    assert(worker && xrtThreadWaitFor(worker, 5000) == XWAIT_OK);
    xrtThreadDestroy(worker);
}

void fixtureBatchFailAfter(int allowance)
{
    assert(xrtMemDebugFailAfter(allowance));
}
