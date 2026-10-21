#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "clipboard.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "clipboard test line %d: %s (%s)\n", __LINE__, \
                #expr, x_clipboard_last_error()); \
        return 1; \
    } \
} while (0)

static int value_equals(xvalue* value, const char* expected)
{
    xstrview text;
    size_t size = strlen(expected);
    int equal = value != NULL && xrtValueGetString(value, &text) &&
                text.Size == size && memcmp(text.Data, expected, size) == 0;
    xrtValueRelease(value);
    return equal;
}

static int value_contains(xvalue* value, const char* expected)
{
    xstrview text;
    size_t size = strlen(expected);
    size_t i;
    int found = 0;
    if (value != NULL && xrtValueGetString(value, &text) && text.Size >= size) {
        for (i = 0; i <= text.Size - size; ++i) {
            if (memcmp(text.Data + i, expected, size) == 0) {
                found = 1;
                break;
            }
        }
    }
    xrtValueRelease(value);
    return found;
}

static void write_u32(unsigned char* out, unsigned long value)
{
    out[0] = (unsigned char)value;
    out[1] = (unsigned char)(value >> 8);
    out[2] = (unsigned char)(value >> 16);
    out[3] = (unsigned char)(value >> 24);
}

static unsigned long read_u32(const unsigned char* data)
{
    return (unsigned long)data[0] | ((unsigned long)data[1] << 8) |
           ((unsigned long)data[2] << 16) | ((unsigned long)data[3] << 24);
}

static int write_file(const wchar_t* path, const void* data, DWORD size)
{
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written = 0;
    if (file == INVALID_HANDLE_VALUE) return 0;
    BOOL ok = WriteFile(file, data, size, &written, NULL);
    CloseHandle(file);
    return ok && written == size;
}

static int file_equals(const wchar_t* path, const void* expected, DWORD size)
{
    unsigned char actual[16];
    DWORD read = 0;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    BOOL ok = size <= sizeof(actual) && GetFileSize(file, NULL) == size &&
              ReadFile(file, actual, size, &read, NULL);
    CloseHandle(file);
    return ok && read == size && memcmp(actual, expected, size) == 0;
}

static int same_bmp_pixels(const wchar_t* path, const unsigned char* expected)
{
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    unsigned char actual[256];
    DWORD read = 0;
    if (file == INVALID_HANDLE_VALUE) return 0;
    DWORD size = GetFileSize(file, NULL);
    BOOL ok = size >= 58 && size <= sizeof(actual) &&
              ReadFile(file, actual, size, &read, NULL);
    CloseHandle(file);
    if (!ok || read != size || actual[0] != 'B' || actual[1] != 'M' ||
        read_u32(actual + 18) != 1 || read_u32(actual + 22) != 1 ||
        actual[28] != 24) return 0;
    unsigned long offset = read_u32(actual + 10);
    return offset <= size - 4 && memcmp(actual + offset, expected + 54, 4) == 0;
}

static DWORD WINAPI check_thread_state(void* context)
{
    HDESK desktop = (HDESK)context;
    if (!SetThreadDesktop(desktop) || x_clipboard_last_error()[0] != '\0') return 1;
    if (!x_clipboard_set_text("worker")) return 1;
    void* saved = x_clipboard_snapshot();
    if (saved == NULL) return 1;
    int ok = x_clipboard_set_text("worker-temp") && x_clipboard_restore(saved) &&
             value_equals(x_clipboard_get_text(), "worker");
    if (!x_clipboard_snapshot_release(saved)) return 1;
    return ok ? 0 : 1;
}

int main(int argc, char** argv)
{
    if (argc != 2 || argv[1][0] == '\0') {
        fputs("clipboard test needs its build directory\n", stderr);
        return 1;
    }
    /* 单独的 window station 有自己的剪贴板，不碰交互桌面的内容。 */
    HWINSTA station = CreateWindowStationW(NULL, 0, WINSTA_ALL_ACCESS, NULL);
    if (!station || !SetProcessWindowStation(station)) {
        fprintf(stderr, "isolated station: %lu\n", (unsigned long)GetLastError());
        return 1;
    }
    HDESK desktop = CreateDesktopW(L"clipboard-test", NULL, NULL, 0,
                                    GENERIC_ALL, NULL);
    if (!desktop || !SetThreadDesktop(desktop)) {
        fprintf(stderr, "isolated desktop: %lu\n", (unsigned long)GetLastError());
        return 1;
    }

    xvalue* invalid_files = xrtValueInt(1);
    CHECK(invalid_files != NULL);
    CHECK(!x_clipboard_set_files(invalid_files));
    CHECK(strcmp(x_clipboard_last_error(), "clipboard: invalid input or data") == 0);
    xrtValueRelease(invalid_files);
    invalid_files = xrtValueArray();
    CHECK(invalid_files != NULL);
    CHECK(!x_clipboard_set_files(invalid_files));
    CHECK(strcmp(x_clipboard_last_error(), "clipboard: invalid input or data") == 0);
    xvalue* invalid_item = xrtValueInt(1);
    CHECK(invalid_item != NULL && xrtValueArrayAppendTake(invalid_files, &invalid_item));
    xrtClearError();
    CHECK(!x_clipboard_set_files(invalid_files));
    CHECK(strcmp(x_clipboard_last_error(), "clipboard: invalid input or data") == 0);
    CHECK(xrtGetError() == NULL);
    xrtValueRelease(invalid_files);
    CHECK(!x_clipboard_set_file(""));
    CHECK(strcmp(x_clipboard_last_error(), "clipboard: invalid input or data") == 0);
    invalid_files = xrtValueArray();
    xvalue* empty_item = xrtValueString(XRT_STR_LITERAL(""));
    CHECK(invalid_files != NULL && empty_item != NULL &&
          xrtValueArrayAppendTake(invalid_files, &empty_item));
    CHECK(!x_clipboard_set_files(invalid_files));
    CHECK(strcmp(x_clipboard_last_error(), "clipboard: invalid input or data") == 0);
    xrtValueRelease(invalid_files);

    CHECK(x_clipboard_available());
    CHECK(x_clipboard_clear());
    CHECK(value_equals(x_clipboard_get_text(), ""));
    CHECK(x_clipboard_last_error()[0] != '\0');
    CHECK(x_clipboard_set_text("你好-clipboard"));
    CHECK(x_clipboard_has_text());
    CHECK(value_equals(x_clipboard_get_text(), "你好-clipboard"));
    CHECK(x_clipboard_count_formats() > 0);
    CHECK(value_contains(x_clipboard_formats_json(), "CF_UNICODETEXT"));

    /* UTF-8 转换缓冲分配失败时，不能清空已有剪贴板内容。 */
    CHECK(xrtMemDebugFailAfter(0u));
    int text_written = x_clipboard_set_text("replacement");
    int text_failure_triggered = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    CHECK(!text_written && text_failure_triggered);
    CHECK(strcmp(x_clipboard_last_error(), "clipboard: out of memory") == 0);
    CHECK(value_equals(x_clipboard_get_text(), "你好-clipboard"));
    CHECK(x_clipboard_set_text("你好-clipboard"));

    /* 捕获失败不改变剪贴板；故障解除后仍能取得完整快照。 */
    size_t injected = 0u;
    int completed = 0;
    for (size_t failure = 0u; failure < 32u; ++failure) {
        CHECK(xrtMemDebugFailAfter(failure));
        void* attempt = x_clipboard_snapshot();
        int triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (!triggered) {
            CHECK(attempt != NULL && x_clipboard_snapshot_release(attempt));
            completed = 1;
            break;
        }
        CHECK(attempt == NULL);
        CHECK(strcmp(x_clipboard_last_error(), "clipboard: out of memory") == 0);
        CHECK(value_equals(x_clipboard_get_text(), "你好-clipboard"));
        ++injected;
    }
    CHECK(injected > 0u && completed);

    CHECK(!x_clipboard_restore(NULL));
    void* saved = x_clipboard_snapshot();
    CHECK(saved != NULL);
    CHECK(x_clipboard_set_text("allocation-current"));
    CHECK(xrtMemDebugFailAfter(0u));
    int restore_succeeded = x_clipboard_restore(saved);
    int restore_triggered = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    CHECK(!restore_succeeded && restore_triggered);
    CHECK(strcmp(x_clipboard_last_error(), "clipboard: out of memory") == 0);
    CHECK(value_equals(x_clipboard_get_text(), "allocation-current"));
    CHECK(x_clipboard_restore(saved));
    CHECK(x_clipboard_set_text("temporary"));
    CHECK(x_clipboard_restore(saved));
    CHECK(value_equals(x_clipboard_get_text(), "你好-clipboard"));

    /* 返回值拥有独立文本；工作线程读写剪贴板不会影响它。 */
    xvalue* main_text = x_clipboard_get_text();
    HANDLE worker = CreateThread(NULL, 0, check_thread_state, desktop, 0, NULL);
    DWORD worker_result = 1;
    CHECK(worker != NULL);
    CHECK(WaitForSingleObject(worker, 5000u) == WAIT_OBJECT_0);
    CHECK(GetExitCodeThread(worker, &worker_result) && worker_result == 0);
    CHECK(CloseHandle(worker));
    CHECK(value_equals(main_text, "你好-clipboard"));
    CHECK(x_clipboard_last_error()[0] == '\0');
    CHECK(x_clipboard_restore(saved));
    CHECK(value_equals(x_clipboard_get_text(), "你好-clipboard"));

    CHECK(x_clipboard_set_file("C:\\测试\\file.txt"));
    CHECK(x_clipboard_has_files());
    CHECK(value_equals(x_clipboard_get_file(), "C:\\测试\\file.txt"));
    xvalue* files = x_clipboard_get_files();
    CHECK(files && xrtValueCount(files) == 1);
    xstrview first;
    CHECK(xrtValueGetString(xrtValueArrayGet(files, 0), &first));
    CHECK(first.Size == strlen("C:\\测试\\file.txt"));
    CHECK(memcmp(first.Data, "C:\\测试\\file.txt", first.Size) == 0);
    xrtValueRelease(files);

    int64_t custom = x_clipboard_register_format("xlang.clipboard.test");
    CHECK(custom > 0);
    xvalue* format_name = x_clipboard_format_name(custom);
    xstrview format_text;
    CHECK(format_name && xrtValueGetString(format_name, &format_text));
    CHECK(format_text.Size == strlen("xlang.clipboard.test") &&
          memcmp(format_text.Data, "xlang.clipboard.test", format_text.Size) == 0);
    xrtValueRelease(format_name);
    CHECK(!x_clipboard_is_format_available(custom));

    /* 格式名以 UTF-16 字符注册，转成 UTF-8 后可能超过 256 字节。 */
    char long_name[121u * 3u + 1u];
    for (size_t i = 0u; i < 120u; ++i) {
        memcpy(long_name + i * 3u, "\xe4\xb8\xad", 3u);
    }
    long_name[120u * 3u] = '\0';
    int64_t long_custom = x_clipboard_register_format(long_name);
    CHECK(long_custom > 0);
    xvalue* long_format_name = x_clipboard_format_name(long_custom);
    xstrview long_format_text;
    CHECK(long_format_name && xrtValueGetString(long_format_name, &long_format_text));
    CHECK(long_format_text.Size == strlen(long_name) &&
          memcmp(long_format_text.Data, long_name, long_format_text.Size) == 0);
    xrtValueRelease(long_format_name);

    /* 注册格式按原始字节快照，恢复后仍可由原生 Win32 调用读取。 */
    CHECK(x_clipboard_set_text("snapshot-custom"));
    HGLOBAL custom_data = GlobalAlloc(GMEM_MOVEABLE, 5u);
    CHECK(custom_data != NULL);
    void* custom_bytes = GlobalLock(custom_data);
    CHECK(custom_bytes != NULL);
    memcpy(custom_bytes, "\x00\x7f\x80\xff\x01", 5u);
    SetLastError(NO_ERROR);
    BOOL unlocked = GlobalUnlock(custom_data);
    CHECK(unlocked || GetLastError() == NO_ERROR);
    CHECK(OpenClipboard(NULL));
    CHECK(SetClipboardData((UINT)custom, custom_data) != NULL);
    CHECK(CloseClipboard());

    HGLOBAL long_data = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, 1u);
    CHECK(long_data != NULL);
    CHECK(OpenClipboard(NULL));
    CHECK(SetClipboardData((UINT)long_custom, long_data) != NULL);
    CHECK(CloseClipboard());
    xvalue* format_list = x_clipboard_formats();
    CHECK(format_list != NULL);
    int found_long_name = 0;
    for (size_t i = 0u; i < xrtValueCount(format_list); ++i) {
        xstrview item;
        if (xrtValueGetString(xrtValueArrayGet(format_list, i), &item) &&
            item.Size == strlen(long_name) &&
            memcmp(item.Data, long_name, item.Size) == 0) {
            found_long_name = 1;
            break;
        }
    }
    xrtValueRelease(format_list);
    CHECK(found_long_name);
    CHECK(value_contains(x_clipboard_formats_json(), long_name));
    void* custom_saved = x_clipboard_snapshot();
    CHECK(custom_saved != NULL);
    CHECK(x_clipboard_set_text("overwritten"));
    CHECK(x_clipboard_restore(custom_saved));
    CHECK(x_clipboard_is_format_available(custom));
    CHECK(OpenClipboard(NULL));
    HGLOBAL restored = (HGLOBAL)GetClipboardData((UINT)custom);
    CHECK(restored != NULL && GlobalSize(restored) >= 5u);
    const void* restored_bytes = GlobalLock(restored);
    CHECK(restored_bytes != NULL);
    CHECK(memcmp(restored_bytes, "\x00\x7f\x80\xff\x01", 5u) == 0);
    SetLastError(NO_ERROR);
    unlocked = GlobalUnlock(restored);
    CHECK(unlocked || GetLastError() == NO_ERROR);
    CHECK(CloseClipboard());
    CHECK(value_equals(x_clipboard_get_text(), "snapshot-custom"));
    CHECK(x_clipboard_snapshot_release(custom_saved));

    /* CF_LOCALE 应决定传统 CF_TEXT 的代码页，而不是当前进程代码页。 */
    HGLOBAL ansi_data = GlobalAlloc(GMEM_MOVEABLE, 5u);
    HGLOBAL locale_data = GlobalAlloc(GMEM_MOVEABLE, sizeof(LCID));
    CHECK(ansi_data != NULL && locale_data != NULL);
    void* ansi_bytes = GlobalLock(ansi_data);
    void* locale_bytes = GlobalLock(locale_data);
    CHECK(ansi_bytes != NULL && locale_bytes != NULL);
    memcpy(ansi_bytes, "caf\xE9", 5u);
    *(LCID*)locale_bytes = 0x0409u;
    GlobalUnlock(ansi_data);
    GlobalUnlock(locale_data);
    CHECK(OpenClipboard(NULL));
    CHECK(EmptyClipboard());
    CHECK(SetClipboardData(CF_TEXT, ansi_data) != NULL);
    CHECK(SetClipboardData(CF_LOCALE, locale_data) != NULL);
    CHECK(CloseClipboard());
    CHECK(value_equals(x_clipboard_get_text(), "caf\xC3\xA9"));
    CHECK(x_clipboard_restore(saved));

    xvalue* paths = xrtValueArray();
    CHECK(paths != NULL);
    xvalue* second = xrtValueString(XRT_STR_LITERAL("C:\\测试\\two.txt"));
    CHECK(second && xrtValueArrayAppendTake(paths, &second));
    CHECK(x_clipboard_set_files(paths));
    xrtValueRelease(paths);
    files = x_clipboard_get_files();
    CHECK(files && xrtValueCount(files) == 1);
    CHECK(value_equals(x_clipboard_get_file(), "C:\\测试\\two.txt"));
    xrtValueRelease(files);

    CHECK(!x_clipboard_set_text("\xC0\xAF"));
    CHECK(value_equals(x_clipboard_get_file(), "C:\\测试\\two.txt"));

    LOGPALETTE palette = { 0x300, 1, {{ 255, 0, 0, 0 }} };
    HPALETTE handle = CreatePalette(&palette);
    CHECK(handle != NULL);
    CHECK(OpenClipboard(NULL));
    CHECK(EmptyClipboard());
    CHECK(SetClipboardData(CF_PALETTE, handle) != NULL);
    CHECK(CloseClipboard());
    CHECK(!x_clipboard_snapshot());
    CHECK(IsClipboardFormatAvailable(CF_PALETTE));
    CHECK(x_clipboard_restore(saved));
    CHECK(value_equals(x_clipboard_get_text(), "你好-clipboard"));

    /* BMP 文件及导出都使用测试目录里的中文路径。 */
    unsigned char bmp[58] = {0};
    bmp[0] = 'B'; bmp[1] = 'M';
    write_u32(bmp + 2, sizeof(bmp));
    write_u32(bmp + 10, 54);
    write_u32(bmp + 14, 40);
    write_u32(bmp + 18, 1);
    write_u32(bmp + 22, 1);
    bmp[26] = 1;
    bmp[28] = 24;
    write_u32(bmp + 34, 4);
    bmp[54] = 0x12; bmp[55] = 0x34; bmp[56] = 0x56;
    char inputPath[512];
    char outputPath[512];
    char missingPath[512];
    wchar_t input[512];
    wchar_t output[512];
    int pathLength = snprintf(inputPath, sizeof(inputPath), "%s/图像.bmp", argv[1]);
    CHECK(pathLength > 0 && (size_t)pathLength < sizeof(inputPath));
    pathLength = snprintf(outputPath, sizeof(outputPath), "%s/图像导出.bmp", argv[1]);
    CHECK(pathLength > 0 && (size_t)pathLength < sizeof(outputPath));
    pathLength = snprintf(missingPath, sizeof(missingPath), "%s/missing/image.bmp", argv[1]);
    CHECK(pathLength > 0 && (size_t)pathLength < sizeof(missingPath));
    CHECK(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, inputPath, -1,
                              input, (int)(sizeof(input) / sizeof(input[0]))) > 0);
    CHECK(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, outputPath, -1,
                              output, (int)(sizeof(output) / sizeof(output[0]))) > 0);
    CHECK(write_file(input, bmp, sizeof(bmp)));
    CHECK(x_clipboard_set_image(inputPath));
    CHECK(x_clipboard_has_image());
    CHECK(value_contains(x_clipboard_image_info_json(), "\"width\":1"));
    CHECK(write_file(output, "KEEP", 4u));
    CHECK(xrtMemDebugFailAfter(0u));
    int saved_image = x_clipboard_save_image(outputPath);
    int image_oom = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    CHECK(!saved_image && image_oom);
    CHECK(strcmp(x_clipboard_last_error(), "clipboard: out of memory") == 0);
    CHECK(file_equals(output, "KEEP", 4u));
    CHECK(x_clipboard_save_image(outputPath));
    CHECK(same_bmp_pixels(output, bmp));
    CHECK(!x_clipboard_set_image(missingPath));
    CHECK(strcmp(x_clipboard_last_error(), "clipboard: data is not available") == 0);
    CHECK(x_clipboard_has_image());
    write_u32(bmp + 18, 0);
    CHECK(write_file(input, bmp, sizeof(bmp)));
    CHECK(!x_clipboard_set_image(inputPath));
    CHECK(x_clipboard_has_image());
    write_u32(bmp + 18, 0x7fffffff);
    write_u32(bmp + 22, 0x7fffffff);
    CHECK(write_file(input, bmp, sizeof(bmp)));
    CHECK(!x_clipboard_set_image(inputPath));
    CHECK(x_clipboard_has_image());
    CHECK(DeleteFileW(input));
    CHECK(DeleteFileW(output));
    CHECK(x_clipboard_snapshot_release(saved));
    puts("clipboard isolated C: PASS");
    return 0;
}
