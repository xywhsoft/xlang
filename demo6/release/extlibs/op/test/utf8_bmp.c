#include <stdio.h>
#include <string.h>

#include "../op_native.c"

int main(void)
{
    const char *path = "build/test/op/\xe6\xb5\x8b\xe8\xaf\x95.bmp";
    const char *bad = "build/test/op/\xe5\x9d\x8f\xe5\x9b\xbe.bmp";
    const BYTE pixels[16] = {
        1, 2, 3, 255, 4, 5, 6, 255,
        7, 8, 9, 255, 10, 11, 12, 255
    };
    op_bitmap_t image = {0};
    BYTE *file;
    size_t size;
    BITMAPFILEHEADER header;
    BITMAPINFOHEADER info;
    int ok = 0;

    if (!op_save_bmp(path, (BYTE *)pixels, 2, 2)) goto done;
    if (!op_load_bmp(path, &image)) goto done;
    if (image.w != 2 || image.h != 2 || memcmp(image.data, pixels, 16) != 0)
        goto done;
    free(image.data);
    image.data = NULL;

    /* 相似度为 0 时，固定 2x2 模板可验证三种搜索结果与所有权。 */
    {
        void *context = xop_create();
        void *one = NULL, *many = NULL, *indexed = NULL;
        int matched = 0;
        if (!context) goto done;
        if (xop_bind_window(context, (long long)(intptr_t)GetDesktopWindow(),
                            "normal", "normal", "normal", 0)) {
            one = xop_find_pic(context, 0, 0, 1, 1, path, "", 0.0, 0);
            many = xop_find_pic_ex(context, 0, 0, 1, 1, path, "", 0.0, 0);
            indexed = xop_find_pic_n(context, 0, 0, 1, 1, path, "", 0.0, 0);
            matched = one && many && indexed &&
                      strcmp(one, "0,0") == 0 &&
                      strcmp(many, "0,0") == 0 &&
                      strcmp(indexed, "0,0,0") == 0 &&
                      strcmp(one, "0,0") == 0;
        }
        xrtFree(one);
        xrtFree(many);
        xrtFree(indexed);
        xop_destroy(context);
        if (!matched) goto done;
    }

    file = xrtFileReadAll(path, &size);
    if (!file || size != sizeof(header) + sizeof(info) + sizeof(pixels)) {
        xrtFree(file);
        goto done;
    }
    memcpy(&info, file + sizeof(header), sizeof(info));
    info.biWidth = -1;
    memcpy(file + sizeof(header), &info, sizeof(info));
    if (!xrtFileWriteAll(bad, (xbytesview){file, size}) ||
        op_load_bmp(bad, &image)) goto free_file;

    memcpy(&info, file + sizeof(header), sizeof(info));
    info.biWidth = 2;
    memcpy(file + sizeof(header), &info, sizeof(info));
    memcpy(&header, file, sizeof(header));
    header.bfOffBits = (DWORD)(size + 1);
    memcpy(file, &header, sizeof(header));
    if (!xrtFileWriteAll(bad, (xbytesview){file, size}) ||
        op_load_bmp(bad, &image)) goto free_file;

    header.bfOffBits = sizeof(header) + sizeof(info);
    memcpy(file, &header, sizeof(header));
    if (!xrtFileWriteAll(bad, (xbytesview){file, size - 1}) ||
        op_load_bmp(bad, &image)) goto free_file;
    ok = 1;

free_file:
    xrtFree(file);
done:
    free(image.data);
    xrtFileDelete(path);
    xrtFileDelete(bad);
    if (!ok) return 1;
    puts("op utf8 bmp: ok");
    return 0;
}
