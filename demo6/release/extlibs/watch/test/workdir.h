#ifndef XLANG_WATCH_TEST_WORKDIR_H
#define XLANG_WATCH_TEST_WORKDIR_H

#include <stdio.h>

/* 固定相对路径夹具只在本次独占目录内创建和清理。 */
static int watch_test_workdir(int (*run)(void)) {
    str previous = xrtPathCwd();
    str temporary = previous != NULL ?
        xrtDirTemp(NULL, "xlang-watch-test-", "") : NULL;
    str workspace = temporary != NULL ? xrtPathAbs(temporary) : NULL;
    int result = 1;

    if (previous == NULL || workspace == NULL) {
        fprintf(stderr, "watch test: cannot create isolated directory\n");
        if (temporary != NULL) xrtDirRemoveAll(temporary);
    } else if (!xrtPathSetCwd(workspace)) {
        fprintf(stderr, "watch test: cannot enter isolated directory\n");
        xrtDirRemoveAll(workspace);
    } else {
        if (xrtDirCreateAll("build/test/watch_guard")) result = run();
        else fprintf(stderr, "watch test: cannot create fixture directory\n");
        if (!xrtPathSetCwd(previous)) {
            fprintf(stderr, "watch test: cannot restore work directory\n");
            /* 当前目录可能仍在临时目录内，不递归删除它。 */
            xrtFree(workspace);
            xrtFree(temporary);
            xrtFree(previous);
            return 1;
        }
        if (!xrtDirRemoveAll(workspace)) {
            fprintf(stderr, "watch test: cannot remove isolated directory\n");
            result = 1;
        }
    }
    xrtFree(workspace);
    xrtFree(temporary);
    xrtFree(previous);
    return result;
}

#endif
