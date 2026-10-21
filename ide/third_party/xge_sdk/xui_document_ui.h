#ifndef XUI_DOCUMENT_UI_H
#define XUI_DOCUMENT_UI_H
#include "xui_document.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xui_document_renderer_t* xui_document_renderer;
enum xui_doc_display_mode { XUI_DOC_VISUAL = 1, XUI_DOC_SOURCE_TEXT = 2, XUI_DOC_LIVE_MARKDOWN = 3 };
typedef struct xui_doc_rect_t { double x, y, width, height; } xui_doc_rect_t;
typedef struct xui_doc_font_set_t {
    xui_font normal, bold, italic, boldItalic, monospace;
} xui_doc_font_set_t;
typedef xui_font (*xui_doc_font_proc)(xui_context, const char* family, uint32_t marks, float size, void* user);
typedef int (*xui_doc_object_measure_proc)(xui_document_snapshot, xui_doc_node_id, float width, float zoom,
    xui_vec2_t* size, float* baseline, void* user);
typedef int (*xui_doc_object_draw_proc)(xui_document_snapshot, xui_doc_node_id, xui_proxy, xui_draw_context,
    xui_rect_t bounds, void* user);
typedef struct xui_doc_renderer_desc_t {
    uint32_t iSize;
    xui_doc_font_set_t tFonts;
    float fZoom;
    float fParagraphGap;
    float fLineGap;
    float fIndent;
    uint32_t iTextColor;
    uint32_t iBorderColor;
    uint32_t iCodeBackground;
    uint32_t iHighlightColor; /* Fallback for highlighted runs without an explicit background. */
    uint32_t iLinkColor; /* Fallback for link runs without an explicit text color. */
    uint32_t iQuoteBorderColor;
    uint32_t iRuleColor;
    uint32_t iParagraphBackgroundColor;
    uint32_t iTableBorderColor;
    uint32_t iTableHeaderColor;
    uint32_t iTableCellColor;
    uint32_t iImagePlaceholderColor;
    uint32_t iImageBorderColor;
    uint32_t iImageTextColor;
    xui_doc_font_proc onFont;
    xui_doc_object_measure_proc onObjectMeasure;
    xui_doc_object_draw_proc onObjectDraw;
    void* pUser;
    uint64_t iLayoutCacheBudgetBytes; /* Zero selects 32 MiB. All visible blocks are retained even when their sum exceeds it. */
    uint32_t bSourceLineHeightMayVary; /* Printable ASCII may shape taller than font metrics; resolve offscreen SOURCE/LIVE rows and shape long lines in full. */
} xui_doc_renderer_desc_t;
typedef struct xui_doc_renderer_stats_t {
    uint32_t iSize;
    uint64_t iBlocks;
    uint64_t iMeasuredBlocks;
    uint64_t iShapedBytes;
    uint64_t iFragments;
    uint64_t iLayoutPasses;
    uint64_t iSourceBlocks;
    int bLiveSourceFallback;
    uint64_t iSourceBytesScanned; /* Cumulative bytes read to build successful source directories. */
    uint64_t iSourceIncrementalUpdates;
    uint64_t iSourceReusedBlocks; /* Cumulative unchanged line blocks kept by incremental updates. */
    uint64_t iSourceRowsCreated; /* Cumulative SOURCE directory rows allocated during successful builds/updates. */
    uint64_t iLayoutCacheBytes; /* Current block-owned text, shapes and geometry; excludes directories and fonts. */
    uint64_t iLayoutCacheBudgetBytes;
    uint64_t iLayoutCacheEvictions;
    uint64_t iDrawFragmentsExamined; /* Cumulative fragments visited by Draw after line culling. */
    uint64_t iHitFragmentsExamined; /* Cumulative fragments visited by HitTest. */
    uint64_t iCaretFragmentsExamined; /* Cumulative fragments visited by GetCaretRect. */
    uint64_t iTextRunBytes; /* Cumulative successful new run input bytes, including private break measuring text. Joint/line reshaping and cached-run reflow do not add to this counter. */
} xui_doc_renderer_stats_t;

/* The host authorizes a path or supplies encoded bytes; a Markdown URI never
 * triggers file access by itself. Successful loads own their decoded surface
 * through the XUI resource registry. A failed replacement keeps the old
 * resource. Zero limits use 16 MiB encoded / 16 million decoded pixels.
 * Decode/upload and registry mutation run on the XUI owner thread. */
typedef struct xui_doc_image_load_limits_t {
    uint32_t iSize;
    uint64_t iMaxEncodedBytes;
    uint64_t iMaxPixels;
} xui_doc_image_load_limits_t;
/* Decode host-supplied image bytes into a named XUI resource. */
#if XUI_ENABLE_DOCUMENT_VIEW
XUI_API int xuiDocumentImageResourceLoadMemory(xui_context context, const char* resource_name,
    const void* encoded, uint64_t encoded_bytes, const xui_doc_image_load_limits_t* limits,
    xui_resource* out);
/* Load a host-authorized image file into a named XUI resource. */
XUI_API int xuiDocumentImageResourceLoadFile(xui_context context, const char* resource_name,
    const char* authorized_path, const xui_doc_image_load_limits_t* limits, xui_resource* out);
#endif
/* XGE decodes the explicitly authorized local file on a worker. Requests are
 * queued per context: at most 4 workers, 64 outstanding requests and 64 MiB
 * copied encoded input. Poll reaps finished workers and publishes all ready
 * requests on the XUI owner thread, returning BUSY until its request is terminal.
 * Starting another request for the same context/name cancels the older one;
 * a resource changed outside the request makes its result STALE. Release may
 * wait for a running decoder and must precede context destruction. */
typedef struct xui_doc_image_request_t* xui_doc_image_request;
/* Queue decoding of a host-authorized image file. */
#if XUI_ENABLE_DOCUMENT_VIEW
XUI_API int xuiDocumentImageResourceLoadFileAsync(xui_context context, const char* resource_name,
    const char* authorized_path, const xui_doc_image_load_limits_t* limits, xui_doc_image_request* out);
/* Copies encoded bytes before returning; callers may release their input. */
XUI_API int xuiDocumentImageResourceLoadMemoryAsync(xui_context context, const char* resource_name,
    const void* encoded, uint64_t encoded_bytes, const xui_doc_image_load_limits_t* limits,
    xui_doc_image_request* out);
/* Publish a completed image request on the XUI owner thread. */
XUI_API int xuiDocumentImageResourcePoll(xui_doc_image_request request, xui_resource* out);
/* Cancel an outstanding image request. */
XUI_API int xuiDocumentImageResourceCancel(xui_doc_image_request request);
/* Release an image request and join its worker if needed. */
XUI_API void xuiDocumentImageResourceRelease(xui_doc_image_request request);
#endif
typedef struct xui_doc_image_async_stats_t {
    uint32_t iSize;
    uint32_t iRunning, iQueued, iOutstanding;
    uint64_t iCopiedBytes;
} xui_doc_image_async_stats_t;
/* Read-only admission diagnostics. Per context: at most 4 workers, 64
 * outstanding requests and 64 MiB of copied encoded input. */
#if XUI_ENABLE_DOCUMENT_VIEW
XUI_API int xuiDocumentImageResourceGetAsyncStats(xui_context context, xui_doc_image_async_stats_t* stats);

/* The renderer borrows its context/fonts and retains its immutable snapshot.
 * It owns no widget, document writer, selection history, or window. */
XUI_API int xuiDocumentRendererCreate(xui_context context, const xui_doc_renderer_desc_t* desc, xui_document_renderer* out);
/* Release renderer layout and its retained snapshot. */
XUI_API void xuiDocumentRendererRelease(xui_document_renderer renderer);
/* Bind a committed snapshot and optional change set. */
XUI_API int xuiDocumentRendererSetSnapshot(xui_document_renderer renderer, xui_document_snapshot snapshot, xui_document_change_set changes);
/* SOURCE_TEXT / LIVE_MARKDOWN input projection over the retained committed snapshot. Candidate
 * identity/base revision must match. Source hit tests carry iInputGeneration;
 * untouched inactive LIVE semantic positions map into candidate source. NULL
 * restores the snapshot.
 * LIVE keeps inactive blocks on the committed semantic view when the edit stays
 * inside its active source block; uncertain/cross-block edits display candidate
 * source until publication. Snapshot publication clears the projection. This
 * never parses or commits. */
XUI_API int xuiDocumentRendererSetSourceInput(xui_document_renderer renderer, xui_document_prepare prepare);
/* Select visual, source or live Markdown rendering mode. */
XUI_API int xuiDocumentRendererSetMode(xui_document_renderer renderer, uint32_t mode);
/* Re-measure objects after a custom provider changes its size. Named surface
 * Set/Remove/Touch is observed automatically through the XUI registry epoch.
 * This does not alter Document. */
XUI_API int xuiDocumentRendererInvalidateObjects(xui_document_renderer renderer);
/* Call when a borrowed font or onFont selection changes metrics without a
 * default-font pointer, DPI, or width change. Rebuilds layout estimates. */
XUI_API int xuiDocumentRendererInvalidateFonts(xui_document_renderer renderer);
/* LIVE_MARKDOWN exposes the active top-level Markdown container as source.
 * Hit tests/selections in this mode use SOURCE positions. Unknown source
 * boundaries conservatively expose the whole source instead of hiding bytes. */
XUI_API int xuiDocumentRendererSetActivePosition(xui_document_renderer renderer, const xui_doc_position_t* position);
/* Read the source span exposed in live Markdown mode. */
XUI_API int xuiDocumentRendererGetActiveSourceRange(xui_document_renderer renderer, uint64_t* start, uint64_t* end);
/* A zero-height viewport updates the width and returns estimated size without
 * materializing blocks. GetSize reports whether the result is exact. */
XUI_API int xuiDocumentRendererLayout(xui_document_renderer renderer, double width, double viewport_top, double viewport_height);
/* Read measured or estimated document size. */
XUI_API int xuiDocumentRendererGetSize(xui_document_renderer renderer, xui_doc_rect_t* size, int* exact);
/* Map document-space coordinates to a Document position. */
XUI_API int xuiDocumentRendererHitTest(xui_document_renderer renderer, double x, double y, xui_doc_position_t* position);
/* Returns a task ListItem only when the point is inside its drawn [ ]/[x]
 * marker. Coordinates are in the renderer's document space. */
XUI_API int xuiDocumentRendererHitTaskMarker(xui_document_renderer renderer,
    double x, double y, xui_doc_node_id* item);
#endif
typedef struct xui_doc_cell_hit_t {
    uint32_t iSize;
    xui_doc_node_id iTableId, iCellId;
    uint32_t iRow, iColumn, iRowSpan, iColumnSpan;
    xui_doc_rect_t tBounds; /* Renderer: document coordinates; View: widget-local coordinates. */
} xui_doc_cell_hit_t;
typedef struct xui_doc_table_selection_t {
    uint32_t iSize;
    xui_doc_node_id iTableId;
    uint32_t iRow, iColumn, iRows, iColumns;
} xui_doc_table_selection_t;
/* Cell hit testing includes its padding. Nested tables return the innermost cell. */
#if XUI_ENABLE_DOCUMENT_VIEW
XUI_API int xuiDocumentRendererHitTestCell(xui_document_renderer renderer, double x, double y, xui_doc_cell_hit_t* out);
/* Read a table cell's document-space rectangle. */
XUI_API int xuiDocumentRendererGetCellRect(xui_document_renderer renderer, xui_doc_node_id cell, xui_doc_cell_hit_t* out);
/* Materializes only the owning visual block. Rect is in document coordinates;
 * offscreen Y may use estimated heights for preceding unmeasured blocks. */
XUI_API int xuiDocumentRendererGetNodeRect(xui_document_renderer renderer, xui_doc_node_id node, xui_doc_rect_t* out);
/* Read a position's document-space caret rectangle. */
XUI_API int xuiDocumentRendererGetCaretRect(xui_document_renderer renderer, const xui_doc_position_t* position, xui_doc_rect_t* rect);
/* Logical content of the visual row containing position, using caret affinity
 * at wraps. Includes explicit breaks (and SOURCE row terminators), but excludes
 * synthetic plaintext paragraph/cell separators. Parallel table cells remain
 * separate rows. Output uses the renderer's position domain/generation and is
 * unchanged on failure. Only layout needed to locate the row is prepared;
 * querying a lazy horizontal row does not shape its unseen suffix. */
XUI_API int xuiDocumentRendererGetLineRange(xui_document_renderer renderer,
    const xui_doc_position_t* position, xui_doc_range_t* range);
/* Selection geometry in document coordinates, in document order. Adjacent
 * fragments on one visual row are merged; grapheme clusters and objects are
 * indivisible, matching Draw. Materializes the range's blocks, including
 * offscreen ones. Earlier unmeasured blocks may still give estimated Y.
 * Pass rects=NULL, capacity=0 to query total; a smaller buffer receives the
 * first capacity rectangles while total reports the required count. */
XUI_API int xuiDocumentRendererGetRangeRects(xui_document_renderer renderer,
    const xui_doc_range_t* range, xui_doc_rect_t* rects,
    uint64_t capacity, uint64_t* total);
/* Draw visible content, selection and objects in the supplied clip. */
XUI_API int xuiDocumentRendererDraw(xui_document_renderer renderer, xui_draw_context draw, double origin_x, double origin_y,
    xui_rect_t clip, const xui_doc_range_t* selection, uint32_t selection_color);
/* Read cumulative renderer layout diagnostics. */
XUI_API int xuiDocumentRendererGetStats(xui_document_renderer renderer, xui_doc_renderer_stats_t* stats);
#endif

/* Optional browser-backed static math/Mermaid provider. It owns one generic
 * WebView worker attached to parent and caches PNG surfaces by document,
 * revision, node, width, zoom and palette generation. Call Update after XUI Update on the UI
 * thread; onInvalidate should invalidate affected Document views/renderers.
 * Release before destroying the parent/context. The worker uses a one-pixel
 * visible host slot to keep WebView2 rendering while capturing full viewport. */
typedef struct xui_doc_web_provider_t* xui_doc_web_provider;
typedef void (*xui_doc_web_invalidate_proc)(void* user);
typedef struct xui_doc_web_provider_desc_t {
    uint32_t iSize;
    xui_context pContext;
    xui_widget pParent;
    const char* sAssetsFolder; /* Existing absolute path to res/xui/document_web. */
    const char* sUserDataFolder; /* Optional UTF-8 WebView profile path, copied at create; NULL=runtime default. */
    xui_doc_web_invalidate_proc onInvalidate;
    void* pUser;
    uint32_t iTimeoutMs; /* 0=10000 ms; bounds startup/render/paint/capture waits. */
} xui_doc_web_provider_desc_t;
/* Create the optional browser-backed static object provider. */
#if XUI_ENABLE_WEBVIEW
XUI_API int xuiDocumentWebProviderCreate(const xui_doc_web_provider_desc_t* desc,
    xui_doc_web_provider* out);
/* Release the provider and its WebView worker. */
XUI_API void xuiDocumentWebProviderRelease(xui_doc_web_provider provider);
/* Worker failure is a nonfatal display state. Browser-process loss schedules
 * up to three worker restarts; other failures remain visible as error cards.
 * Call Update after XUI Update so due restarts and pending renders advance. */
XUI_API int xuiDocumentWebProviderUpdate(xui_doc_web_provider provider);
/* Opaque RGBA colors; changing either discards old-theme results and
 * invalidates bound views through onInvalidate. Defaults to #111 on white. */
XUI_API int xuiDocumentWebProviderSetPalette(xui_doc_web_provider provider,
    uint32_t foreground, uint32_t background);
#endif
typedef struct xui_doc_web_provider_stats_t {
    uint32_t iSize;
    uint32_t iQueued, iReady, iFailed, iDrawn;
    uint64_t iDrawCalls, iCacheBytes, iPaletteGeneration;
    uint32_t bWorkerFailed;
    char sWorkerError[128];
    uint32_t bRestartPending, bRecovering, iRestartAttempts;
} xui_doc_web_provider_stats_t;
/* Read queue, cache, worker-failure and bounded-restart diagnostics. */
#if XUI_ENABLE_WEBVIEW
XUI_API int xuiDocumentWebProviderGetStats(xui_doc_web_provider provider,
    xui_doc_web_provider_stats_t* out);
/* Measure a browser-backed formula or diagram object. */
XUI_API int xuiDocumentWebObjectMeasure(xui_document_snapshot snapshot,
    xui_doc_node_id node, float width, float zoom, xui_vec2_t* size,
    float* baseline, void* provider);
/* Draw a cached browser-backed object surface. */
XUI_API int xuiDocumentWebObjectDraw(xui_document_snapshot snapshot,
    xui_doc_node_id node, xui_proxy proxy, xui_draw_context draw,
    xui_rect_t bounds, void* provider);
#endif

/* A separate browser interaction panel for one HTML node in a Rich or
 * Markdown Document. It owns one XUI Window and one generic WebView. Source
 * runs in a sandboxed iframe with browser form controls but no scripts, form
 * submission or top navigation. DOM input stays in the browser and never
 * mutates Document history. Create requires a root widget and nonzero input
 * viewport. The caller edits source through Document transactions and calls
 * Update on the UI thread each frame after layout, including while focus is
 * pending.
 * Unrelated revisions do not reset browser form state. Release before the
 * context is destroyed. Only Windows WebView2 builds create a live panel. */
typedef struct xui_doc_html_interaction_t* xui_doc_html_interaction;
/* Called on the UI thread for HTTP(S) links in the HTML node. The URL is
 * valid only during this call. Without a callback, external navigation is
 * suppressed; the host chooses whether/how to open it. */
typedef void (*xui_doc_html_open_link_proc)(const char* url, void* user);
typedef struct xui_doc_html_interaction_desc_t {
    uint32_t iSize;
    xui_context pContext;
    xui_document pDocument;
    xui_doc_node_id iNodeId;
    const char* sTitle; /* NULL selects a default title. */
    xui_rect_t tBounds; /* Nonpositive width/height select a default panel. */
    xui_doc_html_open_link_proc onOpenLink;
    void* pUser;
} xui_doc_html_interaction_desc_t;
/* Create a separate browser panel for one HTML node. */
#if XUI_ENABLE_WEBVIEW
XUI_API int xuiDocumentHtmlInteractionCreate(
    const xui_doc_html_interaction_desc_t* desc, xui_doc_html_interaction* out);
/* Refresh the HTML panel after layout or document changes. */
XUI_API int xuiDocumentHtmlInteractionUpdate(xui_doc_html_interaction interaction);
/* Show the HTML interaction panel. */
XUI_API int xuiDocumentHtmlInteractionShow(xui_doc_html_interaction interaction);
/* Return the panel's owned XUI window widget. */
XUI_API xui_widget xuiDocumentHtmlInteractionGetWindow(xui_doc_html_interaction interaction);
/* Return the panel's owned WebView widget. */
XUI_API xui_widget xuiDocumentHtmlInteractionGetWebView(xui_doc_html_interaction interaction);
/* Release the HTML panel before its context is destroyed. */
XUI_API void xuiDocumentHtmlInteractionRelease(xui_doc_html_interaction interaction);
#endif

typedef void (*xui_doc_view_event_proc)(xui_widget view, xui_doc_node_id node, const char* resource, void* user);
typedef struct xui_doc_view_desc_t {
    uint32_t iSize;
    xui_document pDocument;
    xui_doc_renderer_desc_t tRenderer;
    int bAutoHeight;
    int bDisableSelection;
    uint32_t iSelectionColor;
    xui_doc_view_event_proc onActivate;
    void* pUser;
    uint32_t iBackgroundColor; /* Widget background; zero is transparent. */
} xui_doc_view_desc_t;
/* Get the registered DocumentView widget type. */
#if XUI_ENABLE_DOCUMENT_VIEW
XUI_API xui_widget_type xuiDocumentViewGetType(xui_context context);
/* Create a DocumentView bound to its configured Document. */
XUI_API int xuiDocumentViewCreate(xui_context context, const xui_doc_view_desc_t* desc, xui_widget* out);
/* Bind a Document and refresh view state. */
XUI_API int xuiDocumentViewSetDocument(xui_widget view, xui_document document);
/* Return the currently bound Document handle. */
XUI_API xui_document xuiDocumentViewGetDocument(xui_widget view);
/* Set the view's Document selection range. */
XUI_API int xuiDocumentViewSetSelection(xui_widget view, const xui_doc_range_t* range);
/* Read the view's Document selection range. */
XUI_API int xuiDocumentViewGetSelection(xui_widget view, xui_doc_range_t* range);
/* Select an image, formula, Mermaid, HTML or extension node as one structural
 * range in VISUAL mode. Clicks on those objects use the same range. */
XUI_API int xuiDocumentViewSelectObject(xui_widget view, xui_doc_node_id node);
/* Set horizontal and vertical scroll offsets. */
XUI_API int xuiDocumentViewSetScroll(xui_widget view, double x, double y);
/* Read horizontal and vertical scroll offsets. */
XUI_API int xuiDocumentViewGetScroll(xui_widget view, double* x, double* y);
/* Set the DocumentView display zoom. */
XUI_API int xuiDocumentViewSetZoom(xui_widget view, float zoom);
/* Select visual, source or live Markdown view mode. */
XUI_API int xuiDocumentViewSetMode(xui_widget view, uint32_t mode);
/* Explicit provider invalidation; named surface changes refresh on the next update. */
XUI_API int xuiDocumentViewInvalidateObjects(xui_widget view);
/* Rebuild the renderer after external font metrics change; preserve the
 * visible reading position in a fixed-height view. */
XUI_API int xuiDocumentViewInvalidateFonts(xui_widget view);
/* Return the current DocumentView mode. */
XUI_API uint32_t xuiDocumentViewGetMode(xui_widget view);
/* Read measured or estimated view content size. */
XUI_API int xuiDocumentViewGetContentSize(xui_widget view, xui_doc_rect_t* size, int* exact);
/* Per-view layout diagnostics; counters are cumulative for the current renderer. */
XUI_API int xuiDocumentViewGetRenderStats(xui_widget view, xui_doc_renderer_stats_t* stats);
/* Map widget-local coordinates to a Document position. */
XUI_API int xuiDocumentViewHitTest(xui_widget view, double x, double y, xui_doc_position_t* position);
/* Hit-test a table cell in widget-local coordinates. */
XUI_API int xuiDocumentViewHitTestCell(xui_widget view, double x, double y, xui_doc_cell_hit_t* out);
/* Read a cell rectangle in widget-local coordinates. */
XUI_API int xuiDocumentViewGetCellRect(xui_widget view, xui_doc_node_id cell, xui_doc_cell_hit_t* out);
/* Select a grid rectangle in VISUAL mode. Intersected merged cells are
 * included in full. NULL clears the rectangle while retaining the text caret.
 * A non-NULL selection requires enabled selection and a current table ID;
 * invalid rectangles leave the prior selection unchanged. */
XUI_API int xuiDocumentViewSetTableSelection(xui_widget view, const xui_doc_table_selection_t* selection);
/* Alt+drag uses the same rectangle rules. Returns NOT_FOUND when no
 * rectangular selection is active. */
XUI_API int xuiDocumentViewGetTableSelection(xui_widget view, xui_doc_table_selection_t* out);
/* Find a case-sensitive literal in visible Document content. */
XUI_API int xuiDocumentViewFind(xui_widget view, const char* pattern, uint64_t bytes, int backward, int wrap, xui_doc_range_t* match);
/* Find text with literal or regular-expression options. */
XUI_API int xuiDocumentViewFindEx(xui_widget view, const char* pattern,
    uint64_t bytes, uint32_t flags, int backward, int wrap,
    xui_doc_range_t* match);
/* Store a UTF-8 query for this View/Editor. The plain API searches a
 * case-sensitive literal; Ex accepts XUI_DOC_FIND_REGEX/IGNORE_CASE. Results
 * are searched in semantic text for VISUAL, Markdown source for SOURCE/LIVE.
 * A zero-result query is valid. Changes to the Document recompute results on
 * the next render or result query; changing the bound Document clears the query. */
XUI_API int xuiDocumentViewSetFindQuery(xui_widget view, const char* pattern, uint64_t bytes);
/* Set a persistent search query with matching flags. */
XUI_API int xuiDocumentViewSetFindQueryEx(xui_widget view, const char* pattern,
    uint64_t bytes, uint32_t flags);
/* NULL clears the optional search range. A nonempty range is kept in the
 * View's current coordinate mode and mapped through Document revisions. */
XUI_API int xuiDocumentViewSetFindScope(xui_widget view, const xui_doc_range_t* scope);
/* Read the optional search-scope range. */
XUI_API int xuiDocumentViewGetFindScope(xui_widget view, xui_doc_range_t* scope);
/* Clear the view's persistent find query and results. */
XUI_API int xuiDocumentViewClearFind(xui_widget view);
/* Read the number of current find results. */
XUI_API int xuiDocumentViewGetFindResultCount(xui_widget view, uint64_t* count);
/* Read a result range and whether it is active. */
XUI_API int xuiDocumentViewGetFindResult(xui_widget view, uint64_t index, xui_doc_range_t* range, int* active);
/* Select one current find result and scroll it into view. */
XUI_API int xuiDocumentViewActivateFindResult(xui_widget view, uint64_t index, xui_doc_range_t* match);
#endif

/* MessageList embeds the same renderer/Document selection model used by a
 * DocumentView in a message body. The list owns a renderer, retains the
 * Document, and releases both when the message is replaced or removed.
 * A NULL descriptor detaches the Document and restores plain-text drawing.
 * Coordinates for HitNodeDocument are world coordinates, as for
 * xuiMessageListGetNodeAt. */
typedef struct xui_message_document_desc_t {
    uint32_t iSize;
    xui_document pDocument;
    xui_doc_renderer_desc_t tRenderer; /* iSize == 0 selects renderer defaults. */
    uint32_t iSelectionColor;
    xui_doc_view_event_proc onActivate;
    void* pUser;
    /* UI-thread request to set a task item's checked state. The host owns
     * the Document transaction; this callback does not mutate it. */
    void (*onTaskToggle)(xui_widget list, xui_doc_node_id item,
        int checked, void* user);
} xui_message_document_desc_t;
/* Bind or detach a Document for one MessageList item. */
#if XUI_ENABLE_MESSAGE_LIST
XUI_API int xuiMessageListSetNodeDocument(xui_widget list, const char* message_id,
    const xui_message_document_desc_t* desc);
/* Return the Document bound to one MessageList item. */
XUI_API xui_document xuiMessageListGetNodeDocument(xui_widget list, const char* message_id);
/* Read the bound message's current renderer counters without synchronizing or
 * laying it out. stats->iSize must equal sizeof(*stats); an absent message or
 * Document binding returns XUI_ERROR_NOT_FOUND. */
XUI_API int xuiMessageListGetNodeDocumentRenderStats(xui_widget list,
    const char* message_id, xui_doc_renderer_stats_t* stats);
/* Call after an external object provider changes measured size or pixels. */
XUI_API int xuiMessageListInvalidateNodeDocumentObjects(xui_widget list,
    const char* message_id);
/* Notify one bound message after its renderer font callback changes metrics. */
XUI_API int xuiMessageListInvalidateNodeDocumentFonts(xui_widget list,
    const char* message_id);
/* Hit-test a message Document in world coordinates. */
XUI_API int xuiMessageListHitNodeDocument(xui_widget list, const char* message_id,
    double world_x, double world_y, xui_doc_position_t* position);
/* The local semantic range for this message within a list-wide drag selection.
 * Intermediate Document messages return their complete content range. */
XUI_API int xuiMessageListGetNodeDocumentSelection(xui_widget list, const char* message_id,
    xui_doc_range_t* range);
/* VISUAL message table rectangle, expanded to include complete merged cells.
 * NULL clears a rectangle without changing the underlying Document. */
XUI_API int xuiMessageListSetNodeDocumentTableSelection(xui_widget list,
    const char* message_id, const xui_doc_table_selection_t* selection);
/* Read a message's selected table rectangle. */
XUI_API int xuiMessageListGetNodeDocumentTableSelection(xui_widget list,
    const char* message_id, xui_doc_table_selection_t* out);
#endif

/* A DocumentEditor is a DocumentView subtype; every View API also accepts it.
 * tView is first so the base type can initialize the same renderer/subscription. */
typedef void (*xui_doc_editor_error_proc)(xui_widget editor, int error, void* user);
typedef struct xui_doc_editor_desc_t {
    xui_doc_view_desc_t tView;
    uint32_t iSize;
    uint32_t iMode;
    int bReadOnly;
    uint32_t iCaretColor;
    xui_doc_editor_error_proc onError;
    void* pUser;
    uint64_t iAsyncSourceThresholdBytes; /* SOURCE/LIVE input: 0=100 KiB, UINT64_MAX=synchronous, 1=always async. */
    uint32_t iUndoGroupTimeoutMs; /* Interactive typing/deletion: 0=1000 ms; UINT32_MAX disables merging. */
} xui_doc_editor_desc_t;
enum xui_doc_editor_command {
    XUI_DOC_EDIT_UNDO = 1, XUI_DOC_EDIT_REDO, XUI_DOC_EDIT_SELECT_ALL,
    XUI_DOC_EDIT_COPY, XUI_DOC_EDIT_CUT, XUI_DOC_EDIT_PASTE,
    XUI_DOC_EDIT_BACKSPACE, XUI_DOC_EDIT_DELETE, XUI_DOC_EDIT_ENTER,
    XUI_DOC_EDIT_BOLD, XUI_DOC_EDIT_ITALIC, XUI_DOC_EDIT_UNDERLINE,
    XUI_DOC_EDIT_STRIKE,
    /* VISUAL: Tab/Shift+Tab adjust all selected items in one list as one edit. */
    XUI_DOC_EDIT_INDENT_LIST, XUI_DOC_EDIT_OUTDENT_LIST,
    XUI_DOC_EDIT_PARAGRAPH, XUI_DOC_EDIT_HEADING_1, XUI_DOC_EDIT_HEADING_2,
    XUI_DOC_EDIT_HEADING_3, XUI_DOC_EDIT_HEADING_4, XUI_DOC_EDIT_HEADING_5,
    XUI_DOC_EDIT_HEADING_6,
    XUI_DOC_EDIT_ALIGN_LEFT, XUI_DOC_EDIT_ALIGN_CENTER,
    XUI_DOC_EDIT_ALIGN_RIGHT, XUI_DOC_EDIT_ALIGN_JUSTIFY,
    XUI_DOC_EDIT_CODE, XUI_DOC_EDIT_SUBSCRIPT,
    XUI_DOC_EDIT_SUPERSCRIPT, XUI_DOC_EDIT_HIGHLIGHT,
    /* VISUAL table navigation; Next at the final Cell appends a row. */
    XUI_DOC_EDIT_TABLE_NEXT_CELL, XUI_DOC_EDIT_TABLE_PREVIOUS_CELL,
    XUI_DOC_EDIT_TABLE_INSERT_ROW_BEFORE, XUI_DOC_EDIT_TABLE_INSERT_ROW_AFTER,
    XUI_DOC_EDIT_TABLE_DELETE_ROW, XUI_DOC_EDIT_TABLE_INSERT_COLUMN_BEFORE,
    XUI_DOC_EDIT_TABLE_INSERT_COLUMN_AFTER, XUI_DOC_EDIT_TABLE_DELETE_COLUMN,
    XUI_DOC_EDIT_TABLE_SPLIT_CELL, XUI_DOC_EDIT_TABLE_MERGE_CELLS,
    /* Rich VISUAL: adjust the caret cell's rightmost column by 8 logical
     * units or restore automatic sizing. */
    XUI_DOC_EDIT_TABLE_COLUMN_NARROWER, XUI_DOC_EDIT_TABLE_COLUMN_WIDER,
    XUI_DOC_EDIT_TABLE_COLUMN_AUTO,
    /* VISUAL: clear selected inline formatting in one undoable edit. */
    XUI_DOC_EDIT_CLEAR_FORMATTING,
    /* VISUAL: create a new list from selected sibling paragraphs/headings. */
    XUI_DOC_EDIT_BULLET_LIST, XUI_DOC_EDIT_NUMBER_LIST, XUI_DOC_EDIT_TASK_LIST,
    /* VISUAL: move the selected sibling blocks/list items one place. */
    XUI_DOC_EDIT_MOVE_BLOCK_UP, XUI_DOC_EDIT_MOVE_BLOCK_DOWN,
    /* VISUAL: toggle the nearest quote, or insert a thematic rule/empty code block. */
    XUI_DOC_EDIT_BLOCK_QUOTE, XUI_DOC_EDIT_INSERT_RULE,
    XUI_DOC_EDIT_INSERT_CODE_BLOCK,
    /* VISUAL: toggle the checkbox of the task item containing the caret. */
    XUI_DOC_EDIT_TOGGLE_TASK
};
enum xui_doc_editor_toolbar_group {
    XUI_DOC_TOOLBAR_HISTORY = 1u,
    XUI_DOC_TOOLBAR_CLIPBOARD = 2u,
    XUI_DOC_TOOLBAR_INLINE_FORMAT = 4u,
    XUI_DOC_TOOLBAR_ALIGNMENT = 8u,
    XUI_DOC_TOOLBAR_BLOCKS = 16u,
    XUI_DOC_TOOLBAR_LISTS = 32u,
    XUI_DOC_TOOLBAR_DEFAULT = XUI_DOC_TOOLBAR_HISTORY | XUI_DOC_TOOLBAR_CLIPBOARD |
        XUI_DOC_TOOLBAR_INLINE_FORMAT | XUI_DOC_TOOLBAR_ALIGNMENT |
        XUI_DOC_TOOLBAR_BLOCKS
};
typedef struct xui_doc_command_state_t {
    uint32_t iSize;
    int bEnabled, bActive, bMixed;
    int iDisabledReason;
} xui_doc_command_state_t;
typedef struct xui_doc_paragraph_spacing_state_t {
    uint32_t iSize;
    int bEnabled, bMixed, bUseDefault;
    float fValue; /* Logical units after each paragraph/heading. */
    int iDisabledReason;
} xui_doc_paragraph_spacing_state_t;
/* Get the registered DocumentEditor widget type. */
#if XUI_ENABLE_DOCUMENT_EDITOR
XUI_API xui_widget_type xuiDocumentEditorGetType(xui_context context);
/* Create a DocumentEditor using a DocumentView base. */
XUI_API int xuiDocumentEditorCreate(xui_context context, const xui_doc_editor_desc_t* desc, xui_widget* out);
/* Direct InsertText/Execute calls are programmatic transactions. Unpublished
 * SOURCE calls may share their current batch; published calls do not acquire
 * automatic input groups. To deliver interactive input, dispatch text/key/IME
 * events. Interactive groups use arrival time across flushes/publications;
 * navigation/selection/focus/mode/format/save boundaries break a group, and
 * paste/cut/newline/IME confirmation are independent units. A new unit waits
 * for the preceding pending unit to publish; queued events keep SaveFile BUSY. */
XUI_API int xuiDocumentEditorInsertText(xui_widget editor, const char* text, uint64_t bytes);
/* Markdown SOURCE/LIVE: append UTF-8 source chunks at the document tail. Incomplete
 * code points remain buffered until a following chunk; final_chunk closes the
 * stream and rejects an incomplete final code point. Flush may publish complete
 * frames while the stream stays open; all its frames form one Undo unit.
 * Save/mode changes and other edits stay BUSY until final or CancelInput.
 * CancelInput discards only unpublished input; published frames remain and
 * can be reverted as one history group with Undo. */
XUI_API int xuiDocumentEditorAppendStreamSource(xui_widget editor,
    const char* text, uint64_t bytes, int final_chunk);
/* Execute one enabled editor command. */
XUI_API int xuiDocumentEditorExecute(xui_widget editor, uint32_t command);
/* Built-in command menu. Created on first open; coordinates are in the
 * context/world space used by pointer events. The editor owns its menu. */
XUI_API xui_widget xuiDocumentEditorGetMenuWidget(xui_widget editor);
/* Open the editor's built-in command menu at world coordinates. */
XUI_API int xuiDocumentEditorOpenMenu(xui_widget editor, float x, float y);
/* Populate an existing XUI Toolbar with supported Document commands. Zero
 * groups selects DEFAULT. The host routes the Toolbar select callback to
 * ExecuteToolbarItem and calls SyncToolbar after selection, mode, document or
 * language changes. No widget lifetime is retained by this adapter. */
XUI_API int xuiDocumentEditorSetupToolbar(xui_widget editor, xui_widget toolbar, uint32_t groups);
/* Refresh command availability and state in a bound toolbar. */
XUI_API int xuiDocumentEditorSyncToolbar(xui_widget editor, xui_widget toolbar);
/* Execute the command assigned to a toolbar item. */
XUI_API int xuiDocumentEditorExecuteToolbarItem(xui_widget editor, xui_widget toolbar, int index);
/* Availability checks the current mode, profile, selection, backend and history.
 * A semantic Markdown command still validates representability before commit. */
XUI_API int xuiDocumentEditorQueryCommand(xui_widget editor, uint32_t command, xui_doc_command_state_t* state);
/* Report whether a command is currently available. */
XUI_API int xuiDocumentEditorCanExecute(xui_widget editor, uint32_t command);
/* RICH/VISUAL paragraph spacing; use_default restores the renderer gap.
 * Query reports a mixed selection without discarding its first value. */
XUI_API int xuiDocumentEditorQueryParagraphSpacing(xui_widget editor, xui_doc_paragraph_spacing_state_t* state);
/* Set or restore Rich paragraph spacing for the selection. */
XUI_API int xuiDocumentEditorSetParagraphSpacing(xui_widget editor, float spacing, int use_default);
/* Enable or disable editor mutations. */
XUI_API int xuiDocumentEditorSetReadOnly(xui_widget editor, int read_only);
/* Rich VISUAL table column width; zero restores automatic sizing. Pointer
 * drags on internal and outer-right cell boundaries use the same transaction. */
XUI_API int xuiDocumentEditorSetTableColumnWidth(xui_widget editor,
    xui_doc_node_id table, uint32_t column, float width);
/* Toggle one existing task-list item as one undoable visual edit. Pointer and
 * accessibility activation use this same operation. */
XUI_API int xuiDocumentEditorToggleTaskItem(xui_widget editor,
    xui_doc_node_id item);
/* Return whether the editor is read-only. */
XUI_API int xuiDocumentEditorGetReadOnly(xui_widget editor);
/* Set and clear inline marks on the current selection. */
XUI_API int xuiDocumentEditorSetMarks(xui_widget editor, uint32_t set, uint32_t clear);
#endif
typedef struct xui_doc_editor_text_style_state_t {
    uint32_t iSize;
    int bEnabled;
    int iDisabledReason;
    xui_doc_text_style_query_t tQuery;
} xui_doc_editor_text_style_state_t;
/* VISUAL rich-text style controls. At a caret SetTextStyle changes the pending
 * typing style; a selected range is committed as one Undo unit. */
#if XUI_ENABLE_DOCUMENT_EDITOR
XUI_API int xuiDocumentEditorQueryTextStyle(xui_widget editor, xui_doc_editor_text_style_state_t* state);
/* Set Rich text style at the caret or selected range. */
XUI_API int xuiDocumentEditorSetTextStyle(xui_widget editor, uint32_t fields, const xui_doc_text_style_t* style);
/* Replace a VISUAL selection (or insert at a caret) with a single-line linked
 * label in one Undo unit. SetLink formats a nonempty selection; at a caret a
 * nonempty URI is inserted as the visible label. Empty URI removes a selected
 * link together with its destination/title. */
XUI_API int xuiDocumentEditorInsertLink(xui_widget editor, const char* label, uint64_t label_bytes,
    const char* uri, const char* title);
/* Set or remove the current selection's link destination. */
XUI_API int xuiDocumentEditorSetLink(xui_widget editor, const char* uri, const char* title);
/* Insert an image at the current visual selection. */
XUI_API int xuiDocumentEditorInsertImage(xui_widget editor, const xui_doc_image_desc_t* image);
/* Insert an inline/display formula, Mermaid block or inline/block HTML from a
 * VISUAL editor selection. Successful insertion is one Undo unit. */
XUI_API int xuiDocumentEditorInsertObject(xui_widget editor, uint32_t kind,
    uint32_t flags, const char* utf8, uint64_t bytes);
/* VISUAL block commands. Code insertion places the caret in the new code
 * block; a rule places it after the rule. Both form one Undo unit. */
XUI_API int xuiDocumentEditorInsertCodeBlock(xui_widget editor,
    const char* language, const char* utf8, uint64_t bytes);
/* Change an existing code block's language token. Markdown fenced code keeps
 * its other info bytes; clearing a token that would promote metadata to the
 * language returns UNREPRESENTABLE without publishing. */
XUI_API int xuiDocumentEditorSetCodeBlockLanguage(xui_widget editor,
    xui_doc_node_id code_id, const char* language);
/* Insert a thematic rule at the visual caret. */
XUI_API int xuiDocumentEditorInsertRule(xui_widget editor);
/* Quote the selected consecutive sibling paragraphs/headings, or the block
 * at a caret. Unwrap operates on the nearest enclosing quote at a caret. */
XUI_API int xuiDocumentEditorWrapQuote(xui_widget editor);
/* Remove the quote enclosing the visual caret. */
XUI_API int xuiDocumentEditorUnwrapQuote(xui_widget editor);
/* Insert a footnote reference and definition as one VISUAL edit, then focus
 * the definition body. NULL/empty label selects an unused fnN label. */
XUI_API int xuiDocumentEditorInsertFootnote(xui_widget editor,
    const char* label, const char* initial_utf8, uint64_t initial_bytes);
/* Remove a footnote reference in one VISUAL edit and focus its former place. */
XUI_API int xuiDocumentEditorRemoveFootnoteReference(xui_widget editor,
    xui_doc_node_id reference_id);
/* Insert a table at a collapsed VISUAL caret. A caret inside a paragraph
 * splits that paragraph so text on either side remains in document order.
 * The caret moves to the first cell; the operation is one Undo unit. */
XUI_API int xuiDocumentEditorInsertTable(xui_widget editor, uint32_t rows, uint32_t columns);
/* Update one image's alt text, resource or title. */
XUI_API int xuiDocumentEditorUpdateImage(xui_widget editor, xui_doc_node_id image_id,
    const xui_doc_image_desc_t* image);
/* Replace the complete source payload of a MATH, DIAGRAM, HTML or CODE_BLOCK node in one
 * root edit. The node keeps its ID; Markdown accepts only edits whose parsed
 * semantic result still matches the requested object. SOURCE/LIVE use normal
 * text editing instead. The caller reads source via SnapshotCopyText. */
XUI_API int xuiDocumentEditorUpdateObjectSource(xui_widget editor,
    xui_doc_node_id object_id, const char* utf8, uint64_t bytes);
/* Replace the currently selected find match as one edit. A selection
 * that is not an exact current match returns NOT_FOUND without changing text. */
XUI_API int xuiDocumentEditorReplaceCurrent(xui_widget editor, const char* pattern,
    uint64_t pattern_bytes, const char* replacement, uint64_t replacement_bytes);
/* Replace the selected current search result with matching flags. */
XUI_API int xuiDocumentEditorReplaceCurrentEx(xui_widget editor,
    const char* pattern, uint64_t pattern_bytes, const char* replacement,
    uint64_t replacement_bytes, uint32_t flags);
/* Replace literal matches within the optional scope. */
XUI_API int xuiDocumentEditorReplaceAll(xui_widget editor, const char* pattern, uint64_t pattern_bytes,
    const char* replacement, uint64_t replacement_bytes, const xui_doc_range_t* scope, uint64_t* replaced);
/* Replace matches using literal or regex options. */
XUI_API int xuiDocumentEditorReplaceAllEx(xui_widget editor,
    const char* pattern, uint64_t pattern_bytes, const char* replacement,
    uint64_t replacement_bytes, uint32_t flags,
    const xui_doc_range_t* scope, uint64_t* replaced);
/* Lazy XUI-native find/replace window. The result list and highlights use the
 * same DocumentView query; edits use the Editor transaction/Undo path. */
XUI_API int xuiDocumentEditorOpenFind(xui_widget editor);
/* Open the editor's built-in replace window. */
XUI_API int xuiDocumentEditorOpenReplace(xui_widget editor);
/* Return the editor's owned find/replace window. */
XUI_API xui_widget xuiDocumentEditorGetFindWindow(xui_widget editor);
/* Cancel the current IME composition. */
XUI_API int xuiDocumentEditorCancelComposition(xui_widget editor);
/* Report whether IME composition is active. */
XUI_API int xuiDocumentEditorIsComposing(xui_widget editor);
/* Nonblocking: dispatch/poll latest SOURCE input, publish READY on the owner.
 * BUSY means retry after xuiUpdate; SaveFile and mode/document transitions stay
 * BUSY until input is published or explicitly cancelled. Failed input remains
 * visible and save-blocking; Retry creates a new generation without losing it.
 * Blur keeps accepted input and continues publishing. Widget destruction
 * cancels unpublished input and joins its worker; Flush before destroying to
 * keep that input. Other views continue to expose the committed snapshot. */
XUI_API int xuiDocumentEditorFlush(xui_widget editor);
/* Retry a retained failed source input generation. */
XUI_API int xuiDocumentEditorRetryInput(xui_widget editor);
/* Discard unpublished input and queued editor events. */
XUI_API int xuiDocumentEditorCancelInput(xui_widget editor);
/* Read the pending source candidate's state and generation. */
XUI_API int xuiDocumentEditorGetPendingInput(xui_widget editor, xui_doc_prepare_info_t* info);
#endif
typedef struct xui_doc_editor_pending_work_t {
    uint32_t iSize;
    int bHasInput;
    uint64_t iQueuedEvents;
    int iResult; /* OK when idle; BUSY while running; a retained failure requires RetryInput/CancelInput. */
} xui_doc_editor_pending_work_t;
/* Keyboard/text/IME/pointer events waiting for a flush execute in FIFO order.
 * Direct editing/selection/scroll/zoom APIs return BUSY while such events are
 * queued or replayed, including calls from host callbacks. Clipboard callbacks
 * also reject reentrant Editor APIs; delivered events are appended in order.
 * Explicit CancelInput discards preedit, pending input and queued events;
 * external Document writes also invalidate the suspended clipboard action.
 * Flush drains the queue and latest prepare, bounded
 * to 64 events per call with a 4 ms budget checked between events; an atomic
 * command is never interrupted. Queued events also block DocumentSaveFile. */
#if XUI_ENABLE_DOCUMENT_EDITOR
XUI_API int xuiDocumentEditorGetPendingWork(xui_widget editor, xui_doc_editor_pending_work_t* info);
#endif

#ifdef __cplusplus
}
#endif
#endif
