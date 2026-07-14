#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_line(const char* name, int ok)
{
    if ( !ok ) {
        fprintf(stderr, "%s=fail\n", name);
        exit(1);
    }
    printf("%s=ok\n", name);
}

static int result_stdout_trim_equals(const xprocessresult* result, const char* expected)
{
    str trimmed;
    int ok;

    if ( result == NULL || result->pStdout == NULL ) {
        return expected != NULL && expected[0] == '\0';
    }

    trimmed = xrtTrim((str)result->pStdout, result->iStdoutSize, NULL, 0, FALSE, NULL);
    if ( trimmed == NULL ) {
        return 0;
    }
    ok = xrtStrComp(trimmed, (str)expected, 0, FALSE) == 0;
    xrtFree(trimmed);
    return ok;
}

static int bytes_trim_equals(const void* data, size_t size, const char* expected)
{
    str trimmed;
    int ok;

    if ( data == NULL ) {
        return expected != NULL && expected[0] == '\0';
    }

    trimmed = xrtTrim((str)data, size, NULL, 0, FALSE, NULL);
    if ( trimmed == NULL ) {
        return 0;
    }
    ok = xrtStrComp(trimmed, (str)expected, 0, FALSE) == 0;
    xrtFree(trimmed);
    return ok;
}

static int result_stderr_trim_equals(const xprocessresult* result, const char* expected)
{
    str trimmed;
    int ok;

    if ( result == NULL || result->pStderr == NULL ) {
        return expected != NULL && expected[0] == '\0';
    }

    trimmed = xrtTrim((str)result->pStderr, result->iStderrSize, NULL, 0, FALSE, NULL);
    if ( trimmed == NULL ) {
        return 0;
    }
    ok = xrtStrComp(trimmed, (str)expected, 0, FALSE) == 0;
    xrtFree(trimmed);
    return ok;
}

static void config_shell(xprocessconfig* config, const char* command)
{
    xrtProcessConfigInit(config);
    config->iTargetKind = XPROC_TARGET_SHELL;
    config->sCommand = (str)command;
    config->Stdout.iMode = XPROC_STDIO_PIPE;
    config->Stderr.iMode = XPROC_STDIO_PIPE;
}

static void config_exec(xprocessconfig* config, const char* program, str* args, uint32 count)
{
    xrtProcessConfigInit(config);
    config->iTargetKind = XPROC_TARGET_EXEC;
    config->sProgram = (str)program;
    config->arrArgs = args;
    config->iArgCount = count;
    config->Stdout.iMode = XPROC_STDIO_PIPE;
    config->Stderr.iMode = XPROC_STDIO_PIPE;
}

static int run_shell_result(const char* command, xprocessresult* result)
{
    xprocessconfig config;

    config_shell(&config, command);
    return xrtProcessRun(&config, NULL, 0, result, 5000);
}

static int run_exec_result(const char* program, str* args, uint32 count, xprocessresult* result)
{
    xprocessconfig config;

    config_exec(&config, program, args, count);
    return xrtProcessRun(&config, NULL, 0, result, 5000);
}

static void check_legacy(void)
{
    xprocessresult echoResult;
    xprocessresult okResult;
    xprocessresult failResult;
    int ok;

    memset(&echoResult, 0, sizeof(echoResult));
    memset(&okResult, 0, sizeof(okResult));
    memset(&failResult, 0, sizeof(failResult));

    ok =
        run_shell_result("cmd /c echo hello-process", &echoResult) &&
        run_shell_result("cmd /c exit /b 0", &okResult) &&
        run_shell_result("cmd /c exit /b 5", &failResult) &&
        result_stdout_trim_equals(&echoResult, "hello-process") &&
        xrtProcessResultSuccess(&okResult) &&
        !xrtProcessResultSuccess(&failResult) &&
        okResult.iExitCode == 0 &&
        failResult.iExitCode == 5;

    xrtProcessResultUnit(&echoResult);
    xrtProcessResultUnit(&okResult);
    xrtProcessResultUnit(&failResult);
    require_line("legacy", ok);
}

static void check_run(void)
{
    xprocessresult result;
    int ok;

    memset(&result, 0, sizeof(result));
    ok =
        run_shell_result("cmd /c echo process-run", &result) &&
        xrtProcessResultSuccess(&result) &&
        result.iExitCode == 0 &&
        !result.ExitInfo.bTimedOut &&
        result.ExitInfo.iKind != XPROC_EXIT_SPAWN_FAILED &&
        result_stdout_trim_equals(&result, "process-run") &&
        result_stderr_trim_equals(&result, "");
    xrtProcessResultUnit(&result);
    require_line("run", ok);
}

static void check_configured(void)
{
    xprocessconfig config;
    xprocessresult result;
    str env[] = {(str)"XL6_PROCESS_EXAMPLE=configured"};
    int ok;

    memset(&result, 0, sizeof(result));
    config_shell(&config, "cmd /c echo %XL6_PROCESS_EXAMPLE%");
    config.arrEnv = env;
    config.iEnvCount = 1;
    config.bInheritEnv = true;
    config.iReadChunkSize = 1024;
    config.iMaxEventCount = 64;
    ok =
        xrtProcessRun(&config, NULL, 0, &result, 5000) &&
        xrtProcessResultSuccess(&result) &&
        result_stdout_trim_equals(&result, "configured") &&
        result.ExitInfo.iExitCode == 0;
    xrtProcessResultUnit(&result);
    require_line("configured", ok);
}

static void check_run_file(void)
{
    xprocessresult result;
    str args[] = {(str)"/c", (str)"echo", (str)"process-file"};
    int ok;

    memset(&result, 0, sizeof(result));
    ok =
        run_exec_result("cmd", args, 3, &result) &&
        xrtProcessResultSuccess(&result) &&
        result.iExitCode == 0 &&
        result_stdout_trim_equals(&result, "process-file");
    xrtProcessResultUnit(&result);
    require_line("runFile", ok);
}

static void check_pipeline(void)
{
    xprocessconfig configs[2];
    xprocesspipelineresult result;
    str args[] = {(str)"/c", (str)"findstr alpha"};
    int ok;

    memset(&result, 0, sizeof(result));
    config_shell(&configs[0], "cmd /c echo alpha");
    config_exec(&configs[1], "cmd", args, 2);

    ok =
        xrtProcessPipelineRun(configs, 2, &result, 5000) &&
        result.bSuccess &&
        result.iExitCode == 0 &&
        result.iFailedIndex == -1 &&
        result.iResultCount == 2 &&
        result.iCompletedCount == 2 &&
        result_stdout_trim_equals((const xprocessresult*)&result.arrResults[1], "alpha") &&
        bytes_trim_equals(result.pStdout, result.iStdoutSize, "alpha");
    xrtProcessPipelineResultUnit(&result);
    require_line("pipeline", ok);
}

static void check_pipeline_fail(void)
{
    xprocessconfig configs[2];
    xprocesspipelineresult result;
    int ok;

    memset(&result, 0, sizeof(result));
    config_shell(&configs[0], "cmd /c exit /b 5");
    config_shell(&configs[1], "cmd /c echo should-not-run");

    ok =
        xrtProcessPipelineRun(configs, 2, &result, 5000) &&
        !result.bSuccess &&
        result.iExitCode == 5 &&
        result.iFailedIndex == 0 &&
        result.iResultCount == 2 &&
        result.iCompletedCount == 1 &&
        result.arrResults[0].iExitCode == 5;
    xrtProcessPipelineResultUnit(&result);
    require_line("pipelineFail", ok);
}

static int check_interactive_process(xprocessconfig* config, const char* text)
{
    xprocess* process;
    void* output;
    size_t size = 0;
    str trimmed;
    int ok;

    config->Stdin.iMode = XPROC_STDIO_PIPE;

    process = xrtProcessSpawn(config);
    if ( process == NULL ) {
        return 0;
    }

    ok =
        xrtProcessIsRunning(process) &&
        xrtProcessWriteText(process, (str)text, 0) > 0 &&
        xrtProcessWriteText(process, (str)"\n", 0) > 0 &&
        xrtProcessCloseStdin(process) &&
        xrtProcessWaitTimeout(process, 5000) == 0 &&
        !xrtProcessIsRunning(process) &&
        xrtProcessExitCode(process) == 0;

    output = xrtProcessGetStdout(process, &size);
    trimmed = output != NULL ? xrtTrim((str)output, size, NULL, 0, FALSE, NULL) : NULL;
    ok = ok && trimmed != NULL && xrtStrComp(trimmed, (str)text, 0, FALSE) == 0;
    xrtFree(trimmed);
    xrtFree(output);
    xrtProcessDestroy(process);
    return ok;
}

static void check_process(void)
{
    xprocessconfig fileConfig;
    xprocessconfig shellConfig;
    str args[] = {(str)"/c", (str)"more"};

    config_exec(&fileConfig, "cmd", args, 2);
    config_shell(&shellConfig, "cmd /c more");

    require_line("process",
        check_interactive_process(&fileConfig, "live-process") &&
        check_interactive_process(&shellConfig, "shell-process"));
}

static void check_incremental(void)
{
    xprocessconfig config;
    xprocess* process;
    xprocessreadinfo outInfo;
    xprocessreadinfo errInfo;
    xprocesseventreadinfo eventInfo;
    xprocessexitinfo exitInfo;
    xprocessevent* events;
    void* out;
    void* err;
    size_t outSize = 0;
    size_t errSize = 0;
    uint32 eventCount = 0;
    int ok;

    config_shell(&config, "cmd /c echo incremental-out ^& echo incremental-err 1^>^&2");
    process = xrtProcessSpawn(&config);
    if ( process == NULL ) {
        require_line("incremental", 0);
        return;
    }

    ok = xrtProcessWaitTimeout(process, 5000) == 0;
    out = xrtProcessReadStdoutSince(process, 0, 4, &outSize, &outInfo);
    err = xrtProcessReadStderrSince(process, 0, 0, &errSize, &errInfo);
    events = xrtProcessReadEventsSince(process, 0, 0, &eventCount, &eventInfo);
    ok =
        ok &&
        out != NULL && outSize == 4 && memcmp(out, "incr", 4) == 0 &&
        outInfo.iNextOffset == 4 &&
        bytes_trim_equals(err, errSize, "incremental-err") &&
        eventInfo.iEventEndSeq >= eventInfo.iNextSeq &&
        eventInfo.bDone &&
        xrtProcessGetExitInfo(process, &exitInfo) &&
        exitInfo.iExitCode == 0;

    xrtFree(out);
    xrtFree(err);
    xrtFree(events);
    xrtProcessDestroy(process);
    require_line("incremental", ok);
}

int main(void)
{
    xrtInit();

    check_legacy();
    check_run();
    check_configured();
    check_run_file();
    check_pipeline();
    check_pipeline_fail();
    check_process();
    check_incremental();

    xrtUnit();
    return 0;
}
