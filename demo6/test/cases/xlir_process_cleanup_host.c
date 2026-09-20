/* Execute real shell cases, then inject timeout/infrastructure failure with
 * owned partial output. Every result must pass through the public destructor. */
#include <__xl_runtime/xruntime_decl.h>
#include <stdio.h>
#include <string.h>

static unsigned probeRuns, probeUnits, probeLive;
static int probeMode, probeFailed;
static bool probeProcessRun(const xprocessconfig* config,
    const xprocessrunoptions* options, xprocessresult* result)
{
    ++probeRuns;
    if (probeLive++ != 0 || !config->HideWindow ||
        config->Stdin.Mode != XPROCESS_IO_PIPE || config->Stdout.Mode != XPROCESS_IO_PIPE ||
        config->Stderr.Mode != XPROCESS_IO_PIPE || options->Input.Size != 0) probeFailed = 1;
    if (!probeMode) return xrtProcessRun(config, options, result);
    if (options->Deadline == XRT_DEADLINE_NEVER) probeFailed = 1;
    memset(result, 0, sizeof(*result));
    result->Stdout = (unsigned char*)xrtMalloc(7);
    result->Stderr = (unsigned char*)xrtMalloc(10);
    if (result->Stdout == NULL || result->Stderr == NULL) { probeFailed = 1; return false; }
    memcpy(result->Stdout, "partial", 7); result->StdoutSize = 7;
    memcpy(result->Stderr, "diagnostic", 10); result->StderrSize = 10;
    result->Wait = XWAIT_TIMEOUT;
    return probeMode == 2;
}

static void probeProcessUnit(xprocessresult* result)
{
    ++probeUnits;
    if (probeLive != 1) probeFailed = 1;
    probeLive = 0;
    xrtProcessResultUnit(result);
}

#define xrtProcessRun probeProcessRun
#define xrtProcessResultUnit probeProcessUnit
#define main probeModuleMain
#include "test/cases/xlir_generated_module_host.c"
#undef main
#undef xrtProcessResultUnit
#undef xrtProcessRun

int main(void)
{
    int result = probeModuleMain();
    if (result != 0) return result;
    for (probeMode = 1; probeMode <= 2; ++probeMode) {
#if defined(XLIR_TEST_PROCESS_EXEC)
        char* text = (char*)xl6_xrt_process_exec("injected", 9);
        if (text == NULL || strcmp(text, "partial") != 0) probeFailed = 1;
        xrtFree(text);
#elif defined(XLIR_TEST_PROCESS_SUCCESS)
        if (xl6_xrt_process_success("injected", 9)) probeFailed = 1;
#elif defined(XLIR_TEST_PROCESS_EXIT_CODE)
        if (xl6_xrt_process_exit_code("injected", 9) != -1) probeFailed = 1;
#else
        #error "Select the process operation to verify"
#endif
    }
    if (probeFailed || probeLive != 0 || probeRuns < 4 || probeUnits != probeRuns) {
        fprintf(stderr, "process result ownership/configuration mismatch: runs=%u units=%u live=%u\n",
            probeRuns, probeUnits, probeLive);
        return 1;
    }
    return 0;
}
