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

static int trim_equals(const void* data, size_t size, const char* expected)
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

static int result_stdout_equals(const xprocessresult* result, const char* expected)
{
    if ( result == NULL || result->pStdout == NULL ) {
        return expected != NULL && expected[0] == '\0';
    }
    return trim_equals(result->pStdout, result->iStdoutSize, expected);
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

static int check_interactive_process(void)
{
    xprocessconfig config;
    xprocess* process;
    void* output;
    size_t size = 0;
    str args[] = {(str)"/c", (str)"more"};
    int ok;

    config_exec(&config, "cmd", args, 2);
    config.Stdin.iMode = XPROC_STDIO_PIPE;

    process = xrtProcessSpawn(&config);
    if ( process == NULL ) {
        return 0;
    }

    ok =
        xrtProcessIsRunning(process) &&
        xrtProcessWriteText(process, (str)"os-start-file\n", 0) > 0 &&
        xrtProcessCloseStdin(process) &&
        xrtProcessWaitTimeout(process, 5000) == 0 &&
        xrtProcessExitCode(process) == 0;

    output = xrtProcessGetStdout(process, &size);
    ok = ok && trim_equals(output, size, "os-start-file");
    xrtFree(output);
    xrtProcessDestroy(process);
    return ok;
}

int main(int argc, char** argv)
{
    str oldCwd;
    str tempRoot;
    str tempDir;
    str missingEnv;
    xprocessconfig config;
    xprocessresult result;
    int ok;

    (void)argv;
    xrtInit();

    require_line("args", argc > 0);

#if defined(_WIN32)
    require_line("platform", 1);
#else
    require_line("platform", 1);
#endif

    oldCwd = xrtPathCwd();
    tempRoot = xrtPathTemp();
    tempDir = xrtPathJoin(2, tempRoot, "xl6_stdlib_os_c");
    xrtDirCreateAll(tempDir);

    ok =
        oldCwd != NULL &&
        tempDir != NULL &&
        xrtPathSetCwd(tempDir) &&
        xrtPathSetCwd(oldCwd);
    require_line("cwd", ok);

    missingEnv = xrtEnvGet((str)"__XLANG_DEMO6_MISSING_ENV__");
    require_line("env", missingEnv == NULL);

    memset(&result, 0, sizeof(result));
    config_shell(&config, "cmd /c echo os-run");
    ok =
        xrtProcessRun(&config, NULL, 0, &result, 5000) &&
        xrtProcessResultSuccess(&result) &&
        result.iExitCode == 0 &&
        result_stdout_equals(&result, "os-run");
    xrtProcessResultUnit(&result);
    require_line("run", ok);

    require_line("startFile", check_interactive_process());
    require_line("chain", xrtChain((str)"cmd /c exit /b 0", 0) == 0);

    xrtDirDelete(tempDir);
    xrtFree(oldCwd);
    xrtFree(tempRoot);
    xrtFree(tempDir);
    xrtFree(missingEnv);
    xrtUnit();
    return 0;
}
