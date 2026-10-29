#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "../op_native.c"

static DWORD WINAPI blocked_load(void *data)
{
    WaitForSingleObject((HANDLE)data, INFINITE);
    return 1;
}

static DWORD WINAPI blocked_failure(void *data)
{
    WaitForSingleObject((HANDLE)data, INFINITE);
    return 0;
}

static int allocated(void *address)
{
    MEMORY_BASIC_INFORMATION info;
    return VirtualQuery(address, &info, sizeof(info)) == sizeof(info) &&
           info.State == MEM_COMMIT;
}

static int start_load(op_context *ctx, HANDLE event, void **remote, int succeeds)
{
    op_hook_state *hook = &ctx->hook;
    hook->target_process = OpenProcess(SYNCHRONIZE | PROCESS_VM_OPERATION,
                                        FALSE, GetCurrentProcessId());
    if (!hook->target_process) return 0;
    hook->remote_path = VirtualAllocEx(hook->target_process, NULL, 4096u,
                                       MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!hook->remote_path) return 0;
    *remote = hook->remote_path;
    hook->load_thread = CreateThread(NULL, 0,
                                     succeeds ? blocked_load : blocked_failure,
                                     event, 0, NULL);
    return hook->load_thread != NULL;
}

int main(void)
{
    op_context ctx = {0};
    HANDLE event = CreateEventW(NULL, TRUE, FALSE, NULL);
    HANDLE observed_thread = NULL;
    void *remote = NULL;
    int stage = 1, ok = 0;
    if (!event || !start_load(&ctx, event, &remote, 1)) goto done;
    stage = 2;
    if (op_hook_inject(&ctx, "d3d9") || !ctx.hook.load_thread ||
        !allocated(remote)) goto done;
    if (!SetEvent(event) || WaitForSingleObject(ctx.hook.load_thread, 2000u) != WAIT_OBJECT_0)
        goto done;
    stage = 3;
    if (!op_hook_inject(&ctx, "d3d9") || ctx.hook.load_thread ||
        ctx.hook.remote_path || !ctx.hook.injected || allocated(remote)) goto done;
    op_hook_cleanup(&ctx);
    CloseHandle(event);
    event = CreateEventW(NULL, TRUE, FALSE, NULL);
    remote = NULL;
    stage = 4;
    if (!event || !start_load(&ctx, event, &remote, 0)) goto done;
    if (!SetEvent(event) ||
        WaitForSingleObject(ctx.hook.load_thread, 2000u) != WAIT_OBJECT_0)
        goto done;
    if (op_hook_inject(&ctx, "d3d9") || ctx.hook.injected ||
        ctx.hook.target_process || ctx.hook.load_thread ||
        ctx.hook.remote_path || allocated(remote)) goto done;
    CloseHandle(event);
    event = CreateEventW(NULL, TRUE, FALSE, NULL);
    remote = NULL;
    stage = 5;
    if (!event || !start_load(&ctx, event, &remote, 1)) goto done;
    if (!DuplicateHandle(GetCurrentProcess(), ctx.hook.load_thread,
                         GetCurrentProcess(), &observed_thread, 0,
                         FALSE, DUPLICATE_SAME_ACCESS)) goto done;
    op_hook_cleanup(&ctx);
    if (ctx.hook.target_process || ctx.hook.load_thread ||
        ctx.hook.remote_path || !allocated(remote)) goto done;
    stage = 6;
    if (!SetEvent(event) ||
        WaitForSingleObject(observed_thread, 2000u) != WAIT_OBJECT_0 ||
        !VirtualFree(remote, 0, MEM_RELEASE)) goto done;
    ok = 1;
done:
    if (!ok) fprintf(stderr, "op hook timeout stage %d: remote=%p allocated=%d\n",
                     stage, remote, remote ? allocated(remote) : 0);
    if (event) SetEvent(event);
    op_hook_cleanup(&ctx);
    if (observed_thread) CloseHandle(observed_thread);
    if (event) CloseHandle(event);
    if (ok) puts("op hook timeout: ok");
    return ok ? 0 : 1;
}
