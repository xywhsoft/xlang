/* Compiled with the actual emitted C preincluded, not with a copied bridge. */
#include "xl.h"
#include <assert.h>
#include <stdio.h>

extern void tcc_vfs_clear_dynamic(void);

static void same_memory(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
}

static int mutate_arguments(int count, char** arguments)
{
    assert(count == 2 && arguments && !arguments[count]);
    assert(!strcmp(arguments[0], "same") && !strcmp(arguments[1], "same"));
    arguments[0][0] = 'z';
    assert(!strcmp(arguments[1], "same"));
    return 42;
}

static void arguments_contract(void)
{
    int count = -1; char** argv = NULL;
    assert(xlir_tcc_arguments(NULL, &count, &argv) && count == 0 && !argv);
    xvalue* args = xrtValueArray();
    xvalue* text = xrtValueString((xstrview){"same", 4});
    assert(args && text && xrtValueArrayAppend(args, text) && xrtValueArrayAppend(args, text));
    assert(xlir_tcc_invoke(mutate_arguments, args) == 42);
    xstrview original = {0}; assert(xrtValueGetString(text, &original));
    assert(original.Size == 4 && !memcmp(original.Data, "same", 4));
    size_t point;
    for (point = 0; point < 64; ++point) {
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(point));
        int ok = xlir_tcc_arguments(args, &count, &argv);
        int hit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if (!ok) assert(!argv && count == 0);
        else assert(count == 2 && argv && !argv[count]);
        xrtFree(argv); argv = NULL; xrtClearError(); same_memory(&before);
        assert(xlir_tcc_invoke(mutate_arguments, args) == 42);
        if (!hit) { assert(ok); break; }
    }
    assert(point < 64);
    printf("tcc arguments: %zu allocation prefixes; writable independent copies and balanced retry\n", point + 1);
    assert(xrtValueArraySetNew(args, 0, xrtValueString((xstrview){"a\0b", 3})));
    assert(!xlir_tcc_arguments(args, &count, &argv) && !argv && count == 0);
    xrtClearError();
    assert(xrtValueArraySetNew(args, 0, xrtValueInt(123)));
    assert(!xlir_tcc_arguments(args, &count, &argv) && !argv && count == 0);
    xrtClearError(); xrtValueRelease(text); xrtValueRelease(args);
}

static void handle_contract(int handler)
{
    const XL6_NativeHandlePolicyV1* policy = handler ?
        xl6TccHandlerHandlePolicyV1Get() : xl6TccContextHandlePolicyV1Get();
    size_t point;
    for (point = 0; point < 64; ++point) {
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
        void* slot = handler ? xl6_tcc_handler_create("__xlvfs__/tcc_handle.c") : xl6_tcc_context_create();
        assert(slot);
        assert(xrtMemDebugFailAfter(point));
        xvalue* value = xl6NativeHandleTakeV1(&slot, policy);
        int hit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        assert(!slot);
        if (!hit) assert(value);
        xrtClearError(); xrtValueRelease(value); xrtClearError(); same_memory(&before);
        if (!hit) break;
    }
    assert(point < 64);
    printf("tcc %s: %zu ownership-factory allocation prefixes; real payload cleanup\n",
        handler ? "handler" : "context", point + 1);
}

int main(int argc, char** argv)
{
    assert(argc == 3 && xrtMemDebugEnable(true));
    XL_ContextOptions options = xlDefaultContextOptions();
    options.workDir = "."; options.tccDllPath = argv[1]; options.tccLibPath = argv[2];
    XL_Context* anchor = xlCreateContext(&options); assert(anchor);
    /* Context creation is lazy; load the selected real TCC provider first. */
    assert(xlRunCSourceString(anchor, "int main(void){return 0;}", 0, NULL));
    for (int repeat = 0; repeat < 3; ++repeat) {
        assert(xlir_module_init()); xlir_module_fini(); assert(!xrtGetError());
    }
    arguments_contract();
    assert(xl6_tcc_vfs_mount_text("__xlvfs__/tcc_handle.c", "int main(void){return 42;}"));
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(0));
    assert(!xl6_tcc_handler_create("__xlvfs__/tcc_handle.c") && xrtGetError());
    assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); xrtClearError(); same_memory(&before);
    handle_contract(0); handle_contract(1); tcc_vfs_clear_dynamic();
    xlDestroyContext(anchor);
    XL_RuntimeDrainResult drained = xlRuntimeDrainRetiringChecked(-1);
    assert(drained.ready && drained.ok && !xrtGetError());
    puts("tcc generated bridge: init/fini, arguments, OOM, resident retirement passed");
    return 0;
}
