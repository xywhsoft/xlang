#include "foreign_xui_code_language_plain.h"

#undef NDEBUG
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static xmemdebugsnapshot before;
static double started;

void fixturePlainBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
    started = xrtTimer();
}

void fixturePlainEnd(int language)
{
    double elapsed = (xrtTimer() - started) * 1000.0;
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    printf("XUI_PLAIN_LANGUAGE_SAMPLE language=%d count=128 alloc=%" PRIu64
           " free=%" PRIu64 " realloc=%" PRIu64 " elapsedMs=%.6f\n", language,
           after.AllocCount - before.AllocCount, after.FreeCount - before.FreeCount,
           after.ReallocCount - before.ReallocCount, elapsed);
}

void fixturePlainNative(void)
{
    const xui_code_language_extension_t extensions[] = {{"p1"}, {"p2"}};
    const xui_code_plain_language_t desc = {
        .iSize = sizeof(desc), .sId = "sample", .sName = "Sample", .sLineComment = "//",
        .pExtensions = extensions, .iExtensionCount = 2, .iLexerType = XUI_CODE_LEXER_BUILTIN_C
    };
    xui_code_language_registry registry = NULL;
    assert(xuiCodeLanguageRegistryCreate(&registry) == XUI_OK);
    assert(xuiCodeLanguageRegistryRegisterPlain(registry, &desc) == XUI_OK);
    fixturePlainBegin();
    for (int i = 0; i < 128; ++i)
        assert(xuiCodeLanguageRegistryRegisterPlain(registry, &desc) == XUI_OK);
    fixturePlainEnd(0);

    char replacementId[] = "sample";
    char replacementExtension[] = "new";
    const xui_code_language_extension_t replacementExtensions[] = {{replacementExtension}};
    xui_code_plain_language_t replacement = desc;
    replacement.sId = replacementId;
    replacement.pExtensions = replacementExtensions;
    replacement.iExtensionCount = 1;
    int failures = 0;
    for (uint64 prefix = 0; ; ++prefix) {
        const char* extension = NULL;
        xmemdebugsnapshot beforeFailure, afterFailure;
        assert(prefix < 16);
        xrtMemDebugSnapshot(&beforeFailure);
        assert(xrtMemDebugFailAfter(prefix));
        int status = xuiCodeLanguageRegistryRegisterPlain(registry, &replacement);
        bool injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        xrtClearError();
        if (status == XUI_OK) {
            assert(!injected);
            break;
        }
        assert(injected && status == XUI_ERROR_OUT_OF_MEMORY);
        xrtMemDebugSnapshot(&afterFailure);
        assert(afterFailure.LiveCount == beforeFailure.LiveCount && afterFailure.LiveBytes == beforeFailure.LiveBytes);
        assert(xuiCodeLanguageRegistryGetExtension(registry, 0, 1, &extension) == XUI_OK);
        assert(strcmp(extension, "p2") == 0);
        ++failures;
    }
    assert(failures > 1);
    replacementId[0] = 'X';
    replacementExtension[0] = 'X';
    xui_code_language_t saved = {0};
    assert(xuiCodeLanguageRegistryFindByExtension(registry, "new", &saved) == XUI_OK);
    assert(strcmp(saved.sId, "sample") == 0);
    assert(xuiCodeLanguageRegistryDestroyChecked(registry) == XUI_OK);
    printf("XUI_PLAIN_LANGUAGE_OOM_PREFIXES=%d\n", failures);
}
