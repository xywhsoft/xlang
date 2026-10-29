/* Preinclude the actual pretty fixture's generated C. Check the resident
 * status/output ABI independently of language cleanup and assertions. */
#include <assert.h>
#include <stdio.h>
#include "test/support/module_product_memory.h"

typedef bool (*pretty_file)(const xvalue*, bool, xstrview, bool*);
int main(void)
{
    pretty_file writers[] = {xl6JsonWriteFilePrettyV1, xl6XlonWriteFilePrettyV1};
    xl6ProductMemoryBegin(SIZE_MAX);
    xvalue* value = xrtValueInt(42);
    assert(value != NULL);
    for (size_t i = 0; i < sizeof(writers) / sizeof(writers[0]); ++i) {
        for (int memoryPrior = 0; memoryPrior <= 1; ++memoryPrior) {
            bool output = true;
            xerror* prior = xrtErrorCreate(memoryPrior ? XERR_MEMORY : XERR_VALUE,
                "pretty-test", 17, "prior diagnostic");
            assert(prior != NULL);
            xrtSetErrorTake(prior);
            assert(writers[i](value, true, (xstrview){"a\0b", 3}, &output));
            assert(!output && xrtGetError() == prior);
            xrtClearError();
        }
        {
            bool output = true;
            assert(!writers[i](NULL, false, xrtStrView("unused"), &output));
            assert(!output && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
            xrtClearError();
        }
        {
            bool output = true;
            assert(xrtMemDebugFailAfter(0));
            bool ok = writers[i](value, false, xrtStrView("never-created"), &output);
            assert(xrtMemDebugFailTriggered());
            xrtMemDebugFailClear();
            assert(!ok && !output && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            xrtClearError();
        }
    }
    xrtValueRelease(value);
    assert(!xl6ProductMemoryEnd());
    xl6ProductMemoryVerify();
    puts("pretty file status, prior diagnostic and OOM ownership balanced");
    return 0;
}
