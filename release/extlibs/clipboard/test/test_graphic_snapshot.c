#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "clipboard.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "graphic snapshot line %d: %s (%s)\n", \
        __LINE__, #expr, x_clipboard_last_error()); \
    return 1; \
} } while (0)

static int check_bitmap(void)
{
    BITMAPINFO info;
    HBITMAP bitmap;
    HBITMAP restored;
    DIBSECTION section;
    void* bits = NULL;
    void* saved;
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 2;
    info.bmiHeader.biHeight = 2;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(NULL, &info, DIB_RGB_COLORS, &bits, NULL, 0);
    CHECK(bitmap != NULL && bits != NULL);
    memset(bits, 0x5a, 16);
    CHECK(OpenClipboard(NULL));
    CHECK(EmptyClipboard());
    CHECK(SetClipboardData(CF_BITMAP, bitmap) != NULL);
    CHECK(CloseClipboard());

    saved = x_clipboard_snapshot();
    CHECK(saved != NULL);
    CHECK(x_clipboard_clear());
    CHECK(x_clipboard_restore(saved));
    CHECK(OpenClipboard(NULL));
    restored = (HBITMAP)GetClipboardData(CF_BITMAP);
    CHECK(restored != NULL);
    CHECK(GetObjectW(restored, sizeof(section), &section) != 0);
    CHECK(section.dsBm.bmWidth == 2 && section.dsBm.bmHeight == 2);
    CHECK(CloseClipboard());
    CHECK(x_clipboard_snapshot_release(saved));
    return 0;
}

static int check_metafile(void)
{
    RECT bounds = {0, 0, 100, 100};
    HDC dc = CreateEnhMetaFileW(NULL, NULL, &bounds, NULL);
    HENHMETAFILE metafile;
    HENHMETAFILE restored;
    void* saved;
    CHECK(dc != NULL);
    Rectangle(dc, 10, 10, 90, 90);
    metafile = CloseEnhMetaFile(dc);
    CHECK(metafile != NULL);
    CHECK(OpenClipboard(NULL));
    CHECK(EmptyClipboard());
    CHECK(SetClipboardData(CF_ENHMETAFILE, metafile) != NULL);
    CHECK(CloseClipboard());

    saved = x_clipboard_snapshot();
    if (saved == NULL && OpenClipboard(NULL)) {
        UINT fmt = 0;
        while ((fmt = EnumClipboardFormats(fmt)) != 0) {
            HANDLE value = GetClipboardData(fmt);
            fprintf(stderr, "metafile format=%u data=%p size=%llu\n",
                (unsigned)fmt, (void*)value,
                (unsigned long long)(value != NULL ? GlobalSize((HGLOBAL)value) : 0));
        }
        CloseClipboard();
    }
    CHECK(saved != NULL);
    CHECK(x_clipboard_clear());
    CHECK(x_clipboard_restore(saved));
    CHECK(OpenClipboard(NULL));
    restored = (HENHMETAFILE)GetClipboardData(CF_ENHMETAFILE);
    CHECK(restored != NULL && GetEnhMetaFileBits(restored, 0, NULL) > 0);
    HGLOBAL picture_data = (HGLOBAL)GetClipboardData(CF_METAFILEPICT);
    CHECK(picture_data != NULL && GlobalSize(picture_data) >= sizeof(METAFILEPICT));
    METAFILEPICT* picture = (METAFILEPICT*)GlobalLock(picture_data);
    CHECK(picture != NULL && picture->hMF != NULL);
    CHECK(GetMetaFileBitsEx(picture->hMF, 0, NULL) > 0);
    SetLastError(NO_ERROR);
    CHECK(GlobalUnlock(picture_data) || GetLastError() == NO_ERROR);
    CHECK(CloseClipboard());
    CHECK(x_clipboard_clear());
    CHECK(x_clipboard_restore(saved));
    CHECK(IsClipboardFormatAvailable(CF_ENHMETAFILE));
    CHECK(IsClipboardFormatAvailable(CF_METAFILEPICT));
    CHECK(x_clipboard_snapshot_release(saved));
    saved = x_clipboard_snapshot();
    CHECK(saved != NULL && x_clipboard_snapshot_release(saved));
    DWORD before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    CHECK(before > 0);
    for (int i = 0; i < 64; ++i) {
        saved = x_clipboard_snapshot();
        CHECK(saved != NULL && x_clipboard_snapshot_release(saved));
    }
    DWORD after = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    if (after > before + 2) {
        fprintf(stderr, "metafile GDI handles: before=%lu after=%lu\n",
            (unsigned long)before, (unsigned long)after);
        return 1;
    }
    return 0;
}

int main(void)
{
    /* 独立 window station 的剪贴板与当前交互桌面隔离。 */
    HWINSTA station = CreateWindowStationW(NULL, 0, WINSTA_ALL_ACCESS, NULL);
    HDESK desktop;
    CHECK(station != NULL && SetProcessWindowStation(station));
    desktop = CreateDesktopW(L"clipboard-graphics-test", NULL, NULL, 0,
        GENERIC_ALL, NULL);
    CHECK(desktop != NULL && SetThreadDesktop(desktop));
    CHECK(check_bitmap() == 0);
    CHECK(check_metafile() == 0);
    puts("clipboard graphic snapshot: PASS");
    return 0;
}
