#ifndef XL6_XGE3_RUNTIME_H
#define XL6_XGE3_RUNTIME_H
#include <xrt_decl.h>
#include <xge_build_config.h>
#ifndef XGE_XRT_HOST
#define XGE_XRT_HOST 1
#endif
#if defined(_WIN32) && !defined(XL6_XGE_STATIC)
#ifndef XGE_DLL
#define XGE_DLL 1
#endif
#ifndef XUI_DLL
#define XUI_DLL 1
#endif
#endif
#include <xge.h>
#include <xui.h>
#include <xui_document.h>
#include <xui_document_ui.h>
#include <xge_host_xrt.h>
#include <__xl_runtime/xl6_foreign_view.h>

#ifdef __cplusplus
extern "C" {
#endif
/* Cold checked initialization. Repeat calls validate the identical runtime;
 * the native engine never acquires or embeds a second XRT implementation. */
int xl6Xge3RuntimeBindV1(void);
int xl6Xge3InitWithCode(const xge_desc_t*,const XL6_CodeOwner*);
int xl6Xge3SceneCreateOwned(xge_scene_owned*,const XL6_CodeOwner*);
int xl6Xge3ParticleWorldCreateOwned(xge_particle_world*,const xge_particle_world_desc_t*,const XL6_CodeOwner*);
int xl6Xge3AsyncImageCreateOwned(xge_async_owned*,const XL6_CodeOwner*);
int xl6Xge3AsyncTextureCreateOwned(xge_async_owned*,const XL6_CodeOwner*);
#if XGE_ENABLE_TEXT
int xl6Xge3AsyncFontCreateOwned(xge_async_owned*,const XL6_CodeOwner*);
#endif
#if XGE_ENABLE_AUDIO
int xl6Xge3AsyncSoundCreateOwned(xge_async_owned*,const XL6_CodeOwner*);
#endif
int xl6Xge3GraphicsContextCreateEglOwned(xge_graphics_context*,int,int,bool,const XL6_CodeOwner*);
int xl6Xge3RunOwned(xge_scene_proc,void*,const xge_callback_owner_t*,const XL6_CodeOwner*);
int xl6Xge3DragBegin(xge_data_object,uint32_t,uint32_t,const XL6_CodeOwner*);
int xl6Xge3CopyToClipboard(xge_data_object,const XL6_CodeOwner*);
int xl6Xui3CopyToClipboard(xui_context,xui_data_object,const XL6_CodeOwner*);
int xl6Xge3ClipboardSetItems(const xge_clipboard_item_t*,int,const XL6_CodeOwner*);
int xl6Xui3ClipboardSetItems(xui_context,const xui_clipboard_item_t*,int,const XL6_CodeOwner*);

const XL6_ForeignViewDescriptorV1* xl6Xui3ContextPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3LanguagePolicyGet(void);
void xl6Xui3RollbackLanguage(xui_language value);
const XL6_ForeignViewDescriptorV1* xl6Xui3WidgetPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3WidgetTypePolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3IconCategoryPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3IconPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3ResourcePolicyGet(void);
void xl6Xui3RollbackIconCategory(xui_icon_category);
void xl6Xui3RollbackIcon(xui_icon);
const XL6_ForeignViewDescriptorV1* xl6Xui3MsgBoxPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3ToastPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3FileDialogPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3MsgTipPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3TableCellOutputPolicyGet(void);
const XL6_OwnershipDomainV1* xuiTableViewCellOutputOwnershipDomainV1Get(void);
const XL6_OwnershipDomainV1* xl6Xui3OwnershipDomainV1Get(void);
const XL6_OwnershipDomainV1* xl6Xge3TransferDomainV1Get(void);
const XL6_ForeignViewDescriptorV1* xl6Xge3DataObjectPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3DataObjectPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3FlowGraphPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3WorkflowPolicyGet(void);
const XL6_OwnershipDomainV1* xl6Xui3DocumentModelDomainV1Get(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3DocumentPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3DocumentSnapshotPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3DocumentChangeSetPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3DocumentMatchesPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3DocumentRangeRectsPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3DocumentMarkdownLossesPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3DocumentPreparePolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3DocumentFragmentPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3DocumentSourceIndexPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3DocumentTxnPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3DocumentRendererPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3DocumentHtmlInteractionPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3DocumentWebProviderPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3DocumentImageRequestPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3ProxyPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3FontPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3SurfacePolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3DrawPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3PathPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeFindScopePolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3TextShapePolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3TextLayoutPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeDocumentPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeDocumentBatchPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeDocumentBatchSnapshotPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeDocumentBatchPayloadPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeEditDocumentViewPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeEditSelectionViewPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeSelectionPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeThemePolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeEditCommandMapPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeEditFoldStatePolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeEditThemePolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3InputDecorationPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeLanguageRegistryPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeEditLanguageRegistryPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeEditAnnotationsPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeEditTokenBufferPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeEditMarginsPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeMarginPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeProviderSetPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeEditProvidersPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeCompletionSnapshotPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeHoverSnapshotPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeSignatureSnapshotPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xge3FontPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xge3TexturePolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xge3ImagePolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xge3MaterialPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xge3ShapePolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xge3ShapeScenePolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xge3PathMeasurePolicyGet(void);
typedef struct xl6_xge3_svg_call_t { xge_svg document; } xl6_xge3_svg_call_t;
const XL6_ForeignViewDescriptorV1* xl6Xge3SvgPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xge3SvgPaintPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xge3SvgCachePolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xge3AudioClipPolicyGet(void);
const XL6_ForeignViewDescriptorV1* xl6Xui3InventoryIndicesPolicyGet(void);

/* Grid snapshots terminate their inline text before any callback sees it. */
static inline const char* xl6Xui3InventorySlotText(const xui_inventory_slot_t* slot)
{
    return slot->sText;
}

static inline const char* xl6Xui3InventorySlotHotkey(const xui_inventory_slot_t* slot)
{
    return slot->sHotkey;
}

static inline const char* xl6Xui3TimelineLayerName(const xui_timeline_layer_t* layer)
{
    return layer->sName;
}

static inline const char* xl6Xui3TimelineSpanType(const xui_timeline_span_t* span)
{
    return span->sCustomType;
}

static inline const char* xl6Xui3TimelineSpanLabel(const xui_timeline_span_t* span)
{
    return span->sLabel;
}
#ifdef __cplusplus
}
#endif
/* Getter expressions also work when a C provider lives in another PE image;
 * importing a data symbol through a function-only registry is not portable. */
#define xl6Xui3ContextPolicy (*xl6Xui3ContextPolicyGet())
#define xl6Xui3LanguagePolicy (*xl6Xui3LanguagePolicyGet())
#define xl6Xui3WidgetPolicy (*xl6Xui3WidgetPolicyGet())
#define xl6Xui3WidgetTypePolicy (*xl6Xui3WidgetTypePolicyGet())
#define xl6Xui3IconCategoryPolicy (*xl6Xui3IconCategoryPolicyGet())
#define xl6Xui3IconPolicy (*xl6Xui3IconPolicyGet())
#define xl6Xui3ResourcePolicy (*xl6Xui3ResourcePolicyGet())
#define xl6Xui3CodeFindScopePolicy (*xl6Xui3CodeFindScopePolicyGet())
#define xl6Xui3CodeEditDocumentViewPolicy (*xl6Xui3CodeEditDocumentViewPolicyGet())
#define xl6Xui3CodeEditSelectionViewPolicy (*xl6Xui3CodeEditSelectionViewPolicyGet())
#define xl6Xui3CodeSelectionPolicy (*xl6Xui3CodeSelectionPolicyGet())
#define xl6Xui3CodeThemePolicy (*xl6Xui3CodeThemePolicyGet())
#define xl6Xui3CodeEditCommandMapPolicy (*xl6Xui3CodeEditCommandMapPolicyGet())
#define xl6Xui3CodeEditFoldStatePolicy (*xl6Xui3CodeEditFoldStatePolicyGet())
#define xl6Xui3CodeEditThemePolicy (*xl6Xui3CodeEditThemePolicyGet())
#define xl6Xui3InputDecorationPolicy (*xl6Xui3InputDecorationPolicyGet())
#define xl6Xui3CodeLanguageRegistryPolicy (*xl6Xui3CodeLanguageRegistryPolicyGet())
#define xl6Xui3CodeEditLanguageRegistryPolicy (*xl6Xui3CodeEditLanguageRegistryPolicyGet())
#define xl6Xui3CodeEditAnnotationsPolicy (*xl6Xui3CodeEditAnnotationsPolicyGet())
#define xl6Xui3CodeEditTokenBufferPolicy (*xl6Xui3CodeEditTokenBufferPolicyGet())
#define xl6Xui3CodeEditMarginsPolicy (*xl6Xui3CodeEditMarginsPolicyGet())
#define xl6Xui3CodeMarginPolicy (*xl6Xui3CodeMarginPolicyGet())
#define xl6Xui3CodeProviderSetPolicy (*xl6Xui3CodeProviderSetPolicyGet())
#define xl6Xui3CodeEditProvidersPolicy (*xl6Xui3CodeEditProvidersPolicyGet())
#define xl6Xui3CodeCompletionSnapshotPolicy (*xl6Xui3CodeCompletionSnapshotPolicyGet())
#define xl6Xui3CodeHoverSnapshotPolicy (*xl6Xui3CodeHoverSnapshotPolicyGet())
#define xl6Xui3CodeSignatureSnapshotPolicy (*xl6Xui3CodeSignatureSnapshotPolicyGet())
#define xl6Xui3MsgBoxPolicy (*xl6Xui3MsgBoxPolicyGet())
#define xl6Xui3ToastPolicy (*xl6Xui3ToastPolicyGet())
#define xl6Xui3FileDialogPolicy (*xl6Xui3FileDialogPolicyGet())
#define xl6Xui3MsgTipPolicy (*xl6Xui3MsgTipPolicyGet())
#define xl6Xui3TableCellOutputPolicy (*xl6Xui3TableCellOutputPolicyGet())
#define xuiTableViewCellOutputOwnershipDomainV1 (*xuiTableViewCellOutputOwnershipDomainV1Get())
#define xl6Xui3OwnershipDomainV1 (*xl6Xui3OwnershipDomainV1Get())
#define xl6Xui3DocumentModelDomainV1 (*xl6Xui3DocumentModelDomainV1Get())
#define xl6Xui3DocumentPolicy (*xl6Xui3DocumentPolicyGet())
#define xl6Xui3DocumentSnapshotPolicy (*xl6Xui3DocumentSnapshotPolicyGet())
#define xl6Xui3DocumentChangeSetPolicy (*xl6Xui3DocumentChangeSetPolicyGet())
#define xl6Xui3DocumentMatchesPolicy (*xl6Xui3DocumentMatchesPolicyGet())
#define xl6Xui3DocumentRangeRectsPolicy (*xl6Xui3DocumentRangeRectsPolicyGet())
#define xl6Xui3DocumentMarkdownLossesPolicy (*xl6Xui3DocumentMarkdownLossesPolicyGet())
#define xl6Xui3DocumentPreparePolicy (*xl6Xui3DocumentPreparePolicyGet())
#define xl6Xui3DocumentFragmentPolicy (*xl6Xui3DocumentFragmentPolicyGet())
#define xl6Xui3DocumentSourceIndexPolicy (*xl6Xui3DocumentSourceIndexPolicyGet())
#define xl6Xui3DocumentTxnPolicy (*xl6Xui3DocumentTxnPolicyGet())
#define xl6Xge3TransferDomainV1 (*xl6Xge3TransferDomainV1Get())
#define xl6Xge3DataObjectPolicy (*xl6Xge3DataObjectPolicyGet())
#define xl6Xui3FlowGraphPolicy (*xl6Xui3FlowGraphPolicyGet())
#define xl6Xui3WorkflowPolicy (*xl6Xui3WorkflowPolicyGet())
#define xl6Xui3DataObjectPolicy (*xl6Xui3DataObjectPolicyGet())
#define xl6Xui3DocumentRendererPolicy (*xl6Xui3DocumentRendererPolicyGet())
#define xl6Xui3DocumentHtmlInteractionPolicy (*xl6Xui3DocumentHtmlInteractionPolicyGet())
#define xl6Xui3DocumentWebProviderPolicy (*xl6Xui3DocumentWebProviderPolicyGet())
#define xl6Xui3DocumentImageRequestPolicy (*xl6Xui3DocumentImageRequestPolicyGet())
#define xl6Xui3ProxyPolicy (*xl6Xui3ProxyPolicyGet())
#define xl6Xui3FontPolicy (*xl6Xui3FontPolicyGet())
#define xl6Xui3SurfacePolicy (*xl6Xui3SurfacePolicyGet())
#define xl6Xui3DrawPolicy (*xl6Xui3DrawPolicyGet())
#define xl6Xui3PathPolicy (*xl6Xui3PathPolicyGet())
#define xl6Xui3TextShapePolicy (*xl6Xui3TextShapePolicyGet())
#define xl6Xui3TextLayoutPolicy (*xl6Xui3TextLayoutPolicyGet())
#define xl6Xui3CodeDocumentPolicy (*xl6Xui3CodeDocumentPolicyGet())
#define xl6Xui3CodeDocumentBatchPolicy (*xl6Xui3CodeDocumentBatchPolicyGet())
#define xl6Xui3CodeDocumentBatchSnapshotPolicy (*xl6Xui3CodeDocumentBatchSnapshotPolicyGet())
#define xl6Xui3CodeDocumentBatchPayloadPolicy (*xl6Xui3CodeDocumentBatchPayloadPolicyGet())
/* Observed editor documents and independent documents have the same C handle. */
#define xl6Xui3PrepareEditorDocumentBatch xuiCodeDocumentBatchPrepare
#define xl6Xui3CaptureEditorDocumentBatch xuiCodeDocumentBatchCapture
#define xl6Xge3FontPolicy (*xl6Xge3FontPolicyGet())
#define xl6Xge3TexturePolicy (*xl6Xge3TexturePolicyGet())
#define xl6Xge3ImagePolicy (*xl6Xge3ImagePolicyGet())
#define xl6Xge3MaterialPolicy (*xl6Xge3MaterialPolicyGet())
#define xl6Xge3ShapePolicy (*xl6Xge3ShapePolicyGet())
#define xl6Xge3ShapeScenePolicy (*xl6Xge3ShapeScenePolicyGet())
#define xl6Xge3PathMeasurePolicy (*xl6Xge3PathMeasurePolicyGet())
#define xl6Xge3SvgPolicy (*xl6Xge3SvgPolicyGet())
#define xl6Xge3SvgPaintPolicy (*xl6Xge3SvgPaintPolicyGet())
#define xl6Xge3SvgCachePolicy (*xl6Xge3SvgCachePolicyGet())
#define xl6Xge3AudioClipPolicy (*xl6Xge3AudioClipPolicyGet())
#define xl6Xui3InventoryIndicesPolicy (*xl6Xui3InventoryIndicesPolicyGet())
#endif
