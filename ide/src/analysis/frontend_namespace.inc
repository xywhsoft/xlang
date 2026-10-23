/* Included in the shared import TU after its original member rewrite helper.
 * It supplies real public export spellings to the existing semantic candidate
 * checker, without a second namespace lookup or type/signature algorithm. */
static int xlsImportsNamespaceCursor(const XL6_AstNode* node)
{
    return xlsFrontendNamespaceReady() && node && node->source && node->source->completionQuery &&
        node->text && !strcmp(node->text,XL6_COMPLETION_MARKER) && node->tokenRange.known &&
        node->tokenRange.startOffset==node->source->completionStart && node->tokenRange.endOffset==node->source->completionEnd;
}
static int xlsImportsCaptureNamespace(XL6_AstNode* node,const XL6_ImportSymbolTable* symbols,const char* ns)
{
    if(!xlsFrontendNamespaceStart()) return 0;
    for(size_t i=0;i<symbols->count;++i) {
        const XL6_ImportSymbol* symbol=&symbols->items[i];
        if(symbol->supported && xl6ImportsTextEquals(symbol->ns,ns) &&
            !xlsFrontendNamespaceAdd(symbol->internalName,symbol->name)) return 0;
    }
    xl6ImportsRewriteMemberToName(node,XL6_COMPLETION_MARKER);
    return node->text!=NULL;
}
