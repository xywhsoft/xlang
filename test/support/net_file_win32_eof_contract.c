/* Ordinary Win32 reference: keep every OVERLAPPED/buffer alive until dequeued.
 * Empty regular files are valid input; no sockets, crafted input or faulting
 * pointer is used. Establish the platform contract before adapting XRT. */
#include <windows.h>
#include <assert.h>
#include <stdio.h>
int main(void)
{
    const char* path="net_file_win32_eof_contract.tmp";
    HANDLE file=CreateFileA(path,GENERIC_READ|GENERIC_WRITE,0,NULL,CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED,NULL);assert(file!=INVALID_HANDLE_VALUE);
    HANDLE port=CreateIoCompletionPort(file,NULL,1,1);assert(port);
    OVERLAPPED overlapped={0};char byte=0;DWORD immediate=0;
    BOOL accepted=ReadFile(file,&byte,1,&immediate,&overlapped);
    DWORD initial=accepted?ERROR_SUCCESS:GetLastError();
    DWORD completed=197;ULONG_PTR key=0;OVERLAPPED* returned=NULL;
    BOOL ok=GetQueuedCompletionStatus(port,&completed,&key,&returned,3000);
    DWORD terminal=ok?ERROR_SUCCESS:GetLastError();
    printf("ReadFile(empty): accepted=%d initial=%lu; packet=%d terminal=%lu bytes=%lu key=%llu\n",
        (int)accepted,(unsigned long)initial,returned==&overlapped,(unsigned long)terminal,
        (unsigned long)completed,(unsigned long long)key);
    assert((accepted||initial==ERROR_IO_PENDING||initial==ERROR_HANDLE_EOF)&&
        returned==&overlapped&&key==1&&terminal==ERROR_HANDLE_EOF&&completed==0);
    assert(CloseHandle(file)&&CloseHandle(port)&&DeleteFileA(path));
    puts("Win32 EOF: the OVERLAPPED storage survives the actual EOF completion packet");
    return 0;
}
