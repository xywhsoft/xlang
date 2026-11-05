#ifndef XL6_SOURCE_OVERLAY_H
#define XL6_SOURCE_OVERLAY_H

#include "src/parser/xl6_parser_api.h"
#include "src/compile/xl6_source_report.h"

#define XL6_SOURCE_OVERLAY_FILES 64
#define XL6_SOURCE_OVERLAY_BYTES (8u * 1024u * 1024u)
#define XL6_SOURCE_OVERLAY_TOTAL (32u * 1024u * 1024u)

typedef struct XL6_SourceOverlayItem {
    char* path;
    char* text;
    size_t size;
} XL6_SourceOverlayItem;

typedef struct XL6_SourceOverlay {
    XL6_SourceOverlayItem items[XL6_SOURCE_OVERLAY_FILES];
    size_t count;
    size_t bytes;
    XL6_SourceReport* report; /* Borrowed for one observed check, otherwise NULL. */
    struct XL6_CompletionQuery* completionQuery; /* Borrowed for one query. */
    int semanticIndex; /* Internal opt-in for the same observed compilation. */
} XL6_SourceOverlay;

/* Relative paths resolve at the call's current working directory. Mutations
 * copy all input, are atomic on failure, and never write source files. */
int xl6SourceOverlaySet(XL6_SourceOverlay* overlay, const char* path,
                       const char* text, size_t size);
int xl6SourceOverlayRemove(XL6_SourceOverlay* overlay, const char* path);
void xl6SourceOverlayClear(XL6_SourceOverlay* overlay);
/* 1 found, 0 absent, -1 failed. Only absence permits a disk fallback.
 * On failure *item is NULL and an attached source report is invalidated. */
int xl6SourceOverlayLookup(const XL6_SourceOverlay* overlay, const char* path,
                          const XL6_SourceOverlayItem** item);
/* The same three outcomes, including failures of an attached source report. */
int xl6SourceOverlayExists(const XL6_SourceOverlay* overlay, const char* path);
int xl6SourceOverlayParse(const XL6_SourceOverlay* overlay, const char* path,
                         XL6_ParseResult* result);

#endif
