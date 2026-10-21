#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define HANDLED_TEST_EXCEPTION 0xe0421234u

static LONG CALLBACK handled(EXCEPTION_POINTERS* p)
{
    return p->ExceptionRecord->ExceptionCode == HANDLED_TEST_EXCEPTION
        ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
}

static DWORD WINAPI threadExit(void* unused)
{
    (void)unused;
    return 23;
}

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    if(argc != 2) return 2;
    if(!strcmp(argv[1], "success")) { puts("native diagnostic fixture passed"); return 0; }
    if(!strcmp(argv[1], "exit")) return 17;
    if(!strcmp(argv[1], "handled")) {
        PVOID handler = AddVectoredExceptionHandler(1, handled);
        if(!handler) return 3;
        RaiseException(HANDLED_TEST_EXCEPTION, 0, 0, NULL);
        RemoveVectoredExceptionHandler(handler);
        return 0;
    }
    if(!strcmp(argv[1], "unhandled")) {
        ULONG_PTR information[2] = {0, 0x1234};
        RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 2, information);
        return 4;
    }
    if(!strcmp(argv[1], "breakpoint")) { DebugBreak(); return 5; }
    if(!strcmp(argv[1], "thread")) {
        HANDLE h = CreateThread(NULL, 0, threadExit, NULL, 0, NULL);
        DWORD code = 0;
        if(!h || WaitForSingleObject(h, 5000) != WAIT_OBJECT_0 ||
           !GetExitCodeThread(h, &code)) return 6;
        CloseHandle(h);
        return code == 23 ? 0 : 7;
    }
    if(!strcmp(argv[1], "timeout")) { Sleep(60000); return 8; }
    return 2;
}
