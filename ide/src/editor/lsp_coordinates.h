#ifndef XIDE_EDITOR_LSP_COORDINATES_H
#define XIDE_EDITOR_LSP_COORDINATES_H
#include <xui.h>

/* Private UI-thread conversion for the IDE's validated UTF-8 editor documents.
 * The SDK normalizes line endings. Only the queried line/prefix is decoded;
 * this is not a replacement for validating untrusted protocol source text.
 * Coordinates are zero-based UTF-16. Invalid endpoints fail without changing
 * outputs; EOL overflow and split scalar/surrogate positions are not clamped. */
int xideCodeDocumentOffsetToLsp(xui_code_document document,int offset,int* line,int* character);
int xideCodeDocumentLspToOffset(xui_code_document document,int line,int character,int* offset);
#endif
