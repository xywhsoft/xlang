/*
 * op_wgl_hook.c - OpenGL WGL SwapBuffers Hook DLL
 * Injected into target process, hooks wglSwapBuffers via IAT patch.
 * Captures framebuffer via glReadPixels after each swap.
 * TCC-compatible: compile with `tcc -shared -o op_wgl_hook.dll op_wgl_hook.c -luser32 -lgdi32 -lopengl32`
 */
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "op_hook_shared.h"

/* ------------------------------------------------------------------ */
/* OpenGL declarations (TCC has no GL/gl.h)                            */
/* ------------------------------------------------------------------ */

#define GL_FRONT         0x0404
#define GL_BGRA_EXT      0x80E1
#define GL_UNSIGNED_BYTE 0x1401

typedef void   (WINAPI *PFN_glReadBuffer)(unsigned int);
typedef void   (WINAPI *PFN_glReadPixels)(int, int, int, int, unsigned int, unsigned int, void*);
typedef int    (WINAPI *PFN_glGetError)(void);
typedef void   (WINAPI *PFN_glGetIntegerv)(unsigned int, int*);
typedef void   (WINAPI *PFN_glPixelStorei)(unsigned int, int);

#define GL_VIEWPORT      0x0BA2
#define GL_PACK_ALIGNMENT 0x0D05

static PFN_glReadBuffer  p_glReadBuffer  = NULL;
static PFN_glReadPixels  p_glReadPixels  = NULL;
static PFN_glGetIntegerv p_glGetIntegerv = NULL;
static PFN_glPixelStorei p_glPixelStorei = NULL;

/* ------------------------------------------------------------------ */
/* Hook state                                                          */
/* ------------------------------------------------------------------ */

static void *g_shared_mem = NULL;
static unsigned int g_map_size = 4 * 1024 * 1024;
static BYTE *g_read_buf = NULL;
static int g_buf_size = 0;

typedef BOOL (WINAPI *PFN_wglSwapBuffers)(HDC);
static PFN_wglSwapBuffers g_orig_swap = NULL;

/* ------------------------------------------------------------------ */
/* Hooked wglSwapBuffers                                               */
/* ------------------------------------------------------------------ */

static BOOL WINAPI hook_swap_buffers(HDC hdc) {
    if (g_shared_mem && p_glReadPixels && p_glGetIntegerv) {
        int viewport[4] = {0};
        p_glGetIntegerv(GL_VIEWPORT, viewport);
        int w = viewport[2], h = viewport[3];

        if (w > 0 && h > 0 && w <= 4096 && h <= 4096) {
            int needed = w * h * 4;
            if (needed > g_buf_size) {
                if (g_read_buf) free(g_read_buf);
                g_read_buf = (BYTE*)malloc(needed);
                g_buf_size = needed;
            }
            if (g_read_buf && (unsigned int)needed + sizeof(op_hook_header) <= g_map_size) {
                if (p_glPixelStorei) p_glPixelStorei(GL_PACK_ALIGNMENT, 1);
                p_glReadBuffer(GL_FRONT);
                p_glReadPixels(0, 0, w, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, g_read_buf);

                /* OpenGL reads bottom-up, flip to top-down */
                int row;
                unsigned char *dst = op_hook_pixels(g_shared_mem);
                op_hook_header *hdr = (op_hook_header*)g_shared_mem;
                hdr->magic = OP_HOOK_MAGIC;
                hdr->width = w;
                hdr->height = h;
                hdr->format = 0;
                hdr->frame_counter++;
                for (row = 0; row < h; row++) {
                    memcpy(dst + row * w * 4,
                           g_read_buf + (h - 1 - row) * w * 4,
                           w * 4);
                }
            }
        }
    }
    return g_orig_swap(hdc);
}

/* ------------------------------------------------------------------ */
/* IAT hook: patch wglSwapBuffers in all loaded modules                */
/* ------------------------------------------------------------------ */

static void patch_iat(void) {
    HMODULE gl = GetModuleHandleA("opengl32.dll");
    if (!gl) gl = LoadLibraryA("opengl32.dll");
    if (!gl) return;

    /* Get real wglSwapBuffers address */
    g_orig_swap = (PFN_wglSwapBuffers)GetProcAddress(gl, "wglSwapBuffers");
    if (!g_orig_swap) return;

    /* Load GL functions */
    p_glReadBuffer  = (PFN_glReadBuffer)GetProcAddress(gl, "glReadBuffer");
    p_glReadPixels  = (PFN_glReadPixels)GetProcAddress(gl, "glReadPixels");
    p_glGetIntegerv = (PFN_glGetIntegerv)GetProcAddress(gl, "glGetIntegerv");
    p_glPixelStorei = (PFN_glPixelStorei)GetProcAddress(gl, "glPixelStorei");

    /* Patch IAT of main module */
    HMODULE main_mod = GetModuleHandleA(NULL);
    if (!main_mod) return;
    BYTE *base = (BYTE*)main_mod;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != 0x5A4D) return;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY *dir = &nt->OptionalHeader.DataDirectory[1]; /* IMPORT */
    if (!dir->VirtualAddress) return;
    IMAGE_IMPORT_DESCRIPTOR *imp = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir->VirtualAddress);
    while (imp->Name) {
        IMAGE_THUNK_DATA *thunk = (IMAGE_THUNK_DATA*)(base + imp->FirstThunk);
        while (thunk->u1.Function) {
            void **fn = (void**)&thunk->u1.Function;
            if (*fn == (void*)g_orig_swap) {
                DWORD old;
                VirtualProtect(fn, sizeof(void*), PAGE_READWRITE, &old);
                *fn = (void*)hook_swap_buffers;
                VirtualProtect(fn, sizeof(void*), old, &old);
            }
            thunk++;
        }
        imp++;
    }
}

/* ------------------------------------------------------------------ */
/* DLL Entry                                                           */
/* ------------------------------------------------------------------ */

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, void *reserved) {
    (void)hinst; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        DWORD pid = GetCurrentProcessId();
        g_shared_mem = op_hook_open_shared(pid, g_map_size);
        patch_iat();
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_read_buf) { free(g_read_buf); g_read_buf = NULL; }
        if (g_shared_mem) { UnmapViewOfFile(g_shared_mem); g_shared_mem = NULL; }
    }
    return TRUE;
}
