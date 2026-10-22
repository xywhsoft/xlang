/*
 * op_native.c - OP plugin: window binding, screen capture, color/image finding,
 *               mouse/keyboard simulation, DXGI/D3D9/D3D11/WGL game capture.
 * Pure Win32 API + COM vtable calls. TCC-compatible.
 */
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <wchar.h>
#ifndef XRT_MODULE_PATH_SYSTEM
#define XRT_MODULE_PATH_SYSTEM
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_ONCE
#define XRT_MODULE_ONCE
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_RANDOM_DEFAULT
#define XRT_MODULE_RANDOM_DEFAULT
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#include "xrt.h"
#include "include/op.h"
#include "op_dxgi_defs.h"
#include "hooks/op_hook_shared.h"

/* Manual Toolhelp32 declarations (TCC has no tlhelp32.h) */
#define TH32CS_SNAPPROCESS  0x00000002
#define TH32CS_SNAPMODULE   0x00000008
#define TH32CS_SNAPMODULE32 0x00000010
#define MAX_MODULE_NAME32   255

typedef struct op_process_entry32w {
    DWORD dwSize;
    DWORD cntUsage;
    DWORD th32ProcessID;
    ULONG_PTR th32DefaultHeapID;
    DWORD th32ModuleID;
    DWORD cntThreads;
    DWORD th32ParentProcessID;
    LONG  pcPriClassBase;
    DWORD dwFlags;
    WCHAR szExeFile[260];
} op_process_entry32w;

/*
 * Toolhelp 的 64 位结构必须与 Win32 ABI 完全一致。
 * 缺少 dwSize 时 Process32First/Module32First 可能直接写越界，
 * 因此不自行猜测窄化结构，也不把系统 PID 当作可靠测试对象。
 */
typedef struct op_module_entry32w {
    DWORD   dwSize;
    DWORD   th32ModuleID;
    DWORD   th32ProcessID;
    DWORD   GlblcntUsage;
    DWORD   ProccntUsage;
    BYTE   *modBaseAddr;
    DWORD   modBaseSize;
    HMODULE hModule;
    WCHAR   szModule[MAX_MODULE_NAME32 + 1];
    WCHAR   szExePath[260];
} op_module_entry32w;

typedef HANDLE (WINAPI *PFN_CreateToolhelp32Snapshot)(DWORD, DWORD);
typedef BOOL   (WINAPI *PFN_Process32First)(HANDLE, op_process_entry32w*);
typedef BOOL   (WINAPI *PFN_Process32Next)(HANDLE, op_process_entry32w*);
typedef BOOL   (WINAPI *PFN_Module32First)(HANDLE, op_module_entry32w*);
typedef BOOL   (WINAPI *PFN_Module32Next)(HANDLE, op_module_entry32w*);

static PFN_CreateToolhelp32Snapshot p_CreateToolhelp32Snapshot = NULL;
static PFN_Process32First  p_Process32First  = NULL;
static PFN_Process32Next   p_Process32Next   = NULL;
static PFN_Module32First   p_Module32First   = NULL;
static PFN_Module32Next    p_Module32Next    = NULL;
static xonce op_toolhelp_once = XRT_ONCE_INIT;

static bool op_load_toolhelp(void *unused) {
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    (void)unused;
    if (!k32) k32 = LoadLibraryW(L"kernel32.dll");
    if (k32) {
        p_CreateToolhelp32Snapshot = (PFN_CreateToolhelp32Snapshot)GetProcAddress(k32, "CreateToolhelp32Snapshot");
        p_Process32First = (PFN_Process32First)GetProcAddress(k32, "Process32FirstW");
        p_Process32Next  = (PFN_Process32Next)GetProcAddress(k32, "Process32NextW");
        p_Module32First  = (PFN_Module32First)GetProcAddress(k32, "Module32FirstW");
        p_Module32Next   = (PFN_Module32Next)GetProcAddress(k32, "Module32NextW");
    }
    return true;
}

static void op_init_toolhelp(void) {
    (void)xrtOnce(&op_toolhelp_once, op_load_toolhelp, NULL);
}

/* Minimal OpenGL declarations (TCC has no GL/gl.h) */
#define GL_FRONT        0x0404
#define GL_BGRA_EXT     0x80E1
#define GL_UNSIGNED_BYTE 0x1401
#define OP_GL_READ_BUFFER 0x0C02u
#define OP_GL_PACK_ALIGNMENT 0x0D05u
#define OP_GL_PACK_ROW_LENGTH 0x0D02u
#define OP_GL_PACK_SKIP_ROWS 0x0D03u
#define OP_GL_PACK_SKIP_PIXELS 0x0D04u
#define OP_GL_PIXEL_PACK_BUFFER_BINDING 0x88EDu
#define OP_GL_FRAMEBUFFER_BINDING 0x8CA6u
#define OP_GL_READ_FRAMEBUFFER_BINDING 0x8CAAu
#define OP_GL_VERSION 0x1F02u
#define OP_GL_EXTENSIONS 0x1F03u
typedef void (WINAPI *PFN_glReadBuffer)(unsigned int);
typedef void (WINAPI *PFN_glReadPixels)(int, int, int, int, unsigned int, unsigned int, void*);
typedef void (WINAPI *PFN_glGetIntegerv)(unsigned int, int *);
typedef void (WINAPI *PFN_glPixelStorei)(unsigned int, int);
typedef const unsigned char *(WINAPI *PFN_glGetString)(unsigned int);
typedef HGLRC (WINAPI *PFN_wglGetCurrentContext)(void);
typedef HDC (WINAPI *PFN_wglGetCurrentDC)(void);
static PFN_glReadBuffer  p_glReadBuffer  = NULL;
static PFN_glReadPixels  p_glReadPixels  = NULL;
static PFN_glGetIntegerv p_glGetIntegerv = NULL;
static PFN_glPixelStorei p_glPixelStorei = NULL;
static PFN_glGetString p_glGetString = NULL;
static PFN_wglGetCurrentContext p_wglGetCurrentContext = NULL;
static PFN_wglGetCurrentDC p_wglGetCurrentDC = NULL;
static xonce op_gl_once = XRT_ONCE_INIT;

static bool op_load_gl(void *unused) {
    HMODULE gl = GetModuleHandleW(L"opengl32.dll");
    (void)unused;
    if (!gl) gl = LoadLibraryW(L"opengl32.dll");
    if (gl) {
        p_glReadBuffer = (PFN_glReadBuffer)GetProcAddress(gl, "glReadBuffer");
        p_glReadPixels = (PFN_glReadPixels)GetProcAddress(gl, "glReadPixels");
        p_glGetIntegerv = (PFN_glGetIntegerv)GetProcAddress(gl, "glGetIntegerv");
        p_glPixelStorei = (PFN_glPixelStorei)GetProcAddress(gl, "glPixelStorei");
        p_glGetString = (PFN_glGetString)GetProcAddress(gl, "glGetString");
        p_wglGetCurrentContext = (PFN_wglGetCurrentContext)GetProcAddress(gl,
                                                                          "wglGetCurrentContext");
        p_wglGetCurrentDC = (PFN_wglGetCurrentDC)GetProcAddress(gl,
                                                                 "wglGetCurrentDC");
    }
    return true;
}

static void op_init_gl(void) {
    (void)xrtOnce(&op_gl_once, op_load_gl, NULL);
}

/* Dynamically loaded user32 functions (may be missing from TCC import lib) */
typedef BOOL (WINAPI *PFN_PrintWindow)(HWND, HDC, UINT);
typedef UINT (WINAPI *PFN_SendInput)(UINT, LPVOID, int);
static PFN_PrintWindow p_PrintWindow = NULL;
static PFN_SendInput   p_SendInput   = NULL;
static xonce op_user32_once = XRT_ONCE_INIT;

static bool op_load_user32(void *unused) {
    HMODULE u32 = GetModuleHandleW(L"user32.dll");
    (void)unused;
    if (!u32) u32 = LoadLibraryW(L"user32.dll");
    if (u32) {
        p_PrintWindow = (PFN_PrintWindow)GetProcAddress(u32, "PrintWindow");
        p_SendInput = (PFN_SendInput)GetProcAddress(u32, "SendInput");
    }
    return true;
}

static void op_init_user32(void) {
    (void)xrtOnce(&op_user32_once, op_load_user32, NULL);
}
static void opSetError(const char* sMessage)
{
    xrtSetErrorInfo(XERR_STATE, "xlang.extlib.op", 0,
        sMessage != NULL ? sMessage : "op error");
}
typedef struct {
    HANDLE map_handle;
    BYTE *map_ptr;
    HANDLE target_process;
    HANDLE load_thread;
    void *remote_path;
    DWORD pid;
    char map_name[64];
    int injected;
} op_hook_state;

typedef struct {
    HMODULE                 d3d11_module;
    ID3D11Device           *device;
    ID3D11DeviceContext    *device_ctx;
    IDXGIOutputDuplication *dup;
    ID3D11Texture2D        *staging;
    int                     staging_w, staging_h;
    BYTE                   *last_frame;
    int                     last_w, last_h;
} op_dxgi_state;

/* Context definition (needed early for DXGI/hook capture) */
typedef struct op_context {
    HWND    bind_hwnd;
    HDC     mem_dc;
    HBITMAP mem_bmp;
    HBITMAP old_bmp;
    BYTE   *frame_buf;
    int     frame_w, frame_h;
    char    display_mode[32];
    char    mouse_mode[16];
    char    keypad_mode[16];
    int     bound;
    op_hook_state hook;
    int     dxgi_attached;
} op_context;

/* ------------------------------------------------------------------ */
/* DXGI Desktop Duplication state                                      */
/* ------------------------------------------------------------------ */

static op_dxgi_state g_dxgi;
static unsigned int g_dxgi_users;
static xmutex g_dxgi_mutex;
static xonce g_dxgi_once = XRT_ONCE_INIT;

static bool op_dxgi_init_mutex(void *unused) {
    (void)unused;
    return xrtMutexInit(&g_dxgi_mutex);
}

static int op_dxgi_lock(void) {
    if (!xrtOnce(&g_dxgi_once, op_dxgi_init_mutex, NULL) ||
        !xrtMutexLock(&g_dxgi_mutex)) {
        opSetError("op: DXGI lock initialization failed");
        return 0;
    }
    return 1;
}

static void op_dxgi_cleanup(op_dxgi_state *dxgi) {
    free(dxgi->last_frame);
    dxgi->last_frame = NULL;
    dxgi->last_w = dxgi->last_h = 0;
    OP_RELEASE(dxgi->staging);
    OP_RELEASE(dxgi->dup);
    OP_RELEASE(dxgi->device_ctx);
    OP_RELEASE(dxgi->device);
    if (dxgi->d3d11_module) {
        FreeLibrary(dxgi->d3d11_module);
        dxgi->d3d11_module = NULL;
    }
    dxgi->staging_w = dxgi->staging_h = 0;
}

static int op_dxgi_init(op_dxgi_state *dxgi) {
    PFN_D3D11CreateDevice create_device;
    unsigned int feature_level = D3D_FEATURE_LEVEL_11_0;
    IDXGIDevice *dxgi_device = NULL;
    IDXGIAdapter *adapter = NULL;
    OP_IUnknown *output = NULL;
    IDXGIOutput1 *output1 = NULL;
    HRESULT hr = E_FAIL;
    const char *stage = "LoadLibraryW";

    if (dxgi->dup) return 1;
    dxgi->d3d11_module = LoadLibraryW(L"d3d11.dll");
    if (!dxgi->d3d11_module) goto failed;
    stage = "D3D11CreateDevice symbol";
    create_device = (PFN_D3D11CreateDevice)GetProcAddress(dxgi->d3d11_module,
                                                            "D3D11CreateDevice");
    if (!create_device) goto failed;
    stage = "D3D11CreateDevice";
    hr = create_device(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
                       &feature_level, 1, D3D11_SDK_VERSION,
                       &dxgi->device, NULL, &dxgi->device_ctx);
    if (FAILED(hr) || !dxgi->device || !dxgi->device_ctx) goto failed;

    /* 设备所属适配器上的第一个输出；所有 Op 共享这一份 duplication。 */
    stage = "QueryInterface IDXGIDevice";
    hr = dxgi->device->lpVtbl->QueryInterface(dxgi->device,
                                               &IID_IDXGIDevice, (void**)&dxgi_device);
    if (FAILED(hr) || !dxgi_device) goto failed;
    stage = "GetAdapter";
    hr = dxgi_device->lpVtbl->GetAdapter(dxgi_device, &adapter);
    if (FAILED(hr) || !adapter) goto failed;
    stage = "EnumOutputs";
    hr = adapter->lpVtbl->EnumOutputs(adapter, 0, &output);
    if (FAILED(hr) || !output) goto failed;
    stage = "QueryInterface IDXGIOutput1";
    hr = output->lpVtbl->QueryInterface(output, &IID_IDXGIOutput1, (void**)&output1);
    if (FAILED(hr) || !output1) goto failed;
    stage = "DuplicateOutput";
    hr = output1->lpVtbl->DuplicateOutput(output1,
             (OP_IUnknown*)dxgi->device, &dxgi->dup);
    if (FAILED(hr) || !dxgi->dup) goto failed;
    OP_RELEASE(output1);
    OP_RELEASE(output);
    OP_RELEASE(adapter);
    OP_RELEASE(dxgi_device);
    return 1;

failed:
    {
        char message[128];
        snprintf(message, sizeof(message), "op: DXGI %s failed (0x%08x)",
                 stage, (unsigned int)hr);
        opSetError(message);
    }
    OP_RELEASE(output1);
    OP_RELEASE(output);
    OP_RELEASE(adapter);
    OP_RELEASE(dxgi_device);
    op_dxgi_cleanup(dxgi);
    return 0;
}

static int op_dxgi_copy_cached(op_context *ctx, const op_dxgi_state *dxgi) {
    BYTE *next = NULL;
    size_t bytes;
    if (!dxgi->last_frame || dxgi->last_w <= 0 || dxgi->last_h <= 0)
        return 0;
    bytes = (size_t)dxgi->last_w * (size_t)dxgi->last_h * 4u;
    if (!ctx->frame_buf || ctx->frame_w != dxgi->last_w ||
        ctx->frame_h != dxgi->last_h) {
        next = (BYTE *)malloc(bytes);
        if (!next) {
            opSetError("op: out of memory for DXGI frame");
            return 0;
        }
    }
    memcpy(next ? next : ctx->frame_buf, dxgi->last_frame, bytes);
    if (next) {
        free(ctx->frame_buf);
        ctx->frame_buf = next;
        ctx->frame_w = dxgi->last_w;
        ctx->frame_h = dxgi->last_h;
    }
    return 1;
}

static int op_dxgi_capture_locked(op_context *ctx) {
    op_dxgi_state *dxgi = &g_dxgi;
    if (!op_dxgi_init(dxgi)) return 0;
    if (!ctx->dxgi_attached) {
        ctx->dxgi_attached = 1;
        g_dxgi_users++;
    }

    IDXGIResource *desktop_res = NULL;
    DXGI_OUTDUPL_FRAME_INFO fi;
    HRESULT hr = dxgi->dup->lpVtbl->AcquireNextFrame(dxgi->dup, 100, &fi, &desktop_res);
    if (FAILED(hr) || !desktop_res) {
        char message[96];
        /* 只有等待超时可复用上一帧；设备丢失等错误不可伪装成成功。 */
        if ((unsigned int)hr == 0x887A0027u &&
            dxgi->last_frame) return op_dxgi_copy_cached(ctx, dxgi);
        snprintf(message, sizeof(message), "op: DXGI AcquireNextFrame failed (0x%08x)",
                 (unsigned int)hr);
        opSetError(message);
        return 0;
    }

    /* Get ID3D11Texture2D from resource */
    ID3D11Texture2D *desktop_tex = NULL;
    hr = desktop_res->lpVtbl->QueryInterface(
             desktop_res, &IID_ID3D11Texture2D, (void**)&desktop_tex);
    OP_RELEASE(desktop_res);
    if (FAILED(hr) || !desktop_tex) {
        dxgi->dup->lpVtbl->ReleaseFrame(dxgi->dup);
        return 0;
    }

    /* Get texture dimensions */
    D3D11_TEXTURE2D_DESC desc;
    desktop_tex->lpVtbl->GetDesc(desktop_tex, &desc);
    if (desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM ||
        !op_hook_frame_fits(desc.Width, desc.Height, OP_HOOK_MAP_SIZE,
                            sizeof(op_hook_header))) {
        OP_RELEASE(desktop_tex);
        dxgi->dup->lpVtbl->ReleaseFrame(dxgi->dup);
        opSetError("op: unsupported DXGI frame format or size");
        return 0;
    }
    int w = (int)desc.Width, h = (int)desc.Height;
    BYTE *next = NULL;
    BYTE *next_cache = NULL;

    /* Create/recreate staging texture if needed */
    if (!dxgi->staging || dxgi->staging_w != w || dxgi->staging_h != h) {
        OP_RELEASE(dxgi->staging);
        D3D11_TEXTURE2D_DESC sd;
        memset(&sd, 0, sizeof(sd));
        sd.Width = w;
        sd.Height = h;
        sd.MipLevels = 1;
        sd.ArraySize = 1;
        sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.Usage = D3D11_USAGE_STAGING;
        sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        hr = dxgi->device->lpVtbl->CreateTexture2D(dxgi->device, &sd, NULL, &dxgi->staging);
        if (FAILED(hr) || !dxgi->staging) {
            OP_RELEASE(desktop_tex);
            dxgi->dup->lpVtbl->ReleaseFrame(dxgi->dup);
            opSetError("op: DXGI staging texture creation failed");
            return 0;
        }
        dxgi->staging_w = w;
        dxgi->staging_h = h;
    }

    if (ctx->frame_w != w || ctx->frame_h != h || !ctx->frame_buf) {
        next = (BYTE *)malloc((size_t)w * (size_t)h * 4u);
        if (!next) {
            OP_RELEASE(desktop_tex);
            dxgi->dup->lpVtbl->ReleaseFrame(dxgi->dup);
            opSetError("op: out of memory for DXGI frame");
            return 0;
        }
    }
    if (dxgi->last_w != w || dxgi->last_h != h || !dxgi->last_frame) {
        next_cache = (BYTE *)malloc((size_t)w * (size_t)h * 4u);
        if (!next_cache) {
            free(next);
            OP_RELEASE(desktop_tex);
            dxgi->dup->lpVtbl->ReleaseFrame(dxgi->dup);
            opSetError("op: out of memory for DXGI cache");
            return 0;
        }
    }

    dxgi->device_ctx->lpVtbl->CopyResource(
        dxgi->device_ctx, dxgi->staging, desktop_tex);
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = dxgi->device_ctx->lpVtbl->Map(
        dxgi->device_ctx, dxgi->staging, 0, 1/*READ*/, 0, &mapped);
    if (FAILED(hr) || !mapped.pData || mapped.RowPitch < (unsigned int)w * 4u) {
        if (SUCCEEDED(hr))
            dxgi->device_ctx->lpVtbl->Unmap(dxgi->device_ctx, dxgi->staging, 0);
        free(next);
        free(next_cache);
        OP_RELEASE(desktop_tex);
        dxgi->dup->lpVtbl->ReleaseFrame(dxgi->dup);
        opSetError("op: DXGI staging read failed");
        return 0;
    }
    BYTE *target = next ? next : ctx->frame_buf;
    int row;
    for (row = 0; row < h; row++) {
        memcpy(target + (size_t)row * (size_t)w * 4u,
               (BYTE*)mapped.pData + (size_t)row * mapped.RowPitch,
               (size_t)w * 4u);
    }
    dxgi->device_ctx->lpVtbl->Unmap(dxgi->device_ctx, dxgi->staging, 0);
    if (next_cache) {
        free(dxgi->last_frame);
        dxgi->last_frame = next_cache;
        dxgi->last_w = w;
        dxgi->last_h = h;
    }
    memcpy(dxgi->last_frame, target, (size_t)w * (size_t)h * 4u);
    if (next) {
        free(ctx->frame_buf);
        ctx->frame_buf = next;
        ctx->frame_w = w;
        ctx->frame_h = h;
    }

    OP_RELEASE(desktop_tex);
    dxgi->dup->lpVtbl->ReleaseFrame(dxgi->dup);
    return 1;
}

static int op_dxgi_capture(op_context *ctx) {
    int result;
    if (!op_dxgi_lock()) return 0;
    result = op_dxgi_capture_locked(ctx);
    xrtMutexUnlock(&g_dxgi_mutex);
    return result;
}

static void op_dxgi_detach(op_context *ctx) {
    if (!ctx->dxgi_attached || !op_dxgi_lock()) return;
    ctx->dxgi_attached = 0;
    if (--g_dxgi_users == 0) op_dxgi_cleanup(&g_dxgi);
    xrtMutexUnlock(&g_dxgi_mutex);
}

/* ------------------------------------------------------------------ */
/* Hook shared memory (for d3d9/d3d11/wgl injected modes)              */
/* ------------------------------------------------------------------ */

static void op_hook_cleanup(op_context *ctx);

static int op_hook_finish_load(op_hook_state *hook) {
    DWORD result, loaded = 0;
    if (!hook->load_thread) return hook->injected;
    result = WaitForSingleObject(hook->load_thread, 0u);
    if (result == WAIT_TIMEOUT) {
        opSetError("op: hook DLL load is still pending");
        return 0;
    }
    if (result != WAIT_OBJECT_0) {
        opSetError("op: hook DLL load status unavailable");
        return 0;
    }
    GetExitCodeThread(hook->load_thread, &loaded);
    VirtualFreeEx(hook->target_process, hook->remote_path, 0, MEM_RELEASE);
    CloseHandle(hook->load_thread);
    hook->load_thread = NULL;
    hook->remote_path = NULL;
    if (result != WAIT_OBJECT_0 || loaded == 0u) {
        opSetError("op: target process could not load hook DLL");
        return 0;
    }
    hook->injected = 1;
    return 1;
}

static int op_hook_inject(op_context *ctx, const char *mode) {
    op_hook_state *hook = &ctx->hook;
    str app_dir = NULL;
    str hooks_dir = NULL;
    str dll_path = NULL;
    uint16 *wide_path = NULL;
    HANDLE process = NULL;
    HANDLE thread = NULL;
    void *remote_path = NULL;
    size_t path_bytes;
    SIZE_T written = 0;
    DWORD wait_result;
    DWORD load_result = 0;
    int success = 0;
    if (hook->injected) return 1;
    if (hook->load_thread) {
        if (op_hook_finish_load(hook)) return 1;
        if (!hook->load_thread) op_hook_cleanup(ctx);
        return 0;
    }

    const char *dll_name = "op_d3d9_hook.dll";
    if (strcmp(mode, "d3d11") == 0) dll_name = "op_d3d11_hook.dll";
    else if (strcmp(mode, "wgl") == 0) dll_name = "op_wgl_hook.dll";

    app_dir = xrtPathAppDir();
    if (app_dir != NULL) hooks_dir = xrtPathJoin(app_dir, "extlibs/op/hooks");
    if (hooks_dir != NULL) dll_path = xrtPathJoin(hooks_dir, dll_name);
    if (dll_path != NULL) wide_path = xrtUtf8To16(dll_path, NULL);
    if (wide_path == NULL) {
        opSetError("op: hook DLL path conversion failed");
        goto done;
    }
    if (GetFileAttributesW((const wchar_t*)wide_path) == INVALID_FILE_ATTRIBUTES) {
        opSetError("op: hook DLL not found");
        goto done;
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(ctx->bind_hwnd, &pid);
    if (!pid) { opSetError("op: cannot get target PID"); goto done; }
    hook->pid = pid;

    snprintf(hook->map_name, sizeof(hook->map_name), "op_hook_v2_%lu", pid);
    DWORD map_size = OP_HOOK_MAP_SIZE;
    hook->map_handle = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL,
                                         PAGE_READWRITE, 0, map_size, hook->map_name);
    if (!hook->map_handle) { opSetError("op: CreateFileMapping failed"); goto done; }
    hook->map_ptr = (BYTE*)MapViewOfFile(hook->map_handle, FILE_MAP_ALL_ACCESS, 0, 0, map_size);
    if (!hook->map_ptr) { opSetError("op: MapViewOfFile failed"); goto done; }
    memset(hook->map_ptr, 0, sizeof(op_hook_header));

    process = OpenProcess(SYNCHRONIZE | PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                          PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                          FALSE, pid);
    if (!process) { opSetError("op: OpenProcess failed"); goto done; }
    path_bytes = ((size_t)wcslen((const wchar_t*)wide_path) + 1u) * sizeof(wchar_t);
    remote_path = VirtualAllocEx(process, NULL, path_bytes,
                                 MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!remote_path) { opSetError("op: VirtualAllocEx failed"); goto done; }
    if (!WriteProcessMemory(process, remote_path, wide_path, path_bytes, &written) ||
        written != path_bytes) {
        opSetError("op: WriteProcessMemory failed");
        goto done;
    }

    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    void *load_library = kernel ? (void*)GetProcAddress(kernel, "LoadLibraryW") : NULL;
    if (!load_library) { opSetError("op: LoadLibraryW not found"); goto done; }
    thread = CreateRemoteThread(process, NULL, 0,
                                (LPTHREAD_START_ROUTINE)load_library,
                                remote_path, 0, NULL);
    if (!thread) { opSetError("op: CreateRemoteThread failed"); goto done; }
    wait_result = WaitForSingleObject(thread, 5000u);
    if (wait_result == WAIT_TIMEOUT) {
        opSetError("op: hook DLL load is still pending");
        hook->target_process = process;
        hook->load_thread = thread;
        hook->remote_path = remote_path;
        process = NULL;
        thread = NULL;
        remote_path = NULL;
        goto done;
    }
    if (wait_result != WAIT_OBJECT_0) {
        opSetError("op: hook DLL load wait failed");
        /* 无法证明线程已退出时，同样由延迟回收器持有远程路径。 */
        hook->target_process = process;
        hook->load_thread = thread;
        hook->remote_path = remote_path;
        process = NULL;
        thread = NULL;
        remote_path = NULL;
        goto done;
    }
    if (!GetExitCodeThread(thread, &load_result) || load_result == 0u) {
        opSetError("op: target process could not load hook DLL");
        goto done;
    }
    VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
    remote_path = NULL;
    hook->injected = 1;
    hook->target_process = process;
    process = NULL;
    success = 1;
done:
    if (thread) CloseHandle(thread);
    if (remote_path && process) VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
    if (process) CloseHandle(process);
    xrtFree(wide_path);
    xrtFree(dll_path);
    xrtFree(hooks_dir);
    xrtFree(app_dir);
    if (!success && !hook->load_thread) op_hook_cleanup(ctx);
    return success;
}

static int op_hook_capture(op_context *ctx) {
    op_hook_state *hook = &ctx->hook;
    int attempt;
    if (!hook->injected || !hook->map_ptr) {
        opSetError("op: hook not injected");
        return 0;
    }
    if (hook->target_process) {
        DWORD state = WaitForSingleObject(hook->target_process, 0u);
        if (state == WAIT_OBJECT_0) {
            opSetError("op: target process exited");
            op_hook_cleanup(ctx);
            return 0;
        }
        if (state != WAIT_TIMEOUT) {
            char message[96];
            snprintf(message, sizeof(message), "op: target status check failed (%lu, error %lu)",
                     (unsigned long)state, (unsigned long)GetLastError());
            opSetError(message);
            return 0;
        }
    }

    for (attempt = 0; attempt < 3; ++attempt) {
        op_hook_header *hdr = (op_hook_header*)hook->map_ptr;
        unsigned int w = hdr->width;
        unsigned int h = hdr->height;
        unsigned int copied_w;
        unsigned int copied_h;
        size_t capacity;
        if (hdr->magic != OP_HOOK_MAGIC ||
            !op_hook_frame_fits(w, h, OP_HOOK_MAP_SIZE, sizeof(op_hook_header))) continue;
        if (ctx->frame_w != (int)w || ctx->frame_h != (int)h || !ctx->frame_buf) {
            BYTE *next = (BYTE *)malloc((size_t)w * (size_t)h * 4u);
            if (next == NULL) {
                opSetError("op: out of memory for hook frame");
                return 0;
            }
            free(ctx->frame_buf);
            ctx->frame_buf = next;
            ctx->frame_w = (int)w;
            ctx->frame_h = (int)h;
        }
        capacity = (size_t)ctx->frame_w * (size_t)ctx->frame_h * 4u;
        if (op_hook_snapshot(hook->map_ptr, ctx->frame_buf, capacity,
                             &copied_w, &copied_h)) {
            ctx->frame_w = (int)copied_w;
            ctx->frame_h = (int)copied_h;
            return 1;
        }
    }
    opSetError("op: hook frame is not ready or changed during read");
    return 0;
}

static void op_hook_cleanup(op_context *ctx) {
    op_hook_state *hook = &ctx->hook;
    if (hook->load_thread) {
        DWORD state = WaitForSingleObject(hook->load_thread, 0u);
        /* 已完成才释放路径；未完成时不能留下可能执行已卸载 JIT 代码的线程。 */
        if (state == WAIT_OBJECT_0)
            VirtualFreeEx(hook->target_process, hook->remote_path, 0, MEM_RELEASE);
        CloseHandle(hook->load_thread);
        hook->load_thread = NULL;
        hook->remote_path = NULL;
    }
    if (hook->map_ptr) { UnmapViewOfFile(hook->map_ptr); hook->map_ptr = NULL; }
    if (hook->map_handle) { CloseHandle(hook->map_handle); hook->map_handle = NULL; }
    if (hook->target_process) { CloseHandle(hook->target_process); hook->target_process = NULL; }
    hook->injected = 0;
}

/* ------------------------------------------------------------------ */
/* Context (definition moved above for DXGI/hook)                      */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

void *xop_create(void) {
    op_context *ctx = (op_context *)calloc(1, sizeof(op_context));
    if (!ctx) { opSetError("op: out of memory"); return NULL; }
    strcpy(ctx->display_mode, "normal");
    strcpy(ctx->mouse_mode, "normal");
    strcpy(ctx->keypad_mode, "normal");
    return ctx;
}

void xop_destroy(void *ptr) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) return;
    if (ctx->mem_dc) {
        if (ctx->old_bmp) SelectObject(ctx->mem_dc, ctx->old_bmp);
        DeleteDC(ctx->mem_dc);
    }
    if (ctx->mem_bmp) DeleteObject(ctx->mem_bmp);
    if (ctx->frame_buf) free(ctx->frame_buf);
    op_dxgi_detach(ctx);
    op_hook_cleanup(ctx);
    free(ctx);
}

/* ------------------------------------------------------------------ */
/* Window binding                                                      */
/* ------------------------------------------------------------------ */

long long xop_unbind(void *ptr);

long long xop_bind_window(void *ptr, long long hwnd, const char *display,
                          const char *mouse, const char *keypad, long long mode) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    if (!IsWindow((HWND)(intptr_t)hwnd)) { opSetError("op: invalid hwnd"); return 0; }

    if (ctx->bound) xop_unbind(ptr);

    ctx->bind_hwnd = (HWND)(intptr_t)hwnd;
    ctx->bound = 1;

    if (display && *display) strncpy(ctx->display_mode, display, 31);
    else strcpy(ctx->display_mode, "normal");

    if (mouse && *mouse) strncpy(ctx->mouse_mode, mouse, 15);
    else strcpy(ctx->mouse_mode, "normal");

    if (keypad && *keypad) strncpy(ctx->keypad_mode, keypad, 15);
    else strcpy(ctx->keypad_mode, "normal");

    return 1;
}

long long xop_unbind(void *ptr) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    if (ctx->mem_dc) {
        if (ctx->old_bmp) SelectObject(ctx->mem_dc, ctx->old_bmp);
        DeleteDC(ctx->mem_dc);
        ctx->mem_dc = NULL;
        ctx->old_bmp = NULL;
    }
    if (ctx->mem_bmp) { DeleteObject(ctx->mem_bmp); ctx->mem_bmp = NULL; }
    if (ctx->frame_buf) { free(ctx->frame_buf); ctx->frame_buf = NULL; }
    ctx->frame_w = ctx->frame_h = 0;
    ctx->bind_hwnd = NULL;
    ctx->bound = 0;
    op_dxgi_detach(ctx);
    op_hook_cleanup(ctx);
    return 1;
}

long long xop_is_bind(void *ptr) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) return 0;
    return ctx->bound;
}

long long xop_get_bind_window(void *ptr) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) return 0;
    return (long long)(intptr_t)ctx->bind_hwnd;
}

/* ------------------------------------------------------------------ */
/* Capture frame (internal)                                            */
/* ------------------------------------------------------------------ */

static int op_frame_bytes(int w, int h, size_t *bytes) {
    if (w <= 0 || h <= 0 ||
        (size_t)w > ((size_t)256u * 1024u * 1024u / 4u) / (size_t)h) {
        opSetError("op: frame size exceeds limit");
        return 0;
    }
    *bytes = (size_t)w * (size_t)h * 4u;
    return 1;
}

static int op_ensure_frame(op_context *ctx, int w, int h) {
    HDC screen_dc, next_dc;
    HBITMAP next_bmp, next_old;
    BYTE *next_frame;
    size_t bytes;
    /* 普通/GDI 帧独立于 hook 映射，最多占 256 MiB。 */
    if (!op_frame_bytes(w, h, &bytes)) return 0;
    if (ctx->frame_w == w && ctx->frame_h == h && ctx->mem_dc &&
        ctx->mem_bmp && ctx->frame_buf) return 1;
    screen_dc = GetDC(NULL);
    if (!screen_dc) { opSetError("op: screen DC unavailable"); return 0; }
    next_dc = CreateCompatibleDC(screen_dc);
    next_bmp = CreateCompatibleBitmap(screen_dc, w, h);
    ReleaseDC(NULL, screen_dc);
    next_frame = (BYTE *)malloc(bytes);
    next_old = next_dc && next_bmp ? (HBITMAP)SelectObject(next_dc, next_bmp) : NULL;
    if (!next_dc || !next_bmp || !next_frame || !next_old) {
        if (next_old) SelectObject(next_dc, next_old);
        if (next_bmp) DeleteObject(next_bmp);
        if (next_dc) DeleteDC(next_dc);
        free(next_frame);
        opSetError("op: GDI frame allocation failed");
        return 0;
    }
    if (ctx->mem_dc) {
        if (ctx->old_bmp) SelectObject(ctx->mem_dc, ctx->old_bmp);
        DeleteDC(ctx->mem_dc);
    }
    if (ctx->mem_bmp) DeleteObject(ctx->mem_bmp);
    free(ctx->frame_buf);
    ctx->mem_dc = next_dc;
    ctx->mem_bmp = next_bmp;
    ctx->old_bmp = next_old;
    ctx->frame_buf = next_frame;
    ctx->frame_w = w;
    ctx->frame_h = h;
    return 1;
}

static int op_gl_direct_capture(op_context *ctx, int w, int h) {
    const unsigned char *version, *extensions = NULL;
    int major = 0, minor = 0, binding = 0;
    int read_buffer, alignment, row_length, skip_rows, skip_pixels;
    size_t row_bytes, frame_bytes;
    BYTE *row, *next_frame = NULL, *target;
    int y;

    if (!op_frame_bytes(w, h, &frame_bytes)) return 0;
    row_bytes = (size_t)w * 4u;
    op_init_gl();
    if (!p_glReadBuffer || !p_glReadPixels || !p_glGetIntegerv ||
        !p_glPixelStorei || !p_glGetString || !p_wglGetCurrentContext ||
        !p_wglGetCurrentDC || !p_wglGetCurrentContext() ||
        WindowFromDC(p_wglGetCurrentDC()) != ctx->bind_hwnd) {
        opSetError("op: OpenGL mode requires the bound window's current context");
        return 0;
    }
    version = p_glGetString(OP_GL_VERSION);
    if (!version || sscanf((const char *)version, "%d.%d", &major, &minor) != 2) {
        opSetError("op: OpenGL version unavailable");
        return 0;
    }
    if (major < 3) extensions = p_glGetString(OP_GL_EXTENSIONS);
    if (major > 2 || (major == 2 && minor >= 1) ||
        (extensions && (strstr((const char *)extensions, "GL_ARB_pixel_buffer_object") ||
                        strstr((const char *)extensions, "GL_EXT_pixel_buffer_object")))) {
        p_glGetIntegerv(OP_GL_PIXEL_PACK_BUFFER_BINDING, &binding);
        if (binding) {
            opSetError("op: OpenGL pixel pack buffer is unsupported");
            return 0;
        }
    }
    binding = 0;
    if (major >= 3) {
        p_glGetIntegerv(OP_GL_READ_FRAMEBUFFER_BINDING, &binding);
    } else if (extensions &&
               (strstr((const char *)extensions, "GL_ARB_framebuffer_object") ||
                strstr((const char *)extensions, "GL_EXT_framebuffer_object"))) {
        p_glGetIntegerv(OP_GL_FRAMEBUFFER_BINDING, &binding);
    }
    if (binding) {
        opSetError("op: OpenGL framebuffer object is unsupported");
        return 0;
    }
    if (!ctx->frame_buf || ctx->frame_w != w || ctx->frame_h != h) {
        next_frame = (BYTE *)malloc(frame_bytes);
        if (!next_frame) {
            opSetError("op: out of memory for OpenGL frame");
            return 0;
        }
    }
    row = (BYTE *)malloc(row_bytes);
    if (!row) {
        free(next_frame);
        opSetError("op: out of memory for OpenGL row");
        return 0;
    }
    target = next_frame ? next_frame : ctx->frame_buf;
    p_glGetIntegerv(OP_GL_READ_BUFFER, &read_buffer);
    p_glGetIntegerv(OP_GL_PACK_ALIGNMENT, &alignment);
    p_glGetIntegerv(OP_GL_PACK_ROW_LENGTH, &row_length);
    p_glGetIntegerv(OP_GL_PACK_SKIP_ROWS, &skip_rows);
    p_glGetIntegerv(OP_GL_PACK_SKIP_PIXELS, &skip_pixels);
    p_glReadBuffer(GL_FRONT);
    p_glPixelStorei(OP_GL_PACK_ALIGNMENT, 1);
    p_glPixelStorei(OP_GL_PACK_ROW_LENGTH, 0);
    p_glPixelStorei(OP_GL_PACK_SKIP_ROWS, 0);
    p_glPixelStorei(OP_GL_PACK_SKIP_PIXELS, 0);
    p_glReadPixels(0, 0, w, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, target);
    p_glPixelStorei(OP_GL_PACK_SKIP_PIXELS, skip_pixels);
    p_glPixelStorei(OP_GL_PACK_SKIP_ROWS, skip_rows);
    p_glPixelStorei(OP_GL_PACK_ROW_LENGTH, row_length);
    p_glPixelStorei(OP_GL_PACK_ALIGNMENT, alignment);
    p_glReadBuffer((unsigned int)read_buffer);
    for (y = 0; y < h / 2; ++y) {
        BYTE *top = target + (size_t)y * row_bytes;
        BYTE *bottom = target + (size_t)(h - 1 - y) * row_bytes;
        memcpy(row, top, row_bytes);
        memcpy(top, bottom, row_bytes);
        memcpy(bottom, row, row_bytes);
    }
    free(row);
    if (next_frame) {
        free(ctx->frame_buf);
        ctx->frame_buf = next_frame;
        ctx->frame_w = w;
        ctx->frame_h = h;
    }
    return 1;
}

static int op_capture_frame(op_context *ctx) {
    if (!ctx->bound || !ctx->bind_hwnd) {
        opSetError("op: not bound");
        return 0;
    }

    /* DXGI Desktop Duplication mode */
    if (strcmp(ctx->display_mode, "dxgi") == 0) {
        return op_dxgi_capture(ctx);
    }

    /* Hook-injected modes (d3d9/d3d11/wgl) */
    if (strcmp(ctx->display_mode, "d3d9") == 0 ||
        strcmp(ctx->display_mode, "d3d11") == 0 ||
        strcmp(ctx->display_mode, "wgl") == 0) {
        if (!ctx->hook.injected) {
            if (!op_hook_inject(ctx, ctx->display_mode)) return 0;
        }
        return op_hook_capture(ctx);
    }

    RECT rc;
    if (!GetClientRect(ctx->bind_hwnd, &rc)) {
        opSetError("op: window rectangle unavailable");
        return 0;
    }
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) { opSetError("op: window has zero size"); return 0; }

    if (strcmp(ctx->display_mode, "opengl") == 0)
        return op_gl_direct_capture(ctx, w, h);
    if (!op_ensure_frame(ctx, w, h)) return 0;

    if (strcmp(ctx->display_mode, "gdi") == 0) {
        /* PrintWindow - works for occluded/minimized windows */
        op_init_user32();
        if (p_PrintWindow) {
            if (!p_PrintWindow(ctx->bind_hwnd, ctx->mem_dc, PW_CLIENTONLY)) {
                opSetError("op: PrintWindow failed");
                return 0;
            }
        } else {
            HDC src_dc2 = GetDC(ctx->bind_hwnd);
            if (!src_dc2) { opSetError("op: window DC unavailable"); return 0; }
            int copied = BitBlt(ctx->mem_dc, 0, 0, w, h, src_dc2, 0, 0, SRCCOPY);
            ReleaseDC(ctx->bind_hwnd, src_dc2);
            if (!copied) { opSetError("op: BitBlt failed"); return 0; }
        }
    } else {
        /* normal: GetDC + BitBlt */
        HDC src_dc = GetDC(ctx->bind_hwnd);
        if (!src_dc) { opSetError("op: window DC unavailable"); return 0; }
        int copied = BitBlt(ctx->mem_dc, 0, 0, w, h, src_dc, 0, 0, SRCCOPY);
        ReleaseDC(ctx->bind_hwnd, src_dc);
        if (!copied) { opSetError("op: BitBlt failed"); return 0; }
    }

    /* extract pixels from mem_dc to frame_buf */
    BITMAPINFOHEADER bi;
    memset(&bi, 0, sizeof(bi));
    bi.biSize = sizeof(bi);
    bi.biWidth = w;
    bi.biHeight = -h; /* top-down */
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    if (!SelectObject(ctx->mem_dc, ctx->old_bmp)) {
        opSetError("op: GDI bitmap deselection failed");
        return 0;
    }
    int lines = GetDIBits(ctx->mem_dc, ctx->mem_bmp, 0, (UINT)h, ctx->frame_buf,
                          (BITMAPINFO *)&bi, DIB_RGB_COLORS);
    if (!SelectObject(ctx->mem_dc, ctx->mem_bmp) || lines != h) {
        opSetError("op: GDI pixel read failed");
        return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Image & Color                                                       */
/* ------------------------------------------------------------------ */

/* 搜索区域使用闭区间；先按 64 位与帧求交，避免窄化回绕后索引越界。 */
static int op_clip_search_rect(const op_context *ctx,
                               long long x1, long long y1,
                               long long x2, long long y2,
                               int *sx1, int *sy1, int *sx2, int *sy2) {
    if (!ctx || ctx->frame_w <= 0 || ctx->frame_h <= 0 ||
        x2 < x1 || y2 < y1 || x2 < 0 || y2 < 0 ||
        x1 >= ctx->frame_w || y1 >= ctx->frame_h) return 0;
    *sx1 = (int)(x1 < 0 ? 0 : x1);
    *sy1 = (int)(y1 < 0 ? 0 : y1);
    *sx2 = (int)(x2 >= ctx->frame_w ? ctx->frame_w - 1 : x2);
    *sy2 = (int)(y2 >= ctx->frame_h ? ctx->frame_h - 1 : y2);
    return *sx1 <= *sx2 && *sy1 <= *sy2;
}

static void *op_coordinate_text(int x, int y) {
    char result[64];
    snprintf(result, sizeof(result), "%d,%d", x, y);
    return xrtStrDup(result);
}

void *xop_get_color(void *ptr, long long x, long long y) {
    op_context *ctx = (op_context *)ptr;
    char result[8];
    if (!ctx) { opSetError("op: null context"); return NULL; }
    if (!op_capture_frame(ctx)) return NULL;
    if (x < 0 || x >= ctx->frame_w || y < 0 || y >= ctx->frame_h) {
        opSetError("op: coordinates out of range");
        return NULL;
    }
    int idx = ((int)y * ctx->frame_w + (int)x) * 4;
    BYTE b = ctx->frame_buf[idx];
    BYTE g = ctx->frame_buf[idx + 1];
    BYTE r = ctx->frame_buf[idx + 2];
    snprintf(result, sizeof(result), "%02X%02X%02X", r, g, b);
    return xrtStrDup(result);
}

void *xop_get_colors(void *ptr, long long x1, long long y1,
                     long long x2, long long y2) {
    op_context *ctx = (op_context *)ptr;
    int sx1, sy1, sx2, sy2;
    xbuffer result;
    int yy, xx;
    if (!ctx) { opSetError("op: null context"); return NULL; }
    if (!op_capture_frame(ctx)) return NULL;
    if (!op_clip_search_rect(ctx, x1, y1, x2, y2,
                             &sx1, &sy1, &sx2, &sy2)) return xrtStrDup("");

    /* 每个像素占六个十六进制字符和一个分隔符，拒绝截断。 */
    if ((size_t)(sx2 - sx1 + 1) * (size_t)(sy2 - sy1 + 1) >
        (16u * 1024u * 1024u - 1u) / 7u) {
        opSetError("op: color result exceeds output limit");
        return NULL;
    }
    if (!xrtBufferInit(&result)) return NULL;

    for (yy = sy1; yy <= sy2; yy++) {
        for (xx = sx1; xx <= sx2; xx++) {
            int idx = (yy * ctx->frame_w + xx) * 4;
            BYTE b = ctx->frame_buf[idx];
            BYTE g = ctx->frame_buf[idx + 1];
            BYTE r = ctx->frame_buf[idx + 2];
            char pixel[8];
            int size = snprintf(pixel, sizeof(pixel), "%s%02X%02X%02X",
                                result.Size ? "|" : "", r, g, b);
            if (size < 0 || !xrtBufferAppend(&result,
                              (xbytesview){(const uint8 *)pixel, (size_t)size})) {
                xrtBufferUnit(&result);
                return NULL;
            }
        }
    }
    if (!xrtBufferAppendByte(&result, 0)) { xrtBufferUnit(&result); return NULL; }
    return xrtBufferTake(&result, NULL, NULL);
}

static int op_save_bmp(const char *path, BYTE *buf, int w, int h) {
    xfile file;
    int data_size;
    int ok;
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER ih;

    if (!path || !buf || w <= 0 || h <= 0 || w > 4096 || h > 4096)
        return 0;
    data_size = w * h * 4;
    memset(&fh, 0, sizeof(fh));
    fh.bfType = 0x4D42; /* 'BM' */
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + data_size;

    memset(&ih, 0, sizeof(ih));
    ih.biSize = sizeof(ih);
    ih.biWidth = w;
    ih.biHeight = -h; /* top-down */
    ih.biPlanes = 1;
    ih.biBitCount = 32;
    ih.biCompression = BI_RGB;
    ih.biSizeImage = data_size;

    file = xrtOpen(path, XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE);
    if (!file) return 0;
    ok = xrtWriteFull(file, &fh, sizeof(fh), NULL) &&
         xrtWriteFull(file, &ih, sizeof(ih), NULL) &&
         xrtWriteFull(file, buf, (size_t)data_size, NULL);
    if (!xrtClose(file)) ok = 0;
    return ok;
}

long long xop_capture(void *ptr, long long x1, long long y1,
                      long long x2, long long y2, const char *file) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    if (!file || !*file) { opSetError("op: empty file path"); return 0; }
    if (!op_capture_frame(ctx)) return 0;

    int w = (int)(x2 - x1);
    int h = (int)(y2 - y1);
    if (w <= 0 || h <= 0) { opSetError("op: invalid capture region"); return 0; }

    /* clip to frame */
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x1 + w > ctx->frame_w) w = ctx->frame_w - (int)x1;
    if (y1 + h > ctx->frame_h) h = ctx->frame_h - (int)y1;
    if (w <= 0 || h <= 0) { opSetError("op: capture region out of bounds"); return 0; }

    /* copy sub-region */
    BYTE *sub = (BYTE *)malloc(w * h * 4);
    if (!sub) { opSetError("op: out of memory"); return 0; }
    int row;
    for (row = 0; row < h; row++) {
        memcpy(sub + row * w * 4,
               ctx->frame_buf + ((int)y1 + row) * ctx->frame_w * 4 + (int)x1 * 4,
               w * 4);
    }

    int ok = op_save_bmp(file, sub, w, h);
    free(sub);
    if (!ok) opSetError("op: failed to write bmp file");
    return ok;
}

void *xop_get_frame_info(void *ptr) {
    op_context *ctx = (op_context *)ptr;
    char result[64];
    if (!ctx) { opSetError("op: null context"); return NULL; }
    if (!op_capture_frame(ctx)) return xrtStrDup("0x0");
    snprintf(result, sizeof(result), "%dx%d", ctx->frame_w, ctx->frame_h);
    return xrtStrDup(result);
}

/* ------------------------------------------------------------------ */
/* Color parsing & matching                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    int r, g, b;
    int dr, dg, db;
    int negate; /* 1 if prefixed with @ */
} op_color_t;

#define OP_MAX_COLORS 64

static int op_hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int op_hex2(const char *s, int *out) {
    int high = op_hex_digit(s[0]);
    int low = op_hex_digit(s[1]);
    if (high < 0 || low < 0) return 0;
    *out = high * 16 + low;
    return 1;
}

/* Parse "RRGGBB" or "RRGGBB-DRDGDB", optionally prefixed with @ */
static int op_parse_one_color(const char *s, size_t length, op_color_t *out) {
    op_color_t value = {0};
    if (length != 0u && *s == '@') {
        value.negate = 1;
        ++s;
        --length;
    }
    if ((length != 6u && length != 13u) ||
        !op_hex2(s, &value.r) || !op_hex2(s + 2, &value.g) ||
        !op_hex2(s + 4, &value.b)) return 0;
    if (length == 13u && (s[6] != '-' ||
        !op_hex2(s + 7, &value.dr) || !op_hex2(s + 9, &value.dg) ||
        !op_hex2(s + 11, &value.db))) return 0;
    *out = value;
    return 1;
}

/* Parse multi-color "RRGGBB-DRDGDB|RRGGBB|..." */
static int op_parse_colors(const char *color_str, op_color_t *colors, int max) {
    int count = 0;
    const char *p = color_str;
    while (*p != '\0') {
        const char *sep = strchr(p, '|');
        size_t length = sep != NULL ? (size_t)(sep - p) : strlen(p);
        if (count == max || !op_parse_one_color(p, length, &colors[count])) return 0;
        ++count;
        if (sep == NULL) break;
        p = sep + 1;
        if (*p == '\0') return 0;
    }
    return count;
}

static int op_parse_delta_color(const char *text, int *r, int *g, int *b) {
    *r = *g = *b = 0;
    if (text == NULL || *text == '\0') return 1;
    return strlen(text) == 6u && op_hex2(text, r) &&
        op_hex2(text + 2, g) && op_hex2(text + 4, b);
}

static int op_pixel_match(BYTE pr, BYTE pg, BYTE pb, const op_color_t *c, double sim) {
    int diff_r = abs((int)pr - c->r);
    int diff_g = abs((int)pg - c->g);
    int diff_b = abs((int)pb - c->b);

    int max_dr = c->dr > 0 ? c->dr : (int)((1.0 - sim) * 255);
    int max_dg = c->dg > 0 ? c->dg : (int)((1.0 - sim) * 255);
    int max_db = c->db > 0 ? c->db : (int)((1.0 - sim) * 255);

    int matched = (diff_r <= max_dr && diff_g <= max_dg && diff_b <= max_db);
    return c->negate ? !matched : matched;
}

/* Check if pixel matches any of the color list */
static int op_match_any(BYTE pr, BYTE pg, BYTE pb, op_color_t *colors, int count, double sim) {
    int i;
    for (i = 0; i < count; i++) {
        if (op_pixel_match(pr, pg, pb, &colors[i], sim)) return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* FindColor                                                           */
/* ------------------------------------------------------------------ */

void *xop_find_color(void *ptr, long long x1, long long y1,
                      long long x2, long long y2,
                      const char *color, double sim, long long dir) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return NULL; }
    if (!color || !*color) { opSetError("op: empty color"); return NULL; }
    if (!op_capture_frame(ctx)) return NULL;

    op_color_t colors[OP_MAX_COLORS];
    int ncolors = op_parse_colors(color, colors, OP_MAX_COLORS);
    if (ncolors == 0) { opSetError("op: invalid color format"); return NULL; }

    int sx1, sy1, sx2, sy2;
    if (!op_clip_search_rect(ctx, x1, y1, x2, y2,
                             &sx1, &sy1, &sx2, &sy2)) return xrtStrDup("");

    int x_start, x_end, x_step, y_start, y_end, y_step;
    if (dir == 2 || dir == 3) { x_start = sx2; x_end = sx1; x_step = -1; }
    else { x_start = sx1; x_end = sx2; x_step = 1; }
    if (dir == 1 || dir == 3) { y_start = sy2; y_end = sy1; y_step = -1; }
    else { y_start = sy1; y_end = sy2; y_step = 1; }

    int yy, xx;
    for (yy = y_start; ; yy += y_step) {
        for (xx = x_start; ; xx += x_step) {
            int idx = (yy * ctx->frame_w + xx) * 4;
            BYTE b = ctx->frame_buf[idx];
            BYTE g = ctx->frame_buf[idx + 1];
            BYTE r = ctx->frame_buf[idx + 2];
            if (op_match_any(r, g, b, colors, ncolors, sim)) {
                return op_coordinate_text(xx, yy);
            }
            if (xx == x_end) break;
        }
        if (yy == y_end) break;
    }
    return xrtStrDup("");
}

void *xop_find_color_ex(void *ptr, long long x1, long long y1,
                        long long x2, long long y2,
                        const char *color, double sim, long long dir) {
    op_context *ctx = (op_context *)ptr;
    xbuffer result;
    if (!ctx) { opSetError("op: null context"); return NULL; }
    if (!color || !*color) { opSetError("op: empty color"); return NULL; }
    if (!op_capture_frame(ctx)) return NULL;

    op_color_t colors[OP_MAX_COLORS];
    int ncolors = op_parse_colors(color, colors, OP_MAX_COLORS);
    if (ncolors == 0) { opSetError("op: invalid color format"); return NULL; }

    int sx1, sy1, sx2, sy2;
    if (!op_clip_search_rect(ctx, x1, y1, x2, y2,
                             &sx1, &sy1, &sx2, &sy2)) return xrtStrDup("");
    if (!xrtBufferInit(&result)) return NULL;

    int x_start, x_end, x_step, y_start, y_end, y_step;
    if (dir == 2 || dir == 3) { x_start = sx2; x_end = sx1; x_step = -1; }
    else { x_start = sx1; x_end = sx2; x_step = 1; }
    if (dir == 1 || dir == 3) { y_start = sy2; y_end = sy1; y_step = -1; }
    else { y_start = sy1; y_end = sy2; y_step = 1; }

    int yy, xx;
    for (yy = y_start; ; yy += y_step) {
        for (xx = x_start; ; xx += x_step) {
            int idx = (yy * ctx->frame_w + xx) * 4;
            BYTE b = ctx->frame_buf[idx];
            BYTE g = ctx->frame_buf[idx + 1];
            BYTE r = ctx->frame_buf[idx + 2];
            if (op_match_any(r, g, b, colors, ncolors, sim)) {
                char entry[48];
                int size = snprintf(entry, sizeof(entry), "%s%d,%d",
                                    result.Size ? "|" : "", xx, yy);
                if (size < 0 || result.Size > 16u * 1024u * 1024u - 1u - (size_t)size) {
                    opSetError("op: color matches exceed output limit");
                    xrtBufferUnit(&result);
                    return NULL;
                }
                if (!xrtBufferAppend(&result,
                         (xbytesview){(const uint8 *)entry, (size_t)size})) {
                    xrtBufferUnit(&result);
                    return NULL;
                }
            }
            if (xx == x_end) break;
        }
        if (yy == y_end) break;
    }
    if (!xrtBufferAppendByte(&result, 0)) { xrtBufferUnit(&result); return NULL; }
    return xrtBufferTake(&result, NULL, NULL);
}

/* ------------------------------------------------------------------ */
/* FindMultiColor                                                      */
/* offset format: "x1|y1|RRGGBB-DRDGDB,x2|y2|RRGGBB-DRDGDB,..."      */
/* ------------------------------------------------------------------ */

typedef struct {
    int ox, oy;
    op_color_t color;
} op_offset_point_t;

#define OP_MAX_OFFSETS 128

static int op_parse_offset_int(const char *begin, const char *end, int *out) {
    char *tail;
    long value;
    if (begin == end || (*begin != '+' && *begin != '-' &&
        (*begin < '0' || *begin > '9'))) return 0;
    errno = 0;
    value = strtol(begin, &tail, 10);
    if (errno == ERANGE || tail != end || value < INT_MIN || value > INT_MAX) return 0;
    *out = (int)value;
    return 1;
}

static int op_parse_offsets(const char *s, op_offset_point_t *pts, int max) {
    int count = 0;
    const char *p = s;
    while (*p != '\0') {
        const char *comma = strchr(p, ',');
        const char *end = comma != NULL ? comma : p + strlen(p);
        const char *pipe1 = memchr(p, '|', (size_t)(end - p));
        const char *pipe2 = pipe1 != NULL ?
            memchr(pipe1 + 1, '|', (size_t)(end - pipe1 - 1)) : NULL;
        if (count == max || pipe2 == NULL ||
            !op_parse_offset_int(p, pipe1, &pts[count].ox) ||
            !op_parse_offset_int(pipe1 + 1, pipe2, &pts[count].oy) ||
            !op_parse_one_color(pipe2 + 1, (size_t)(end - pipe2 - 1),
                &pts[count].color)) return 0;
        count++;
        if (comma == NULL) break;
        p = comma + 1;
        if (*p == '\0') return 0;
    }
    return count;
}

void *xop_find_multi_color(void *ptr, long long x1, long long y1,
                            long long x2, long long y2,
                            const char *first_color, const char *offset_color,
                            double sim, long long dir) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return NULL; }
    if (!first_color || !*first_color) { opSetError("op: empty first_color"); return NULL; }
    if (!offset_color || !*offset_color) { opSetError("op: empty offset_color"); return NULL; }
    if (!op_capture_frame(ctx)) return NULL;

    op_color_t first_colors[OP_MAX_COLORS];
    int nfirst = op_parse_colors(first_color, first_colors, OP_MAX_COLORS);
    if (nfirst == 0) { opSetError("op: invalid first_color"); return NULL; }

    op_offset_point_t offsets[OP_MAX_OFFSETS];
    int noffsets = op_parse_offsets(offset_color, offsets, OP_MAX_OFFSETS);
    if (noffsets == 0) { opSetError("op: invalid offset_color"); return NULL; }

    int sx1, sy1, sx2, sy2;
    if (!op_clip_search_rect(ctx, x1, y1, x2, y2,
                             &sx1, &sy1, &sx2, &sy2)) return xrtStrDup("");

    int x_start, x_end, x_step, y_start, y_end, y_step;
    if (dir == 2 || dir == 3) { x_start = sx2; x_end = sx1; x_step = -1; }
    else { x_start = sx1; x_end = sx2; x_step = 1; }
    if (dir == 1 || dir == 3) { y_start = sy2; y_end = sy1; y_step = -1; }
    else { y_start = sy1; y_end = sy2; y_step = 1; }

    int yy, xx;
    for (yy = y_start; ; yy += y_step) {
        for (xx = x_start; ; xx += x_step) {
            int idx = (yy * ctx->frame_w + xx) * 4;
            BYTE b = ctx->frame_buf[idx];
            BYTE g = ctx->frame_buf[idx + 1];
            BYTE r = ctx->frame_buf[idx + 2];
            if (!op_match_any(r, g, b, first_colors, nfirst, sim)) goto next_mc;

            /* verify all offset points */
            {
                int k, all_ok = 1;
                for (k = 0; k < noffsets; k++) {
                    int64_t px = (int64_t)xx + offsets[k].ox;
                    int64_t py = (int64_t)yy + offsets[k].oy;
                    if (px < 0 || px >= ctx->frame_w || py < 0 || py >= ctx->frame_h) {
                        all_ok = 0; break;
                    }
                    int pidx = (int)((py * ctx->frame_w + px) * 4);
                    BYTE pb = ctx->frame_buf[pidx];
                    BYTE pg = ctx->frame_buf[pidx + 1];
                    BYTE pr = ctx->frame_buf[pidx + 2];
                    if (!op_pixel_match(pr, pg, pb, &offsets[k].color, sim)) {
                        all_ok = 0; break;
                    }
                }
                if (all_ok) {
                    return op_coordinate_text(xx, yy);
                }
            }
next_mc:
            if (xx == x_end) break;
        }
        if (yy == y_end) break;
    }
    return xrtStrDup("");
}

/* ------------------------------------------------------------------ */
/* FindPic - BMP template matching                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    BYTE *data;  /* BGRA */
    int w, h;
} op_bitmap_t;

static int op_load_bmp(const char *path, op_bitmap_t *bmp) {
    BYTE *raw;
    size_t size;
    size_t row_size;
    size_t pixel_bytes;
    size_t offset;
    int w, h, top_down, bpp;
    int row, col;
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER ih;

    if (!path || !bmp) return 0;
    raw = xrtFileReadAllLimit(path, 65u * 1024u * 1024u, &size);
    if (!raw) return 0;
    if (size < sizeof(fh) + sizeof(ih)) goto invalid;
    memcpy(&fh, raw, sizeof(fh));
    memcpy(&ih, raw + sizeof(fh), sizeof(ih));
    if (fh.bfType != 0x4D42 || ih.biSize < sizeof(ih) ||
        ih.biSize > size - sizeof(fh) || ih.biPlanes != 1 ||
        ih.biCompression != BI_RGB || (ih.biBitCount != 24 && ih.biBitCount != 32) ||
        ih.biWidth <= 0 || ih.biWidth > 4096 ||
        ih.biHeight == 0 || ih.biHeight < -4096 || ih.biHeight > 4096)
        goto invalid;

    w = ih.biWidth;
    h = ih.biHeight < 0 ? -ih.biHeight : ih.biHeight;
    top_down = ih.biHeight < 0;
    bpp = ih.biBitCount;
    row_size = (((size_t)w * bpp + 31u) / 32u) * 4u;
    pixel_bytes = row_size * (size_t)h;
    offset = fh.bfOffBits;
    if (offset < sizeof(fh) + ih.biSize || offset > size ||
        pixel_bytes > size - offset)
        goto invalid;

    bmp->w = w;
    bmp->h = h;
    bmp->data = (BYTE *)malloc((size_t)w * (size_t)h * 4u);
    if (!bmp->data) goto invalid;

    for (row = 0; row < h; row++) {
        int src_y = top_down ? row : (h - 1 - row);
        BYTE *src = raw + offset + (size_t)src_y * row_size;
        BYTE *dst = bmp->data + (size_t)row * (size_t)w * 4u;
        for (col = 0; col < w; col++) {
            if (bpp == 32) {
                dst[col * 4 + 0] = src[col * 4 + 0]; /* B */
                dst[col * 4 + 1] = src[col * 4 + 1]; /* G */
                dst[col * 4 + 2] = src[col * 4 + 2]; /* R */
                dst[col * 4 + 3] = src[col * 4 + 3]; /* A */
            } else {
                dst[col * 4 + 0] = src[col * 3 + 0];
                dst[col * 4 + 1] = src[col * 3 + 1];
                dst[col * 4 + 2] = src[col * 3 + 2];
                dst[col * 4 + 3] = 255;
            }
        }
    }
    xrtFree(raw);
    return 1;

invalid:
    xrtFree(raw);
    return 0;
}

static int op_match_at(BYTE *screen, int sw, int sx, int sy,
                       op_bitmap_t *tmpl, int dr, int dg, int db, double sim) {
    int tw = tmpl->w, th = tmpl->h;
    int total = tw * th, matched = 0;
    int ty, tx;
    for (ty = 0; ty < th; ty++) {
        for (tx = 0; tx < tw; tx++) {
            int si = ((sy + ty) * sw + (sx + tx)) * 4;
            int ti = (ty * tw + tx) * 4;
            /* skip fully transparent template pixels */
            if (tmpl->data[ti + 3] == 0) { matched++; continue; }
            if (abs((int)screen[si + 2] - (int)tmpl->data[ti + 2]) <= dr &&
                abs((int)screen[si + 1] - (int)tmpl->data[ti + 1]) <= dg &&
                abs((int)screen[si]     - (int)tmpl->data[ti])     <= db) {
                matched++;
            }
        }
    }
    return (double)matched / (double)total >= sim;
}

void *xop_find_pic(void *ptr, long long x1, long long y1,
                    long long x2, long long y2,
                    const char *pic_file, const char *delta_color,
                    double sim, long long dir) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return NULL; }
    if (!pic_file || !*pic_file) { opSetError("op: empty pic_file"); return NULL; }
    if (!op_capture_frame(ctx)) return NULL;

    op_bitmap_t tmpl;
    if (!op_load_bmp(pic_file, &tmpl)) { opSetError("op: failed to load bmp"); return NULL; }

    /* parse delta color "RRGGBB" as tolerance */
    int dr = 0, dg = 0, db = 0;
    if (!op_parse_delta_color(delta_color, &dr, &dg, &db)) {
        free(tmpl.data);
        opSetError("op: invalid delta_color");
        return NULL;
    }

    int sx1, sy1, sx2, sy2;
    if (!op_clip_search_rect(ctx, x1, y1, x2, y2,
                             &sx1, &sy1, &sx2, &sy2)) {
        free(tmpl.data);
        return xrtStrDup("");
    }

    /* limit search so template fits */
    if (sx2 - sx1 + 1 < tmpl.w || sy2 - sy1 + 1 < tmpl.h) {
        free(tmpl.data);
        return xrtStrDup("");
    }

    int x_start, x_end, x_step, y_start, y_end, y_step;
    if (dir == 2 || dir == 3) { x_start = sx2 - tmpl.w + 1; x_end = sx1; x_step = -1; }
    else { x_start = sx1; x_end = sx2 - tmpl.w + 1; x_step = 1; }
    if (dir == 1 || dir == 3) { y_start = sy2 - tmpl.h + 1; y_end = sy1; y_step = -1; }
    else { y_start = sy1; y_end = sy2 - tmpl.h + 1; y_step = 1; }

    int yy, xx;
    for (yy = y_start; ; yy += y_step) {
        for (xx = x_start; ; xx += x_step) {
            if (op_match_at(ctx->frame_buf, ctx->frame_w, xx, yy, &tmpl, dr, dg, db, sim)) {
                void *result = op_coordinate_text(xx, yy);
                free(tmpl.data);
                return result;
            }
            if (xx == x_end) break;
        }
        if (yy == y_end) break;
    }
    free(tmpl.data);
    return xrtStrDup("");
}

void *xop_find_pic_ex(void *ptr, long long x1, long long y1,
                       long long x2, long long y2,
                       const char *pic_file, const char *delta_color,
                       double sim, long long dir) {
    op_context *ctx = (op_context *)ptr;
    xbuffer result;
    if (!ctx) { opSetError("op: null context"); return NULL; }
    if (!pic_file || !*pic_file) { opSetError("op: empty pic_file"); return NULL; }
    if (!op_capture_frame(ctx)) return NULL;

    op_bitmap_t tmpl;
    if (!op_load_bmp(pic_file, &tmpl)) { opSetError("op: failed to load bmp"); return NULL; }

    int dr = 0, dg = 0, db = 0;
    if (!op_parse_delta_color(delta_color, &dr, &dg, &db)) {
        free(tmpl.data);
        opSetError("op: invalid delta_color");
        return NULL;
    }

    int sx1, sy1, sx2, sy2;
    if (!op_clip_search_rect(ctx, x1, y1, x2, y2,
                             &sx1, &sy1, &sx2, &sy2)) {
        free(tmpl.data);
        return xrtStrDup("");
    }

    if (sx2 - sx1 + 1 < tmpl.w || sy2 - sy1 + 1 < tmpl.h) {
        free(tmpl.data);
        return xrtStrDup("");
    }
    if (!xrtBufferInit(&result)) { free(tmpl.data); return NULL; }

    int x_start, x_end, x_step, y_start, y_end, y_step;
    if (dir == 2 || dir == 3) { x_start = sx2 - tmpl.w + 1; x_end = sx1; x_step = -1; }
    else { x_start = sx1; x_end = sx2 - tmpl.w + 1; x_step = 1; }
    if (dir == 1 || dir == 3) { y_start = sy2 - tmpl.h + 1; y_end = sy1; y_step = -1; }
    else { y_start = sy1; y_end = sy2 - tmpl.h + 1; y_step = 1; }

    int yy, xx;
    for (yy = y_start; ; yy += y_step) {
        for (xx = x_start; ; xx += x_step) {
            if (op_match_at(ctx->frame_buf, ctx->frame_w, xx, yy, &tmpl, dr, dg, db, sim)) {
                char entry[48];
                int size = snprintf(entry, sizeof(entry), "%s%d,%d",
                                    result.Size ? "|" : "", xx, yy);
                if (size < 0 || result.Size > 16u * 1024u * 1024u - 1u - (size_t)size) {
                    opSetError("op: picture matches exceed output limit");
                    xrtBufferUnit(&result);
                    free(tmpl.data);
                    return NULL;
                }
                if (!xrtBufferAppend(&result,
                         (xbytesview){(const uint8 *)entry, (size_t)size})) {
                    xrtBufferUnit(&result);
                    free(tmpl.data);
                    return NULL;
                }
            }
            if (xx == x_end) break;
        }
        if (yy == y_end) break;
    }
    free(tmpl.data);
    if (!xrtBufferAppendByte(&result, 0)) { xrtBufferUnit(&result); return NULL; }
    return xrtBufferTake(&result, NULL, NULL);
}

/* ------------------------------------------------------------------ */
/* Mouse simulation                                                    */
/* ------------------------------------------------------------------ */

static void op_mouse_send(op_context *ctx, DWORD flags, int x, int y, DWORD data) {
    if (strcmp(ctx->mouse_mode, "windows") == 0 && ctx->bind_hwnd) {
        /* background mode: PostMessage */
        POINT pt = {x, y};
        ClientToScreen(ctx->bind_hwnd, &pt);
        ScreenToClient(ctx->bind_hwnd, &pt);
        LPARAM lp = MAKELPARAM(pt.x, pt.y);

        if (flags & MOUSEEVENTF_MOVE) {
            PostMessage(ctx->bind_hwnd, WM_MOUSEMOVE, 0, lp);
        }
        if (flags & MOUSEEVENTF_LEFTDOWN) {
            PostMessage(ctx->bind_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
        }
        if (flags & MOUSEEVENTF_LEFTUP) {
            PostMessage(ctx->bind_hwnd, WM_LBUTTONUP, 0, lp);
        }
        if (flags & MOUSEEVENTF_RIGHTDOWN) {
            PostMessage(ctx->bind_hwnd, WM_RBUTTONDOWN, MK_RBUTTON, lp);
        }
        if (flags & MOUSEEVENTF_RIGHTUP) {
            PostMessage(ctx->bind_hwnd, WM_RBUTTONUP, 0, lp);
        }
        if (flags & MOUSEEVENTF_MIDDLEDOWN) {
            PostMessage(ctx->bind_hwnd, WM_MBUTTONDOWN, MK_MBUTTON, lp);
        }
        if (flags & MOUSEEVENTF_MIDDLEUP) {
            PostMessage(ctx->bind_hwnd, WM_MBUTTONUP, 0, lp);
        }
        if (flags & MOUSEEVENTF_WHEEL) {
            PostMessage(ctx->bind_hwnd, WM_MOUSEWHEEL,
                        MAKEWPARAM(0, (short)data), lp);
        }
    } else {
        /* foreground mode */
        if (flags & MOUSEEVENTF_MOVE) {
            POINT pt = {x, y};
            if (ctx->bind_hwnd) ClientToScreen(ctx->bind_hwnd, &pt);
            SetCursorPos(pt.x, pt.y);
        } else {
            mouse_event(flags, 0, 0, data, 0);
        }
    }
}

long long xop_move_to(void *ptr, long long x, long long y) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    op_mouse_send(ctx, MOUSEEVENTF_MOVE, (int)x, (int)y, 0);
    return 1;
}

long long xop_left_click(void *ptr) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    op_mouse_send(ctx, MOUSEEVENTF_LEFTDOWN, 0, 0, 0);
    op_mouse_send(ctx, MOUSEEVENTF_LEFTUP, 0, 0, 0);
    return 1;
}

long long xop_right_click(void *ptr) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    op_mouse_send(ctx, MOUSEEVENTF_RIGHTDOWN, 0, 0, 0);
    op_mouse_send(ctx, MOUSEEVENTF_RIGHTUP, 0, 0, 0);
    return 1;
}

long long xop_middle_click(void *ptr) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    op_mouse_send(ctx, MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0);
    op_mouse_send(ctx, MOUSEEVENTF_MIDDLEUP, 0, 0, 0);
    return 1;
}

long long xop_left_down(void *ptr) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    op_mouse_send(ctx, MOUSEEVENTF_LEFTDOWN, 0, 0, 0);
    return 1;
}

long long xop_left_up(void *ptr) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    op_mouse_send(ctx, MOUSEEVENTF_LEFTUP, 0, 0, 0);
    return 1;
}

long long xop_right_down(void *ptr) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    op_mouse_send(ctx, MOUSEEVENTF_RIGHTDOWN, 0, 0, 0);
    return 1;
}

long long xop_right_up(void *ptr) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    op_mouse_send(ctx, MOUSEEVENTF_RIGHTUP, 0, 0, 0);
    return 1;
}

long long xop_wheel_up(void *ptr) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    op_mouse_send(ctx, MOUSEEVENTF_WHEEL, 0, 0, 120);
    return 1;
}

long long xop_wheel_down(void *ptr) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    op_mouse_send(ctx, MOUSEEVENTF_WHEEL, 0, 0, (DWORD)(-120));
    return 1;
}

long long xop_drag_to(void *ptr, long long x1, long long y1,
                      long long x2, long long y2) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    op_mouse_send(ctx, MOUSEEVENTF_MOVE, (int)x1, (int)y1, 0);
    op_mouse_send(ctx, MOUSEEVENTF_LEFTDOWN, 0, 0, 0);
    op_mouse_send(ctx, MOUSEEVENTF_MOVE, (int)x2, (int)y2, 0);
    op_mouse_send(ctx, MOUSEEVENTF_LEFTUP, 0, 0, 0);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Keyboard simulation                                                 */
/* ------------------------------------------------------------------ */

long long xop_key_press(void *ptr, long long vk) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    if (strcmp(ctx->keypad_mode, "windows") == 0 && ctx->bind_hwnd) {
        PostMessage(ctx->bind_hwnd, WM_KEYDOWN, (WPARAM)vk, 0);
        PostMessage(ctx->bind_hwnd, WM_KEYUP, (WPARAM)vk, 0);
    } else {
        keybd_event((BYTE)vk, 0, 0, 0);
        keybd_event((BYTE)vk, 0, KEYEVENTF_KEYUP, 0);
    }
    return 1;
}

long long xop_key_down(void *ptr, long long vk) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    if (strcmp(ctx->keypad_mode, "windows") == 0 && ctx->bind_hwnd) {
        PostMessage(ctx->bind_hwnd, WM_KEYDOWN, (WPARAM)vk, 0);
    } else {
        keybd_event((BYTE)vk, 0, 0, 0);
    }
    return 1;
}

long long xop_key_up(void *ptr, long long vk) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    if (strcmp(ctx->keypad_mode, "windows") == 0 && ctx->bind_hwnd) {
        PostMessage(ctx->bind_hwnd, WM_KEYUP, (WPARAM)vk, 0);
    } else {
        keybd_event((BYTE)vk, 0, KEYEVENTF_KEYUP, 0);
    }
    return 1;
}

static int op_send_unicode_unit(op_context *ctx, uint16 unit) {
    INPUT inp;

    if (strcmp(ctx->keypad_mode, "windows") == 0 && ctx->bind_hwnd)
        return PostMessageW(ctx->bind_hwnd, WM_CHAR, (WPARAM)unit, 0) != 0;
    if (!p_SendInput) return 0;
    memset(&inp, 0, sizeof(inp));
    inp.type = INPUT_KEYBOARD;
    inp.ki.wScan = unit;
    inp.ki.dwFlags = KEYEVENTF_UNICODE;
    if (p_SendInput(1, &inp, sizeof(inp)) != 1) return 0;
    inp.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    return p_SendInput(1, &inp, sizeof(inp)) == 1;
}

long long xop_key_press_str(void *ptr, const char *str, long long delay) {
    op_context *ctx = (op_context *)ptr;
    uint16 *wide;
    size_t units;
    size_t i;
    DWORD ms;

    if (!ctx) { opSetError("op: null context"); return 0; }
    if (!str) return 0;
    wide = xrtUtf8To16(str, &units);
    if (!wide) { opSetError("op: invalid UTF-8 keyboard text"); return 0; }
    ms = delay > 0 ? (DWORD)(delay > 0xffffffffLL ? 0xffffffffLL : delay) : 50;
    if (strcmp(ctx->keypad_mode, "windows") != 0 || !ctx->bind_hwnd)
        op_init_user32();

    for (i = 0; i < units; ++i) {
        if (!op_send_unicode_unit(ctx, wide[i])) break;
        /* 代理对属于一个字符，延迟只放在完整字符后。 */
        if (!(wide[i] >= 0xD800 && wide[i] <= 0xDBFF &&
              i + 1 < units && wide[i + 1] >= 0xDC00 && wide[i + 1] <= 0xDFFF))
            Sleep(ms);
    }
    xrtFree(wide);
    if (i != units) { opSetError("op: cannot send Unicode keyboard text"); return 0; }
    return 1;
}

long long xop_key_char(void *ptr, long long char_code) {
    op_context *ctx = (op_context *)ptr;
    uint16 units[2];
    size_t count = 1;
    size_t i;

    if (!ctx) { opSetError("op: null context"); return 0; }
    if (char_code < 0 || char_code > 0x10FFFF ||
        (char_code >= 0xD800 && char_code <= 0xDFFF)) {
        opSetError("op: invalid Unicode character");
        return 0;
    }
    if (char_code <= 0xFFFF) {
        units[0] = (uint16)char_code;
    } else {
        uint32 scalar = (uint32)char_code - 0x10000;
        units[0] = (uint16)(0xD800 + (scalar >> 10));
        units[1] = (uint16)(0xDC00 + (scalar & 0x3FF));
        count = 2;
    }
    if (strcmp(ctx->keypad_mode, "windows") != 0 || !ctx->bind_hwnd)
        op_init_user32();
    for (i = 0; i < count; ++i) {
        if (!op_send_unicode_unit(ctx, units[i])) {
            opSetError("op: cannot send Unicode character");
            return 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Window finding (module-level, no instance)                          */
/* ------------------------------------------------------------------ */

long long xop_find_window(const char *class_name, const char *title) {
    uint16 *wide_class = (class_name && *class_name) ? xrtUtf8To16(class_name, NULL) : NULL;
    uint16 *wide_title = (title && *title) ? xrtUtf8To16(title, NULL) : NULL;
    HWND hwnd;

    if ((class_name && *class_name && !wide_class) ||
        (title && *title && !wide_title)) {
        xrtFree(wide_class);
        xrtFree(wide_title);
        return 0;
    }
    hwnd = FindWindowW((const WCHAR *)wide_class, (const WCHAR *)wide_title);
    xrtFree(wide_class);
    xrtFree(wide_title);
    return (long long)(intptr_t)hwnd;
}

typedef struct {
    uint16 *title_filter;
    uint16 *class_filter;
    int filter_visible;
    DWORD process_ids[1024];
    int process_count;
    char result[65536];
    int pos;
    int failed;
} op_enum_ctx_t;

static int op_enum_process_matches(const op_enum_ctx_t *ec, DWORD pid) {
    int i;

    if (!ec || ec->process_count == 0) return 1;
    for (i = 0; i < ec->process_count; ++i) {
        if (ec->process_ids[i] == pid) return 1;
    }
    return 0;
}

static void op_enum_collect_processes(op_enum_ctx_t *ec, const char *process) {
    op_process_entry32w pe;
    HANDLE snap;
    char *end = NULL;
    unsigned long long numeric_pid;
    uint16 *wide_process;

    if (!ec || !process || !*process) return;
    numeric_pid = strtoull(process, &end, 10);
    if (end != process && *end == '\0' && numeric_pid != 0) {
        if (numeric_pid > UINT32_MAX) { ec->failed = 1; return; }
        ec->process_ids[ec->process_count++] = (DWORD)numeric_pid;
        return;
    }

    wide_process = xrtUtf8To16(process, NULL);
    if (!wide_process) { ec->failed = 1; return; }
    op_init_toolhelp();
    if (!p_CreateToolhelp32Snapshot || !p_Process32First || !p_Process32Next) {
        xrtFree(wide_process);
        ec->failed = 1;
        return;
    }
    snap = p_CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        xrtFree(wide_process);
        ec->failed = 1;
        return;
    }

    memset(&pe, 0, sizeof(pe));
    pe.dwSize = sizeof(pe);
    if (p_Process32First(snap, &pe)) {
        do {
            if (wcsstr(pe.szExeFile, (const WCHAR *)wide_process) != NULL) {
                if (ec->process_count == (int)(sizeof(ec->process_ids) / sizeof(ec->process_ids[0]))) {
                    ec->failed = 1;
                    break;
                }
                ec->process_ids[ec->process_count++] = pe.th32ProcessID;
            }
        } while (p_Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    xrtFree(wide_process);
}

static BOOL CALLBACK op_enum_proc(HWND hwnd, LPARAM lParam) {
    op_enum_ctx_t *ec = (op_enum_ctx_t *)lParam;
    DWORD pid = 0;
    WCHAR class_name[256] = L"";

    if (ec->filter_visible && !IsWindowVisible(hwnd)) return TRUE;
    if (ec->process_count > 0) {
        GetWindowThreadProcessId(hwnd, &pid);
        if (!op_enum_process_matches(ec, pid)) return TRUE;
    }

    if (ec->title_filter) {
        int units = GetWindowTextLengthW(hwnd);
        uint16 *title;
        BOOL match;

        if (units <= 0 || units > 65535) return TRUE;
        title = (uint16 *)xrtMalloc(((size_t)units + 1u) * sizeof(uint16));
        if (!title) { ec->failed = 1; return FALSE; }
        title[0] = 0;
        if (!GetWindowTextW(hwnd, (WCHAR *)title, units + 1)) {
            xrtFree(title);
            return TRUE;
        }
        match = wcsstr((const WCHAR *)title,
                       (const WCHAR *)ec->title_filter) != NULL;
        xrtFree(title);
        if (!match) return TRUE;
    }
    if (ec->class_filter) {
        if (!GetClassNameW(hwnd, class_name, 256) ||
            !wcsstr(class_name, (const WCHAR *)ec->class_filter)) return TRUE;
    }

    {
        int remaining = (int)sizeof(ec->result) - ec->pos;
        int written = snprintf(ec->result + ec->pos, remaining,
                               "%s%lld", ec->pos ? "," : "",
                               (long long)(intptr_t)hwnd);
        if (written < 0 || written >= remaining) {
            ec->failed = 1;
            return FALSE;
        }
        ec->pos += written;
    }
    return TRUE;
}

void *xop_enum_window(long long parent, const char *title,
                      const char *class_name, long long filter) {
    op_enum_ctx_t ec;
    memset(&ec, 0, sizeof(ec));
    ec.title_filter = (title && *title) ? xrtUtf8To16(title, NULL) : NULL;
    ec.class_filter = (class_name && *class_name) ? xrtUtf8To16(class_name, NULL) : NULL;
    if ((title && *title && !ec.title_filter) ||
        (class_name && *class_name && !ec.class_filter)) {
        xrtFree(ec.title_filter);
        xrtFree(ec.class_filter);
        return xrtStrDup("");
    }
    ec.filter_visible = (filter & 1) ? 1 : 0;

    if (parent) {
        EnumChildWindows((HWND)(intptr_t)parent, op_enum_proc, (LPARAM)&ec);
    } else {
        EnumWindows(op_enum_proc, (LPARAM)&ec);
    }
    xrtFree(ec.title_filter);
    xrtFree(ec.class_filter);
    if (ec.failed) return xrtStrDup("");
    ec.result[ec.pos] = '\0';
    return xrtStrDup(ec.result);
}

void *xop_enum_window_by_process(const char *process, const char *title,
                                 const char *class_name, long long filter) {
    op_enum_ctx_t ec;

    memset(&ec, 0, sizeof(ec));
    ec.title_filter = (title && *title) ? xrtUtf8To16(title, NULL) : NULL;
    ec.class_filter = (class_name && *class_name) ? xrtUtf8To16(class_name, NULL) : NULL;
    if ((title && *title && !ec.title_filter) ||
        (class_name && *class_name && !ec.class_filter)) {
        xrtFree(ec.title_filter);
        xrtFree(ec.class_filter);
        return xrtStrDup("");
    }
    ec.filter_visible = (filter & 1) ? 1 : 0;
    op_enum_collect_processes(&ec, process);
    if (ec.failed) {
        xrtFree(ec.title_filter);
        xrtFree(ec.class_filter);
        return xrtStrDup("");
    }
    if (ec.process_count > 0) {
        EnumWindows(op_enum_proc, (LPARAM)&ec);
    }
    xrtFree(ec.title_filter);
    xrtFree(ec.class_filter);
    if (ec.failed) return xrtStrDup("");
    ec.result[ec.pos] = '\0';
    return xrtStrDup(ec.result);
}

void *xop_get_screen_size(void) {
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    char result[64];
    snprintf(result, sizeof(result), "%dx%d", w, h);
    return xrtStrDup(result);
}

void *xop_get_cursor_pos(void) {
    POINT pt;
    char result[64];
    GetCursorPos(&pt);
    snprintf(result, sizeof(result), "%ld,%ld", pt.x, pt.y);
    return xrtStrDup(result);
}

/* ------------------------------------------------------------------ */
/* Window management (module-level)                                    */
/* ------------------------------------------------------------------ */

void *xop_get_window_rect(long long hwnd) {
    RECT rc;
    char result[64];
    if (!GetWindowRect((HWND)(intptr_t)hwnd, &rc)) return xrtStrDup("0,0,0,0");
    snprintf(result, sizeof(result), "%ld,%ld,%ld,%ld", rc.left, rc.top, rc.right, rc.bottom);
    return xrtStrDup(result);
}

long long xop_move_window(long long hwnd, long long x, long long y) {
    HWND h = (HWND)(intptr_t)hwnd;
    if (!IsWindow(h)) return 0;
    RECT rc;
    GetWindowRect(h, &rc);
    return MoveWindow(h, (int)x, (int)y, rc.right - rc.left, rc.bottom - rc.top, TRUE);
}

long long xop_set_window_size(long long hwnd, long long w, long long ht) {
    HWND hw = (HWND)(intptr_t)hwnd;
    if (!IsWindow(hw)) return 0;
    RECT rc;
    GetWindowRect(hw, &rc);
    return MoveWindow(hw, rc.left, rc.top, (int)w, (int)ht, TRUE);
}

void *xop_get_window_title(long long hwnd) {
    HWND h = (HWND)(intptr_t)hwnd;
    int units;
    uint16 *wide;
    char *utf8;

    if (!IsWindow(h)) return xrtStrDup("");
    units = GetWindowTextLengthW(h);
    if (units < 0) return xrtStrDup("");
    wide = (uint16 *)xrtMalloc(((size_t)units + 1u) * sizeof(uint16));
    if (!wide) return NULL;
    if (!GetWindowTextW(h, (WCHAR *)wide, units + 1) && units != 0) {
        xrtFree(wide);
        return xrtStrDup("");
    }
    utf8 = xrtUtf16To8(wide, NULL);
    xrtFree(wide);
    return utf8;
}

long long xop_set_window_title(long long hwnd, const char *title) {
    HWND h = (HWND)(intptr_t)hwnd;
    uint16 *wide;
    BOOL ok;

    if (!IsWindow(h)) return 0;
    wide = xrtUtf8To16(title ? title : "", NULL);
    if (!wide) return 0;
    ok = SetWindowTextW(h, (const WCHAR *)wide);
    xrtFree(wide);
    return ok != 0;
}

long long xop_set_window_state(long long hwnd, long long mode) {
    HWND h = (HWND)(intptr_t)hwnd;
    if (!IsWindow(h)) return 0;
    int cmd;
    switch ((int)mode) {
        case 0: cmd = SW_HIDE; break;
        case 1: cmd = SW_SHOW; break;
        case 2: cmd = SW_MINIMIZE; break;
        case 3: cmd = SW_MAXIMIZE; break;
        case 4: cmd = SW_RESTORE; break;
        case 5: PostMessage(h, WM_CLOSE, 0, 0); return 1;
        default: cmd = SW_SHOW; break;
    }
    return ShowWindow(h, cmd);
}

long long xop_send_string(long long hwnd, const char *str) {
    HWND h = (HWND)(intptr_t)hwnd;
    uint16 *wide;
    size_t units;
    size_t i;

    if (!IsWindow(h) || !str) return 0;
    wide = xrtUtf8To16(str, &units);
    if (!wide) return 0;
    for (i = 0; i < units; ++i) {
        if (!PostMessageW(h, WM_CHAR, (WPARAM)wide[i], 0)) {
            xrtFree(wide);
            opSetError("op: cannot post window text");
            return 0;
        }
    }
    xrtFree(wide);
    return 1;
}

void *xop_client_to_screen(long long hwnd, long long x, long long y) {
    POINT pt = {(LONG)x, (LONG)y};
    char result[64];
    ClientToScreen((HWND)(intptr_t)hwnd, &pt);
    snprintf(result, sizeof(result), "%ld,%ld", pt.x, pt.y);
    return xrtStrDup(result);
}

void *xop_screen_to_client(long long hwnd, long long x, long long y) {
    POINT pt = {(LONG)x, (LONG)y};
    char result[64];
    ScreenToClient((HWND)(intptr_t)hwnd, &pt);
    snprintf(result, sizeof(result), "%ld,%ld", pt.x, pt.y);
    return xrtStrDup(result);
}

/* ------------------------------------------------------------------ */
/* FindColorBlock & GetColorNum                                        */
/* ------------------------------------------------------------------ */

void *xop_find_color_block(void *ptr, long long x1, long long y1,
                            long long x2, long long y2,
                            const char *color, double sim,
                            long long width, long long height) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return NULL; }
    if (!color || !*color) { opSetError("op: empty color"); return NULL; }
    if (!op_capture_frame(ctx)) return NULL;

    op_color_t colors[OP_MAX_COLORS];
    int ncolors = op_parse_colors(color, colors, OP_MAX_COLORS);
    if (ncolors == 0) { opSetError("op: invalid color format"); return NULL; }

    int sx1, sy1, sx2, sy2;
    if (!op_clip_search_rect(ctx, x1, y1, x2, y2,
                             &sx1, &sy1, &sx2, &sy2)) return xrtStrDup("");
    if (width < 1 || height < 1 || width > sx2 - sx1 + 1 ||
        height > sy2 - sy1 + 1) return xrtStrDup("");
    int bw = (int)width, bh = (int)height;

    int yy, xx;
    for (yy = sy1; yy <= sy2 - bh + 1; yy++) {
        for (xx = sx1; xx <= sx2 - bw + 1; xx++) {
            /* check if the block is mostly matching color */
            int total = bw * bh, matched = 0;
            int by, bx;
            for (by = 0; by < bh; by++) {
                for (bx = 0; bx < bw; bx++) {
                    int idx = ((yy + by) * ctx->frame_w + (xx + bx)) * 4;
                    BYTE b = ctx->frame_buf[idx];
                    BYTE g = ctx->frame_buf[idx + 1];
                    BYTE r = ctx->frame_buf[idx + 2];
                    if (op_match_any(r, g, b, colors, ncolors, sim)) matched++;
                }
            }
            if (matched == total) {
                return op_coordinate_text(xx, yy);
            }
        }
    }
    return xrtStrDup("");
}

long long xop_get_color_num(void *ptr, long long x1, long long y1,
                            long long x2, long long y2,
                            const char *color, double sim) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    if (!color || !*color) { opSetError("op: empty color"); return 0; }
    if (!op_capture_frame(ctx)) return 0;

    op_color_t colors[OP_MAX_COLORS];
    int ncolors = op_parse_colors(color, colors, OP_MAX_COLORS);
    if (ncolors == 0) { opSetError("op: invalid color format"); return 0; }

    int sx1, sy1, sx2, sy2;
    if (!op_clip_search_rect(ctx, x1, y1, x2, y2,
                             &sx1, &sy1, &sx2, &sy2)) return 0;

    long long count = 0;
    int yy, xx;
    for (yy = sy1; yy <= sy2; yy++) {
        for (xx = sx1; xx <= sx2; xx++) {
            int idx = (yy * ctx->frame_w + xx) * 4;
            BYTE b = ctx->frame_buf[idx];
            BYTE g = ctx->frame_buf[idx + 1];
            BYTE r = ctx->frame_buf[idx + 2];
            if (op_match_any(r, g, b, colors, ncolors, sim)) count++;
        }
    }
    return count;
}

/* ------------------------------------------------------------------ */
/* FindPicN - multi-picture matching                                   */
/* picFiles format: "1.bmp|2.bmp|3.bmp"                                */
/* Returns: "index,x,y" or "" if not found                             */
/* ------------------------------------------------------------------ */

void *xop_find_pic_n(void *ptr, long long x1, long long y1,
                      long long x2, long long y2,
                      const char *pic_files, const char *delta_color,
                      double sim, long long dir) {
    op_context *ctx = (op_context *)ptr;
    int sx1, sy1, sx2, sy2;
    if (!ctx) { opSetError("op: null context"); return NULL; }
    if (!pic_files || !*pic_files) { opSetError("op: empty pic_files"); return NULL; }
    if (!op_capture_frame(ctx)) return NULL;
    if (!op_clip_search_rect(ctx, x1, y1, x2, y2,
                             &sx1, &sy1, &sx2, &sy2)) return xrtStrDup("");

    int dr = 0, dg = 0, db = 0;
    if (!op_parse_delta_color(delta_color, &dr, &dg, &db)) {
        opSetError("op: invalid delta_color");
        return NULL;
    }

    char tmp[4096];
    if (strlen(pic_files) >= sizeof(tmp)) {
        opSetError("op: pic_files exceeds input limit");
        return NULL;
    }
    strncpy(tmp, pic_files, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    int index = 0;
    char *p = tmp;
    while (p && *p) {
        char *sep = strchr(p, '|');
        if (sep) *sep = '\0';

        op_bitmap_t tmpl;
        if (op_load_bmp(p, &tmpl)) {
            if (sx2 - sx1 + 1 >= tmpl.w && sy2 - sy1 + 1 >= tmpl.h) {
                int x_start, x_end, x_step, y_start, y_end, y_step;
                if (dir == 2 || dir == 3) { x_start = sx2 - tmpl.w + 1; x_end = sx1; x_step = -1; }
                else { x_start = sx1; x_end = sx2 - tmpl.w + 1; x_step = 1; }
                if (dir == 1 || dir == 3) { y_start = sy2 - tmpl.h + 1; y_end = sy1; y_step = -1; }
                else { y_start = sy1; y_end = sy2 - tmpl.h + 1; y_step = 1; }

                int yy, xx;
                for (yy = y_start; ; yy += y_step) {
                    for (xx = x_start; ; xx += x_step) {
                        if (op_match_at(ctx->frame_buf, ctx->frame_w, xx, yy, &tmpl, dr, dg, db, sim)) {
                            char result[64];
                            snprintf(result, sizeof(result), "%d,%d,%d", index, xx, yy);
                            free(tmpl.data);
                            return xrtStrDup(result);
                        }
                        if (xx == x_end) break;
                    }
                    if (yy == y_end) break;
                }
            }
            free(tmpl.data);
        }
        index++;
        p = sep ? sep + 1 : NULL;
    }
    return xrtStrDup("");
}

/* ------------------------------------------------------------------ */
/* CaptureToMem - returns hex string of BGRA pixels                    */
/* ------------------------------------------------------------------ */

void *xop_capture_to_mem(void *ptr, long long x1, long long y1,
                         long long x2, long long y2) {
    op_context *ctx = (op_context *)ptr;
    int sx1, sy1, sx2, sy2, w, h, head, x, y, k;
    size_t pixels, size, pos;
    char header[64];
    char *result;
    static const char hex[] = "0123456789ABCDEF";

    if (!ctx) { opSetError("op: null context"); return NULL; }
    if (!op_capture_frame(ctx)) return NULL;
    if (x2 <= x1 || y2 <= y1) { opSetError("op: invalid region"); return NULL; }
    if (x2 <= 0 || y2 <= 0 || x1 >= ctx->frame_w || y1 >= ctx->frame_h)
        return xrtStrDup("");
    sx1 = (int)(x1 < 0 ? 0 : x1);
    sy1 = (int)(y1 < 0 ? 0 : y1);
    sx2 = (int)(x2 > ctx->frame_w ? ctx->frame_w : x2);
    sy2 = (int)(y2 > ctx->frame_h ? ctx->frame_h : y2);
    w = sx2 - sx1;
    h = sy2 - sy1;
    if (w <= 0 || h <= 0) return xrtStrDup("");
    head = snprintf(header, sizeof(header), "%dx%d|", w, h);
    pixels = (size_t)w * (size_t)h;
    if (head <= 0 || pixels > (16u * 1024u * 1024u - 1u - (size_t)head) / 8u) {
        opSetError("op: capture result exceeds output limit");
        return NULL;
    }
    size = (size_t)head + pixels * 8u;
    result = (char *)xrtMalloc(size + 1u);
    if (!result) { opSetError("op: capture result allocation failed"); return NULL; }
    memcpy(result, header, (size_t)head);
    pos = (size_t)head;
    for (y = sy1; y < sy2; y++) {
        for (x = sx1; x < sx2; x++) {
            size_t index = ((size_t)y * (size_t)ctx->frame_w + (size_t)x) * 4u;
            for (k = 0; k < 4; k++) {
                BYTE value = ctx->frame_buf[index + (size_t)k];
                result[pos++] = hex[value >> 4];
                result[pos++] = hex[value & 15];
            }
        }
    }
    result[pos] = '\0';
    return result;
}

/* ------------------------------------------------------------------ */
/* ImageCompare & ImageHash                                            */
/* ------------------------------------------------------------------ */

double xop_image_compare(void *ptr, long long x1, long long y1,
                         long long x2, long long y2,
                         const char *pic_file, double sim) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0.0; }
    if (!pic_file || !*pic_file) { opSetError("op: empty pic_file"); return 0.0; }
    if (!op_capture_frame(ctx)) return 0.0;
    if (x1 < 0 || y1 < 0 || x2 <= x1 || y2 <= y1 ||
        x2 > ctx->frame_w || y2 > ctx->frame_h) return 0.0;

    op_bitmap_t tmpl;
    if (!op_load_bmp(pic_file, &tmpl)) { opSetError("op: failed to load bmp"); return 0.0; }

    int w = (int)(x2 - x1);
    int h = (int)(y2 - y1);
    if (w != tmpl.w || h != tmpl.h) { free(tmpl.data); return 0.0; }
    int total = w * h, matched = 0;
    int yy, xx;
    for (yy = 0; yy < h; yy++) {
        for (xx = 0; xx < w; xx++) {
            int si = (((int)y1 + yy) * ctx->frame_w + ((int)x1 + xx)) * 4;
            int ti = (yy * w + xx) * 4;
            if (tmpl.data[ti + 3] == 0) { matched++; continue; }
            if (abs((int)ctx->frame_buf[si+2] - (int)tmpl.data[ti+2]) <= 10 &&
                abs((int)ctx->frame_buf[si+1] - (int)tmpl.data[ti+1]) <= 10 &&
                abs((int)ctx->frame_buf[si]   - (int)tmpl.data[ti])   <= 10) {
                matched++;
            }
        }
    }
    free(tmpl.data);
    return (double)matched / (double)total;
}

void *xop_image_hash(void *ptr, long long x1, long long y1,
                     long long x2, long long y2) {
    op_context *ctx = (op_context *)ptr;
    char result[32];
    if (!ctx) { opSetError("op: null context"); return NULL; }
    if (!op_capture_frame(ctx)) return NULL;

    if (x2 <= x1 || y2 <= y1 || x1 >= ctx->frame_w ||
        y1 >= ctx->frame_h || x2 <= 0 || y2 <= 0) return xrtStrDup("");
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > ctx->frame_w) x2 = ctx->frame_w;
    if (y2 > ctx->frame_h) y2 = ctx->frame_h;
    int w = (int)(x2 - x1);
    int h = (int)(y2 - y1);

    /* simple average hash: resize to 8x8, compute mean, output 16 hex chars */
    unsigned char hash8x8[64];
    long sum = 0;
    int i, j;
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++) {
            int sx = (int)x1 + j * w / 8;
            int sy = (int)y1 + i * h / 8;
            int idx = (sy * ctx->frame_w + sx) * 4;
            int gray = (ctx->frame_buf[idx+2] * 3 + ctx->frame_buf[idx+1] * 6 + ctx->frame_buf[idx] * 1) / 10;
            hash8x8[i * 8 + j] = (unsigned char)gray;
            sum += gray;
        }
    }
    int mean = (int)(sum / 64);
    /* build 64-bit hash */
    unsigned long long hv = 0;
    for (i = 0; i < 64; i++) {
        if (hash8x8[i] > mean) hv |= (1ULL << i);
    }
    snprintf(result, sizeof(result), "%016llX", hv);
    return xrtStrDup(result);
}

/* ------------------------------------------------------------------ */
/* Mouse enhancement                                                   */
/* ------------------------------------------------------------------ */

long long xop_move_to_smooth(void *ptr, long long x, long long y,
                             long long steps, long long delay) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    int n = (steps > 0) ? (int)steps : 10;
    int ms = (delay > 0) ? (int)delay : 10;

    POINT cur;
    GetCursorPos(&cur);
    int start_x = cur.x, start_y = cur.y;
    int end_x = (int)x, end_y = (int)y;
    if (ctx->bind_hwnd) {
        POINT pt = {end_x, end_y};
        ClientToScreen(ctx->bind_hwnd, &pt);
        end_x = pt.x; end_y = pt.y;
    }

    int i;
    for (i = 1; i <= n; i++) {
        int cx = start_x + (end_x - start_x) * i / n;
        int cy = start_y + (end_y - start_y) * i / n;
        SetCursorPos(cx, cy);
        if (i < n) Sleep(ms);
    }
    return 1;
}

long long xop_drag_path(void *ptr, const char *points, long long delay) {
    op_context *ctx = (op_context *)ptr;
    const char *p, *end;
    int coords[512];
    int ncoords = 0;
    int ms, i;
    if (!ctx) { opSetError("op: null context"); return 0; }
    if (!points || !*points) { opSetError("op: empty points"); return 0; }
    if (delay < 0 || delay > INT_MAX) { opSetError("op: invalid drag delay"); return 0; }
    ms = delay == 0 ? 20 : (int)delay;

    /* 先校验全部坐标，避免畸形路径执行到一半才失败。 */
    p = points;
    for (;;) {
        end = strchr(p, ',');
        if (!end) end = p + strlen(p);
        if (ncoords == 512 || !op_parse_offset_int(p, end, &coords[ncoords])) {
            opSetError("op: invalid drag path");
            return 0;
        }
        ncoords++;
        if (*end == '\0') break;
        p = end + 1;
    }
    if (ncoords < 4 || (ncoords & 1)) {
        opSetError("op: drag path needs complete point pairs");
        return 0;
    }

    /* move to first point and press */
    op_mouse_send(ctx, MOUSEEVENTF_MOVE, coords[0], coords[1], 0);
    op_mouse_send(ctx, MOUSEEVENTF_LEFTDOWN, 0, 0, 0);
    Sleep(ms);

    for (i = 2; i < ncoords; i += 2) {
        op_mouse_send(ctx, MOUSEEVENTF_MOVE, coords[i], coords[i+1], 0);
        Sleep(ms);
    }
    op_mouse_send(ctx, MOUSEEVENTF_LEFTUP, 0, 0, 0);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Keyboard enhancement - keyCombo                                     */
/* ------------------------------------------------------------------ */

static int op_parse_vk(const char *name) {
    if (!name || !*name) return 0;
    if (_stricmp(name, "ctrl") == 0 || _stricmp(name, "control") == 0) return VK_CONTROL;
    if (_stricmp(name, "alt") == 0) return VK_MENU;
    if (_stricmp(name, "shift") == 0) return VK_SHIFT;
    if (_stricmp(name, "win") == 0 || _stricmp(name, "lwin") == 0) return VK_LWIN;
    if (_stricmp(name, "enter") == 0 || _stricmp(name, "return") == 0) return VK_RETURN;
    if (_stricmp(name, "tab") == 0) return VK_TAB;
    if (_stricmp(name, "esc") == 0 || _stricmp(name, "escape") == 0) return VK_ESCAPE;
    if (_stricmp(name, "space") == 0) return VK_SPACE;
    if (_stricmp(name, "backspace") == 0 || _stricmp(name, "back") == 0) return VK_BACK;
    if (_stricmp(name, "delete") == 0 || _stricmp(name, "del") == 0) return VK_DELETE;
    if (_stricmp(name, "insert") == 0 || _stricmp(name, "ins") == 0) return VK_INSERT;
    if (_stricmp(name, "home") == 0) return VK_HOME;
    if (_stricmp(name, "end") == 0) return VK_END;
    if (_stricmp(name, "pageup") == 0 || _stricmp(name, "pgup") == 0) return VK_PRIOR;
    if (_stricmp(name, "pagedown") == 0 || _stricmp(name, "pgdn") == 0) return VK_NEXT;
    if (_stricmp(name, "up") == 0) return VK_UP;
    if (_stricmp(name, "down") == 0) return VK_DOWN;
    if (_stricmp(name, "left") == 0) return VK_LEFT;
    if (_stricmp(name, "right") == 0) return VK_RIGHT;
    if (_stricmp(name, "printscreen") == 0 || _stricmp(name, "prtsc") == 0) return VK_SNAPSHOT;
    /* F1-F24：数字后不允许额外字符。 */
    size_t len = strlen(name);
    if ((name[0] == 'f' || name[0] == 'F') && len >= 2 && len <= 3 &&
        name[1] >= '0' && name[1] <= '9' &&
        (len == 2 || (name[2] >= '0' && name[2] <= '9'))) {
        int fn = name[1] - '0';
        if (len == 3) fn = fn * 10 + name[2] - '0';
        if (fn >= 1 && fn <= 24) return VK_F1 + fn - 1;
    }
    /* single letter A-Z */
    if (len == 1 && ((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= 'A' && name[0] <= 'Z')))
        return (name[0] >= 'a') ? name[0] - 32 : name[0];
    /* single digit 0-9 */
    if (len == 1 && name[0] >= '0' && name[0] <= '9')
        return name[0];
    return 0;
}

long long xop_key_combo(void *ptr, const char *combo) {
    op_context *ctx = (op_context *)ptr;
    const char *p, *end;
    char key[32];
    int vks[8];
    int nvk = 0;
    int bg, i;
    if (!ctx) { opSetError("op: null context"); return 0; }
    if (!combo || !*combo) { opSetError("op: empty combo"); return 0; }

    /* 每个键都必须有效；解析完之前不投递键盘消息。 */
    p = combo;
    for (;;) {
        size_t length;
        int vk;
        end = strchr(p, '+');
        if (!end) end = p + strlen(p);
        while (p < end && (*p == ' ' || *p == '\t')) p++;
        while (end > p && (end[-1] == ' ' || end[-1] == '\t')) end--;
        length = (size_t)(end - p);
        if (nvk == 8 || length == 0 || length >= sizeof(key)) {
            opSetError("op: invalid key combo");
            return 0;
        }
        memcpy(key, p, length);
        key[length] = '\0';
        vk = op_parse_vk(key);
        if (!vk) { opSetError("op: invalid key combo"); return 0; }
        vks[nvk++] = vk;
        end = strchr(p, '+');
        if (!end) break;
        p = end + 1;
    }
    bg = (strcmp(ctx->keypad_mode, "windows") == 0 && ctx->bind_hwnd);
    /* press all keys in order */
    for (i = 0; i < nvk; i++) {
        if (bg) PostMessage(ctx->bind_hwnd, WM_KEYDOWN, (WPARAM)vks[i], 0);
        else keybd_event((BYTE)vks[i], 0, 0, 0);
    }
    Sleep(30);
    /* release in reverse order */
    for (i = nvk - 1; i >= 0; i--) {
        if (bg) PostMessage(ctx->bind_hwnd, WM_KEYUP, (WPARAM)vks[i], 0);
        else keybd_event((BYTE)vks[i], 0, KEYEVENTF_KEYUP, 0);
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Memory R/W (module-level)                                           */
/* ------------------------------------------------------------------ */

static HANDLE op_open_proc(long long pid, DWORD access) {
    HANDLE h;
    if (pid <= 0 || pid > UINT32_MAX) {
        opSetError("op: invalid process id");
        return NULL;
    }
    h = OpenProcess(access, FALSE, (DWORD)pid);
    if (!h) opSetError("op: cannot open process");
    return h;
}

long long xop_read_int(long long pid, long long addr) {
    HANDLE h = op_open_proc(pid, PROCESS_VM_READ);
    if (!h) return 0;
    int val = 0;
    SIZE_T rd = 0;
    BOOL ok = ReadProcessMemory(h, (LPCVOID)(intptr_t)addr, &val, sizeof(val), &rd);
    CloseHandle(h);
    if (!ok || rd != sizeof(val)) { opSetError("op: cannot read int"); return 0; }
    return (long long)val;
}

double xop_read_float(long long pid, long long addr) {
    HANDLE h = op_open_proc(pid, PROCESS_VM_READ);
    if (!h) return 0.0;
    float val = 0.0f;
    SIZE_T rd = 0;
    BOOL ok = ReadProcessMemory(h, (LPCVOID)(intptr_t)addr, &val, sizeof(val), &rd);
    CloseHandle(h);
    if (!ok || rd != sizeof(val)) { opSetError("op: cannot read float"); return 0.0; }
    return (double)val;
}

void *xop_read_string(long long pid, long long addr, long long len) {
    HANDLE h;
    char *result;
    SIZE_T rd = 0;
    size_t size;
    BOOL ok;
    if (len <= 0 || len > 16 * 1024 * 1024) {
        opSetError("op: invalid string read length");
        return NULL;
    }
    h = op_open_proc(pid, PROCESS_VM_READ);
    if (!h) return NULL;
    size = (size_t)len;
    result = (char *)xrtMalloc(size + 1u);
    if (!result) { CloseHandle(h); opSetError("op: string allocation failed"); return NULL; }
    ok = ReadProcessMemory(h, (LPCVOID)(intptr_t)addr, result, size, &rd);
    CloseHandle(h);
    if (!ok || rd != size) {
        xrtFree(result);
        opSetError("op: cannot read complete string range");
        return NULL;
    }
    result[size] = '\0';
    size = strlen(result);
    if (!xrtUtf8Valid((xstrview){result, size}, NULL)) {
        xrtFree(result);
        opSetError("op: process string is not UTF-8");
        return NULL;
    }
    return result;
}

long long xop_write_int(long long pid, long long addr, long long value) {
    if (value < INT32_MIN || value > INT32_MAX) {
        opSetError("op: int value exceeds 32-bit range");
        return 0;
    }
    HANDLE h = op_open_proc(pid, PROCESS_VM_WRITE | PROCESS_VM_OPERATION);
    if (!h) return 0;
    int val = (int)value;
    SIZE_T wr = 0;
    BOOL ok = WriteProcessMemory(h, (LPVOID)(intptr_t)addr, &val, sizeof(val), &wr);
    CloseHandle(h);
    if (!ok || wr != sizeof(val)) { opSetError("op: cannot write int"); return 0; }
    return 1;
}

long long xop_write_float(long long pid, long long addr, double value) {
    if (value != value || value < -FLT_MAX || value > FLT_MAX ||
        (value != 0.0 && (float)value == 0.0f)) {
        opSetError("op: float value exceeds finite 32-bit range");
        return 0;
    }
    HANDLE h = op_open_proc(pid, PROCESS_VM_WRITE | PROCESS_VM_OPERATION);
    if (!h) return 0;
    float val = (float)value;
    SIZE_T wr = 0;
    BOOL ok = WriteProcessMemory(h, (LPVOID)(intptr_t)addr, &val, sizeof(val), &wr);
    CloseHandle(h);
    if (!ok || wr != sizeof(val)) { opSetError("op: cannot write float"); return 0; }
    return 1;
}

long long xop_write_string(long long pid, long long addr, const char *value) {
    HANDLE h = op_open_proc(pid, PROCESS_VM_WRITE | PROCESS_VM_OPERATION);
    if (!h) return 0;
    if (!value) value = "";
    SIZE_T wr = 0;
    BOOL ok = WriteProcessMemory(h, (LPVOID)(intptr_t)addr, value, strlen(value) + 1, &wr);
    CloseHandle(h);
    if (!ok || wr != strlen(value) + 1u) { opSetError("op: cannot write string"); return 0; }
    return 1;
}

static int op_memory_hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int op_memory_readable(const MEMORY_BASIC_INFORMATION *info) {
    DWORD access = info->Protect & 0xffu;
    if (info->State != MEM_COMMIT || (info->Protect & PAGE_GUARD)) return 0;
    return access == PAGE_READONLY || access == PAGE_READWRITE ||
           access == PAGE_WRITECOPY || access == PAGE_EXECUTE_READ ||
           access == PAGE_EXECUTE_READWRITE || access == PAGE_EXECUTE_WRITECOPY;
}

void *xop_find_memory(long long pid, long long start_addr,
                      long long end_addr, const char *pattern) {
    unsigned char pat[256], mask[256], chunk[4096 + 255];
    const char *p = pattern;
    size_t plen = 0, carry = 0;
    uintptr_t cursor, end;
    HANDLE h;
    int incomplete = 0;

    if (start_addr < 0 || end_addr <= start_addr ||
        (uint64_t)end_addr > (uint64_t)UINTPTR_MAX) {
        opSetError("op: invalid memory search range");
        return NULL;
    }
    if (!p || !*p) { opSetError("op: empty memory pattern"); return NULL; }
    /* 十六进制字节可连续书写或由空白分隔；只有 ?? 表示一个通配字节。 */
    while (*p) {
        int hi, lo;
        if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') { p++; continue; }
        if (plen == sizeof(pat)) { opSetError("op: memory pattern too long"); return NULL; }
        if (p[0] == '?' && p[1] == '?') {
            pat[plen] = 0;
            mask[plen++] = 0;
            p += 2;
            continue;
        }
        hi = op_memory_hex_digit(p[0]);
        lo = p[1] ? op_memory_hex_digit(p[1]) : -1;
        if (hi < 0 || lo < 0) { opSetError("op: invalid memory pattern"); return NULL; }
        pat[plen] = (unsigned char)((hi << 4) | lo);
        mask[plen++] = 1;
        p += 2;
    }
    if (!plen) { opSetError("op: empty memory pattern"); return NULL; }
    h = op_open_proc(pid, PROCESS_VM_READ | PROCESS_QUERY_INFORMATION);
    if (!h) return NULL;

    cursor = (uintptr_t)start_addr;
    end = (uintptr_t)end_addr;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION info;
        uintptr_t base, limit;
        if (!VirtualQueryEx(h, (LPCVOID)cursor, &info, sizeof(info))) {
            incomplete = 1;
            break;
        }
        base = (uintptr_t)info.BaseAddress;
        limit = info.RegionSize > UINTPTR_MAX - base ? UINTPTR_MAX :
                base + (uintptr_t)info.RegionSize;
        if (limit <= cursor) { incomplete = 1; break; }
        if (limit > end) limit = end;
        if (!op_memory_readable(&info)) {
            carry = 0;
            cursor = limit;
            continue;
        }
        while (cursor < limit) {
            size_t want = (size_t)(limit - cursor);
            SIZE_T read = 0;
            size_t total, i;
            if (want > 4096) want = 4096;
            if (!ReadProcessMemory(h, (LPCVOID)cursor, chunk + carry, want, &read) ||
                read != want) {
                incomplete = 1;
                carry = 0;
                cursor += want;
                continue;
            }
            total = carry + want;
            for (i = 0; i + plen <= total; i++) {
                size_t j;
                for (j = 0; j < plen; j++) {
                    if (mask[j] && chunk[i + j] != pat[j]) break;
                }
                if (j == plen) {
                    char result[32];
                    snprintf(result, sizeof(result), "%lld",
                             (long long)(cursor - carry + i));
                    CloseHandle(h);
                    return xrtStrDup(result);
                }
            }
            carry = total < plen - 1u ? total : plen - 1u;
            memmove(chunk, chunk + total - carry, carry);
            cursor += want;
        }
    }
    CloseHandle(h);
    if (incomplete) { opSetError("op: memory scan incomplete"); return NULL; }
    return xrtStrDup("");
}

/* ------------------------------------------------------------------ */
/* Process enumeration (module-level)                                  */
/* ------------------------------------------------------------------ */

void *xop_enum_process(const char *filter) {
    uint16 *wide_filter = (filter && *filter) ? xrtUtf8To16(filter, NULL) : NULL;
    int failed = 0;
    op_process_entry32w pe;
    HANDLE snap;
    xbuffer result;

    if (filter && *filter && !wide_filter) return NULL;
    op_init_toolhelp();
    if (!p_CreateToolhelp32Snapshot || !p_Process32First || !p_Process32Next) {
        xrtFree(wide_filter);
        opSetError("op: toolhelp not available"); return NULL;
    }
    snap = p_CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        xrtFree(wide_filter);
        opSetError("op: snapshot failed"); return NULL;
    }
    if (!xrtBufferInit(&result)) { CloseHandle(snap); xrtFree(wide_filter); return NULL; }

    memset(&pe, 0, sizeof(pe));
    pe.dwSize = sizeof(pe);
    if (p_Process32First(snap, &pe)) {
        do {
            char *name;
            char entry[1024];
            int written;

            if (wide_filter && !wcsstr(pe.szExeFile, (const WCHAR *)wide_filter)) continue;
            name = xrtUtf16To8((const uint16 *)pe.szExeFile, NULL);
            if (!name) { failed = 1; break; }
            written = snprintf(entry, sizeof(entry), "%s%lu:%s",
                               result.Size ? "|" : "", pe.th32ProcessID, name);
            xrtFree(name);
            if (written < 0 || written >= (int)sizeof(entry) ||
                (size_t)written > 16u * 1024u * 1024u - result.Size) {
                opSetError("op: process list exceeds output limit");
                failed = 1;
                break;
            }
            if (!xrtBufferAppend(&result, (xbytesview){(const uint8 *)entry, (size_t)written})) {
                failed = 1;
                break;
            }
        } while (p_Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    xrtFree(wide_filter);
    if (!failed && !xrtBufferAppendByte(&result, 0)) failed = 1;
    if (failed) { xrtBufferUnit(&result); return NULL; }
    return xrtBufferTake(&result, NULL, NULL);
}

void *xop_get_process_modules(long long pid) {
    op_module_entry32w me;
    HANDLE snap;
    int failed = 0;
    xbuffer result;

    if (pid < 0 || pid > UINT32_MAX) { opSetError("op: invalid process id"); return NULL; }
    op_init_toolhelp();
    if (!p_CreateToolhelp32Snapshot || !p_Module32First || !p_Module32Next) {
        opSetError("op: toolhelp not available"); return NULL;
    }
    snap = p_CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, (DWORD)pid);
    if (snap == INVALID_HANDLE_VALUE) { opSetError("op: module snapshot failed"); return NULL; }
    if (!xrtBufferInit(&result)) { CloseHandle(snap); return NULL; }

    memset(&me, 0, sizeof(me));
    me.dwSize = sizeof(me);
    if (p_Module32First(snap, &me)) {
        do {
            char *name = xrtUtf16To8((const uint16 *)me.szModule, NULL);
            char entry[1024];
            int written;

            if (!name) { failed = 1; break; }
            written = snprintf(entry, sizeof(entry), "%s%08llX:%s",
                               result.Size ? "|" : "", (long long)(intptr_t)me.modBaseAddr, name);
            xrtFree(name);
            if (written < 0 || written >= (int)sizeof(entry) ||
                (size_t)written > 16u * 1024u * 1024u - result.Size) {
                opSetError("op: module list exceeds output limit");
                failed = 1;
                break;
            }
            if (!xrtBufferAppend(&result, (xbytesview){(const uint8 *)entry, (size_t)written})) {
                failed = 1;
                break;
            }
        } while (p_Module32Next(snap, &me));
    }
    CloseHandle(snap);
    if (!failed && !xrtBufferAppendByte(&result, 0)) failed = 1;
    if (failed) { xrtBufferUnit(&result); return NULL; }
    return xrtBufferTake(&result, NULL, NULL);
}

/* ------------------------------------------------------------------ */
/* System tools (module-level)                                         */
/* ------------------------------------------------------------------ */

void xop_delay(long long ms) {
    xrtClearError();
    if (ms < 0) {
        xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.op", 0,
            "delay requires non-negative milliseconds");
        return;
    }
    if ((uint64)ms > UINT64_MAX / 1000u) {
        xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.op", 0,
            "delay exceeds the supported duration");
        return;
    }
    xrtSleepUs((uint64)ms * 1000u);
}

long long xop_random_int(long long min, long long max) {
    xrtClearError();
    if (min > max) {
        xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.op", 0,
            "randomInt requires min <= max");
        return 0;
    }
    return xrtFastRandRangeClosed(min, max);
}

long long xop_set_clipboard(const char *text) {
    uint16 *wide;
    size_t units;
    size_t bytes;
    HGLOBAL block;
    void *dst;
    int ok = 0;

    wide = xrtUtf8To16(text ? text : "", &units);
    if (!wide || units >= SIZE_MAX / sizeof(WCHAR)) {
        xrtFree(wide);
        opSetError("op: invalid or oversized UTF-8 clipboard text");
        return 0;
    }
    bytes = (units + 1) * sizeof(WCHAR);
    block = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!block) {
        xrtFree(wide);
        opSetError("op: cannot allocate clipboard text");
        return 0;
    }
    dst = GlobalLock(block);
    if (!dst) {
        xrtFree(wide);
        GlobalFree(block);
        opSetError("op: cannot lock clipboard text");
        return 0;
    }
    memcpy(dst, wide, bytes);
    xrtFree(wide);
    GlobalUnlock(block);
    if (!OpenClipboard(NULL)) {
        GlobalFree(block);
        opSetError("op: cannot open clipboard");
        return 0;
    }
    if (EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, block)) {
        ok = 1;
    } else {
        GlobalFree(block);
        opSetError("op: cannot set Unicode clipboard text");
    }
    CloseClipboard();
    return ok;
}

void *xop_get_clipboard(void) {
    HGLOBAL block;
    const uint16 *wide;
    size_t units;
    size_t limit;
    size_t bytes;
    char *utf8;

    if (!OpenClipboard(NULL)) { opSetError("op: cannot open clipboard"); return NULL; }
    block = GetClipboardData(CF_UNICODETEXT);
    if (!block) { CloseClipboard(); return xrtStrDup(""); }
    wide = (const uint16 *)GlobalLock(block);
    if (!wide) {
        CloseClipboard();
        opSetError("op: cannot lock clipboard text");
        return NULL;
    }
    limit = GlobalSize(block) / sizeof(uint16);
    if (limit > 16u * 1024u * 1024u) limit = 16u * 1024u * 1024u;
    for (units = 0; units < limit && wide[units]; ++units) {}
    if (units == limit) {
        utf8 = NULL;
        opSetError("op: clipboard text is unterminated or exceeds read limit");
    } else {
        utf8 = xrtUtf16To8(wide, &bytes);
        if (!utf8) {
            opSetError("op: invalid Unicode clipboard text");
        } else if (bytes > 16u * 1024u * 1024u) {
            xrtFree(utf8);
            utf8 = NULL;
            opSetError("op: clipboard text exceeds read limit");
        }
    }
    GlobalUnlock(block);
    CloseClipboard();
    return utf8;
}
