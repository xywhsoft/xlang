#include "xui_internal.h"
typedef struct xui_code_completion_owned_item_t {
	xui_code_completion_item_t tItem;
	char* sLabel;
	char* sInsertText;
	char* sDetail;
	char* sDocumentation;
	char* sFilterText;
	char* sSortText;
	char* sCommitCharacters;
	int iMatchScore;
} xui_code_completion_owned_item_t;

typedef struct xui_code_edit_text_segment_t {
	int iStart;
	int iEnd;
	float fX;
	xui_text_shape_t tShape;
} xui_code_edit_text_segment_t;

typedef struct xui_code_edit_caret_stop_t {
	int iOffset;
	float fX;
} xui_code_edit_caret_stop_t;

typedef struct xui_code_edit_line_layout_t {
	uint32_t iDocumentVersion;
	xui_font pFont;
	int iTabColumns;
	char* sText;
	int iTextSize;
	xui_code_edit_text_segment_t* pSegments;
	int iSegmentCount;
	int iSegmentCapacity;
	xui_code_edit_caret_stop_t* pStops;
	int iStopCount;
	int iStopCapacity;
	float fWidth;
	int* pWrapOffsets;
	int iWrapOffsetCount;
	int iWrapOffsetCapacity;
	float fWrapWidth;
	uint32_t iUseStamp;
} xui_code_edit_line_layout_t;

