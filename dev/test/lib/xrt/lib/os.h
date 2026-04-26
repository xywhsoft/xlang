


// 运行程序
XXAPI ptr xrtRun(str sPath, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		GetStartupInfoW(&si);
		si.wShowWindow = SW_SHOW;
		u16str sPathW = xrtUTF8to16(sPath, iSize);
		CreateProcessW(NULL, sPathW, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
		xrtFree(sPathW);
		return (ptr)pi.hProcess;
	#else
		pid_t pid = fork();
		if ( pid == 0 ) {
			execl("/bin/sh", "sh", "-c", sPath, (char*)NULL);
			// 如果这里继续执行，说明 execl 失败了
			return NULL;
		} else if (pid > 0) {
			return (ptr)(intptr_t)pid;
		} else {
			return NULL;
		}
	#endif
}



// 打开文件（ Windows 系统使用 ShellExecute，Linux 系统使用 xdg-open ）
XXAPI ptr xrtStart(str sPath, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		u16str sPathW = xrtUTF8to16(sPath, iSize);
		return (ptr)ShellExecuteW(0, NULL, sPathW, NULL, NULL, SW_SHOW);
		xrtFree(sPathW);
	#else
		pid_t pid = fork();
		if ( pid == 0 ) {
			execlp("xdg-open", "xdg-open", sPath, (char*)NULL);
			// 如果这里继续执行，说明 execlp 失败了
			return NULL;
		} else if (pid > 0) {
			return (ptr)(intptr_t)pid;
		} else {
			return NULL;
		}
	#endif
}



// 运行程序并等待程序运行结束
XXAPI int xrtChain(str sPath, size_t iSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		DWORD iRet = 0;
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		GetStartupInfoW(&si);
		si.wShowWindow = SW_SHOW;
		u16str sPathW = xrtUTF8to16(sPath, iSize);
		CreateProcessW(NULL, sPathW, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
		xrtFree(sPathW);
		WaitForSingleObject(pi.hProcess, INFINITE);
		GetExitCodeProcess(pi.hProcess, &iRet);
		return iRet;
	#else
		pid_t pid = fork();
		if ( pid == 0 ) {
			execl("/bin/sh", "sh", "-c", sPath, (char*)NULL);
			// 如果这里继续执行，说明 execl 失败了
			return -1;
		} else if ( pid > 0 ) {
			// 等待子进程运行结束
			int status;
			waitpid(pid, &status, 0);
			if ( WIFEXITED(status) ) {
				return WEXITSTATUS(status);
			} else {
				// 异常退出
				return -1;
			}
		} else {
			return -1;
		}
	#endif
}