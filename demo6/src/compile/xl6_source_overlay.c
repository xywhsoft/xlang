#include "src/compile/xl6_source_overlay.h"
#include "src/common/xl6_path_identity.h"

#ifdef _WIN32
#include <windows.h>
#endif

static int xl6SourceOverlayUtf8(const char* text, size_t size)
{
    size_t i = 0;
    while (i < size) {
        unsigned char a = (unsigned char)text[i++];
        unsigned int value, minimum;
        size_t extra;
        if (a == 0) return 0;
        if (a < 0x80) continue;
        if (a >= 0xc2 && a <= 0xdf) { value = a & 31; minimum = 0x80; extra = 1; }
        else if (a >= 0xe0 && a <= 0xef) { value = a & 15; minimum = 0x800; extra = 2; }
        else if (a >= 0xf0 && a <= 0xf4) { value = a & 7; minimum = 0x10000; extra = 3; }
        else return 0;
        if (extra > size - i) return 0;
        while (extra--) {
            unsigned char b = (unsigned char)text[i++];
            if ((b & 0xc0) != 0x80) return 0;
            value = (value << 6) | (b & 63);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return 0;
    }
    return 1;
}

static int xl6SourceOverlayIndex(const XL6_SourceOverlay* overlay, const char* absolute, size_t* index)
{
    size_t i;
    for (i = 0; overlay && i < overlay->count; ++i) {
        int equal = xl6PathIdentityEqual(overlay->items[i].path, absolute);
        if (equal < 0) return 0;
        if (equal) { *index = i; return 1; }
    }
    *index = overlay ? overlay->count : 0;
    return 1;
}

int xl6SourceOverlaySet(XL6_SourceOverlay* overlay, const char* path, const char* text, size_t size)
{
    char* absolute;
    char* copy;
    size_t index, oldSize;
    if (!overlay || !path || !*path || !text || size > XL6_SOURCE_OVERLAY_BYTES ||
        !xl6SourceOverlayUtf8(text, size)) return 0;
    absolute = xrtPathAbs(path);
    if (!absolute) return 0;
    if (!xl6SourceOverlayIndex(overlay, absolute, &index)) { xrtFree(absolute); return 0; }
    oldSize = index < overlay->count ? overlay->items[index].size : 0;
    if ((index == overlay->count && overlay->count == XL6_SOURCE_OVERLAY_FILES) ||
        size > XL6_SOURCE_OVERLAY_TOTAL - (overlay->bytes - oldSize)) {
        xrtFree(absolute); return 0;
    }
    copy = xrtStrDupN(text, size);
    if (!copy) { xrtFree(absolute); return 0; }
    if (index < overlay->count) {
        xrtFree(overlay->items[index].path);
        xrtFree(overlay->items[index].text);
    } else overlay->count++;
    overlay->items[index].path = absolute;
    overlay->items[index].text = copy;
    overlay->items[index].size = size;
    overlay->bytes = overlay->bytes - oldSize + size;
    return 1;
}

int xl6SourceOverlayRemove(XL6_SourceOverlay* overlay, const char* path)
{
    char* absolute;
    size_t index;
    if (!overlay || !path || !*path) return 0;
    absolute = xrtPathAbs(path);
    if (!absolute) return 0;
    if (!xl6SourceOverlayIndex(overlay, absolute, &index)) { xrtFree(absolute); return 0; }
    xrtFree(absolute);
    if (index == overlay->count) return 1;
    overlay->bytes -= overlay->items[index].size;
    xrtFree(overlay->items[index].path);
    xrtFree(overlay->items[index].text);
    memmove(&overlay->items[index], &overlay->items[index + 1],
            (overlay->count - index - 1) * sizeof(overlay->items[0]));
    memset(&overlay->items[--overlay->count], 0, sizeof(overlay->items[0]));
    return 1;
}

void xl6SourceOverlayClear(XL6_SourceOverlay* overlay)
{
    size_t i;
    if (!overlay) return;
    for (i = 0; i < overlay->count; ++i) {
        xrtFree(overlay->items[i].path);
        xrtFree(overlay->items[i].text);
    }
    memset(overlay, 0, sizeof(*overlay));
}

int xl6SourceOverlayLookup(const XL6_SourceOverlay* overlay, const char* path,
                          const XL6_SourceOverlayItem** item)
{
    char* absolute;
    size_t index;
    if (!item) return -1;
    *item = NULL;
    if (overlay && overlay->report && !overlay->report->complete) return -1;
    if (!path || !*path) goto failed;
    if (!overlay || !overlay->count) return 0;
    absolute = xrtPathAbs(path);
    if (!absolute) goto failed;
    if (!xl6SourceOverlayIndex(overlay, absolute, &index)) { xrtFree(absolute); goto failed; }
    xrtFree(absolute);
    if (index == overlay->count) return 0;
    *item = &overlay->items[index];
    return 1;
failed:
    xl6SourceReportInvalidate(overlay ? overlay->report : NULL, path, "cannot resolve source overlay identity");
    return -1;
}

int xl6SourceOverlayExists(const XL6_SourceOverlay* overlay, const char* path)
{
    const XL6_SourceOverlayItem* item;
    XL6_SourceReport* report = overlay ? overlay->report : NULL;
    int status = xl6SourceOverlayLookup(overlay, path, &item);
    if (status != 0) return status;
    status = xl6SourceReportExists(report, path);
    return report && !report->complete ? -1 : status;
}

int xl6SourceOverlayParse(const XL6_SourceOverlay* overlay, const char* path, XL6_ParseResult* result)
{
    const XL6_SourceOverlayItem* item;
    XL6_SourceReport* report = overlay ? overlay->report : NULL;
    char* text;
    size_t size;
    int ok;
    if (xl6SourceOverlayLookup(overlay, path, &item) < 0) {
        xl6DiagAddFile(&result->diags, path, 1, 1, "cannot resolve source overlay identity");
        return 0;
    }
    if (item) {
        if (!xl6SourceReportText(report, item->path, item->text, item->size, "overlay")) {
            xl6DiagAddFile(&result->diags, path, 1, 1, "cannot retain source overlay report");
            return 0;
        }
        return xl6ParseSourceWithCompletion(item->text, item->path, result, overlay ? overlay->completionQuery : NULL);
    }
    if (!report) return xl6ParseFile(path, result);
    text = xl6SourceReportReadText(report, path, XL6_SOURCE_REPORT_SOURCE, &size);
    if (!text) {
        xl6DiagAddFile(&result->diags, path, 1, 1, "cannot read source report snapshot");
        return 0;
    }
    ok = xl6ParseSourceWithCompletion(text, path, result, overlay ? overlay->completionQuery : NULL);
    xrtFree(text);
    return ok;
}
