"""Reviewed Document APIs with public replacements in the managed packages."""

REPLACEMENTS = {
    'xuiDocumentCreate': ('xui.createDocument', 'xuiDocumentCreateOwned'),
    'xuiDocumentOpenFile': ('xui.openDocument', 'xuiDocumentOpenFileOwned'),
    'xuiDocumentDeserialize': ('xui.deserializeDocument', 'xuiDocumentDeserializeOwned'),
    'xuiDocumentFragmentImportHtml': (
        'xui.importDocumentFragmentHtml', 'xuiDocumentFragmentImportHtmlOwned'),
    'xuiDocumentFragmentDeserialize': (
        'xui.deserializeDocumentFragment', 'xuiDocumentFragmentDeserializeOwned'),
    'xuiDocumentSnapshotAnalyzeMarkdownConversion': (
        'xui.DocumentSnapshot.collectMarkdownLosses', 'xuiDocumentSnapshotCollectMarkdownLosses'),
    'xuiDocumentSnapshotFind': ('xui.DocumentSnapshot.find', 'xuiDocumentSnapshotFindOwned'),
    'xuiDocumentSnapshotFindEx': ('xui.DocumentSnapshot.findIn', 'xuiDocumentSnapshotFindOwned'),
    'xuiDocumentRendererGetRangeRects': (
        'xui.DocumentRenderer.collectRangeRects', 'xuiDocumentRendererCollectRangeRects'),
    'xuiDocumentWebProviderSetInvalidate': (
        'xui.DocumentWebProvider.setInvalidate', 'xuiDocumentWebProviderSetInvalidateOwned'),
    'xuiDocumentHtmlInteractionSetOpenLink': (
        'xui.DocumentHtmlInteraction.setOpenLink', 'xuiDocumentHtmlInteractionSetOpenLinkOwned'),
    'xuiDocumentWebObjectMeasure': (
        'xui.DocumentWebProvider.measureObject', 'xuiDocumentWebProviderMeasureObject'),
    'xuiDocumentWebObjectDraw': (
        'xui.DocumentWebProvider.drawObject', 'xuiDocumentWebProviderDrawObject'),
}

# Each share acquires a new native reference and returns an independent view.
# The Retain aliases below also accept native raw models, which stay C-only.
_SHARE_MODELS = {
    'Document': ('AddRef', 'Retain'),
    'DocumentSnapshot': ('AddRef', 'Retain'),
    'DocumentChangeSet': ('AddRef', 'Retain'),
    'DocumentPrepare': ('AddRef', 'Retain'),
    'DocumentFragment': ('AddRef', 'Retain'),
    'DocumentSourceIndex': ('AddRef', 'Retain'),
    'DocumentMatches': ('AddRef',),
    'DocumentMarkdownLosses': ('AddRef',),
    'DocumentRangeRects': ('AddRef',),
}
LIFECYCLE_REPLACEMENTS = {
    f'xui{model}{suffix}': (f'xui.{model}.share', f'xui{model}ShareOwned')
    for model, suffixes in _SHARE_MODELS.items() for suffix in suffixes
}
LIFECYCLE_REPLACEMENTS.update({
    'xuiDocumentSubscribe': ('xui.Document.subscribe', 'xuiDocumentSubscribeOwned'),
    'xuiDocumentUnsubscribe': ('xui.Document.unsubscribeChecked', 'xuiDocumentUnsubscribeChecked'),
    'xuiDocumentWebProviderRelease': ('xui.DocumentWebProvider.close', 'xuiDocumentWebProviderReleaseChecked'),
    'xuiDocumentHtmlInteractionRelease': ('xui.DocumentHtmlInteraction.close', 'xuiDocumentHtmlInteractionReleaseChecked'),
    'xuiDocumentImageResourceRelease': ('xui.DocumentImageRequest.close', 'xuiDocumentImageResourceReleaseChecked'),
})

# Unlike the direct aliases above, close consumes the view through its policy.
# Audit the descriptor accessor's compiled path to the checked native release.
CLOSE_REPLACEMENTS = {}
for _model in (*_SHARE_MODELS, 'DocumentTxn', 'DocumentRenderer'):
    _logical = 'DocumentTransaction' if _model == 'DocumentTxn' else _model
    _method = f'xui.{_logical}.close'
    _policy = f'xl6Xui3{_model}Policy'
    _release = f'xui{_model}ReleaseOwnerChecked'
    _consume = 'destroy_document_renderer' if _model == 'DocumentRenderer' else f'destroy_{_model}'
    _suffixes = ('ReleaseChecked',) if _model in ('DocumentMatches', 'DocumentMarkdownLosses', 'DocumentRangeRects') else ('Release', 'ReleaseChecked')
    for _suffix in _suffixes:
        CLOSE_REPLACEMENTS[f'xui{_model}{_suffix}'] = (_method, _policy, _consume, _release)
