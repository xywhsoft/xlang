/* WGL 交换前读取当前 OpenGL 后缓冲；SwapBuffers 使用函数级跳板。 */
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "op_hook_shared.h"
#include "third_party/minhook/include/MinHook.h"

#define GL_BACK            0x0405u
#define GL_BGRA_EXT        0x80E1u
#define GL_UNSIGNED_BYTE   0x1401u
#define GL_VIEWPORT        0x0BA2u
#define GL_READ_BUFFER     0x0C02u
#define GL_PACK_ALIGNMENT  0x0D05u

typedef void (WINAPI *PFN_glReadBuffer)(unsigned int);
typedef void (WINAPI *PFN_glReadPixels)(int, int, int, int, unsigned int, unsigned int, void *);
typedef void (WINAPI *PFN_glGetIntegerv)(unsigned int, int *);
typedef void (WINAPI *PFN_glPixelStorei)(unsigned int, int);
typedef void *(WINAPI *PFN_wglGetCurrentContext)(void);
typedef BOOL (WINAPI *PFN_SwapBuffers)(HDC);

static PFN_glReadBuffer g_read_buffer;
static PFN_glReadPixels g_read_pixels;
static PFN_glGetIntegerv g_get_integer;
static PFN_glPixelStorei g_pixel_store;
static PFN_wglGetCurrentContext g_current_context;
static PFN_SwapBuffers g_original_swap;
static CRITICAL_SECTION g_capture_lock;
static void *g_shared_mem;
static BYTE *g_read_buf;
static size_t g_buf_size;
static volatile LONG g_hook_status;
static volatile LONG g_hook_calls;

__declspec(dllexport) LONG op_wgl_hook_status(void) {
    return InterlockedCompareExchange(&g_hook_status, 0, 0);
}

__declspec(dllexport) LONG op_wgl_hook_calls(void) {
    return InterlockedCompareExchange(&g_hook_calls, 0, 0);
}

static BOOL WINAPI hook_swap_buffers(HDC dc) {
    InterlockedIncrement(&g_hook_calls);
    if (g_shared_mem && g_current_context && g_current_context() &&
        g_read_buffer && g_read_pixels && g_get_integer && g_pixel_store) {
        int viewport[4] = {0};
        int previous_read = 0;
        int previous_pack = 0;
        size_t bytes;
        int row;
        EnterCriticalSection(&g_capture_lock);
        g_get_integer(GL_VIEWPORT, viewport);
        if (viewport[2] > 0 && viewport[3] > 0 &&
            op_hook_frame_fits((unsigned int)viewport[2], (unsigned int)viewport[3],
                               OP_HOOK_MAP_SIZE, sizeof(op_hook_header))) {
            bytes = (size_t)viewport[2] * (size_t)viewport[3] * 4u;
            if (bytes > g_buf_size) {
                BYTE *next = (BYTE *)realloc(g_read_buf, bytes);
                if (next) { g_read_buf = next; g_buf_size = bytes; }
            }
            if (g_buf_size >= bytes) {
                BYTE *target = op_hook_pixels(g_shared_mem);
                op_hook_header *header = (op_hook_header *)g_shared_mem;
                g_get_integer(GL_READ_BUFFER, &previous_read);
                g_get_integer(GL_PACK_ALIGNMENT, &previous_pack);
                g_read_buffer(GL_BACK);
                g_pixel_store(GL_PACK_ALIGNMENT, 1);
                g_read_pixels(viewport[0], viewport[1], viewport[2], viewport[3],
                              GL_BGRA_EXT, GL_UNSIGNED_BYTE, g_read_buf);
                g_read_buffer((unsigned int)previous_read);
                g_pixel_store(GL_PACK_ALIGNMENT, previous_pack);
                op_hook_begin_frame(header, (unsigned int)viewport[2],
                                    (unsigned int)viewport[3]);
                for (row = 0; row < viewport[3]; ++row) {
                    memcpy(target + (size_t)row * (size_t)viewport[2] * 4u,
                           g_read_buf + (size_t)(viewport[3] - 1 - row) *
                                        (size_t)viewport[2] * 4u,
                           (size_t)viewport[2] * 4u);
                }
                op_hook_end_frame(header);
            }
        }
        LeaveCriticalSection(&g_capture_lock);
    }
    return g_original_swap(dc);
}

static DWORD WINAPI init_hook(void *unused) {
    HMODULE gl;
    HMODULE gdi;
    HMODULE pinned;
    FARPROC symbol;
    LPVOID trampoline = NULL;
    MH_STATUS status;
    (void)unused;

    /* 图形 DLL 的加载和函数入口修改不能放在 DllMain 的 loader lock 下。 */
    gl = GetModuleHandleW(L"opengl32.dll");
    if (!gl) gl = LoadLibraryW(L"opengl32.dll");
    gdi = GetModuleHandleW(L"gdi32.dll");
    if (!gdi) gdi = LoadLibraryW(L"gdi32.dll");
    if (!gl || !gdi) goto failed;
#define LOAD_GL(member, name) do { \
    symbol = GetProcAddress(gl, name); \
    if (sizeof(symbol) == sizeof(member)) memcpy(&member, &symbol, sizeof(member)); \
} while (0)
    LOAD_GL(g_read_buffer, "glReadBuffer");
    LOAD_GL(g_read_pixels, "glReadPixels");
    LOAD_GL(g_get_integer, "glGetIntegerv");
    LOAD_GL(g_pixel_store, "glPixelStorei");
    LOAD_GL(g_current_context, "wglGetCurrentContext");
#undef LOAD_GL
    if (!g_read_buffer || !g_read_pixels || !g_get_integer || !g_pixel_store ||
        !g_current_context) goto failed;
    symbol = GetProcAddress(gdi, "SwapBuffers");
    if (!symbol) goto failed;
    status = MH_Initialize();
    if (status != MH_OK) goto failed;
    status = MH_CreateHook((LPVOID)symbol, (LPVOID)hook_swap_buffers, &trampoline);
    if (status != MH_OK) { MH_Uninitialize(); goto failed; }
    memcpy(&g_original_swap, &trampoline, sizeof(g_original_swap));
    status = MH_EnableHook((LPVOID)symbol);
    if (status != MH_OK) {
        MH_RemoveHook((LPVOID)symbol);
        MH_Uninitialize();
        goto failed;
    }
    /* 跳板仍指向本 DLL；只能随目标进程退出卸载。 */
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_PIN, (LPCSTR)&g_hook_status, &pinned)) {
        MH_DisableHook((LPVOID)symbol);
        MH_RemoveHook((LPVOID)symbol);
        MH_Uninitialize();
        goto failed;
    }
    InterlockedExchange(&g_hook_status, 1);
    return 0u;
failed:
    InterlockedExchange(&g_hook_status, -1);
    return 1u;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        HANDLE thread;
        DisableThreadLibraryCalls(instance);
        InitializeCriticalSection(&g_capture_lock);
        g_shared_mem = op_hook_open_shared(GetCurrentProcessId(), OP_HOOK_MAP_SIZE);
        if (!g_shared_mem) { InterlockedExchange(&g_hook_status, -2); return TRUE; }
        thread = CreateThread(NULL, 0, init_hook, NULL, 0, NULL);
        if (thread) CloseHandle(thread);
        else InterlockedExchange(&g_hook_status, -3);
    } else if (reason == DLL_PROCESS_DETACH && reserved == NULL) {
        free(g_read_buf);
        if (g_shared_mem) UnmapViewOfFile(g_shared_mem);
        DeleteCriticalSection(&g_capture_lock);
    }
    return TRUE;
}
