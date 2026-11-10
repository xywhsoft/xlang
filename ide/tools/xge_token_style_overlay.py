"""Derive the additive XUI visible-token style resolver in a matched source set.

No lexical buffer copy, document change or layout invalidation. The same scoped
source edit is applied to upstream and to the frozen IDE SDK cohort; archives
and build receipts retain both before/after identities. Never edits an SDK DLL.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path


def derive(root):
    root = Path(root)
    changes = {}

    def edit(relative, replacements):
        path = root / relative
        original = path.read_bytes()
        newline = b"\r\n" if b"\r\n" in original else b"\n"
        body = original
        for before, after in replacements:
            before = before.replace("\n", newline.decode()).encode()
            after = after.replace("\n", newline.decode()).encode()
            if body.count(before) != 1:
                raise ValueError(f"{relative}: expected one anchor: {before[:80]!r}")
            body = body.replace(before, after, 1)
        changes[relative] = dict(before=hashlib.sha256(original).hexdigest(),
                                 after=hashlib.sha256(body).hexdigest())
        return path, body

    header = edit("include/xui.h", [
        ("typedef int (*xui_code_fold_proc)", """/* Optional owner/render-thread foreground override for a visible lexical token.
 * Token offsets are document-wide UTF-8 bytes, including on later lines.
 * Document/token are borrowed for this call only. Do not retain them, mutate
 * UI/document state or reenter rendering. User data lives until uninstalled or
 * the editor is destroyed. Return XUI_OK with a defined nontransparent style
 * id to override; all other outcomes preserve lexical theme/CSS foreground. */
#define XUI_CODE_TOKEN_STYLE_RESOLVER 1
typedef int (*xui_code_token_style_proc)(xui_code_document_t* pDocument,
    const xui_code_token_t* pToken, int* pStyleId, void* pUser);

typedef int (*xui_code_fold_proc)"""),
        ("XUI_API int xuiCodeEditSetStyle(xui_widget pWidget, int iStyleId, const xui_code_style_t* pStyle);", """XUI_API int xuiCodeEditSetStyle(xui_widget pWidget, int iStyleId, const xui_code_style_t* pStyle);
/* O(1) callback/user switch: repaint only; lexical cache and layout are intact.
 * NULL callback uninstalls and clears user. Getter requires at least one output. */
XUI_API int xuiCodeEditSetTokenStyleResolver(xui_widget pWidget, xui_code_token_style_proc onStyle, void* pUser);
XUI_API int xuiCodeEditGetTokenStyleResolver(xui_widget pWidget, xui_code_token_style_proc* pStyle, void** pUser);"""),
    ])
    source = edit("src/xui/code/xui_code_edit.c", [
        ("\txui_code_token_buffer pTokenBuffer;", "\txui_code_token_buffer pTokenBuffer;\n\txui_code_token_style_proc onTokenStyle;\n\tvoid* pTokenStyleUser;"),
        ("static int __xuiCodeEditRenderTokenSpan(", """static int __xuiCodeEditTokenForeground(xui_widget pWidget,
    xui_code_edit_data_t* pData, const xui_code_token_t* pToken, int iDocumentStart, uint32_t* pColor)
{
    xui_code_style_t tStyle;
    const char* sProperty;
    int iStyleId = pToken->iStyle;
    xui_code_token_t tGlobal = *pToken;
    tGlobal.iStartOffset += iDocumentStart;
    tGlobal.iEndOffset += iDocumentStart;
    if ( pData->onTokenStyle != NULL &&
         pData->onTokenStyle(pData->pDocument, &tGlobal, &iStyleId, pData->pTokenStyleUser) == XUI_OK &&
         xuiCodeThemeGetStyle(pData->pTheme, iStyleId, &tStyle) == XUI_OK &&
         __xuiCodeEditAlpha(tStyle.iForeground) != 0 ) {
        *pColor = tStyle.iForeground;
        return XUI_OK;
    }
    memset(&tStyle, 0, sizeof(tStyle));
    if ( __xuiCodeEditGetTokenStyle(pData->pTheme, *pToken, &tStyle) != XUI_OK ||
         __xuiCodeEditAlpha(tStyle.iForeground) == 0 ) return XUI_ERROR_NOT_FOUND;
    sProperty = __xuiCodeEditSyntaxColorProperty(pToken->iKind);
    *pColor = sProperty != NULL ? __xuiCodeEditColor(pWidget, sProperty, tStyle.iForeground) : tStyle.iForeground;
    return XUI_OK;
}

static int __xuiCodeEditRenderTokenSpan("""),
        ("\txui_code_style_t tStyle;\n\tconst char* sProperty;\n\tuint32_t iColor;\n\tint iStart;", "\tuint32_t iColor;\n\tint iStart;"),
        ("\tmemset(&tStyle, 0, sizeof(tStyle));\n\tiRet = __xuiCodeEditGetTokenStyle(pData->pTheme, tToken, &tStyle);\n\tif ( iRet != XUI_OK || __xuiCodeEditAlpha(tStyle.iForeground) == 0 ) return XUI_OK;\n\tsProperty = __xuiCodeEditSyntaxColorProperty(tToken.iKind);\n\tiColor = (sProperty != NULL) ? __xuiCodeEditColor(pWidget, sProperty, tStyle.iForeground) : tStyle.iForeground;", "\tiRet = __xuiCodeEditTokenForeground(pWidget, pData, &tToken, iDocumentStart, &iColor);\n\tif ( iRet != XUI_OK ) return XUI_OK;"),
        ("\t\t\t\t\txui_code_style_t tStyle;\n\t\t\t\t\tconst char* sProperty;", "\t\t\t\t\tuint32_t iTokenColor;"),
        ("\t\t\t\t\tmemset(&tStyle, 0, sizeof(tStyle));\n\t\t\t\t\tif ( __xuiCodeEditGetTokenStyle(pData->pTheme, pTokens[iToken], &tStyle) != XUI_OK ||\n\t\t\t\t\t     __xuiCodeEditAlpha(tStyle.iForeground) == 0 ) continue;\n\t\t\t\t\tsProperty = __xuiCodeEditSyntaxColorProperty(pTokens[iToken].iKind);", "\t\t\t\t\tif ( __xuiCodeEditTokenForeground(pWidget, pData, &pTokens[iToken], iDocumentStart, &iTokenColor) != XUI_OK ) continue;"),
        ("\t\t\t\t\tarrSpans[iSpanCount].iColor = sProperty != NULL ?\n\t\t\t\t\t\t__xuiCodeEditColor(pWidget, sProperty, tStyle.iForeground) : tStyle.iForeground;", "\t\t\t\t\tarrSpans[iSpanCount].iColor = iTokenColor;"),
        ("int iLineStart, int iLineEnd, xui_code_token_t tToken,", "int iLineStart, int iLineEnd, int iDocumentStart, xui_code_token_t tToken,"),
        ("const char* sText, int iLineStart, int iRenderStart, int iRenderEnd,", "const char* sText, int iLineStart, int iDocumentStart, int iRenderStart, int iRenderEnd,"),
        ("pData, sText, iLineStart, iRenderEnd, tToken,", "pData, sText, iLineStart, iRenderEnd, iDocumentStart, tToken,"),
        ("pData, sText, 0, iRenderStart - iStart, iRenderEnd - iStart,", "pData, sText, 0, iStart, iRenderStart - iStart, iRenderEnd - iStart,"),
        ("XUI_API xui_code_fold_state xuiCodeEditGetFoldState(", """XUI_API int xuiCodeEditSetTokenStyleResolver(xui_widget pWidget,
    xui_code_token_style_proc onStyle, void* pUser)
{
    xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
    if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
    pData->onTokenStyle = onStyle;
    pData->pTokenStyleUser = onStyle != NULL ? pUser : NULL;
    return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCodeEditGetTokenStyleResolver(xui_widget pWidget,
    xui_code_token_style_proc* pStyle, void** pUser)
{
    xui_code_edit_data_t* pData = __xuiCodeEditGetData(pWidget);
    if ( pStyle != NULL ) *pStyle = NULL;
    if ( pUser != NULL ) *pUser = NULL;
    if ( pData == NULL || (pStyle == NULL && pUser == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
    if ( pStyle != NULL ) *pStyle = pData->onTokenStyle;
    if ( pUser != NULL ) *pUser = pData->pTokenStyleUser;
    return XUI_OK;
}

XUI_API xui_code_fold_state xuiCodeEditGetFoldState("""),
    ])
    # Validate all anchors first so failed derivation cannot leave half an API.
    for path, body in (header, source):
        path.write_bytes(body)
    return changes


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    args = parser.parse_args()
    print(json.dumps(derive(args.root), indent=2))
