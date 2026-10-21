#ifndef XUI_DOCUMENT_H
#define XUI_DOCUMENT_H

/* Unified, window-independent document API. All input strings are UTF-8.
 * Owning output handles must be released. Snapshot slices are borrowed until
 * the snapshot is released. A live document has one writer; snapshots may be
 * read concurrently. No node, cell, or view owns a separate undo history. */
#include "xui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xui_document_t* xui_document;
typedef struct xui_doc_snapshot_t* xui_document_snapshot;
typedef struct xui_doc_transaction_t* xui_document_transaction;
typedef struct xui_doc_change_set_t* xui_document_change_set;
typedef struct xui_doc_prepare_t* xui_document_prepare;
typedef struct xui_doc_fragment_t* xui_document_fragment;
typedef struct xui_doc_source_index_t* xui_document_source_index;
typedef uint64_t xui_doc_node_id;
#define XUI_DOCUMENT_ROOT UINT64_C(1)
#define XUI_DOCUMENT_APPEND UINT64_MAX

enum xui_doc_profile {
    XUI_DOCUMENT_RICH = 1,
    XUI_DOCUMENT_MARKDOWN = 2
};
enum xui_doc_markdown_dialect {
    XUI_MD_COMMONMARK = 1, XUI_MD_GFM = 2, XUI_MD_EXTENDED = 3
};
enum xui_doc_node_kind {
    XUI_DOC_ROOT = 1, XUI_DOC_PARAGRAPH, XUI_DOC_HEADING, XUI_DOC_TEXT,
    XUI_DOC_QUOTE, XUI_DOC_LIST, XUI_DOC_LIST_ITEM, XUI_DOC_CODE_BLOCK,
    XUI_DOC_TABLE, XUI_DOC_ROW, XUI_DOC_CELL, XUI_DOC_IMAGE,
    XUI_DOC_RULE, XUI_DOC_SOFT_BREAK, XUI_DOC_HARD_BREAK,
    XUI_DOC_HTML, XUI_DOC_MATH, XUI_DOC_DIAGRAM, XUI_DOC_FOOTNOTE,
    XUI_DOC_FOOTNOTE_REF, XUI_DOC_FRONT_MATTER, XUI_DOC_EXTENSION
};
enum xui_doc_mark {
    XUI_DOC_BOLD = 1u, XUI_DOC_ITALIC = 2u, XUI_DOC_UNDERLINE = 4u,
    XUI_DOC_STRIKE = 8u, XUI_DOC_CODE = 16u, XUI_DOC_SUBSCRIPT = 32u,
    XUI_DOC_SUPERSCRIPT = 64u, XUI_DOC_LINK = 128u, XUI_DOC_HIGHLIGHT = 256u
};
enum xui_doc_error {
    XUI_DOC_ERROR_SCHEMA = -100, XUI_DOC_ERROR_UTF8 = -101,
    XUI_DOC_ERROR_STALE = -102, XUI_DOC_ERROR_BUSY = -103,
    XUI_DOC_ERROR_LIMIT = -104, XUI_DOC_ERROR_FORMAT = -105,
    XUI_DOC_ERROR_DOMAIN = -106, XUI_DOC_ERROR_UNREPRESENTABLE = -107,
    XUI_DOC_ERROR_IO = -108, XUI_DOC_ERROR_CANCELLED = -109
};
enum xui_doc_position_kind { XUI_DOC_POSITION_TEXT = 1, XUI_DOC_POSITION_GAP = 2, XUI_DOC_POSITION_SOURCE = 3 };
enum xui_doc_affinity { XUI_DOC_BEFORE = 0, XUI_DOC_AFTER = 1 };
enum xui_doc_domain { XUI_DOC_SEMANTIC = 1, XUI_DOC_SOURCE = 2 };
enum xui_doc_file_format { XUI_DOC_FILE_NATIVE = 1, XUI_DOC_FILE_MARKDOWN = 2, XUI_DOC_FILE_TEXT = 3, XUI_DOC_FILE_HTML = 4 };
enum xui_doc_markdown_loss_reason {
    XUI_DOC_MD_LOSS_NODE = 1u,       /* Node kind has no selected-dialect syntax. */
    XUI_DOC_MD_LOSS_DIALECT = 2u,    /* Feature needs a richer Markdown dialect. */
    XUI_DOC_MD_LOSS_MARK = 4u,       /* Inline mark cannot be encoded. */
    XUI_DOC_MD_LOSS_TEXT_STYLE = 8u, /* Font, foreground or background styling. */
    XUI_DOC_MD_LOSS_BLOCK_STYLE = 16u, /* Alignment or paragraph spacing. */
    XUI_DOC_MD_LOSS_GEOMETRY = 32u, /* Image size, table widths or cell spans. */
    XUI_DOC_MD_LOSS_TABLE_CONTENT = 64u, /* Cell content is not a GFM cell. */
    XUI_DOC_MD_LOSS_METADATA = 128u, /* Resource/info/title or flags would be ignored. */
    XUI_DOC_MD_LOSS_EMPTY_BLOCK = 256u, /* Empty paragraph would disappear. */
    XUI_DOC_MD_LOSS_ROUNDTRIP = 512u, /* Generated Markdown reparses differently. */
    XUI_DOC_MD_LOSS_LANGUAGE = 1024u /* Plain Markdown cannot encode natural-language overrides. */
};
enum xui_doc_change_flags {
    XUI_DOC_CHANGE_TEXT = 1u, XUI_DOC_CHANGE_STRUCTURE = 2u,
    XUI_DOC_CHANGE_STYLE = 4u, XUI_DOC_CHANGE_SOURCE = 8u,
    XUI_DOC_CHANGE_RESOURCE = 16u, XUI_DOC_CHANGE_RESET = 32u
};
enum xui_doc_operation_kind {
    XUI_DOC_OP_TEXT = 1, XUI_DOC_OP_INSERT, XUI_DOC_OP_DELETE,
    XUI_DOC_OP_MOVE, XUI_DOC_OP_ATTRIBUTES, XUI_DOC_OP_SOURCE,
    XUI_DOC_OP_SPLIT, XUI_DOC_OP_MERGE
};
enum xui_doc_operation_mapping_flags {
    /* A container SPLIT/MERGE maps gaps; this MOVE still records ancestry. */
    XUI_DOC_OP_ANCESTRY_ONLY = 1u
};
enum xui_doc_mapping {
    XUI_DOC_MAP_EXACT = 0, XUI_DOC_MAP_DELETED = 1, XUI_DOC_MAP_APPROXIMATE = 2,
    XUI_DOC_MAP_COLLAPSED = 3, /* Interior of an indivisible decoded source segment. */
    XUI_DOC_MAP_SYNTAX = 4    /* A source delimiter/gap mapped to a content boundary. */
};
enum xui_doc_source_segment_kind {
    XUI_DOC_SOURCE_DIRECT = 1, XUI_DOC_SOURCE_ESCAPE, XUI_DOC_SOURCE_ENTITY,
    XUI_DOC_SOURCE_NORMALIZED, XUI_DOC_SOURCE_SYNTHETIC
};
enum xui_doc_source_segment_flags { XUI_DOC_SOURCE_PARTIAL_START = 1u, XUI_DOC_SOURCE_PARTIAL_END = 2u };
typedef struct xui_doc_source_segment_t {
    uint32_t iSize, iKind;
    uint64_t iTextStart, iTextEnd, iSourceStart, iSourceEnd;
    uint32_t iFlags, iReserved;
} xui_doc_source_segment_t;
/* One lossless raw Markdown line: [LineStart,IndentEnd) is leading space/Tab,
 * [IndentEnd,TrailingStart) is the body, [TrailingStart,ContentEnd) is trailing
 * space/Tab, and [ContentEnd,LineEnd) is the original LF, CR or CRLF. Blank
 * lines assign all horizontal whitespace to the leading range. */
typedef struct xui_doc_source_line_t {
    uint32_t iSize, iReserved;
    uint64_t iLineStart, iIndentEnd, iTrailingStart, iContentEnd, iLineEnd;
} xui_doc_source_line_t;
enum xui_doc_block_syntax_kind {
    XUI_DOC_BLOCK_SYNTAX_ATX_HEADING = 1,
    XUI_DOC_BLOCK_SYNTAX_SETEXT_HEADING,
    XUI_DOC_BLOCK_SYNTAX_FENCED_CODE,
    XUI_DOC_BLOCK_SYNTAX_THEMATIC_BREAK,
    XUI_DOC_BLOCK_SYNTAX_LIST_ITEM,
    XUI_DOC_BLOCK_SYNTAX_QUOTE_OPEN,
    XUI_DOC_BLOCK_SYNTAX_TABLE_UNDERLINE,
    XUI_DOC_BLOCK_SYNTAX_INDENTED_CODE,
    XUI_DOC_BLOCK_SYNTAX_FRONT_MATTER
};
typedef struct xui_doc_block_syntax_t {
    uint32_t iSize, iKind;
    /* Parser-confirmed marker byte ranges in the original Markdown source.
     * ATX: opening hashes and optional closing hashes. Setext: underline only.
     * Fence: opener, optional closer and its raw opening-line tail. The tail
     * includes all spaces, tabs and info text between the opener and newline;
     * an empty tail is a valid zero-length range. Rule: the complete
     * nontrailing marker.
     * List item: bullet/number delimiter, its following raw horizontal
     * whitespace, optional task checkbox and its following whitespace.
     * Quote: the opening '>'; all explicit continuation prefixes are indexed.
     * Admonition quotes additionally expose the original '[!TYPE]' header as
     * their secondary marker, preserving its case and source spelling.
     * Table: the confirmed separator line, excluding its line ending.
     * Indented code: the first retained line's horizontal whitespace; all
     * retained lines, including internal blank lines, have indexed records.
     * Front matter: the exact opening '---' and closing '---' or '...' markers.
     * Their complete original line endings are available through GetSourceLine;
     * the node's source range is its literal body, including original endings.
     * Absent secondary and non-fence tail endpoints are UINT64_MAX. */
    uint64_t iPrimaryStart, iPrimaryEnd, iSecondaryStart, iSecondaryEnd;
    uint64_t iFenceTailStart, iFenceTailEnd;
    /* Quote only: number of parser-confirmed '>' bytes, including the opener.
     * Lazy continuation lines have no prefix and are not counted. */
    uint64_t iQuotePrefixCount;
    /* Table only: parser-confirmed cell underlines and unescaped pipe tokens. */
    uint64_t iTableTokenCount;
    /* List item only: explicitly matched continuation-indent ranges. */
    uint64_t iListIndentCount;
    /* List item only: raw horizontal whitespace after the opening bullet or
     * ordered delimiter; an empty range is valid. With a task checkbox, this
     * ends before '['. The task gap starts after ']' and may also be empty.
     * Non-list nodes and absent task checkboxes return UINT64_MAX pairs. */
    uint64_t iListMarkerGapStart, iListMarkerGapEnd;
    uint64_t iTaskMarkerGapStart, iTaskMarkerGapEnd;
    /* Fence only: the parser's info text after leading ASCII spaces and
     * before trailing ASCII spaces, and its first language token. Both can
     * be valid empty ranges. Other block kinds return UINT64_MAX pairs. */
    uint64_t iFenceInfoStart, iFenceInfoEnd;
    uint64_t iFenceLanguageStart, iFenceLanguageEnd;
    /* Indented code only: parser-confirmed retained content lines. */
    uint64_t iCodeIndentCount;
    /* ATX/Setext heading only: parser-confirmed raw inline content extent,
     * excluding markers and their surrounding whitespace. Empty is valid.
     * Multiline Setext content can include continuation/container prefixes.
     * Other block kinds return UINT64_MAX pairs. */
    uint64_t iHeadingContentStart, iHeadingContentEnd;
} xui_doc_block_syntax_t;
enum xui_doc_code_indent_flags { XUI_DOC_CODE_INDENT_BLANK = 1u };
typedef struct xui_doc_code_indent_t {
    uint32_t iSize, iFlags;
    /* Raw horizontal whitespace immediately before the code line's content.
     * A blank line may have an empty range and has no required code column. */
    uint64_t iSourceStart, iSourceEnd;
    /* Parser's zero-based logical columns on the original line. For a
     * nonblank line [Start,Content) supplies the four code-indent columns;
     * End includes excess whitespace. Content is UINT32_MAX on blank lines.
     * A content boundary can fall inside a tab. */
    uint32_t iIndentStartColumn, iContentColumn, iIndentEndColumn, iReserved;
} xui_doc_code_indent_t;
enum xui_doc_break_syntax_kind {
    XUI_DOC_BREAK_SOFT = 1,
    XUI_DOC_BREAK_HARD_BACKSLASH,
    XUI_DOC_BREAK_HARD_SPACES,
    XUI_DOC_BREAK_HARD_FORCED
};
typedef struct xui_doc_break_syntax_t {
    uint32_t iSize, iKind;
    /* Parser-confirmed raw source ranges for one semantic break node.
     * Trailing horizontal whitespace may be empty. The hard marker is the
     * backslash or the complete trailing whitespace run; soft and forced
     * breaks have UINT64_MAX marker endpoints. LineEnding preserves the
     * original LF, CR or CRLF bytes. */
    uint64_t iTrailingStart, iTrailingEnd;
    uint64_t iHardMarkerStart, iHardMarkerEnd;
    uint64_t iLineEndingStart, iLineEndingEnd;
} xui_doc_break_syntax_t;
typedef struct xui_doc_list_indent_t {
    uint32_t iSize, iReserved;
    /* Raw horizontal whitespace after the last quote marker on that line.
     * Nested list-item ranges can overlap; lazy continuation has no record. */
    uint64_t iSourceStart, iSourceEnd;
    /* Zero-based logical columns on the original line, with tabs advancing
     * to the next multiple of four. Each list layer consumes the interval
     * [iIndentStartColumn, iContentColumn); iIndentEndColumn includes any
     * remaining whitespace. A content boundary can fall inside a tab. */
    uint32_t iIndentStartColumn, iContentColumn, iIndentEndColumn, iReserved2;
} xui_doc_list_indent_t;
enum xui_doc_table_token_kind {
    XUI_DOC_TABLE_TOKEN_PIPE = 1,
    XUI_DOC_TABLE_TOKEN_UNDERLINE
};
enum xui_doc_table_token_flags {
    XUI_DOC_TABLE_TOKEN_LEFT_COLON = 1u,
    XUI_DOC_TABLE_TOKEN_RIGHT_COLON = 2u
};
typedef struct xui_doc_table_token_t {
    uint32_t iSize, iKind, iRow, iOrdinal, iFlags, iReserved;
    uint64_t iSourceStart, iSourceEnd;
} xui_doc_table_token_t;
enum xui_doc_inline_syntax_kind {
    XUI_DOC_SYNTAX_EMPHASIS = 1, XUI_DOC_SYNTAX_STRONG, XUI_DOC_SYNTAX_UNDERLINE,
    XUI_DOC_SYNTAX_STRIKE, XUI_DOC_SYNTAX_CODE, XUI_DOC_SYNTAX_LINK,
    XUI_DOC_SYNTAX_IMAGE, XUI_DOC_SYNTAX_MATH, XUI_DOC_SYNTAX_DISPLAY_MATH,
    XUI_DOC_SYNTAX_FOOTNOTE_REF, XUI_DOC_SYNTAX_HIGHLIGHT,
    XUI_DOC_SYNTAX_SUBSCRIPT, XUI_DOC_SYNTAX_SUPERSCRIPT,
    XUI_DOC_SYNTAX_CANDIDATE_LINK, XUI_DOC_SYNTAX_CANDIDATE_IMAGE,
    XUI_DOC_SYNTAX_CANDIDATE_FOOTNOTE,
    /* Inline link/image suffix fields. Parent is LINK or IMAGE; Source includes
     * the field delimiters and Content contains the raw value. References use
     * iDefinitionIndex on the parent instead; autolinks have no suffix fields. */
    XUI_DOC_SYNTAX_DESTINATION, XUI_DOC_SYNTAX_TITLE
};
enum xui_doc_inline_syntax_flags { XUI_DOC_SYNTAX_AUTOLINK = 1u };
typedef struct xui_doc_inline_syntax_t {
    uint32_t iSize, iKind;
    /* Indices belong to this snapshot's source tree, not to the semantic
     * NodeId space. UINT64_MAX means no parent or no resolved definition. */
    uint64_t iParentIndex, iDefinitionIndex;
    uint64_t iSourceStart, iContentStart, iContentEnd, iSourceEnd;
    uint32_t iFlags, iReserved;
} xui_doc_inline_syntax_t;
enum xui_doc_reference_kind {
    XUI_DOC_REFERENCE_LINK = 1, XUI_DOC_REFERENCE_FOOTNOTE = 2
};
typedef struct xui_doc_reference_definition_t {
    uint32_t iSize, iKind;
    /* Source includes the complete lines and their container prefixes.
     * Label/destination/title are raw content ranges, excluding delimiters.
     * Footnotes have no destination/title; their endpoints are UINT64_MAX. */
    uint64_t iSourceStart, iSourceEnd, iLabelStart, iLabelEnd;
    uint64_t iDestinationStart, iDestinationEnd, iTitleStart, iTitleEnd;
} xui_doc_reference_definition_t;
typedef struct xui_doc_source_info_t {
    uint32_t iSize;
    uint64_t iInlineSyntaxCount, iReferenceDefinitionCount, iReferenceCandidateCount;
} xui_doc_source_info_t;

/* Immutable source ownership index, derived from one Markdown snapshot. It
 * retains that snapshot, never creates content/history, and does not reparse.
 * Records join semantic source ranges, parser inline syntax, definitions and
 * unresolved candidates. Tokens partition every original byte exactly once.
 * Embedded payloads can be opaque; a byte partition alone is not a claim that
 * every embedded language or unused definition body has a complete CST. */
enum xui_doc_source_record_kind {
    XUI_DOC_SOURCE_RECORD_ROOT = 1, XUI_DOC_SOURCE_RECORD_NODE,
    XUI_DOC_SOURCE_RECORD_INLINE, XUI_DOC_SOURCE_RECORD_DEFINITION,
    XUI_DOC_SOURCE_RECORD_CANDIDATE,
    XUI_DOC_SOURCE_RECORD_SYNTAX /* Parser syntax without a semantic NodeId. */
};
/* SYNTAX record iDetail. iOriginIndex is its definition ordinal, or the
 * source-ordered ordinary arena ordinal when SYNTAX_DOCUMENT is set. Both
 * refer to the retained snapshot. Field content endpoints exclude delimiters. */
enum xui_doc_source_syntax_record_kind {
    XUI_DOC_SOURCE_SYNTAX_SCOPE = 1, XUI_DOC_SOURCE_SYNTAX_BLOCK,
    XUI_DOC_SOURCE_SYNTAX_INLINE, XUI_DOC_SOURCE_SYNTAX_TEXT,
    XUI_DOC_SOURCE_SYNTAX_MARKER, XUI_DOC_SOURCE_SYNTAX_FIELD,
    XUI_DOC_SOURCE_SYNTAX_INDENT, XUI_DOC_SOURCE_SYNTAX_BREAK,
    XUI_DOC_SOURCE_SYNTAX_OPAQUE
};
enum xui_doc_source_record_flags {
    XUI_DOC_SOURCE_RECORD_UNMAPPED = 1u, XUI_DOC_SOURCE_RECORD_OPAQUE = 2u,
    XUI_DOC_SOURCE_RECORD_SYNTAX_OPEN = 4u, XUI_DOC_SOURCE_RECORD_SYNTAX_CLOSE = 8u,
    XUI_DOC_SOURCE_RECORD_SYNTAX_TABLE = 16u, XUI_DOC_SOURCE_RECORD_SYNTAX_CANDIDATE = 32u,
    XUI_DOC_SOURCE_RECORD_SYNTAX_QUOTE_INDENT = 64u,
    XUI_DOC_SOURCE_RECORD_SYNTAX_LIST_FIRST_INDENT = 128u,
    XUI_DOC_SOURCE_RECORD_SYNTAX_LIST_CONTINUATION_INDENT = 256u,
    XUI_DOC_SOURCE_RECORD_SYNTAX_CODE_INDENT = 512u,
    XUI_DOC_SOURCE_RECORD_SYNTAX_BREAK_TRAILING = 1024u,
    XUI_DOC_SOURCE_RECORD_SYNTAX_BREAK_MARKER = 2048u,
    XUI_DOC_SOURCE_RECORD_SYNTAX_BREAK_ENDING = 4096u,
    XUI_DOC_SOURCE_RECORD_SYNTAX_REFERENCE = 8192u,
    XUI_DOC_SOURCE_RECORD_SYNTAX_TAIL = 16384u,
    XUI_DOC_SOURCE_RECORD_SYNTAX_FENCE_INFO = 32768u,
    XUI_DOC_SOURCE_RECORD_SYNTAX_FENCE_LANGUAGE = 65536u,
    XUI_DOC_SOURCE_RECORD_SYNTAX_DEFINITION_PREFIX = 131072u,
    XUI_DOC_SOURCE_RECORD_SYNTAX_DEFINITION_SEPARATOR = 262144u,
    XUI_DOC_SOURCE_RECORD_SYNTAX_DOCUMENT = 524288u /* Ordinary source arena owner. */
};
typedef struct xui_doc_source_record_t {
    uint32_t iSize, iKind, iDetail, iFlags;
    /* Source-index parent, never a NodeId. Root has UINT64_MAX. Mapped child
     * ranges are contained by their parent; source ancestry can differ from
     * semantic ancestry, for example with relocated footnote projections. */
    uint64_t iParentIndex, iNodeId, iOriginIndex;
    uint64_t iSourceStart, iSourceEnd, iContentStart, iContentEnd;
} xui_doc_source_record_t;
enum xui_doc_source_token_kind {
    XUI_DOC_SOURCE_TOKEN_CONTENT = 1, XUI_DOC_SOURCE_TOKEN_INLINE_MARKER,
    XUI_DOC_SOURCE_TOKEN_BLOCK_MARKER, XUI_DOC_SOURCE_TOKEN_FIELD_CONTENT,
    XUI_DOC_SOURCE_TOKEN_REFERENCE_SYNTAX, XUI_DOC_SOURCE_TOKEN_REFERENCE_LABEL,
    XUI_DOC_SOURCE_TOKEN_REFERENCE_DESTINATION, XUI_DOC_SOURCE_TOKEN_REFERENCE_TITLE,
    XUI_DOC_SOURCE_TOKEN_HORIZONTAL_SPACE, XUI_DOC_SOURCE_TOKEN_LINE_ENDING,
    XUI_DOC_SOURCE_TOKEN_BOM, XUI_DOC_SOURCE_TOKEN_HTML,
    XUI_DOC_SOURCE_TOKEN_HTML_COMMENT, XUI_DOC_SOURCE_TOKEN_UNCLASSIFIED,
    XUI_DOC_SOURCE_TOKEN_OPAQUE, XUI_DOC_SOURCE_TOKEN_FENCE_INFO,
    XUI_DOC_SOURCE_TOKEN_CONTAINER_INDENT, XUI_DOC_SOURCE_TOKEN_LIST_GAP,
    XUI_DOC_SOURCE_TOKEN_CODE_INDENT, XUI_DOC_SOURCE_TOKEN_TABLE,
    XUI_DOC_SOURCE_TOKEN_BREAK_MARKER,
    XUI_DOC_SOURCE_TOKEN_SCOPE /* Binding only: an enclosing source record. */
};
enum xui_doc_source_token_flags {
    XUI_DOC_SOURCE_TOKEN_PARSER_CONFIRMED = 1u,
    XUI_DOC_SOURCE_TOKEN_OPAQUE_PAYLOAD = 2u
};
typedef struct xui_doc_source_token_t {
    uint32_t iSize, iKind, iDetail, iFlags;
    uint64_t iSourceStart, iSourceEnd, iRecordIndex, iNodeId;
    /* Semantic provenance, if present. DIRECT text offsets are narrowed to
     * this token; decoded/normalized segments retain the complete text range.
     * Their original source interval remains available even if markers split
     * the token. Definition-only syntax retains its original interval and
     * optional segment kind, but has no semantic text offsets. Otherwise
     * absent provenance uses UINT64_MAX endpoints. */
    uint64_t iTextStart, iTextEnd, iOriginalSourceStart, iOriginalSourceEnd;
    uint32_t iSegmentKind, iSegmentFlags;
} xui_doc_source_token_t;
/* Bindings preserve overlapping roles, including zero-length syntax anchors.
 * Unlike partition tokens, a binding retains its complete original range.
 * Quote/list-first indentation includes parser-owned whitespace before the
 * marker. A partly consumed Tab byte can occur in multiple container ranges;
 * each binding's logical columns identify its per-container consumption. */
enum xui_doc_source_binding_origin {
    XUI_DOC_SOURCE_BINDING_RANGE = 0, XUI_DOC_SOURCE_BINDING_PROVENANCE,
    XUI_DOC_SOURCE_BINDING_BLOCK_PRIMARY, XUI_DOC_SOURCE_BINDING_BLOCK_SECONDARY,
    XUI_DOC_SOURCE_BINDING_FENCE_TAIL, XUI_DOC_SOURCE_BINDING_FENCE_INFO,
    XUI_DOC_SOURCE_BINDING_FENCE_LANGUAGE, XUI_DOC_SOURCE_BINDING_LIST_MARKER_GAP,
    XUI_DOC_SOURCE_BINDING_TASK_MARKER_GAP, XUI_DOC_SOURCE_BINDING_QUOTE_PREFIX,
    XUI_DOC_SOURCE_BINDING_QUOTE_INDENT, XUI_DOC_SOURCE_BINDING_LIST_FIRST_INDENT,
    XUI_DOC_SOURCE_BINDING_LIST_CONTINUATION_INDENT, XUI_DOC_SOURCE_BINDING_CODE_INDENT,
    XUI_DOC_SOURCE_BINDING_TABLE_TOKEN, XUI_DOC_SOURCE_BINDING_INLINE_OPEN,
    XUI_DOC_SOURCE_BINDING_INLINE_CLOSE, XUI_DOC_SOURCE_BINDING_INLINE_FIELD,
    XUI_DOC_SOURCE_BINDING_REFERENCE_LABEL, XUI_DOC_SOURCE_BINDING_REFERENCE_DESTINATION,
    XUI_DOC_SOURCE_BINDING_REFERENCE_TITLE, XUI_DOC_SOURCE_BINDING_BREAK_TRAILING,
    XUI_DOC_SOURCE_BINDING_BREAK_HARD_MARKER, XUI_DOC_SOURCE_BINDING_BREAK_LINE_ENDING,
    XUI_DOC_SOURCE_BINDING_DEFINITION_SYNTAX, XUI_DOC_SOURCE_BINDING_DOCUMENT_SYNTAX
};
enum xui_doc_source_binding_metadata {
    XUI_DOC_SOURCE_BINDING_COLUMNS = 1u, XUI_DOC_SOURCE_BINDING_TABLE = 2u,
    XUI_DOC_SOURCE_BINDING_SOURCE_ONLY = 4u
};
typedef struct xui_doc_source_binding_t {
    uint32_t iSize, iOriginKind;
    xui_doc_source_token_t tRange;
    /* Ordinal in the retained node's provenance/indent/table vector. For
     * DEFINITION_SYNTAX it is the definition's concrete syntax ordinal, with
     * the definition ordinal in the enclosing record's iOriginIndex.
     * DOCUMENT_SYNTAX uses an ordinary arena's syntax ordinal, with that
     * source-ordered arena ordinal in the enclosing record's iOriginIndex.
     * SOURCE_ONLY has original ranges and optional segment kind but no
     * semantic text offsets. Other unsupported ordinals are UINT64_MAX. */
    uint64_t iOriginIndex;
    uint32_t iMetadata, iMarkerFlags, iRow, iOrdinal;
    /* Parser logical columns, not byte offsets. Columns belong to the parser
     * input containing this record (footnote bodies can be dedented input).
     * A partly consumed Tab may have end > content. Unsupported columns and
     * blank-code content columns are UINT64_MAX. */
    uint64_t iStartColumn, iMarkerColumn, iContentColumn, iEndColumn;
} xui_doc_source_binding_t;
/* Nonzero stops traversal and returns XUI_DOC_ERROR_CANCELLED. The borrowed
 * binding pointer lasts only for the callback; copying its fields is safe. */
typedef int (*xui_doc_source_binding_proc)(void* user, uint64_t ordinal,
    const xui_doc_source_binding_t* binding);
typedef int (*xui_doc_source_cancel_proc)(void* user);
typedef struct xui_doc_source_index_desc_t {
    uint32_t iSize, iReserved;
    /* Peak working allocations, including headers but excluding the retained
     * snapshot. 0 selects 64 MiB; UINT64_MAX removes this additional cap. */
    uint64_t iMaxWorkingBytes;
    /* Called only during construction; return nonzero to cancel. The caller
     * may build on a worker thread. Callback storage is not retained. */
    xui_doc_source_cancel_proc onCancel;
    void* pCancelUser;
} xui_doc_source_index_desc_t;
typedef struct xui_doc_source_index_info_t {
    uint32_t iSize, iReserved;
    uint64_t iDocumentId, iRevision, iSourceBytes, iRecordCount, iTokenCount;
    uint64_t iUnclassifiedBytes, iOpaqueBytes;
    uint64_t iAllocatedBytes, iPeakWorkingBytes, iBuildSteps, iSourceBytesRead, iAnnotationCount;
    uint64_t iBindingCount;
} xui_doc_source_index_info_t;
enum xui_doc_node_flags {
    XUI_DOC_ORDERED = 1u, XUI_DOC_TASK = 2u, XUI_DOC_CHECKED = 4u,
    XUI_DOC_HEADER = 8u, XUI_DOC_BLOCK = 16u, XUI_DOC_TIGHT = 32u,
    XUI_DOC_SPACING_EXPLICIT = 64u, /* fParagraphSpacing overrides the renderer's default, including zero. */
    XUI_DOC_TEXT_COLOR_EXPLICIT_ZERO = 128u, /* iTextColor == 0 is a literal transparent color. */
    XUI_DOC_BACKGROUND_COLOR_EXPLICIT_ZERO = 256u, /* iBackgroundColor == 0 is a literal transparent color. */
    XUI_DOC_TEXT_COLOR_CURRENT = 512u, /* CSS color:currentColor; resolve the inherited foreground. */
    XUI_DOC_BACKGROUND_COLOR_CURRENT = 1024u, /* CSS background-color:currentColor; resolve at paint time. */
    XUI_DOC_ALIGNMENT_EXPLICIT_LEFT = 2048u /* iAlignment == 0 means left, overriding ancestor alignment. */
};

typedef void* (*xui_doc_alloc_proc)(void* user, size_t bytes);
typedef void (*xui_doc_free_proc)(void* user, void* pointer);

typedef struct xui_doc_desc_t {
    uint32_t iSize;
    uint32_t iProfile;
    uint32_t iMarkdownDialect; /* 0 selects EXTENDED: GFM + math, footnotes, admonitions, front matter, Mermaid. */
    uint32_t iHistoryLimit;   /* 0 selects 256; bDisableHistory explicitly opts out. */
    uint64_t iHistoryMaxBytes; /* 0 selects 64 MiB; UINT64_MAX removes the byte cap. */
    int bDisableHistory;
    uint64_t iMaxTextBytes;  /* 0 selects 256 MiB. */
    uint64_t iMaxNodes;      /* Semantic nodes; 0 selects 1,000,000. */
    xui_doc_alloc_proc onAlloc;
    xui_doc_free_proc onFree;
    void* pAllocatorUser;   /* Must outlive every handle/snapshot/prepare using it.
                            * Callbacks must be thread-safe when readers release
                            * snapshots or prepare runs on worker threads. */
    uint64_t iMaxSyntaxRecords; /* Records collected in one Markdown parser input or arena;
                                * independent of semantic nodes. 0 selects 8,000,000.
                                * Storage grows with actual input, not the configured cap.
                                * Retained document storage is diagnosed separately. */
} xui_doc_desc_t;

typedef struct xui_doc_attributes_t {
    uint32_t iMarks;
    uint32_t iFlags;
    uint32_t iHeadingLevel;
    uint32_t iAlignment;    /* 0 inherits (default left); explicit-left flag overrides, 1 center, 2 right, 3 justified. */
    uint32_t iRowSpan;
    uint32_t iColumnSpan;
    uint64_t iListStart;
    uint32_t iTextColor;       /* RGBA; zero uses the default unless TEXT_COLOR_EXPLICIT_ZERO or TEXT_COLOR_CURRENT is set. */
    uint32_t iBackgroundColor; /* RGBA; zero is absent unless BACKGROUND_COLOR_EXPLICIT_ZERO or BACKGROUND_COLOR_CURRENT is set. */
    /* Layout values are finite logical units in [0, 1000000]. */
    float fFontSize;
    float fWidth;
    float fHeight;
    float fParagraphSpacing;
    char sFontFamily[64];
    /* BCP 47 natural language, at most 255 bytes. NULL/empty inherits;
     * "und" explicitly resets inheritance. Inputs are copied; snapshot
     * results borrow storage until that snapshot is released. */
    const char* sLanguage;
} xui_doc_attributes_t;
enum xui_doc_text_style_field {
    XUI_DOC_TEXT_STYLE_COLOR = 1u,
    XUI_DOC_TEXT_STYLE_BACKGROUND = 2u,
    XUI_DOC_TEXT_STYLE_FONT_SIZE = 4u,
    XUI_DOC_TEXT_STYLE_FONT_FAMILY = 8u,
    XUI_DOC_TEXT_STYLE_LANGUAGE = 16u
};
typedef struct xui_doc_text_style_t {
    uint32_t iSize;
    uint32_t iExplicitFields; /* COLOR/BACKGROUND bits make zero literal; query also sets bits for nonzero colors. */
    uint32_t iTextColor;       /* RGBA; zero clears unless COLOR is in iExplicitFields. */
    uint32_t iBackgroundColor; /* RGBA; zero clears unless BACKGROUND is in iExplicitFields. */
    float fFontSize;           /* Logical units; zero restores the theme size. */
    char sFontFamily[64];      /* UTF-8; empty restores the default family. */
    uint32_t iCurrentColorFields; /* COLOR/BACKGROUND bits set the CSS currentColor keyword. */
    /* Owned query/pending-style value; empty clears the local override. */
    char sLanguage[256];
} xui_doc_text_style_t;
typedef struct xui_doc_text_style_query_t {
    uint32_t iSize;
    int bHasText;
    uint32_t iMixedFields; /* Fields differing within a nonempty selection. */
    xui_doc_text_style_t tStyle; /* First selected run, or caret context. */
} xui_doc_text_style_query_t;
enum xui_doc_block_style_field {
    XUI_DOC_BLOCK_STYLE_ALIGNMENT = 1u,
    XUI_DOC_BLOCK_STYLE_SPACING = 2u
};
typedef struct xui_doc_block_style_t {
    uint32_t iSize;
    uint32_t iAlignment; /* 0 left, 1 center, 2 right, 3 justified. */
    float fParagraphSpacing; /* Logical units after the block; zero is valid when explicit. */
    int bSpacingExplicit; /* 0 restores the renderer's default gap. */
    int bAlignmentInherited; /* With ALIGNMENT and zero, restore the ancestor's alignment. */
} xui_doc_block_style_t;

typedef struct xui_doc_node_desc_t {
    uint32_t iSize;
    uint32_t iKind;
    xui_doc_attributes_t tAttributes;
    const char* sText;
    uint64_t iTextBytes;
    const char* sResource;  /* URI or extension identifier; copied. */
    const char* sInfo;      /* Code language, extension data type, or label. */
    const char* sTitle;
    const char* sLinkTarget; /* IMAGE only: enclosing hyperlink destination. */
    const char* sLinkTitle;  /* IMAGE only: enclosing hyperlink title. */
    /* XUI_DOC_EXTENSION only. sInfo names the payload type; unknown types and
     * versions are retained without interpretation by the Document core. */
    const void* pExtensionPayload;
    uint64_t iExtensionPayloadBytes;
    uint32_t iExtensionVersion; /* Zero selects version 1. */
    int bExtensionRequired;
} xui_doc_node_desc_t;

typedef struct xui_doc_node_info_t {
    uint32_t iSize;
    uint32_t iKind;
    xui_doc_node_id iId;
    xui_doc_node_id iParentId;
    uint64_t iChildCount;
    uint64_t iTextBytes;
    xui_doc_attributes_t tAttributes;
    const char* sResource;
    const char* sInfo;
    const char* sTitle;
    const char* sLinkTarget; /* Borrowed from snapshot; IMAGE only. */
    const char* sLinkTitle;  /* Borrowed from snapshot; IMAGE only. */
    uint64_t iSourceStart;
    uint64_t iSourceEnd;
    int bSourceExact;
    /* Complete block syntax, including markers and terminating line ending.
     * UINT64_MAX when no block syntax range exists (rich/inline nodes). */
    uint64_t iSyntaxStart;
    uint64_t iSyntaxEnd;
    uint64_t iSourceSegmentCount;
    uint64_t iExtensionPayloadBytes;
    uint32_t iExtensionVersion;
    int bExtensionRequired;
} xui_doc_node_info_t;

typedef struct xui_doc_markdown_loss_t {
    uint32_t iSize;
    uint32_t iNodeKind;
    xui_doc_node_id iNodeId;
    uint32_t iReasons; /* xui_doc_markdown_loss_reason bit set. */
    uint32_t iReserved;
} xui_doc_markdown_loss_t;
enum xui_doc_rich_conversion_loss_reason {
    XUI_DOC_RICH_LOSS_SOURCE_SYNTAX = 1u, /* Raw Markdown bytes and spelling. */
    XUI_DOC_RICH_LOSS_REFERENCE_DEFINITIONS = 2u /* Reference definitions are source-only. */
};
typedef struct xui_doc_rich_conversion_report_t {
    uint32_t iSize;
    uint32_t iReasons; /* xui_doc_rich_conversion_loss_reason bit set. */
    uint64_t iSourceBytes;
    uint64_t iReferenceDefinitions;
    uint64_t iInlineSyntaxEntries;
} xui_doc_rich_conversion_report_t;

typedef struct xui_doc_position_t {
    uint32_t iSize;
    uint32_t iKind;
    uint64_t iDocumentId;
    uint64_t iRevision;
    xui_doc_node_id iNodeId;
    uint64_t iOffset;
    uint32_t iAffinity;
    uint32_t iReserved;
    uint64_t iInputGeneration; /* 0 for committed content; otherwise an exact Prepare input projection. */
} xui_doc_position_t;
typedef struct xui_doc_range_t { xui_doc_position_t tAnchor, tCaret; } xui_doc_range_t;

typedef struct xui_doc_txn_desc_t {
    uint32_t iSize;
    uint32_t iDomain;
    uint64_t iBaseRevision; /* 0 means the current revision. */
    uint64_t iOrigin;
    uint64_t iGroup;        /* Groups consecutive commits with the same origin and domain. */
} xui_doc_txn_desc_t;

typedef struct xui_doc_operation_t {
    /* SPLIT with iParentId == 0 splits text; nonzero splits a container at a
     * child gap. MERGE joins containers at iOffset. Inspect iMappingFlags
     * before applying child MOVE records to gaps a second time. */
    uint32_t iKind;
    uint32_t iFlags;
    xui_doc_node_id iNodeId;
    xui_doc_node_id iParentId;
    xui_doc_node_id iOtherNodeId;
    uint64_t iOffset;
    uint64_t iOldLength;
    uint64_t iNewLength;
    uint32_t iMappingFlags;
    uint32_t iReserved;
} xui_doc_operation_t;

typedef struct xui_doc_change_info_t {
    uint32_t iSize;
    uint32_t iFlags;
    uint64_t iBeforeRevision;
    uint64_t iAfterRevision;
    uint64_t iOrigin;
    uint64_t iInputGeneration; /* Prepare publication only; ordinary writes and Undo/Redo use zero. */
    uint64_t iOperationCount;
    const xui_doc_operation_t* pOperations;
    int bUndo;
    uint32_t iDomain;       /* Authoritative operations: SEMANTIC or SOURCE. */
} xui_doc_change_info_t;

typedef struct xui_doc_stats_t {
    uint32_t iSize;
    uint64_t iLiveBytes;
    uint64_t iPeakBytes;
    uint64_t iAllocations;
    uint64_t iNodes;
    uint64_t iUndoCount;
    uint64_t iRedoCount;
    uint64_t iMarkdownParses; /* Parse requests, including full-parse fallback within one request. */
    uint64_t iMarkdownParsedBytes; /* Sum of attempted window/full source bytes; fallback may count both. */
    uint64_t iPreparedPublishes; /* Publish used precomputed storage accounting. */
    uint64_t iPreparedStorageUpdates; /* Allocation headers updated by those publications. */
    uint64_t iCurrentBytes;
    uint64_t iHistoryBytes;   /* Unique history storage beyond the current state, plus history records/ops. */
    uint64_t iHistoryMaxBytes;
    uint64_t iSnapshotCount;  /* Owning handles; Retain does not create another handle. */
    uint64_t iMarkdownIncrementalParses; /* Completed independent-block candidates; includes unpublished candidates. */
    uint64_t iPreparedAccountingVisits; /* Reachability visits in successful prepared accounting plans. */
} xui_doc_stats_t;
typedef struct xui_doc_memory_stats_t {
    uint32_t iSize;
    uint64_t iLiveBytes, iCurrentBytes, iHistoryBytes;
    uint64_t iSnapshotBytes, iSnapshotAdditionalBytes, iSnapshotCount, iSnapshotHandleBytes;
    uint64_t iOtherBytes;
} xui_doc_memory_stats_t;

typedef void (*xui_doc_change_proc)(xui_document document,
    xui_document_change_set change, void* user);

/* Lifetime, immutable reads, subscriptions and diagnostics. */
#if XUI_ENABLE_DOCUMENT
XUI_API int xuiDocumentCreate(const xui_doc_desc_t* desc, xui_document* out);
/* Retain a shared Document handle. */
XUI_API void xuiDocumentRetain(xui_document document);
/* Release a shared Document handle. */
XUI_API void xuiDocumentRelease(xui_document document);
/* Return the current committed revision. */
XUI_API uint64_t xuiDocumentGetRevision(xui_document document);
/* Return this Document's stable identity. */
XUI_API uint64_t xuiDocumentGetIdentity(xui_document document);
/* Return the Rich or Markdown profile. */
XUI_API int xuiDocumentGetProfile(xui_document document);
/* Return the selected Markdown dialect. */
XUI_API int xuiDocumentGetMarkdownDialect(xui_document document);
/* Acquire an immutable snapshot of committed content. */
XUI_API int xuiDocumentAcquireSnapshot(xui_document document, xui_document_snapshot* out);
/* Retain an immutable snapshot handle. */
XUI_API void xuiDocumentSnapshotRetain(xui_document_snapshot snapshot);
/* Release an immutable snapshot handle. */
XUI_API void xuiDocumentSnapshotRelease(xui_document_snapshot snapshot);
/* Return the source Document identity. */
XUI_API uint64_t xuiDocumentSnapshotGetIdentity(xui_document_snapshot snapshot);
/* Return the snapshot's committed revision. */
XUI_API uint64_t xuiDocumentSnapshotGetRevision(xui_document_snapshot snapshot);
/* Read a node by stable ID from this snapshot. */
XUI_API int xuiDocumentSnapshotGetNode(xui_document_snapshot snapshot, xui_doc_node_id id, xui_doc_node_info_t* out);
/* Read a parent's child ID by zero-based index. */
XUI_API int xuiDocumentSnapshotGetChild(xui_document_snapshot snapshot, xui_doc_node_id parent, uint64_t index, xui_doc_node_id* out);
/* Ordered text-to-source segments. Unknown source endpoints are UINT64_MAX. */
XUI_API int xuiDocumentSnapshotGetSourceSegment(xui_document_snapshot snapshot, xui_doc_node_id node, uint64_t index, xui_doc_source_segment_t* out);
/* Partition the raw Markdown line containing a source byte offset. Offset at
 * EOF, or an empty source, returns NOT_FOUND; continue at iLineEnd to iterate. */
XUI_API int xuiDocumentSnapshotGetSourceLine(xui_document_snapshot snapshot, uint64_t source_offset, xui_doc_source_line_t* out);
/* Read a parsed heading, code, rule, list-item, quote or table marker by NodeId.
 * Other Markdown nodes return NOT_FOUND; Rich snapshots return UNSUPPORTED. */
XUI_API int xuiDocumentSnapshotGetBlockSyntax(xui_document_snapshot snapshot, xui_doc_node_id node, xui_doc_block_syntax_t* out);
/* Read parser-confirmed Markdown line-break trivia by SoftBreak/HardBreak
 * NodeId. Other nodes return NOT_FOUND; Rich snapshots return UNSUPPORTED. */
XUI_API int xuiDocumentSnapshotGetBreakSyntax(xui_document_snapshot snapshot,
    xui_doc_node_id node, xui_doc_break_syntax_t* out);
/* Read one explicit block-quote '>' in source order, including its opener.
 * The count is returned by GetBlockSyntax; lazy lines have no token. */
XUI_API int xuiDocumentSnapshotGetQuotePrefix(xui_document_snapshot snapshot,
    xui_doc_node_id node, uint64_t index, uint64_t* start, uint64_t* end);
/* Read one parser-confirmed list-item continuation indentation and the
 * per-layer logical columns in source order. The opening item line and lazy
 * continuation lines have no record; GetBlockSyntax returns the count. */
XUI_API int xuiDocumentSnapshotGetListContinuationIndent(xui_document_snapshot snapshot,
    xui_doc_node_id item, uint64_t index, xui_doc_list_indent_t* out);
/* Read one retained indented-code line's raw indentation and logical columns.
 * GetBlockSyntax returns the count; fenced code and other nodes return NOT_FOUND. */
XUI_API int xuiDocumentSnapshotGetCodeIndent(xui_document_snapshot snapshot,
    xui_doc_node_id code, uint64_t index, xui_doc_code_indent_t* out);
/* Read one parser-confirmed GFM table token in source order. Row 0 is the
 * header, row 1 the separator, and rows 2+ are body rows. Ordinal counts
 * tokens of the same kind within a row. For an underline token, flags record
 * its optional left/right alignment colons. Count comes from GetBlockSyntax. */
XUI_API int xuiDocumentSnapshotGetTableToken(xui_document_snapshot snapshot,
    xui_doc_node_id table, uint64_t index, xui_doc_table_token_t* out);
/* Read counts for the snapshot's Markdown source metadata. */
XUI_API int xuiDocumentSnapshotGetSourceInfo(xui_document_snapshot snapshot, xui_doc_source_info_t* out);
/* Read one indexed Markdown inline-syntax record. */
XUI_API int xuiDocumentSnapshotGetInlineSyntax(xui_document_snapshot snapshot, uint64_t index, xui_doc_inline_syntax_t* out);
/* Read one indexed reference definition. */
XUI_API int xuiDocumentSnapshotGetReferenceDefinition(xui_document_snapshot snapshot, uint64_t index, xui_doc_reference_definition_t* out);
/* An unresolved reference candidate uses the inline-syntax range fields:
 * source encloses the queried [label], content is the raw label without
 * brackets, and parent/definition are UINT64_MAX. Full-reference forms may
 * expose more than one candidate because fallback lookup is also possible. */
XUI_API int xuiDocumentSnapshotGetReferenceCandidate(xui_document_snapshot snapshot, uint64_t index, xui_doc_inline_syntax_t* out);
/* Construction publishes only a complete index. Failure clears *out and
 * leaves source, revision, identities, save point and history unchanged. */
XUI_API int xuiDocumentSnapshotCreateSourceIndex(xui_document_snapshot snapshot,
    const xui_doc_source_index_desc_t* desc, xui_document_source_index* out);
XUI_API void xuiDocumentSourceIndexRetain(xui_document_source_index index);
XUI_API void xuiDocumentSourceIndexRelease(xui_document_source_index index);
XUI_API int xuiDocumentSourceIndexGetInfo(xui_document_source_index index,
    xui_doc_source_index_info_t* out);
XUI_API int xuiDocumentSourceIndexGetRecord(xui_document_source_index index,
    uint64_t ordinal, xui_doc_source_record_t* out);
XUI_API int xuiDocumentSourceIndexGetToken(xui_document_source_index index,
    uint64_t ordinal, xui_doc_source_token_t* out);
XUI_API int xuiDocumentSourceIndexGetBinding(xui_document_source_index index,
    uint64_t ordinal, xui_doc_source_binding_t* out);
/* Visit every overlapping complete binding in source/ordinal order without
 * allocation. [start,end) excludes anchors at end. start == end is a caret:
 * visit ranges containing start and zero-length anchors exactly at start;
 * EOF has anchors only. Bounds beyond EOF or start > end are invalid.
 * *visited (optional) counts callbacks, including the one stopping traversal.
 * Calls retain the index during callbacks; nested read/query calls are safe.
 * The caller must own a reference when entering the function. */
XUI_API int xuiDocumentSourceIndexVisitBindings(xui_document_source_index index,
    uint64_t start, uint64_t end, xui_doc_source_binding_proc onBinding,
    void* user, uint64_t* visited);
/* Binary search the unique token containing offset. EOF returns NOT_FOUND;
 * an offset beyond EOF is invalid. Failure writes UINT64_MAX to *ordinal. */
XUI_API int xuiDocumentSourceIndexFindToken(xui_document_source_index index,
    uint64_t offset, uint64_t* ordinal);
/* Read original bytes through the retained snapshot, also after Document and
 * the caller's snapshot have been released. No whole-source copy is needed. */
XUI_API int xuiDocumentSourceIndexReadSource(xui_document_source_index index,
    uint64_t offset, void* buffer, uint64_t bytes);
/* Copy functions report the required byte length (without a terminator).
 * A NULL buffer queries the length. A supplied buffer needs length+1 bytes. */
XUI_API int xuiDocumentSnapshotCopyText(xui_document_snapshot snapshot, xui_doc_node_id node, char* buffer, uint64_t capacity, uint64_t* length);
/* Copies opaque extension bytes exactly; capacity need only equal length.
 * A NULL buffer queries length. Non-extension nodes return INVALID_ARGUMENT. */
XUI_API int xuiDocumentSnapshotCopyExtensionPayload(xui_document_snapshot snapshot,
    xui_doc_node_id node, void* buffer, uint64_t capacity, uint64_t* length);
/* Copy the complete Markdown source into a caller buffer. */
XUI_API int xuiDocumentSnapshotCopySource(xui_document_snapshot snapshot, char* buffer, uint64_t capacity, uint64_t* length);
/* Read a byte range of one node's text. */
XUI_API int xuiDocumentSnapshotReadText(xui_document_snapshot snapshot, xui_doc_node_id node, uint64_t offset, void* buffer, uint64_t bytes);
/* Read a byte range of the Markdown source. */
XUI_API int xuiDocumentSnapshotReadSource(xui_document_snapshot snapshot, uint64_t offset, void* buffer, uint64_t bytes);
/* Register a change callback and return its subscription token. */
XUI_API int xuiDocumentSubscribe(xui_document document, xui_doc_change_proc callback, void* user, uint64_t* token);
/* Remove the subscription identified by its token. */
XUI_API void xuiDocumentUnsubscribe(xui_document document, uint64_t token);
/* Read allocation, history and parse diagnostics. */
XUI_API int xuiDocumentGetStats(xui_document document, xui_doc_stats_t* out);
/* Detailed diagnostics walk the union of retained snapshot roots without
 * allocating. SnapshotBytes includes shared current/history data; Additional
 * excludes current/history, but may also be retained by external ChangeSets or
 * transactions. OtherBytes covers those handles, candidates and temporaries.
 * All byte counts include Document allocation headers, not system-heap overhead.
 * Owner-thread call; concurrent snapshot reads/Retain/Release are supported. */
XUI_API int xuiDocumentGetMemoryStats(xui_document document, xui_doc_memory_stats_t* out);
/* The same allocator-wide diagnostics through a retained snapshot, including
 * after its live Document is released. Safe on snapshot reader threads. */
XUI_API int xuiDocumentSnapshotGetMemoryStats(xui_document_snapshot snapshot, xui_doc_memory_stats_t* out);
/* Combined Undo+Redo limits. 0 selects the defaults above. Evict the farthest
 * history step first (oldest Undo on ties); an oversized single step can be
 * discarded while its edit still commits. External snapshots stay valid.
 * Both calls are allocation-free, preserve content/revision/saved state, and
 * return BUSY during a writer or change callback. */
XUI_API int xuiDocumentSetHistoryLimits(xui_document document, uint32_t max_steps, uint64_t max_bytes);
/* Discard Undo and Redo history without changing content. */
XUI_API int xuiDocumentClearHistory(xui_document document);
/* Report whether committed content differs from the saved state. */
XUI_API int xuiDocumentIsDirty(xui_document document);
/* Mark the supplied snapshot as successfully saved. */
XUI_API int xuiDocumentMarkSaved(xui_document document, xui_document_snapshot saved);
/* UTF-8 paths. Snapshot export writes an exclusive temporary file in the target
 * directory and atomically replaces the target using XRT's filesystem layer.
 * Export never changes saved state. SaveFile accepts native or Markdown only.
 * For asynchronous saving, export a retained snapshot and MarkSaved that exact
 * snapshot on the document owner thread only after successful publication. */
XUI_API int xuiDocumentSnapshotExportFile(xui_document_snapshot snapshot, const char* path, uint32_t format);
/* Save committed content and update the saved state on success. */
XUI_API int xuiDocumentSaveFile(xui_document document, const char* path, uint32_t format);
/* Returns a new, clean document without an import undo entry. max_file_bytes=0
 * selects a 256 MiB input cap. Native and Markdown are accepted input formats. */
XUI_API int xuiDocumentOpenFile(const char* path, uint32_t format, const xui_doc_desc_t* desc, uint64_t max_file_bytes, xui_document* out);
/* Inspect a Rich snapshot before explicitly converting it to Markdown.
 * Reports one entry per affected node in document order; a root ROUNDTRIP
 * entry covers syntax/parser differences not attributable to one attribute.
 * Query with losses=NULL/capacity=0 first; total is always the required count.
 * A too-small supplied array receives its prefix and returns BUFFER_TOO_SMALL.
 * This read-only operation does not change the source or its saved state. */
XUI_API int xuiDocumentSnapshotAnalyzeMarkdownConversion(
    xui_document_snapshot snapshot, uint32_t dialect,
    xui_doc_markdown_loss_t* losses, uint64_t capacity, uint64_t* total);
/* Explicit lossless Rich -> Markdown conversion. On any reported loss or
 * semantic round-trip mismatch, returns UNREPRESENTABLE and no document.
 * The independent result is clean, editable and retains its Markdown source. */
XUI_API int xuiDocumentSnapshotConvertToMarkdown(
    xui_document_snapshot snapshot, uint32_t dialect, xui_document* out);
/* The caller explicitly authorizes selected losses from the preceding
 * analysis: MARK, TEXT_STYLE, BLOCK_STYLE, GEOMETRY and LANGUAGE. Unsupported
 * nodes, dialect features, metadata, empty blocks, table content/merges and
 * semantic round-trip differences still reject atomically. accepted_reasons
 * must contain only these five bits. The input snapshot remains unchanged. */
XUI_API int xuiDocumentSnapshotConvertToMarkdownWithPolicy(
    xui_document_snapshot snapshot, uint32_t dialect,
    uint32_t accepted_reasons, xui_document* out);
/* Markdown -> Rich preserves every semantic node/mark/resource in an
 * independent editable Document. Source spelling and reference definitions
 * have no Rich-profile representation. Analyze first; conversion requires
 * explicit acceptance of every reported reason and never changes the input. */
XUI_API int xuiDocumentSnapshotAnalyzeRichConversion(
    xui_document_snapshot snapshot, xui_doc_rich_conversion_report_t* report);
/* Create an independent Rich Document after accepting reported losses. */
XUI_API int xuiDocumentSnapshotConvertToRich(
    xui_document_snapshot snapshot, uint32_t accepted_reasons, xui_document* out);

/* Transactions are all-or-nothing. An operation failure poisons the transaction.
 * Commit on a failed transaction publishes nothing. Release implies Abort. */
XUI_API int xuiDocumentBeginTransaction(xui_document document, const xui_doc_txn_desc_t* desc, xui_document_transaction* out);
/* Read a node from the current transaction draft. */
XUI_API int xuiDocumentTxnGetNode(xui_document_transaction transaction, xui_doc_node_id id, xui_doc_node_info_t* out);
/* Insert a schema-valid node into the transaction draft. */
XUI_API int xuiDocumentTxnInsertNode(xui_document_transaction transaction, xui_doc_node_id parent, uint64_t child_index, const xui_doc_node_desc_t* desc, xui_doc_node_id* out);
/* Copy one complete subtree from a retained snapshot into this transaction.
 * Every copied node receives a new ID in the target document; the source
 * snapshot stays unchanged. The root is not a copyable subtree. Markdown
 * targets rewrite and validate the resulting semantic tree before commit.
 * Unsupported target-profile attributes are rejected atomically. */
XUI_API int xuiDocumentTxnCopySubtree(xui_document_transaction transaction,
    xui_document_snapshot source, xui_doc_node_id source_node,
    xui_doc_node_id target_parent, uint64_t child_index,
    xui_doc_node_id* copied_root);
/* Copy a nonempty semantic range as structural children of its lowest common
 * ancestor. A range inside one Text node yields a clipped Text; across runs it
 * yields clipped inline nodes; across blocks it yields clipped block trees.
 * target_parent must accept those root kinds. Every copied node gets a new ID.
 * Partial table/row/cell objects and partial non-Text payloads are unsupported;
 * use whole-subtree or table-matrix operations for those objects. */
XUI_API int xuiDocumentTxnCopyRange(xui_document_transaction transaction,
    xui_document_snapshot source, const xui_doc_range_t* range,
    xui_doc_node_id target_parent, uint64_t child_index,
    xui_doc_node_id* copied_first, uint64_t* copied_count);
/* A native fragment owns an immutable, standalone Rich semantic snapshot.
 * Capture accepts a nonempty semantic range, including inline or block roots;
 * its source Document and Snapshot may be released afterward. Serialization is
 * versioned binary header plus the native Document JSON and is clipboard-safe.
 * Insert keeps one target transaction and assigns fresh target-local NodeIds.
 * The target parent must accept the fragment's root node kinds. */
XUI_API int xuiDocumentFragmentCreateRange(xui_document_snapshot source,
    const xui_doc_range_t* range, xui_document_fragment* out);
/* Retain an immutable native fragment. */
XUI_API void xuiDocumentFragmentRetain(xui_document_fragment fragment);
/* Release an immutable native fragment. */
XUI_API void xuiDocumentFragmentRelease(xui_document_fragment fragment);
/* Serialize a fragment into a caller-owned native buffer. */
XUI_API int xuiDocumentFragmentSerialize(xui_document_fragment fragment,
    char** out, uint64_t* bytes);
/* Export only the selected fragment roots as UTF-8 HTML. Capture wrappers
 * used for schema validation (for example, a Paragraph around inline roots)
 * are not emitted. The result uses xuiDocumentFreeBuffer. */
XUI_API int xuiDocumentFragmentExportHtml(xui_document_fragment fragment,
    char** out, uint64_t* bytes);
/* Parse a UTF-8 HTML fragment into the unified Rich schema. Active content is
 * discarded; supported block/inline structure becomes an independent native
 * fragment that can be inserted into Rich or representable Markdown targets.
 * Unsupported presentation details may be dropped. */
XUI_API int xuiDocumentFragmentImportHtml(const char* html, uint64_t bytes,
    xui_document_fragment* out);
/* Deserialize a native buffer into an independent fragment. */
XUI_API int xuiDocumentFragmentDeserialize(const char* data, uint64_t bytes,
    xui_document_fragment* out);
/* Copy a fragment's roots into the target transaction. */
XUI_API int xuiDocumentTxnInsertFragment(xui_document_transaction transaction,
    xui_document_fragment fragment, xui_doc_node_id target_parent,
    uint64_t child_index, xui_doc_node_id* copied_first,
    uint64_t* copied_count);
/* Replace a semantic selection (or insert at a caret) with a native fragment.
 * Inline roots split a Text run as needed; block roots split a paragraph and
 * enter its parent container. The whole operation is one transaction, with a
 * resulting caret after the inserted roots. Incompatible structures reject. */
XUI_API int xuiDocumentTxnReplaceRangeWithFragment(
    xui_document_transaction transaction, const xui_doc_range_t* range,
    xui_document_fragment fragment, xui_doc_position_t* caret);
/* Delete a node from the transaction draft. */
XUI_API int xuiDocumentTxnDeleteNode(xui_document_transaction transaction, xui_doc_node_id node);
/* Move a node to a parent and child index in the draft. */
XUI_API int xuiDocumentTxnMoveNode(xui_document_transaction transaction, xui_doc_node_id node, xui_doc_node_id parent, uint64_t child_index);
/* Replace a UTF-8 byte range in a text node. */
XUI_API int xuiDocumentTxnReplaceText(xui_document_transaction transaction, xui_doc_node_id node, uint64_t start, uint64_t end, const char* utf8, uint64_t bytes);
/* Replace a node's semantic and presentation attributes. */
XUI_API int xuiDocumentTxnSetAttributes(xui_document_transaction transaction, xui_doc_node_id node, const xui_doc_attributes_t* attributes);
/* Set a node's resource, info and title strings. */
XUI_API int xuiDocumentTxnSetResource(xui_document_transaction transaction, xui_doc_node_id node, const char* resource, const char* info, const char* title);
/* Replaces an extension's opaque payload and version in one root transaction. */
XUI_API int xuiDocumentTxnSetExtensionPayload(xui_document_transaction transaction,
    xui_doc_node_id node, const void* payload, uint64_t bytes,
    uint32_t version, int required);
#endif
typedef struct xui_doc_image_desc_t {
    uint32_t iSize;
    const char* sResource; /* Non-NULL UTF-8 URI or surface name; empty is valid only for Markdown. */
    const char* sAlt;      /* UTF-8 text; may be NULL only when iAltBytes is zero. */
    uint64_t iAltBytes;
    const char* sTitle;    /* Optional UTF-8 title. */
    const char* sLinkTarget; /* NULL: unlinked; empty string: linked to the document. */
    const char* sLinkTitle;  /* Optional title of the enclosing hyperlink. */
    float fWidth, fHeight; /* Logical units; zero uses intrinsic size. Markdown requires both zero. */
} xui_doc_image_desc_t;
/* Replace a semantic selection with one inline image (or create a paragraph
 * at a structural gap), as one transaction. Returns its stable node ID and a
 * caret immediately after it. Markdown rewrites and verifies the affected
 * syntax; an unrepresentable edit fails without publishing a partial result. */
#if XUI_ENABLE_DOCUMENT
XUI_API int xuiDocumentTxnInsertImage(xui_document_transaction transaction, const xui_doc_range_t* range,
    const xui_doc_image_desc_t* image, xui_doc_node_id* image_id, xui_doc_position_t* caret);
/* Insert MATH, DIAGRAM or HTML source as one semantic object. MATH is inline
 * and accepts XUI_DOC_BLOCK for display math; HTML accepts that flag for a
 * block object; DIAGRAM is always a block. Inline objects may replace a
 * selection. Block objects require a collapsed caret and split a paragraph
 * only when inserted in its interior. Markdown requires EXTENDED for MATH or
 * DIAGRAM; fenced/block source gains a final LF when missing, then the parsed
 * result is verified before committing. */
XUI_API int xuiDocumentTxnInsertObject(xui_document_transaction transaction,
    const xui_doc_range_t* range, uint32_t kind, uint32_t flags,
    const char* utf8, uint64_t bytes, xui_doc_node_id* object_id,
    xui_doc_position_t* caret);
/* Insert a fenced code block or thematic rule at a collapsed semantic caret.
 * A caret inside a paragraph splits it first. The code language is a single
 * UTF-8 token; Markdown code content gains a final LF when missing. */
XUI_API int xuiDocumentTxnInsertCodeBlock(xui_document_transaction transaction,
    const xui_doc_range_t* range, const char* language, const char* utf8,
    uint64_t bytes, xui_doc_node_id* code_id, xui_doc_position_t* caret);
/* Change the language token of an existing code block, keeping its NodeId and
 * body. Markdown fenced code preserves its other info bytes and spelling.
 * NULL or empty language clears it if representable; retained metadata that
 * would become a language returns UNREPRESENTABLE without publication. */
XUI_API int xuiDocumentTxnSetCodeBlockLanguage(xui_document_transaction transaction,
    xui_doc_node_id code_id, const char* language);
/* Insert a thematic rule at a collapsed semantic caret. */
XUI_API int xuiDocumentTxnInsertRule(xui_document_transaction transaction,
    const xui_doc_range_t* range, xui_doc_node_id* rule_id,
    xui_doc_position_t* caret);
/* Wrap complete blocks in one quote node. Text endpoints expand to their
 * paragraph/heading blocks. Both edges are lifted to the nearest common
 * parent that accepts a quote, splitting any intervening Quote/List/ListItem
 * containers at child gaps. Stable child anchors survive both edge splits,
 * including a middle range in one list and partial list-item bodies. Ordered
 * suffix lists continue their numbering; only a split item's prefix retains
 * its task/check marker. CanExecute uses the same structural preflight.
 * Markdown retains raw unselected edge prefixes and representable suffixes
 * through nested containers. A partial item suffix needs a new list opener
 * and can rewrite the affected container; the complete candidate must reparse
 * to the requested tree before publication. At a root quote boundary whose
 * edges split only quotes, wrapping preserves all selected syntax bytes and
 * adds quote prefixes/separators; link definitions (including unused duplicate
 * and multiline definitions) retain their order and parsed values. Whole
 * selected lists and other blocks keep their original spelling on this path.
 * Other unsupported container
 * boundaries and unrepresentable Markdown operations fail atomically.
 * In Markdown, a split loose list of simple items
 * becomes tight when no blank line remains between its retained items.
 * At a collapsed text caret, wraps its current block. Markdown rejects blocks
 * that cannot retain their kind when reparsed inside a quote (such as front
 * matter and footnote definitions).
 * Unwrap removes the nearest quote containing the caret, moving all its
 * children up one level and preserving their NodeIds. */
XUI_API int xuiDocumentTxnWrapQuoteRange(xui_document_transaction transaction,
    const xui_doc_range_t* range, xui_doc_node_id* quote_id,
    xui_doc_range_t* after);
/* Remove the enclosing quote while keeping child NodeIds. */
XUI_API int xuiDocumentTxnUnwrapQuote(xui_document_transaction transaction,
    const xui_doc_position_t* at, xui_doc_position_t* caret);
/* Wrap consecutive sibling paragraphs/headings in a new list, retaining the
 * original block and inline NodeIds. Flags are XUI_DOC_ORDERED and/or
 * XUI_DOC_TASK; start is 1 or greater for ordered lists and 0 otherwise.
 * A collapsed eligible container gap creates an empty first item. Markdown
 * task lists require GFM or EXTENDED. The returned range keeps its direction. */
XUI_API int xuiDocumentTxnCreateListRange(xui_document_transaction transaction,
    const xui_doc_range_t* range, uint32_t flags, uint64_t start,
    xui_doc_node_id* list_id, xui_doc_range_t* after);
/* Change contiguous selected items in one list or adjacent sibling lists under
 * the same parent. A parent gap may select one or more complete sibling lists.
 * Prefix/suffix items keep their old style and ordered numbering; affected
 * items retain their NodeIds and checked state when they remain tasks. Each
 * selected list segment starts at start when ordered. The same flags/start
 * rules as CreateListRange apply. */
XUI_API int xuiDocumentTxnSetListStyleRange(xui_document_transaction transaction,
    const xui_doc_range_t* range, uint32_t flags, uint64_t start,
    xui_doc_range_t* after);
/* Remove list structure from selected items in one list or adjacent sibling
 * lists under the same parent, moving their child blocks to that parent.
 * A parent gap may select complete lists; the returned gap covers the moved
 * blocks. Other items remain lists. */
XUI_API int xuiDocumentTxnUnlistRange(xui_document_transaction transaction,
    const xui_doc_range_t* range, xui_doc_range_t* after);
/* Create a footnote reference at a semantic selection and its definition in
 * the same root transaction. NULL/empty label chooses the next free fnN label.
 * The initial body is UTF-8 plain text (newlines split paragraphs); empty
 * body leaves the definition ready for input. Returns both caret locations.
 * Markdown requires EXTENDED and a verified semantic round trip. */
XUI_API int xuiDocumentTxnInsertFootnote(xui_document_transaction transaction,
    const xui_doc_range_t* range, const char* label,
    const char* initial_utf8, uint64_t initial_bytes,
    xui_doc_node_id* reference_id, xui_doc_node_id* footnote_id,
    xui_doc_position_t* reference_caret, xui_doc_position_t* body_caret);
/* Remove one footnote reference. Definitions that become unreachable from
 * document body references are removed; previously unused Rich definitions
 * remain untouched. The caret occupies the former reference position;
 * Markdown verifies the resulting source round trip. */
XUI_API int xuiDocumentTxnRemoveFootnoteReference(xui_document_transaction transaction,
    xui_doc_node_id reference_id, xui_doc_position_t* caret);
/* Update an existing image's source, alt, title and size in one transaction. */
XUI_API int xuiDocumentTxnUpdateImage(xui_document_transaction transaction, xui_doc_node_id image_id,
    const xui_doc_image_desc_t* image);
/* LINK requires a destination and is changed through TxnSetLink, not SetMarks. */
XUI_API int xuiDocumentTxnSetMarks(xui_document_transaction transaction, const xui_doc_range_t* range, uint32_t set, uint32_t clear);
/* Apply a link to selected inline content, splitting text runs at the range
 * edges. NULL or empty URI removes the link and its destination/title. The
 * selection must be nonempty; Markdown commits only if it can round-trip the
 * resulting structure without changing the requested semantics. */
XUI_API int xuiDocumentTxnSetLink(xui_document_transaction transaction, const xui_doc_range_t* range,
    const char* uri, const char* title);
/* Replace the range (including a collapsed caret) with a single-line linked
 * label in one root transaction. Returns a caret after the inserted label in
 * the transaction draft. */
XUI_API int xuiDocumentTxnInsertLink(xui_document_transaction transaction, const xui_doc_range_t* range,
    const char* label, uint64_t label_bytes, const char* uri, const char* title, xui_doc_position_t* caret);
/* common: marks present throughout the selection; mixed: marks present on only
 * part of it. Structural gaps are supported, including a root Select All range. */
XUI_API int xuiDocumentSnapshotQueryMarks(xui_document_snapshot snapshot, const xui_doc_range_t* range, uint32_t* common, uint32_t* mixed);
#endif
typedef struct xui_doc_link_info_t {
    uint32_t iSize;
    int bLinked; /* Every selected inline run has a link mark. */
    int bMixed;  /* Link presence, destination or title differs within the selection. */
    const char* sUri;   /* Borrowed from snapshot; NULL when absent or mixed. */
    const char* sTitle; /* Borrowed from snapshot; NULL when absent or mixed. */
} xui_doc_link_info_t;
/* Query link state and borrowed URI/title for a semantic range. */
#if XUI_ENABLE_DOCUMENT
XUI_API int xuiDocumentSnapshotQueryLink(xui_document_snapshot snapshot, const xui_doc_range_t* range,
    xui_doc_link_info_t* out);
/* Apply only the named fields to selected inline runs. A nonempty semantic
 * selection is required. Rich text supports these fields; Markdown rejects
 * them because no selected dialect has a lossless representation. */
XUI_API int xuiDocumentTxnSetTextStyle(xui_document_transaction transaction, const xui_doc_range_t* range,
    uint32_t fields, const xui_doc_text_style_t* style);
/* Clear selected inline marks, link target/title, text colors, font size and
 * font family in one semantic transaction. Block styles and embedded objects
 * are unchanged. The selection must be nonempty. Markdown rewrites and
 * verifies its source against the resulting semantic document. */
XUI_API int xuiDocumentTxnClearFormatting(xui_document_transaction transaction,
    const xui_doc_range_t* range);
/* The query is valid for both profiles, but editing rich-only fields requires
 * the RICH profile. A caret query inspects its adjacent inline run. */
XUI_API int xuiDocumentSnapshotQueryTextStyle(xui_document_snapshot snapshot, const xui_doc_range_t* range,
    xui_doc_text_style_query_t* out);
/* Case-sensitive UTF-8 literal search over semantic text or Markdown source.
 * Matches may cross style runs and paragraph boundaries. A NULL scope searches
 * the complete document. total reports all non-overlapping matches; the caller
 * can first query with matches=NULL/capacity=0, then allocate its result array. */
XUI_API int xuiDocumentSnapshotFind(xui_document_snapshot snapshot, uint32_t domain,
    const char* pattern, uint64_t pattern_bytes, const xui_doc_range_t* scope,
    xui_doc_range_t* matches, uint64_t capacity, uint64_t* total);
#endif
enum {
    XUI_DOC_FIND_REGEX = 1u << 0,
    XUI_DOC_FIND_IGNORE_CASE = 1u << 1,
    XUI_DOC_FIND_WHOLE_WORD = 1u << 2,
    XUI_DOC_REPLACE_EXPAND = 1u << 3
};
/* FindEx uses the same semantic/source projection and non-overlapping result
 * order as Find. A regex query uses XRT's UTF-8 engine; an IGNORE_CASE literal
 * query is escaped before compilation. Regex anchors refer to the supplied
 * scope when one is provided. Empty regex matches are returned as collapsed
 * positions. WHOLE_WORD uses XUI's Unicode natural-word boundaries on the
 * complete projection, including text outside an optional scope. The flags=0
 * path retains Find's literal-search behavior. */
#if XUI_ENABLE_DOCUMENT
XUI_API int xuiDocumentSnapshotFindEx(xui_document_snapshot snapshot, uint32_t domain,
    const char* pattern, uint64_t pattern_bytes, uint32_t flags,
    const xui_doc_range_t* scope, xui_doc_range_t* matches,
    uint64_t capacity, uint64_t* total);
/* Replace every match in one atomic root transaction. An unsupported structural
 * boundary poisons the entire transaction; no partial replacement is published. */
XUI_API int xuiDocumentTxnReplaceAll(xui_document_transaction transaction,
    const char* pattern, uint64_t pattern_bytes, const char* replacement, uint64_t replacement_bytes,
    const xui_doc_range_t* scope, uint64_t* replaced);
/* The replacement is literal unless REGEX and REPLACE_EXPAND are both set.
 * In that mode $0/$1..., ${name}, and $$ expand captures or a literal '$'.
 * Invalid templates fail atomically. All matches share one root transaction. */
XUI_API int xuiDocumentTxnReplaceAllEx(xui_document_transaction transaction,
    const char* pattern, uint64_t pattern_bytes, const char* replacement,
    uint64_t replacement_bytes, uint32_t flags, const xui_doc_range_t* scope,
    uint64_t* replaced);
/* Structural text commands. The returned caret addresses the transaction draft;
 * set its revision to the committed revision after a successful commit. Newlines
 * split paragraphs; code/math/HTML literals retain their newlines. A range may
 * span sibling paragraphs, but cannot accidentally join independent cell scopes. */
XUI_API int xuiDocumentTxnReplaceRange(xui_document_transaction transaction, const xui_doc_range_t* range,
    const char* utf8, uint64_t bytes, xui_doc_position_t* caret);
/* Split a block at a semantic position in the draft. */
XUI_API int xuiDocumentTxnSplitBlock(xui_document_transaction transaction, const xui_doc_position_t* at, xui_doc_position_t* caret);
/* Split the paragraph at a position directly inside a list item. The trailing
 * paragraph and subsequent blocks become a new sibling item; the list's
 * ordered/tight attributes are preserved. A new task item starts unchecked. */
XUI_API int xuiDocumentTxnSplitListItem(xui_document_transaction transaction, const xui_doc_position_t* at, xui_doc_position_t* caret);
/* An empty list item exits to a paragraph in the list's parent container.
 * When the item is between siblings, the remaining items form a second list;
 * ordered numbering continues from its original position. */
XUI_API int xuiDocumentTxnExitListItem(xui_document_transaction transaction, const xui_doc_position_t* at, xui_doc_position_t* caret);
/* Indent an item under its previous sibling, or lift it from a nested list
 * after its parent item. Following nested siblings remain under the lifted
 * item. Outdenting a root-level item converts its blocks to ordinary blocks
 * and splits the surrounding list. Both commands retain a content caret. */
XUI_API int xuiDocumentTxnIndentListItem(xui_document_transaction transaction, const xui_doc_position_t* at, xui_doc_position_t* caret);
/* Move one list item outward in the transaction draft. */
XUI_API int xuiDocumentTxnOutdentListItem(xui_document_transaction transaction, const xui_doc_position_t* at, xui_doc_position_t* caret);
/* Adjust all items between two semantic positions in one list or adjacent
 * sibling lists under the same parent. Intermediate blocks must all be lists;
 * endpoint and empty items are included. Cross-list indentation requires an
 * unselected preceding item as its parent. The returned range keeps the
 * original selection direction; the adjustment is one transaction. */
XUI_API int xuiDocumentTxnIndentListRange(xui_document_transaction transaction, const xui_doc_range_t* range, xui_doc_range_t* after);
/* Outdent the selected consecutive list items. */
XUI_API int xuiDocumentTxnOutdentListRange(xui_document_transaction transaction, const xui_doc_range_t* range, xui_doc_range_t* after);
/* Set every paragraph/heading touched by a semantic selection, including a
 * structural gap range over complete blocks. Level 0 converts to a paragraph;
 * 1..6 convert to the matching heading. A collapsed eligible container gap
 * inserts an empty heading. Other block types remain intact. The operation
 * retains inline content/NodeIds and the original selection direction. */
XUI_API int xuiDocumentTxnSetHeading(xui_document_transaction transaction, const xui_doc_range_t* range,
    uint32_t level, xui_doc_range_t* after);
/* Apply the selected style fields to every paragraph/heading touched by a
 * semantic range. A collapsed eligible container gap creates a styled empty
 * paragraph. Markdown has no lossless representation for these body styles
 * and reports XUI_DOC_ERROR_UNREPRESENTABLE. */
XUI_API int xuiDocumentTxnSetBlockStyleRange(xui_document_transaction transaction, const xui_doc_range_t* range,
    uint32_t fields, const xui_doc_block_style_t* style, xui_doc_range_t* after);
/* Move a contiguous range of sibling blocks or list items one place up/down.
 * A structural parent gap keeps covering the moved range; text selections
 * retain their NodeIds and direction. down is 0 (up) or 1 (down). */
XUI_API int xuiDocumentTxnMoveBlockRange(xui_document_transaction transaction,
    const xui_doc_range_t* range, int down, xui_doc_range_t* after);
/* Join compatible adjacent blocks in the draft. */
XUI_API int xuiDocumentTxnJoinBlocks(xui_document_transaction transaction, xui_doc_node_id left, xui_doc_node_id right, xui_doc_position_t* caret);
/* Copy plain UTF-8 text from a semantic range. */
XUI_API int xuiDocumentSnapshotCopyRange(xui_document_snapshot snapshot, const xui_doc_range_t* range, char** out, uint64_t* bytes);
/* Table commands preserve a rectangular grid including row/column spans.
 * Row/column indices are zero based. Deleting the last row/column deletes the
 * table. Splitting keeps merged content in the upper-left cell; undo restores
 * the exact pre-merge distribution. */
XUI_API int xuiDocumentTxnInsertTable(xui_document_transaction transaction, xui_doc_node_id parent, uint64_t index,
    uint32_t rows, uint32_t columns, int header, xui_doc_node_id* out);
/* Insert a table row at the requested index. */
XUI_API int xuiDocumentTxnInsertTableRow(xui_document_transaction transaction, xui_doc_node_id table, uint32_t row);
/* Delete a table row at the requested index. */
XUI_API int xuiDocumentTxnDeleteTableRow(xui_document_transaction transaction, xui_doc_node_id table, uint32_t row);
/* Insert a table column at the requested index. */
XUI_API int xuiDocumentTxnInsertTableColumn(xui_document_transaction transaction, xui_doc_node_id table, uint32_t column);
/* Delete a table column at the requested index. */
XUI_API int xuiDocumentTxnDeleteTableColumn(xui_document_transaction transaction, xui_doc_node_id table, uint32_t column);
/* Table-owned preferred column widths in logical units. Zero means automatic.
 * Rich documents persist widths across rows, spans, Undo and native saves;
 * Markdown rejects this rich-only edit rather than dropping it on export. */
XUI_API int xuiDocumentSnapshotGetTableColumnWidth(xui_document_snapshot snapshot,
    xui_doc_node_id table, uint32_t column, float* out);
/* Set a Rich table column width; zero restores automatic width. */
XUI_API int xuiDocumentTxnSetTableColumnWidth(xui_document_transaction transaction,
    xui_doc_node_id table, uint32_t column, float width);
/* Merge a rectangular group of table cells. */
XUI_API int xuiDocumentTxnMergeCells(xui_document_transaction transaction, xui_doc_node_id table,
    uint32_t row, uint32_t column, uint32_t rows, uint32_t columns, xui_doc_node_id* out);
/* Split a merged table cell into separate cells. */
XUI_API int xuiDocumentTxnSplitCell(xui_document_transaction transaction, xui_doc_node_id cell);
/* Paste a UTF-8 TSV rectangle at an existing cell. Double-quoted fields may
 * contain tabs, row breaks and doubled quotes; malformed quoting is rejected.
 * Rich cells preserve embedded row breaks. Markdown rejects fields containing
 * row breaks as unrepresentable. The table grows at its bottom/right edges;
 * intersecting merged cells are rejected. Missing fields in short input rows
 * become empty cells. The last destination cell ID is returned for selection. */
XUI_API int xuiDocumentTxnPasteTableMatrix(xui_document_transaction transaction,
    xui_doc_node_id table, uint32_t row, uint32_t column, const char* utf8,
    uint64_t bytes, xui_doc_node_id* last_cell);
/* Clear every whole cell covered by an existing rectangle, preserving the
 * table grid and merged spans. Partial overlap with a merged cell is rejected.
 * The first destination cell ID is returned for the post-commit caret. */
XUI_API int xuiDocumentTxnClearTableMatrix(xui_document_transaction transaction,
    xui_doc_node_id table, uint32_t row, uint32_t column, uint32_t rows,
    uint32_t columns, xui_doc_node_id* first_cell);
/* Copy a complete cell rectangle as quoted TSV. A merged cell may be copied
 * only when its whole span is inside the rectangle; covered slots are empty. */
XUI_API int xuiDocumentSnapshotCopyTableMatrix(xui_document_snapshot snapshot,
    xui_doc_node_id table, uint32_t row, uint32_t column, uint32_t rows,
    uint32_t columns, char** out, uint64_t* bytes);
/* Replace a byte range of Markdown source in the draft. */
XUI_API int xuiDocumentTxnReplaceSource(xui_document_transaction transaction, uint64_t start, uint64_t end, const char* utf8, uint64_t bytes);
/* Validate and atomically publish a transaction. */
XUI_API int xuiDocumentTxnCommit(xui_document_transaction transaction, xui_document_change_set* out);
/* Discard a transaction's unpublished changes. */
XUI_API void xuiDocumentTxnAbort(xui_document_transaction transaction);
/* Release a transaction, aborting it if still open. */
XUI_API void xuiDocumentTxnRelease(xui_document_transaction transaction);
/* Undo one committed history step. */
XUI_API int xuiDocumentUndo(xui_document document, xui_document_change_set* out);
/* Redo one undone history step. */
XUI_API int xuiDocumentRedo(xui_document document, xui_document_change_set* out);
/* Report whether an Undo step is available. */
XUI_API int xuiDocumentCanUndo(xui_document document);
/* Report whether a Redo step is available. */
XUI_API int xuiDocumentCanRedo(xui_document document);
#endif

/* One pending preparation per live Document. Patches use UTF-8 byte offsets
 * in sequential order, starting from the committed base revision. Create
 * copies the input and does not parse; a successful new Create supersedes the
 * previous candidate. A failed Create leaves that candidate intact.
 * Run is synchronous on its calling thread, so an executor may run it on a
 * worker. Retain before dispatch; release that reference when Run returns.
 * Run also prepares the ChangeSet/history and storage-accounting updates.
 * With unchanged history roots/limits, core Publish allocates nothing and
 * applies this table without traversing the source/tree. History changes
 * invalidate only the optimization; Publish uses the ordinary commit path.
 * Only Publish touches the live document/history or invokes change callbacks.
 * Owner-thread calls: PrepareSource, PrepareContinueSource, PrepareStreamSource,
 * PreparePublish, CancelPrepare, HasPrepare.
 * Other prepare calls support concurrent use through retained handles. */
typedef struct xui_doc_source_patch_t {
    uint32_t iSize;
    uint64_t iStart, iEnd;
    const char* sText;
    uint64_t iTextBytes;
} xui_doc_source_patch_t;
enum xui_doc_prepare_state {
    XUI_DOC_PREPARE_QUEUED = 1, XUI_DOC_PREPARE_RUNNING,
    XUI_DOC_PREPARE_READY, XUI_DOC_PREPARE_PUBLISHING,
    XUI_DOC_PREPARE_COMMITTED, XUI_DOC_PREPARE_FAILED
};
typedef struct xui_doc_prepare_info_t {
    uint32_t iSize, iState;
    uint64_t iDocumentId, iBaseRevision, iGeneration, iSourceBytes, iPatchCount;
    uint64_t iCommittedRevision; /* Nonzero only after Publish succeeds. */
    int iResult; /* BUSY while queued/running/publishing; otherwise final result. */
    int bCancellationRequested;
    uint32_t iBufferedUtf8Bytes; /* 0..3 incomplete streaming bytes, excluded from iSourceBytes. */
} xui_doc_prepare_info_t;
/* Prepare a Markdown source replacement for later publication. */
#if XUI_ENABLE_DOCUMENT
XUI_API int xuiDocumentPrepareSource(xui_document document, const xui_doc_txn_desc_t* desc,
    const xui_doc_source_patch_t* patches, uint64_t count, xui_document_prepare* out);
/* Extend the current candidate's immutable source, including while Run is
 * active. Patch offsets start from that candidate, not the committed source.
 * Inherits its base revision/origin/group and all earlier patches. Source and
 * operation-log storage are shared; no full copy or parse occurs here. Only
 * successful creation supersedes previous. Uncancelled failed Run can be
 * retried/edited this way; an explicitly cancelled or stale handle cannot.
 * The entire chain publishes as one transaction and one history step (subject
 * to normal grouping/budgets). iPatchCount is cumulative across the chain. */
XUI_API int xuiDocumentPrepareContinueSource(xui_document document, xui_document_prepare previous,
    const xui_doc_source_patch_t* patches, uint64_t count, xui_document_prepare* out);
/* Append network/producer chunks, retaining up to three trailing UTF-8 bytes.
 * previous==NULL starts from committed EOF; desc is accepted only then. With
 * previous, append to its source and buffered bytes, inheriting its origin and
 * group. Invalid chunks (including already-invalid partial prefixes), limits
 * and OOM preserve the previous candidate in full. final_chunk requires a
 * complete code point at EOF, including for an empty final chunk.
 * Copy/ReadSource expose only complete characters. Buffered bytes keep Run and
 * Publish BUSY; ContinueSource also returns BUSY until StreamSource completes
 * them. The source+buffer obey the document byte limit. Dispatch Run once per
 * host frame/batch when iBufferedUtf8Bytes==0; the core owns no timer or thread. */
XUI_API int xuiDocumentPrepareStreamSource(xui_document document, xui_document_prepare previous,
    const xui_doc_txn_desc_t* desc, const char* utf8, uint64_t bytes, int final_chunk, xui_document_prepare* out);
/* Retain an asynchronous source-prepare handle. */
XUI_API void xuiDocumentPrepareRetain(xui_document_prepare prepare);
/* The final Release may reclaim a large candidate or retired history graph.
 * A latency-sensitive host can transfer its remaining handle to a worker
 * after Publish/cancellation; the core does not create cleanup threads. */
XUI_API void xuiDocumentPrepareRelease(xui_document_prepare prepare);
/* Run parsing and validation for a prepared source candidate. */
XUI_API int xuiDocumentPrepareRun(xui_document_prepare prepare);
/* Read the candidate's generation, state and result. */
XUI_API int xuiDocumentPrepareGetInfo(xui_document_prepare prepare, xui_doc_prepare_info_t* out);
/* Input projection only: it has no semantic tree or independent history.
 * Copy is safe while Run is active. It uses the usual length+1 buffer rule. */
XUI_API int xuiDocumentPrepareCopySource(xui_document_prepare prepare, char* buffer, uint64_t capacity, uint64_t* length);
/* Read exactly bytes without a terminator or allocation, including arbitrary
 * byte slices within a UTF-8 character. Returns INVALID_ARGUMENT past EOF. */
XUI_API int xuiDocumentPrepareReadSource(xui_document_prepare prepare, uint64_t offset, void* buffer, uint64_t bytes);
/* A candidate position is rejected by committed Snapshot/Transaction APIs.
 * It is meaningful only with this exact document/base revision/generation. */
XUI_API int xuiDocumentPrepareSourcePosition(xui_document_prepare prepare, uint64_t offset, uint32_t affinity, xui_doc_position_t* position);
/* Map a SOURCE position between retained immutable candidate generations.
 * NULL from/to selects their shared committed base; at least one is non-NULL.
 * Non-NULL handles must be on one continuation lineage, with identical
 * document identity/base revision. Skipped generations and reverse mapping
 * use the recorded patch log; deleted interiors return MAP_DELETED.
 * Queries allocate nothing, remain valid after cancellation/supersession and
 * support concurrent Run. They do not publish or change history. Failures
 * preserve both outputs; position and output may be the same address.
 * After publication, MapPosition on its ChangeSet maps that exact candidate
 * generation to the committed source without applying the patches twice. */
XUI_API int xuiDocumentPrepareMapSourcePosition(xui_document_prepare from,
    xui_document_prepare to, const xui_doc_position_t* position,
    xui_doc_position_t* output, int* mapping);
/* Create a TEXT/GAP position in the immutable semantic preview of a VISUAL
 * input. Source-only preparations return UNSUPPORTED. Snapshot/Transaction
 * APIs reject tagged input positions; the exact Prepare owns their meaning. */
XUI_API int xuiDocumentPrepareSemanticPosition(xui_document_prepare prepare,
    uint64_t node, uint32_t kind, uint64_t offset, uint32_t affinity,
    xui_doc_position_t* position);
/* General input mapping. SOURCE uses the same source-patch rules above;
 * TEXT/GAP uses the actual semantic transaction log of VISUAL previews.
 * NULL denotes the shared committed base. Retained handles, continuation
 * lineage validation, no-allocation queries, concurrent Run and atomic failure
 * follow MapSourcePosition. Deleted semantic interiors collapse with quality
 * MAP_DELETED. The publishing ChangeSet maps its exact input generation to
 * the parsed committed tree, including repartitioned text and object gaps. */
XUI_API int xuiDocumentPrepareMapPosition(xui_document_prepare from,
    xui_document_prepare to, const xui_doc_position_t* position,
    xui_doc_position_t* output, int* mapping);
/* Cooperative cancellation; already-started publication wins. Cancel requests
 * do not wait for a worker or detach the document's pending handle. */
XUI_API void xuiDocumentPrepareCancel(xui_document_prepare prepare);
/* Clears the owner's pending handle and requests cancellation, without waiting.
 * Workers retain their own handles. Also used before intentionally discarding
 * pending input on focus/mode changes. Returns BUSY inside change callbacks. */
XUI_API int xuiDocumentCancelPrepare(xui_document document);
/* Report whether a source candidate remains attached. */
XUI_API int xuiDocumentHasPrepare(xui_document document);
/* READY only. Verifies identity, base revision and current generation. BUSY
 * leaves a candidate retryable; other failures finish that generation and
 * publish nothing. Uncancelled failures remain attached, blocking SaveFile
 * until ContinueSource/StreamSource retries or edits them, or CancelPrepare
 * explicitly discards them. Ordinary successful writes/Undo/Redo supersede input.
 * SaveFile returns BUSY while a candidate remains attached; explicit snapshot
 * export continues to export that snapshot's committed content. */
XUI_API int xuiDocumentPreparePublish(xui_document document, xui_document_prepare prepare, xui_document_change_set* out);
/* Retain an immutable committed change set. */
XUI_API void xuiDocumentChangeSetRetain(xui_document_change_set change);
/* Release an immutable committed change set. */
XUI_API void xuiDocumentChangeSetRelease(xui_document_change_set change);
/* Read a committed change set's operations and revisions. */
XUI_API int xuiDocumentChangeSetGetInfo(xui_document_change_set change, xui_doc_change_info_t* out);
/* Input and output may alias. Semantic positions follow semantic operations;
 * SOURCE positions follow byte patches. Only exact mappings return EXACT. */
XUI_API int xuiDocumentMapPosition(xui_document_change_set change, const xui_doc_position_t* before, xui_doc_position_t* after, int* mapping);
/* Map a Markdown source byte offset to a Document position. */
XUI_API int xuiDocumentSourceToPosition(xui_document_snapshot snapshot, uint64_t source_offset, xui_doc_position_t* out, int* mapping);
/* Map a source byte offset with explicit boundary affinity. */
XUI_API int xuiDocumentSourceToPositionEx(xui_document_snapshot snapshot, uint64_t source_offset, uint32_t affinity, xui_doc_position_t* out, int* mapping);
/* Map a Document position to a Markdown source byte offset. */
XUI_API int xuiDocumentPositionToSource(xui_document_snapshot snapshot, const xui_doc_position_t* position, uint64_t* out, int* mapping);
/* Compare two positions in one immutable snapshot. */
XUI_API int xuiDocumentSnapshotComparePositions(xui_document_snapshot snapshot, const xui_doc_position_t* a, const xui_doc_position_t* b, int* order);
/* Allocate a plain-text export of snapshot content. */
XUI_API int xuiDocumentSnapshotCopyPlainText(xui_document_snapshot snapshot, char** out, uint64_t* bytes);

/* Loading is transactional; failed input leaves the document unchanged. */
XUI_API int xuiDocumentLoadMarkdown(xui_document document, const char* utf8, uint64_t bytes);
/* Serialize an immutable snapshot into native Document data. */
XUI_API int xuiDocumentSerialize(xui_document_snapshot snapshot, char** out, uint64_t* bytes);
/* Create a Document from native serialized data. */
XUI_API int xuiDocumentDeserialize(const xui_doc_desc_t* desc, const char* data, uint64_t bytes, xui_document* out);
/* Allocate a UTF-8 HTML export of the snapshot. */
XUI_API int xuiDocumentExportHtml(xui_document_snapshot snapshot, char** out, uint64_t* bytes);
/* Buffers returned by serialize/export are released with this function. */
XUI_API void xuiDocumentFreeBuffer(void* buffer);
#endif

#ifdef __cplusplus
}
#endif
#endif
