#ifndef XL6_TEXT_PIECES_H
#define XL6_TEXT_PIECES_H
#include "src/runtime/xl6_text_internal.h"
/* A checkpointable XRT cursor. No delimiter algorithms live in the adapter. */
typedef union XL6_TextPieces {
    xstrsplit split;
    xstrlines lines;
    xstrfields fields;
} XL6_TextPieces;
static inline bool xl6TextPiecesInit(XL6_TextPieces* pieces, xstrview text, xstrview separator, unsigned mode)
{
    return mode == 0 ? xrtStrSplitInit(&pieces->split, text, separator) :
        mode == 1 ? xrtStrLinesInit(&pieces->lines, text) : xrtStrFieldsInit(&pieces->fields, text);
}
static inline bool xl6TextPiecesNext(XL6_TextPieces* pieces, xstrview* item, unsigned mode)
{
    return mode == 0 ? xrtStrSplitNext(&pieces->split, item) :
        mode == 1 ? xrtStrLinesNext(&pieces->lines, item) : xrtStrFieldsNext(&pieces->fields, item);
}
#endif
