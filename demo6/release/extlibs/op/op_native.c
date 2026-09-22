/*
 * op_native.c - OP plugin: window binding, screen capture, color/image finding,
 *               mouse/keyboard simulation, DXGI/D3D9/D3D11/WGL game capture.
 * Pure Win32 API + COM vtable calls. TCC-compatible.
 */
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "xrt.h"
#include "op_dxgi_defs.h"

/* Manual Toolhelp32 declarations (TCC has no tlhelp32.h) */
#define TH32CS_SNAPPROCESS  0x00000002
#define TH32CS_SNAPMODULE   0x00000008
#define TH32CS_SNAPMODULE32 0x00000010
#define MAX_MODULE_NAME32   255

typedef struct tagPROCESSENTRY32 {
    DWORD dwSize;
    DWORD cntUsage;
    DWORD th32ProcessID;
    ULONG_PTR th32DefaultHeapID;
    DWORD th32ModuleID;
    DWORD cntThreads;
    DWORD th32ParentProcessID;
    LONG  pcPriClassBase;
    DWORD dwFlags;
    char  szExeFile[260];
} PROCESSENTRY32;

/*
 * Toolhelp 的 64 位结构必须与 Win32 ABI 完全一致。
 * 缺少 dwSize 时 Process32First/Module32First 可能直接写越界，
 * 因此不自行猜测窄化结构，也不把系统 PID 当作可靠测试对象。
 */
typedef struct tagMODULEENTRY32 {
    DWORD   dwSize;
    DWORD   th32ModuleID;
    DWORD   th32ProcessID;
    DWORD   GlblcntUsage;
    DWORD   ProccntUsage;
    BYTE   *modBaseAddr;
    DWORD   modBaseSize;
    HMODULE hModule;
    char    szModule[MAX_MODULE_NAME32 + 1];
    char    szExePath[260];
} MODULEENTRY32;

typedef HANDLE (WINAPI *PFN_CreateToolhelp32Snapshot)(DWORD, DWORD);
typedef BOOL   (WINAPI *PFN_Process32First)(HANDLE, PROCESSENTRY32*);
typedef BOOL   (WINAPI *PFN_Process32Next)(HANDLE, PROCESSENTRY32*);
typedef BOOL   (WINAPI *PFN_Module32First)(HANDLE, MODULEENTRY32*);
typedef BOOL   (WINAPI *PFN_Module32Next)(HANDLE, MODULEENTRY32*);

static PFN_CreateToolhelp32Snapshot p_CreateToolhelp32Snapshot = NULL;
static PFN_Process32First  p_Process32First  = NULL;
static PFN_Process32Next   p_Process32Next   = NULL;
static PFN_Module32First   p_Module32First   = NULL;
static PFN_Module32Next    p_Module32Next    = NULL;
static int op_toolhelp_inited = 0;

static void op_init_toolhelp(void) {
    if (op_toolhelp_inited) return;
    op_toolhelp_inited = 1;
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    if (!k32) k32 = LoadLibraryA("kernel32.dll");
    if (k32) {
        p_CreateToolhelp32Snapshot = (PFN_CreateToolhelp32Snapshot)GetProcAddress(k32, "CreateToolhelp32Snapshot");
        p_Process32First = (PFN_Process32First)GetProcAddress(k32, "Process32First");
        p_Process32Next  = (PFN_Process32Next)GetProcAddress(k32, "Process32Next");
        p_Module32First  = (PFN_Module32First)GetProcAddress(k32, "Module32First");
        p_Module32Next   = (PFN_Module32Next)GetProcAddress(k32, "Module32Next");
    }
}

/* Minimal OpenGL declarations (TCC has no GL/gl.h) */
#define GL_FRONT        0x0404
#define GL_BGRA_EXT     0x80E1
#define GL_UNSIGNED_BYTE 0x1401
typedef void (*PFN_glReadBuffer)(unsigned int);
typedef void (*PFN_glReadPixels)(int, int, int, int, unsigned int, unsigned int, void*);
static PFN_glReadBuffer  p_glReadBuffer  = NULL;
static PFN_glReadPixels  p_glReadPixels  = NULL;

static void op_init_gl(void) {
    if (p_glReadBuffer) return;
    HMODULE gl = GetModuleHandleA("opengl32.dll");
    if (!gl) gl = LoadLibraryA("opengl32.dll");
    if (gl) {
        p_glReadBuffer = (PFN_glReadBuffer)GetProcAddress(gl, "glReadBuffer");
        p_glReadPixels = (PFN_glReadPixels)GetProcAddress(gl, "glReadPixels");
    }
}

/* Dynamically loaded user32 functions (may be missing from TCC import lib) */
typedef BOOL (WINAPI *PFN_PrintWindow)(HWND, HDC, UINT);
typedef UINT (WINAPI *PFN_SendInput)(UINT, LPVOID, int);
static PFN_PrintWindow p_PrintWindow = NULL;
static PFN_SendInput   p_SendInput   = NULL;
static int op_user32_inited = 0;

static void op_init_user32(void) {
    if (op_user32_inited) return;
    op_user32_inited = 1;
    HMODULE u32 = GetModuleHandleA("user32.dll");
    if (!u32) u32 = LoadLibraryA("user32.dll");
    if (u32) {
        p_PrintWindow = (PFN_PrintWindow)GetProcAddress(u32, "PrintWindow");
        p_SendInput = (PFN_SendInput)GetProcAddress(u32, "SendInput");
    }
}
static void opSetError(const char* sMessage)
{
    xrtSetErrorInfo(XERR_STATE, "xlang.extlib.op", 0,
        sMessage != NULL ? sMessage : "op error");
}
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
} op_context;

/* ------------------------------------------------------------------ */
/* DXGI Desktop Duplication state                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    ID3D11Device           *device;
    ID3D11DeviceContext    *device_ctx;
    IDXGIOutputDuplication *dup;
    ID3D11Texture2D        *staging;
    int                     staging_w, staging_h;
    int                     inited;
} op_dxgi_state;

static op_dxgi_state g_dxgi = {0};

static int op_dxgi_init(void) {
    if (g_dxgi.inited) return g_dxgi.dup ? 1 : 0;
    g_dxgi.inited = 1;

    HMODULE d3d11 = LoadLibraryA("d3d11.dll");
    HMODULE dxgi  = LoadLibraryA("dxgi.dll");
    if (!d3d11 || !dxgi) return 0;

    PFN_D3D11CreateDevice pD3D11CreateDevice =
        (PFN_D3D11CreateDevice)GetProcAddress(d3d11, "D3D11CreateDevice");
    PFN_CreateDXGIFactory1 pCreateDXGIFactory1 =
        (PFN_CreateDXGIFactory1)GetProcAddress(dxgi, "CreateDXGIFactory1");
    if (!pD3D11CreateDevice || !pCreateDXGIFactory1) return 0;

    /* Create D3D11 device */
    unsigned int feature_level = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = pD3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
                                    &feature_level, 1, D3D11_SDK_VERSION,
                                    &g_dxgi.device, NULL, &g_dxgi.device_ctx);
    if (FAILED(hr) || !g_dxgi.device) return 0;

    /* Get DXGI adapter from device */
    IDXGIAdapter *adapter = NULL;
    hr = g_dxgi.device->lpVtbl->QueryInterface((IUnknown*)g_dxgi.device,
                                               &IID_IDXGIDevice, (void**)&adapter);
    /* IDXGIDevice::GetAdapter is at vtable slot 7 */
    if (FAILED(hr) || !adapter) return 0;

    /* EnumOutputs(0) to get primary output */
    IDXGIOutput1 *output1 = NULL;
    hr = adapter->lpVtbl->EnumOutputs(adapter, 0, &output1);
    OP_RELEASE(adapter);
    if (FAILED(hr) || !output1) return 0;

    /* DuplicateOutput */
    hr = output1->lpVtbl->DuplicateOutput(output1,
             (IUnknown*)g_dxgi.device, &g_dxgi.dup);
    OP_RELEASE(output1);
    if (FAILED(hr) || !g_dxgi.dup) return 0;

    return 1;
}

static int op_dxgi_capture(op_context *ctx) {
    if (!op_dxgi_init()) {
        opSetError("op: DXGI Desktop Duplication init failed");
        return 0;
    }

    IDXGIResource *desktop_res = NULL;
    DXGI_OUTDUPL_FRAME_INFO fi;
    HRESULT hr = g_dxgi.dup->lpVtbl->AcquireNextFrame(g_dxgi.dup, 100, &fi, &desktop_res);
    if (FAILED(hr) || !desktop_res) {
        /* timeout or no new frame - try to use last frame */
        if (ctx->frame_buf && ctx->frame_w > 0) return 1;
        opSetError("op: DXGI AcquireNextFrame failed");
        return 0;
    }

    /* Get ID3D11Texture2D from resource */
    ID3D11Texture2D *desktop_tex = NULL;
    hr = ((IUnknown*)desktop_res)->lpVtbl->QueryInterface(
             (IUnknown*)desktop_res, &IID_ID3D11Texture2D, (void**)&desktop_tex);
    OP_RELEASE(desktop_res);
    if (FAILED(hr) || !desktop_tex) {
        g_dxgi.dup->lpVtbl->ReleaseFrame(g_dxgi.dup);
        return 0;
    }

    /* Get texture dimensions */
    D3D11_TEXTURE2D_DESC desc;
    desktop_tex->lpVtbl->GetDesc(desktop_tex, &desc);
    int w = (int)desc.Width, h = (int)desc.Height;

    /* Ensure frame buffer */
    if (ctx->frame_w != w || ctx->frame_h != h || !ctx->frame_buf) {
        if (ctx->frame_buf) free(ctx->frame_buf);
        ctx->frame_buf = (BYTE *)malloc(w * h * 4);
        ctx->frame_w = w;
        ctx->frame_h = h;
    }

    /* Create/recreate staging texture if needed */
    if (!g_dxgi.staging || g_dxgi.staging_w != w || g_dxgi.staging_h != h) {
        OP_RELEASE(g_dxgi.staging);
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
        g_dxgi.device->lpVtbl->CreateTexture2D(g_dxgi.device, &sd, NULL, &g_dxgi.staging);
        g_dxgi.staging_w = w;
        g_dxgi.staging_h = h;
    }

    if (g_dxgi.staging) {
        /* Copy desktop texture to staging */
        g_dxgi.device_ctx->lpVtbl->CopyResource(
            g_dxgi.device_ctx, g_dxgi.staging, desktop_tex);

        /* Map and read pixels */
        D3D11_MAPPED_SUBRESOURCE mapped;
        hr = g_dxgi.device_ctx->lpVtbl->Map(
            g_dxgi.device_ctx, g_dxgi.staging, 0, 1/*READ*/, 0, &mapped);
        if (SUCCEEDED(hr)) {
            int row;
            for (row = 0; row < h; row++) {
                memcpy(ctx->frame_buf + row * w * 4,
                       (BYTE*)mapped.pData + row * mapped.RowPitch,
                       w * 4);
            }
            g_dxgi.device_ctx->lpVtbl->Unmap(g_dxgi.device_ctx, g_dxgi.staging, 0);
        }
    }

    OP_RELEASE(desktop_tex);
    g_dxgi.dup->lpVtbl->ReleaseFrame(g_dxgi.dup);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Hook shared memory (for d3d9/d3d11/wgl injected modes)              */
/* ------------------------------------------------------------------ */

#define OP_HOOK_MAGIC 0x4F50484B  /* "OPHK" */

typedef struct {
    unsigned int magic;
    unsigned int width;
    unsigned int height;
    unsigned int frame_counter;
    unsigned int format; /* 0 = BGRA */
    /* pixel data follows */
} op_hook_header;

typedef struct {
    HANDLE  map_handle;
    BYTE   *map_ptr;
    DWORD   pid;
    char    map_name[64];
    char    dll_path[512];
    HANDLE  remote_thread;
    int     injected;
} op_hook_state;

static op_hook_state g_hook = {0};

static int op_hook_inject(op_context *ctx, const char *mode) {
    if (g_hook.injected) return 1;

    /* Determine DLL path based on mode */
    char exe_dir[512];
    GetModuleFileNameA(NULL, exe_dir, sizeof(exe_dir));
    char *slash = strrchr(exe_dir, '\\');
    if (slash) *slash = '\0';

    const char *dll_name = "op_d3d9_hook.dll";
    if (strcmp(mode, "d3d11") == 0) dll_name = "op_d3d11_hook.dll";
    else if (strcmp(mode, "wgl") == 0) dll_name = "op_wgl_hook.dll";

    snprintf(g_hook.dll_path, sizeof(g_hook.dll_path),
             "%s\\extlibs\\op\\hooks\\%s", exe_dir, dll_name);

    /* Check DLL exists */
    if (GetFileAttributesA(g_hook.dll_path) == INVALID_FILE_ATTRIBUTES) {
        opSetError("op: hook DLL not found");
        return 0;
    }

    /* Get target PID from hwnd */
    DWORD pid = 0;
    GetWindowThreadProcessId(ctx->bind_hwnd, &pid);
    if (!pid) { opSetError("op: cannot get target PID"); return 0; }
    g_hook.pid = pid;

    /* Create shared memory */
    snprintf(g_hook.map_name, sizeof(g_hook.map_name), "op_hook_%lu", pid);
    /* 4MB shared memory (enough for 1024x1024 BGRA) */
    DWORD map_size = 4 * 1024 * 1024;
    g_hook.map_handle = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL,
                                           PAGE_READWRITE, 0, map_size, g_hook.map_name);
    if (!g_hook.map_handle) { opSetError("op: CreateFileMapping failed"); return 0; }
    g_hook.map_ptr = (BYTE*)MapViewOfFile(g_hook.map_handle, FILE_MAP_ALL_ACCESS, 0, 0, map_size);
    if (!g_hook.map_ptr) { opSetError("op: MapViewOfFile failed"); return 0; }
    memset(g_hook.map_ptr, 0, map_size);

    /* Inject DLL via CreateRemoteThread + LoadLibraryA */
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) { opSetError("op: OpenProcess failed"); return 0; }

    void *remote_mem = VirtualAllocEx(hProc, NULL, strlen(g_hook.dll_path) + 1,
                                      MEM_COMMIT, PAGE_READWRITE);
    if (!remote_mem) { CloseHandle(hProc); opSetError("op: VirtualAllocEx failed"); return 0; }

    WriteProcessMemory(hProc, remote_mem, g_hook.dll_path, strlen(g_hook.dll_path) + 1, NULL);

    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    void *pLoadLib = (void*)GetProcAddress(k32, "LoadLibraryA");
    g_hook.remote_thread = CreateRemoteThread(hProc, NULL, 0,
                                              (LPTHREAD_START_ROUTINE)pLoadLib,
                                              remote_mem, 0, NULL);
    CloseHandle(hProc);

    if (!g_hook.remote_thread) { opSetError("op: CreateRemoteThread failed"); return 0; }

    /* Wait for injection to complete */
    WaitForSingleObject(g_hook.remote_thread, 5000);
    g_hook.injected = 1;

    /* Wait a bit for the hook to initialize and capture first frame */
    Sleep(200);
    return 1;
}

static int op_hook_capture(op_context *ctx) {
    if (!g_hook.injected || !g_hook.map_ptr) {
        opSetError("op: hook not injected");
        return 0;
    }

    op_hook_header *hdr = (op_hook_header*)g_hook.map_ptr;
    if (hdr->magic != OP_HOOK_MAGIC) {
        /* hook not ready yet */
        if (ctx->frame_buf && ctx->frame_w > 0) return 1; /* use last frame */
        opSetError("op: hook not ready");
        return 0;
    }

    int w = (int)hdr->width;
    int h = (int)hdr->height;
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return 0;

    /* Ensure frame buffer */
    if (ctx->frame_w != w || ctx->frame_h != h || !ctx->frame_buf) {
        if (ctx->frame_buf) free(ctx->frame_buf);
        ctx->frame_buf = (BYTE *)malloc(w * h * 4);
        ctx->frame_w = w;
        ctx->frame_h = h;
    }

    /* Copy pixel data from shared memory */
    BYTE *src = g_hook.map_ptr + sizeof(op_hook_header);
    memcpy(ctx->frame_buf, src, w * h * 4);
    return 1;
}

static void op_hook_cleanup(void) {
    if (g_hook.map_ptr) { UnmapViewOfFile(g_hook.map_ptr); g_hook.map_ptr = NULL; }
    if (g_hook.map_handle) { CloseHandle(g_hook.map_handle); g_hook.map_handle = NULL; }
    if (g_hook.remote_thread) { CloseHandle(g_hook.remote_thread); g_hook.remote_thread = NULL; }
    g_hook.injected = 0;
}

/* ------------------------------------------------------------------ */
/* Context (definition moved above for DXGI/hook)                      */
/* ------------------------------------------------------------------ */

static char op_buf[65536];

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
    op_hook_cleanup();
    free(ctx);
}

/* ------------------------------------------------------------------ */
/* Window binding                                                      */
/* ------------------------------------------------------------------ */

long long xop_bind_window(void *ptr, long long hwnd, const char *display,
                          const char *mouse, const char *keypad, long long mode) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    if (!IsWindow((HWND)(intptr_t)hwnd)) { opSetError("op: invalid hwnd"); return 0; }

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
    op_hook_cleanup();
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

static int op_ensure_frame(op_context *ctx, int w, int h) {
    if (ctx->frame_w == w && ctx->frame_h == h && ctx->mem_dc) return 1;

    /* release old */
    if (ctx->mem_dc) {
        if (ctx->old_bmp) SelectObject(ctx->mem_dc, ctx->old_bmp);
        DeleteDC(ctx->mem_dc);
        ctx->mem_dc = NULL;
        ctx->old_bmp = NULL;
    }
    if (ctx->mem_bmp) { DeleteObject(ctx->mem_bmp); ctx->mem_bmp = NULL; }
    if (ctx->frame_buf) { free(ctx->frame_buf); ctx->frame_buf = NULL; }

    HDC screen_dc = GetDC(NULL);
    ctx->mem_dc = CreateCompatibleDC(screen_dc);
    ctx->mem_bmp = CreateCompatibleBitmap(screen_dc, w, h);
    ctx->old_bmp = (HBITMAP)SelectObject(ctx->mem_dc, ctx->mem_bmp);
    ReleaseDC(NULL, screen_dc);

    ctx->frame_buf = (BYTE *)malloc(w * h * 4);
    ctx->frame_w = w;
    ctx->frame_h = h;
    return (ctx->mem_dc && ctx->frame_buf) ? 1 : 0;
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
        if (!g_hook.injected) {
            if (!op_hook_inject(ctx, ctx->display_mode)) return 0;
        }
        return op_hook_capture(ctx);
    }

    RECT rc;
    GetClientRect(ctx->bind_hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) { opSetError("op: window has zero size"); return 0; }

    if (!op_ensure_frame(ctx, w, h)) { opSetError("op: frame alloc failed"); return 0; }

    if (strcmp(ctx->display_mode, "gdi") == 0) {
        /* PrintWindow - works for occluded/minimized windows */
        op_init_user32();
        if (p_PrintWindow) {
            p_PrintWindow(ctx->bind_hwnd, ctx->mem_dc, 2);
        } else {
            HDC src_dc2 = GetDC(ctx->bind_hwnd);
            BitBlt(ctx->mem_dc, 0, 0, w, h, src_dc2, 0, 0, SRCCOPY);
            ReleaseDC(ctx->bind_hwnd, src_dc2);
        }
    } else if (strcmp(ctx->display_mode, "opengl") == 0) {
        /* glReadPixels - only works in-process with current GL context */
        op_init_gl();
        if (p_glReadBuffer && p_glReadPixels) {
            p_glReadBuffer(GL_FRONT);
            p_glReadPixels(0, 0, w, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, ctx->frame_buf);
            return 1;
        }
        /* fallback to BitBlt if GL not available */
        HDC src_dc = GetDC(ctx->bind_hwnd);
        BitBlt(ctx->mem_dc, 0, 0, w, h, src_dc, 0, 0, SRCCOPY);
        ReleaseDC(ctx->bind_hwnd, src_dc);
    } else {
        /* normal: GetDC + BitBlt */
        HDC src_dc = GetDC(ctx->bind_hwnd);
        BitBlt(ctx->mem_dc, 0, 0, w, h, src_dc, 0, 0, SRCCOPY);
        ReleaseDC(ctx->bind_hwnd, src_dc);
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
    GetDIBits(ctx->mem_dc, ctx->mem_bmp, 0, h, ctx->frame_buf,
              (BITMAPINFO *)&bi, DIB_RGB_COLORS);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Image & Color                                                       */
/* ------------------------------------------------------------------ */

const char *xop_get_color(void *ptr, long long x, long long y) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return ""; }
    if (!op_capture_frame(ctx)) return "";
    if (x < 0 || x >= ctx->frame_w || y < 0 || y >= ctx->frame_h) {
        opSetError("op: coordinates out of range");
        return "";
    }
    int idx = ((int)y * ctx->frame_w + (int)x) * 4;
    BYTE b = ctx->frame_buf[idx];
    BYTE g = ctx->frame_buf[idx + 1];
    BYTE r = ctx->frame_buf[idx + 2];
    snprintf(op_buf, sizeof(op_buf), "%02X%02X%02X", r, g, b);
    return op_buf;
}

const char *xop_get_colors(void *ptr, long long x1, long long y1,
                           long long x2, long long y2) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return ""; }
    if (!op_capture_frame(ctx)) return "";

    int pos = 0;
    int yy, xx;
    for (yy = (int)y1; yy <= (int)y2 && pos < (int)sizeof(op_buf) - 16; yy++) {
        for (xx = (int)x1; xx <= (int)x2 && pos < (int)sizeof(op_buf) - 16; xx++) {
            if (xx < 0 || xx >= ctx->frame_w || yy < 0 || yy >= ctx->frame_h) continue;
            int idx = (yy * ctx->frame_w + xx) * 4;
            BYTE b = ctx->frame_buf[idx];
            BYTE g = ctx->frame_buf[idx + 1];
            BYTE r = ctx->frame_buf[idx + 2];
            pos += snprintf(op_buf + pos, sizeof(op_buf) - pos, "%02X%02X%02X|", r, g, b);
        }
    }
    if (pos > 0 && op_buf[pos - 1] == '|') op_buf[pos - 1] = '\0';
    return op_buf;
}

static int op_save_bmp(const char *path, BYTE *buf, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;

    int row_size = w * 4;
    int data_size = row_size * h;
    int file_size = 54 + data_size;

    BITMAPFILEHEADER fh;
    memset(&fh, 0, sizeof(fh));
    fh.bfType = 0x4D42; /* 'BM' */
    fh.bfSize = file_size;
    fh.bfOffBits = 54;

    BITMAPINFOHEADER ih;
    memset(&ih, 0, sizeof(ih));
    ih.biSize = 40;
    ih.biWidth = w;
    ih.biHeight = -h; /* top-down */
    ih.biPlanes = 1;
    ih.biBitCount = 32;
    ih.biCompression = BI_RGB;
    ih.biSizeImage = data_size;

    fwrite(&fh, 1, sizeof(fh), f);
    fwrite(&ih, 1, sizeof(ih), f);
    fwrite(buf, 1, data_size, f);
    fclose(f);
    return 1;
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

const char *xop_get_frame_info(void *ptr) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return "0x0"; }
    if (!op_capture_frame(ctx)) return "0x0";
    snprintf(op_buf, sizeof(op_buf), "%dx%d", ctx->frame_w, ctx->frame_h);
    return op_buf;
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

static int op_hex2(const char *s) {
    int v = 0, i;
    for (i = 0; i < 2 && s[i]; i++) {
        char c = s[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v += c - '0';
        else if (c >= 'a' && c <= 'f') v += c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v += c - 'A' + 10;
    }
    return v;
}

/* Parse "RRGGBB" or "RRGGBB-DRDGDB", optionally prefixed with @ */
static int op_parse_one_color(const char *s, op_color_t *out) {
    memset(out, 0, sizeof(*out));
    if (*s == '@') { out->negate = 1; s++; }
    if (strlen(s) < 6) return 0;
    out->r = op_hex2(s);
    out->g = op_hex2(s + 2);
    out->b = op_hex2(s + 4);
    const char *dash = strchr(s, '-');
    if (dash && strlen(dash) >= 7) {
        dash++;
        out->dr = op_hex2(dash);
        out->dg = op_hex2(dash + 2);
        out->db = op_hex2(dash + 4);
    }
    return 1;
}

/* Parse multi-color "RRGGBB-DRDGDB|RRGGBB|..." */
static int op_parse_colors(const char *color_str, op_color_t *colors, int max) {
    char tmp[1024];
    strncpy(tmp, color_str, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    int count = 0;
    char *p = tmp;
    while (p && *p && count < max) {
        char *sep = strchr(p, '|');
        if (sep) *sep = '\0';
        if (op_parse_one_color(p, &colors[count])) count++;
        p = sep ? sep + 1 : NULL;
    }
    return count;
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

const char *xop_find_color(void *ptr, long long x1, long long y1,
                           long long x2, long long y2,
                           const char *color, double sim, long long dir) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return ""; }
    if (!color || !*color) { opSetError("op: empty color"); return ""; }
    if (!op_capture_frame(ctx)) return "";

    op_color_t colors[OP_MAX_COLORS];
    int ncolors = op_parse_colors(color, colors, OP_MAX_COLORS);
    if (ncolors == 0) { opSetError("op: invalid color format"); return ""; }

    int sx1 = (int)x1, sy1 = (int)y1, sx2 = (int)x2, sy2 = (int)y2;
    if (sx1 < 0) sx1 = 0;
    if (sy1 < 0) sy1 = 0;
    if (sx2 >= ctx->frame_w) sx2 = ctx->frame_w - 1;
    if (sy2 >= ctx->frame_h) sy2 = ctx->frame_h - 1;

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
                snprintf(op_buf, sizeof(op_buf), "%d,%d", xx, yy);
                return op_buf;
            }
            if (xx == x_end) break;
        }
        if (yy == y_end) break;
    }
    return "";
}

const char *xop_find_color_ex(void *ptr, long long x1, long long y1,
                              long long x2, long long y2,
                              const char *color, double sim, long long dir) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return ""; }
    if (!color || !*color) { opSetError("op: empty color"); return ""; }
    if (!op_capture_frame(ctx)) return "";

    op_color_t colors[OP_MAX_COLORS];
    int ncolors = op_parse_colors(color, colors, OP_MAX_COLORS);
    if (ncolors == 0) { opSetError("op: invalid color format"); return ""; }

    int sx1 = (int)x1, sy1 = (int)y1, sx2 = (int)x2, sy2 = (int)y2;
    if (sx1 < 0) sx1 = 0;
    if (sy1 < 0) sy1 = 0;
    if (sx2 >= ctx->frame_w) sx2 = ctx->frame_w - 1;
    if (sy2 >= ctx->frame_h) sy2 = ctx->frame_h - 1;

    int x_start, x_end, x_step, y_start, y_end, y_step;
    if (dir == 2 || dir == 3) { x_start = sx2; x_end = sx1; x_step = -1; }
    else { x_start = sx1; x_end = sx2; x_step = 1; }
    if (dir == 1 || dir == 3) { y_start = sy2; y_end = sy1; y_step = -1; }
    else { y_start = sy1; y_end = sy2; y_step = 1; }

    int pos = 0;
    int yy, xx;
    for (yy = y_start; ; yy += y_step) {
        for (xx = x_start; ; xx += x_step) {
            int idx = (yy * ctx->frame_w + xx) * 4;
            BYTE b = ctx->frame_buf[idx];
            BYTE g = ctx->frame_buf[idx + 1];
            BYTE r = ctx->frame_buf[idx + 2];
            if (op_match_any(r, g, b, colors, ncolors, sim)) {
                if (pos > 0 && pos < (int)sizeof(op_buf) - 1) op_buf[pos++] = '|';
                pos += snprintf(op_buf + pos, sizeof(op_buf) - pos, "%d,%d", xx, yy);
                if (pos >= (int)sizeof(op_buf) - 16) goto done_ex;
            }
            if (xx == x_end) break;
        }
        if (yy == y_end) break;
    }
done_ex:
    op_buf[pos] = '\0';
    return op_buf;
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

static int op_parse_offsets(const char *s, op_offset_point_t *pts, int max) {
    char tmp[4096];
    strncpy(tmp, s, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    int count = 0;
    char *p = tmp;
    while (p && *p && count < max) {
        char *comma = strchr(p, ',');
        if (comma) *comma = '\0';
        /* parse "x|y|RRGGBB-DRDGDB" */
        char *pipe1 = strchr(p, '|');
        if (!pipe1) { p = comma ? comma + 1 : NULL; continue; }
        *pipe1 = '\0';
        char *pipe2 = strchr(pipe1 + 1, '|');
        if (!pipe2) { p = comma ? comma + 1 : NULL; continue; }
        *pipe2 = '\0';
        pts[count].ox = atoi(p);
        pts[count].oy = atoi(pipe1 + 1);
        op_parse_one_color(pipe2 + 1, &pts[count].color);
        count++;
        p = comma ? comma + 1 : NULL;
    }
    return count;
}

const char *xop_find_multi_color(void *ptr, long long x1, long long y1,
                                 long long x2, long long y2,
                                 const char *first_color, const char *offset_color,
                                 double sim, long long dir) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return ""; }
    if (!first_color || !*first_color) { opSetError("op: empty first_color"); return ""; }
    if (!offset_color || !*offset_color) { opSetError("op: empty offset_color"); return ""; }
    if (!op_capture_frame(ctx)) return "";

    op_color_t first_colors[OP_MAX_COLORS];
    int nfirst = op_parse_colors(first_color, first_colors, OP_MAX_COLORS);
    if (nfirst == 0) { opSetError("op: invalid first_color"); return ""; }

    op_offset_point_t offsets[OP_MAX_OFFSETS];
    int noffsets = op_parse_offsets(offset_color, offsets, OP_MAX_OFFSETS);
    if (noffsets == 0) { opSetError("op: invalid offset_color"); return ""; }

    int sx1 = (int)x1, sy1 = (int)y1, sx2 = (int)x2, sy2 = (int)y2;
    if (sx1 < 0) sx1 = 0;
    if (sy1 < 0) sy1 = 0;
    if (sx2 >= ctx->frame_w) sx2 = ctx->frame_w - 1;
    if (sy2 >= ctx->frame_h) sy2 = ctx->frame_h - 1;

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
                    int px = xx + offsets[k].ox;
                    int py = yy + offsets[k].oy;
                    if (px < 0 || px >= ctx->frame_w || py < 0 || py >= ctx->frame_h) {
                        all_ok = 0; break;
                    }
                    int pidx = (py * ctx->frame_w + px) * 4;
                    BYTE pb = ctx->frame_buf[pidx];
                    BYTE pg = ctx->frame_buf[pidx + 1];
                    BYTE pr = ctx->frame_buf[pidx + 2];
                    if (!op_pixel_match(pr, pg, pb, &offsets[k].color, sim)) {
                        all_ok = 0; break;
                    }
                }
                if (all_ok) {
                    snprintf(op_buf, sizeof(op_buf), "%d,%d", xx, yy);
                    return op_buf;
                }
            }
next_mc:
            if (xx == x_end) break;
        }
        if (yy == y_end) break;
    }
    return "";
}

/* ------------------------------------------------------------------ */
/* FindPic - BMP template matching                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    BYTE *data;  /* BGRA */
    int w, h;
} op_bitmap_t;

static int op_load_bmp(const char *path, op_bitmap_t *bmp) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER ih;
    if (fread(&fh, 1, sizeof(fh), f) != sizeof(fh)) { fclose(f); return 0; }
    if (fh.bfType != 0x4D42) { fclose(f); return 0; }
    if (fread(&ih, 1, sizeof(ih), f) != sizeof(ih)) { fclose(f); return 0; }

    int w = ih.biWidth;
    int h = ih.biHeight < 0 ? -ih.biHeight : ih.biHeight;
    int top_down = ih.biHeight < 0;
    int bpp = ih.biBitCount;

    if (bpp != 24 && bpp != 32) { fclose(f); return 0; }

    int src_row = ((w * bpp / 8) + 3) & ~3;
    BYTE *raw = (BYTE *)malloc(src_row * h);
    if (!raw) { fclose(f); return 0; }

    fseek(f, fh.bfOffBits, SEEK_SET);
    if ((int)fread(raw, 1, src_row * h, f) != src_row * h) {
        free(raw); fclose(f); return 0;
    }
    fclose(f);

    bmp->w = w;
    bmp->h = h;
    bmp->data = (BYTE *)malloc(w * h * 4);
    if (!bmp->data) { free(raw); return 0; }

    int row, col;
    for (row = 0; row < h; row++) {
        int src_y = top_down ? row : (h - 1 - row);
        BYTE *src = raw + src_y * src_row;
        BYTE *dst = bmp->data + row * w * 4;
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
    free(raw);
    return 1;
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

const char *xop_find_pic(void *ptr, long long x1, long long y1,
                         long long x2, long long y2,
                         const char *pic_file, const char *delta_color,
                         double sim, long long dir) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return ""; }
    if (!pic_file || !*pic_file) { opSetError("op: empty pic_file"); return ""; }
    if (!op_capture_frame(ctx)) return "";

    op_bitmap_t tmpl;
    if (!op_load_bmp(pic_file, &tmpl)) { opSetError("op: failed to load bmp"); return ""; }

    /* parse delta color "RRGGBB" as tolerance */
    int dr = 0, dg = 0, db = 0;
    if (delta_color && strlen(delta_color) >= 6) {
        dr = op_hex2(delta_color);
        dg = op_hex2(delta_color + 2);
        db = op_hex2(delta_color + 4);
    }

    int sx1 = (int)x1, sy1 = (int)y1, sx2 = (int)x2, sy2 = (int)y2;
    if (sx1 < 0) sx1 = 0;
    if (sy1 < 0) sy1 = 0;
    if (sx2 >= ctx->frame_w) sx2 = ctx->frame_w - 1;
    if (sy2 >= ctx->frame_h) sy2 = ctx->frame_h - 1;

    /* limit search so template fits */
    if (sx2 - sx1 + 1 < tmpl.w || sy2 - sy1 + 1 < tmpl.h) {
        free(tmpl.data);
        return "";
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
                snprintf(op_buf, sizeof(op_buf), "%d,%d", xx, yy);
                free(tmpl.data);
                return op_buf;
            }
            if (xx == x_end) break;
        }
        if (yy == y_end) break;
    }
    free(tmpl.data);
    return "";
}

const char *xop_find_pic_ex(void *ptr, long long x1, long long y1,
                            long long x2, long long y2,
                            const char *pic_file, const char *delta_color,
                            double sim, long long dir) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return ""; }
    if (!pic_file || !*pic_file) { opSetError("op: empty pic_file"); return ""; }
    if (!op_capture_frame(ctx)) return "";

    op_bitmap_t tmpl;
    if (!op_load_bmp(pic_file, &tmpl)) { opSetError("op: failed to load bmp"); return ""; }

    int dr = 0, dg = 0, db = 0;
    if (delta_color && strlen(delta_color) >= 6) {
        dr = op_hex2(delta_color);
        dg = op_hex2(delta_color + 2);
        db = op_hex2(delta_color + 4);
    }

    int sx1 = (int)x1, sy1 = (int)y1, sx2 = (int)x2, sy2 = (int)y2;
    if (sx1 < 0) sx1 = 0;
    if (sy1 < 0) sy1 = 0;
    if (sx2 >= ctx->frame_w) sx2 = ctx->frame_w - 1;
    if (sy2 >= ctx->frame_h) sy2 = ctx->frame_h - 1;

    if (sx2 - sx1 + 1 < tmpl.w || sy2 - sy1 + 1 < tmpl.h) {
        free(tmpl.data);
        return "";
    }

    int x_start, x_end, x_step, y_start, y_end, y_step;
    if (dir == 2 || dir == 3) { x_start = sx2 - tmpl.w + 1; x_end = sx1; x_step = -1; }
    else { x_start = sx1; x_end = sx2 - tmpl.w + 1; x_step = 1; }
    if (dir == 1 || dir == 3) { y_start = sy2 - tmpl.h + 1; y_end = sy1; y_step = -1; }
    else { y_start = sy1; y_end = sy2 - tmpl.h + 1; y_step = 1; }

    int pos = 0;
    int yy, xx;
    for (yy = y_start; ; yy += y_step) {
        for (xx = x_start; ; xx += x_step) {
            if (op_match_at(ctx->frame_buf, ctx->frame_w, xx, yy, &tmpl, dr, dg, db, sim)) {
                if (pos > 0 && pos < (int)sizeof(op_buf) - 1) op_buf[pos++] = '|';
                pos += snprintf(op_buf + pos, sizeof(op_buf) - pos, "%d,%d", xx, yy);
                if (pos >= (int)sizeof(op_buf) - 16) goto done_pic_ex;
            }
            if (xx == x_end) break;
        }
        if (yy == y_end) break;
    }
done_pic_ex:
    op_buf[pos] = '\0';
    free(tmpl.data);
    return op_buf;
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

long long xop_key_press_str(void *ptr, const char *str, long long delay) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    if (!str) return 0;
    op_init_user32();
    int ms = (delay > 0) ? (int)delay : 50;
    const char *p;
    for (p = str; *p; p++) {
        if (strcmp(ctx->keypad_mode, "windows") == 0 && ctx->bind_hwnd) {
            PostMessage(ctx->bind_hwnd, WM_CHAR, (WPARAM)(unsigned char)*p, 0);
        } else if (p_SendInput) {
            INPUT inp;
            memset(&inp, 0, sizeof(inp));
            inp.type = INPUT_KEYBOARD;
            inp.ki.wScan = (unsigned char)*p;
            inp.ki.dwFlags = KEYEVENTF_UNICODE;
            p_SendInput(1, &inp, sizeof(inp));
            inp.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
            p_SendInput(1, &inp, sizeof(inp));
        } else {
            keybd_event((BYTE)VkKeyScanA(*p), 0, 0, 0);
            keybd_event((BYTE)VkKeyScanA(*p), 0, KEYEVENTF_KEYUP, 0);
        }
        Sleep(ms);
    }
    return 1;
}

long long xop_key_char(void *ptr, long long char_code) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    op_init_user32();
    if (strcmp(ctx->keypad_mode, "windows") == 0 && ctx->bind_hwnd) {
        PostMessage(ctx->bind_hwnd, WM_CHAR, (WPARAM)char_code, 0);
    } else if (p_SendInput) {
        INPUT inp;
        memset(&inp, 0, sizeof(inp));
        inp.type = INPUT_KEYBOARD;
        inp.ki.wScan = (unsigned short)char_code;
        inp.ki.dwFlags = KEYEVENTF_UNICODE;
        p_SendInput(1, &inp, sizeof(inp));
        inp.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        p_SendInput(1, &inp, sizeof(inp));
    } else {
        keybd_event((BYTE)char_code, 0, 0, 0);
        keybd_event((BYTE)char_code, 0, KEYEVENTF_KEYUP, 0);
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Window finding (module-level, no instance)                          */
/* ------------------------------------------------------------------ */

long long xop_find_window(const char *class_name, const char *title) {
    HWND hwnd = FindWindowA(
        (class_name && *class_name) ? class_name : NULL,
        (title && *title) ? title : NULL
    );
    return (long long)(intptr_t)hwnd;
}

typedef struct {
    char title_filter[256];
    char class_filter[256];
    int filter_visible;
    DWORD process_ids[1024];
    int process_count;
    char result[65536];
    int pos;
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
    PROCESSENTRY32 pe;
    HANDLE snap;
    char *end = NULL;
    unsigned long numeric_pid;

    if (!ec || !process || !*process) return;
    numeric_pid = strtoul(process, &end, 10);
    if (end != process && *end == '\0' && numeric_pid != 0) {
        ec->process_ids[ec->process_count++] = (DWORD)numeric_pid;
        return;
    }

    op_init_toolhelp();
    if (!p_CreateToolhelp32Snapshot || !p_Process32First || !p_Process32Next) return;
    snap = p_CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    memset(&pe, 0, sizeof(pe));
    pe.dwSize = sizeof(pe);
    if (p_Process32First(snap, &pe)) {
        do {
            if (strstr(pe.szExeFile, process) != NULL &&
                ec->process_count < (int)(sizeof(ec->process_ids) / sizeof(ec->process_ids[0]))) {
                ec->process_ids[ec->process_count++] = pe.th32ProcessID;
            }
        } while (p_Process32Next(snap, &pe));
    }
    CloseHandle(snap);
}

static BOOL CALLBACK op_enum_proc(HWND hwnd, LPARAM lParam) {
    op_enum_ctx_t *ec = (op_enum_ctx_t *)lParam;
    DWORD pid = 0;

    if (ec->filter_visible && !IsWindowVisible(hwnd)) return TRUE;
    if (ec->process_count > 0) {
        GetWindowThreadProcessId(hwnd, &pid);
        if (!op_enum_process_matches(ec, pid)) return TRUE;
    }

    char title[256] = "";
    char cls[256] = "";
    GetWindowTextA(hwnd, title, sizeof(title));
    GetClassNameA(hwnd, cls, sizeof(cls));

    if (ec->title_filter[0] && !strstr(title, ec->title_filter)) return TRUE;
    if (ec->class_filter[0] && !strstr(cls, ec->class_filter)) return TRUE;

    if (ec->pos > 0 && ec->pos < (int)sizeof(ec->result) - 1)
        ec->result[ec->pos++] = ',';
    ec->pos += snprintf(ec->result + ec->pos, sizeof(ec->result) - ec->pos,
                        "%lld", (long long)(intptr_t)hwnd);
    return TRUE;
}

const char *xop_enum_window(long long parent, const char *title,
                            const char *class_name, long long filter) {
    op_enum_ctx_t ec;
    memset(&ec, 0, sizeof(ec));
    if (title && *title) strncpy(ec.title_filter, title, 255);
    if (class_name && *class_name) strncpy(ec.class_filter, class_name, 255);
    ec.filter_visible = (filter & 1) ? 1 : 0;

    if (parent) {
        EnumChildWindows((HWND)(intptr_t)parent, op_enum_proc, (LPARAM)&ec);
    } else {
        EnumWindows(op_enum_proc, (LPARAM)&ec);
    }
    ec.result[ec.pos] = '\0';
    /* copy to static buffer for return */
    memcpy(op_buf, ec.result, ec.pos + 1);
    return op_buf;
}

const char *xop_enum_window_by_process(const char *process, const char *title,
                                       const char *class_name, long long filter) {
    op_enum_ctx_t ec;

    memset(&ec, 0, sizeof(ec));
    if (title && *title) strncpy(ec.title_filter, title, 255);
    if (class_name && *class_name) strncpy(ec.class_filter, class_name, 255);
    ec.filter_visible = (filter & 1) ? 1 : 0;
    op_enum_collect_processes(&ec, process);
    if (ec.process_count > 0) {
        EnumWindows(op_enum_proc, (LPARAM)&ec);
    }
    ec.result[ec.pos] = '\0';
    memcpy(op_buf, ec.result, ec.pos + 1);
    return op_buf;
}

const char *xop_get_screen_size(void) {
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    snprintf(op_buf, sizeof(op_buf), "%dx%d", w, h);
    return op_buf;
}

const char *xop_get_cursor_pos(void) {
    POINT pt;
    GetCursorPos(&pt);
    snprintf(op_buf, sizeof(op_buf), "%ld,%ld", pt.x, pt.y);
    return op_buf;
}

/* ------------------------------------------------------------------ */
/* Window management (module-level)                                    */
/* ------------------------------------------------------------------ */

const char *xop_get_window_rect(long long hwnd) {
    RECT rc;
    if (!GetWindowRect((HWND)(intptr_t)hwnd, &rc)) return "0,0,0,0";
    snprintf(op_buf, sizeof(op_buf), "%ld,%ld,%ld,%ld", rc.left, rc.top, rc.right, rc.bottom);
    return op_buf;
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

const char *xop_get_window_title(long long hwnd) {
    HWND h = (HWND)(intptr_t)hwnd;
    if (!IsWindow(h)) return "";
    GetWindowTextA(h, op_buf, sizeof(op_buf));
    return op_buf;
}

long long xop_set_window_title(long long hwnd, const char *title) {
    HWND h = (HWND)(intptr_t)hwnd;
    if (!IsWindow(h)) return 0;
    return SetWindowTextA(h, title ? title : "");
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
    if (!IsWindow(h) || !str) return 0;
    const char *p;
    for (p = str; *p; p++) {
        PostMessageA(h, WM_CHAR, (WPARAM)(unsigned char)*p, 0);
    }
    return 1;
}

const char *xop_client_to_screen(long long hwnd, long long x, long long y) {
    POINT pt = {(LONG)x, (LONG)y};
    ClientToScreen((HWND)(intptr_t)hwnd, &pt);
    snprintf(op_buf, sizeof(op_buf), "%ld,%ld", pt.x, pt.y);
    return op_buf;
}

const char *xop_screen_to_client(long long hwnd, long long x, long long y) {
    POINT pt = {(LONG)x, (LONG)y};
    ScreenToClient((HWND)(intptr_t)hwnd, &pt);
    snprintf(op_buf, sizeof(op_buf), "%ld,%ld", pt.x, pt.y);
    return op_buf;
}

/* ------------------------------------------------------------------ */
/* FindColorBlock & GetColorNum                                        */
/* ------------------------------------------------------------------ */

const char *xop_find_color_block(void *ptr, long long x1, long long y1,
                                 long long x2, long long y2,
                                 const char *color, double sim,
                                 long long width, long long height) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return ""; }
    if (!color || !*color) { opSetError("op: empty color"); return ""; }
    if (!op_capture_frame(ctx)) return "";

    op_color_t colors[OP_MAX_COLORS];
    int ncolors = op_parse_colors(color, colors, OP_MAX_COLORS);
    if (ncolors == 0) { opSetError("op: invalid color format"); return ""; }

    int bw = (int)width, bh = (int)height;
    if (bw < 1) bw = 1;
    if (bh < 1) bh = 1;

    int sx1 = (int)x1, sy1 = (int)y1, sx2 = (int)x2, sy2 = (int)y2;
    if (sx1 < 0) sx1 = 0;
    if (sy1 < 0) sy1 = 0;
    if (sx2 >= ctx->frame_w) sx2 = ctx->frame_w - 1;
    if (sy2 >= ctx->frame_h) sy2 = ctx->frame_h - 1;

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
                snprintf(op_buf, sizeof(op_buf), "%d,%d", xx, yy);
                return op_buf;
            }
        }
    }
    return "";
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
    if (ncolors == 0) return 0;

    int sx1 = (int)x1, sy1 = (int)y1, sx2 = (int)x2, sy2 = (int)y2;
    if (sx1 < 0) sx1 = 0;
    if (sy1 < 0) sy1 = 0;
    if (sx2 >= ctx->frame_w) sx2 = ctx->frame_w - 1;
    if (sy2 >= ctx->frame_h) sy2 = ctx->frame_h - 1;

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

const char *xop_find_pic_n(void *ptr, long long x1, long long y1,
                           long long x2, long long y2,
                           const char *pic_files, const char *delta_color,
                           double sim, long long dir) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return ""; }
    if (!pic_files || !*pic_files) { opSetError("op: empty pic_files"); return ""; }
    if (!op_capture_frame(ctx)) return "";

    int dr = 0, dg = 0, db = 0;
    if (delta_color && strlen(delta_color) >= 6) {
        dr = op_hex2(delta_color);
        dg = op_hex2(delta_color + 2);
        db = op_hex2(delta_color + 4);
    }

    char tmp[4096];
    strncpy(tmp, pic_files, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    int index = 0;
    char *p = tmp;
    while (p && *p) {
        char *sep = strchr(p, '|');
        if (sep) *sep = '\0';

        op_bitmap_t tmpl;
        if (op_load_bmp(p, &tmpl)) {
            int sx1 = (int)x1, sy1 = (int)y1, sx2 = (int)x2, sy2 = (int)y2;
            if (sx1 < 0) sx1 = 0;
            if (sy1 < 0) sy1 = 0;
            if (sx2 >= ctx->frame_w) sx2 = ctx->frame_w - 1;
            if (sy2 >= ctx->frame_h) sy2 = ctx->frame_h - 1;

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
                            snprintf(op_buf, sizeof(op_buf), "%d,%d,%d", index, xx, yy);
                            free(tmpl.data);
                            return op_buf;
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
    return "";
}

/* ------------------------------------------------------------------ */
/* CaptureToMem - returns hex string of BGRA pixels                    */
/* ------------------------------------------------------------------ */

const char *xop_capture_to_mem(void *ptr, long long x1, long long y1,
                               long long x2, long long y2) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return ""; }
    if (!op_capture_frame(ctx)) return "";

    int w = (int)(x2 - x1);
    int h = (int)(y2 - y1);
    if (w <= 0 || h <= 0) { opSetError("op: invalid region"); return ""; }
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x1 + w > ctx->frame_w) w = ctx->frame_w - (int)x1;
    if (y1 + h > ctx->frame_h) h = ctx->frame_h - (int)y1;
    if (w <= 0 || h <= 0) return "";

    /* limit output size to avoid overflow: max ~16k pixels */
    int max_pixels = ((int)sizeof(op_buf) - 64) / 8;
    int total = w * h;
    if (total > max_pixels) total = max_pixels;

    int pos = 0;
    pos += snprintf(op_buf + pos, sizeof(op_buf) - pos, "%dx%d|", w, h);
    int i;
    for (i = 0; i < total && pos < (int)sizeof(op_buf) - 10; i++) {
        int px = (int)x1 + (i % w);
        int py = (int)y1 + (i / w);
        int idx = (py * ctx->frame_w + px) * 4;
        pos += snprintf(op_buf + pos, sizeof(op_buf) - pos, "%02X%02X%02X%02X",
                        ctx->frame_buf[idx], ctx->frame_buf[idx+1],
                        ctx->frame_buf[idx+2], ctx->frame_buf[idx+3]);
    }
    op_buf[pos] = '\0';
    return op_buf;
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

    op_bitmap_t tmpl;
    if (!op_load_bmp(pic_file, &tmpl)) { opSetError("op: failed to load bmp"); return 0.0; }

    int w = (int)(x2 - x1);
    int h = (int)(y2 - y1);
    if (w != tmpl.w || h != tmpl.h) { free(tmpl.data); return 0.0; }
    if (x1 < 0 || y1 < 0 || x1 + w > ctx->frame_w || y1 + h > ctx->frame_h) {
        free(tmpl.data); return 0.0;
    }

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

const char *xop_image_hash(void *ptr, long long x1, long long y1,
                           long long x2, long long y2) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return ""; }
    if (!op_capture_frame(ctx)) return "";

    int w = (int)(x2 - x1);
    int h = (int)(y2 - y1);
    if (w <= 0 || h <= 0) return "";
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x1 + w > ctx->frame_w) w = ctx->frame_w - (int)x1;
    if (y1 + h > ctx->frame_h) h = ctx->frame_h - (int)y1;

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
    snprintf(op_buf, sizeof(op_buf), "%016llX", hv);
    return op_buf;
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
    if (!ctx) { opSetError("op: null context"); return 0; }
    if (!points || !*points) { opSetError("op: empty points"); return 0; }

    int ms = (delay > 0) ? (int)delay : 20;
    char tmp[4096];
    strncpy(tmp, points, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    /* format: "x1,y1,x2,y2,x3,y3,..." */
    int coords[512];
    int ncoords = 0;
    char *p = tmp;
    while (p && *p && ncoords < 512) {
        char *sep = strchr(p, ',');
        if (sep) *sep = '\0';
        coords[ncoords++] = atoi(p);
        p = sep ? sep + 1 : NULL;
    }
    if (ncoords < 4) { opSetError("op: need at least 2 points"); return 0; }
    ncoords &= ~1; /* make even */

    /* move to first point and press */
    op_mouse_send(ctx, MOUSEEVENTF_MOVE, coords[0], coords[1], 0);
    op_mouse_send(ctx, MOUSEEVENTF_LEFTDOWN, 0, 0, 0);
    Sleep(ms);

    int i;
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
    /* F1-F12 */
    if ((name[0] == 'f' || name[0] == 'F') && name[1] >= '0' && name[1] <= '9') {
        int fn = atoi(name + 1);
        if (fn >= 1 && fn <= 24) return VK_F1 + fn - 1;
    }
    /* single letter A-Z */
    if (strlen(name) == 1 && ((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= 'A' && name[0] <= 'Z')))
        return (name[0] >= 'a') ? name[0] - 32 : name[0];
    /* single digit 0-9 */
    if (strlen(name) == 1 && name[0] >= '0' && name[0] <= '9')
        return name[0];
    return 0;
}

long long xop_key_combo(void *ptr, const char *combo) {
    op_context *ctx = (op_context *)ptr;
    if (!ctx) { opSetError("op: null context"); return 0; }
    if (!combo || !*combo) { opSetError("op: empty combo"); return 0; }

    char tmp[256];
    strncpy(tmp, combo, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    int vks[8];
    int nvk = 0;
    char *p = tmp;
    while (p && *p && nvk < 8) {
        char *sep = strchr(p, '+');
        if (sep) *sep = '\0';
        /* trim spaces */
        while (*p == ' ') p++;
        char *end = p + strlen(p) - 1;
        while (end > p && *end == ' ') *end-- = '\0';
        int vk = op_parse_vk(p);
        if (vk) vks[nvk++] = vk;
        p = sep ? sep + 1 : NULL;
    }
    if (nvk == 0) { opSetError("op: no valid keys in combo"); return 0; }

    int bg = (strcmp(ctx->keypad_mode, "windows") == 0 && ctx->bind_hwnd);
    int i;
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

static HANDLE op_open_proc(long long pid) {
    return OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
                       FALSE, (DWORD)pid);
}

long long xop_read_int(long long pid, long long addr) {
    HANDLE h = op_open_proc(pid);
    if (!h) { opSetError("op: cannot open process"); return 0; }
    int val = 0;
    SIZE_T rd = 0;
    ReadProcessMemory(h, (LPCVOID)(intptr_t)addr, &val, sizeof(val), &rd);
    CloseHandle(h);
    return (long long)val;
}

double xop_read_float(long long pid, long long addr) {
    HANDLE h = op_open_proc(pid);
    if (!h) { opSetError("op: cannot open process"); return 0.0; }
    float val = 0.0f;
    SIZE_T rd = 0;
    ReadProcessMemory(h, (LPCVOID)(intptr_t)addr, &val, sizeof(val), &rd);
    CloseHandle(h);
    return (double)val;
}

const char *xop_read_string(long long pid, long long addr, long long len) {
    HANDLE h = op_open_proc(pid);
    if (!h) { opSetError("op: cannot open process"); return ""; }
    int n = (len > 0 && len < (long long)sizeof(op_buf) - 1) ? (int)len : 256;
    SIZE_T rd = 0;
    memset(op_buf, 0, n + 1);
    ReadProcessMemory(h, (LPCVOID)(intptr_t)addr, op_buf, n, &rd);
    op_buf[rd] = '\0';
    CloseHandle(h);
    return op_buf;
}

long long xop_write_int(long long pid, long long addr, long long value) {
    HANDLE h = op_open_proc(pid);
    if (!h) { opSetError("op: cannot open process"); return 0; }
    int val = (int)value;
    SIZE_T wr = 0;
    BOOL ok = WriteProcessMemory(h, (LPVOID)(intptr_t)addr, &val, sizeof(val), &wr);
    CloseHandle(h);
    return ok ? 1 : 0;
}

long long xop_write_float(long long pid, long long addr, double value) {
    HANDLE h = op_open_proc(pid);
    if (!h) { opSetError("op: cannot open process"); return 0; }
    float val = (float)value;
    SIZE_T wr = 0;
    BOOL ok = WriteProcessMemory(h, (LPVOID)(intptr_t)addr, &val, sizeof(val), &wr);
    CloseHandle(h);
    return ok ? 1 : 0;
}

long long xop_write_string(long long pid, long long addr, const char *value) {
    HANDLE h = op_open_proc(pid);
    if (!h) { opSetError("op: cannot open process"); return 0; }
    if (!value) value = "";
    SIZE_T wr = 0;
    BOOL ok = WriteProcessMemory(h, (LPVOID)(intptr_t)addr, value, strlen(value) + 1, &wr);
    CloseHandle(h);
    return ok ? 1 : 0;
}

const char *xop_find_memory(long long pid, long long start_addr,
                            long long end_addr, const char *pattern) {
    HANDLE h = op_open_proc(pid);
    if (!h) { opSetError("op: cannot open process"); return ""; }
    if (!pattern || !*pattern) { CloseHandle(h); return ""; }

    /* parse hex pattern "AABB CC DD" (spaces = wildcards) */
    unsigned char pat[256];
    unsigned char mask[256]; /* 1=match, 0=wildcard */
    int plen = 0;
    const char *pp = pattern;
    while (*pp && plen < 256) {
        if (*pp == ' ') { pp++; continue; }
        if (pp[0] == '?' && pp[1] == '?') {
            pat[plen] = 0; mask[plen] = 0; plen++; pp += 2;
        } else {
            pat[plen] = (unsigned char)op_hex2(pp);
            mask[plen] = 1; plen++;
            pp += 2;
        }
    }
    if (plen == 0) { CloseHandle(h); return ""; }

    /* scan memory in 4KB pages */
    long long addr;
    unsigned char page[4096];
    SIZE_T rd;
    for (addr = start_addr; addr < end_addr; addr += 4096) {
        if (!ReadProcessMemory(h, (LPCVOID)(intptr_t)addr, page, 4096, &rd)) continue;
        long long i;
        for (i = 0; i <= (long long)rd - plen; i++) {
            int j, found = 1;
            for (j = 0; j < plen; j++) {
                if (mask[j] && page[i + j] != pat[j]) { found = 0; break; }
            }
            if (found) {
                snprintf(op_buf, sizeof(op_buf), "%lld", addr + i);
                CloseHandle(h);
                return op_buf;
            }
        }
    }
    CloseHandle(h);
    return "";
}

/* ------------------------------------------------------------------ */
/* Process enumeration (module-level)                                  */
/* ------------------------------------------------------------------ */

const char *xop_enum_process(const char *filter) {
    op_init_toolhelp();
    if (!p_CreateToolhelp32Snapshot || !p_Process32First || !p_Process32Next) {
        opSetError("op: toolhelp not available"); return "";
    }
    HANDLE snap = p_CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) { opSetError("op: snapshot failed"); return ""; }

    PROCESSENTRY32 pe;
    memset(&pe, 0, sizeof(pe));
    pe.dwSize = sizeof(pe);

    int pos = 0;
    if (p_Process32First(snap, &pe)) {
        do {
            if (filter && *filter && !strstr(pe.szExeFile, filter)) continue;
            if (pos > 0 && pos < (int)sizeof(op_buf) - 1) op_buf[pos++] = '|';
            pos += snprintf(op_buf + pos, sizeof(op_buf) - pos, "%lu:%s",
                            pe.th32ProcessID, pe.szExeFile);
            if (pos >= (int)sizeof(op_buf) - 64) break;
        } while (p_Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    op_buf[pos] = '\0';
    return op_buf;
}

const char *xop_get_process_modules(long long pid) {
    op_init_toolhelp();
    if (!p_CreateToolhelp32Snapshot || !p_Module32First || !p_Module32Next) {
        opSetError("op: toolhelp not available"); return "";
    }
    HANDLE snap = p_CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, (DWORD)pid);
    if (snap == INVALID_HANDLE_VALUE) { opSetError("op: module snapshot failed"); return ""; }

    MODULEENTRY32 me;
    memset(&me, 0, sizeof(me));
    me.dwSize = sizeof(me);

    int pos = 0;
    if (p_Module32First(snap, &me)) {
        do {
            if (pos > 0 && pos < (int)sizeof(op_buf) - 1) op_buf[pos++] = '|';
            pos += snprintf(op_buf + pos, sizeof(op_buf) - pos, "%08llX:%s",
                            (long long)(intptr_t)me.modBaseAddr, me.szModule);
            if (pos >= (int)sizeof(op_buf) - 64) break;
        } while (p_Module32Next(snap, &me));
    }
    CloseHandle(snap);
    op_buf[pos] = '\0';
    return op_buf;
}

/* ------------------------------------------------------------------ */
/* System tools (module-level)                                         */
/* ------------------------------------------------------------------ */

void xop_delay(long long ms) {
    if (ms > 0) Sleep((DWORD)ms);
}

long long xop_random_int(long long min, long long max) {
    if (min >= max) return min;
    long long range = max - min + 1;
    return min + (long long)(rand() % (int)range);
}

long long xop_set_clipboard(const char *text) {
    if (!text) text = "";
    if (!OpenClipboard(NULL)) { opSetError("op: cannot open clipboard"); return 0; }
    EmptyClipboard();
    int len = (int)strlen(text) + 1;
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, len);
    if (!hg) { CloseClipboard(); return 0; }
    char *dst = (char *)GlobalLock(hg);
    memcpy(dst, text, len);
    GlobalUnlock(hg);
    SetClipboardData(CF_TEXT, hg);
    CloseClipboard();
    return 1;
}

const char *xop_get_clipboard(void) {
    if (!OpenClipboard(NULL)) { opSetError("op: cannot open clipboard"); return ""; }
    HANDLE hg = GetClipboardData(CF_TEXT);
    if (!hg) { CloseClipboard(); op_buf[0] = '\0'; return op_buf; }
    char *src = (char *)GlobalLock(hg);
    if (src) {
        strncpy(op_buf, src, sizeof(op_buf) - 1);
        op_buf[sizeof(op_buf) - 1] = '\0';
        GlobalUnlock(hg);
    } else {
        op_buf[0] = '\0';
    }
    CloseClipboard();
    return op_buf;
}
