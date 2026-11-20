"""Reviewed Document APIs with public replacements in the managed packages."""

REPLACEMENTS = {
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
