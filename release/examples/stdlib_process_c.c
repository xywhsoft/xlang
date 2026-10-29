#define XRT_MODULE_PROCESS_PIPELINE
#define XRT_MODULE_STRING
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



/* 输出一项示例结果，失败时立即结束进程。 */
static void requireLine(cstr sName, bool bOk)
{
	if ( !bOk ) {
		fprintf(stderr, "%s=fail\n", sName);
		exit(1);
	}
	printf("%s=ok\n", sName);
}



/* 比较去除两侧 ASCII 空白后的字节视图。 */
static bool bytesTrimEqual(cbytes pData, size_t iSize, cstr sExpected)
{
	xstrview Actual = xrtStrTrim(xrtStrViewN((cstr)pData, iSize));

	return xrtStrEqual(Actual, xrtStrView(sExpected));
}



/* 判断进程状态是否为指定退出码。 */
static bool statusCode(const xprocessstatus* pStatus, int32 iCode)
{
	return (pStatus != NULL) &&
		(pStatus->Kind == XPROCESS_EXIT_CODE) &&
		(pStatus->Code == iCode);
}



/* 完整写入一个短缓冲，处理平台允许的部分写入。 */
static bool processWriteAll(
	xprocess* pProcess,
	cbytes pData,
	size_t iSize
)
{
	size_t iOffset = 0u;

	while ( iOffset < iSize ) {
		int64 iWritten = xrtProcessWrite(
			pProcess,
			pData + iOffset,
			iSize - iOffset
		);

		if ( iWritten <= 0 ) {
			return false;
		}
		iOffset += (size_t)iWritten;
	}
	return true;
}



/* 读取固定数量字节；提前 EOF 视为失败。 */
static bool processReadExact(
	xprocess* pProcess,
	xprocessstream Stream,
	bytes pData,
	size_t iSize
)
{
	size_t iOffset = 0u;

	while ( iOffset < iSize ) {
		int64 iRead = xrtProcessRead(
			pProcess,
			Stream,
			pData + iOffset,
			iSize - iOffset
		);

		if ( iRead <= 0 ) {
			return false;
		}
		iOffset += (size_t)iRead;
	}
	return true;
}



/* 读取短输出直到 EOF；缓冲区不足时稳定失败。 */
static bool processReadAll(
	xprocess* pProcess,
	xprocessstream Stream,
	bytes pData,
	size_t iCapacity,
	size_t* pSize
)
{
	size_t iSize = 0u;

	while ( iSize < iCapacity ) {
		int64 iRead = xrtProcessRead(
			pProcess,
			Stream,
			pData + iSize,
			iCapacity - iSize
		);

		if ( iRead < 0 ) {
			return false;
		}
		if ( iRead == 0 ) {
			*pSize = iSize;
			return true;
		}
		iSize += (size_t)iRead;
	}
	return false;
}





/* 验证 Shell 快捷函数、成功判断和非零退出码。 */
static void checkShell(void)
{
	xprocessresult Echo;
	xprocessresult Success;
	xprocessresult Failure;
	bool bOk;

	memset(&Echo, 0, sizeof(Echo));
	memset(&Success, 0, sizeof(Success));
	memset(&Failure, 0, sizeof(Failure));
	bOk = xrtProcessShell("cmd /c echo hello-process", &Echo) &&
		xrtProcessShell("cmd /c exit /b 0", &Success) &&
		xrtProcessShell("cmd /c exit /b 5", &Failure) &&
		bytesTrimEqual(Echo.Stdout, Echo.StdoutSize, "hello-process") &&
		xrtProcessResultSuccess(&Success) &&
		!xrtProcessResultSuccess(&Failure) &&
		statusCode(&Success.Status, 0) &&
		statusCode(&Failure.Status, 5);
	xrtProcessResultUnit(&Echo);
	xrtProcessResultUnit(&Success);
	xrtProcessResultUnit(&Failure);
	requireLine("shell", bOk);
}



/* 验证默认有界捕获结果。 */
static void checkRun(void)
{
	xprocessresult Result;
	bool bOk;

	memset(&Result, 0, sizeof(Result));
	bOk = xrtProcessShell("cmd /c echo process-run", &Result) &&
		xrtProcessResultSuccess(&Result) &&
		statusCode(&Result.Status, 0) &&
		(Result.Wait == XWAIT_OK) &&
		!Result.StdoutTruncated &&
		!Result.StderrTruncated &&
		bytesTrimEqual(Result.Stdout, Result.StdoutSize, "process-run") &&
		bytesTrimEqual(Result.Stderr, Result.StderrSize, "");
	xrtProcessResultUnit(&Result);
	requireLine("run", bOk);
}



/* 验证工作配置与结构化环境变量。 */
static void checkConfigured(void)
{
	const xprocessenv pEnv[] = {
		{ "XL6_PROCESS_EXAMPLE", "configured" }
	};
	xprocessconfig Config;
	xprocessresult Result;
	bool bOk;

	memset(&Result, 0, sizeof(Result));
	bOk = xrtProcessShellConfigInit(
		&Config,
		"cmd /c echo %XL6_PROCESS_EXAMPLE%"
	);
	Config.Env = pEnv;
	Config.EnvCount = 1u;
	bOk = bOk && xrtProcessRun(&Config, NULL, &Result) &&
		xrtProcessResultSuccess(&Result) &&
		bytesTrimEqual(Result.Stdout, Result.StdoutSize, "configured");
	xrtProcessResultUnit(&Result);
	requireLine("configured", bOk);
}



/* 验证无需 Shell 的直接程序执行。 */
static void checkRunFile(void)
{
	const cstr pArgs[] = { "/c", "echo", "process-file" };
	xprocessresult Result;
	bool bOk;

	memset(&Result, 0, sizeof(Result));
	bOk = xrtProcessCapture("cmd", pArgs, 3u, &Result) &&
		xrtProcessResultSuccess(&Result) &&
		bytesTrimEqual(Result.Stdout, Result.StdoutSize, "process-file");
	xrtProcessResultUnit(&Result);
	requireLine("runFile", bOk);
}



/* 验证真实 OS 管道和逐阶段退出状态。 */
static void checkPipeline(void)
{
	xprocessconfig pStages[2];
	xprocesspipelineresult Result;
	bool bOk;

	memset(&Result, 0, sizeof(Result));
	bOk = xrtProcessShellConfigInit(
		&pStages[0],
		"cmd /c echo alpha"
	) && xrtProcessShellConfigInit(
		&pStages[1],
		"cmd /c findstr alpha"
	) && xrtProcessPipeline(pStages, 2u, NULL, &Result) &&
		xrtProcessPipelineSuccess(&Result) &&
		(Result.StageCount == 2u) &&
		statusCode(&Result.Stages[0].Status, 0) &&
		statusCode(&Result.Stages[1].Status, 0) &&
		bytesTrimEqual(Result.Stdout, Result.StdoutSize, "alpha");
	xrtProcessPipelineResultUnit(&Result);
	requireLine("pipeline", bOk);
}



/* 验证非零阶段不会改变末段退出码语义。 */
static void checkPipelineFailure(void)
{
	xprocessconfig pStages[2];
	xprocesspipelineresult Result;
	bool bOk;

	memset(&Result, 0, sizeof(Result));
	bOk = xrtProcessShellConfigInit(
		&pStages[0],
		"cmd /c exit /b 5"
	) && xrtProcessShellConfigInit(
		&pStages[1],
		"cmd /c echo should-not-run"
	) && xrtProcessPipeline(pStages, 2u, NULL, &Result) &&
		!xrtProcessPipelineSuccess(&Result) &&
		(Result.StageCount == 2u) &&
		statusCode(&Result.Stages[0].Status, 5) &&
		statusCode(&Result.Stages[1].Status, 0);
	xrtProcessPipelineResultUnit(&Result);
	requireLine("pipelineFail", bOk);
}



/* 验证低级 stdin/stdout 流式进程接口。 */
static bool checkInteractiveOne(cstr sCommand, cstr sText)
{
	xprocessconfig Config;
	xprocessstatus Status;
	xprocess* pProcess;
	unsigned char pOutput[256];
	size_t iOutputSize = 0u;
	bool bOk;

	if ( !xrtProcessShellConfigInit(&Config, sCommand) ) {
		return false;
	}
	Config.Stdin.Mode = XPROCESS_IO_PIPE;
	Config.Stdout.Mode = XPROCESS_IO_PIPE;
	Config.Stderr.Mode = XPROCESS_IO_NULL;
	pProcess = xrtProcessSpawn(&Config);
	if ( pProcess == NULL ) {
		return false;
	}
	bOk = (xrtProcessState(pProcess) == XPROCESS_RUNNING) &&
		processWriteAll(pProcess, (cbytes)sText, strlen(sText)) &&
		processWriteAll(pProcess, (cbytes)"\n", 1u) &&
		xrtProcessClose(pProcess, XPROCESS_STDIN) &&
		(xrtProcessWaitFor(pProcess, UINT64_C(5000000)) == XWAIT_OK) &&
		(xrtProcessState(pProcess) == XPROCESS_EXITED) &&
		xrtProcessStatus(pProcess, &Status) &&
		statusCode(&Status, 0) &&
		processReadAll(
			pProcess,
			XPROCESS_STDOUT,
			pOutput,
			sizeof(pOutput),
			&iOutputSize
		) &&
		bytesTrimEqual(pOutput, iOutputSize, sText);
	xrtProcessDestroy(pProcess);
	return bOk;
}



/* 验证直接进程对象可以按调用方需要控制读写和等待。 */
static void checkProcess(void)
{
	requireLine(
		"process",
		checkInteractiveOne("cmd /c more", "live-process") &&
		checkInteractiveOne("cmd /c more", "shell-process")
	);
}



/* 验证调用方可以先读固定前缀，再读取剩余双流内容。 */
static void checkIncremental(void)
{
	xprocessconfig Config;
	xprocessstatus Status;
	xprocess* pProcess;
	unsigned char pPrefix[4];
	unsigned char pStdout[256];
	unsigned char pStderr[256];
	size_t iStdoutSize = 0u;
	size_t iStderrSize = 0u;
	bool bOk;

	bOk = xrtProcessShellConfigInit(
		&Config,
		"cmd /c echo incremental-out ^& echo incremental-err 1^>^&2"
	);
	Config.Stdout.Mode = XPROCESS_IO_PIPE;
	Config.Stderr.Mode = XPROCESS_IO_PIPE;
	pProcess = bOk ? xrtProcessSpawn(&Config) : NULL;
	if ( pProcess == NULL ) {
		requireLine("incremental", false);
		return;
	}
	bOk = (xrtProcessWaitFor(pProcess, UINT64_C(5000000)) == XWAIT_OK) &&
		processReadExact(
			pProcess,
			XPROCESS_STDOUT,
			pPrefix,
			sizeof(pPrefix)
		) &&
		processReadAll(
			pProcess,
			XPROCESS_STDOUT,
			pStdout,
			sizeof(pStdout),
			&iStdoutSize
		) &&
		processReadAll(
			pProcess,
			XPROCESS_STDERR,
			pStderr,
			sizeof(pStderr),
			&iStderrSize
		) &&
		(memcmp(pPrefix, "incr", sizeof(pPrefix)) == 0) &&
		(iStdoutSize >= 11u) &&
		(memcmp(pStdout, "emental-out", 11u) == 0) &&
		bytesTrimEqual(pStderr, iStderrSize, "incremental-err") &&
		xrtProcessStatus(pProcess, &Status) &&
		statusCode(&Status, 0);
	xrtProcessDestroy(pProcess);
	requireLine("incremental", bOk);
}





/* 依次运行与 xlang 示例相同的 Process 场景。 */
int main(void)
{
	checkShell();
	checkRun();
	checkConfigured();
	checkRunFile();
	checkPipeline();
	checkPipelineFailure();
	checkProcess();
	checkIncremental();
	return 0;
}
