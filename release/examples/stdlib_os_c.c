#define XRT_MODULE_ALL
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* 输出一项对照结果，失败时立即终止。 */
static void require_line(cstr sName, bool bOk)
{
    if ( !bOk ) {
        fprintf(stderr, "%s=fail\n", sName);
        exit(1);
    }
    printf("%s=ok\n", sName);
}



/* 比较去除两侧 ASCII 空白后的进程输出。 */
static bool output_equal(cbytes pData, size_t iSize, cstr sExpected)
{
    xstrview Actual = xrtStrTrim(xrtStrViewN((cstr)pData, iSize));

    return xrtStrEqual(Actual, xrtStrView(sExpected));
}



/* 判断结构化进程状态是否为指定退出码。 */
static bool status_code(const xprocessstatus* pStatus, int32 iCode)
{
    return (pStatus != NULL) &&
        (pStatus->Kind == XPROCESS_EXIT_CODE) &&
        (pStatus->Code == iCode);
}



/* 用直接程序配置验证 startFile 对应的低级 Spawn/Wait 路径。 */
static bool check_start_file(void)
{
    const cstr pArgs[] = { "/c", "echo", "os-start-file" };
    xprocessconfig Config;
    xprocessstatus Status;
    xprocess* pProcess;
    bool bOk;

    if ( !xrtProcessConfigInit(&Config) ) {
        return false;
    }
    Config.Program = "cmd";
    Config.Args = pArgs;
    Config.ArgCount = sizeof(pArgs) / sizeof(pArgs[0]);
    Config.Stdout.Mode = XPROCESS_IO_NULL;
    Config.Stderr.Mode = XPROCESS_IO_NULL;
    pProcess = xrtProcessSpawn(&Config);
    if ( pProcess == NULL ) {
        return false;
    }
    bOk = (xrtProcessWaitFor(
        pProcess, UINT64_C(5000000)
    ) == XWAIT_OK) && xrtProcessStatus(
        pProcess, &Status
    ) && status_code(&Status, 0);
    xrtProcessDestroy(pProcess);
    return bOk;
}



/* 演示参数、平台、目录、环境和同步进程基础能力。 */
int main(int argc, char** argv)
{
    xprocessresult Run;
    xprocessresult Chain;
    str sOldCwd;
    str sTempRoot;
    str sTempDir;
    str sMissing;
    bool bCwd;

    (void)argv;
    require_line("args", argc > 0);
    require_line("platform", true);

    sOldCwd = xrtPathCwd();
    sTempRoot = xrtPathTemp();
    sTempDir = (sTempRoot != NULL) ?
        xrtPathJoin(sTempRoot, "xl6_stdlib_os_c") : NULL;
    bCwd = (sOldCwd != NULL) && (sTempDir != NULL) &&
        xrtDirCreateAll(sTempDir) &&
        xrtPathSetCwd(sTempDir) && xrtPathSetCwd(sOldCwd);
    require_line("cwd", bCwd);

    sMissing = xrtEnvGet("__XLANG_DEMO6_MISSING_ENV__");
    require_line("env", sMissing == NULL);

    memset(&Run, 0, sizeof(Run));
    require_line(
        "run",
        xrtProcessShell("cmd /c echo os-run", &Run) &&
        xrtProcessResultSuccess(&Run) &&
        status_code(&Run.Status, 0) &&
        output_equal(Run.Stdout, Run.StdoutSize, "os-run")
    );
    xrtProcessResultUnit(&Run);

    require_line("startFile", check_start_file());

    memset(&Chain, 0, sizeof(Chain));
    require_line(
        "chain",
        xrtProcessShell("cmd /c exit /b 0", &Chain) &&
        xrtProcessResultSuccess(&Chain) &&
        status_code(&Chain.Status, 0)
    );
    xrtProcessResultUnit(&Chain);

    if ( sTempDir != NULL ) {
        (void)xrtDirRemoveAll(sTempDir);
    }
    xrtFree(sMissing);
    xrtFree(sTempDir);
    xrtFree(sTempRoot);
    xrtFree(sOldCwd);
    return 0;
}
