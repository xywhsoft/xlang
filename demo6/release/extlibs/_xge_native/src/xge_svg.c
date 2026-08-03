#define XGE_SVG_MAGIC 0x58475356u
#define XGE_SVG_ATTR_MAX 4096
#define XGE_SVG_STACK_MAX 128
#define XGE_SVG_DASH_MAX 32
#define XGE_SVG_ID_MAX 128
#define XGE_SVG_STYLE_SELECTOR_TAG 0
#define XGE_SVG_STYLE_SELECTOR_CLASS 1
#define XGE_SVG_STYLE_SELECTOR_TAG_CLASS 4
#define XGE_SVG_CLIP_USER_SPACE 0
#define XGE_SVG_CLIP_OBJECT_BOUNDING_BOX 1
#define XGE_SVG_MASK_USER_SPACE 0
#define XGE_SVG_MASK_OBJECT_BOUNDING_BOX 1
#define XGE_SVG_MASK_TYPE_LUMINANCE 0
#define XGE_SVG_MASK_TYPE_ALPHA 1
#define XGE_SVG_PATTERN_USER_SPACE 0
#define XGE_SVG_PATTERN_OBJECT_BOUNDING_BOX 1
#define XGE_SVG_FILTER_UNITS_USER_SPACE 0
#define XGE_SVG_FILTER_UNITS_OBJECT_BOUNDING_BOX 1
#define XGE_SVG_LENGTH_BASIS_X 0
#define XGE_SVG_LENGTH_BASIS_Y 1
#define XGE_SVG_LENGTH_BASIS_OTHER 2
#define XGE_SVG_STYLE_IMPORTANCE_ANY -1
#define XGE_SVG_STYLE_IMPORTANCE_NORMAL 0
#define XGE_SVG_STYLE_IMPORTANCE_IMPORTANT 1
#define XGE_SVG_STYLE_APPLY_ALL 0
#define XGE_SVG_STYLE_APPLY_FONT_SIZE 1
#define XGE_SVG_STYLE_APPLY_EXCEPT_FONT_SIZE 2
#define XGE_SVG_TEXT_ANCHOR_START 0
#define XGE_SVG_TEXT_ANCHOR_MIDDLE 1
#define XGE_SVG_TEXT_ANCHOR_END 2
#define XGE_SVG_TEXT_SPACE_DEFAULT 0
#define XGE_SVG_TEXT_SPACE_PRESERVE 1
#define XGE_SVG_DEFAULT_FONT_SIZE 10.0f
#define XGE_SVG_PAINT_FILL 0
#define XGE_SVG_PAINT_STROKE 1
#define XGE_SVG_PAINT_MARKERS 2
#define XGE_SVG_PAINT_COMPONENT_COUNT 3
#define XGE_SVG_GRADIENT_HAS_X1			0x00000001
#define XGE_SVG_GRADIENT_HAS_Y1			0x00000002
#define XGE_SVG_GRADIENT_HAS_X2			0x00000004
#define XGE_SVG_GRADIENT_HAS_Y2			0x00000008
#define XGE_SVG_GRADIENT_HAS_CX			0x00000010
#define XGE_SVG_GRADIENT_HAS_CY			0x00000020
#define XGE_SVG_GRADIENT_HAS_R			0x00000040
#define XGE_SVG_GRADIENT_HAS_FX			0x00000080
#define XGE_SVG_GRADIENT_HAS_FY			0x00000100
#define XGE_SVG_GRADIENT_HAS_FR			0x00004000
#define XGE_SVG_GRADIENT_HAS_UNITS		0x00000200
#define XGE_SVG_GRADIENT_HAS_SPREAD		0x00000400
#define XGE_SVG_GRADIENT_HAS_TRANSFORM	0x00000800
#define XGE_SVG_GRADIENT_HAS_STOPS		0x00001000
#define XGE_SVG_GRADIENT_RESOLVED		0x00002000
#define XGE_SVG_PATTERN_HAS_X				0x00000001
#define XGE_SVG_PATTERN_HAS_Y				0x00000002
#define XGE_SVG_PATTERN_HAS_WIDTH			0x00000004
#define XGE_SVG_PATTERN_HAS_HEIGHT			0x00000008
#define XGE_SVG_PATTERN_HAS_UNITS			0x00000010
#define XGE_SVG_PATTERN_HAS_CONTENT_UNITS	0x00000020
#define XGE_SVG_PATTERN_HAS_TRANSFORM		0x00000040
#define XGE_SVG_PATTERN_HAS_VIEWBOX			0x00000080
#define XGE_SVG_PATTERN_HAS_ASPECT			0x00000100
#define XGE_SVG_PATTERN_RESOLVED			0x00000200
#define XGE_SVG_PATTERN_HAS_OVERFLOW		0x00000400
#define XGE_SVG_DATA_IMAGE_UTF8			1
#define XGE_SVG_DATA_IMAGE_BASE64		2
#define XGE_SVG_FILTER_PRIMITIVE_REGION_X 0x01
#define XGE_SVG_FILTER_PRIMITIVE_REGION_Y 0x02
#define XGE_SVG_FILTER_PRIMITIVE_REGION_W 0x04
#define XGE_SVG_FILTER_PRIMITIVE_REGION_H 0x08
#define XGE_SVG_BOUNDS_MAX_DEPTH 32
#define XGE_SVG_DRAW_ITEM_SHAPE 1
#define XGE_SVG_DRAW_ITEM_TEXT 2
#define XGE_SVG_DRAW_ITEM_SVG_IMAGE 3
#define XGE_SVG_DRAW_ITEM_RASTER_IMAGE 4
#define XGE_SVG_DRAW_ITEM_SCENE 5
#define XGE_SVG_DRAW_ITEM_FILTER 6
#define XGE_SVG_DRAW_ITEM_GROUP 7
#define XGE_SVG_DRAW_ITEM_PENDING_USE 8
#define XGE_SVG_PENDING_SHAPE_FILL_GRADIENT 0x01
#define XGE_SVG_PENDING_SHAPE_STROKE_GRADIENT 0x02
#define XGE_SVG_PENDING_SHAPE_CLIP 0x04
#define XGE_SVG_RUNTIME_COMPOSITION_MASK 0x01
#define XGE_SVG_RUNTIME_COMPOSITION_CLIP 0x02
#define XGE_SVG_HIERARCHY_COMPOSITION_MASK(iLevel) (1u << ((unsigned)(iLevel) * 2u))
#define XGE_SVG_HIERARCHY_COMPOSITION_CLIP(iLevel) (1u << ((unsigned)(iLevel) * 2u + 1u))
#define XGE_SVG_FLOAT_ABS_MAX 3.40282346638528860e+38
#define XGE_SVG_EPSILON 0.00001f
#define XGE_SVG_FONT_FAMILY_MAX 128

typedef struct xge_svg_style_t {
	uint32_t iCurrentColor;
	uint32_t iFillColor;
	char sFillGradientId[XGE_SVG_ID_MAX];
	uint32_t iStrokeColor;
	char sStrokeGradientId[XGE_SVG_ID_MAX];
	uint32_t iStopColor;
	float fOpacity;
	float fFillOpacity;
	float fStrokeOpacity;
	float fStopOpacity;
	float fStrokeWidth;
	float fMiterLimit;
	float fFontSize;
	char sFontFamily[XGE_SVG_FONT_FAMILY_MAX];
	float fDashPattern[XGE_SVG_DASH_MAX];
	float fDashOffset;
	char sClipId[XGE_SVG_ID_MAX];
	char sMaskId[XGE_SVG_ID_MAX];
	char sFilterId[XGE_SVG_ID_MAX];
	int iDashCount;
	int iFillRule;
	int bFillRuleSet;
	int iLineCap;
	int iLineJoin;
	int iTextAnchor;
	int iTextSpace;
	int iBlend;
	int iMaskType;
	int iPaintOrder[XGE_SVG_PAINT_COMPONENT_COUNT];
	int bStrokeFirst;
	int bVisible;
	int bVisibility;
	int bBlendSet;
	int bStopColorSet;
	int bStopOpacitySet;
	int bColorSet;
	int bFillSet;
	int bFillCurrentColor;
	int bFillOpacitySet;
	int bFillOpacityNonFinite;
	int bStrokeSet;
	int bStrokeCurrentColor;
	int bStrokeOpacitySet;
	int bStrokeOpacityNonFinite;
	int bStrokeWidthSet;
	int bStrokeNonScaling;
	int bClipPathLocal;
	int bViewportClip;
	xge_rect_t tViewportClip;
	xge_shape_ex_matrix_t tTransform;
} xge_svg_style_t;

typedef struct xge_svg_cache_entry_t {
	char* sURI;
	xge_svg pSvg;
	struct xge_svg_cache_entry_t* pNext;
} xge_svg_cache_entry_t;

typedef struct xge_svg_text_item_t {
	char* sText;
	xge_svg_style_t tStyle;
	float fX;
	float fY;
	float fAnchorAdvance;
	int bClipRect;
	xge_rect_t tClipRect;
	xge_shape_ex_scene pMaskScene;
	int iMaskMethod;
} xge_svg_text_item_t;

typedef struct xge_svg_image_item_t {
	xge_svg pSvg;
	xge_svg_style_t tStyle;
	xge_rect_t tRect;
	int bClipRect;
	xge_rect_t tClipRect;
	xge_shape_ex_scene pMaskScene;
	int iMaskMethod;
} xge_svg_image_item_t;

typedef struct xge_svg_raster_image_t {
	int iRefCount;
	xge_image_t tImage;
	xge_texture_t tTexture;
} xge_svg_raster_image_t;

typedef struct xge_svg_raster_item_t {
	xge_svg_raster_image_t* pRaster;
	xge_svg_style_t tStyle;
	xge_rect_t tRect;
	uint32_t iColor;
	int iAspectAlignX;
	int iAspectAlignY;
	int iAspectMeetOrSlice;
	int bClipRect;
	xge_rect_t tClipRect;
	xge_shape_ex_scene pMaskScene;
	int iMaskMethod;
} xge_svg_raster_item_t;

typedef struct xge_svg_draw_item_t xge_svg_draw_item_t;

typedef struct xge_svg_filter_effect_t {
	xge_svg_draw_item_t* pItems;
	xge_rect_t tObjectBounds;
	xge_rect_t tOutputRegion;
	xge_shape_ex_matrix_t tTransform;
	int iItemCount;
	int iFilterIndex;
	int bPendingUseBounds;
} xge_svg_filter_effect_t;

typedef struct xge_svg_group_item_t {
	xge_svg_draw_item_t* pItems;
	xge_shape_ex_scene pMaskScene;
	xge_shape_ex_matrix_t tPaintTransform;
	char sMaskId[XGE_SVG_ID_MAX];
	int iItemCount;
	int iMaskMethod;
	float fOpacity;
	float fPaintOpacity;
	int bPaintTransform;
	int bPaintClipComposition;
	int bPaintOwnsClipComposition;
	int bPaintOwnsMaskComposition;
	int bMasked;
	int bMaskResolved;
	int bMaskEmpty;
} xge_svg_group_item_t;

typedef struct xge_svg_def_item_t xge_svg_def_item_t;

typedef struct xge_svg_def_group_item_t {
	xge_svg_def_item_t* pItems;
	int iItemCount;
	int iBlend;
	int bBlendSet;
	float fOpacity;
	char sMaskId[XGE_SVG_ID_MAX];
	char sFilterId[XGE_SVG_ID_MAX];
	xge_shape_ex_matrix_t tFilterTransform;
} xge_svg_def_group_item_t;

struct xge_svg_def_item_t {
	int iType;
	union {
		int iIndex;
		xge_svg_def_group_item_t* pGroup;
	} u;
};

struct xge_svg_draw_item_t {
	uint32_t iId;
	int iType;
	int iBlend;
	int bBlendSet;
	float fPaintOpacity;
	int bPaintOpacity;
	xge_shape_ex_matrix_t tRuntimeTransform;
	float fRuntimeTranslateX;
	float fRuntimeTranslateY;
	float fRuntimeScaleX;
	float fRuntimeScaleY;
	float fRuntimeRotateRadians;
	float fRuntimeOpacity;
	int iRuntimeBlend;
	int bRuntimeStateInitialized;
	int bRuntimeTransform;
	int bRuntimeTransformOverride;
	int bRuntimeOpacity;
	int bRuntimeVisible;
	int iRuntimeVisible;
	int bRuntimeBlend;
	int bRuntimeMaskSet;
	int iRuntimeMaskMethod;
	int iRuntimeMaskTargetType;
	xge_shape_ex pRuntimeMaskShape;
	xge_shape_ex_scene pRuntimeMaskTargetScene;
	xge_shape_ex_scene pRuntimeMaskOwnerScene;
	int bRuntimeClipSet;
	xge_shape_ex pRuntimeClipShape;
	xge_shape_ex_scene pRuntimeClipOwnerScene;
	xge_svg pOwner;
	struct xge_svg_draw_item_t* pParentPaint;
	xge_shape_ex pIdentityShape;
	union {
		xge_shape_ex pShape;
		xge_shape_ex_scene pScene;
		xge_svg_filter_effect_t* pFilter;
		xge_svg_group_item_t* pGroup;
		xge_svg_text_item_t tText;
		xge_svg_image_item_t tImage;
		xge_svg_raster_item_t tRaster;
		int iPendingUseSerial;
	} u;
};

typedef struct xge_svg_def_t {
	char* sId;
	char sGroupFilterId[XGE_SVG_ID_MAX];
	xge_shape_ex* pShapes;
	xge_svg_style_t* pShapeStyles;
	xge_svg_text_item_t* pTexts;
	xge_svg_image_item_t* pImages;
	xge_svg_raster_item_t* pRasters;
	xge_svg_def_item_t* pItems;
	xge_rect_t tViewBox;
	float fWidth;
	float fHeight;
	int iShapeCount;
	int iShapeCapacity;
	int iTextCount;
	int iTextCapacity;
	int iImageCount;
	int iImageCapacity;
	int iRasterCount;
	int iRasterCapacity;
	int iItemCount;
	int iItemCapacity;
	int bPatternContent;
	int bHasViewBox;
	int bHasWidth;
	int bHasHeight;
	int bOverflowVisible;
	int iAspectAlignX;
	int iAspectAlignY;
	int iAspectMeetOrSlice;
	xge_shape_ex_matrix_t tGroupFilterTransform;
} xge_svg_def_t;

typedef struct xge_svg_pattern_t {
	char* sId;
	char* sHrefId;
	int iDefIndex;
	xge_rect_t tRect;
	int iUnits;
	int iContentUnits;
	xge_shape_ex_matrix_t tTransform;
	int iFlags;
} xge_svg_pattern_t;

typedef struct xge_svg_clip_path_t {
	char* sId;
	xge_rect_t* pRects;
	xge_shape_ex* pShapes;
	int iUnits;
	int iRectCount;
	int iRectCapacity;
	int iShapeCount;
	int iShapeCapacity;
	int bHasRect;
	int bHasShape;
} xge_svg_clip_path_t;

typedef struct xge_svg_mask_shape_t {
	xge_shape_ex pShape;
} xge_svg_mask_shape_t;

typedef struct xge_svg_mask_t {
	char* sId;
	xge_svg_mask_shape_t* pShapes;
	xge_rect_t tRegion;
	int iShapeCount;
	int iShapeCapacity;
	int iUnits;
	int iContentUnits;
	int iType;
} xge_svg_mask_t;

typedef struct xge_svg_filter_primitive_t {
	xge_rect_t tRegion;
	float fGaussianBlurX;
	float fGaussianBlurY;
	int iRegionFlags;
	int iRegionPercentFlags;
} xge_svg_filter_primitive_t;

typedef struct xge_svg_filter_t {
	char* sId;
	xge_rect_t tRegion;
	xge_svg_filter_primitive_t* pPrimitives;
	int iFilterUnits;
	int iUnits;
	int iPrimitiveCount;
	int iPrimitiveCapacity;
} xge_svg_filter_t;

typedef struct xge_svg_linear_gradient_t {
	char* sId;
	char* sHrefId;
	float fX1;
	float fY1;
	float fX2;
	float fY2;
	int iUnits;
	int iSpread;
	int iFlags;
	int iPercentFlags;
	xge_svg_style_t tStyle;
	uint32_t iStopCurrentColor;
	int bStopCurrentColorSet;
	xge_shape_ex_matrix_t tTransform;
	xge_shape_ex_color_stop_t* pStops;
	int iStopCount;
	int iStopCapacity;
} xge_svg_linear_gradient_t;

typedef struct xge_svg_radial_gradient_t {
	char* sId;
	char* sHrefId;
	float fCX;
	float fCY;
	float fR;
	float fFX;
	float fFY;
	float fFR;
	int iUnits;
	int iSpread;
	int iFlags;
	int iPercentFlags;
	xge_svg_style_t tStyle;
	uint32_t iStopCurrentColor;
	int bStopCurrentColorSet;
	xge_shape_ex_matrix_t tTransform;
	xge_shape_ex_color_stop_t* pStops;
	int iStopCount;
	int iStopCapacity;
} xge_svg_radial_gradient_t;

typedef struct xge_svg_style_rule_t {
	int iSelectorType;
	char* sName;
	char* sStyle;
	const char* pSourcePosition;
} xge_svg_style_rule_t;

typedef struct xge_svg_element_stack_item_t {
	const char* pTag;
	const char* pTagEnd;
	int iNodeIndex;
} xge_svg_element_stack_item_t;

typedef struct xge_svg_element_node_t {
	const char* pTag;
	const char* pTagEnd;
	int iParentIndex;
	int iPrevSiblingIndex;
	int iNextSiblingIndex;
	int iChildCount;
	int bHasTextContent;
} xge_svg_element_node_t;

typedef struct xge_svg_pending_use_t {
	char sId[XGE_SVG_ID_MAX];
	xge_svg_style_t tStyle;
	float fX;
	float fY;
	float fW;
	float fH;
	int bHasWidth;
	int bHasHeight;
	int iTargetDef;
	int iMainIdDefIndex;
	int iExtraMainIdDefIndex;
	int iSerial;
	int bResolved;
} xge_svg_pending_use_t;

typedef struct xge_svg_pending_shape_resource_t {
	xge_shape_ex pShape;
	xge_svg_style_t tStyle;
	int iFlags;
} xge_svg_pending_shape_resource_t;

typedef struct xge_svg_font_cache_t {
	float fSize;
	char sFamily[XGE_SVG_FONT_FAMILY_MAX];
	xge_font_t tFont;
} xge_svg_font_cache_t;

typedef struct xge_svg_font_face_t {
	char* sFamily;
	unsigned char* pData;
	int iSize;
	xge_font_face pFace;
	int bLoadAttempted;
} xge_svg_font_face_t;

struct xge_svg_t {
	uint32_t iMagic;
	int iRefCount;
	xge_svg_draw_item_t tPicturePaint;
	xge_svg_draw_item_t tRootPaint;
	xge_svg_draw_item_t tContentPaint;
	xge_svg_draw_item_t tDocumentPaint;
	xge_svg_draw_item_t* pItems;
	int iItemCount;
	int iItemCapacity;
	xge_svg_def_t* pDefs;
	int iDefCount;
	int iDefCapacity;
	xge_svg_pattern_t* pPatterns;
	int iPatternCount;
	int iPatternCapacity;
	xge_svg_clip_path_t* pClipPaths;
	int iClipPathCount;
	int iClipPathCapacity;
	xge_svg_mask_t* pMasks;
	int iMaskCount;
	int iMaskCapacity;
	xge_svg_filter_t* pFilters;
	int iFilterCount;
	int iFilterCapacity;
	xge_svg_linear_gradient_t* pLinearGradients;
	int iLinearGradientCount;
	int iLinearGradientCapacity;
	xge_svg_radial_gradient_t* pRadialGradients;
	int iRadialGradientCount;
	int iRadialGradientCapacity;
	xge_svg_style_rule_t* pStyleRules;
	int iStyleRuleCount;
	int iStyleRuleCapacity;
	xge_svg_pending_use_t* pPendingUses;
	int iPendingUseCount;
	int iPendingUseCapacity;
	xge_svg_pending_shape_resource_t* pPendingShapeResources;
	int iPendingShapeResourceCount;
	int iPendingShapeResourceCapacity;
	xge_svg_font_cache_t* pFontCaches;
	int iFontCacheCount;
	int iFontCacheCapacity;
	xge_svg_font_face_t* pFontFaces;
	int iFontFaceCount;
	int iFontFaceCapacity;
	xge_rect_t tViewBox;
	int bHasViewBox;
	float fIntrinsicWidth;
	float fIntrinsicHeight;
	float fWidth;
	float fHeight;
	int bLoaded;
	float fOriginX;
	float fOriginY;
	int iAspectAlignX;
	int iAspectAlignY;
	int iAspectMeetOrSlice;
	char* sBaseDir;
	xge_svg_element_node_t* pElementNodes;
	int iElementNodeCount;
	int iElementNodeCapacity;
	int iCurrentElementNode;
	int arrLastChildByDepth[XGE_SVG_STACK_MAX];
	xge_svg_element_stack_item_t arrElementStack[XGE_SVG_STACK_MAX];
	int iElementStackTop;
};

static xge_svg_cache_entry_t* g_xgeSvgCacheHead;

typedef struct xge_svg_gaussian_renderer_t {
	xge_shader_t tShader;
	int bInitialized;
} xge_svg_gaussian_renderer_t;

typedef struct xge_svg_gaussian_kernel_t {
	int arrRadius[3];
	int iLevel;
	int iExtent;
} xge_svg_gaussian_kernel_t;

static xge_svg_gaussian_renderer_t g_xgeSvgGaussianRenderer;

static int __xgeSvgTagNameEquals(const char* pTag, const char* sName);
static int __xgeSvgLoadMemoryEx(xge_svg pSvg, const void* pData, int iSize, const char* sBaseDir);
static int __xgeSvgRasterizeMemoryEx(const void* pData, int iSize, int iWidth, int iHeight, void* pPixels, int iStride, const char* sBaseDir);
static int __xgeSvgTextureLoadMemoryEx(xge_texture pTexture, const void* pData, int iSize, int iWidth, int iHeight, const char* sBaseDir);
static int __xgeSvgColorNameEquals(const char* sText, const char* sName);
static int __xgeSvgParseColor(const char* sText, uint32_t* pColor);
static int __xgeSvgParseColorThorvg(const char* sText, uint32_t* pColor);
static void __xgeSvgSkipSpaces(const char** ppText);
static xge_rect_t __xgeSvgObjectRectToBounds(xge_rect_t tRect, xge_rect_t tBounds);
static xge_shape_ex_matrix_t __xgeSvgMatrixIdentity(void);
static xge_shape_ex_matrix_t __xgeSvgMatrixMul(xge_shape_ex_matrix_t tParent, xge_shape_ex_matrix_t tLocal);
static int __xgeSvgMatrixInvert(xge_shape_ex_matrix_t tMatrix, xge_shape_ex_matrix_t* pOut);
static xge_vec2_t __xgeSvgMatrixPoint(xge_shape_ex_matrix_t tMatrix, xge_vec2_t tPoint);
static xge_rect_t __xgeSvgMatrixRectBounds(xge_shape_ex_matrix_t tMatrix, xge_rect_t tRect);
static xge_rect_t __xgeSvgRectIntersect(xge_rect_t a, xge_rect_t b);
static xge_rect_t __xgeSvgRectUnion(xge_rect_t a, xge_rect_t b);
static int __xgeSvgFloatFinite(float fValue);
static int __xgeSvgMaskFind(xge_svg pSvg, const char* sId);
static int __xgeSvgMaskMethod(const xge_svg_mask_t* pMask);
static int __xgeSvgMaskSceneResolve(const xge_svg_mask_t* pMask, xge_rect_t tBounds, xge_shape_ex_scene* ppScene);
static int __xgeSvgDrawItemBoundsWithParent(xge_svg pSvg, xge_svg_draw_item_t* pItem, xge_shape_ex_matrix_t tParent, float fTolerance, xge_rect_t* pBounds, int iDepth);
static int __xgeSvgGetBoundsWithParent(xge_svg pSvg, xge_shape_ex_matrix_t tParent, float fTolerance, xge_rect_t* pBounds, int iDepth);
static int __xgeSvgDrawItemGetOBB(xge_svg pSvg, const xge_svg_draw_item_t* pItem, float fTolerance, xge_vec2_t* pPoints4, int iDepth);
static int __xgeSvgDrawItemIntersectsWithParent(xge_svg pSvg, const xge_svg_draw_item_t* pItem, xge_shape_ex_matrix_t tParent, xge_rect_t tRect, float fTolerance, int* pIntersects, int iDepth);
static int __xgeSvgGroupChildrenBoundsWithParent(xge_svg pSvg, xge_svg_group_item_t* pGroup, xge_shape_ex_matrix_t tParent, float fTolerance, xge_rect_t* pBounds, int* pHasBounds, int iDepth);
static int __xgeSvgGroupMaskSceneEnsure(xge_svg pSvg, xge_svg_group_item_t* pGroup, xge_rect_t tLocalBounds);
static void __xgeSvgPaintTreeBind(xge_svg pSvg);
static int __xgeSvgWrapGroupFilter(xge_svg pSvg, int iFirstItem, int iFilterIndex, xge_shape_ex_matrix_t tTransform);
static int __xgeSvgFilterEffectRefreshPendingUseBounds(xge_svg pSvg, xge_svg_filter_effect_t* pEffect);

static int __xgeSvgValid(xge_svg pSvg)
{
	return (pSvg != NULL) && (pSvg->iMagic == XGE_SVG_MAGIC);
}

static char* __xgeSvgUriBaseDir(const char* sURI)
{
	const char* pLastSlash;
	const char* p;
	size_t iLen;
	char* sBase;

	if ( sURI == NULL ) {
		return NULL;
	}
	pLastSlash = NULL;
	for ( p = sURI; *p != '\0'; p++ ) {
		if ( (*p == '/') || (*p == '\\') ) {
			pLastSlash = p;
		}
	}
	if ( pLastSlash == NULL ) {
		return __xgeStrDup("");
	}
	iLen = (size_t)(pLastSlash - sURI) + 1u;
	sBase = (char*)xrtMalloc(iLen + 1u);
	if ( sBase == NULL ) {
		return NULL;
	}
	memcpy(sBase, sURI, iLen);
	sBase[iLen] = '\0';
	return sBase;
}

static int __xgeSvgUriIsAbsolute(const char* sURI)
{
	if ( (sURI == NULL) || (sURI[0] == '\0') ) {
		return 0;
	}
	if ( __xgeUriSchemeLen(sURI) > 0 ) {
		return 1;
	}
	return xrtPathIsAbs((str)sURI, 0) ? 1 : 0;
}

static int __xgeSvgRangeDupTrimmed(const char* sText, char** ppOut)
{
	const char* pStart;
	const char* pEnd;
	size_t iLen;
	char* sOut;

	if ( (sText == NULL) || (ppOut == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppOut = NULL;
	pStart = sText;
	while ( (*pStart == ' ') || (*pStart == '\t') || (*pStart == '\r') || (*pStart == '\n') || (*pStart == '\f') ) {
		pStart++;
	}
	pEnd = pStart + strlen(pStart);
	while ( (pEnd > pStart) &&
	        ((pEnd[-1] == ' ') || (pEnd[-1] == '\t') || (pEnd[-1] == '\r') || (pEnd[-1] == '\n') || (pEnd[-1] == '\f')) ) {
		pEnd--;
	}
	iLen = (size_t)(pEnd - pStart);
	sOut = (char*)xrtMalloc(iLen + 1u);
	if ( sOut == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	memcpy(sOut, pStart, iLen);
	sOut[iLen] = '\0';
	*ppOut = sOut;
	return XGE_OK;
}

static char* __xgeSvgResolveHrefUri(xge_svg pSvg, const char* sHref)
{
	char* sTrimmed;
	char* sResolved;
	size_t iBaseLen;
	size_t iHrefLen;
	int iRet;

	if ( (sHref == NULL) || (sHref[0] == '\0') ) {
		return NULL;
	}
	sTrimmed = NULL;
	iRet = __xgeSvgRangeDupTrimmed(sHref, &sTrimmed);
	if ( iRet != XGE_OK ) {
		return NULL;
	}
	if ( (sTrimmed[0] == '\0') || (sTrimmed[0] == '#') || __xgeSvgUriIsAbsolute(sTrimmed) ||
	     !__xgeSvgValid(pSvg) || (pSvg->sBaseDir == NULL) || (pSvg->sBaseDir[0] == '\0') ) {
		return sTrimmed;
	}
	iBaseLen = strlen(pSvg->sBaseDir);
	iHrefLen = strlen(sTrimmed);
	if ( iBaseLen > (SIZE_MAX - iHrefLen - 2u) ) {
		xrtFree(sTrimmed);
		return NULL;
	}
	sResolved = (char*)xrtMalloc(iBaseLen + iHrefLen + 2u);
	if ( sResolved == NULL ) {
		xrtFree(sTrimmed);
		return NULL;
	}
	memcpy(sResolved, pSvg->sBaseDir, iBaseLen);
	if ( (iBaseLen > 0) && (pSvg->sBaseDir[iBaseLen - 1] != '/') && (pSvg->sBaseDir[iBaseLen - 1] != '\\') ) {
		sResolved[iBaseLen++] = '/';
	}
	memcpy(sResolved + iBaseLen, sTrimmed, iHrefLen + 1u);
	xrtFree(sTrimmed);
	return sResolved;
}

static int __xgeSvgBaseDirSet(xge_svg pSvg, const char* sBaseDir)
{
	char* sCopy;

	if ( !__xgeSvgValid(pSvg) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	sCopy = NULL;
	if ( (sBaseDir != NULL) && (sBaseDir[0] != '\0') ) {
		sCopy = __xgeStrDup(sBaseDir);
		if ( sCopy == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
	}
	xrtFree(pSvg->sBaseDir);
	pSvg->sBaseDir = sCopy;
	return XGE_OK;
}

static int __xgeSvgRasterImageCreateFromMemory(const void* pData, int iSize, xge_svg_raster_image_t** ppImage)
{
	xge_svg_raster_image_t* pImage;
	int iRet;

	if ( (pData == NULL) || (iSize <= 0) || (ppImage == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppImage = NULL;
	pImage = (xge_svg_raster_image_t*)xrtCalloc(1, sizeof(*pImage));
	if ( pImage == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iRet = xgeImageLoadMemoryEx(&pImage->tImage, pData, iSize, XGE_IMAGE_PREMULTIPLIED);
	if ( iRet != XGE_OK ) {
		xrtFree(pImage);
		return iRet;
	}
	pImage->iRefCount = 1;
	*ppImage = pImage;
	return XGE_OK;
}

static int __xgeSvgRasterImageAddRef(xge_svg_raster_image_t* pImage)
{
	if ( pImage == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pImage->iRefCount < INT32_MAX ) {
		pImage->iRefCount++;
	}
	return pImage->iRefCount;
}

static void __xgeSvgRasterImageDestroy(xge_svg_raster_image_t* pImage)
{
	if ( pImage == NULL ) {
		return;
	}
	if ( pImage->iRefCount > 1 ) {
		pImage->iRefCount--;
		return;
	}
	xgeTextureFree(&pImage->tTexture);
	xgeImageFree(&pImage->tImage);
	memset(pImage, 0, sizeof(*pImage));
	xrtFree(pImage);
}

static int __xgeSvgRasterImageEnsureTexture(xge_svg_raster_image_t* pImage)
{
	if ( (pImage == NULL) || (pImage->tImage.pPixels == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pImage->tTexture.iRefCount > 0 ) {
		return XGE_OK;
	}
	return xgeTextureCreateFromImage(&pImage->tTexture, &pImage->tImage);
}

static void __xgeSvgDrawItemReset(xge_svg_draw_item_t* pItem)
{
	int i;

	if ( pItem == NULL ) {
		return;
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_SHAPE ) {
		xgeShapeExDestroy(pItem->u.pShape);
	} else if ( pItem->iType == XGE_SVG_DRAW_ITEM_SCENE ) {
		xgeShapeExSceneDestroy(pItem->u.pScene);
	} else if ( pItem->iType == XGE_SVG_DRAW_ITEM_FILTER ) {
		if ( pItem->u.pFilter != NULL ) {
			for ( i = 0; i < pItem->u.pFilter->iItemCount; i++ ) {
				__xgeSvgDrawItemReset(&pItem->u.pFilter->pItems[i]);
			}
			xrtFree(pItem->u.pFilter->pItems);
			xrtFree(pItem->u.pFilter);
		}
	} else if ( pItem->iType == XGE_SVG_DRAW_ITEM_GROUP ) {
		if ( pItem->u.pGroup != NULL ) {
			for ( i = 0; i < pItem->u.pGroup->iItemCount; i++ ) {
				__xgeSvgDrawItemReset(&pItem->u.pGroup->pItems[i]);
			}
			xgeShapeExSceneDestroy(pItem->u.pGroup->pMaskScene);
			xrtFree(pItem->u.pGroup->pItems);
			xrtFree(pItem->u.pGroup);
		}
	} else if ( pItem->iType == XGE_SVG_DRAW_ITEM_TEXT ) {
		xrtFree(pItem->u.tText.sText);
		xgeShapeExSceneDestroy(pItem->u.tText.pMaskScene);
	} else if ( pItem->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE ) {
		xgeSvgDestroy(pItem->u.tImage.pSvg);
		xgeShapeExSceneDestroy(pItem->u.tImage.pMaskScene);
	} else if ( pItem->iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE ) {
		__xgeSvgRasterImageDestroy(pItem->u.tRaster.pRaster);
		xgeShapeExSceneDestroy(pItem->u.tRaster.pMaskScene);
	}
	memset(pItem, 0, sizeof(*pItem));
}

static void __xgeSvgSyntheticPaintReset(xge_svg_draw_item_t* pPaint)
{
	xge_svg pOwner;
	xge_svg_draw_item_t* pParent;
	int iType;

	if ( pPaint == NULL ) return;
	pOwner = pPaint->pOwner;
	pParent = pPaint->pParentPaint;
	iType = pPaint->iType;
	xgeShapeExSceneDestroy(pPaint->pRuntimeMaskOwnerScene);
	xgeShapeExSceneDestroy(pPaint->pRuntimeClipOwnerScene);
	memset(pPaint, 0, sizeof(*pPaint));
	pPaint->pOwner = pOwner;
	pPaint->pParentPaint = pParent;
	pPaint->iType = iType;
}

static void __xgeSvgDrawItemsClear(xge_svg pSvg)
{
	int i;

	if ( !__xgeSvgValid(pSvg) ) {
		return;
	}
	for ( i = 0; i < pSvg->iItemCount; i++ ) {
		__xgeSvgDrawItemReset(&pSvg->pItems[i]);
	}
	xrtFree(pSvg->pItems);
	pSvg->pItems = NULL;
	pSvg->iItemCount = 0;
	pSvg->iItemCapacity = 0;
}

static int __xgeSvgDrawItemReserve(xge_svg pSvg)
{
	xge_svg_draw_item_t* pItems;
	int iCapacity;

	if ( !__xgeSvgValid(pSvg) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pSvg->iItemCount < pSvg->iItemCapacity ) {
		return XGE_OK;
	}
	iCapacity = pSvg->iItemCapacity > 0 ? pSvg->iItemCapacity * 2 : 32;
	pItems = (xge_svg_draw_item_t*)xrtRealloc(pSvg->pItems, (size_t)iCapacity * sizeof(*pItems));
	if ( pItems == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	memset(pItems + pSvg->iItemCapacity, 0, (size_t)(iCapacity - pSvg->iItemCapacity) * sizeof(*pItems));
	pSvg->pItems = pItems;
	pSvg->iItemCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeSvgDrawItemsWrapFilter(
	xge_svg pSvg,
	int iFirstItem,
	int iFilterIndex,
	xge_rect_t tObjectBounds,
	xge_rect_t tOutputRegion,
	xge_shape_ex_matrix_t tTransform
)
{
	xge_svg_filter_effect_t* pEffect;
	xge_svg_draw_item_t* pItem;
	int iItemCount;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (iFirstItem < 0) || (iFirstItem > pSvg->iItemCount) ||
	     (iFilterIndex < 0) || (iFilterIndex >= pSvg->iFilterCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iItemCount = pSvg->iItemCount - iFirstItem;
	if ( iItemCount <= 0 ) {
		return XGE_OK;
	}
	pEffect = (xge_svg_filter_effect_t*)xrtMalloc(sizeof(*pEffect));
	if ( pEffect == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	memset(pEffect, 0, sizeof(*pEffect));
	pEffect->pItems = (xge_svg_draw_item_t*)xrtMalloc((size_t)iItemCount * sizeof(*pEffect->pItems));
	if ( pEffect->pItems == NULL ) {
		xrtFree(pEffect);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	memcpy(pEffect->pItems, pSvg->pItems + iFirstItem, (size_t)iItemCount * sizeof(*pEffect->pItems));
	memset(pSvg->pItems + iFirstItem, 0, (size_t)iItemCount * sizeof(*pSvg->pItems));
	pEffect->iItemCount = iItemCount;
	pEffect->iFilterIndex = iFilterIndex;
	pEffect->tObjectBounds = tObjectBounds;
	pEffect->tOutputRegion = tOutputRegion;
	pEffect->tTransform = tTransform;
	pSvg->iItemCount = iFirstItem;
	iRet = __xgeSvgDrawItemReserve(pSvg);
	if ( iRet != XGE_OK ) {
		int i;

		for ( i = 0; i < pEffect->iItemCount; i++ ) {
			__xgeSvgDrawItemReset(&pEffect->pItems[i]);
		}
		xrtFree(pEffect->pItems);
		xrtFree(pEffect);
		return iRet;
	}
	pItem = &pSvg->pItems[pSvg->iItemCount++];
	memset(pItem, 0, sizeof(*pItem));
	pItem->iType = XGE_SVG_DRAW_ITEM_FILTER;
	pItem->u.pFilter = pEffect;
	return XGE_OK;
}

static int __xgeSvgDrawItemHasMask(const xge_svg_draw_item_t* pItem)
{
	int i;
	int iTargetType;

	if ( pItem == NULL ) return 0;
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_SHAPE ) {
		iTargetType = XGE_SHAPE_EX_MASK_TARGET_NONE;
		return (xgeShapeExMaskGet(pItem->u.pShape, NULL, &iTargetType, NULL, NULL) == XGE_OK) &&
		       (iTargetType != XGE_SHAPE_EX_MASK_TARGET_NONE);
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_SCENE ) {
		int iCount;

		iTargetType = XGE_SHAPE_EX_MASK_TARGET_NONE;
		if ( (xgeShapeExSceneMaskGet(pItem->u.pScene, NULL, &iTargetType, NULL, NULL) == XGE_OK) &&
		     (iTargetType != XGE_SHAPE_EX_MASK_TARGET_NONE) ) {
			return 1;
		}
		iCount = 0;
		if ( xgeShapeExSceneGetCount(pItem->u.pScene, &iCount) != XGE_OK ) return 0;
		for ( i = 0; i < iCount; i++ ) {
			xge_shape_ex pShape = NULL;

			if ( xgeShapeExSceneGetAt(pItem->u.pScene, i, &pShape) != XGE_OK ) continue;
			iTargetType = XGE_SHAPE_EX_MASK_TARGET_NONE;
			if ( (xgeShapeExMaskGet(pShape, NULL, &iTargetType, NULL, NULL) == XGE_OK) &&
			     (iTargetType != XGE_SHAPE_EX_MASK_TARGET_NONE) ) {
				return 1;
			}
		}
		return 0;
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_TEXT ) return pItem->u.tText.pMaskScene != NULL;
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE ) return pItem->u.tImage.pMaskScene != NULL;
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE ) return pItem->u.tRaster.pMaskScene != NULL;
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_FILTER) && (pItem->u.pFilter != NULL) ) {
		for ( i = 0; i < pItem->u.pFilter->iItemCount; i++ ) {
			if ( __xgeSvgDrawItemHasMask(&pItem->u.pFilter->pItems[i]) ) return 1;
		}
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_GROUP) && (pItem->u.pGroup != NULL) ) {
		if ( pItem->u.pGroup->bMasked ) return 1;
		for ( i = 0; i < pItem->u.pGroup->iItemCount; i++ ) {
			if ( __xgeSvgDrawItemHasMask(&pItem->u.pGroup->pItems[i]) ) return 1;
		}
	}
	return 0;
}

static int __xgeSvgDrawItemHasPendingUse(const xge_svg_draw_item_t* pItem)
{
	int i;

	if ( pItem == NULL ) return 0;
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_PENDING_USE ) return 1;
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_GROUP) && (pItem->u.pGroup != NULL) ) {
		for ( i = 0; i < pItem->u.pGroup->iItemCount; i++ ) {
			if ( __xgeSvgDrawItemHasPendingUse(&pItem->u.pGroup->pItems[i]) ) return 1;
		}
	} else if ( (pItem->iType == XGE_SVG_DRAW_ITEM_FILTER) &&
	            (pItem->u.pFilter != NULL) ) {
		for ( i = 0; i < pItem->u.pFilter->iItemCount; i++ ) {
			if ( __xgeSvgDrawItemHasPendingUse(&pItem->u.pFilter->pItems[i]) ) return 1;
		}
	}
	return 0;
}

static int __xgeSvgDrawItemsHavePendingUse(const xge_svg_draw_item_t* pItems, int iItemCount)
{
	int i;

	if ( (pItems == NULL) || (iItemCount <= 0) ) return 0;
	for ( i = 0; i < iItemCount; i++ ) {
		if ( __xgeSvgDrawItemHasPendingUse(&pItems[i]) ) return 1;
	}
	return 0;
}

static int __xgeSvgDrawItemMaskedOpacity(const xge_svg_draw_item_t* pItem, float* pOpacity)
{
	int i;
	int iCount;
	int iTargetType;

	if ( (pItem == NULL) || (pOpacity == NULL) ) return 0;
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_SCENE ) {
		iTargetType = XGE_SHAPE_EX_MASK_TARGET_NONE;
		if ( (xgeShapeExSceneMaskGet(pItem->u.pScene, NULL, &iTargetType, NULL, NULL) == XGE_OK) &&
		     (iTargetType != XGE_SHAPE_EX_MASK_TARGET_NONE) ) {
			return xgeShapeExSceneOpacityGet(pItem->u.pScene, pOpacity) == XGE_OK;
		}
		iCount = 0;
		if ( xgeShapeExSceneGetCount(pItem->u.pScene, &iCount) != XGE_OK ) return 0;
		for ( i = 0; i < iCount; i++ ) {
			xge_shape_ex pShape = NULL;

			if ( xgeShapeExSceneGetAt(pItem->u.pScene, i, &pShape) != XGE_OK ) continue;
			iTargetType = XGE_SHAPE_EX_MASK_TARGET_NONE;
			if ( (xgeShapeExMaskGet(pShape, NULL, &iTargetType, NULL, NULL) == XGE_OK) &&
			     (iTargetType != XGE_SHAPE_EX_MASK_TARGET_NONE) ) {
				return xgeShapeExOpacityGet(pShape, pOpacity) == XGE_OK;
			}
		}
		return 0;
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_TEXT) && (pItem->u.tText.pMaskScene != NULL) ) {
		*pOpacity = pItem->u.tText.tStyle.fOpacity;
		return 1;
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE) && (pItem->u.tImage.pMaskScene != NULL) ) {
		*pOpacity = pItem->u.tImage.tStyle.fOpacity;
		return 1;
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE) && (pItem->u.tRaster.pMaskScene != NULL) ) {
		*pOpacity = pItem->u.tRaster.tStyle.fOpacity;
		return 1;
	}
	return 0;
}

static void __xgeSvgSceneEnableIsolatedClipAA(xge_shape_ex_scene pScene, int bInheritedClip)
{
	int bClipped;
	int i;

	if ( pScene == NULL ) return;
	bClipped = bInheritedClip || (pScene->iClipShapeCount > 0);
	for ( i = 0; i < pScene->iChildCount; i++ ) {
		xge_shape_ex_scene_child_t* pChild = &pScene->pChildren[i];

		if ( (pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE) && (pChild->pShape != NULL) ) {
			if ( bClipped || (pChild->pShape->iClipShapeCount > 0) ) {
				pChild->pShape->bAxisAlignedFillAntialias = 1;
			}
		} else if ( (pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SCENE) &&
		            (pChild->pScene != NULL) ) {
			__xgeSvgSceneEnableIsolatedClipAA(pChild->pScene, bClipped);
		}
	}
}

static void __xgeSvgDrawItemEnableIsolatedClipAA(xge_svg_draw_item_t* pItem)
{
	int i;

	if ( pItem == NULL ) return;
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_SHAPE ) {
		if ( (pItem->u.pShape != NULL) && (pItem->u.pShape->iClipShapeCount > 0) ) {
			pItem->u.pShape->bAxisAlignedFillAntialias = 1;
		}
		return;
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_SCENE ) {
		__xgeSvgSceneEnableIsolatedClipAA(pItem->u.pScene, 0);
		return;
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_GROUP) && (pItem->u.pGroup != NULL) ) {
		for ( i = 0; i < pItem->u.pGroup->iItemCount; i++ ) {
			__xgeSvgDrawItemEnableIsolatedClipAA(&pItem->u.pGroup->pItems[i]);
		}
		return;
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_FILTER) && (pItem->u.pFilter != NULL) ) {
		for ( i = 0; i < pItem->u.pFilter->iItemCount; i++ ) {
			__xgeSvgDrawItemEnableIsolatedClipAA(&pItem->u.pFilter->pItems[i]);
		}
	}
}

static int __xgeSvgDrawItemsWrapGroup(
	xge_svg pSvg,
	int iFirstItem,
	int bBlendSet,
	int iBlend,
	float fOpacity,
	const char* sMaskId,
	uint32_t iId,
	int bForce,
	float fPaintOpacity,
	const xge_shape_ex_matrix_t* pPaintTransform
)
{
	xge_svg_group_item_t* pGroup;
	xge_svg_draw_item_t* pItem;
	int bHasMask;
	int iMask;
	int i;
	int iItemCount;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (iFirstItem < 0) || (iFirstItem > pSvg->iItemCount) ||
	     !__xgeSvgFloatFinite(fOpacity) || !__xgeSvgFloatFinite(fPaintOpacity) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iItemCount = pSvg->iItemCount - iFirstItem;
	if ( (iItemCount <= 0) && (iId == 0) ) {
		return XGE_OK;
	}
	bHasMask = (sMaskId != NULL) && (sMaskId[0] != '\0');
	iMask = bHasMask ? __xgeSvgMaskFind(pSvg, sMaskId) : -1;
	if ( (fOpacity <= 0.0f) && (iId == 0) ) {
		int i;
		for ( i = iFirstItem; i < pSvg->iItemCount; i++ ) {
			__xgeSvgDrawItemReset(&pSvg->pItems[i]);
		}
		pSvg->iItemCount = iFirstItem;
		return XGE_OK;
	}
	if ( fOpacity > 1.0f ) fOpacity = 1.0f;
	if ( !bBlendSet && !bHasMask && (fOpacity >= (1.0f - XGE_SVG_EPSILON)) &&
	     (iId == 0) && !bForce ) {
		return XGE_OK;
	}
	pGroup = (xge_svg_group_item_t*)xrtCalloc(1, sizeof(*pGroup));
	if ( pGroup == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( iItemCount > 0 ) {
		pGroup->pItems = (xge_svg_draw_item_t*)xrtMalloc((size_t)iItemCount * sizeof(*pGroup->pItems));
		if ( pGroup->pItems == NULL ) {
			xrtFree(pGroup);
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		memcpy(pGroup->pItems, pSvg->pItems + iFirstItem, (size_t)iItemCount * sizeof(*pGroup->pItems));
		memset(pSvg->pItems + iFirstItem, 0, (size_t)iItemCount * sizeof(*pSvg->pItems));
	}
	pGroup->iItemCount = iItemCount;
	pGroup->fOpacity = fOpacity;
	pGroup->fPaintOpacity = iItemCount <= 0 ? 1.0f :
		(fPaintOpacity < 0.0f ? 0.0f : (fPaintOpacity > 1.0f ? 1.0f : fPaintOpacity));
	pGroup->tPaintTransform = pPaintTransform != NULL ? *pPaintTransform : __xgeSvgMatrixIdentity();
	pGroup->bPaintTransform = pPaintTransform != NULL;
	if ( bHasMask ) {
		strncpy(pGroup->sMaskId, sMaskId, sizeof(pGroup->sMaskId) - 1);
		pGroup->sMaskId[sizeof(pGroup->sMaskId) - 1] = '\0';
		pGroup->iMaskMethod = (iMask >= 0) ? __xgeSvgMaskMethod(&pSvg->pMasks[iMask]) : XGE_SHAPE_EX_MASK_ALPHA;
		pGroup->bMasked = 1;
	}
	for ( i = 0; i < iItemCount; i++ ) {
		if ( __xgeSvgDrawItemHasMask(&pGroup->pItems[i]) ) {
			pGroup->bMasked = 1;
			break;
		}
	}
	if ( !pGroup->bMasked && (bBlendSet || (fOpacity < (1.0f - XGE_SVG_EPSILON))) ) {
		for ( i = 0; i < iItemCount; i++ ) {
			__xgeSvgDrawItemEnableIsolatedClipAA(&pGroup->pItems[i]);
		}
	}
	pSvg->iItemCount = iFirstItem;
	iRet = __xgeSvgDrawItemReserve(pSvg);
	if ( iRet != XGE_OK ) {
		int i;
		for ( i = 0; i < pGroup->iItemCount; i++ ) {
			__xgeSvgDrawItemReset(&pGroup->pItems[i]);
		}
		xrtFree(pGroup->pItems);
		xrtFree(pGroup);
		return iRet;
	}
	pItem = &pSvg->pItems[pSvg->iItemCount++];
	memset(pItem, 0, sizeof(*pItem));
	pItem->iId = iId;
	pItem->iType = XGE_SVG_DRAW_ITEM_GROUP;
	pItem->iBlend = bBlendSet ? iBlend : XGE_BLEND_ALPHA;
	pItem->bBlendSet = bBlendSet;
	pItem->u.pGroup = pGroup;
	return XGE_OK;
}

static void __xgeSvgFontFacesClear(xge_svg pSvg)
{
	int i;

	if ( !__xgeSvgValid(pSvg) ) {
		return;
	}
	for ( i = 0; i < pSvg->iFontFaceCount; i++ ) {
		xrtFree(pSvg->pFontFaces[i].sFamily);
		xrtFree(pSvg->pFontFaces[i].pData);
		xgeFontFaceFree(pSvg->pFontFaces[i].pFace);
	}
	xrtFree(pSvg->pFontFaces);
	pSvg->pFontFaces = NULL;
	pSvg->iFontFaceCount = 0;
	pSvg->iFontFaceCapacity = 0;
}

static xge_svg_font_face_t* __xgeSvgFontFaceFind(xge_svg pSvg, const char* sFamily)
{
	int i;

	if ( !__xgeSvgValid(pSvg) || (sFamily == NULL) || (sFamily[0] == '\0') ) {
		return NULL;
	}
	for ( i = pSvg->iFontFaceCount - 1; i >= 0; i-- ) {
		if ( (pSvg->pFontFaces[i].sFamily != NULL) && (strcmp(pSvg->pFontFaces[i].sFamily, sFamily) == 0) ) {
			return &pSvg->pFontFaces[i];
		}
	}
	return NULL;
}

static int __xgeSvgFontFaceAdd(xge_svg pSvg, char* sFamily, unsigned char* pData, int iSize)
{
	xge_svg_font_face_t* pFaces;
	xge_svg_font_face_t* pFace;
	int iCapacity;

	if ( !__xgeSvgValid(pSvg) || (sFamily == NULL) || (sFamily[0] == '\0') || (pData == NULL) || (iSize <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pSvg->iFontFaceCount >= pSvg->iFontFaceCapacity ) {
		iCapacity = pSvg->iFontFaceCapacity > 0 ? pSvg->iFontFaceCapacity * 2 : 4;
		pFaces = (xge_svg_font_face_t*)xrtRealloc(pSvg->pFontFaces, (size_t)iCapacity * sizeof(*pFaces));
		if ( pFaces == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		memset(pFaces + pSvg->iFontFaceCapacity, 0, (size_t)(iCapacity - pSvg->iFontFaceCapacity) * sizeof(*pFaces));
		pSvg->pFontFaces = pFaces;
		pSvg->iFontFaceCapacity = iCapacity;
	}
	pFace = &pSvg->pFontFaces[pSvg->iFontFaceCount++];
	pFace->sFamily = sFamily;
	pFace->pData = pData;
	pFace->iSize = iSize;
	return XGE_OK;
}

static xge_font_face __xgeSvgFontFaceEnsure(xge_svg_font_face_t* pFace)
{
	if ( (pFace == NULL) || pFace->bLoadAttempted ) return pFace != NULL ? pFace->pFace : NULL;
	pFace->bLoadAttempted = 1;
	if ( (pFace->pData != NULL) && (pFace->iSize > 0) ) {
		(void)xgeFontFaceLoadMemory(&pFace->pFace, pFace->pData, pFace->iSize, NULL);
	}
	xrtFree(pFace->pData);
	pFace->pData = NULL;
	pFace->iSize = 0;
	return pFace->pFace;
}

static void __xgeSvgFontCacheClear(xge_svg pSvg)
{
	int i;

	if ( !__xgeSvgValid(pSvg) ) {
		return;
	}
	for ( i = 0; i < pSvg->iFontCacheCount; i++ ) {
		xgeFontFree(&pSvg->pFontCaches[i].tFont);
	}
	xrtFree(pSvg->pFontCaches);
	pSvg->pFontCaches = NULL;
	pSvg->iFontCacheCount = 0;
	pSvg->iFontCacheCapacity = 0;
}

static float __xgeSvgFontCacheKey(float fSize)
{
	if ( fSize < 0.25f ) {
		return 0.25f;
	}
	return floorf((fSize * 4.0f) + 0.5f) * 0.25f;
}

static int __xgeSvgFontCreateFaceEm(xge_font pFont, xge_font_face pFace, float fSize)
{
	xge_font_instance_desc_t tDesc;

	if ( (pFont == NULL) || (pFace == NULL) || (fSize <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.fPixelSize = fSize;
	tDesc.iFlags = XGE_FONT_SIZE_EM;
	return xgeFontCreate(pFont, pFace, &tDesc);
}

static int __xgeSvgFontCacheGet(xge_svg pSvg, const char* sFamily, float fSize, xge_font* ppFont)
{
	xge_svg_font_cache_t* pCaches;
	xge_svg_font_cache_t* pCache;
	xge_svg_font_face_t* pFace;
	xge_font_face pFontFace;
	float fKey;
	int iCapacity;
	int iRet;
	int i;

	if ( !__xgeSvgValid(pSvg) || (ppFont == NULL) || (fSize <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppFont = NULL;
	fKey = __xgeSvgFontCacheKey(fSize);
	if ( sFamily == NULL ) {
		sFamily = "";
	}
	for ( i = 0; i < pSvg->iFontCacheCount; i++ ) {
		if ( (fabsf(pSvg->pFontCaches[i].fSize - fKey) <= 0.001f) &&
		     (strcmp(pSvg->pFontCaches[i].sFamily, sFamily) == 0) ) {
			*ppFont = &pSvg->pFontCaches[i].tFont;
			return XGE_OK;
		}
	}
	if ( pSvg->iFontCacheCount >= pSvg->iFontCacheCapacity ) {
		iCapacity = pSvg->iFontCacheCapacity > 0 ? pSvg->iFontCacheCapacity * 2 : 4;
		pCaches = (xge_svg_font_cache_t*)xrtRealloc(pSvg->pFontCaches, (size_t)iCapacity * sizeof(*pCaches));
		if ( pCaches == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		pSvg->pFontCaches = pCaches;
		pSvg->iFontCacheCapacity = iCapacity;
	}
	pCache = &pSvg->pFontCaches[pSvg->iFontCacheCount];
	memset(pCache, 0, sizeof(*pCache));
	pFace = __xgeSvgFontFaceFind(pSvg, sFamily);
	pFontFace = __xgeSvgFontFaceEnsure(pFace);
	if ( pFontFace != NULL ) {
		iRet = __xgeSvgFontCreateFaceEm(&pCache->tFont, pFontFace, fKey);
	} else {
		iRet = XGE_ERROR_RESOURCE_FAILED;
	}
	if ( iRet != XGE_OK ) {
		memset(&pCache->tFont, 0, sizeof(pCache->tFont));
		iRet = xgeFontFallbackGetEx(&pCache->tFont, fKey, XGE_FONT_SIZE_EM);
	}
	if ( iRet != XGE_OK ) {
		memset(pCache, 0, sizeof(*pCache));
		return iRet;
	}
	pCache->fSize = fKey;
	{
		size_t iFamilyLength = strlen(sFamily);
		if ( iFamilyLength >= sizeof(pCache->sFamily) ) iFamilyLength = sizeof(pCache->sFamily) - 1;
		memcpy(pCache->sFamily, sFamily, iFamilyLength);
		pCache->sFamily[iFamilyLength] = '\0';
	}
	pSvg->iFontCacheCount++;
	*ppFont = &pCache->tFont;
	return XGE_OK;
}

static void __xgeSvgElementNodesClear(xge_svg pSvg)
{
	if ( !__xgeSvgValid(pSvg) ) {
		return;
	}
	xrtFree(pSvg->pElementNodes);
	pSvg->pElementNodes = NULL;
	pSvg->iElementNodeCount = 0;
	pSvg->iElementNodeCapacity = 0;
	pSvg->iCurrentElementNode = -1;
	memset(pSvg->arrLastChildByDepth, 0xff, sizeof(pSvg->arrLastChildByDepth));
}

static int __xgeSvgElementNodeEnsure(xge_svg pSvg, const char* pTag, const char* pTagEnd, int* pNodeIndex)
{
	xge_svg_element_node_t* pNodes;
	xge_svg_element_node_t* pNode;
	int iCapacity;
	int iDepth;
	int iParentIndex;
	int iPrevSiblingIndex;
	int iIndex;
	int i;

	if ( !__xgeSvgValid(pSvg) || (pTag == NULL) || (pTagEnd == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (pSvg->iCurrentElementNode >= 0) && (pSvg->iCurrentElementNode < pSvg->iElementNodeCount) &&
	     (pSvg->pElementNodes[pSvg->iCurrentElementNode].pTag == pTag) &&
	     (pSvg->pElementNodes[pSvg->iCurrentElementNode].pTagEnd == pTagEnd) ) {
		if ( pNodeIndex != NULL ) {
			*pNodeIndex = pSvg->iCurrentElementNode;
		}
		return XGE_OK;
	}
	for ( i = pSvg->iCurrentElementNode + 1; i < pSvg->iElementNodeCount; i++ ) {
		if ( (pSvg->pElementNodes[i].pTag == pTag) &&
		     (pSvg->pElementNodes[i].pTagEnd == pTagEnd) ) {
			pSvg->iCurrentElementNode = i;
			if ( pNodeIndex != NULL ) {
				*pNodeIndex = i;
			}
			return XGE_OK;
		}
	}
	for ( i = 0; (i < pSvg->iElementNodeCount) && (i <= pSvg->iCurrentElementNode); i++ ) {
		if ( (pSvg->pElementNodes[i].pTag == pTag) &&
		     (pSvg->pElementNodes[i].pTagEnd == pTagEnd) ) {
			pSvg->iCurrentElementNode = i;
			if ( pNodeIndex != NULL ) {
				*pNodeIndex = i;
			}
			return XGE_OK;
		}
	}
	iDepth = pSvg->iElementStackTop;
	if ( iDepth < 0 ) {
		iDepth = 0;
	}
	if ( iDepth >= XGE_SVG_STACK_MAX ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iParentIndex = (pSvg->iElementStackTop > 0) ? pSvg->arrElementStack[pSvg->iElementStackTop - 1].iNodeIndex : -1;
	iPrevSiblingIndex = pSvg->arrLastChildByDepth[iDepth];
	if ( pSvg->iElementNodeCount >= pSvg->iElementNodeCapacity ) {
		iCapacity = pSvg->iElementNodeCapacity > 0 ? pSvg->iElementNodeCapacity * 2 : 128;
		pNodes = (xge_svg_element_node_t*)xrtRealloc(pSvg->pElementNodes, (size_t)iCapacity * sizeof(*pNodes));
		if ( pNodes == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		pSvg->pElementNodes = pNodes;
		pSvg->iElementNodeCapacity = iCapacity;
	}
	iIndex = pSvg->iElementNodeCount++;
	pNode = &pSvg->pElementNodes[iIndex];
	pNode->pTag = pTag;
	pNode->pTagEnd = pTagEnd;
	pNode->iParentIndex = iParentIndex;
	pNode->iPrevSiblingIndex = iPrevSiblingIndex;
	pNode->iNextSiblingIndex = -1;
	pNode->iChildCount = 0;
	pNode->bHasTextContent = 0;
	if ( (iParentIndex >= 0) && (iParentIndex < pSvg->iElementNodeCount) ) {
		pSvg->pElementNodes[iParentIndex].iChildCount++;
	}
	if ( (iPrevSiblingIndex >= 0) && (iPrevSiblingIndex < pSvg->iElementNodeCount) ) {
		pSvg->pElementNodes[iPrevSiblingIndex].iNextSiblingIndex = iIndex;
	}
	pSvg->arrLastChildByDepth[iDepth] = iIndex;
	pSvg->iCurrentElementNode = iIndex;
	if ( pNodeIndex != NULL ) {
		*pNodeIndex = iIndex;
	}
	return XGE_OK;
}

static int __xgeSvgElementStackPush(xge_svg pSvg, const char* pTag, const char* pTagEnd)
{
	int iNodeIndex;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pTag == NULL) || (pTagEnd == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pSvg->iElementStackTop >= XGE_SVG_STACK_MAX ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iNodeIndex = -1;
	iRet = __xgeSvgElementNodeEnsure(pSvg, pTag, pTagEnd, &iNodeIndex);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pSvg->arrElementStack[pSvg->iElementStackTop].pTag = pTag;
	pSvg->arrElementStack[pSvg->iElementStackTop].pTagEnd = pTagEnd;
	pSvg->arrElementStack[pSvg->iElementStackTop].iNodeIndex = iNodeIndex;
	pSvg->iElementStackTop++;
	if ( pSvg->iElementStackTop < XGE_SVG_STACK_MAX ) {
		pSvg->arrLastChildByDepth[pSvg->iElementStackTop] = -1;
	}
	return XGE_OK;
}

static void __xgeSvgElementStackPop(xge_svg pSvg, const char* sName)
{
	if ( !__xgeSvgValid(pSvg) || (pSvg->iElementStackTop <= 0) ) {
		return;
	}
	if ( (sName != NULL) && !__xgeSvgTagNameEquals(pSvg->arrElementStack[pSvg->iElementStackTop - 1].pTag, sName) ) {
		return;
	}
	pSvg->iElementStackTop--;
	if ( (pSvg->iElementStackTop + 1) < XGE_SVG_STACK_MAX ) {
		pSvg->arrLastChildByDepth[pSvg->iElementStackTop + 1] = -1;
	}
	pSvg->iCurrentElementNode = -1;
}

static void __xgeSvgDefItemReset(xge_svg_def_item_t* pItem)
{
	int i;

	if ( pItem == NULL ) {
		return;
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_GROUP) && (pItem->u.pGroup != NULL) ) {
		for ( i = 0; i < pItem->u.pGroup->iItemCount; i++ ) {
			__xgeSvgDefItemReset(&pItem->u.pGroup->pItems[i]);
		}
		xrtFree(pItem->u.pGroup->pItems);
		xrtFree(pItem->u.pGroup);
	}
	memset(pItem, 0, sizeof(*pItem));
}

static void __xgeSvgDefItemsClear(xge_svg_def_t* pDef)
{
	int i;

	if ( pDef == NULL ) {
		return;
	}
	for ( i = 0; i < pDef->iItemCount; i++ ) {
		__xgeSvgDefItemReset(&pDef->pItems[i]);
	}
	pDef->iItemCount = 0;
}

static int __xgeSvgDefItemReserve(xge_svg_def_t* pDef)
{
	xge_svg_def_item_t* pItems;
	int iCapacity;

	if ( pDef == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pDef->iItemCount < pDef->iItemCapacity ) {
		return XGE_OK;
	}
	iCapacity = pDef->iItemCapacity > 0 ? pDef->iItemCapacity * 2 : 4;
	pItems = (xge_svg_def_item_t*)xrtRealloc(pDef->pItems, (size_t)iCapacity * sizeof(*pItems));
	if ( pItems == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	memset(pItems + pDef->iItemCapacity, 0, (size_t)(iCapacity - pDef->iItemCapacity) * sizeof(*pItems));
	pDef->pItems = pItems;
	pDef->iItemCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeSvgDefItemAddLeaf(xge_svg_def_t* pDef, int iType, int iIndex)
{
	xge_svg_def_item_t* pItem;
	int iRet;

	if ( (pDef == NULL) || (iIndex < 0) ||
	     ((iType != XGE_SVG_DRAW_ITEM_SCENE) &&
	      (iType != XGE_SVG_DRAW_ITEM_TEXT) &&
	      (iType != XGE_SVG_DRAW_ITEM_SVG_IMAGE) &&
	      (iType != XGE_SVG_DRAW_ITEM_RASTER_IMAGE)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeSvgDefItemReserve(pDef);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pItem = &pDef->pItems[pDef->iItemCount++];
	memset(pItem, 0, sizeof(*pItem));
	pItem->iType = iType;
	pItem->u.iIndex = iIndex;
	return XGE_OK;
}

static int __xgeSvgDefItemsWrapGroup(
	xge_svg_def_t* pDef,
	int iFirstItem,
	int bBlendSet,
	int iBlend,
	float fOpacity,
	const char* sMaskId,
	const char* sFilterId,
	xge_shape_ex_matrix_t tFilterTransform,
	int bForce
)
{
	xge_svg_def_group_item_t* pGroup;
	xge_svg_def_item_t* pItem;
	int iItemCount;
	int bHasFilter;
	int iRet;

	if ( (pDef == NULL) || (iFirstItem < 0) || (iFirstItem > pDef->iItemCount) ||
	     !__xgeSvgFloatFinite(fOpacity) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iItemCount = pDef->iItemCount - iFirstItem;
	if ( iItemCount <= 0 ) {
		return XGE_OK;
	}
	bHasFilter = (sFilterId != NULL) && (sFilterId[0] != '\0');
	if ( !bBlendSet && !bHasFilter && ((sMaskId == NULL) || (sMaskId[0] == '\0')) &&
	     (fOpacity >= (1.0f - XGE_SVG_EPSILON)) && !bForce ) {
		return XGE_OK;
	}
	if ( fOpacity < 0.0f ) fOpacity = 0.0f;
	if ( fOpacity > 1.0f ) fOpacity = 1.0f;
	pGroup = (xge_svg_def_group_item_t*)xrtCalloc(1, sizeof(*pGroup));
	if ( pGroup == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pGroup->pItems = (xge_svg_def_item_t*)xrtMalloc((size_t)iItemCount * sizeof(*pGroup->pItems));
	if ( pGroup->pItems == NULL ) {
		xrtFree(pGroup);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	memcpy(pGroup->pItems, pDef->pItems + iFirstItem, (size_t)iItemCount * sizeof(*pGroup->pItems));
	memset(pDef->pItems + iFirstItem, 0, (size_t)iItemCount * sizeof(*pDef->pItems));
	pGroup->iItemCount = iItemCount;
	pGroup->iBlend = bBlendSet ? iBlend : XGE_BLEND_ALPHA;
	pGroup->bBlendSet = bBlendSet;
	pGroup->fOpacity = fOpacity;
	pGroup->tFilterTransform = tFilterTransform;
	if ( (sMaskId != NULL) && (sMaskId[0] != '\0') ) {
		strncpy(pGroup->sMaskId, sMaskId, sizeof(pGroup->sMaskId) - 1);
		pGroup->sMaskId[sizeof(pGroup->sMaskId) - 1] = '\0';
	}
	if ( bHasFilter ) {
		strncpy(pGroup->sFilterId, sFilterId, sizeof(pGroup->sFilterId) - 1);
		pGroup->sFilterId[sizeof(pGroup->sFilterId) - 1] = '\0';
	}
	pDef->iItemCount = iFirstItem;
	iRet = __xgeSvgDefItemReserve(pDef);
	if ( iRet != XGE_OK ) {
		int i;
		for ( i = 0; i < pGroup->iItemCount; i++ ) {
			__xgeSvgDefItemReset(&pGroup->pItems[i]);
		}
		xrtFree(pGroup->pItems);
		xrtFree(pGroup);
		return iRet;
	}
	pItem = &pDef->pItems[pDef->iItemCount++];
	memset(pItem, 0, sizeof(*pItem));
	pItem->iType = XGE_SVG_DRAW_ITEM_GROUP;
	pItem->u.pGroup = pGroup;
	return XGE_OK;
}

static int __xgeSvgDefItemsCloneArray(
	const xge_svg_def_item_t* pSource,
	int iCount,
	int iShapeOffset,
	int iTextOffset,
	int iImageOffset,
	int iRasterOffset,
	xge_svg_def_item_t** ppItems
)
{
	xge_svg_def_item_t* pItems;
	int i;
	int iRet;

	if ( ppItems == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppItems = NULL;
	if ( iCount <= 0 ) {
		return XGE_OK;
	}
	if ( pSource == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pItems = (xge_svg_def_item_t*)xrtCalloc((size_t)iCount, sizeof(*pItems));
	if ( pItems == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	for ( i = 0; i < iCount; i++ ) {
		pItems[i].iType = pSource[i].iType;
		if ( pSource[i].iType == XGE_SVG_DRAW_ITEM_GROUP ) {
			xge_svg_def_group_item_t* pSourceGroup = pSource[i].u.pGroup;
			xge_svg_def_group_item_t* pGroup;

			if ( pSourceGroup == NULL ) {
				iRet = XGE_ERROR_INVALID_ARGUMENT;
				goto fail;
			}
			pGroup = (xge_svg_def_group_item_t*)xrtCalloc(1, sizeof(*pGroup));
			if ( pGroup == NULL ) {
				iRet = XGE_ERROR_OUT_OF_MEMORY;
				goto fail;
			}
			*pGroup = *pSourceGroup;
			pGroup->pItems = NULL;
			pItems[i].u.pGroup = pGroup;
			iRet = __xgeSvgDefItemsCloneArray(
				pSourceGroup->pItems, pSourceGroup->iItemCount,
				iShapeOffset, iTextOffset, iImageOffset, iRasterOffset,
				&pGroup->pItems
			);
			if ( iRet != XGE_OK ) {
				goto fail;
			}
		} else {
			pItems[i].u.iIndex = pSource[i].u.iIndex;
			if ( pSource[i].iType == XGE_SVG_DRAW_ITEM_SCENE ) pItems[i].u.iIndex += iShapeOffset;
			else if ( pSource[i].iType == XGE_SVG_DRAW_ITEM_TEXT ) pItems[i].u.iIndex += iTextOffset;
			else if ( pSource[i].iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE ) pItems[i].u.iIndex += iImageOffset;
			else if ( pSource[i].iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE ) pItems[i].u.iIndex += iRasterOffset;
			else {
				iRet = XGE_ERROR_INVALID_ARGUMENT;
				goto fail;
			}
		}
	}
	*ppItems = pItems;
	return XGE_OK;

fail:
	for ( i = 0; i < iCount; i++ ) {
		__xgeSvgDefItemReset(&pItems[i]);
	}
	xrtFree(pItems);
	return iRet;
}

static void __xgeSvgDefItemsPrependFilterTransform(xge_svg_def_item_t* pItems, int iItemCount, xge_shape_ex_matrix_t tTransform)
{
	int i;

	if ( pItems == NULL ) {
		return;
	}
	for ( i = 0; i < iItemCount; i++ ) {
		if ( (pItems[i].iType == XGE_SVG_DRAW_ITEM_GROUP) && (pItems[i].u.pGroup != NULL) ) {
			xge_svg_def_group_item_t* pGroup = pItems[i].u.pGroup;

			if ( pGroup->sFilterId[0] != '\0' ) {
				pGroup->tFilterTransform = __xgeSvgMatrixMul(tTransform, pGroup->tFilterTransform);
			}
			__xgeSvgDefItemsPrependFilterTransform(pGroup->pItems, pGroup->iItemCount, tTransform);
		}
	}
}

static void __xgeSvgDefReset(xge_svg_def_t* pDef)
{
	int i;

	if ( pDef == NULL ) {
		return;
	}
	__xgeSvgDefItemsClear(pDef);
	for ( i = 0; i < pDef->iShapeCount; i++ ) {
		xgeShapeExDestroy(pDef->pShapes[i]);
	}
	for ( i = 0; i < pDef->iTextCount; i++ ) {
		xrtFree(pDef->pTexts[i].sText);
		xgeShapeExSceneDestroy(pDef->pTexts[i].pMaskScene);
	}
	for ( i = 0; i < pDef->iImageCount; i++ ) {
		xgeSvgDestroy(pDef->pImages[i].pSvg);
		xgeShapeExSceneDestroy(pDef->pImages[i].pMaskScene);
	}
	for ( i = 0; i < pDef->iRasterCount; i++ ) {
		__xgeSvgRasterImageDestroy(pDef->pRasters[i].pRaster);
		xgeShapeExSceneDestroy(pDef->pRasters[i].pMaskScene);
	}
	xrtFree(pDef->pShapes);
	xrtFree(pDef->pShapeStyles);
	xrtFree(pDef->pTexts);
	xrtFree(pDef->pImages);
	xrtFree(pDef->pRasters);
	xrtFree(pDef->pItems);
	xrtFree(pDef->sId);
	memset(pDef, 0, sizeof(*pDef));
}

static void __xgeSvgDefShapesClear(xge_svg_def_t* pDef)
{
	int i;

	if ( pDef == NULL ) {
		return;
	}
	__xgeSvgDefItemsClear(pDef);
	for ( i = 0; i < pDef->iShapeCount; i++ ) {
		xgeShapeExDestroy(pDef->pShapes[i]);
	}
	for ( i = 0; i < pDef->iTextCount; i++ ) {
		xrtFree(pDef->pTexts[i].sText);
		xgeShapeExSceneDestroy(pDef->pTexts[i].pMaskScene);
	}
	for ( i = 0; i < pDef->iImageCount; i++ ) {
		xgeSvgDestroy(pDef->pImages[i].pSvg);
		xgeShapeExSceneDestroy(pDef->pImages[i].pMaskScene);
	}
	for ( i = 0; i < pDef->iRasterCount; i++ ) {
		__xgeSvgRasterImageDestroy(pDef->pRasters[i].pRaster);
		xgeShapeExSceneDestroy(pDef->pRasters[i].pMaskScene);
	}
	pDef->iShapeCount = 0;
	pDef->iTextCount = 0;
	pDef->iImageCount = 0;
	pDef->iRasterCount = 0;
	pDef->sGroupFilterId[0] = '\0';
	pDef->tGroupFilterTransform = __xgeSvgMatrixIdentity();
}

static void __xgeSvgDefsClear(xge_svg pSvg)
{
	int i;

	if ( !__xgeSvgValid(pSvg) ) {
		return;
	}
	for ( i = 0; i < pSvg->iDefCount; i++ ) {
		__xgeSvgDefReset(&pSvg->pDefs[i]);
	}
	xrtFree(pSvg->pDefs);
	pSvg->pDefs = NULL;
	pSvg->iDefCount = 0;
	pSvg->iDefCapacity = 0;
}

static int __xgeSvgDefFind(xge_svg pSvg, const char* sId)
{
	int i;

	if ( !__xgeSvgValid(pSvg) || (sId == NULL) || (sId[0] == '\0') ) {
		return -1;
	}
	for ( i = 0; i < pSvg->iDefCount; i++ ) {
		if ( (pSvg->pDefs[i].sId != NULL) && (strcmp(pSvg->pDefs[i].sId, sId) == 0) ) {
			return i;
		}
	}
	return -1;
}

static int __xgeSvgDefGetOrCreate(xge_svg pSvg, const char* sId, int* pIndex)
{
	xge_svg_def_t* pDefs;
	int iIndex;
	int iCapacity;

	if ( !__xgeSvgValid(pSvg) || (sId == NULL) || (sId[0] == '\0') || (pIndex == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xgeSvgDefFind(pSvg, sId);
	if ( iIndex >= 0 ) {
		*pIndex = iIndex;
		return XGE_OK;
	}
	if ( pSvg->iDefCount >= pSvg->iDefCapacity ) {
		iCapacity = pSvg->iDefCapacity > 0 ? pSvg->iDefCapacity * 2 : 16;
		pDefs = (xge_svg_def_t*)xrtRealloc(pSvg->pDefs, (size_t)iCapacity * sizeof(*pDefs));
		if ( pDefs == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		memset(pDefs + pSvg->iDefCapacity, 0, (size_t)(iCapacity - pSvg->iDefCapacity) * sizeof(*pDefs));
		pSvg->pDefs = pDefs;
		pSvg->iDefCapacity = iCapacity;
	}
	iIndex = pSvg->iDefCount++;
	pSvg->pDefs[iIndex].sId = __xgeStrDup(sId);
	if ( pSvg->pDefs[iIndex].sId == NULL ) {
		pSvg->iDefCount--;
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pSvg->pDefs[iIndex].iAspectAlignX = XGE_SVG_ASPECT_ALIGN_MID;
	pSvg->pDefs[iIndex].iAspectAlignY = XGE_SVG_ASPECT_ALIGN_MID;
	pSvg->pDefs[iIndex].iAspectMeetOrSlice = XGE_SVG_ASPECT_MEET;
	pSvg->pDefs[iIndex].tGroupFilterTransform = __xgeSvgMatrixIdentity();
	*pIndex = iIndex;
	return XGE_OK;
}

static int __xgeSvgDefAddShape(xge_svg_def_t* pDef, xge_shape_ex pShape, const xge_svg_style_t* pStyle)
{
	xge_shape_ex* pShapes;
	xge_svg_style_t* pShapeStyles;
	int iCapacity;
	int iIndex;
	int iRet;

	if ( (pDef == NULL) || (pShape == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pDef->iShapeCount >= pDef->iShapeCapacity ) {
		iCapacity = pDef->iShapeCapacity > 0 ? pDef->iShapeCapacity * 2 : 4;
		pShapes = (xge_shape_ex*)xrtRealloc(pDef->pShapes, (size_t)iCapacity * sizeof(*pShapes));
		if ( pShapes == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		pDef->pShapes = pShapes;
		pShapeStyles = (xge_svg_style_t*)xrtRealloc(pDef->pShapeStyles, (size_t)iCapacity * sizeof(*pShapeStyles));
		if ( pShapeStyles == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		pDef->pShapeStyles = pShapeStyles;
		pDef->iShapeCapacity = iCapacity;
	}
	xgeShapeExAddRef(pShape);
	iIndex = pDef->iShapeCount++;
	pDef->pShapes[iIndex] = pShape;
	if ( pStyle != NULL ) {
		pDef->pShapeStyles[iIndex] = *pStyle;
	} else {
		memset(&pDef->pShapeStyles[iIndex], 0, sizeof(pDef->pShapeStyles[iIndex]));
	}
	iRet = __xgeSvgDefItemAddLeaf(pDef, XGE_SVG_DRAW_ITEM_SCENE, iIndex);
	if ( iRet != XGE_OK ) {
		pDef->iShapeCount--;
		xgeShapeExDestroy(pDef->pShapes[iIndex]);
		memset(&pDef->pShapes[iIndex], 0, sizeof(pDef->pShapes[iIndex]));
		memset(&pDef->pShapeStyles[iIndex], 0, sizeof(pDef->pShapeStyles[iIndex]));
		return iRet;
	}
	return XGE_OK;
}

static int __xgeSvgDefAddText(xge_svg_def_t* pDef, const xge_svg_text_item_t* pText)
{
	xge_svg_text_item_t* pTexts;
	int iCapacity;
	int iIndex;
	int iRet;

	if ( (pDef == NULL) || (pText == NULL) || (pText->sText == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pDef->iTextCount >= pDef->iTextCapacity ) {
		iCapacity = pDef->iTextCapacity > 0 ? pDef->iTextCapacity * 2 : 2;
		pTexts = (xge_svg_text_item_t*)xrtRealloc(pDef->pTexts, (size_t)iCapacity * sizeof(*pTexts));
		if ( pTexts == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		pDef->pTexts = pTexts;
		pDef->iTextCapacity = iCapacity;
	}
	iIndex = pDef->iTextCount;
	pDef->pTexts[iIndex] = *pText;
	pDef->pTexts[iIndex].sText = __xgeStrDup(pText->sText);
	if ( pDef->pTexts[iIndex].sText == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( pDef->pTexts[iIndex].pMaskScene != NULL ) {
		if ( xgeShapeExSceneAddRef(pDef->pTexts[iIndex].pMaskScene) != XGE_OK ) {
			xrtFree(pDef->pTexts[iIndex].sText);
			memset(&pDef->pTexts[iIndex], 0, sizeof(pDef->pTexts[iIndex]));
			return XGE_ERROR_INVALID_ARGUMENT;
		}
	}
	pDef->iTextCount++;
	iRet = __xgeSvgDefItemAddLeaf(pDef, XGE_SVG_DRAW_ITEM_TEXT, iIndex);
	if ( iRet != XGE_OK ) {
		pDef->iTextCount--;
		xgeShapeExSceneDestroy(pDef->pTexts[iIndex].pMaskScene);
		xrtFree(pDef->pTexts[iIndex].sText);
		memset(&pDef->pTexts[iIndex], 0, sizeof(pDef->pTexts[iIndex]));
		return iRet;
	}
	return XGE_OK;
}

static int __xgeSvgDefAddImage(xge_svg_def_t* pDef, const xge_svg_image_item_t* pImage)
{
	xge_svg_image_item_t* pImages;
	int iCapacity;
	int iRet;

	if ( (pDef == NULL) || (pImage == NULL) || !__xgeSvgValid(pImage->pSvg) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pDef->iImageCount >= pDef->iImageCapacity ) {
		iCapacity = pDef->iImageCapacity > 0 ? pDef->iImageCapacity * 2 : 2;
		pImages = (xge_svg_image_item_t*)xrtRealloc(pDef->pImages, (size_t)iCapacity * sizeof(*pImages));
		if ( pImages == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		pDef->pImages = pImages;
		pDef->iImageCapacity = iCapacity;
	}
	pDef->pImages[pDef->iImageCount] = *pImage;
	xgeSvgAddRef(pImage->pSvg);
	if ( pImage->pMaskScene != NULL ) {
		iRet = xgeShapeExSceneAddRef(pImage->pMaskScene);
		if ( iRet != XGE_OK ) {
			xgeSvgDestroy(pDef->pImages[pDef->iImageCount].pSvg);
			memset(&pDef->pImages[pDef->iImageCount], 0, sizeof(pDef->pImages[pDef->iImageCount]));
			return iRet;
		}
	}
	pDef->iImageCount++;
	iRet = __xgeSvgDefItemAddLeaf(pDef, XGE_SVG_DRAW_ITEM_SVG_IMAGE, pDef->iImageCount - 1);
	if ( iRet != XGE_OK ) {
		pDef->iImageCount--;
		xgeShapeExSceneDestroy(pDef->pImages[pDef->iImageCount].pMaskScene);
		xgeSvgDestroy(pDef->pImages[pDef->iImageCount].pSvg);
		memset(&pDef->pImages[pDef->iImageCount], 0, sizeof(pDef->pImages[pDef->iImageCount]));
		return iRet;
	}
	return XGE_OK;
}

static int __xgeSvgDefAddRasterImage(xge_svg_def_t* pDef, const xge_svg_raster_item_t* pImage)
{
	xge_svg_raster_item_t* pImages;
	int iCapacity;
	int iRet;

	if ( (pDef == NULL) || (pImage == NULL) || (pImage->pRaster == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pDef->iRasterCount >= pDef->iRasterCapacity ) {
		iCapacity = pDef->iRasterCapacity > 0 ? pDef->iRasterCapacity * 2 : 2;
		pImages = (xge_svg_raster_item_t*)xrtRealloc(pDef->pRasters, (size_t)iCapacity * sizeof(*pImages));
		if ( pImages == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		pDef->pRasters = pImages;
		pDef->iRasterCapacity = iCapacity;
	}
	pDef->pRasters[pDef->iRasterCount] = *pImage;
	iRet = __xgeSvgRasterImageAddRef(pImage->pRaster);
	if ( iRet <= 0 ) {
		memset(&pDef->pRasters[pDef->iRasterCount], 0, sizeof(pDef->pRasters[pDef->iRasterCount]));
		return iRet;
	}
	if ( pImage->pMaskScene != NULL ) {
		iRet = xgeShapeExSceneAddRef(pImage->pMaskScene);
		if ( iRet != XGE_OK ) {
			__xgeSvgRasterImageDestroy(pDef->pRasters[pDef->iRasterCount].pRaster);
			memset(&pDef->pRasters[pDef->iRasterCount], 0, sizeof(pDef->pRasters[pDef->iRasterCount]));
			return iRet;
		}
	}
	pDef->iRasterCount++;
	iRet = __xgeSvgDefItemAddLeaf(pDef, XGE_SVG_DRAW_ITEM_RASTER_IMAGE, pDef->iRasterCount - 1);
	if ( iRet != XGE_OK ) {
		pDef->iRasterCount--;
		xgeShapeExSceneDestroy(pDef->pRasters[pDef->iRasterCount].pMaskScene);
		__xgeSvgRasterImageDestroy(pDef->pRasters[pDef->iRasterCount].pRaster);
		memset(&pDef->pRasters[pDef->iRasterCount], 0, sizeof(pDef->pRasters[pDef->iRasterCount]));
		return iRet;
	}
	return XGE_OK;
}

static void __xgeSvgPatternReset(xge_svg_pattern_t* pPattern)
{
	if ( pPattern == NULL ) {
		return;
	}
	xrtFree(pPattern->sId);
	xrtFree(pPattern->sHrefId);
	memset(pPattern, 0, sizeof(*pPattern));
	pPattern->iDefIndex = -1;
	pPattern->iUnits = XGE_SVG_PATTERN_OBJECT_BOUNDING_BOX;
	pPattern->iContentUnits = XGE_SVG_PATTERN_USER_SPACE;
	pPattern->tTransform = __xgeSvgMatrixIdentity();
}

static void __xgeSvgPatternsClear(xge_svg pSvg)
{
	int i;

	if ( !__xgeSvgValid(pSvg) ) {
		return;
	}
	for ( i = 0; i < pSvg->iPatternCount; i++ ) {
		__xgeSvgPatternReset(&pSvg->pPatterns[i]);
	}
	xrtFree(pSvg->pPatterns);
	pSvg->pPatterns = NULL;
	pSvg->iPatternCount = 0;
	pSvg->iPatternCapacity = 0;
}

static int __xgeSvgPatternFind(xge_svg pSvg, const char* sId)
{
	int i;

	if ( !__xgeSvgValid(pSvg) || (sId == NULL) || (sId[0] == '\0') ) {
		return -1;
	}
	for ( i = 0; i < pSvg->iPatternCount; i++ ) {
		if ( (pSvg->pPatterns[i].sId != NULL) && (strcmp(pSvg->pPatterns[i].sId, sId) == 0) ) {
			return i;
		}
	}
	return -1;
}

static int __xgeSvgPatternGetOrCreate(xge_svg pSvg, const char* sId, int* pIndex)
{
	xge_svg_pattern_t* pPatterns;
	int iIndex;
	int iCapacity;

	if ( !__xgeSvgValid(pSvg) || (sId == NULL) || (sId[0] == '\0') || (pIndex == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xgeSvgPatternFind(pSvg, sId);
	if ( iIndex >= 0 ) {
		*pIndex = iIndex;
		return XGE_OK;
	}
	if ( pSvg->iPatternCount >= pSvg->iPatternCapacity ) {
		iCapacity = pSvg->iPatternCapacity > 0 ? pSvg->iPatternCapacity * 2 : 8;
		pPatterns = (xge_svg_pattern_t*)xrtRealloc(pSvg->pPatterns, (size_t)iCapacity * sizeof(*pPatterns));
		if ( pPatterns == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		memset(pPatterns + pSvg->iPatternCapacity, 0, (size_t)(iCapacity - pSvg->iPatternCapacity) * sizeof(*pPatterns));
		pSvg->pPatterns = pPatterns;
		pSvg->iPatternCapacity = iCapacity;
	}
	iIndex = pSvg->iPatternCount++;
	memset(&pSvg->pPatterns[iIndex], 0, sizeof(pSvg->pPatterns[iIndex]));
	pSvg->pPatterns[iIndex].sId = __xgeStrDup(sId);
	if ( pSvg->pPatterns[iIndex].sId == NULL ) {
		pSvg->iPatternCount--;
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pSvg->pPatterns[iIndex].iDefIndex = -1;
	pSvg->pPatterns[iIndex].tRect.fX = 0.0f;
	pSvg->pPatterns[iIndex].tRect.fY = 0.0f;
	pSvg->pPatterns[iIndex].tRect.fW = 0.0f;
	pSvg->pPatterns[iIndex].tRect.fH = 0.0f;
	pSvg->pPatterns[iIndex].iUnits = XGE_SVG_PATTERN_OBJECT_BOUNDING_BOX;
	pSvg->pPatterns[iIndex].iContentUnits = XGE_SVG_PATTERN_USER_SPACE;
	pSvg->pPatterns[iIndex].tTransform = __xgeSvgMatrixIdentity();
	pSvg->pPatterns[iIndex].iFlags = 0;
	*pIndex = iIndex;
	return XGE_OK;
}

static int __xgeSvgDefHasContent(const xge_svg_def_t* pDef)
{
	return (pDef != NULL) && ((pDef->iItemCount > 0) || (pDef->iShapeCount > 0) ||
		(pDef->iTextCount > 0) || (pDef->iImageCount > 0) || (pDef->iRasterCount > 0));
}

static int __xgeSvgDefCopyContent(xge_svg_def_t* pDst, const xge_svg_def_t* pSrc)
{
	xge_svg_def_item_t* pClonedItems;
	int iShapeOffset;
	int iTextOffset;
	int iImageOffset;
	int iRasterOffset;
	int iFlatItemStart;
	int i;
	int iRet;

	if ( (pDst == NULL) || (pSrc == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iShapeOffset = pDst->iShapeCount;
	iTextOffset = pDst->iTextCount;
	iImageOffset = pDst->iImageCount;
	iRasterOffset = pDst->iRasterCount;
	iFlatItemStart = pDst->iItemCount;
	for ( i = 0; i < pSrc->iShapeCount; i++ ) {
		xge_shape_ex pClone;

		iRet = xgeShapeExClone(pSrc->pShapes[i], &pClone);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		iRet = __xgeSvgDefAddShape(pDst, pClone, (pSrc->pShapeStyles != NULL) ? &pSrc->pShapeStyles[i] : NULL);
		xgeShapeExDestroy(pClone);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	for ( i = 0; i < pSrc->iTextCount; i++ ) {
		iRet = __xgeSvgDefAddText(pDst, &pSrc->pTexts[i]);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	for ( i = 0; i < pSrc->iImageCount; i++ ) {
		iRet = __xgeSvgDefAddImage(pDst, &pSrc->pImages[i]);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	for ( i = 0; i < pSrc->iRasterCount; i++ ) {
		iRet = __xgeSvgDefAddRasterImage(pDst, &pSrc->pRasters[i]);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	if ( pSrc->iItemCount > 0 ) {
		pClonedItems = NULL;
		iRet = __xgeSvgDefItemsCloneArray(
			pSrc->pItems, pSrc->iItemCount,
			iShapeOffset, iTextOffset, iImageOffset, iRasterOffset,
			&pClonedItems
		);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		while ( pDst->iItemCount > iFlatItemStart ) {
			pDst->iItemCount--;
			__xgeSvgDefItemReset(&pDst->pItems[pDst->iItemCount]);
		}
		for ( i = 0; i < pSrc->iItemCount; i++ ) {
			iRet = __xgeSvgDefItemReserve(pDst);
			if ( iRet != XGE_OK ) {
				int j;

				for ( j = i; j < pSrc->iItemCount; j++ ) {
					__xgeSvgDefItemReset(&pClonedItems[j]);
				}
				xrtFree(pClonedItems);
				return iRet;
			}
			pDst->pItems[pDst->iItemCount++] = pClonedItems[i];
			memset(&pClonedItems[i], 0, sizeof(pClonedItems[i]));
		}
		xrtFree(pClonedItems);
	}
	return XGE_OK;
}

static int __xgeSvgDefPrependTransform(xge_svg_def_t* pDef, xge_shape_ex_matrix_t tTransform)
{
	int i;
	int iRet;

	if ( pDef == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < pDef->iShapeCount; i++ ) {
		xge_shape_ex pShape = pDef->pShapes[i];

		if ( pShape != NULL ) {
			xge_shape_ex_matrix_t tShapeTransform = __xgeSvgMatrixMul(tTransform, pShape->tTransform);
			iRet = xgeShapeExTransformSet(pShape, &tShapeTransform);
			if ( iRet != XGE_OK ) {
				return iRet;
			}
		}
	}
	for ( i = 0; i < pDef->iTextCount; i++ ) {
		pDef->pTexts[i].tStyle.tTransform = __xgeSvgMatrixMul(tTransform, pDef->pTexts[i].tStyle.tTransform);
	}
	for ( i = 0; i < pDef->iImageCount; i++ ) {
		pDef->pImages[i].tStyle.tTransform = __xgeSvgMatrixMul(tTransform, pDef->pImages[i].tStyle.tTransform);
	}
	for ( i = 0; i < pDef->iRasterCount; i++ ) {
		pDef->pRasters[i].tStyle.tTransform = __xgeSvgMatrixMul(tTransform, pDef->pRasters[i].tStyle.tTransform);
	}
	__xgeSvgDefItemsPrependFilterTransform(pDef->pItems, pDef->iItemCount, tTransform);
	if ( pDef->sGroupFilterId[0] != '\0' ) {
		pDef->tGroupFilterTransform = __xgeSvgMatrixMul(tTransform, pDef->tGroupFilterTransform);
	}
	return XGE_OK;
}

static int __xgeSvgDefRemoveParentTransform(xge_svg pSvg, int iDefIndex, xge_shape_ex_matrix_t tParentTransform)
{
	xge_shape_ex_matrix_t tInverse;

	if ( !__xgeSvgValid(pSvg) || (iDefIndex < 0) || (iDefIndex >= pSvg->iDefCount) ) {
		return XGE_OK;
	}
	if ( !__xgeSvgMatrixInvert(tParentTransform, &tInverse) ) {
		return XGE_OK;
	}
	return __xgeSvgDefPrependTransform(&pSvg->pDefs[iDefIndex], tInverse);
}

static int __xgeSvgResolvePattern(xge_svg pSvg, int iIndex, int iDepth)
{
	xge_svg_pattern_t* pPattern;
	xge_svg_pattern_t* pSource;
	xge_svg_def_t* pDef;
	xge_svg_def_t* pSourceDef;
	int iSource;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (iIndex < 0) || (iIndex >= pSvg->iPatternCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iDepth > pSvg->iPatternCount ) {
		return XGE_OK;
	}
	pPattern = &pSvg->pPatterns[iIndex];
	if ( (pPattern->iFlags & XGE_SVG_PATTERN_RESOLVED) != 0 ) {
		return XGE_OK;
	}
	if ( (pPattern->sHrefId == NULL) || (pPattern->sHrefId[0] == '\0') ) {
		pPattern->iFlags |= XGE_SVG_PATTERN_RESOLVED;
		return XGE_OK;
	}
	iSource = __xgeSvgPatternFind(pSvg, pPattern->sHrefId);
	if ( iSource < 0 ) {
		return XGE_OK;
	}
	if ( iSource == iIndex ) {
		pPattern->iFlags |= XGE_SVG_PATTERN_RESOLVED;
		return XGE_OK;
	}
	iRet = __xgeSvgResolvePattern(pSvg, iSource, iDepth + 1);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pSource = &pSvg->pPatterns[iSource];
	if ( (pPattern->iFlags & XGE_SVG_PATTERN_HAS_X) == 0 ) pPattern->tRect.fX = pSource->tRect.fX;
	if ( (pPattern->iFlags & XGE_SVG_PATTERN_HAS_Y) == 0 ) pPattern->tRect.fY = pSource->tRect.fY;
	if ( (pPattern->iFlags & XGE_SVG_PATTERN_HAS_WIDTH) == 0 ) pPattern->tRect.fW = pSource->tRect.fW;
	if ( (pPattern->iFlags & XGE_SVG_PATTERN_HAS_HEIGHT) == 0 ) pPattern->tRect.fH = pSource->tRect.fH;
	if ( (pPattern->iFlags & XGE_SVG_PATTERN_HAS_UNITS) == 0 ) pPattern->iUnits = pSource->iUnits;
	if ( (pPattern->iFlags & XGE_SVG_PATTERN_HAS_CONTENT_UNITS) == 0 ) pPattern->iContentUnits = pSource->iContentUnits;
	if ( (pPattern->iFlags & XGE_SVG_PATTERN_HAS_TRANSFORM) == 0 ) pPattern->tTransform = pSource->tTransform;
	if ( (pPattern->iDefIndex >= 0) && (pPattern->iDefIndex < pSvg->iDefCount) &&
	     (pSource->iDefIndex >= 0) && (pSource->iDefIndex < pSvg->iDefCount) ) {
		pDef = &pSvg->pDefs[pPattern->iDefIndex];
		pSourceDef = &pSvg->pDefs[pSource->iDefIndex];
		if ( ((pPattern->iFlags & XGE_SVG_PATTERN_HAS_VIEWBOX) == 0) && pSourceDef->bHasViewBox ) {
			pDef->tViewBox = pSourceDef->tViewBox;
			pDef->bHasViewBox = 1;
		}
		if ( (pPattern->iFlags & XGE_SVG_PATTERN_HAS_ASPECT) == 0 ) {
			pDef->iAspectAlignX = pSourceDef->iAspectAlignX;
			pDef->iAspectAlignY = pSourceDef->iAspectAlignY;
			pDef->iAspectMeetOrSlice = pSourceDef->iAspectMeetOrSlice;
		}
		if ( (pPattern->iFlags & XGE_SVG_PATTERN_HAS_OVERFLOW) == 0 ) {
			pDef->bOverflowVisible = pSourceDef->bOverflowVisible;
		}
		if ( !__xgeSvgDefHasContent(pDef) && __xgeSvgDefHasContent(pSourceDef) ) {
			iRet = __xgeSvgDefCopyContent(pDef, pSourceDef);
			if ( iRet != XGE_OK ) {
				return iRet;
			}
		}
	}
	pPattern->iFlags |= XGE_SVG_PATTERN_RESOLVED;
	return XGE_OK;
}

static void __xgeSvgClipPathReset(xge_svg_clip_path_t* pClip)
{
	int i;

	if ( pClip == NULL ) {
		return;
	}
	xrtFree(pClip->sId);
	xrtFree(pClip->pRects);
	for ( i = 0; i < pClip->iShapeCount; i++ ) {
		xgeShapeExDestroy(pClip->pShapes[i]);
	}
	xrtFree(pClip->pShapes);
	memset(pClip, 0, sizeof(*pClip));
}

static void __xgeSvgClipPathsClear(xge_svg pSvg)
{
	int i;

	if ( !__xgeSvgValid(pSvg) ) {
		return;
	}
	for ( i = 0; i < pSvg->iClipPathCount; i++ ) {
		__xgeSvgClipPathReset(&pSvg->pClipPaths[i]);
	}
	xrtFree(pSvg->pClipPaths);
	pSvg->pClipPaths = NULL;
	pSvg->iClipPathCount = 0;
	pSvg->iClipPathCapacity = 0;
}

static int __xgeSvgClipPathFind(xge_svg pSvg, const char* sId)
{
	int i;

	if ( !__xgeSvgValid(pSvg) || (sId == NULL) || (sId[0] == '\0') ) {
		return -1;
	}
	for ( i = 0; i < pSvg->iClipPathCount; i++ ) {
		if ( (pSvg->pClipPaths[i].sId != NULL) && (strcmp(pSvg->pClipPaths[i].sId, sId) == 0) ) {
			return i;
		}
	}
	return -1;
}

static int __xgeSvgClipPathGetOrCreate(xge_svg pSvg, const char* sId, int* pIndex)
{
	xge_svg_clip_path_t* pClipPaths;
	int iIndex;
	int iCapacity;

	if ( !__xgeSvgValid(pSvg) || (sId == NULL) || (sId[0] == '\0') || (pIndex == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xgeSvgClipPathFind(pSvg, sId);
	if ( iIndex >= 0 ) {
		*pIndex = iIndex;
		return XGE_OK;
	}
	if ( pSvg->iClipPathCount >= pSvg->iClipPathCapacity ) {
		iCapacity = pSvg->iClipPathCapacity > 0 ? pSvg->iClipPathCapacity * 2 : 8;
		pClipPaths = (xge_svg_clip_path_t*)xrtRealloc(pSvg->pClipPaths, (size_t)iCapacity * sizeof(*pClipPaths));
		if ( pClipPaths == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		memset(pClipPaths + pSvg->iClipPathCapacity, 0, (size_t)(iCapacity - pSvg->iClipPathCapacity) * sizeof(*pClipPaths));
		pSvg->pClipPaths = pClipPaths;
		pSvg->iClipPathCapacity = iCapacity;
	}
	iIndex = pSvg->iClipPathCount++;
	pSvg->pClipPaths[iIndex].sId = __xgeStrDup(sId);
	if ( pSvg->pClipPaths[iIndex].sId == NULL ) {
		pSvg->iClipPathCount--;
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	*pIndex = iIndex;
	return XGE_OK;
}

static int __xgeSvgRectAppend(xge_rect_t** ppRects, int* pCount, int* pCapacity, xge_rect_t tRect)
{
	xge_rect_t* pRects;
	int iCapacity;

	if ( (ppRects == NULL) || (pCount == NULL) || (pCapacity == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XGE_OK;
	}
	if ( *pCount >= *pCapacity ) {
		iCapacity = *pCapacity > 0 ? *pCapacity * 2 : 4;
		pRects = (xge_rect_t*)xrtRealloc(*ppRects, (size_t)iCapacity * sizeof(*pRects));
		if ( pRects == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		*ppRects = pRects;
		*pCapacity = iCapacity;
	}
	(*ppRects)[(*pCount)++] = tRect;
	return XGE_OK;
}

static int __xgeSvgRectSubtract(xge_rect_t tRect, xge_rect_t tCut, xge_rect_t* pOut, int iOutCapacity)
{
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;
	float fRectRight;
	float fRectBottom;
	int iCount;

	if ( (pOut == NULL) || (iOutCapacity < 4) ) {
		return 0;
	}
	fRectRight = tRect.fX + tRect.fW;
	fRectBottom = tRect.fY + tRect.fH;
	fLeft = fmaxf(tRect.fX, tCut.fX);
	fTop = fmaxf(tRect.fY, tCut.fY);
	fRight = fminf(fRectRight, tCut.fX + tCut.fW);
	fBottom = fminf(fRectBottom, tCut.fY + tCut.fH);
	if ( (fRight <= fLeft) || (fBottom <= fTop) ) {
		pOut[0] = tRect;
		return 1;
	}
	iCount = 0;
	if ( fTop > tRect.fY ) {
		pOut[iCount++] = (xge_rect_t){tRect.fX, tRect.fY, tRect.fW, fTop - tRect.fY};
	}
	if ( fBottom < fRectBottom ) {
		pOut[iCount++] = (xge_rect_t){tRect.fX, fBottom, tRect.fW, fRectBottom - fBottom};
	}
	if ( fLeft > tRect.fX ) {
		pOut[iCount++] = (xge_rect_t){tRect.fX, fTop, fLeft - tRect.fX, fBottom - fTop};
	}
	if ( fRight < fRectRight ) {
		pOut[iCount++] = (xge_rect_t){fRight, fTop, fRectRight - fRight, fBottom - fTop};
	}
	return iCount;
}

static int __xgeSvgClipPathAddRect(xge_svg_clip_path_t* pClip, xge_rect_t tRect)
{
	xge_rect_t* pPieces;
	int iPieceCount;
	int iPieceCapacity;
	int i;
	int iRet;

	if ( pClip == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XGE_OK;
	}
	pPieces = NULL;
	iPieceCount = 0;
	iPieceCapacity = 0;
	iRet = __xgeSvgRectAppend(&pPieces, &iPieceCount, &iPieceCapacity, tRect);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	for ( i = 0; i < pClip->iRectCount && iPieceCount > 0; i++ ) {
		xge_rect_t* pNextPieces;
		int iNextCount;
		int iNextCapacity;
		int j;

		pNextPieces = NULL;
		iNextCount = 0;
		iNextCapacity = 0;
		for ( j = 0; j < iPieceCount; j++ ) {
			xge_rect_t arrSubtracted[4];
			int iSubCount;
			int k;

			iSubCount = __xgeSvgRectSubtract(pPieces[j], pClip->pRects[i], arrSubtracted, 4);
			for ( k = 0; k < iSubCount; k++ ) {
				iRet = __xgeSvgRectAppend(&pNextPieces, &iNextCount, &iNextCapacity, arrSubtracted[k]);
				if ( iRet != XGE_OK ) {
					xrtFree(pPieces);
					xrtFree(pNextPieces);
					return iRet;
				}
			}
		}
		xrtFree(pPieces);
		pPieces = pNextPieces;
		iPieceCount = iNextCount;
		iPieceCapacity = iNextCapacity;
	}
	for ( i = 0; i < iPieceCount; i++ ) {
		iRet = __xgeSvgRectAppend(&pClip->pRects, &pClip->iRectCount, &pClip->iRectCapacity, pPieces[i]);
		if ( iRet != XGE_OK ) {
			xrtFree(pPieces);
			return iRet;
		}
		pClip->bHasRect = 1;
	}
	xrtFree(pPieces);
	return XGE_OK;
}

static int __xgeSvgClipPathAddShape(xge_svg_clip_path_t* pClip, xge_shape_ex pShape)
{
	xge_shape_ex* pShapes;
	int iCapacity;
	int iRet;

	if ( (pClip == NULL) || (pShape == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pClip->iShapeCount >= pClip->iShapeCapacity ) {
		iCapacity = pClip->iShapeCapacity > 0 ? pClip->iShapeCapacity * 2 : 4;
		pShapes = (xge_shape_ex*)xrtRealloc(pClip->pShapes, (size_t)iCapacity * sizeof(*pShapes));
		if ( pShapes == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		pClip->pShapes = pShapes;
		pClip->iShapeCapacity = iCapacity;
	}
	iRet = xgeShapeExAddRef(pShape);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pClip->pShapes[pClip->iShapeCount++] = pShape;
	pClip->bHasShape = 1;
	return XGE_OK;
}

static void __xgeSvgMaskReset(xge_svg_mask_t* pMask)
{
	int i;

	if ( pMask == NULL ) {
		return;
	}
	xrtFree(pMask->sId);
	for ( i = 0; i < pMask->iShapeCount; i++ ) {
		xgeShapeExDestroy(pMask->pShapes[i].pShape);
	}
	xrtFree(pMask->pShapes);
	memset(pMask, 0, sizeof(*pMask));
}

static void __xgeSvgMasksClear(xge_svg pSvg)
{
	int i;

	if ( !__xgeSvgValid(pSvg) ) {
		return;
	}
	for ( i = 0; i < pSvg->iMaskCount; i++ ) {
		__xgeSvgMaskReset(&pSvg->pMasks[i]);
	}
	xrtFree(pSvg->pMasks);
	pSvg->pMasks = NULL;
	pSvg->iMaskCount = 0;
	pSvg->iMaskCapacity = 0;
}

static int __xgeSvgMaskFind(xge_svg pSvg, const char* sId)
{
	int i;

	if ( !__xgeSvgValid(pSvg) || (sId == NULL) || (sId[0] == '\0') ) {
		return -1;
	}
	for ( i = 0; i < pSvg->iMaskCount; i++ ) {
		if ( (pSvg->pMasks[i].sId != NULL) && (strcmp(pSvg->pMasks[i].sId, sId) == 0) ) {
			return i;
		}
	}
	return -1;
}

static int __xgeSvgMaskGetOrCreate(xge_svg pSvg, const char* sId, int* pIndex)
{
	xge_svg_mask_t* pMasks;
	int iIndex;
	int iCapacity;

	if ( !__xgeSvgValid(pSvg) || (sId == NULL) || (sId[0] == '\0') || (pIndex == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xgeSvgMaskFind(pSvg, sId);
	if ( iIndex >= 0 ) {
		*pIndex = iIndex;
		return XGE_OK;
	}
	if ( pSvg->iMaskCount >= pSvg->iMaskCapacity ) {
		iCapacity = pSvg->iMaskCapacity > 0 ? pSvg->iMaskCapacity * 2 : 8;
		pMasks = (xge_svg_mask_t*)xrtRealloc(pSvg->pMasks, (size_t)iCapacity * sizeof(*pMasks));
		if ( pMasks == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		memset(pMasks + pSvg->iMaskCapacity, 0, (size_t)(iCapacity - pSvg->iMaskCapacity) * sizeof(*pMasks));
		pSvg->pMasks = pMasks;
		pSvg->iMaskCapacity = iCapacity;
	}
	iIndex = pSvg->iMaskCount++;
	pSvg->pMasks[iIndex].sId = __xgeStrDup(sId);
	if ( pSvg->pMasks[iIndex].sId == NULL ) {
		pSvg->iMaskCount--;
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pSvg->pMasks[iIndex].iContentUnits = XGE_SVG_MASK_USER_SPACE;
	pSvg->pMasks[iIndex].iUnits = XGE_SVG_MASK_OBJECT_BOUNDING_BOX;
	pSvg->pMasks[iIndex].iType = XGE_SVG_MASK_TYPE_LUMINANCE;
	pSvg->pMasks[iIndex].tRegion = (xge_rect_t){-0.1f, -0.1f, 1.2f, 1.2f};
	*pIndex = iIndex;
	return XGE_OK;
}

static int __xgeSvgMaskAddShape(xge_svg_mask_t* pMask, xge_shape_ex pShape)
{
	xge_svg_mask_shape_t* pShapes;
	int iCapacity;
	int iRet;

	if ( (pMask == NULL) || (pShape == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pMask->iShapeCount >= pMask->iShapeCapacity ) {
		iCapacity = pMask->iShapeCapacity > 0 ? pMask->iShapeCapacity * 2 : 4;
		pShapes = (xge_svg_mask_shape_t*)xrtRealloc(pMask->pShapes, (size_t)iCapacity * sizeof(*pShapes));
		if ( pShapes == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		pMask->pShapes = pShapes;
		pMask->iShapeCapacity = iCapacity;
	}
	iRet = xgeShapeExAddRef(pShape);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pMask->pShapes[pMask->iShapeCount].pShape = pShape;
	pMask->iShapeCount++;
	return XGE_OK;
}

static void __xgeSvgLinearGradientReset(xge_svg_linear_gradient_t* pGradient)
{
	if ( pGradient == NULL ) {
		return;
	}
	xrtFree(pGradient->sId);
	xrtFree(pGradient->sHrefId);
	xrtFree(pGradient->pStops);
	memset(pGradient, 0, sizeof(*pGradient));
}

static void __xgeSvgLinearGradientsClear(xge_svg pSvg)
{
	int i;

	if ( !__xgeSvgValid(pSvg) ) {
		return;
	}
	for ( i = 0; i < pSvg->iLinearGradientCount; i++ ) {
		__xgeSvgLinearGradientReset(&pSvg->pLinearGradients[i]);
	}
	xrtFree(pSvg->pLinearGradients);
	pSvg->pLinearGradients = NULL;
	pSvg->iLinearGradientCount = 0;
	pSvg->iLinearGradientCapacity = 0;
}

static void __xgeSvgStyleRuleReset(xge_svg_style_rule_t* pRule)
{
	if ( pRule == NULL ) {
		return;
	}
	xrtFree(pRule->sName);
	xrtFree(pRule->sStyle);
	memset(pRule, 0, sizeof(*pRule));
}

static void __xgeSvgStyleRulesClear(xge_svg pSvg)
{
	int i;

	if ( !__xgeSvgValid(pSvg) ) {
		return;
	}
	for ( i = 0; i < pSvg->iStyleRuleCount; i++ ) {
		__xgeSvgStyleRuleReset(&pSvg->pStyleRules[i]);
	}
	xrtFree(pSvg->pStyleRules);
	pSvg->pStyleRules = NULL;
	pSvg->iStyleRuleCount = 0;
	pSvg->iStyleRuleCapacity = 0;
}

static void __xgeSvgPendingUsesClear(xge_svg pSvg)
{
	if ( !__xgeSvgValid(pSvg) ) {
		return;
	}
	xrtFree(pSvg->pPendingUses);
	pSvg->pPendingUses = NULL;
	pSvg->iPendingUseCount = 0;
	pSvg->iPendingUseCapacity = 0;
}

static void __xgeSvgPendingShapeResourcesClear(xge_svg pSvg)
{
	int i;

	if ( !__xgeSvgValid(pSvg) ) {
		return;
	}
	for ( i = 0; i < pSvg->iPendingShapeResourceCount; i++ ) {
		xgeShapeExDestroy(pSvg->pPendingShapeResources[i].pShape);
	}
	xrtFree(pSvg->pPendingShapeResources);
	pSvg->pPendingShapeResources = NULL;
	pSvg->iPendingShapeResourceCount = 0;
	pSvg->iPendingShapeResourceCapacity = 0;
}

static int __xgeSvgPendingShapeResourceAdd(xge_svg pSvg, xge_shape_ex pShape, const xge_svg_style_t* pStyle, int iFlags)
{
	xge_svg_pending_shape_resource_t* pResources;
	xge_svg_pending_shape_resource_t* pResource;
	int i;
	int iCapacity;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pShape == NULL) || (pStyle == NULL) || (iFlags == 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < pSvg->iPendingShapeResourceCount; i++ ) {
		pResource = &pSvg->pPendingShapeResources[i];
		if ( pResource->pShape == pShape ) {
			pResource->tStyle = *pStyle;
			pResource->iFlags |= iFlags;
			return XGE_OK;
		}
	}
	if ( pSvg->iPendingShapeResourceCount >= pSvg->iPendingShapeResourceCapacity ) {
		iCapacity = pSvg->iPendingShapeResourceCapacity > 0 ? pSvg->iPendingShapeResourceCapacity * 2 : 16;
		pResources = (xge_svg_pending_shape_resource_t*)xrtRealloc(
			pSvg->pPendingShapeResources, (size_t)iCapacity * sizeof(*pResources)
		);
		if ( pResources == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		memset(
			pResources + pSvg->iPendingShapeResourceCapacity, 0,
			(size_t)(iCapacity - pSvg->iPendingShapeResourceCapacity) * sizeof(*pResources)
		);
		pSvg->pPendingShapeResources = pResources;
		pSvg->iPendingShapeResourceCapacity = iCapacity;
	}
	iRet = xgeShapeExAddRef(pShape);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pResource = &pSvg->pPendingShapeResources[pSvg->iPendingShapeResourceCount++];
	memset(pResource, 0, sizeof(*pResource));
	pResource->pShape = pShape;
	pResource->tStyle = *pStyle;
	pResource->iFlags = iFlags;
	return XGE_OK;
}

static int __xgeSvgPendingUseAdd(xge_svg pSvg, const char* sId, const xge_svg_style_t* pStyle, int iTargetDef, int iMainIdDefIndex, int iExtraMainIdDefIndex, float fX, float fY, float fW, float fH, int bHasWidth, int bHasHeight)
{
	xge_svg_pending_use_t* pPendingUses;
	xge_svg_pending_use_t* pUse;
	xge_svg_draw_item_t* pItem;
	int iCapacity;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (sId == NULL) || (sId[0] == '\0') || (pStyle == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pSvg->iPendingUseCount >= pSvg->iPendingUseCapacity ) {
		iCapacity = pSvg->iPendingUseCapacity > 0 ? pSvg->iPendingUseCapacity * 2 : 16;
		pPendingUses = (xge_svg_pending_use_t*)xrtRealloc(pSvg->pPendingUses, (size_t)iCapacity * sizeof(*pPendingUses));
		if ( pPendingUses == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		memset(pPendingUses + pSvg->iPendingUseCapacity, 0, (size_t)(iCapacity - pSvg->iPendingUseCapacity) * sizeof(*pPendingUses));
		pSvg->pPendingUses = pPendingUses;
		pSvg->iPendingUseCapacity = iCapacity;
	}
	pUse = &pSvg->pPendingUses[pSvg->iPendingUseCount++];
	memset(pUse, 0, sizeof(*pUse));
	{
		size_t iLen = strlen(sId);
		if ( iLen >= sizeof(pUse->sId) ) {
			iLen = sizeof(pUse->sId) - 1u;
		}
		memcpy(pUse->sId, sId, iLen);
		pUse->sId[iLen] = '\0';
	}
	pUse->tStyle = *pStyle;
	pUse->fX = fX;
	pUse->fY = fY;
	pUse->fW = fW;
	pUse->fH = fH;
	pUse->bHasWidth = bHasWidth;
	pUse->bHasHeight = bHasHeight;
	pUse->iTargetDef = iTargetDef;
	pUse->iMainIdDefIndex = iMainIdDefIndex;
	pUse->iExtraMainIdDefIndex = iExtraMainIdDefIndex;
	pUse->iSerial = pSvg->iPendingUseCount;
	if ( iTargetDef < 0 ) {
		iRet = __xgeSvgDrawItemReserve(pSvg);
		if ( iRet != XGE_OK ) {
			pSvg->iPendingUseCount--;
			memset(pUse, 0, sizeof(*pUse));
			return iRet;
		}
		pItem = &pSvg->pItems[pSvg->iItemCount++];
		memset(pItem, 0, sizeof(*pItem));
		pItem->iType = XGE_SVG_DRAW_ITEM_PENDING_USE;
		pItem->u.iPendingUseSerial = pUse->iSerial;
	}
	return XGE_OK;
}

static int __xgeSvgDrawItemsReplacePendingUse(
	xge_svg pSvg,
	xge_svg_draw_item_t** ppItems,
	int* pItemCount,
	int* pItemCapacity,
	int iSerial,
	const xge_svg_draw_item_t* pReplacement,
	int iReplacementCount,
	int* pFound
)
{
	xge_svg_draw_item_t* pItems;
	int i;

	if ( !__xgeSvgValid(pSvg) || (ppItems == NULL) || (pItemCount == NULL) || (pFound == NULL) ||
	     (iSerial <= 0) || (iReplacementCount < 0) ||
	     ((iReplacementCount > 0) && (pReplacement == NULL)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pFound = 0;
	pItems = *ppItems;
	for ( i = 0; i < *pItemCount; i++ ) {
		if ( (pItems[i].iType == XGE_SVG_DRAW_ITEM_PENDING_USE) &&
		     (pItems[i].u.iPendingUseSerial == iSerial) ) {
			int iOldCount = *pItemCount;
			int iNewCount = iOldCount - 1 + iReplacementCount;

			if ( (pItemCapacity != NULL) && (iNewCount > *pItemCapacity) ) {
				int iCapacity = *pItemCapacity > 0 ? *pItemCapacity : 32;

				while ( iCapacity < iNewCount ) iCapacity *= 2;
				pItems = (xge_svg_draw_item_t*)xrtRealloc(
					pItems, (size_t)iCapacity * sizeof(*pItems)
				);
				if ( pItems == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
				memset(pItems + *pItemCapacity, 0,
				       (size_t)(iCapacity - *pItemCapacity) * sizeof(*pItems));
				*ppItems = pItems;
				*pItemCapacity = iCapacity;
			} else if ( (pItemCapacity == NULL) && (iNewCount > iOldCount) ) {
				pItems = (xge_svg_draw_item_t*)xrtRealloc(
					pItems, (size_t)iNewCount * sizeof(*pItems)
				);
				if ( pItems == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
				*ppItems = pItems;
			}
			memmove(
				pItems + i + iReplacementCount,
				pItems + i + 1,
				(size_t)(iOldCount - i - 1) * sizeof(*pItems)
			);
			if ( iReplacementCount > 0 ) {
				memcpy(pItems + i, pReplacement,
				       (size_t)iReplacementCount * sizeof(*pItems));
			}
			if ( iNewCount < iOldCount ) {
				memset(pItems + iNewCount, 0,
				       (size_t)(iOldCount - iNewCount) * sizeof(*pItems));
			}
			*pItemCount = iNewCount;
			*pFound = 1;
			return XGE_OK;
		}
		if ( (pItems[i].iType == XGE_SVG_DRAW_ITEM_GROUP) &&
		     (pItems[i].u.pGroup != NULL) ) {
			xge_svg_group_item_t* pGroup = pItems[i].u.pGroup;
			int iRet = __xgeSvgDrawItemsReplacePendingUse(
				pSvg,
				&pGroup->pItems, &pGroup->iItemCount, NULL, iSerial,
				pReplacement, iReplacementCount, pFound
			);

			if ( iRet != XGE_OK ) return iRet;
			if ( *pFound ) {
				int j;
				xgeShapeExSceneDestroy(pGroup->pMaskScene);
				pGroup->pMaskScene = NULL;
				pGroup->bMaskResolved = 0;
				pGroup->bMaskEmpty = 0;
				pGroup->bMasked = pGroup->sMaskId[0] != '\0';
				for ( j = 0; j < pGroup->iItemCount; j++ ) {
					if ( __xgeSvgDrawItemHasMask(&pGroup->pItems[j]) ) {
						pGroup->bMasked = 1;
						break;
					}
				}
				return XGE_OK;
			}
		} else if ( (pItems[i].iType == XGE_SVG_DRAW_ITEM_FILTER) &&
		            (pItems[i].u.pFilter != NULL) ) {
			xge_svg_filter_effect_t* pFilter = pItems[i].u.pFilter;
			int iRet = __xgeSvgDrawItemsReplacePendingUse(
				pSvg,
				&pFilter->pItems, &pFilter->iItemCount, NULL, iSerial,
				pReplacement, iReplacementCount, pFound
			);

			if ( iRet != XGE_OK ) return iRet;
			if ( *pFound ) {
				iRet = __xgeSvgFilterEffectRefreshPendingUseBounds(pSvg, pFilter);
				return iRet;
			}
		}
	}
	return XGE_OK;
}

static int __xgeSvgReplacePendingUseMainItems(xge_svg pSvg, int iSerial, int iFirstItem)
{
	xge_svg_draw_item_t* pReplacement;
	int iReplacementCount;
	int bFound;
	int i;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (iSerial <= 0) ||
	     (iFirstItem < 0) || (iFirstItem > pSvg->iItemCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iReplacementCount = pSvg->iItemCount - iFirstItem;
	pReplacement = NULL;
	if ( iReplacementCount > 0 ) {
		pReplacement = (xge_svg_draw_item_t*)xrtMalloc(
			(size_t)iReplacementCount * sizeof(*pReplacement)
		);
		if ( pReplacement == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
		memcpy(pReplacement, pSvg->pItems + iFirstItem,
		       (size_t)iReplacementCount * sizeof(*pReplacement));
		memset(pSvg->pItems + iFirstItem, 0,
		       (size_t)iReplacementCount * sizeof(*pReplacement));
	}
	pSvg->iItemCount = iFirstItem;
	iRet = __xgeSvgDrawItemsReplacePendingUse(
		pSvg,
		&pSvg->pItems, &pSvg->iItemCount, &pSvg->iItemCapacity,
		iSerial, pReplacement, iReplacementCount, &bFound
	);
	if ( (iRet != XGE_OK) || !bFound ) {
		for ( i = 0; i < iReplacementCount; i++ ) {
			__xgeSvgDrawItemReset(&pReplacement[i]);
		}
	}
	xrtFree(pReplacement);
	return iRet;
}

static void __xgeSvgTrimRange(const char** ppStart, const char** ppEnd)
{
	const char* pStart;
	const char* pEnd;

	if ( (ppStart == NULL) || (ppEnd == NULL) || (*ppStart == NULL) || (*ppEnd == NULL) ) {
		return;
	}
	pStart = *ppStart;
	pEnd = *ppEnd;
	while ( (pStart < pEnd) && ((*pStart == ' ') || (*pStart == '\t') || (*pStart == '\r') || (*pStart == '\n')) ) {
		pStart++;
	}
	while ( (pEnd > pStart) && ((pEnd[-1] == ' ') || (pEnd[-1] == '\t') || (pEnd[-1] == '\r') || (pEnd[-1] == '\n')) ) {
		pEnd--;
	}
	*ppStart = pStart;
	*ppEnd = pEnd;
}

static int __xgeSvgRangeStartsWith(const char* pStart, const char* pEnd, const char* sText)
{
	int iLen;

	if ( (pStart == NULL) || (pEnd == NULL) || (sText == NULL) || (pEnd < pStart) ) {
		return 0;
	}
	iLen = (int)strlen(sText);
	return ((int)(pEnd - pStart) >= iLen) && (strncmp(pStart, sText, (size_t)iLen) == 0);
}

static int __xgeSvgRangeDup(const char* pStart, const char* pEnd, char** ppOut)
{
	char* sOut;
	int iLen;

	if ( ppOut == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppOut = NULL;
	if ( (pStart == NULL) || (pEnd == NULL) || (pEnd < pStart) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iLen = (int)(pEnd - pStart);
	sOut = (char*)xrtMalloc((size_t)iLen + 1u);
	if ( sOut == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( iLen > 0 ) {
		memcpy(sOut, pStart, (size_t)iLen);
	}
	sOut[iLen] = '\0';
	*ppOut = sOut;
	return XGE_OK;
}

static int __xgeSvgStyleNormalizeThorvgClass(const char* sStyle, char** ppNormalized);

static int __xgeSvgStyleRuleAdd(xge_svg pSvg, int iSelectorType, const char* pNameStart, const char* pNameEnd, const char* pStyleStart, const char* pStyleEnd)
{
	xge_svg_style_rule_t* pRules;
	xge_svg_style_rule_t* pRule;
	char* sMergedStyle;
	char* sNormalizedStyle;
	int i;
	int iCapacity;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pNameStart == NULL) || (pNameEnd == NULL) || (pStyleStart == NULL) || (pStyleEnd == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	__xgeSvgTrimRange(&pNameStart, &pNameEnd);
	if ( pNameEnd <= pNameStart ) {
		return XGE_OK;
	}
	__xgeSvgTrimRange(&pStyleStart, &pStyleEnd);
	if ( iSelectorType == XGE_SVG_STYLE_SELECTOR_CLASS ) {
		int iNameLen = (int)(pNameEnd - pNameStart);
		int iStyleLen = (int)(pStyleEnd - pStyleStart);

		for ( i = 0; i < pSvg->iStyleRuleCount; i++ ) {
			int iOldLen;
			int iSeparatorLen;

			pRule = &pSvg->pStyleRules[i];
			if ( (pRule->iSelectorType != iSelectorType) || (pRule->sName == NULL) ||
			     ((int)strlen(pRule->sName) != iNameLen) ||
			     (strncmp(pRule->sName, pNameStart, (size_t)iNameLen) != 0) ) {
				continue;
			}
			if ( iStyleLen <= 0 ) {
				return XGE_OK;
			}
			iOldLen = pRule->sStyle != NULL ? (int)strlen(pRule->sStyle) : 0;
			iSeparatorLen = iOldLen > 0 ? 1 : 0;
			sMergedStyle = (char*)xrtRealloc(pRule->sStyle, (size_t)(iOldLen + iSeparatorLen + iStyleLen + 1));
			if ( sMergedStyle == NULL ) {
				return XGE_ERROR_OUT_OF_MEMORY;
			}
			if ( iSeparatorLen > 0 ) {
				sMergedStyle[iOldLen] = ';';
			}
			memcpy(sMergedStyle + iOldLen + iSeparatorLen, pStyleStart, (size_t)iStyleLen);
			sMergedStyle[iOldLen + iSeparatorLen + iStyleLen] = '\0';
			pRule->sStyle = sMergedStyle;
			sNormalizedStyle = NULL;
			iRet = __xgeSvgStyleNormalizeThorvgClass(pRule->sStyle, &sNormalizedStyle);
			if ( iRet != XGE_OK ) {
				return iRet;
			}
			xrtFree(pRule->sStyle);
			pRule->sStyle = sNormalizedStyle;
			return XGE_OK;
		}
	}
	if ( pSvg->iStyleRuleCount >= pSvg->iStyleRuleCapacity ) {
		iCapacity = pSvg->iStyleRuleCapacity > 0 ? pSvg->iStyleRuleCapacity * 2 : 16;
		pRules = (xge_svg_style_rule_t*)xrtRealloc(pSvg->pStyleRules, (size_t)iCapacity * sizeof(*pRules));
		if ( pRules == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		memset(pRules + pSvg->iStyleRuleCapacity, 0, (size_t)(iCapacity - pSvg->iStyleRuleCapacity) * sizeof(*pRules));
		pSvg->pStyleRules = pRules;
		pSvg->iStyleRuleCapacity = iCapacity;
	}
	pRule = &pSvg->pStyleRules[pSvg->iStyleRuleCount];
	memset(pRule, 0, sizeof(*pRule));
	pRule->iSelectorType = iSelectorType;
	pRule->pSourcePosition = pStyleStart;
	iRet = __xgeSvgRangeDup(pNameStart, pNameEnd, &pRule->sName);
	if ( iRet == XGE_OK ) {
		iRet = __xgeSvgRangeDup(pStyleStart, pStyleEnd, &pRule->sStyle);
	}
	if ( (iRet == XGE_OK) && (iSelectorType == XGE_SVG_STYLE_SELECTOR_CLASS) ) {
		sNormalizedStyle = NULL;
		iRet = __xgeSvgStyleNormalizeThorvgClass(pRule->sStyle, &sNormalizedStyle);
		if ( iRet == XGE_OK ) {
			xrtFree(pRule->sStyle);
			pRule->sStyle = sNormalizedStyle;
		}
	}
	if ( iRet != XGE_OK ) {
		__xgeSvgStyleRuleReset(pRule);
		return iRet;
	}
	pSvg->iStyleRuleCount++;
	return XGE_OK;
}

static int __xgeSvgLinearGradientFind(xge_svg pSvg, const char* sId)
{
	int i;

	if ( !__xgeSvgValid(pSvg) || (sId == NULL) || (sId[0] == '\0') ) {
		return -1;
	}
	for ( i = 0; i < pSvg->iLinearGradientCount; i++ ) {
		if ( (pSvg->pLinearGradients[i].sId != NULL) && (strcmp(pSvg->pLinearGradients[i].sId, sId) == 0) ) {
			return i;
		}
	}
	return -1;
}

static int __xgeSvgLinearGradientGetOrCreate(xge_svg pSvg, const char* sId, int* pIndex)
{
	xge_svg_linear_gradient_t* pGradients;
	int iIndex;
	int iCapacity;

	if ( !__xgeSvgValid(pSvg) || (sId == NULL) || (sId[0] == '\0') || (pIndex == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xgeSvgLinearGradientFind(pSvg, sId);
	if ( iIndex >= 0 ) {
		*pIndex = iIndex;
		return XGE_OK;
	}
	if ( pSvg->iLinearGradientCount >= pSvg->iLinearGradientCapacity ) {
		iCapacity = pSvg->iLinearGradientCapacity > 0 ? pSvg->iLinearGradientCapacity * 2 : 8;
		pGradients = (xge_svg_linear_gradient_t*)xrtRealloc(pSvg->pLinearGradients, (size_t)iCapacity * sizeof(*pGradients));
		if ( pGradients == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		memset(pGradients + pSvg->iLinearGradientCapacity, 0, (size_t)(iCapacity - pSvg->iLinearGradientCapacity) * sizeof(*pGradients));
		pSvg->pLinearGradients = pGradients;
		pSvg->iLinearGradientCapacity = iCapacity;
	}
	iIndex = pSvg->iLinearGradientCount++;
	pSvg->pLinearGradients[iIndex].sId = __xgeStrDup(sId);
	if ( pSvg->pLinearGradients[iIndex].sId == NULL ) {
		pSvg->iLinearGradientCount--;
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pSvg->pLinearGradients[iIndex].fX1 = 0.0f;
	pSvg->pLinearGradients[iIndex].fY1 = 0.0f;
	pSvg->pLinearGradients[iIndex].fX2 = 1.0f;
	pSvg->pLinearGradients[iIndex].fY2 = 0.0f;
	pSvg->pLinearGradients[iIndex].iUnits = XGE_SHAPE_EX_GRADIENT_OBJECT_BOUNDING_BOX;
	pSvg->pLinearGradients[iIndex].iSpread = XGE_SHAPE_EX_GRADIENT_SPREAD_PAD;
	pSvg->pLinearGradients[iIndex].iPercentFlags = XGE_SVG_GRADIENT_HAS_X1 | XGE_SVG_GRADIENT_HAS_Y1 |
		XGE_SVG_GRADIENT_HAS_X2 | XGE_SVG_GRADIENT_HAS_Y2;
	pSvg->pLinearGradients[iIndex].tTransform = __xgeSvgMatrixIdentity();
	*pIndex = iIndex;
	return XGE_OK;
}

static int __xgeSvgLinearGradientAddStop(xge_svg_linear_gradient_t* pGradient, xge_shape_ex_color_stop_t tStop)
{
	xge_shape_ex_color_stop_t* pStops;
	int iCapacity;

	if ( pGradient == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pGradient->iStopCount >= pGradient->iStopCapacity ) {
		iCapacity = pGradient->iStopCapacity > 0 ? pGradient->iStopCapacity * 2 : 4;
		pStops = (xge_shape_ex_color_stop_t*)xrtRealloc(pGradient->pStops, (size_t)iCapacity * sizeof(*pStops));
		if ( pStops == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		pGradient->pStops = pStops;
		pGradient->iStopCapacity = iCapacity;
	}
	if ( tStop.fOffset < 0.0f ) tStop.fOffset = 0.0f;
	if ( tStop.fOffset > 1.0f ) tStop.fOffset = 1.0f;
	pGradient->pStops[pGradient->iStopCount++] = tStop;
	pGradient->iFlags |= XGE_SVG_GRADIENT_HAS_STOPS;
	return XGE_OK;
}

static void __xgeSvgRadialGradientReset(xge_svg_radial_gradient_t* pGradient)
{
	if ( pGradient == NULL ) {
		return;
	}
	xrtFree(pGradient->sId);
	xrtFree(pGradient->sHrefId);
	xrtFree(pGradient->pStops);
	memset(pGradient, 0, sizeof(*pGradient));
}

static void __xgeSvgRadialGradientsClear(xge_svg pSvg)
{
	int i;

	if ( !__xgeSvgValid(pSvg) ) {
		return;
	}
	for ( i = 0; i < pSvg->iRadialGradientCount; i++ ) {
		__xgeSvgRadialGradientReset(&pSvg->pRadialGradients[i]);
	}
	xrtFree(pSvg->pRadialGradients);
	pSvg->pRadialGradients = NULL;
	pSvg->iRadialGradientCount = 0;
	pSvg->iRadialGradientCapacity = 0;
}

static int __xgeSvgRadialGradientFind(xge_svg pSvg, const char* sId)
{
	int i;

	if ( !__xgeSvgValid(pSvg) || (sId == NULL) || (sId[0] == '\0') ) {
		return -1;
	}
	for ( i = 0; i < pSvg->iRadialGradientCount; i++ ) {
		if ( (pSvg->pRadialGradients[i].sId != NULL) && (strcmp(pSvg->pRadialGradients[i].sId, sId) == 0) ) {
			return i;
		}
	}
	return -1;
}

static int __xgeSvgRadialGradientGetOrCreate(xge_svg pSvg, const char* sId, int* pIndex)
{
	xge_svg_radial_gradient_t* pGradients;
	int iIndex;
	int iCapacity;

	if ( !__xgeSvgValid(pSvg) || (sId == NULL) || (sId[0] == '\0') || (pIndex == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xgeSvgRadialGradientFind(pSvg, sId);
	if ( iIndex >= 0 ) {
		*pIndex = iIndex;
		return XGE_OK;
	}
	if ( pSvg->iRadialGradientCount >= pSvg->iRadialGradientCapacity ) {
		iCapacity = pSvg->iRadialGradientCapacity > 0 ? pSvg->iRadialGradientCapacity * 2 : 8;
		pGradients = (xge_svg_radial_gradient_t*)xrtRealloc(pSvg->pRadialGradients, (size_t)iCapacity * sizeof(*pGradients));
		if ( pGradients == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		memset(pGradients + pSvg->iRadialGradientCapacity, 0, (size_t)(iCapacity - pSvg->iRadialGradientCapacity) * sizeof(*pGradients));
		pSvg->pRadialGradients = pGradients;
		pSvg->iRadialGradientCapacity = iCapacity;
	}
	iIndex = pSvg->iRadialGradientCount++;
	pSvg->pRadialGradients[iIndex].sId = __xgeStrDup(sId);
	if ( pSvg->pRadialGradients[iIndex].sId == NULL ) {
		pSvg->iRadialGradientCount--;
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pSvg->pRadialGradients[iIndex].fCX = 0.5f;
	pSvg->pRadialGradients[iIndex].fCY = 0.5f;
	pSvg->pRadialGradients[iIndex].fR = 0.5f;
	pSvg->pRadialGradients[iIndex].fFX = 0.5f;
	pSvg->pRadialGradients[iIndex].fFY = 0.5f;
	pSvg->pRadialGradients[iIndex].fFR = 0.0f;
	pSvg->pRadialGradients[iIndex].iUnits = XGE_SHAPE_EX_GRADIENT_OBJECT_BOUNDING_BOX;
	pSvg->pRadialGradients[iIndex].iSpread = XGE_SHAPE_EX_GRADIENT_SPREAD_PAD;
	pSvg->pRadialGradients[iIndex].iPercentFlags = XGE_SVG_GRADIENT_HAS_CX | XGE_SVG_GRADIENT_HAS_CY |
		XGE_SVG_GRADIENT_HAS_R | XGE_SVG_GRADIENT_HAS_FX | XGE_SVG_GRADIENT_HAS_FY | XGE_SVG_GRADIENT_HAS_FR;
	pSvg->pRadialGradients[iIndex].tTransform = __xgeSvgMatrixIdentity();
	*pIndex = iIndex;
	return XGE_OK;
}

static int __xgeSvgRadialGradientAddStop(xge_svg_radial_gradient_t* pGradient, xge_shape_ex_color_stop_t tStop)
{
	xge_shape_ex_color_stop_t* pStops;
	int iCapacity;

	if ( pGradient == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pGradient->iStopCount >= pGradient->iStopCapacity ) {
		iCapacity = pGradient->iStopCapacity > 0 ? pGradient->iStopCapacity * 2 : 4;
		pStops = (xge_shape_ex_color_stop_t*)xrtRealloc(pGradient->pStops, (size_t)iCapacity * sizeof(*pStops));
		if ( pStops == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		pGradient->pStops = pStops;
		pGradient->iStopCapacity = iCapacity;
	}
	if ( tStop.fOffset < 0.0f ) tStop.fOffset = 0.0f;
	if ( tStop.fOffset > 1.0f ) tStop.fOffset = 1.0f;
	pGradient->pStops[pGradient->iStopCount++] = tStop;
	pGradient->iFlags |= XGE_SVG_GRADIENT_HAS_STOPS;
	return XGE_OK;
}

static int __xgeSvgLinearGradientInheritRadialCommon(xge_svg_linear_gradient_t* pGradient, const xge_svg_radial_gradient_t* pSource)
{
	int i;
	int iRet;

	if ( (pGradient == NULL) || (pSource == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_UNITS) == 0 ) pGradient->iUnits = pSource->iUnits;
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_SPREAD) == 0 ) pGradient->iSpread = pSource->iSpread;
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_TRANSFORM) == 0 ) pGradient->tTransform = pSource->tTransform;
	if ( ((pGradient->iFlags & XGE_SVG_GRADIENT_HAS_STOPS) == 0) && (pSource->iStopCount > 0) ) {
		pGradient->iStopCount = 0;
		for ( i = 0; i < pSource->iStopCount; i++ ) {
			iRet = __xgeSvgLinearGradientAddStop(pGradient, pSource->pStops[i]);
			if ( iRet != XGE_OK ) {
				return iRet;
			}
		}
	}
	return XGE_OK;
}

static int __xgeSvgRadialGradientInheritLinearCommon(xge_svg_radial_gradient_t* pGradient, const xge_svg_linear_gradient_t* pSource)
{
	int i;
	int iRet;

	if ( (pGradient == NULL) || (pSource == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_UNITS) == 0 ) pGradient->iUnits = pSource->iUnits;
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_SPREAD) == 0 ) pGradient->iSpread = pSource->iSpread;
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_TRANSFORM) == 0 ) pGradient->tTransform = pSource->tTransform;
	if ( ((pGradient->iFlags & XGE_SVG_GRADIENT_HAS_STOPS) == 0) && (pSource->iStopCount > 0) ) {
		pGradient->iStopCount = 0;
		for ( i = 0; i < pSource->iStopCount; i++ ) {
			iRet = __xgeSvgRadialGradientAddStop(pGradient, pSource->pStops[i]);
			if ( iRet != XGE_OK ) {
				return iRet;
			}
		}
	}
	return XGE_OK;
}

static int __xgeSvgResolveRadialGradient(xge_svg pSvg, int iIndex, int iDepth);

static int __xgeSvgResolveLinearGradient(xge_svg pSvg, int iIndex, int iDepth)
{
	xge_svg_linear_gradient_t* pGradient;
	xge_svg_linear_gradient_t* pSource;
	xge_svg_radial_gradient_t* pRadialSource;
	int iSource;
	int i;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (iIndex < 0) || (iIndex >= pSvg->iLinearGradientCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iDepth > (pSvg->iLinearGradientCount + pSvg->iRadialGradientCount) ) {
		return XGE_OK;
	}
	pGradient = &pSvg->pLinearGradients[iIndex];
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_RESOLVED) != 0 ) {
		return XGE_OK;
	}
	if ( (pGradient->sHrefId == NULL) || (pGradient->sHrefId[0] == '\0') ) {
		pGradient->iFlags |= XGE_SVG_GRADIENT_RESOLVED;
		return XGE_OK;
	}
	iSource = __xgeSvgLinearGradientFind(pSvg, pGradient->sHrefId);
	if ( iSource < 0 ) {
		iSource = __xgeSvgRadialGradientFind(pSvg, pGradient->sHrefId);
		if ( iSource < 0 ) {
			return XGE_OK;
		}
		iRet = __xgeSvgResolveRadialGradient(pSvg, iSource, iDepth + 1);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		pRadialSource = &pSvg->pRadialGradients[iSource];
		iRet = __xgeSvgLinearGradientInheritRadialCommon(pGradient, pRadialSource);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		pGradient->iFlags |= XGE_SVG_GRADIENT_RESOLVED;
		return XGE_OK;
	}
	if ( iSource == iIndex ) {
		pGradient->iFlags |= XGE_SVG_GRADIENT_RESOLVED;
		return XGE_OK;
	}
	iRet = __xgeSvgResolveLinearGradient(pSvg, iSource, iDepth + 1);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pSource = &pSvg->pLinearGradients[iSource];
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_X1) == 0 ) {
		pGradient->fX1 = pSource->fX1;
		pGradient->iPercentFlags = (pGradient->iPercentFlags & ~XGE_SVG_GRADIENT_HAS_X1) | (pSource->iPercentFlags & XGE_SVG_GRADIENT_HAS_X1);
	}
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_Y1) == 0 ) {
		pGradient->fY1 = pSource->fY1;
		pGradient->iPercentFlags = (pGradient->iPercentFlags & ~XGE_SVG_GRADIENT_HAS_Y1) | (pSource->iPercentFlags & XGE_SVG_GRADIENT_HAS_Y1);
	}
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_X2) == 0 ) {
		pGradient->fX2 = pSource->fX2;
		pGradient->iPercentFlags = (pGradient->iPercentFlags & ~XGE_SVG_GRADIENT_HAS_X2) | (pSource->iPercentFlags & XGE_SVG_GRADIENT_HAS_X2);
	}
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_Y2) == 0 ) {
		pGradient->fY2 = pSource->fY2;
		pGradient->iPercentFlags = (pGradient->iPercentFlags & ~XGE_SVG_GRADIENT_HAS_Y2) | (pSource->iPercentFlags & XGE_SVG_GRADIENT_HAS_Y2);
	}
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_UNITS) == 0 ) pGradient->iUnits = pSource->iUnits;
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_SPREAD) == 0 ) pGradient->iSpread = pSource->iSpread;
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_TRANSFORM) == 0 ) pGradient->tTransform = pSource->tTransform;
	if ( ((pGradient->iFlags & XGE_SVG_GRADIENT_HAS_STOPS) == 0) && (pSource->iStopCount > 0) ) {
		pGradient->iStopCount = 0;
		for ( i = 0; i < pSource->iStopCount; i++ ) {
			iRet = __xgeSvgLinearGradientAddStop(pGradient, pSource->pStops[i]);
			if ( iRet != XGE_OK ) {
				return iRet;
			}
		}
	}
	pGradient->iFlags |= XGE_SVG_GRADIENT_RESOLVED;
	return XGE_OK;
}

static int __xgeSvgResolveRadialGradient(xge_svg pSvg, int iIndex, int iDepth)
{
	xge_svg_radial_gradient_t* pGradient;
	xge_svg_radial_gradient_t* pSource;
	xge_svg_linear_gradient_t* pLinearSource;
	int iSource;
	int i;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (iIndex < 0) || (iIndex >= pSvg->iRadialGradientCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iDepth > (pSvg->iLinearGradientCount + pSvg->iRadialGradientCount) ) {
		return XGE_OK;
	}
	pGradient = &pSvg->pRadialGradients[iIndex];
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_RESOLVED) != 0 ) {
		return XGE_OK;
	}
	if ( (pGradient->sHrefId == NULL) || (pGradient->sHrefId[0] == '\0') ) {
		pGradient->iFlags |= XGE_SVG_GRADIENT_RESOLVED;
		return XGE_OK;
	}
	iSource = __xgeSvgRadialGradientFind(pSvg, pGradient->sHrefId);
	if ( iSource < 0 ) {
		iSource = __xgeSvgLinearGradientFind(pSvg, pGradient->sHrefId);
		if ( iSource < 0 ) {
			return XGE_OK;
		}
		iRet = __xgeSvgResolveLinearGradient(pSvg, iSource, iDepth + 1);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		pLinearSource = &pSvg->pLinearGradients[iSource];
		iRet = __xgeSvgRadialGradientInheritLinearCommon(pGradient, pLinearSource);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		pGradient->iFlags |= XGE_SVG_GRADIENT_RESOLVED;
		return XGE_OK;
	}
	if ( iSource == iIndex ) {
		pGradient->iFlags |= XGE_SVG_GRADIENT_RESOLVED;
		return XGE_OK;
	}
	iRet = __xgeSvgResolveRadialGradient(pSvg, iSource, iDepth + 1);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pSource = &pSvg->pRadialGradients[iSource];
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_CX) == 0 ) {
		pGradient->fCX = pSource->fCX;
		pGradient->iPercentFlags = (pGradient->iPercentFlags & ~XGE_SVG_GRADIENT_HAS_CX) | (pSource->iPercentFlags & XGE_SVG_GRADIENT_HAS_CX);
	}
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_CY) == 0 ) {
		pGradient->fCY = pSource->fCY;
		pGradient->iPercentFlags = (pGradient->iPercentFlags & ~XGE_SVG_GRADIENT_HAS_CY) | (pSource->iPercentFlags & XGE_SVG_GRADIENT_HAS_CY);
	}
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_R) == 0 ) {
		pGradient->fR = pSource->fR;
		pGradient->iPercentFlags = (pGradient->iPercentFlags & ~XGE_SVG_GRADIENT_HAS_R) | (pSource->iPercentFlags & XGE_SVG_GRADIENT_HAS_R);
	}
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_FX) == 0 ) {
		pGradient->fFX = pSource->fFX;
		pGradient->iPercentFlags = (pGradient->iPercentFlags & ~XGE_SVG_GRADIENT_HAS_FX) | (pSource->iPercentFlags & XGE_SVG_GRADIENT_HAS_FX);
	}
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_FY) == 0 ) {
		pGradient->fFY = pSource->fFY;
		pGradient->iPercentFlags = (pGradient->iPercentFlags & ~XGE_SVG_GRADIENT_HAS_FY) | (pSource->iPercentFlags & XGE_SVG_GRADIENT_HAS_FY);
	}
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_FR) == 0 ) {
		pGradient->fFR = pSource->fFR;
		pGradient->iPercentFlags = (pGradient->iPercentFlags & ~XGE_SVG_GRADIENT_HAS_FR) | (pSource->iPercentFlags & XGE_SVG_GRADIENT_HAS_FR);
	}
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_UNITS) == 0 ) pGradient->iUnits = pSource->iUnits;
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_SPREAD) == 0 ) pGradient->iSpread = pSource->iSpread;
	if ( (pGradient->iFlags & XGE_SVG_GRADIENT_HAS_TRANSFORM) == 0 ) pGradient->tTransform = pSource->tTransform;
	if ( ((pGradient->iFlags & XGE_SVG_GRADIENT_HAS_STOPS) == 0) && (pSource->iStopCount > 0) ) {
		pGradient->iStopCount = 0;
		for ( i = 0; i < pSource->iStopCount; i++ ) {
			iRet = __xgeSvgRadialGradientAddStop(pGradient, pSource->pStops[i]);
			if ( iRet != XGE_OK ) {
				return iRet;
			}
		}
	}
	pGradient->iFlags |= XGE_SVG_GRADIENT_RESOLVED;
	return XGE_OK;
}

static int __xgeSvgResolveGradients(xge_svg pSvg)
{
	int i;
	int iRet;

	if ( !__xgeSvgValid(pSvg) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < pSvg->iLinearGradientCount; i++ ) {
		iRet = __xgeSvgResolveLinearGradient(pSvg, i, 0);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	for ( i = 0; i < pSvg->iRadialGradientCount; i++ ) {
		iRet = __xgeSvgResolveRadialGradient(pSvg, i, 0);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	return XGE_OK;
}

static int __xgeSvgIsNameEnd(char c)
{
	return (c == '\0') || (c == '>') || (c == '/') || (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}

static const char* __xgeSvgFindTagEnd(const char* pText)
{
	char cQuote;
	int iBracketDepth;
	int bUseBracketDepth;

	if ( pText == NULL ) {
		return NULL;
	}
	if ( strncmp(pText, "<!--", 4) == 0 ) {
		const char* pEnd = strstr(pText + 4, "-->");
		return pEnd != NULL ? pEnd + 2 : NULL;
	}
	if ( strncmp(pText, "<![CDATA[", 9) == 0 ) {
		const char* pEnd = strstr(pText + 9, "]]>");
		return pEnd != NULL ? pEnd + 2 : NULL;
	}
	if ( strncmp(pText, "<?", 2) == 0 ) {
		const char* pEnd = strstr(pText + 2, "?>");
		return pEnd != NULL ? pEnd + 1 : NULL;
	}
	cQuote = 0;
	iBracketDepth = 0;
	bUseBracketDepth = (pText[0] == '<') && (pText[1] == '!');
	while ( *pText != '\0' ) {
		if ( cQuote != 0 ) {
			if ( *pText == cQuote ) {
				cQuote = 0;
			}
		} else if ( (*pText == '"') || (*pText == '\'') ) {
			cQuote = *pText;
		} else if ( bUseBracketDepth && (pText[0] == '[') ) {
			iBracketDepth++;
		} else if ( bUseBracketDepth && (pText[0] == ']') && (iBracketDepth > 0) ) {
			iBracketDepth--;
		} else if ( (*pText == '>') && (iBracketDepth <= 0) ) {
			return pText;
		}
		pText++;
	}
	return NULL;
}

static int __xgeSvgTagNameEquals(const char* pTag, const char* sName)
{
	size_t iLen;

	if ( (pTag == NULL) || (sName == NULL) || (*pTag != '<') ) {
		return 0;
	}
	pTag++;
	if ( *pTag == '/' ) {
		return 0;
	}
	while ( (*pTag == ' ') || (*pTag == '\t') || (*pTag == '\r') || (*pTag == '\n') ) {
		pTag++;
	}
	iLen = strlen(sName);
	return (strncmp(pTag, sName, iLen) == 0) && __xgeSvgIsNameEnd(pTag[iLen]);
}

static int __xgeSvgTagNameCopy(const char* pTag, char* sOut, int iOutCapacity)
{
	const char* p;
	int iLen;

	if ( (pTag == NULL) || (sOut == NULL) || (iOutCapacity <= 0) || (*pTag != '<') ) {
		return 0;
	}
	sOut[0] = '\0';
	p = pTag + 1;
	if ( *p == '/' ) {
		return 0;
	}
	while ( (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') ) {
		p++;
	}
	iLen = 0;
	while ( (p[iLen] != '\0') && !__xgeSvgIsNameEnd(p[iLen]) ) {
		iLen++;
	}
	if ( iLen <= 0 ) {
		return 0;
	}
	if ( iLen >= iOutCapacity ) {
		iLen = iOutCapacity - 1;
	}
	memcpy(sOut, p, (size_t)iLen);
	sOut[iLen] = '\0';
	return 1;
}

static int __xgeSvgIsCloseTagName(const char* pTag, const char* sName)
{
	size_t iLen;

	if ( (pTag == NULL) || (sName == NULL) || (*pTag != '<') ) {
		return 0;
	}
	pTag++;
	while ( (*pTag == ' ') || (*pTag == '\t') || (*pTag == '\r') || (*pTag == '\n') ) {
		pTag++;
	}
	if ( *pTag != '/' ) {
		return 0;
	}
	pTag++;
	while ( (*pTag == ' ') || (*pTag == '\t') || (*pTag == '\r') || (*pTag == '\n') ) {
		pTag++;
	}
	iLen = strlen(sName);
	return (strncmp(pTag, sName, iLen) == 0) && __xgeSvgIsNameEnd(pTag[iLen]);
}

static int __xgeSvgTagNameIsGroupContainer(const char* pTag)
{
	return __xgeSvgTagNameEquals(pTag, "g") ||
	       __xgeSvgTagNameEquals(pTag, "svg");
}

static int __xgeSvgIsCloseGroupContainer(const char* pTag)
{
	return __xgeSvgIsCloseTagName(pTag, "g") ||
	       __xgeSvgIsCloseTagName(pTag, "svg");
}

static int __xgeSvgTagNameIsClipMaskContainer(const char* pTag)
{
	return __xgeSvgTagNameIsGroupContainer(pTag);
}

static int __xgeSvgTagNameIsShapeElement(const char* pTag)
{
	return __xgeSvgTagNameEquals(pTag, "path") ||
	       __xgeSvgTagNameEquals(pTag, "rect") ||
	       __xgeSvgTagNameEquals(pTag, "circle") ||
	       __xgeSvgTagNameEquals(pTag, "ellipse") ||
	       __xgeSvgTagNameEquals(pTag, "line") ||
	       __xgeSvgTagNameEquals(pTag, "polyline") ||
	       __xgeSvgTagNameEquals(pTag, "polygon");
}

static int __xgeSvgIsCloseShapeElement(const char* pTag)
{
	return __xgeSvgIsCloseTagName(pTag, "path") ||
	       __xgeSvgIsCloseTagName(pTag, "rect") ||
	       __xgeSvgIsCloseTagName(pTag, "circle") ||
	       __xgeSvgIsCloseTagName(pTag, "ellipse") ||
	       __xgeSvgIsCloseTagName(pTag, "line") ||
	       __xgeSvgIsCloseTagName(pTag, "polyline") ||
	       __xgeSvgIsCloseTagName(pTag, "polygon");
}

static int __xgeSvgTagNameIsFilterElement(const char* pTag)
{
	return __xgeSvgTagNameEquals(pTag, "feGaussianBlur");
}

static int __xgeSvgIsCloseFilterElement(const char* pTag)
{
	return __xgeSvgIsCloseTagName(pTag, "feGaussianBlur");
}

static int __xgeSvgTagNameIsElementNodeSupported(const char* pTag)
{
	return __xgeSvgTagNameIsGroupContainer(pTag) ||
	       __xgeSvgTagNameIsShapeElement(pTag) ||
	       __xgeSvgTagNameIsFilterElement(pTag) ||
	       __xgeSvgTagNameEquals(pTag, "defs") ||
	       __xgeSvgTagNameEquals(pTag, "clipPath") ||
	       __xgeSvgTagNameEquals(pTag, "mask") ||
	       __xgeSvgTagNameEquals(pTag, "filter") ||
	       __xgeSvgTagNameEquals(pTag, "linearGradient") ||
	       __xgeSvgTagNameEquals(pTag, "radialGradient") ||
	       __xgeSvgTagNameEquals(pTag, "stop") ||
	       __xgeSvgTagNameEquals(pTag, "pattern") ||
	       __xgeSvgTagNameEquals(pTag, "symbol") ||
	       __xgeSvgTagNameEquals(pTag, "use") ||
	       __xgeSvgTagNameEquals(pTag, "image") ||
	       __xgeSvgTagNameEquals(pTag, "text") ||
	       __xgeSvgTagNameEquals(pTag, "tspan") ||
	       __xgeSvgTagNameEquals(pTag, "textPath");
}

static int __xgeSvgIsCloseElementNodeSupported(const char* pTag)
{
	return __xgeSvgIsCloseGroupContainer(pTag) ||
	       __xgeSvgIsCloseShapeElement(pTag) ||
	       __xgeSvgIsCloseFilterElement(pTag) ||
	       __xgeSvgIsCloseTagName(pTag, "defs") ||
	       __xgeSvgIsCloseTagName(pTag, "clipPath") ||
	       __xgeSvgIsCloseTagName(pTag, "mask") ||
	       __xgeSvgIsCloseTagName(pTag, "filter") ||
	       __xgeSvgIsCloseTagName(pTag, "linearGradient") ||
	       __xgeSvgIsCloseTagName(pTag, "radialGradient") ||
	       __xgeSvgIsCloseTagName(pTag, "stop") ||
	       __xgeSvgIsCloseTagName(pTag, "pattern") ||
	       __xgeSvgIsCloseTagName(pTag, "symbol") ||
	       __xgeSvgIsCloseTagName(pTag, "use") ||
	       __xgeSvgIsCloseTagName(pTag, "image") ||
	       __xgeSvgIsCloseTagName(pTag, "text") ||
	       __xgeSvgIsCloseTagName(pTag, "tspan") ||
	       __xgeSvgIsCloseTagName(pTag, "textPath");
}

static int __xgeSvgIsCloseClipMaskContainer(const char* pTag)
{
	return __xgeSvgIsCloseGroupContainer(pTag);
}

static int __xgeSvgTagNameIsRawContentSkip(const char* pTag)
{
	return __xgeSvgTagNameEquals(pTag, "style");
}

static int __xgeSvgIsSelfClosingTag(const char* pTag, const char* pTagEnd)
{
	const char* p;

	if ( (pTag == NULL) || (pTagEnd == NULL) || (pTagEnd <= pTag) ) {
		return 0;
	}
	p = pTagEnd;
	while ( (p > pTag) && ((p[-1] == ' ') || (p[-1] == '\t') || (p[-1] == '\r') || (p[-1] == '\n')) ) {
		p--;
	}
	return (p > pTag) && (p[-1] == '/');
}

static int __xgeSvgRangeHasNonSpace(const char* pStart, const char* pEnd)
{
	const char* p;

	if ( (pStart == NULL) || (pEnd == NULL) || (pEnd <= pStart) ) {
		return 0;
	}
	for ( p = pStart; p < pEnd; p++ ) {
		if ( (*p != ' ') && (*p != '\t') && (*p != '\r') && (*p != '\n') && (*p != '\f') ) {
			return 1;
		}
	}
	return 0;
}

static int __xgeSvgElementRangeEnd(const char* pTag, const char* pTagEnd, const char* pLimit, const char** ppElementEnd);

static int __xgeSvgElementNodesPreScan(xge_svg pSvg, const char* sText)
{
	const char* p;
	const char* pScan;
	const char* pTextEnd;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (sText == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	__xgeSvgElementNodesClear(pSvg);
	pSvg->iElementStackTop = 0;
	pTextEnd = sText + strlen(sText);
	pScan = sText;
	while ( (p = strchr(pScan, '<')) != NULL ) {
		const char* pTagEnd;
		int bSelfClosing;
		int iNodeIndex;

		if ( (p > pScan) && (pSvg->iElementStackTop > 0) && __xgeSvgRangeHasNonSpace(pScan, p) ) {
			int iParentNode = pSvg->arrElementStack[pSvg->iElementStackTop - 1].iNodeIndex;
			if ( (iParentNode >= 0) && (iParentNode < pSvg->iElementNodeCount) ) {
				pSvg->pElementNodes[iParentNode].bHasTextContent = 1;
			}
		}
		pTagEnd = __xgeSvgFindTagEnd(p);
		if ( pTagEnd == NULL ) {
			break;
		}
		if ( (p[1] == '!') || (p[1] == '?') ) {
			pScan = pTagEnd + 1;
			continue;
		}
		bSelfClosing = __xgeSvgIsSelfClosingTag(p, pTagEnd);
		if ( (p[1] != '/') && __xgeSvgTagNameIsRawContentSkip(p) ) {
			const char* pElementEnd;

			if ( __xgeSvgElementRangeEnd(p, pTagEnd, pTextEnd, &pElementEnd) ) {
				pScan = pElementEnd;
			} else {
				pScan = pTagEnd + 1;
			}
			continue;
		}
		if ( p[1] == '/' ) {
			if ( __xgeSvgIsCloseElementNodeSupported(p) ) {
				__xgeSvgElementStackPop(pSvg, NULL);
			}
			pScan = pTagEnd + 1;
			continue;
		}
		if ( !__xgeSvgTagNameIsElementNodeSupported(p) ) {
			pScan = pTagEnd + 1;
			continue;
		}
		iNodeIndex = -1;
		iRet = __xgeSvgElementNodeEnsure(pSvg, p, pTagEnd, &iNodeIndex);
		if ( iRet != XGE_OK ) {
			pSvg->iElementStackTop = 0;
			pSvg->iCurrentElementNode = -1;
			memset(pSvg->arrLastChildByDepth, 0xff, sizeof(pSvg->arrLastChildByDepth));
			return iRet;
		}
		if ( !bSelfClosing ) {
			iRet = __xgeSvgElementStackPush(pSvg, p, pTagEnd);
			if ( iRet != XGE_OK ) {
				pSvg->iElementStackTop = 0;
				pSvg->iCurrentElementNode = -1;
				memset(pSvg->arrLastChildByDepth, 0xff, sizeof(pSvg->arrLastChildByDepth));
				return iRet;
			}
		}
		pScan = pTagEnd + 1;
	}
	if ( (pScan != NULL) && (pSvg->iElementStackTop > 0) && __xgeSvgRangeHasNonSpace(pScan, pScan + strlen(pScan)) ) {
		int iParentNode = pSvg->arrElementStack[pSvg->iElementStackTop - 1].iNodeIndex;
		if ( (iParentNode >= 0) && (iParentNode < pSvg->iElementNodeCount) ) {
			pSvg->pElementNodes[iParentNode].bHasTextContent = 1;
		}
	}
	pSvg->iElementStackTop = 0;
	pSvg->iCurrentElementNode = -1;
	memset(pSvg->arrLastChildByDepth, 0xff, sizeof(pSvg->arrLastChildByDepth));
	return XGE_OK;
}

static int __xgeSvgCssIsSpace(char c)
{
	return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n') || (c == '\f');
}

static int __xgeSvgCssAsciiLower(int c);
static int __xgeSvgCssRangeEqualsText(const char* pStart, const char* pEnd, const char* sText);

static int __xgeSvgCssRangeCopy(const char* pStart, const char* pEnd, char* sOut, int iOutCapacity)
{
	int iLen;

	if ( (pStart == NULL) || (pEnd == NULL) || (sOut == NULL) || (iOutCapacity <= 0) || (pEnd < pStart) ) {
		return 0;
	}
	__xgeSvgTrimRange(&pStart, &pEnd);
	iLen = (int)(pEnd - pStart);
	if ( (iLen <= 0) || (iLen >= iOutCapacity) ) {
		return 0;
	}
	memcpy(sOut, pStart, (size_t)iLen);
	sOut[iLen] = '\0';
	return 1;
}

static int __xgeSvgCssIdentChar(char c)
{
	return ((c >= 'a') && (c <= 'z')) ||
	       ((c >= 'A') && (c <= 'Z')) ||
	       ((c >= '0') && (c <= '9')) ||
	       (c == '-') || (c == '_');
}

static int __xgeSvgCssHexValue(char c)
{
	if ( (c >= '0') && (c <= '9') ) {
		return c - '0';
	}
	if ( (c >= 'a') && (c <= 'f') ) {
		return c - 'a' + 10;
	}
	if ( (c >= 'A') && (c <= 'F') ) {
		return c - 'A' + 10;
	}
	return -1;
}

static void __xgeSvgCssOutByte(char* sOut, int iOutCapacity, int* pWrite, char c)
{
	if ( (sOut == NULL) || (pWrite == NULL) || (iOutCapacity <= 0) ) {
		return;
	}
	if ( *pWrite < iOutCapacity - 1 ) {
		sOut[*pWrite] = c;
		(*pWrite)++;
	}
}

static void __xgeSvgCssOutUtf8(char* sOut, int iOutCapacity, int* pWrite, uint32_t iCodepoint)
{
	if ( iCodepoint <= 0x7Fu ) {
		__xgeSvgCssOutByte(sOut, iOutCapacity, pWrite, (char)iCodepoint);
	} else if ( iCodepoint <= 0x7FFu ) {
		__xgeSvgCssOutByte(sOut, iOutCapacity, pWrite, (char)(0xC0u | (iCodepoint >> 6)));
		__xgeSvgCssOutByte(sOut, iOutCapacity, pWrite, (char)(0x80u | (iCodepoint & 0x3Fu)));
	} else if ( iCodepoint <= 0xFFFFu ) {
		if ( (iCodepoint >= 0xD800u) && (iCodepoint <= 0xDFFFu) ) {
			return;
		}
		__xgeSvgCssOutByte(sOut, iOutCapacity, pWrite, (char)(0xE0u | (iCodepoint >> 12)));
		__xgeSvgCssOutByte(sOut, iOutCapacity, pWrite, (char)(0x80u | ((iCodepoint >> 6) & 0x3Fu)));
		__xgeSvgCssOutByte(sOut, iOutCapacity, pWrite, (char)(0x80u | (iCodepoint & 0x3Fu)));
	} else if ( iCodepoint <= 0x10FFFFu ) {
		__xgeSvgCssOutByte(sOut, iOutCapacity, pWrite, (char)(0xF0u | (iCodepoint >> 18)));
		__xgeSvgCssOutByte(sOut, iOutCapacity, pWrite, (char)(0x80u | ((iCodepoint >> 12) & 0x3Fu)));
		__xgeSvgCssOutByte(sOut, iOutCapacity, pWrite, (char)(0x80u | ((iCodepoint >> 6) & 0x3Fu)));
		__xgeSvgCssOutByte(sOut, iOutCapacity, pWrite, (char)(0x80u | (iCodepoint & 0x3Fu)));
	}
}

static int __xgeSvgCssEscapeNext(const char** ppText, const char* pEnd, uint32_t* pCodepoint)
{
	const char* p;
	int iHexCount;
	uint32_t iValue;

	if ( (ppText == NULL) || (*ppText == NULL) || (pEnd == NULL) || (pCodepoint == NULL) ||
	     (*ppText >= pEnd) || (**ppText != '\\') ) {
		return 0;
	}
	p = *ppText + 1;
	if ( p >= pEnd ) {
		return 0;
	}
	if ( (*p == '\n') || (*p == '\r') || (*p == '\f') ) {
		return 0;
	}
	iValue = 0;
	iHexCount = 0;
	while ( (p < pEnd) && (iHexCount < 6) ) {
		int iHex = __xgeSvgCssHexValue(*p);
		if ( iHex < 0 ) {
			break;
		}
		iValue = (iValue << 4) | (uint32_t)iHex;
		iHexCount++;
		p++;
	}
	if ( iHexCount > 0 ) {
		if ( (p < pEnd) && __xgeSvgCssIsSpace(*p) ) {
			p++;
		}
		if ( (iValue == 0) || (iValue > 0x10FFFFu) ||
		     ((iValue >= 0xD800u) && (iValue <= 0xDFFFu)) ) {
			iValue = 0xFFFDu;
		}
		*pCodepoint = iValue;
		*ppText = p;
		return 1;
	}
	*pCodepoint = (unsigned char)*p;
	*ppText = p + 1;
	return 1;
}

static int __xgeSvgCssIsSelectorTokenDelimiter(char c)
{
	return (c == '.') || (c == '#') || (c == '[') || (c == ':') ||
	       (c == '>') || (c == '+') || (c == '~') || (c == ']') ||
	       (c == '*') || __xgeSvgCssIsSpace(c);
}

static int __xgeSvgCssIdentifierTokenEnd(const char* pStart, const char* pEnd, const char** ppTokenEnd)
{
	const char* p;
	int bAny;

	if ( (pStart == NULL) || (pEnd == NULL) || (ppTokenEnd == NULL) || (pStart >= pEnd) ) {
		return 0;
	}
	p = pStart;
	bAny = 0;
	while ( p < pEnd ) {
		if ( *p == '\\' ) {
			uint32_t iCodepoint;
			const char* pNext = p;

			if ( !__xgeSvgCssEscapeNext(&pNext, pEnd, &iCodepoint) ) {
				return 0;
			}
			p = pNext;
			bAny = 1;
			continue;
		}
		if ( __xgeSvgCssIsSelectorTokenDelimiter(*p) ) {
			break;
		}
		p++;
		bAny = 1;
	}
	if ( !bAny ) {
		return 0;
	}
	*ppTokenEnd = p;
	return 1;
}

static int __xgeSvgCssIdentifierTokenCopyDecoded(const char* pStart, const char* pEnd, char* sOut, int iOutCapacity)
{
	int iWrite;

	if ( (pStart == NULL) || (pEnd == NULL) || (sOut == NULL) || (iOutCapacity <= 0) || (pEnd <= pStart) ) {
		return 0;
	}
	iWrite = 0;
	while ( pStart < pEnd ) {
		if ( *pStart == '\\' ) {
			uint32_t iCodepoint;
			if ( !__xgeSvgCssEscapeNext(&pStart, pEnd, &iCodepoint) ) {
				return 0;
			}
			__xgeSvgCssOutUtf8(sOut, iOutCapacity, &iWrite, iCodepoint);
			continue;
		}
		__xgeSvgCssOutByte(sOut, iOutCapacity, &iWrite, *pStart);
		pStart++;
	}
	sOut[iWrite < iOutCapacity ? iWrite : (iOutCapacity - 1)] = '\0';
	return iWrite < iOutCapacity;
}

static int __xgeSvgParseStyleText(xge_svg pSvg, const char* pStart, const char* pEnd);
static int __xgeSvgElementRangeEnd(const char* pTag, const char* pTagEnd, const char* pLimit, const char** ppElementEnd);
static int __xgeSvgDecodeBase64(const char* sText, unsigned char** ppData, int* pSize);

static const char* __xgeSvgCssSkipAtRule(const char* pStart, const char* pEnd)
{
	const char* p;
	int iDepth;
	char cQuote;

	if ( (pStart == NULL) || (pEnd == NULL) || (pStart >= pEnd) ) {
		return pEnd;
	}
	p = pStart;
	iDepth = 0;
	cQuote = 0;
	while ( p < pEnd ) {
		if ( cQuote != 0 ) {
			if ( *p == '\\' ) {
				p += (p + 1 < pEnd) ? 2 : 1;
				continue;
			}
			if ( *p == cQuote ) {
				cQuote = 0;
			}
			p++;
			continue;
		}
		if ( (*p == '"') || (*p == '\'') ) {
			cQuote = *p++;
			continue;
		}
		if ( ((p + 1) < pEnd) && (p[0] == '/') && (p[1] == '*') ) {
			p += 2;
			while ( ((p + 1) < pEnd) && !((p[0] == '*') && (p[1] == '/')) ) {
				p++;
			}
			if ( (p + 1) < pEnd ) {
				p += 2;
			}
			continue;
		}
		if ( (*p == ';') && (iDepth == 0) ) {
			return p + 1;
		}
		if ( *p == '{' ) {
			iDepth++;
			p++;
			continue;
		}
		if ( *p == '}' ) {
			if ( iDepth > 0 ) {
				iDepth--;
				p++;
				if ( iDepth == 0 ) {
					return p;
				}
				continue;
			}
			return p + 1;
		}
		p++;
	}
	return pEnd;
}

static int __xgeSvgCssDeclarationRange(const char* pStart, const char* pEnd, const char* sName, const char** ppValueStart, const char** ppValueEnd)
{
	const char* p;
	size_t iNameLength;
	int bFound;

	if ( (pStart == NULL) || (pEnd == NULL) || (sName == NULL) || (ppValueStart == NULL) || (ppValueEnd == NULL) || (pEnd < pStart) ) {
		return 0;
	}
	*ppValueStart = NULL;
	*ppValueEnd = NULL;
	iNameLength = strlen(sName);
	p = pStart;
	bFound = 0;
	while ( p < pEnd ) {
		const char* pNameStart;
		const char* pNameEnd;
		const char* pValueStart;
		const char* pValueEnd;
		int iParenDepth;
		char cQuote;

		while ( (p < pEnd) && ((*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') || (*p == ';')) ) p++;
		if ( p >= pEnd ) break;
		pNameStart = p;
		while ( (p < pEnd) && (*p != ':') && (*p != ';') ) p++;
		if ( (p >= pEnd) || (*p != ':') ) {
			while ( (p < pEnd) && (*p != ';') ) p++;
			continue;
		}
		pNameEnd = p;
		__xgeSvgTrimRange(&pNameStart, &pNameEnd);
		pValueStart = ++p;
		iParenDepth = 0;
		cQuote = 0;
		while ( p < pEnd ) {
			if ( cQuote != 0 ) {
				if ( *p == '\\' ) {
					p += (p + 1 < pEnd) ? 2 : 1;
					continue;
				}
				if ( *p == cQuote ) cQuote = 0;
				p++;
				continue;
			}
			if ( (*p == '\'') || (*p == '"') ) {
				cQuote = *p++;
				continue;
			}
			if ( *p == '(' ) {
				iParenDepth++;
				p++;
				continue;
			}
			if ( (*p == ')') && (iParenDepth > 0) ) {
				iParenDepth--;
				p++;
				continue;
			}
			if ( (*p == ';') && (iParenDepth == 0) ) break;
			p++;
		}
		pValueEnd = p;
		__xgeSvgTrimRange(&pValueStart, &pValueEnd);
		if ( ((size_t)(pNameEnd - pNameStart) == iNameLength) && (memcmp(pNameStart, sName, iNameLength) == 0) ) {
			*ppValueStart = pValueStart;
			*ppValueEnd = pValueEnd;
			bFound = 1;
		}
		if ( (p < pEnd) && (*p == ';') ) p++;
	}
	return bFound;
}

static int __xgeSvgFontFaceSourceDecode(const char* pStart, const char* pEnd, unsigned char** ppData, int* pSize)
{
	const char* pOpen;
	const char* pClose;
	const char* pQuoteStart;
	const char* pQuoteEnd;
	const char* pPayload;
	int iSourceLength;
	int iLimit;
	int i;

	if ( (pStart == NULL) || (pEnd == NULL) || (ppData == NULL) || (pSize == NULL) || (pEnd <= pStart) ) {
		return 0;
	}
	*ppData = NULL;
	*pSize = 0;
	pOpen = pStart;
	while ( (pOpen < pEnd) && (*pOpen != '(') ) pOpen++;
	pClose = pOpen;
	while ( (pClose < pEnd) && (*pClose != ')') ) pClose++;
	if ( (pOpen >= pEnd) || (pClose >= pEnd) || (pOpen >= pClose) ) return 0;
	pQuoteStart = pOpen + 1;
	while ( (pQuoteStart < pClose) && (*pQuoteStart != '\'') ) pQuoteStart++;
	if ( pQuoteStart >= pClose ) return 0;
	pQuoteStart++;
	pQuoteEnd = pQuoteStart;
	while ( (pQuoteEnd < pClose) && (*pQuoteEnd != '\'') ) pQuoteEnd++;
	if ( pQuoteEnd <= pQuoteStart ) return 0;
	while ( (pQuoteStart < pQuoteEnd) && ((*pQuoteStart == ' ') || (*pQuoteStart == '\t') || (*pQuoteStart == '\r') || (*pQuoteStart == '\n')) ) pQuoteStart++;
	while ( (pQuoteEnd > pQuoteStart) && ((pQuoteEnd[-1] == ' ') || (pQuoteEnd[-1] == '\t') || (pQuoteEnd[-1] == '\r') || (pQuoteEnd[-1] == '\n')) ) pQuoteEnd--;
	iSourceLength = (int)(pQuoteEnd - pQuoteStart);
	iLimit = iSourceLength < 40 ? iSourceLength : 40;
	pPayload = NULL;
	for ( i = 0; i + 10 <= iLimit; i++ ) {
		if ( (memcmp(pQuoteStart + i, "ttf;base64", 10) == 0) ||
		     (memcmp(pQuoteStart + i, "otf;base64", 10) == 0) ) {
			if ( (i + 10) < iSourceLength ) pPayload = pQuoteStart + i + 11;
			break;
		}
	}
	if ( (pPayload == NULL) || (pPayload >= pQuoteEnd) ) return 0;
	{
		char* sBase64;
		int iRet;

		iRet = __xgeSvgRangeDup(pPayload, pQuoteEnd, &sBase64);
		if ( iRet != XGE_OK ) return iRet;
		iRet = __xgeSvgDecodeBase64(sBase64, ppData, pSize);
		xrtFree(sBase64);
		return iRet == XGE_OK ? 1 : 0;
	}
}

static int __xgeSvgParseFontFaceRule(xge_svg pSvg, const char* pStart, const char* pEnd)
{
	const char* pFamilyStart;
	const char* pFamilyEnd;
	const char* pSourceStart;
	const char* pSourceEnd;
	char* sFamily;
	unsigned char* pData;
	int iSize;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pStart == NULL) || (pEnd == NULL) || (pEnd < pStart) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( !__xgeSvgCssDeclarationRange(pStart, pEnd, "font-family", &pFamilyStart, &pFamilyEnd) ||
	     !__xgeSvgCssDeclarationRange(pStart, pEnd, "src", &pSourceStart, &pSourceEnd) ) {
		return XGE_OK;
	}
	if ( ((pFamilyEnd - pFamilyStart) >= 2) && (pFamilyStart[0] == '\'') && (pFamilyEnd[-1] == '\'') ) {
		pFamilyStart++;
		pFamilyEnd--;
	}
	if ( pFamilyEnd <= pFamilyStart ) return XGE_OK;
	sFamily = NULL;
	pData = NULL;
	iSize = 0;
	iRet = __xgeSvgRangeDup(pFamilyStart, pFamilyEnd, &sFamily);
	if ( iRet != XGE_OK ) return iRet;
	iRet = __xgeSvgFontFaceSourceDecode(pSourceStart, pSourceEnd, &pData, &iSize);
	if ( iRet <= 0 ) {
		xrtFree(sFamily);
		xrtFree(pData);
		return iRet < 0 ? iRet : XGE_OK;
	}
	iRet = __xgeSvgFontFaceAdd(pSvg, sFamily, pData, iSize);
	if ( iRet != XGE_OK ) {
		xrtFree(sFamily);
		xrtFree(pData);
	}
	return iRet;
}

static int __xgeSvgCssRangeEqualsLiteral(const char* pStart, const char* pEnd, const char* sText)
{
	size_t iLen;

	if ( (pStart == NULL) || (pEnd == NULL) || (sText == NULL) || (pEnd < pStart) ) {
		return 0;
	}
	iLen = strlen(sText);
	return ((size_t)(pEnd - pStart) == iLen) && (strncmp(pStart, sText, iLen) == 0);
}

static int __xgeSvgCssThorvgTagSupported(const char* pStart, const char* pEnd)
{
	__xgeSvgTrimRange(&pStart, &pEnd);
	return __xgeSvgCssRangeEqualsLiteral(pStart, pEnd, "defs") ||
	       __xgeSvgCssRangeEqualsLiteral(pStart, pEnd, "g") ||
	       __xgeSvgCssRangeEqualsLiteral(pStart, pEnd, "svg") ||
	       __xgeSvgCssRangeEqualsLiteral(pStart, pEnd, "mask") ||
	       __xgeSvgCssRangeEqualsLiteral(pStart, pEnd, "clipPath") ||
	       __xgeSvgCssRangeEqualsLiteral(pStart, pEnd, "style") ||
	       __xgeSvgCssRangeEqualsLiteral(pStart, pEnd, "symbol") ||
	       __xgeSvgCssRangeEqualsLiteral(pStart, pEnd, "filter") ||
	       __xgeSvgCssRangeEqualsLiteral(pStart, pEnd, "use") ||
	       __xgeSvgCssRangeEqualsLiteral(pStart, pEnd, "circle") ||
	       __xgeSvgCssRangeEqualsLiteral(pStart, pEnd, "ellipse") ||
	       __xgeSvgCssRangeEqualsLiteral(pStart, pEnd, "path") ||
	       __xgeSvgCssRangeEqualsLiteral(pStart, pEnd, "polygon") ||
	       __xgeSvgCssRangeEqualsLiteral(pStart, pEnd, "rect") ||
	       __xgeSvgCssRangeEqualsLiteral(pStart, pEnd, "polyline") ||
	       __xgeSvgCssRangeEqualsLiteral(pStart, pEnd, "line") ||
	       __xgeSvgCssRangeEqualsLiteral(pStart, pEnd, "image") ||
	       __xgeSvgCssRangeEqualsLiteral(pStart, pEnd, "text") ||
	       __xgeSvgCssRangeEqualsLiteral(pStart, pEnd, "feGaussianBlur");
}

static int __xgeSvgParseStyleClassSelectorList(xge_svg pSvg, const char* pStart, const char* pEnd, const char* pStyleStart, const char* pStyleEnd)
{
	const char* p;
	const char* pItemStart;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pStart == NULL) || (pEnd == NULL) || (pStyleStart == NULL) || (pStyleEnd == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	p = pStart;
	pItemStart = pStart;
	while ( p <= pEnd ) {
		if ( (p == pEnd) || (*p == ',') ) {
			const char* pItemEnd = p;

			__xgeSvgTrimRange(&pItemStart, &pItemEnd);
			if ( (pItemStart < pItemEnd) && (*pItemStart == '.') ) {
				pItemStart++;
				__xgeSvgTrimRange(&pItemStart, &pItemEnd);
			}
			if ( pItemEnd > pItemStart ) {
				iRet = __xgeSvgStyleRuleAdd(pSvg, XGE_SVG_STYLE_SELECTOR_CLASS, pItemStart, pItemEnd, pStyleStart, pStyleEnd);
				if ( iRet != XGE_OK ) {
					return iRet;
				}
			}
			pItemStart = p + 1;
		}
		p++;
	}
	return XGE_OK;
}

static int __xgeSvgParseStyleSelector(xge_svg pSvg, const char* pStart, const char* pEnd, const char* pStyleStart, const char* pStyleEnd)
{
	const char* pDot;
	char sName[XGE_SVG_ATTR_MAX];
	int iTagLen;
	int iClassLen;

	__xgeSvgTrimRange(&pStart, &pEnd);
	if ( pEnd <= pStart ) {
		return XGE_OK;
	}
	if ( pEnd[-1] == '.' ) {
		return XGE_OK;
	}
	pDot = pStart;
	while ( (pDot < pEnd) && (*pDot != '.') ) {
		pDot++;
	}
	if ( pDot == pStart ) {
		return __xgeSvgParseStyleClassSelectorList(pSvg, pStart + 1, pEnd, pStyleStart, pStyleEnd);
	}
	if ( pDot >= pEnd ) {
		if ( !__xgeSvgCssThorvgTagSupported(pStart, pEnd) ) {
			return XGE_OK;
		}
		return __xgeSvgStyleRuleAdd(pSvg, XGE_SVG_STYLE_SELECTOR_TAG, pStart, pEnd, pStyleStart, pStyleEnd);
	}
	if ( !__xgeSvgCssThorvgTagSupported(pStart, pDot) ) {
		return XGE_OK;
	}
	iTagLen = (int)(pDot - pStart);
	iClassLen = (int)(pEnd - pDot - 1);
	if ( (iTagLen <= 0) || (iClassLen <= 0) || ((iTagLen + 1 + iClassLen) >= (int)sizeof(sName)) ) {
		return XGE_OK;
	}
	memcpy(sName, pStart, (size_t)iTagLen);
	sName[iTagLen] = '.';
	memcpy(sName + iTagLen + 1, pDot + 1, (size_t)iClassLen);
	sName[iTagLen + 1 + iClassLen] = '\0';
	return __xgeSvgStyleRuleAdd(pSvg, XGE_SVG_STYLE_SELECTOR_TAG_CLASS, sName, sName + strlen(sName), pStyleStart, pStyleEnd);
}

static int __xgeSvgParseStyleSelectorList(xge_svg pSvg, const char* pStart, const char* pEnd, const char* pStyleStart, const char* pStyleEnd)
{
	if ( !__xgeSvgValid(pSvg) || (pStart == NULL) || (pEnd == NULL) || (pStyleStart == NULL) || (pStyleEnd == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	return __xgeSvgParseStyleSelector(pSvg, pStart, pEnd, pStyleStart, pStyleEnd);
}

static int __xgeSvgParseStyleText(xge_svg pSvg, const char* pStart, const char* pEnd)
{
	const char* p;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pStart == NULL) || (pEnd == NULL) || (pEnd < pStart) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	p = pStart;
	while ( p < pEnd ) {
		const char* pSelectorStart;
		const char* pSelectorEnd;
		const char* pStyleStart;
		const char* pStyleEnd;

		while ( p < pEnd ) {
			if ( (p + 1 < pEnd) && (p[0] == '/') && (p[1] == '*') ) {
				p += 2;
				while ( (p + 1 < pEnd) && !((p[0] == '*') && (p[1] == '/')) ) {
					p++;
				}
				if ( p + 1 < pEnd ) {
					p += 2;
				}
				continue;
			}
			if ( __xgeSvgRangeStartsWith(p, pEnd, "<!--") ) {
				p += 4;
				continue;
			}
			if ( __xgeSvgRangeStartsWith(p, pEnd, "-->") ) {
				p += 3;
				continue;
			}
			if ( (*p != ' ') && (*p != '\t') && (*p != '\r') && (*p != '\n') ) {
				break;
			}
			p++;
		}
		if ( (p < pEnd) && (*p == '@') ) {
			const char* pAtRuleStart = p;
			const char* pAtRuleEnd = __xgeSvgCssSkipAtRule(p, pEnd);
			const char* pRuleNameEnd = pAtRuleStart + strlen("@font-face");

			if ( (pRuleNameEnd <= pEnd) && __xgeSvgRangeStartsWith(pAtRuleStart, pEnd, "@font-face") &&
			     ((pRuleNameEnd == pEnd) || (*pRuleNameEnd == ' ') || (*pRuleNameEnd == '\t') ||
			      (*pRuleNameEnd == '\r') || (*pRuleNameEnd == '\n') || (*pRuleNameEnd == '{')) ) {
				const char* pOpen = pRuleNameEnd;
				const char* pClose = pAtRuleEnd;

				while ( (pOpen < pAtRuleEnd) && (*pOpen != '{') ) pOpen++;
				if ( (pClose > pOpen) && (pClose[-1] == '}') ) pClose--;
				if ( (pOpen < pClose) && (*pOpen == '{') ) {
					iRet = __xgeSvgParseFontFaceRule(pSvg, pOpen + 1, pClose);
					if ( iRet != XGE_OK ) return iRet;
				}
			}
			p = pAtRuleEnd;
			continue;
		}
		pSelectorStart = p;
		while ( (p < pEnd) && (*p != '{') ) {
			p++;
		}
		if ( p >= pEnd ) {
			break;
		}
		pSelectorEnd = p;
		p++;
		pStyleStart = p;
		while ( (p < pEnd) && (*p != '}') ) {
			p++;
		}
		pStyleEnd = p;
		iRet = __xgeSvgParseStyleSelectorList(pSvg, pSelectorStart, pSelectorEnd, pStyleStart, pStyleEnd);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		if ( p < pEnd ) {
			p++;
		}
	}
	return XGE_OK;
}

static int __xgeSvgParseStyleBlocks(xge_svg pSvg, const char* sText)
{
	const char* p;
	int bParsedStyle;

	if ( !__xgeSvgValid(pSvg) || (sText == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	p = sText;
	bParsedStyle = 0;
	while ( (p = strchr(p, '<')) != NULL ) {
		const char* pTagEnd = __xgeSvgFindTagEnd(p);

		if ( pTagEnd == NULL ) {
			break;
		}
		if ( !__xgeSvgTagNameEquals(p, "style") ) {
			p = pTagEnd + 1;
			continue;
		}
		{
			const char* pClose = strstr(pTagEnd + 1, "</style");
			const char* pCloseEnd;
			const char* pStyleStart = pTagEnd + 1;
			const char* pStyleEnd;
			int iRet;

			if ( pClose == NULL ) {
				break;
			}
			pCloseEnd = __xgeSvgFindTagEnd(pClose);
			if ( pCloseEnd == NULL ) {
				break;
			}
			if ( bParsedStyle ) {
				p = pCloseEnd + 1;
				continue;
			}
			pStyleEnd = pClose;

			__xgeSvgTrimRange(&pStyleStart, &pStyleEnd);
			if ( __xgeSvgRangeStartsWith(pStyleStart, pStyleEnd, "<![CDATA[") ) {
				const char* pCDataEnd;

				pStyleStart += 9;
				pCDataEnd = strstr(pStyleStart, "]]>");
				if ( (pCDataEnd != NULL) && (pCDataEnd <= pStyleEnd) ) {
					pStyleEnd = pCDataEnd;
				}
			}
			__xgeSvgTrimRange(&pStyleStart, &pStyleEnd);
			if ( __xgeSvgRangeStartsWith(pStyleStart, pStyleEnd, "<!--") ) {
				pStyleStart += 4;
			}
			if ( ((pStyleEnd - pStyleStart) >= 3) && (strncmp(pStyleEnd - 3, "-->", 3) == 0) ) {
				pStyleEnd -= 3;
			}
			iRet = __xgeSvgParseStyleText(pSvg, pStyleStart, pStyleEnd);
			if ( iRet != XGE_OK ) {
				return iRet;
			}
			bParsedStyle = 1;
			p = pCloseEnd + 1;
		}
	}
	return XGE_OK;
}

static void __xgeSvgAttrOutByte(char* sOut, int iOutCapacity, int* pWrite, char c)
{
	if ( (sOut == NULL) || (pWrite == NULL) || (iOutCapacity <= 0) ) {
		return;
	}
	if ( *pWrite < (iOutCapacity - 1) ) {
		sOut[*pWrite] = c;
	}
	(*pWrite)++;
}

static int __xgeSvgThorvgEntityLength(const char* pValue, const char* pValueEnd)
{
	static const char* arrEntities[] = {
		"&#10;", "&quot;", "&nbsp;", "&apos;", "&amp;", "&lt;", "&gt;", "&#035;", "&#039;"
	};
	int i;

	if ( (pValue == NULL) || (pValueEnd == NULL) || (pValue >= pValueEnd) || (*pValue != '&') ) {
		return 0;
	}
	for ( i = 0; i < (int)(sizeof(arrEntities) / sizeof(arrEntities[0])); i++ ) {
		int iLength = (int)strlen(arrEntities[i]);

		if ( ((pValueEnd - pValue) >= iLength) && (strncmp(pValue, arrEntities[i], (size_t)iLength) == 0) ) {
			return iLength;
		}
	}
	return 0;
}

static void __xgeSvgAttrCopyThorvg(const char* pValue, const char* pValueEnd, char* sOut, int iOutCapacity)
{
	int iWrite;

	if ( (sOut == NULL) || (iOutCapacity <= 0) ) {
		return;
	}
	iWrite = 0;
	while ( (pValue != NULL) && (pValue < pValueEnd) ) {
		int iEntityLength = __xgeSvgThorvgEntityLength(pValue, pValueEnd);

		if ( iEntityLength > 0 ) {
			pValue += iEntityLength;
			continue;
		}
		__xgeSvgAttrOutByte(sOut, iOutCapacity, &iWrite, *pValue);
		pValue++;
	}
	sOut[iWrite < iOutCapacity ? iWrite : (iOutCapacity - 1)] = '\0';
}

static int __xgeSvgAttrCopy(const char* pTag, const char* pTagEnd, const char* sName, char* sOut, int iOutCapacity)
{
	const char* p;
	size_t iNameLen;

	if ( (pTag == NULL) || (pTagEnd == NULL) || (sName == NULL) || (sOut == NULL) || (iOutCapacity <= 0) ) {
		return 0;
	}
	sOut[0] = '\0';
	iNameLen = strlen(sName);
	p = pTag;
	while ( p < pTagEnd ) {
		const char* pValue;
		const char* pValueEnd;
		char cQuote;

		if ( ((p == pTag) || __xgeSvgIsNameEnd(*(p - 1)) || (*(p - 1) == '<')) &&
		     ((size_t)(pTagEnd - p) > iNameLen) &&
		     (strncmp(p, sName, iNameLen) == 0) ) {
			const char* q = p + iNameLen;
			while ( (q < pTagEnd) && ((*q == ' ') || (*q == '\t') || (*q == '\r') || (*q == '\n')) ) q++;
			if ( (q >= pTagEnd) || (*q != '=') ) {
				p++;
				continue;
			}
			q++;
			while ( (q < pTagEnd) && ((*q == ' ') || (*q == '\t') || (*q == '\r') || (*q == '\n')) ) q++;
			if ( q >= pTagEnd ) {
				return 0;
			}
			cQuote = 0;
			if ( (*q == '"') || (*q == '\'') ) {
				cQuote = *q++;
			}
			pValue = q;
			if ( cQuote != 0 ) {
				while ( (q < pTagEnd) && (*q != cQuote) ) q++;
				pValueEnd = q;
			} else {
				while ( (q < pTagEnd) && !__xgeSvgIsNameEnd(*q) ) q++;
				pValueEnd = q;
			}
			__xgeSvgAttrCopyThorvg(pValue, pValueEnd, sOut, iOutCapacity);
			return 1;
		}
		p++;
	}
	return 0;
}

static int __xgeSvgAttrDup(const char* pTag, const char* pTagEnd, const char* sName, char** ppOut)
{
	char* sOut;
	size_t iTagLength;

	if ( (pTag == NULL) || (pTagEnd == NULL) || (pTagEnd < pTag) ||
	     (sName == NULL) || (ppOut == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppOut = NULL;
	iTagLength = (size_t)(pTagEnd - pTag);
	if ( iTagLength >= (size_t)INT_MAX ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	sOut = (char*)xrtMalloc(iTagLength + 1u);
	if ( sOut == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( !__xgeSvgAttrCopy(pTag, pTagEnd, sName, sOut, (int)iTagLength + 1) ) {
		xrtFree(sOut);
		return XGE_ERROR_NOT_FOUND;
	}
	*ppOut = sOut;
	return XGE_OK;
}

static int __xgeSvgPathShapeCreate(const char* pTag, const char* pTagEnd, xge_shape_ex* ppShape)
{
	char* sPath;
	xge_shape_ex pShape;
	int iRet;

	if ( ppShape == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppShape = NULL;
	sPath = NULL;
	pShape = NULL;
	iRet = __xgeSvgAttrDup(pTag, pTagEnd, "d", &sPath);
	if ( iRet == XGE_ERROR_NOT_FOUND ) {
		return XGE_OK;
	}
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = xgeShapeExCreate(&pShape);
	if ( iRet == XGE_OK ) {
		iRet = xgeShapeExAppendSvgPath(pShape, sPath);
	}
	xrtFree(sPath);
	if ( iRet != XGE_OK ) {
		if ( pShape != NULL ) {
			xgeShapeExDestroy(pShape);
		}
		return iRet == XGE_ERROR_INVALID_ARGUMENT ? XGE_OK : iRet;
	}
	*ppShape = pShape;
	return XGE_OK;
}

static int __xgeSvgAttrNext(const char** ppCursor, const char* pTagEnd, char* sName, int iNameCapacity, char* sValue, int iValueCapacity)
{
	const char* p;
	const char* pNameStart;
	const char* pNameEnd;
	const char* pValueStart;
	const char* pValueEnd;
	char cQuote;
	int iNameLen;

	if ( (ppCursor == NULL) || (*ppCursor == NULL) || (pTagEnd == NULL) ||
	     (sName == NULL) || (iNameCapacity <= 0) || (sValue == NULL) || (iValueCapacity <= 0) ) {
		return 0;
	}
	sName[0] = '\0';
	sValue[0] = '\0';
	p = *ppCursor;
	if ( p >= pTagEnd ) {
		return 0;
	}
	if ( *p == '<' ) {
		p++;
		if ( (p < pTagEnd) && (*p == '/') ) {
			p++;
		}
		while ( (p < pTagEnd) && !__xgeSvgCssIsSpace(*p) && (*p != '/') && (*p != '>') ) {
			p++;
		}
	}
	for ( ;; ) {
		while ( (p < pTagEnd) && __xgeSvgCssIsSpace(*p) ) {
			p++;
		}
		if ( (p >= pTagEnd) || (*p == '/') || (*p == '>') ) {
			*ppCursor = p;
			return 0;
		}
		pNameStart = p;
		while ( (p < pTagEnd) && !__xgeSvgCssIsSpace(*p) && (*p != '=') && (*p != '/') && (*p != '>') ) {
			p++;
		}
		pNameEnd = p;
		while ( (p < pTagEnd) && __xgeSvgCssIsSpace(*p) ) {
			p++;
		}
		if ( (p >= pTagEnd) || (*p != '=') || (pNameEnd <= pNameStart) ) {
			while ( (p < pTagEnd) && !__xgeSvgCssIsSpace(*p) && (*p != '>') ) {
				p++;
			}
			continue;
		}
		p++;
		while ( (p < pTagEnd) && __xgeSvgCssIsSpace(*p) ) {
			p++;
		}
		if ( p >= pTagEnd ) {
			*ppCursor = p;
			return 0;
		}
		cQuote = 0;
		if ( (*p == '"') || (*p == '\'') ) {
			cQuote = *p++;
		}
		pValueStart = p;
		if ( cQuote != 0 ) {
			while ( (p < pTagEnd) && (*p != cQuote) ) {
				p++;
			}
			pValueEnd = p;
			if ( p < pTagEnd ) {
				p++;
			}
		} else {
			while ( (p < pTagEnd) && !__xgeSvgCssIsSpace(*p) && (*p != '>') ) {
				p++;
			}
			pValueEnd = p;
		}
		iNameLen = (int)(pNameEnd - pNameStart);
		if ( iNameLen >= iNameCapacity ) {
			iNameLen = iNameCapacity - 1;
		}
		memcpy(sName, pNameStart, (size_t)iNameLen);
		sName[iNameLen] = '\0';
		__xgeSvgAttrCopyThorvg(pValueStart, pValueEnd, sValue, iValueCapacity);
		*ppCursor = p;
		return 1;
	}
}

static int __xgeSvgAttrCopyLast(
	const char* pTag,
	const char* pTagEnd,
	const char* sTargetName,
	char* sOut,
	int iOutCapacity
)
{
	char sName[XGE_SVG_ATTR_MAX];
	char sValue[XGE_SVG_ATTR_MAX];
	const char* pCursor;
	int bFound;

	if ( (pTag == NULL) || (pTagEnd == NULL) || (sTargetName == NULL) ||
	     (sOut == NULL) || (iOutCapacity <= 0) ) {
		return 0;
	}
	sOut[0] = '\0';
	pCursor = pTag;
	bFound = 0;
	while ( __xgeSvgAttrNext(&pCursor, pTagEnd, sName, sizeof(sName), sValue, sizeof(sValue)) ) {
		if ( strcmp(sName, sTargetName) == 0 ) {
			__xgeSvgAttrCopyThorvg(sValue, sValue + strlen(sValue), sOut, iOutCapacity);
			bFound = 1;
		}
	}
	return bFound;
}

static int __xgeSvgAttrHasExactValue(
	const char* pTag,
	const char* pTagEnd,
	const char* sTargetName,
	const char* sTargetValue
)
{
	char sName[XGE_SVG_ATTR_MAX];
	char sValue[XGE_SVG_ATTR_MAX];
	const char* pCursor;

	if ( (pTag == NULL) || (pTagEnd == NULL) || (sTargetName == NULL) || (sTargetValue == NULL) ) {
		return 0;
	}
	pCursor = pTag;
	while ( __xgeSvgAttrNext(&pCursor, pTagEnd, sName, sizeof(sName), sValue, sizeof(sValue)) ) {
		if ( (strcmp(sName, sTargetName) == 0) && (strcmp(sValue, sTargetValue) == 0) ) {
			return 1;
		}
	}
	return 0;
}

static int __xgeSvgHrefSpaceLen(const char* pText)
{
	if ( pText == NULL ) {
		return 0;
	}
	if ( (*pText == ' ') || (*pText == '\t') || (*pText == '\r') || (*pText == '\n') || (*pText == '\f') ) {
		return 1;
	}
	if ( ((unsigned char)pText[0] == 0xC2u) && ((unsigned char)pText[1] == 0xA0u) ) {
		return 2;
	}
	return 0;
}

static int __xgeSvgHrefCopyTrimmed(const char* pTag, const char* pTagEnd, char* sOut, int iOutCapacity)
{
	char sValue[XGE_SVG_ATTR_MAX];
	const char* pStart;
	const char* pEnd;
	char cQuote;
	int iLen;

	if ( (sOut == NULL) || (iOutCapacity <= 0) ) {
		return 0;
	}
	sOut[0] = '\0';
	if ( !__xgeSvgAttrCopy(pTag, pTagEnd, "href", sValue, sizeof(sValue)) &&
	     !__xgeSvgAttrCopy(pTag, pTagEnd, "xlink:href", sValue, sizeof(sValue)) ) {
		return 0;
	}
	pStart = sValue;
	while ( __xgeSvgHrefSpaceLen(pStart) > 0 ) {
		pStart += __xgeSvgHrefSpaceLen(pStart);
	}
	pEnd = sValue + strlen(sValue);
	while ( pEnd > pStart ) {
		if ( __xgeSvgHrefSpaceLen(pEnd - 1) == 1 ) {
			pEnd--;
		} else if ( ((pEnd - pStart) >= 2) &&
		            ((unsigned char)pEnd[-2] == 0xC2u) && ((unsigned char)pEnd[-1] == 0xA0u) ) {
			pEnd -= 2;
		} else {
			break;
		}
	}
	if ( ((pEnd - pStart) >= 5) &&
	     (__xgeSvgCssAsciiLower((unsigned char)pStart[0]) == 'u') &&
	     (__xgeSvgCssAsciiLower((unsigned char)pStart[1]) == 'r') &&
	     (__xgeSvgCssAsciiLower((unsigned char)pStart[2]) == 'l') &&
	     (pStart[3] == '(') ) {
		pStart += 4;
		while ( __xgeSvgHrefSpaceLen(pStart) > 0 ) {
			pStart += __xgeSvgHrefSpaceLen(pStart);
		}
		while ( (pEnd > pStart) && __xgeSvgHrefSpaceLen(pEnd - 1) == 1 ) {
			pEnd--;
		}
		if ( (pEnd > pStart) && (pEnd[-1] == ')') ) {
			pEnd--;
		}
		while ( (pEnd > pStart) && __xgeSvgHrefSpaceLen(pEnd - 1) == 1 ) {
			pEnd--;
		}
		cQuote = 0;
		if ( (pStart < pEnd) && ((*pStart == '"') || (*pStart == '\'')) ) {
			cQuote = *pStart++;
		}
		if ( (cQuote != 0) && (pEnd > pStart) && (pEnd[-1] == cQuote) ) {
			pEnd--;
		}
	}
	iLen = (int)(pEnd - pStart);
	if ( iLen <= 0 ) {
		return 0;
	}
	if ( iLen >= iOutCapacity ) {
		iLen = iOutCapacity - 1;
	}
	memcpy(sOut, pStart, (size_t)iLen);
	sOut[iLen] = '\0';
	return 1;
}

static int __xgeSvgHrefIsLocalRef(const char* sHref)
{
	return (sHref != NULL) && (sHref[0] == '#');
}

static const char* __xgeSvgHrefFragment(const char* sHref)
{
	const char* p;

	if ( sHref == NULL ) {
		return NULL;
	}
	for ( p = sHref; *p != '\0'; p++ ) {
		if ( (*p == '#') && ((p == sHref) || (p[-1] != '&')) ) {
			return p;
		}
	}
	return NULL;
}

static int __xgeSvgHrefHasFragment(const char* sHref)
{
	return __xgeSvgHrefFragment(sHref) != NULL;
}

static int __xgeSvgHrefIdCopyFromString(const char* sHref, char* sOut, int iOutCapacity);

static int __xgeSvgUseHrefIdCopy(const char* pTag, const char* pTagEnd, char* sOut, int iOutCapacity)
{
	char sValue[XGE_SVG_ATTR_MAX];
	const char* pValue;
	int iLen;

	if ( (sOut == NULL) || (iOutCapacity <= 0) ) {
		return 0;
	}
	sOut[0] = '\0';
	if ( !__xgeSvgAttrCopy(pTag, pTagEnd, "href", sValue, sizeof(sValue)) &&
	     !__xgeSvgAttrCopy(pTag, pTagEnd, "xlink:href", sValue, sizeof(sValue)) ) {
		return 0;
	}
	pValue = sValue;
	while ( __xgeSvgHrefSpaceLen(pValue) > 0 ) {
		pValue += __xgeSvgHrefSpaceLen(pValue);
	}
	if ( *pValue == '#' ) {
		pValue++;
	}
	iLen = (int)strlen(pValue);
	if ( iLen <= 0 ) {
		return 0;
	}
	if ( iLen >= iOutCapacity ) {
		iLen = iOutCapacity - 1;
	}
	memcpy(sOut, pValue, (size_t)iLen);
	sOut[iLen] = '\0';
	return 1;
}

static int __xgeSvgHrefIdCopy(const char* pTag, const char* pTagEnd, char* sOut, int iOutCapacity)
{
	char sValue[XGE_SVG_ATTR_MAX];

	if ( (sOut == NULL) || (iOutCapacity <= 0) ) {
		return 0;
	}
	sOut[0] = '\0';
	if ( !__xgeSvgHrefCopyTrimmed(pTag, pTagEnd, sValue, sizeof(sValue)) ) {
		return 0;
	}
	return __xgeSvgHrefIdCopyFromString(sValue, sOut, iOutCapacity);
}

static int __xgeSvgHrefIdCopyFromString(const char* sHref, char* sOut, int iOutCapacity)
{
	const char* pValue;
	int iLen;

	if ( (sOut == NULL) || (iOutCapacity <= 0) ) {
		return 0;
	}
	sOut[0] = '\0';
	if ( sHref == NULL ) {
		return 0;
	}
	pValue = sHref;
	if ( pValue[0] == '#' ) {
		pValue++;
	}
	iLen = 0;
	while ( (pValue[iLen] != '\0') && (pValue[iLen] != ')') &&
	        (pValue[iLen] != ' ') && (pValue[iLen] != '\t') &&
	        (pValue[iLen] != '\r') && (pValue[iLen] != '\n') &&
	        !(((unsigned char)pValue[iLen] == 0xC2u) && ((unsigned char)pValue[iLen + 1] == 0xA0u)) ) {
		iLen++;
	}
	if ( iLen <= 0 ) {
		return 0;
	}
	if ( iLen >= iOutCapacity ) {
		iLen = iOutCapacity - 1;
	}
	memcpy(sOut, pValue, (size_t)iLen);
	sOut[iLen] = '\0';
	return 1;
}

static int __xgeSvgCssAsciiLower(int c)
{
	if ( (c >= 'A') && (c <= 'Z') ) {
		return c + ('a' - 'A');
	}
	return c;
}

static void __xgeSvgCssAsciiLowerString(char* sText)
{
	if ( sText == NULL ) {
		return;
	}
	while ( *sText != '\0' ) {
		*sText = (char)__xgeSvgCssAsciiLower((unsigned char)*sText);
		sText++;
	}
}

static int __xgeSvgCssRangeEqualsText(const char* pStart, const char* pEnd, const char* sText)
{
	if ( (pStart == NULL) || (pEnd == NULL) || (sText == NULL) || (pEnd < pStart) ) {
		return 0;
	}
	while ( (pStart < pEnd) && (*sText != '\0') ) {
		if ( __xgeSvgCssAsciiLower((unsigned char)*pStart) != __xgeSvgCssAsciiLower((unsigned char)*sText) ) {
			return 0;
		}
		pStart++;
		sText++;
	}
	return (pStart == pEnd) && (*sText == '\0');
}

static int __xgeSvgRangeEqualsTextExact(const char* pStart, const char* pEnd, const char* sText)
{
	size_t iLen;

	if ( (pStart == NULL) || (pEnd == NULL) || (sText == NULL) || (pEnd < pStart) ) {
		return 0;
	}
	iLen = strlen(sText);
	return ((size_t)(pEnd - pStart) == iLen) && (memcmp(pStart, sText, iLen) == 0);
}

static int __xgeSvgStyleValueCleanCopy(const char* pValueStart, const char* pValueEnd, char* sOut, int iOutCapacity, int* pImportant)
{
	char sTemp[XGE_SVG_ATTR_MAX];
	int iLen;
	int iEnd;
	int iKeywordStart;
	int iBang;
	int iValueEnd;
	const char* p;

	if ( (pValueStart == NULL) || (pValueEnd == NULL) || (sOut == NULL) || (iOutCapacity <= 0) || (pImportant == NULL) ) {
		return 0;
	}
	iLen = 0;
	p = pValueStart;
	while ( p < pValueEnd ) {
		if ( ((p + 1) < pValueEnd) && (p[0] == '/') && (p[1] == '*') ) {
			p += 2;
			while ( ((p + 1) < pValueEnd) && !((p[0] == '*') && (p[1] == '/')) ) p++;
			if ( (p + 1) < pValueEnd ) p += 2;
			if ( (iLen > 0) && (iLen < (int)sizeof(sTemp) - 1) && (sTemp[iLen - 1] != ' ') ) {
				sTemp[iLen++] = ' ';
			}
			continue;
		}
		if ( (iLen == 0) && ((*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n')) ) {
			p++;
			continue;
		}
		if ( iLen < (int)sizeof(sTemp) - 1 ) {
			sTemp[iLen++] = *p;
		}
		p++;
	}
	while ( (iLen > 0) && ((sTemp[iLen - 1] == ' ') || (sTemp[iLen - 1] == '\t') || (sTemp[iLen - 1] == '\r') || (sTemp[iLen - 1] == '\n')) ) {
		iLen--;
	}
	sTemp[iLen] = '\0';
	*pImportant = 0;
	iEnd = iLen;
	while ( (iEnd > 0) && ((sTemp[iEnd - 1] == ' ') || (sTemp[iEnd - 1] == '\t') || (sTemp[iEnd - 1] == '\r') || (sTemp[iEnd - 1] == '\n')) ) {
		iEnd--;
	}
	iKeywordStart = iEnd - 9;
	if ( (iKeywordStart >= 0) && __xgeSvgRangeEqualsTextExact(sTemp + iKeywordStart, sTemp + iEnd, "important") ) {
		iBang = iKeywordStart;
		while ( (iBang > 0) && ((sTemp[iBang - 1] == ' ') || (sTemp[iBang - 1] == '\t') || (sTemp[iBang - 1] == '\r') || (sTemp[iBang - 1] == '\n')) ) {
			iBang--;
		}
		if ( (iBang > 0) && (sTemp[iBang - 1] == '!') ) {
			*pImportant = 1;
			iLen = iBang - 1;
			while ( (iLen > 0) && ((sTemp[iLen - 1] == ' ') || (sTemp[iLen - 1] == '\t') || (sTemp[iLen - 1] == '\r') || (sTemp[iLen - 1] == '\n')) ) {
				iLen--;
			}
			sTemp[iLen] = '\0';
		}
	}
	iValueEnd = iLen;
	if ( iValueEnd >= iOutCapacity ) {
		iValueEnd = iOutCapacity - 1;
	}
	memcpy(sOut, sTemp, (size_t)iValueEnd);
	sOut[iValueEnd] = '\0';
	return 1;
}

typedef int (*xge_svg_style_value_valid_fn)(const char* sValue, void* pUser);

static int __xgeSvgCssPropertyNameMatches(const char* pText, size_t iNameLen, const char* sName)
{
	if ( (pText == NULL) || (sName == NULL) ) {
		return 0;
	}
	return (strlen(sName) == iNameLen) && (memcmp(pText, sName, iNameLen) == 0);
}

static int __xgeSvgStyleCopyValidEx(const char* sStyle, const char* sName, char* sOut, int iOutCapacity, int iImportanceMode, int bFirstDeclaration, xge_svg_style_value_valid_fn pfnValid, void* pUser)
{
	const char* p;
	size_t iNameLen;
	int bFound;

	if ( (sStyle == NULL) || (sName == NULL) || (sOut == NULL) || (iOutCapacity <= 0) ) {
		return 0;
	}
	sOut[0] = '\0';
	iNameLen = strlen(sName);
	p = sStyle;
	bFound = 0;
	while ( *p != '\0' ) {
		for ( ;; ) {
			while ( (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') || (*p == ';') ) p++;
			if ( (p[0] == '/') && (p[1] == '*') ) {
				p += 2;
				while ( (p[0] != '\0') && !((p[0] == '*') && (p[1] == '/')) ) p++;
				if ( p[0] != '\0' ) p += 2;
				continue;
			}
			break;
		}
		if ( __xgeSvgCssPropertyNameMatches(p, iNameLen, sName) ) {
			const char* q = p + iNameLen;
			while ( (*q == ' ') || (*q == '\t') || (*q == '\r') || (*q == '\n') ) q++;
			if ( *q == ':' ) {
				const char* pValue;
				char sValue[XGE_SVG_ATTR_MAX];
				int bImportant;

				q++;
				for ( ;; ) {
					while ( (*q == ' ') || (*q == '\t') || (*q == '\r') || (*q == '\n') ) q++;
					if ( (q[0] == '/') && (q[1] == '*') ) {
						q += 2;
						while ( (q[0] != '\0') && !((q[0] == '*') && (q[1] == '/')) ) q++;
						if ( q[0] != '\0' ) q += 2;
						continue;
					}
					break;
				}
				pValue = q;
				while ( (*q != '\0') && (*q != ';') ) q++;
				if ( __xgeSvgStyleValueCleanCopy(pValue, q, sValue, sizeof(sValue), &bImportant) &&
				     ((iImportanceMode == XGE_SVG_STYLE_IMPORTANCE_ANY) || (iImportanceMode == bImportant)) &&
				     ((pfnValid == NULL) || pfnValid(sValue, pUser)) ) {
					snprintf(sOut, (size_t)iOutCapacity, "%s", sValue);
					sOut[iOutCapacity - 1] = '\0';
					bFound = 1;
					if ( bFirstDeclaration ) {
						return 1;
					}
				} else if ( bFirstDeclaration ) {
					return 0;
				}
			}
		}
		while ( (*p != '\0') && (*p != ';') ) p++;
	}
	return bFound;
}

static int __xgeSvgStyleCopyEx(const char* sStyle, const char* sName, char* sOut, int iOutCapacity, int iImportanceMode, int bFirstDeclaration)
{
	return __xgeSvgStyleCopyValidEx(sStyle, sName, sOut, iOutCapacity, iImportanceMode, bFirstDeclaration, NULL, NULL);
}

static int __xgeSvgStyleCopy(const char* sStyle, const char* sName, char* sOut, int iOutCapacity)
{
	return __xgeSvgStyleCopyEx(sStyle, sName, sOut, iOutCapacity, XGE_SVG_STYLE_IMPORTANCE_ANY, 0);
}

static int __xgeSvgUrlIdCopyFromValue(const char* sValue, char* sOut, int iOutCapacity);
static int __xgeSvgUrlIdCopyFromValueThorvg(const char* sValue, char* sOut, int iOutCapacity, int* pHandled);

static void __xgeSvgStyleReplayThorvgUrl(const char* sStyle, const char* sName, char* sId, int iIdCapacity, int* pImportant)
{
	const char* p;
	size_t iNameLen;
	int bImportant;

	if ( (sStyle == NULL) || (sName == NULL) || (sId == NULL) || (iIdCapacity <= 0) ) {
		return;
	}
	iNameLen = strlen(sName);
	bImportant = 0;
	p = sStyle;
	while ( *p != '\0' ) {
		for ( ;; ) {
			while ( (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') || (*p == ';') ) p++;
			if ( (p[0] == '/') && (p[1] == '*') ) {
				p += 2;
				while ( (p[0] != '\0') && !((p[0] == '*') && (p[1] == '/')) ) p++;
				if ( p[0] != '\0' ) p += 2;
				continue;
			}
			break;
		}
		if ( __xgeSvgCssPropertyNameMatches(p, iNameLen, sName) ) {
			const char* q;

			q = p + iNameLen;
			while ( (*q == ' ') || (*q == '\t') || (*q == '\r') || (*q == '\n') ) q++;
			if ( *q == ':' ) {
				const char* pValue;
				char sValue[XGE_SVG_ATTR_MAX];
				int bDeclarationImportant;

				q++;
				for ( ;; ) {
					while ( (*q == ' ') || (*q == '\t') || (*q == '\r') || (*q == '\n') ) q++;
					if ( (q[0] == '/') && (q[1] == '*') ) {
						q += 2;
						while ( (q[0] != '\0') && !((q[0] == '*') && (q[1] == '/')) ) q++;
						if ( q[0] != '\0' ) q += 2;
						continue;
					}
					break;
				}
				pValue = q;
				while ( (*q != '\0') && (*q != ';') ) q++;
				if ( __xgeSvgStyleValueCleanCopy(pValue, q, sValue, sizeof(sValue), &bDeclarationImportant) ) {
					if ( bDeclarationImportant || !bImportant ) {
						char sParsedId[XGE_SVG_ID_MAX];
						int bHandled;

						if ( __xgeSvgUrlIdCopyFromValueThorvg(sValue, sParsedId, sizeof(sParsedId), &bHandled) ) {
								snprintf(sId, (size_t)iIdCapacity, "%s", sParsedId);
								sId[iIdCapacity - 1] = '\0';
						} else if ( bHandled ) {
							sId[0] = '\0';
						}
					}
					if ( bDeclarationImportant ) bImportant = 1;
				}
			}
		}
		while ( (*p != '\0') && (*p != ';') ) p++;
	}
	if ( pImportant != NULL ) *pImportant = bImportant;
}

static int __xgeSvgStyleCollectThorvgClassUrl(const char* sStyle, const char* sName, char* sOut, int iOutCapacity, int* pImportant)
{
	char sId[XGE_SVG_ID_MAX];

	if ( (sStyle == NULL) || (sName == NULL) || (sOut == NULL) || (iOutCapacity <= 0) || (pImportant == NULL) ) {
		return 0;
	}
	sId[0] = '\0';
	__xgeSvgStyleReplayThorvgUrl(sStyle, sName, sId, sizeof(sId), pImportant);
	if ( sId[0] == '\0' ) {
		sOut[0] = '\0';
		return 0;
	}
	snprintf(sOut, (size_t)iOutCapacity, "url(#%s)", sId);
	sOut[iOutCapacity - 1] = '\0';
	return 1;
}

typedef struct xge_svg_css_class_property_t {
	const char* sName;
	int bFirstValue;
} xge_svg_css_class_property_t;

typedef struct xge_svg_css_paint_replay_t {
	uint32_t iColor;
	int bNone;
	int bCurrentColor;
	int bUrl;
	char sUrlId[XGE_SVG_ID_MAX];
} xge_svg_css_paint_replay_t;

static int __xgeSvgStyleCollectThorvgClassColor(const char* sStyle, const char* sName, int bPaint, int bStroke, char* sOut, int iOutCapacity, int* pImportant)
{
	const char* p;
	size_t iNameLen;
	xge_svg_css_paint_replay_t tPaint;
	uint32_t iColor;
	int bColorSet;
	int bFound;
	int bImportant;

	if ( (sStyle == NULL) || (sName == NULL) || (sOut == NULL) || (iOutCapacity <= 0) || (pImportant == NULL) ) {
		return 0;
	}
	memset(&tPaint, 0, sizeof(tPaint));
	tPaint.iColor = XGE_COLOR_RGBA(0, 0, 0, 255);
	tPaint.bNone = bStroke;
	iColor = XGE_COLOR_RGBA(0, 0, 0, 255);
	bColorSet = 0;
	bFound = 0;
	bImportant = 0;
	iNameLen = strlen(sName);
	p = sStyle;
	while ( *p != '\0' ) {
		for ( ;; ) {
			while ( (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') || (*p == ';') ) p++;
			if ( (p[0] == '/') && (p[1] == '*') ) {
				p += 2;
				while ( (p[0] != '\0') && !((p[0] == '*') && (p[1] == '/')) ) p++;
				if ( p[0] != '\0' ) p += 2;
				continue;
			}
			break;
		}
		if ( __xgeSvgCssPropertyNameMatches(p, iNameLen, sName) ) {
			const char* q = p + iNameLen;

			while ( (*q == ' ') || (*q == '\t') || (*q == '\r') || (*q == '\n') ) q++;
			if ( *q == ':' ) {
				const char* pValue;
				char sValue[XGE_SVG_ATTR_MAX];
				int bDeclarationImportant;

				q++;
				while ( (*q == ' ') || (*q == '\t') || (*q == '\r') || (*q == '\n') ) q++;
				pValue = q;
				while ( (*q != '\0') && (*q != ';') ) q++;
				if ( __xgeSvgStyleValueCleanCopy(pValue, q, sValue, sizeof(sValue), &bDeclarationImportant) ) {
					bFound = 1;
					if ( bPaint ) {
						if ( strcmp(sValue, "none") == 0 ) {
							tPaint.bNone = 1;
						} else if ( strcmp(sValue, "currentColor") == 0 ) {
							tPaint.bCurrentColor = 1;
							tPaint.bNone = 0;
						} else {
							char sId[XGE_SVG_ID_MAX];
							int bUrlHandled;
							uint32_t iParsed = tPaint.iColor;

							if ( __xgeSvgUrlIdCopyFromValueThorvg(sValue, sId, sizeof(sId), &bUrlHandled) ) {
								snprintf(tPaint.sUrlId, sizeof(tPaint.sUrlId), "%s", sId);
								tPaint.sUrlId[sizeof(tPaint.sUrlId) - 1u] = '\0';
								tPaint.bUrl = 1;
								tPaint.bNone = 0;
							} else if ( bUrlHandled ) {
								tPaint.sUrlId[0] = '\0';
								tPaint.bUrl = 0;
								tPaint.bNone = 0;
							} else if ( __xgeSvgParseColorThorvg(sValue, &iParsed) ) {
								tPaint.iColor = iParsed;
								tPaint.bNone = 0;
							}
						}
					} else {
						uint32_t iParsed = iColor;

						if ( __xgeSvgParseColorThorvg(sValue, &iParsed) ) {
							iColor = iParsed;
							bColorSet = 1;
						}
					}
					if ( bDeclarationImportant ) bImportant = 1;
				}
			}
		}
		while ( (*p != '\0') && (*p != ';') ) p++;
	}
	if ( !bFound || (!bPaint && !bColorSet) ) {
		sOut[0] = '\0';
		*pImportant = bImportant;
		return 0;
	}
	if ( bPaint ) {
		if ( tPaint.bNone ) {
			snprintf(sOut, (size_t)iOutCapacity, "%s", "none");
		} else if ( tPaint.bUrl ) {
			snprintf(sOut, (size_t)iOutCapacity, "url(#%s)", tPaint.sUrlId);
		} else if ( tPaint.bCurrentColor ) {
			snprintf(sOut, (size_t)iOutCapacity, "%s", "currentColor");
		} else {
			snprintf(sOut, (size_t)iOutCapacity, "#%02x%02x%02x",
			         (unsigned int)XGE_COLOR_GET_R(tPaint.iColor),
			         (unsigned int)XGE_COLOR_GET_G(tPaint.iColor),
			         (unsigned int)XGE_COLOR_GET_B(tPaint.iColor));
		}
	} else {
		snprintf(sOut, (size_t)iOutCapacity, "#%02x%02x%02x",
		         (unsigned int)XGE_COLOR_GET_R(iColor),
		         (unsigned int)XGE_COLOR_GET_G(iColor),
		         (unsigned int)XGE_COLOR_GET_B(iColor));
	}
	sOut[iOutCapacity - 1] = '\0';
	*pImportant = bImportant;
	return 1;
}

static int __xgeSvgStyleCollectThorvgClassDashArray(const char* sStyle, char* sOut, int iOutCapacity, int* pImportant)
{
	const char* p;
	size_t iNameLen;
	int iOutLength;
	int bFound;

	if ( (sStyle == NULL) || (sOut == NULL) || (iOutCapacity <= 0) || (pImportant == NULL) ) {
		return 0;
	}
	sOut[0] = '\0';
	*pImportant = 0;
	iNameLen = strlen("stroke-dasharray");
	iOutLength = 0;
	bFound = 0;
	p = sStyle;
	while ( *p != '\0' ) {
		for ( ;; ) {
			while ( (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') || (*p == ';') ) p++;
			if ( (p[0] == '/') && (p[1] == '*') ) {
				p += 2;
				while ( (p[0] != '\0') && !((p[0] == '*') && (p[1] == '/')) ) p++;
				if ( p[0] != '\0' ) p += 2;
				continue;
			}
			break;
		}
		if ( __xgeSvgCssPropertyNameMatches(p, iNameLen, "stroke-dasharray") ) {
			const char* q;

			q = p + iNameLen;
			while ( (*q == ' ') || (*q == '\t') || (*q == '\r') || (*q == '\n') ) q++;
			if ( *q == ':' ) {
				const char* pValue;
				char sValue[XGE_SVG_ATTR_MAX];
				int bDeclarationImportant;

				q++;
				for ( ;; ) {
					while ( (*q == ' ') || (*q == '\t') || (*q == '\r') || (*q == '\n') ) q++;
					if ( (q[0] == '/') && (q[1] == '*') ) {
						q += 2;
						while ( (q[0] != '\0') && !((q[0] == '*') && (q[1] == '/')) ) q++;
						if ( q[0] != '\0' ) q += 2;
						continue;
					}
					break;
				}
				pValue = q;
				while ( (*q != '\0') && (*q != ';') ) q++;
				if ( __xgeSvgStyleValueCleanCopy(pValue, q, sValue, sizeof(sValue), &bDeclarationImportant) ) {
					int iValueLen;
					int iCopyLen;

					bFound = 1;
					if ( bDeclarationImportant ) {
						*pImportant = 1;
					}
					if ( strncmp(sValue, "none", 4u) != 0 ) {
						iValueLen = (int)strlen(sValue);
						if ( (iOutLength > 0) && (iOutLength < iOutCapacity - 1) ) {
							sOut[iOutLength++] = ' ';
						}
						iCopyLen = iOutCapacity - iOutLength - 1;
						if ( iCopyLen > iValueLen ) iCopyLen = iValueLen;
						if ( iCopyLen > 0 ) {
							memcpy(sOut + iOutLength, sValue, (size_t)iCopyLen);
							iOutLength += iCopyLen;
							sOut[iOutLength] = '\0';
						}
					}
				}
			}
		}
		while ( (*p != '\0') && (*p != ';') ) p++;
	}
	if ( bFound && (iOutLength <= 0) ) {
		snprintf(sOut, (size_t)iOutCapacity, "%s", "none");
		sOut[iOutCapacity - 1] = '\0';
	}
	return bFound;
}

static int __xgeSvgStyleNormalizeThorvgClass(const char* sStyle, char** ppNormalized)
{
	static const xge_svg_css_class_property_t arrProperties[] = {
		{"color", 0},
		{"fill", 0},
		{"fill-rule", 0},
		{"fill-opacity", 0},
		{"opacity", 1},
		{"stroke", 0},
		{"stroke-width", 0},
		{"stroke-linejoin", 0},
		{"stroke-miterlimit", 0},
		{"stroke-linecap", 0},
		{"stroke-opacity", 0},
		{"stroke-dasharray", 0},
		{"stroke-dashoffset", 0},
		{"transform", 0},
		{"clip-path", 0},
		{"mask", 0},
		{"mask-type", 0},
		{"display", 1},
		{"paint-order", 1},
		{"filter", 0},
		{"mix-blend-mode", 1},
		{"text-anchor", 1}
	};
	char sValue[XGE_SVG_ATTR_MAX];
	char sImportantValue[XGE_SVG_ATTR_MAX];
	char* sNormalized;
	size_t iCapacity;
	size_t iLength;
	int i;

	if ( ppNormalized == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppNormalized = NULL;
	if ( sStyle == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iCapacity = strlen(sStyle) + 1u;
	if ( iCapacity < 32u ) iCapacity = 32u;
	sNormalized = (char*)xrtMalloc(iCapacity);
	if ( sNormalized == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	sNormalized[0] = '\0';
	iLength = 0u;
	for ( i = 0; i < (int)(sizeof(arrProperties) / sizeof(arrProperties[0])); i++ ) {
		const char* sName = arrProperties[i].sName;
		int bImportant;
		size_t iNameLen;
		size_t iValueLen;
		size_t iRequired;

		if ( strcmp(sName, "color") == 0 ) {
			if ( !__xgeSvgStyleCollectThorvgClassColor(sStyle, sName, 0, 0, sValue, sizeof(sValue), &bImportant) ) {
				continue;
			}
		} else if ( strcmp(sName, "fill") == 0 ) {
			if ( !__xgeSvgStyleCollectThorvgClassColor(sStyle, sName, 1, 0, sValue, sizeof(sValue), &bImportant) ) {
				continue;
			}
		} else if ( strcmp(sName, "stroke") == 0 ) {
			if ( !__xgeSvgStyleCollectThorvgClassColor(sStyle, sName, 1, 1, sValue, sizeof(sValue), &bImportant) ) {
				continue;
			}
		} else if ( strcmp(sName, "stroke-dasharray") == 0 ) {
			if ( !__xgeSvgStyleCollectThorvgClassDashArray(sStyle, sValue, sizeof(sValue), &bImportant) ) {
				continue;
			}
		} else if ( (strcmp(sName, "clip-path") == 0) || (strcmp(sName, "mask") == 0) ) {
			if ( !__xgeSvgStyleCollectThorvgClassUrl(sStyle, sName, sValue, sizeof(sValue), &bImportant) ) {
				continue;
			}
		} else {
			if ( !__xgeSvgStyleCopyEx(sStyle, sName, sValue, sizeof(sValue), XGE_SVG_STYLE_IMPORTANCE_ANY, arrProperties[i].bFirstValue) ) {
				continue;
			}
			bImportant = __xgeSvgStyleCopyEx(sStyle, sName, sImportantValue, sizeof(sImportantValue), XGE_SVG_STYLE_IMPORTANCE_IMPORTANT, 0);
		}
		iNameLen = strlen(sName);
		iValueLen = strlen(sValue);
		iRequired = iLength + iNameLen + 1u + iValueLen + (bImportant ? 11u : 0u) + 2u;
		if ( iRequired > iCapacity ) {
			char* sResized;

			iCapacity *= 2u;
			if ( iCapacity < iRequired ) iCapacity = iRequired;
			sResized = (char*)xrtRealloc(sNormalized, iCapacity);
			if ( sResized == NULL ) {
				xrtFree(sNormalized);
				return XGE_ERROR_OUT_OF_MEMORY;
			}
			sNormalized = sResized;
		}
		memcpy(sNormalized + iLength, sName, iNameLen);
		iLength += iNameLen;
		sNormalized[iLength++] = ':';
		memcpy(sNormalized + iLength, sValue, iValueLen);
		iLength += iValueLen;
		if ( bImportant ) {
			memcpy(sNormalized + iLength, " !important", 11u);
			iLength += 11u;
		}
		sNormalized[iLength++] = ';';
		sNormalized[iLength] = '\0';
	}
	*ppNormalized = sNormalized;
	return XGE_OK;
}

static int __xgeSvgAttrOrStyleCopy(const char* pTag, const char* pTagEnd, const char* sStyle, const char* sName, char* sOut, int iOutCapacity)
{
	if ( __xgeSvgStyleCopy(sStyle, sName, sOut, iOutCapacity) ) {
		return 1;
	}
	return __xgeSvgAttrCopy(pTag, pTagEnd, sName, sOut, iOutCapacity);
}

static int __xgeSvgDoubleIsFiniteFloat(double fValue)
{
	if ( fValue != fValue ) {
		return 0;
	}
	if ( (fValue < -XGE_SVG_FLOAT_ABS_MAX) || (fValue > XGE_SVG_FLOAT_ABS_MAX) ) {
		return 0;
	}
	return 1;
}

static int __xgeSvgDoubleToFloat(double fValue, float* pValue)
{
	if ( (pValue == NULL) || !__xgeSvgDoubleIsFiniteFloat(fValue) ) {
		return 0;
	}
	*pValue = (float)fValue;
	return 1;
}

static int __xgeSvgFloatFinite(float fValue)
{
	return (fValue == fValue) && (fValue >= -XGE_SVG_FLOAT_ABS_MAX) && (fValue <= XGE_SVG_FLOAT_ABS_MAX);
}

static int __xgeSvgNumberIsDigit(char c)
{
	return (c >= '0') && (c <= '9');
}

static int __xgeSvgScanNumberEnd(const char* pText, const char** ppEnd)
{
	const char* p;
	int iDigitCount;
	int iFracDigitCount;
	int iExpDigitCount;

	if ( (pText == NULL) || (ppEnd == NULL) ) {
		return 0;
	}
	p = pText;
	if ( (*p == '+') || (*p == '-') ) {
		p++;
	}
	iDigitCount = 0;
	while ( __xgeSvgNumberIsDigit(*p) ) {
		iDigitCount++;
		p++;
	}
	iFracDigitCount = 0;
	if ( *p == '.' ) {
		p++;
		while ( __xgeSvgNumberIsDigit(*p) ) {
			iFracDigitCount++;
			p++;
		}
	}
	if ( (iDigitCount + iFracDigitCount) <= 0 ) {
		return 0;
	}
	if ( (*p == 'e') || (*p == 'E') ) {
		const char* pExp;

		pExp = p;
		p++;
		if ( (*p == '+') || (*p == '-') ) {
			p++;
		}
		iExpDigitCount = 0;
		while ( __xgeSvgNumberIsDigit(*p) ) {
			iExpDigitCount++;
			p++;
		}
		if ( iExpDigitCount <= 0 ) {
			p = pExp;
		}
	}
	*ppEnd = p;
	return 1;
}

static int __xgeSvgParseNumberAt(const char* pText, const char** ppEnd, double* pValue)
{
	const char* pNumberEnd;
	char* pEnd;
	double fValue;

	if ( (pText == NULL) || (ppEnd == NULL) || (pValue == NULL) ) {
		return 0;
	}
	if ( !__xgeSvgScanNumberEnd(pText, &pNumberEnd) ) {
		return 0;
	}
	fValue = strtod(pText, &pEnd);
	if ( (pEnd == pText) || (pEnd != pNumberEnd) || !__xgeSvgDoubleIsFiniteFloat(fValue) ) {
		return 0;
	}
	*ppEnd = pNumberEnd;
	*pValue = fValue;
	return 1;
}

static int __xgeSvgParseFloat(const char* sText, float* pValue)
{
	const char* pEnd;
	double fValue;

	if ( (sText == NULL) || (pValue == NULL) ) {
		return 0;
	}
	if ( !__xgeSvgParseNumberAt(sText, &pEnd, &fValue) ) {
		return 0;
	}
	return __xgeSvgDoubleToFloat(fValue, pValue);
}

static void __xgeSvgSkipSpaces(const char** ppText);

static int __xgeSvgParseFloatOrPercent(const char* sText, float* pValue)
{
	const char* pEnd;
	double fValue;

	if ( (sText == NULL) || (pValue == NULL) ) {
		return 0;
	}
	while ( (*sText == ' ') || (*sText == '\t') || (*sText == '\r') || (*sText == '\n') ) sText++;
	if ( !__xgeSvgParseNumberAt(sText, &pEnd, &fValue) ) {
		return 0;
	}
	while ( (*pEnd == ' ') || (*pEnd == '\t') ) pEnd++;
	if ( *pEnd == '%' ) {
		fValue /= 100.0;
	}
	return __xgeSvgDoubleToFloat(fValue, pValue);
}

static float __xgeSvgParseStopOffset(const char* sText)
{
	const char* p;
	const char* pPercent;
	const char* pTextEnd;
	const char* pEnd;
	double fValue;

	if ( sText == NULL ) {
		return 0.0f;
	}
	if ( !__xgeSvgParseNumberAt(sText, &pEnd, &fValue) ) {
		return 0.0f;
	}
	p = pEnd;
	__xgeSvgSkipSpaces(&p);
	pTextEnd = sText + strlen(sText);
	pPercent = strchr(sText, '%');
	if ( pPercent != NULL ) {
		if ( (p != pPercent) || ((pPercent + 1) != pTextEnd) ) {
			return 0.0f;
		}
		fValue /= 100.0;
	} else if ( p != pTextEnd ) {
		return 0.0f;
	}
	return (float)fValue;
}

static int __xgeSvgParseOpacity(const char* sText, float* pValue)
{
	const char* p;
	const char* pEnd;
	double fValue;

	if ( (sText == NULL) || (pValue == NULL) ) {
		return 0;
	}
	p = sText;
	__xgeSvgSkipSpaces(&p);
	if ( !__xgeSvgParseNumberAt(p, &pEnd, &fValue) ) {
		return 0;
	}
	p = pEnd;
	__xgeSvgSkipSpaces(&p);
	if ( *p == '%' ) {
		fValue /= 100.0;
		p++;
		__xgeSvgSkipSpaces(&p);
	}
	if ( *p != '\0' ) {
		return 0;
	}
	if ( fValue < 0.0 ) fValue = 0.0;
	if ( fValue > 1.0 ) fValue = 1.0;
	*pValue = (float)fValue;
	return 1;
}

static int __xgeSvgOpacityIsNonFinite(const char* sText)
{
	const char* p;
	const char* pToken;
	size_t iLength;

	if ( sText == NULL ) return 0;
	p = sText;
	while ( (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') ) p++;
	if ( (*p == '+') || (*p == '-') ) p++;
	pToken = p;
	while ( ((*p >= 'a') && (*p <= 'z')) || ((*p >= 'A') && (*p <= 'Z')) ) p++;
	iLength = (size_t)(p - pToken);
	while ( (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') ) p++;
	if ( *p != '\0' ) return 0;
	if ( (iLength == 3) &&
	     ((pToken[0] == 'n') || (pToken[0] == 'N')) &&
	     ((pToken[1] == 'a') || (pToken[1] == 'A')) &&
	     ((pToken[2] == 'n') || (pToken[2] == 'N')) ) return 1;
	if ( (iLength == 3) &&
	     ((pToken[0] == 'i') || (pToken[0] == 'I')) &&
	     ((pToken[1] == 'n') || (pToken[1] == 'N')) &&
	     ((pToken[2] == 'f') || (pToken[2] == 'F')) ) return 1;
	if ( (iLength == 8) &&
	     ((pToken[0] == 'i') || (pToken[0] == 'I')) &&
	     ((pToken[1] == 'n') || (pToken[1] == 'N')) &&
	     ((pToken[2] == 'f') || (pToken[2] == 'F')) &&
	     ((pToken[3] == 'i') || (pToken[3] == 'I')) &&
	     ((pToken[4] == 'n') || (pToken[4] == 'N')) &&
	     ((pToken[5] == 'i') || (pToken[5] == 'I')) &&
	     ((pToken[6] == 't') || (pToken[6] == 'T')) &&
	     ((pToken[7] == 'y') || (pToken[7] == 'Y')) ) return 1;
	return 0;
}

static float __xgeSvgParseOpacityThorvgEx(const char* sText, int* pNonFinite)
{
	const char* p;
	const char* pEnd;
	double fValue;
	long iOpacity;
	uint8_t iAlpha;

	if ( pNonFinite != NULL ) *pNonFinite = 0;
	if ( sText == NULL ) {
		return 1.0f;
	}
	if ( __xgeSvgOpacityIsNonFinite(sText) ) {
		if ( pNonFinite != NULL ) *pNonFinite = 1;
		return 0.0f;
	}
	p = sText;
	while ( (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') ) p++;
	if ( !__xgeSvgParseNumberAt(p, &pEnd, &fValue) ) {
		return 1.0f;
	}
	if ( (pEnd[0] == '%') && (pEnd[1] == '\0') ) {
		fValue *= 2.55;
	} else if ( pEnd[0] == '\0' ) {
		fValue *= 255.0;
	} else {
		return 1.0f;
	}
	iOpacity = lrint(fValue);
	iAlpha = (uint8_t)iOpacity;
	return (float)iAlpha / 255.0f;
}

static float __xgeSvgParseOpacityThorvg(const char* sText)
{
	return __xgeSvgParseOpacityThorvgEx(sText, NULL);
}

static int __xgeSvgUnitStartsWith(const char* pText, const char* sUnit)
{
	int i;

	if ( (pText == NULL) || (sUnit == NULL) ) {
		return 0;
	}
	for ( i = 0; sUnit[i] != '\0'; i++ ) {
		char a = pText[i];
		char b = sUnit[i];
		if ( (a >= 'A') && (a <= 'Z') ) a = (char)(a - 'A' + 'a');
		if ( a != b ) {
			return 0;
		}
	}
	return 1;
}

static float __xgeSvgLengthPercentRef(xge_svg pSvg, int iBasis)
{
	float fW;
	float fH;

	fW = 0.0f;
	fH = 0.0f;
	if ( __xgeSvgValid(pSvg) ) {
		if ( pSvg->bHasViewBox ) {
			fW = pSvg->tViewBox.fW;
			fH = pSvg->tViewBox.fH;
		}
		if ( fW <= 0.0f ) fW = pSvg->fWidth;
		if ( fH <= 0.0f ) fH = pSvg->fHeight;
	}
	if ( fW <= 0.0f ) fW = 1.0f;
	if ( fH <= 0.0f ) fH = 1.0f;
	if ( iBasis == XGE_SVG_LENGTH_BASIS_X ) return fW;
	if ( iBasis == XGE_SVG_LENGTH_BASIS_Y ) return fH;
	return sqrtf((fW * fW + fH * fH) * 0.5f);
}

static int __xgeSvgParseLengthTokenEx(const char** ppText, float fPercentRef, float fFontSize, float* pValue)
{
	const char* p;
	const char* pNumberEnd;
	double fValue;
	float fScale;

	if ( (ppText == NULL) || (*ppText == NULL) || (pValue == NULL) ) {
		return 0;
	}
	if ( fFontSize <= 0.0f ) {
		fFontSize = XGE_SVG_DEFAULT_FONT_SIZE;
	}
	p = *ppText;
	while ( (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') || (*p == ',') ) p++;
	if ( !__xgeSvgParseNumberAt(p, &pNumberEnd, &fValue) ) {
		return 0;
	}
	p = pNumberEnd;
	while ( (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') ) p++;
	fScale = 1.0f;
	if ( *p == '%' ) {
		fValue = fValue * (double)fPercentRef / 100.0;
		p++;
	} else if ( __xgeSvgUnitStartsWith(p, "px") ) {
		p += 2;
	} else if ( __xgeSvgUnitStartsWith(p, "pt") ) {
		fScale = 96.0f / 72.0f;
		p += 2;
	} else if ( __xgeSvgUnitStartsWith(p, "pc") ) {
		fScale = 16.0f;
		p += 2;
	} else if ( __xgeSvgUnitStartsWith(p, "in") ) {
		fScale = 96.0f;
		p += 2;
	} else if ( __xgeSvgUnitStartsWith(p, "cm") ) {
		fScale = 96.0f / 2.54f;
		p += 2;
	} else if ( __xgeSvgUnitStartsWith(p, "mm") ) {
		fScale = 96.0f / 25.4f;
		p += 2;
	} else if ( __xgeSvgUnitStartsWith(p, "q") ) {
		fScale = 96.0f / 101.6f;
		p += 1;
	} else if ( __xgeSvgUnitStartsWith(p, "em") ) {
		fScale = fFontSize;
		p += 2;
	} else if ( __xgeSvgUnitStartsWith(p, "ex") ) {
		fScale = fFontSize * 0.5f;
		p += 2;
	} else if ( ((*p >= 'a') && (*p <= 'z')) || ((*p >= 'A') && (*p <= 'Z')) ) {
		return 0;
	}
	fValue *= (double)fScale;
	if ( !__xgeSvgDoubleToFloat(fValue, pValue) ) {
		return 0;
	}
	*ppText = p;
	return 1;
}

static int __xgeSvgStringHasUnit(const char* sText, const char* sUnit)
{
	const char* p;
	int i;

	if ( (sText == NULL) || (sUnit == NULL) || (sUnit[0] == '\0') ) {
		return 0;
	}
	for ( p = sText; *p != '\0'; p++ ) {
		for ( i = 0; sUnit[i] != '\0'; i++ ) {
			char a = p[i];
			char b = sUnit[i];
			if ( (a >= 'A') && (a <= 'Z') ) a = (char)(a - 'A' + 'a');
			if ( a != b ) {
				break;
			}
		}
		if ( sUnit[i] == '\0' ) {
			return 1;
		}
	}
	return 0;
}

static int __xgeSvgParseLengthLooseEx(const char* sText, float fPercentRef, float fFontSize, float* pValue)
{
	const char* p;
	const char* pUnit;
	const char* pNumberEnd;
	double fValue;
	float fScale;

	if ( (sText == NULL) || (pValue == NULL) ) {
		return 0;
	}
	if ( fFontSize <= 0.0f ) {
		fFontSize = XGE_SVG_DEFAULT_FONT_SIZE;
	}
	p = sText;
	while ( (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') || (*p == ',') ) p++;
	if ( !__xgeSvgParseNumberAt(p, &pNumberEnd, &fValue) ) {
		*pValue = 0.0f;
		return 1;
	}
	pUnit = pNumberEnd;
	while ( (*pUnit == ' ') || (*pUnit == '\t') || (*pUnit == '\r') || (*pUnit == '\n') ) pUnit++;
	if ( *pUnit == '%' ) {
		fValue = fValue * (double)fPercentRef / 100.0;
	} else {
		fScale = 1.0f;
		if ( __xgeSvgStringHasUnit(sText, "em") ) fScale = fFontSize;
		else if ( __xgeSvgStringHasUnit(sText, "ex") ) fScale = fFontSize * 0.5f;
		else if ( __xgeSvgStringHasUnit(sText, "cm") ) fScale = 96.0f / 2.54f;
		else if ( __xgeSvgStringHasUnit(sText, "mm") ) fScale = 96.0f / 25.4f;
		else if ( __xgeSvgStringHasUnit(sText, "pt") ) fScale = 96.0f / 72.0f;
		else if ( __xgeSvgStringHasUnit(sText, "pc") ) fScale = 16.0f;
		else if ( __xgeSvgStringHasUnit(sText, "in") ) fScale = 96.0f;
		fValue *= (double)fScale;
	}
	if ( !__xgeSvgDoubleToFloat(fValue, pValue) ) {
		*pValue = 0.0f;
	}
	return 1;
}

static int __xgeSvgParseLengthStrictEx(const char* sText, float fPercentRef, float fFontSize, float* pValue)
{
	const char* p;

	if ( (sText == NULL) || (pValue == NULL) ) {
		return 0;
	}
	p = sText;
	if ( !__xgeSvgParseLengthTokenEx(&p, fPercentRef, fFontSize, pValue) ) {
		return 0;
	}
	while ( (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') ) p++;
	return *p == '\0';
}

static int __xgeSvgParseLengthEx(const char* sText, float fPercentRef, float fFontSize, float* pValue)
{
	if ( (sText == NULL) || (pValue == NULL) ) {
		return 0;
	}
	if ( __xgeSvgParseLengthStrictEx(sText, fPercentRef, fFontSize, pValue) ) {
		return 1;
	}
	return __xgeSvgParseLengthLooseEx(sText, fPercentRef, fFontSize, pValue);
}

static int __xgeSvgParseFontSize(const char* sText, float fInheritedFontSize, float* pValue)
{
	float fValue;

	if ( (sText == NULL) || (pValue == NULL) ) {
		return 0;
	}
	if ( fInheritedFontSize <= 0.0f ) {
		fInheritedFontSize = XGE_SVG_DEFAULT_FONT_SIZE;
	}
	if ( !__xgeSvgParseLengthLooseEx(sText, fInheritedFontSize, fInheritedFontSize, &fValue) ) {
		fValue = 0.0f;
	}
	if ( fValue < 0.0f ) {
		fValue = 0.0f;
	}
	*pValue = fValue;
	return 1;
}

static float __xgeSvgAttrLengthRefEx(const char* pTag, const char* pTagEnd, const char* sName, float fPercentRef, float fFontSize, float fDefault)
{
	char sValue[XGE_SVG_ATTR_MAX];
	float fValue;

	if ( !__xgeSvgAttrCopy(pTag, pTagEnd, sName, sValue, sizeof(sValue)) ) {
		return fDefault;
	}
	return __xgeSvgParseLengthEx(sValue, fPercentRef, fFontSize, &fValue) ? fValue : fDefault;
}

static float __xgeSvgAttrLengthLastRefEx(const char* pTag, const char* pTagEnd, const char* sName, float fPercentRef, float fFontSize, float fDefault)
{
	char sValue[XGE_SVG_ATTR_MAX];
	float fValue;

	if ( !__xgeSvgAttrCopyLast(pTag, pTagEnd, sName, sValue, sizeof(sValue)) ) {
		return fDefault;
	}
	return __xgeSvgParseLengthEx(sValue, fPercentRef, fFontSize, &fValue) ? fValue : fDefault;
}

static int __xgeSvgAttrLengthCopyEx(const char* pTag, const char* pTagEnd, const char* sName, float fPercentRef, float fFontSize, float* pValue)
{
	char sValue[XGE_SVG_ATTR_MAX];

	if ( (pValue == NULL) || !__xgeSvgAttrCopy(pTag, pTagEnd, sName, sValue, sizeof(sValue)) ) {
		return 0;
	}
	return __xgeSvgParseLengthEx(sValue, fPercentRef, fFontSize, pValue);
}

static int __xgeSvgAttrLengthCopyPercentEx(
	const char* pTag,
	const char* pTagEnd,
	const char* sName,
	float fPercentRef,
	float fFontSize,
	float* pValue,
	int* pPercentage
)
{
	char sValue[XGE_SVG_ATTR_MAX];
	const char* pEnd;

	if ( (pValue == NULL) || (pPercentage == NULL) ||
	     !__xgeSvgAttrCopy(pTag, pTagEnd, sName, sValue, sizeof(sValue)) ) {
		return 0;
	}
	pEnd = sValue + strlen(sValue);
	while ( (pEnd > sValue) && ((pEnd[-1] == ' ') || (pEnd[-1] == '\t') ||
	       (pEnd[-1] == '\r') || (pEnd[-1] == '\n')) ) {
		pEnd--;
	}
	*pPercentage = (pEnd > sValue) && (pEnd[-1] == '%');
	return __xgeSvgParseLengthEx(sValue, fPercentRef, fFontSize, pValue);
}

static int __xgeSvgAttrLengthLastCopyPercentEx(
	const char* pTag,
	const char* pTagEnd,
	const char* sName,
	float fPercentRef,
	float fFontSize,
	float* pValue,
	int* pPercentage
)
{
	char sValue[XGE_SVG_ATTR_MAX];
	const char* pEnd;

	if ( (pValue == NULL) || (pPercentage == NULL) ||
	     !__xgeSvgAttrCopyLast(pTag, pTagEnd, sName, sValue, sizeof(sValue)) ) {
		return 0;
	}
	pEnd = sValue + strlen(sValue);
	while ( (pEnd > sValue) && ((pEnd[-1] == ' ') || (pEnd[-1] == '\t') ||
	       (pEnd[-1] == '\r') || (pEnd[-1] == '\n')) ) {
		pEnd--;
	}
	*pPercentage = (pEnd > sValue) && (pEnd[-1] == '%');
	return __xgeSvgParseLengthEx(sValue, fPercentRef, fFontSize, pValue);
}

static float __xgeSvgAttrLengthEx(xge_svg pSvg, const char* pTag, const char* pTagEnd, const char* sName, int iBasis, float fFontSize, float fDefault)
{
	return __xgeSvgAttrLengthRefEx(pTag, pTagEnd, sName, __xgeSvgLengthPercentRef(pSvg, iBasis), fFontSize, fDefault);
}

static void __xgeSvgNormalizeRectRadii(float* pRX, float* pRY, int* pHasRX, int* pHasRY)
{
	if ( (pRX == NULL) || (pRY == NULL) || (pHasRX == NULL) || (pHasRY == NULL) ) {
		return;
	}
	if ( *pHasRX && !*pHasRY && (*pRX < XGE_SVG_EPSILON) ) {
		*pRX = 0.0f;
		*pRY = 0.0f;
		return;
	}
	if ( !*pHasRX && *pHasRY && (*pRY < XGE_SVG_EPSILON) ) {
		*pRX = 0.0f;
		*pRY = 0.0f;
		return;
	}
	if ( *pHasRX && !*pHasRY && (*pRX >= XGE_SVG_EPSILON) ) {
		*pRY = *pRX;
		*pHasRY = 1;
	} else if ( !*pHasRX && *pHasRY && (*pRY >= XGE_SVG_EPSILON) ) {
		*pRX = *pRY;
		*pHasRX = 1;
	}
	if ( !*pHasRX ) {
		*pRX = 0.0f;
	}
	if ( !*pHasRY ) {
		*pRY = 0.0f;
	}
}

static void __xgeSvgCircleEllipseRadii(xge_svg pSvg, const char* pTag, const char* pTagEnd, int bObjectBounds, float fFontSize, float* pRX, float* pRY)
{
	float fR;

	if ( (pRX == NULL) || (pRY == NULL) ) {
		return;
	}
	if ( __xgeSvgTagNameEquals(pTag, "circle") ) {
		fR = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "r", 1.0f, fFontSize, 0.0f) :
			__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "r", XGE_SVG_LENGTH_BASIS_OTHER, fFontSize, 0.0f);
		*pRX = fR;
		*pRY = fR;
		return;
	}
	*pRX = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "rx", 1.0f, fFontSize, 0.0f) :
		__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "rx", XGE_SVG_LENGTH_BASIS_X, fFontSize, 0.0f);
	*pRY = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "ry", 1.0f, fFontSize, 0.0f) :
		__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "ry", XGE_SVG_LENGTH_BASIS_Y, fFontSize, 0.0f);
}

static int __xgeSvgUrlIdCopyFromValueEx(const char* sValue, char* sOut, int iOutCapacity, const char** ppAfter)
{
	const char* p;
	const char* pIdStart;
	const char* pIdEnd;
	char cQuote;

	if ( ppAfter != NULL ) {
		*ppAfter = sValue;
	}
	if ( (sValue == NULL) || (sOut == NULL) || (iOutCapacity <= 0) ) {
		return 0;
	}
	sOut[0] = '\0';
	while ( (*sValue == ' ') || (*sValue == '\t') || (*sValue == '\r') || (*sValue == '\n') ) sValue++;
	if ( (sValue[0] != 'u') || (sValue[1] != 'r') || (sValue[2] != 'l') ||
	     (sValue[3] != '(') ) {
		return 0;
	}
	p = sValue + 4;
	while ( (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') ) p++;
	cQuote = 0;
	if ( (*p == '"') || (*p == '\'') ) {
		cQuote = *p++;
	}
	if ( *p != '#' ) {
		return 0;
	}
	p++;
	pIdStart = p;
	while ( *p != '\0' ) {
		if ( cQuote != 0 ) {
			if ( *p == cQuote ) {
				break;
			}
		} else if ( (*p == ')') || (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') ) {
			break;
		}
		p++;
	}
	pIdEnd = p;
	if ( pIdEnd <= pIdStart ) {
		return 0;
	}
	if ( (pIdEnd - pIdStart) >= iOutCapacity ) {
		return 0;
	}
	memcpy(sOut, pIdStart, (size_t)(pIdEnd - pIdStart));
	sOut[pIdEnd - pIdStart] = '\0';
	if ( cQuote != 0 ) {
		if ( *p != cQuote ) {
			return 0;
		}
		p++;
		while ( (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') ) p++;
	}
	if ( *p != ')' ) {
		return 0;
	}
	p++;
	while ( (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') ) p++;
	if ( ppAfter != NULL ) {
		*ppAfter = p;
	}
	return 1;
}

static int __xgeSvgUrlIdCopyFromValue(const char* sValue, char* sOut, int iOutCapacity)
{
	return __xgeSvgUrlIdCopyFromValueEx(sValue, sOut, iOutCapacity, NULL);
}

static int __xgeSvgUrlIdCopyFromValueThorvg(const char* sValue, char* sOut, int iOutCapacity, int* pHandled)
{
	const char* pOpen;
	const char* pClose;
	const char* pHash;
	const char* pIdStart;
	const char* pIdLast;
	const char* p;
	int iLength;

	if ( pHandled != NULL ) *pHandled = 0;
	if ( (sValue == NULL) || (sOut == NULL) || (iOutCapacity <= 0) ) {
		return 0;
	}
	sOut[0] = '\0';
	while ( (*sValue == ' ') || (*sValue == '\t') || (*sValue == '\r') || (*sValue == '\n') ) sValue++;
	if ( (sValue[0] != 'u') || (sValue[1] != 'r') || (sValue[2] != 'l') ) {
		return 0;
	}
	if ( pHandled != NULL ) *pHandled = 1;
	pOpen = strchr(sValue + 3, '(');
	pClose = strchr(sValue + 3, ')');
	if ( (pOpen == NULL) || (pClose == NULL) || (pOpen >= pClose) ) {
		return 0;
	}
	pHash = strchr(sValue + 3, '#');
	if ( (pHash == NULL) || (pHash >= pClose) ) {
		return 0;
	}
	pIdStart = pHash + 1;
	pIdLast = pClose - 1;
	while ( (pIdStart < pIdLast) && ((*pIdLast == ' ') || (*pIdLast == '\'') || (*pIdLast == '"')) ) pIdLast--;
	if ( pIdLast < pIdStart ) {
		return 0;
	}
	for ( p = pIdStart; p < pIdLast; p++ ) {
		if ( (*p == ' ') || (*p == '\'') ) {
			return 0;
		}
	}
	iLength = (int)(pIdLast - pIdStart + 1);
	if ( (iLength <= 0) || (iLength >= iOutCapacity) ) {
		return 0;
	}
	memcpy(sOut, pIdStart, (size_t)iLength);
	sOut[iLength] = '\0';
	return 1;
}

static int __xgeSvgParseUserObjectUnits(const char* sText, int iUserSpaceValue, int iObjectBoundingBoxValue, int* pUnits)
{
	if ( (sText == NULL) || (pUnits == NULL) ) {
		return 0;
	}
	if ( strcmp(sText, "userSpaceOnUse") == 0 ) {
		*pUnits = iUserSpaceValue;
		return 1;
	}
	if ( strcmp(sText, "objectBoundingBox") == 0 ) {
		*pUnits = iObjectBoundingBoxValue;
		return 1;
	}
	return 0;
}

static void __xgeSvgFilterPrimitivesReset(xge_svg_filter_t* pFilter)
{
	if ( pFilter == NULL ) {
		return;
	}
	xrtFree(pFilter->pPrimitives);
	pFilter->pPrimitives = NULL;
	pFilter->iPrimitiveCount = 0;
	pFilter->iPrimitiveCapacity = 0;
}

static xge_svg_filter_primitive_t* __xgeSvgFilterPrimitiveAppend(xge_svg_filter_t* pFilter)
{
	xge_svg_filter_primitive_t* pPrimitives;
	xge_svg_filter_primitive_t* pPrimitive;
	int iCapacity;

	if ( pFilter == NULL ) {
		return NULL;
	}
	if ( pFilter->iPrimitiveCount >= pFilter->iPrimitiveCapacity ) {
		iCapacity = pFilter->iPrimitiveCapacity > 0 ? pFilter->iPrimitiveCapacity * 2 : 4;
		pPrimitives = (xge_svg_filter_primitive_t*)xrtRealloc(
			pFilter->pPrimitives,
			(size_t)iCapacity * sizeof(*pPrimitives)
		);
		if ( pPrimitives == NULL ) {
			return NULL;
		}
		memset(
			pPrimitives + pFilter->iPrimitiveCapacity,
			0,
			(size_t)(iCapacity - pFilter->iPrimitiveCapacity) * sizeof(*pPrimitives)
		);
		pFilter->pPrimitives = pPrimitives;
		pFilter->iPrimitiveCapacity = iCapacity;
	}
	pPrimitive = &pFilter->pPrimitives[pFilter->iPrimitiveCount++];
	memset(pPrimitive, 0, sizeof(*pPrimitive));
	return pPrimitive;
}

static void __xgeSvgFilterPrimitiveParseRegionAttrs(
	xge_svg pSvg,
	const xge_svg_filter_t* pFilter,
	xge_svg_filter_primitive_t* pPrimitive,
	const char* pTag,
	const char* pTagEnd
)
{
	float fRefX;
	float fRefY;
	float fValue;
	int bPercentage;

	if ( !__xgeSvgValid(pSvg) || (pFilter == NULL) || (pPrimitive == NULL) ||
	     (pTag == NULL) || (pTagEnd == NULL) ) {
		return;
	}
	fRefX = (pFilter->iUnits == XGE_SVG_FILTER_UNITS_OBJECT_BOUNDING_BOX) ?
		1.0f : __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_X);
	fRefY = (pFilter->iUnits == XGE_SVG_FILTER_UNITS_OBJECT_BOUNDING_BOX) ?
		1.0f : __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_Y);
	if ( __xgeSvgAttrLengthLastCopyPercentEx(pTag, pTagEnd, "x", fRefX, XGE_SVG_DEFAULT_FONT_SIZE, &fValue, &bPercentage) ) {
		pPrimitive->tRegion.fX = fValue;
		pPrimitive->iRegionFlags |= XGE_SVG_FILTER_PRIMITIVE_REGION_X;
		if ( bPercentage ) pPrimitive->iRegionPercentFlags |= XGE_SVG_FILTER_PRIMITIVE_REGION_X;
	}
	if ( __xgeSvgAttrLengthLastCopyPercentEx(pTag, pTagEnd, "y", fRefY, XGE_SVG_DEFAULT_FONT_SIZE, &fValue, &bPercentage) ) {
		pPrimitive->tRegion.fY = fValue;
		pPrimitive->iRegionFlags |= XGE_SVG_FILTER_PRIMITIVE_REGION_Y;
		if ( bPercentage ) pPrimitive->iRegionPercentFlags |= XGE_SVG_FILTER_PRIMITIVE_REGION_Y;
	}
	if ( __xgeSvgAttrLengthLastCopyPercentEx(pTag, pTagEnd, "width", fRefX, XGE_SVG_DEFAULT_FONT_SIZE, &fValue, &bPercentage) ) {
		pPrimitive->tRegion.fW = fValue;
		pPrimitive->iRegionFlags |= XGE_SVG_FILTER_PRIMITIVE_REGION_W;
		if ( bPercentage ) pPrimitive->iRegionPercentFlags |= XGE_SVG_FILTER_PRIMITIVE_REGION_W;
	}
	if ( __xgeSvgAttrLengthLastCopyPercentEx(pTag, pTagEnd, "height", fRefY, XGE_SVG_DEFAULT_FONT_SIZE, &fValue, &bPercentage) ) {
		pPrimitive->tRegion.fH = fValue;
		pPrimitive->iRegionFlags |= XGE_SVG_FILTER_PRIMITIVE_REGION_H;
		if ( bPercentage ) pPrimitive->iRegionPercentFlags |= XGE_SVG_FILTER_PRIMITIVE_REGION_H;
	}
}

static void __xgeSvgFilterReset(xge_svg_filter_t* pFilter)
{
	if ( pFilter == NULL ) {
		return;
	}
	__xgeSvgFilterPrimitivesReset(pFilter);
	xrtFree(pFilter->sId);
	memset(pFilter, 0, sizeof(*pFilter));
}

static void __xgeSvgFiltersClear(xge_svg pSvg)
{
	int i;

	if ( !__xgeSvgValid(pSvg) ) {
		return;
	}
	for ( i = 0; i < pSvg->iFilterCount; i++ ) {
		__xgeSvgFilterReset(&pSvg->pFilters[i]);
	}
	xrtFree(pSvg->pFilters);
	pSvg->pFilters = NULL;
	pSvg->iFilterCount = 0;
	pSvg->iFilterCapacity = 0;
}

static int __xgeSvgFilterFind(xge_svg pSvg, const char* sId)
{
	int i;

	if ( !__xgeSvgValid(pSvg) || (sId == NULL) || (sId[0] == '\0') ) {
		return -1;
	}
	for ( i = 0; i < pSvg->iFilterCount; i++ ) {
		if ( (pSvg->pFilters[i].sId != NULL) && (strcmp(pSvg->pFilters[i].sId, sId) == 0) ) {
			return i;
		}
	}
	return -1;
}

static int __xgeSvgFilterGetOrCreate(xge_svg pSvg, const char* sId, int* pIndex)
{
	xge_svg_filter_t* pFilters;
	int iIndex;
	int iCapacity;

	if ( !__xgeSvgValid(pSvg) || (sId == NULL) || (sId[0] == '\0') || (pIndex == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xgeSvgFilterFind(pSvg, sId);
	if ( iIndex >= 0 ) {
		*pIndex = iIndex;
		return XGE_OK;
	}
	if ( pSvg->iFilterCount >= pSvg->iFilterCapacity ) {
		iCapacity = pSvg->iFilterCapacity > 0 ? pSvg->iFilterCapacity * 2 : 8;
		pFilters = (xge_svg_filter_t*)xrtRealloc(
			pSvg->pFilters,
			(size_t)iCapacity * sizeof(*pFilters)
		);
		if ( pFilters == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		memset(
			pFilters + pSvg->iFilterCapacity,
			0,
			(size_t)(iCapacity - pSvg->iFilterCapacity) * sizeof(*pFilters)
		);
		pSvg->pFilters = pFilters;
		pSvg->iFilterCapacity = iCapacity;
	}
	iIndex = pSvg->iFilterCount++;
	pSvg->pFilters[iIndex].sId = __xgeStrDup(sId);
	if ( pSvg->pFilters[iIndex].sId == NULL ) {
		pSvg->iFilterCount--;
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pSvg->pFilters[iIndex].iUnits = XGE_SVG_FILTER_UNITS_USER_SPACE;
	pSvg->pFilters[iIndex].iFilterUnits = XGE_SVG_FILTER_UNITS_OBJECT_BOUNDING_BOX;
	pSvg->pFilters[iIndex].tRegion = (xge_rect_t){-0.1f, -0.1f, 1.2f, 1.2f};
	*pIndex = iIndex;
	return XGE_OK;
}

static int __xgeSvgParseFilter(xge_svg pSvg, const char* pTag, const char* pTagEnd, int* pFilterIndex)
{
	char sId[XGE_SVG_ATTR_MAX];
	xge_svg_filter_t* pFilter;
	int iIndex;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pTag == NULL) || (pTagEnd == NULL) || (pFilterIndex == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pFilterIndex = -1;
	if ( !__xgeSvgAttrCopyLast(pTag, pTagEnd, "id", sId, sizeof(sId)) || (sId[0] == '\0') ) {
		return XGE_OK;
	}
	iRet = __xgeSvgFilterGetOrCreate(pSvg, sId, &iIndex);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pFilter = &pSvg->pFilters[iIndex];
	__xgeSvgFilterPrimitivesReset(pFilter);
	pFilter->iUnits = XGE_SVG_FILTER_UNITS_USER_SPACE;
	pFilter->iFilterUnits = XGE_SVG_FILTER_UNITS_OBJECT_BOUNDING_BOX;
	pFilter->tRegion = (xge_rect_t){-0.1f, -0.1f, 1.2f, 1.2f};
	if ( __xgeSvgAttrHasExactValue(pTag, pTagEnd, "filterUnits", "userSpaceOnUse") ) {
		pFilter->iFilterUnits = XGE_SVG_FILTER_UNITS_USER_SPACE;
	}
	if ( pFilter->iFilterUnits == XGE_SVG_FILTER_UNITS_OBJECT_BOUNDING_BOX ) {
		pFilter->tRegion.fX = __xgeSvgAttrLengthLastRefEx(pTag, pTagEnd, "x", 1.0f, XGE_SVG_DEFAULT_FONT_SIZE, -0.1f);
		pFilter->tRegion.fY = __xgeSvgAttrLengthLastRefEx(pTag, pTagEnd, "y", 1.0f, XGE_SVG_DEFAULT_FONT_SIZE, -0.1f);
		pFilter->tRegion.fW = __xgeSvgAttrLengthLastRefEx(pTag, pTagEnd, "width", 1.0f, XGE_SVG_DEFAULT_FONT_SIZE, 1.2f);
		pFilter->tRegion.fH = __xgeSvgAttrLengthLastRefEx(pTag, pTagEnd, "height", 1.0f, XGE_SVG_DEFAULT_FONT_SIZE, 1.2f);
	} else {
		float fRefX = __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_X);
		float fRefY = __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_Y);

		pFilter->tRegion.fX = __xgeSvgAttrLengthLastRefEx(
			pTag, pTagEnd, "x", fRefX, XGE_SVG_DEFAULT_FONT_SIZE, -0.1f
		);
		pFilter->tRegion.fY = __xgeSvgAttrLengthLastRefEx(
			pTag, pTagEnd, "y", fRefY, XGE_SVG_DEFAULT_FONT_SIZE, -0.1f
		);
		pFilter->tRegion.fW = __xgeSvgAttrLengthLastRefEx(
			pTag, pTagEnd, "width", fRefX, XGE_SVG_DEFAULT_FONT_SIZE, 1.2f
		);
		pFilter->tRegion.fH = __xgeSvgAttrLengthLastRefEx(
			pTag, pTagEnd, "height", fRefY, XGE_SVG_DEFAULT_FONT_SIZE, 1.2f
		);
	}
	if ( pFilter->tRegion.fW < 0.0f ) pFilter->tRegion.fW = 0.0f;
	if ( pFilter->tRegion.fH < 0.0f ) pFilter->tRegion.fH = 0.0f;
	if ( __xgeSvgAttrHasExactValue(pTag, pTagEnd, "primitiveUnits", "objectBoundingBox") ) {
		pFilter->iUnits = XGE_SVG_FILTER_UNITS_OBJECT_BOUNDING_BOX;
	}
	*pFilterIndex = iIndex;
	return XGE_OK;
}

static float __xgeSvgToFloatThorvg(const char* sText, char** ppEnd)
{
	char* pEnd;
	float fValue;

	if ( ppEnd != NULL ) *ppEnd = (char*)sText;
	if ( sText == NULL ) return 0.0f;
	fValue = strtof(sText, &pEnd);
	if ( (pEnd != sText) && ((*pEnd == 'e') || (*pEnd == 'E')) &&
	     ((pEnd[1] == 'm') || (pEnd[1] == 'M')) ) {
		pEnd += 2;
	}
	if ( ppEnd != NULL ) *ppEnd = pEnd;
	return fValue;
}

static int __xgeSvgParseGaussianBlurStdDeviation(const char* sText, float* pStdX, float* pStdY)
{
	const char* p;
	float arrValues[2];
	int iCount;

	if ( (sText == NULL) || (pStdX == NULL) || (pStdY == NULL) ) {
		return 0;
	}
	p = sText;
	arrValues[0] = 0.0f;
	arrValues[1] = 0.0f;
	iCount = 0;
	while ( (*p != '\0') && (iCount < 2) ) {
		char* pEnd;
		float fValue;

		__xgeSvgSkipSpaces(&p);
		if ( *p == ',' ) p++;
		/* ThorVG treats stdDeviation as bare numbers, not SVG lengths. */
		fValue = __xgeSvgToFloatThorvg(p, &pEnd);
		if ( fValue < 0.0f ) {
			break;
		}
		arrValues[iCount++] = fValue;
		p = (pEnd != NULL) ? pEnd : p;
	}
	if ( iCount <= 0 ) {
		*pStdX = 0.0f;
		*pStdY = 0.0f;
		return 0;
	}
	*pStdX = arrValues[0];
	*pStdY = (iCount == 1) ? arrValues[0] : arrValues[1];
	return 1;
}

static int __xgeSvgParseFilterFeGaussianBlur(
	xge_svg pSvg,
	int iFilterIndex,
	const char* pTag,
	const char* pTagEnd
)
{
	char sValue[XGE_SVG_ATTR_MAX];
	xge_svg_filter_t* pFilter;
	xge_svg_filter_primitive_t* pPrimitive;
	float fStdX;
	float fStdY;

	if ( !__xgeSvgValid(pSvg) || (iFilterIndex < 0) || (iFilterIndex >= pSvg->iFilterCount) ||
	     (pTag == NULL) || (pTagEnd == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pFilter = &pSvg->pFilters[iFilterIndex];
	pPrimitive = __xgeSvgFilterPrimitiveAppend(pFilter);
	if ( pPrimitive == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	fStdX = 0.0f;
	fStdY = 0.0f;
	if ( __xgeSvgAttrCopyLast(pTag, pTagEnd, "stdDeviation", sValue, sizeof(sValue)) ) {
		__xgeSvgParseGaussianBlurStdDeviation(sValue, &fStdX, &fStdY);
	}
	pPrimitive->fGaussianBlurX = fStdX;
	pPrimitive->fGaussianBlurY = fStdY;
	__xgeSvgFilterPrimitiveParseRegionAttrs(pSvg, pFilter, pPrimitive, pTag, pTagEnd);
	return XGE_OK;
}

static void __xgeSvgFilterPrimitiveBlurForBounds(
	const xge_svg_filter_t* pFilter,
	const xge_svg_filter_primitive_t* pPrimitive,
	xge_rect_t tBounds,
	float* pStdX,
	float* pStdY
)
{
	float fStdX;
	float fStdY;

	if ( (pStdX == NULL) || (pStdY == NULL) ) {
		return;
	}
	fStdX = (pPrimitive != NULL) ? pPrimitive->fGaussianBlurX : 0.0f;
	fStdY = (pPrimitive != NULL) ? pPrimitive->fGaussianBlurY : 0.0f;
	if ( (pFilter != NULL) && (pFilter->iUnits == XGE_SVG_FILTER_UNITS_OBJECT_BOUNDING_BOX) ) {
		fStdX *= tBounds.fW;
		fStdY *= tBounds.fH;
	}
	*pStdX = fStdX;
	*pStdY = fStdY;
}

static int __xgeSvgGaussianKernelInit(float fSigma, xge_svg_gaussian_kernel_t* pKernel)
{
	float fVariance;
	float fMiddle;
	int iLowerWidth;
	int iUpperWidth;
	int iLowerCount;
	int i;

	if ( pKernel == NULL ) {
		return 0;
	}
	memset(pKernel, 0, sizeof(*pKernel));
	if ( !isfinite(fSigma) || (fSigma <= XGE_SVG_EPSILON) ) {
		return 0;
	}
	fVariance = fSigma * fSigma;
	if ( !isfinite(fVariance) ) {
		return 0;
	}
	/* ThorVG uses two of Peter Kovesi's box passes for SVG quality 55. */
	pKernel->iLevel = 2;
	iLowerWidth = (int)sqrtf((12.0f * fVariance / 3.0f) + 1.0f);
	if ( (iLowerWidth & 1) == 0 ) {
		iLowerWidth--;
	}
	if ( iLowerWidth < 1 ) {
		iLowerWidth = 1;
	}
	iUpperWidth = iLowerWidth + 2;
	fMiddle = (12.0f * fVariance - 3.0f * (float)iLowerWidth * (float)iLowerWidth -
	           12.0f * (float)iLowerWidth - 9.0f) /
	          (-4.0f * (float)iLowerWidth - 4.0f);
	iLowerCount = (int)(fMiddle + 0.5f);
	for ( i = 0; i < pKernel->iLevel; i++ ) {
		int iWidth = (i < iLowerCount) ? iLowerWidth : iUpperWidth;

		pKernel->arrRadius[i] = (iWidth - 1) / 2;
		pKernel->iExtent += pKernel->arrRadius[i];
	}
	return pKernel->iExtent;
}

static int __xgeSvgFilterRegionForBounds(
	xge_svg pSvg,
	const char* sFilterId,
	xge_rect_t tBounds,
	xge_rect_t* pRegion
)
{
	xge_svg_filter_t* pFilter;
	xge_rect_t tRegion;
	int iFilter;

	if ( pRegion == NULL ) {
		return 0;
	}
	memset(pRegion, 0, sizeof(*pRegion));
	if ( !__xgeSvgValid(pSvg) || (sFilterId == NULL) || (sFilterId[0] == '\0') ) {
		return 0;
	}
	iFilter = __xgeSvgFilterFind(pSvg, sFilterId);
	if ( iFilter < 0 ) {
		return 0;
	}
	pFilter = &pSvg->pFilters[iFilter];
	tRegion = pFilter->tRegion;
	if ( pFilter->iFilterUnits == XGE_SVG_FILTER_UNITS_OBJECT_BOUNDING_BOX ) {
		tRegion = __xgeSvgObjectRectToBounds(tRegion, tBounds);
	}
	if ( (tRegion.fW <= 0.0f) || (tRegion.fH <= 0.0f) ) {
		return -1;
	}
	*pRegion = tRegion;
	return 1;
}

static int __xgeSvgFilterPrimitiveRegionForBounds(
	const xge_svg_filter_t* pFilter,
	const xge_svg_filter_primitive_t* pPrimitive,
	xge_rect_t tBounds,
	xge_rect_t tFallback,
	xge_rect_t* pRegion
)
{
	xge_rect_t tRegion;

	if ( (pFilter == NULL) || (pPrimitive == NULL) || (pRegion == NULL) ) {
		return 0;
	}
	tRegion = tFallback;
	if ( pPrimitive->iRegionFlags & XGE_SVG_FILTER_PRIMITIVE_REGION_X ) {
		tRegion.fX = ((pFilter->iUnits == XGE_SVG_FILTER_UNITS_OBJECT_BOUNDING_BOX) &&
		              (pPrimitive->iRegionPercentFlags & XGE_SVG_FILTER_PRIMITIVE_REGION_X)) ?
			(tBounds.fX + pPrimitive->tRegion.fX * tBounds.fW) : pPrimitive->tRegion.fX;
	}
	if ( pPrimitive->iRegionFlags & XGE_SVG_FILTER_PRIMITIVE_REGION_Y ) {
		tRegion.fY = ((pFilter->iUnits == XGE_SVG_FILTER_UNITS_OBJECT_BOUNDING_BOX) &&
		              (pPrimitive->iRegionPercentFlags & XGE_SVG_FILTER_PRIMITIVE_REGION_Y)) ?
			(tBounds.fY + pPrimitive->tRegion.fY * tBounds.fH) : pPrimitive->tRegion.fY;
	}
	if ( pPrimitive->iRegionFlags & XGE_SVG_FILTER_PRIMITIVE_REGION_W ) {
		tRegion.fW = ((pFilter->iUnits == XGE_SVG_FILTER_UNITS_OBJECT_BOUNDING_BOX) &&
		              (pPrimitive->iRegionPercentFlags & XGE_SVG_FILTER_PRIMITIVE_REGION_W)) ?
			(pPrimitive->tRegion.fW * tBounds.fW) : pPrimitive->tRegion.fW;
	}
	if ( pPrimitive->iRegionFlags & XGE_SVG_FILTER_PRIMITIVE_REGION_H ) {
		tRegion.fH = ((pFilter->iUnits == XGE_SVG_FILTER_UNITS_OBJECT_BOUNDING_BOX) &&
		              (pPrimitive->iRegionPercentFlags & XGE_SVG_FILTER_PRIMITIVE_REGION_H)) ?
			(pPrimitive->tRegion.fH * tBounds.fH) : pPrimitive->tRegion.fH;
	}
	if ( (tRegion.fW <= 0.0f) || (tRegion.fH <= 0.0f) ) {
		memset(pRegion, 0, sizeof(*pRegion));
		return -1;
	}
	*pRegion = tRegion;
	return 1;
}

static int __xgeSvgFilterGaussianBlurForBounds(
	xge_svg pSvg,
	const char* sFilterId,
	xge_rect_t tBounds,
	float* pStdX,
	float* pStdY,
	xge_rect_t* pOutRegion
)
{
	xge_svg_filter_t* pFilter;
	xge_rect_t tOutRegion;
	float fVarianceX;
	float fVarianceY;
	int iFilter;
	int i;
	int bActive;

	if ( (pStdX == NULL) || (pStdY == NULL) ) {
		return 0;
	}
	*pStdX = 0.0f;
	*pStdY = 0.0f;
	if ( pOutRegion != NULL ) {
		memset(pOutRegion, 0, sizeof(*pOutRegion));
	}
	if ( !__xgeSvgValid(pSvg) || (sFilterId == NULL) || (sFilterId[0] == '\0') ) {
		return 0;
	}
	iFilter = __xgeSvgFilterFind(pSvg, sFilterId);
	if ( iFilter < 0 ) {
		return 0;
	}
	pFilter = &pSvg->pFilters[iFilter];
	if ( pFilter->iPrimitiveCount <= 0 ) {
		return 0;
	}
	tOutRegion = pFilter->tRegion;
	if ( pFilter->iFilterUnits == XGE_SVG_FILTER_UNITS_OBJECT_BOUNDING_BOX ) {
		tOutRegion = __xgeSvgObjectRectToBounds(tOutRegion, tBounds);
	}
	if ( (tOutRegion.fW <= 0.0f) || (tOutRegion.fH <= 0.0f) ) {
		return 0;
	}
	fVarianceX = 0.0f;
	fVarianceY = 0.0f;
	bActive = 0;
	for ( i = 0; i < pFilter->iPrimitiveCount; i++ ) {
		xge_svg_filter_primitive_t* pPrimitive;
		float fStdX;
		float fStdY;
		int bPrimitiveActive;

		pPrimitive = &pFilter->pPrimitives[i];
		__xgeSvgFilterPrimitiveBlurForBounds(pFilter, pPrimitive, tBounds, &fStdX, &fStdY);
		bPrimitiveActive = 0;
		if ( fStdX > 0.0001f ) {
			fVarianceX += fStdX * fStdX;
			bActive = 1;
			bPrimitiveActive = 1;
		}
		if ( fStdY > 0.0001f ) {
			fVarianceY += fStdY * fStdY;
			bActive = 1;
			bPrimitiveActive = 1;
		}
		if ( bPrimitiveActive && (pPrimitive->iRegionFlags != 0) ) {
			xge_rect_t tPrimitiveRegion;

			if ( __xgeSvgFilterPrimitiveRegionForBounds(
				pFilter, pPrimitive, tBounds, tOutRegion, &tPrimitiveRegion
			) < 0 ) {
				tOutRegion.fW = 0.0f;
				tOutRegion.fH = 0.0f;
			} else {
				tOutRegion = __xgeSvgRectIntersect(tOutRegion, tPrimitiveRegion);
			}
		}
	}
	*pStdX = sqrtf(fVarianceX);
	*pStdY = sqrtf(fVarianceY);
	if ( pOutRegion != NULL ) {
		*pOutRegion = tOutRegion;
	}
	if ( (tOutRegion.fW <= 0.0f) || (tOutRegion.fH <= 0.0f) ) {
		return 1;
	}
	return bActive ? 2 : 1;
}

static xge_rect_t __xgeSvgFilterEffectExpandedBounds(
	xge_svg pSvg,
	const xge_svg_filter_effect_t* pEffect
)
{
	xge_rect_t tBounds;
	xge_svg_filter_t* pFilter;
	int i;

	memset(&tBounds, 0, sizeof(tBounds));
	if ( !__xgeSvgValid(pSvg) || (pEffect == NULL) ||
	     (pEffect->iFilterIndex < 0) || (pEffect->iFilterIndex >= pSvg->iFilterCount) ) {
		return tBounds;
	}
	tBounds = pEffect->tObjectBounds;
	pFilter = &pSvg->pFilters[pEffect->iFilterIndex];
	for ( i = 0; i < pFilter->iPrimitiveCount; i++ ) {
		float fStdX;
		float fStdY;
		float fExpandX;
		float fExpandY;
		float fScaleX = fabsf(pEffect->tTransform.fA);
		float fScaleY = fabsf(pEffect->tTransform.fD);
		xge_svg_gaussian_kernel_t tKernel;

		__xgeSvgFilterPrimitiveBlurForBounds(
			pFilter, &pFilter->pPrimitives[i], pEffect->tObjectBounds, &fStdX, &fStdY
		);
		fExpandX = 0.0f;
		fExpandY = 0.0f;
		if ( (fStdX > XGE_SVG_EPSILON) && (fStdY > XGE_SVG_EPSILON) ) {
			if ( __xgeSvgGaussianKernelInit(1.25f * fStdX * fScaleX, &tKernel) > 0 ) {
				fExpandX = (float)tKernel.iExtent;
				fExpandY = (float)tKernel.iExtent;
			}
		} else if ( fStdX > XGE_SVG_EPSILON ) {
			if ( __xgeSvgGaussianKernelInit(1.25f * fStdX * fScaleX, &tKernel) > 0 ) {
				fExpandX = (float)tKernel.iExtent;
			}
		} else if ( fStdY > XGE_SVG_EPSILON ) {
			if ( __xgeSvgGaussianKernelInit(1.25f * fStdY * fScaleY, &tKernel) > 0 ) {
				fExpandY = (float)tKernel.iExtent;
			}
		}
		if ( fExpandX > 0.0f ) {
			tBounds.fX -= fExpandX;
			tBounds.fW += fExpandX * 2.0f;
		}
		if ( fExpandY > 0.0f ) {
			tBounds.fY -= fExpandY;
			tBounds.fH += fExpandY * 2.0f;
		}
	}
	return tBounds;
}

static xge_rect_t __xgeSvgFilterEffectVisualBounds(
	xge_svg pSvg,
	const xge_svg_filter_effect_t* pEffect
)
{
	xge_rect_t tBounds = __xgeSvgFilterEffectExpandedBounds(pSvg, pEffect);

	if ( pEffect == NULL ) {
		memset(&tBounds, 0, sizeof(tBounds));
		return tBounds;
	}
	return __xgeSvgRectIntersect(tBounds, pEffect->tOutputRegion);
}

static xge_rect_t __xgeSvgFilterEffectProcessRegion(
	xge_svg pSvg,
	const xge_svg_filter_effect_t* pEffect
)
{
	xge_rect_t tExpanded = __xgeSvgFilterEffectExpandedBounds(pSvg, pEffect);

	if ( pEffect == NULL ) {
		memset(&tExpanded, 0, sizeof(tExpanded));
		return tExpanded;
	}
	return __xgeSvgRectUnion(tExpanded, pEffect->tOutputRegion);
}

static int __xgeSvgHexNibble(char c)
{
	if ( c >= '0' && c <= '9' ) return c - '0';
	if ( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
	if ( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
	return -1;
}

static int __xgeSvgIsSpace(int c)
{
	return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n') || (c == '\f');
}

static int __xgeSvgIsHexChar(int c)
{
	return ((c >= '0') && (c <= '9')) ||
	       ((c >= 'a') && (c <= 'f')) ||
	       ((c >= 'A') && (c <= 'F'));
}

static void __xgeSvgSkipSpaces(const char** ppText)
{
	if ( ppText == NULL ) {
		return;
	}
	while ( __xgeSvgIsSpace((unsigned char)**ppText) ) {
		(*ppText)++;
	}
}

static uint32_t __xgeSvgColorAlpha(uint32_t iColor, float fAlpha)
{
	int iA;

	if ( fAlpha < 0.0f ) fAlpha = 0.0f;
	if ( fAlpha > 1.0f ) fAlpha = 1.0f;
	iA = (int)((float)XGE_COLOR_GET_A(iColor) * fAlpha + 0.5f);
	if ( iA < 0 ) iA = 0;
	if ( iA > 255 ) iA = 255;
	return XGE_COLOR_RGBA(XGE_COLOR_GET_R(iColor), XGE_COLOR_GET_G(iColor), XGE_COLOR_GET_B(iColor), iA);
}

static int __xgeSvgStyleHasPaintAlpha(const xge_svg_style_t* pStyle, int bStroke)
{
	if ( pStyle == NULL ) {
		return 0;
	}
	if ( bStroke ) {
		if ( (pStyle->fStrokeWidth <= 0.0f) || (pStyle->fStrokeOpacity <= 0.0f) ) {
			return 0;
		}
		return (pStyle->sStrokeGradientId[0] != '\0') || (XGE_COLOR_GET_A(pStyle->iStrokeColor) > 0);
	}
	if ( pStyle->fFillOpacity <= 0.0f ) {
		return 0;
	}
	return (pStyle->sFillGradientId[0] != '\0') || (XGE_COLOR_GET_A(pStyle->iFillColor) > 0);
}

static void __xgeSvgApplyShapeStyle(xge_svg pSvg, xge_shape_ex pShape, const xge_svg_style_t* pStyle);
static int __xgeSvgShapeClipRectIntersect(xge_shape_ex pShape, xge_rect_t tClip);
static int __xgeSvgAddStyledShape(xge_svg pSvg, xge_shape_ex pShape, const xge_svg_style_t* pStyle, int iDefIndex);
static int __xgeSvgApplyViewportClipToShape(xge_shape_ex pShape, const xge_svg_style_t* pStyle);
static xge_svg_clip_path_t* __xgeSvgClipPathGetById(xge_svg pSvg, const char* sClipId);
static int __xgeSvgApplyClipPath(xge_svg pSvg, xge_shape_ex pShape, const char* sClipId);
static int __xgeSvgApplyUseInstance(xge_svg pSvg, int iDefIndex, const xge_svg_style_t* pUseStyle, int iTargetDef, float fX, float fY, float fW, float fH, int bHasWidth, int bHasHeight, xge_shape_ex_matrix_t* pLogicalTransform);
static int __xgeSvgApplyUseInstanceWithAspect(xge_svg pSvg, int iDefIndex, const xge_svg_style_t* pUseStyle, int iTargetDef, float fX, float fY, float fW, float fH, int bHasWidth, int bHasHeight, const char* sAspect);
static int __xgeSvgAddHrefImage(xge_svg pSvg, const xge_svg_style_t* pStyle, const char* sHref, xge_rect_t tRect, const char* sAspect, int iDefIndex);

typedef struct xge_svg_color_name_t {
	const char* sName;
	unsigned char r;
	unsigned char g;
	unsigned char b;
} xge_svg_color_name_t;

static int __xgeSvgAsciiLower(int c)
{
	if ( (c >= 'A') && (c <= 'Z') ) {
		return c + ('a' - 'A');
	}
	return c;
}

static int __xgeSvgColorNameEquals(const char* sText, const char* sName)
{
	const char* pEnd;

	if ( (sText == NULL) || (sName == NULL) ) {
		return 0;
	}
	while ( (*sText == ' ') || (*sText == '\t') || (*sText == '\r') || (*sText == '\n') ) {
		sText++;
	}
	pEnd = sText + strlen(sText);
	while ( (pEnd > sText) && ((pEnd[-1] == ' ') || (pEnd[-1] == '\t') || (pEnd[-1] == '\r') || (pEnd[-1] == '\n')) ) {
		pEnd--;
	}
	while ( (sText < pEnd) && (*sName != '\0') ) {
		if ( __xgeSvgAsciiLower((unsigned char)*sText) != __xgeSvgAsciiLower((unsigned char)*sName) ) {
			return 0;
		}
		sText++;
		sName++;
	}
	return (sText == pEnd) && (*sName == '\0');
}

static int __xgeSvgAsciiStartsWithNoCase(const char* sText, const char* sPrefix)
{
	if ( (sText == NULL) || (sPrefix == NULL) ) {
		return 0;
	}
	while ( *sPrefix != '\0' ) {
		if ( __xgeSvgAsciiLower((unsigned char)*sText) != __xgeSvgAsciiLower((unsigned char)*sPrefix) ) {
			return 0;
		}
		sText++;
		sPrefix++;
	}
	return 1;
}

static int __xgeSvgParseNamedColor(const char* sText, uint32_t* pColor)
{
	static const xge_svg_color_name_t sColors[] = {
		{"aliceblue", 240, 248, 255}, {"antiquewhite", 250, 235, 215}, {"aqua", 0, 255, 255}, {"aquamarine", 127, 255, 212},
		{"azure", 240, 255, 255}, {"beige", 245, 245, 220}, {"bisque", 255, 228, 196}, {"black", 0, 0, 0},
		{"blanchedalmond", 255, 235, 205}, {"blue", 0, 0, 255}, {"blueviolet", 138, 43, 226}, {"brown", 165, 42, 42},
		{"burlywood", 222, 184, 135}, {"cadetblue", 95, 158, 160}, {"chartreuse", 127, 255, 0}, {"chocolate", 210, 105, 30},
		{"coral", 255, 127, 80}, {"cornflowerblue", 100, 149, 237}, {"cornsilk", 255, 248, 220}, {"crimson", 220, 20, 60},
		{"cyan", 0, 255, 255}, {"darkblue", 0, 0, 139}, {"darkcyan", 0, 139, 139}, {"darkgoldenrod", 184, 134, 11},
		{"darkgray", 169, 169, 169}, {"darkgreen", 0, 100, 0}, {"darkgrey", 169, 169, 169}, {"darkkhaki", 189, 183, 107},
		{"darkmagenta", 139, 0, 139}, {"darkolivegreen", 85, 107, 47}, {"darkorange", 255, 140, 0}, {"darkorchid", 153, 50, 204},
		{"darkred", 139, 0, 0}, {"darksalmon", 233, 150, 122}, {"darkseagreen", 143, 188, 143}, {"darkslateblue", 72, 61, 139},
		{"darkslategray", 47, 79, 79}, {"darkslategrey", 47, 79, 79}, {"darkturquoise", 0, 206, 209}, {"darkviolet", 148, 0, 211},
		{"deeppink", 255, 20, 147}, {"deepskyblue", 0, 191, 255}, {"dimgray", 105, 105, 105}, {"dimgrey", 105, 105, 105},
		{"dodgerblue", 30, 144, 255}, {"firebrick", 178, 34, 34}, {"floralwhite", 255, 250, 240}, {"forestgreen", 34, 139, 34},
		{"fuchsia", 255, 0, 255}, {"gainsboro", 220, 220, 220}, {"ghostwhite", 248, 248, 255}, {"gold", 255, 215, 0},
		{"goldenrod", 218, 165, 32}, {"gray", 128, 128, 128}, {"green", 0, 128, 0}, {"greenyellow", 173, 255, 47},
		{"grey", 128, 128, 128}, {"honeydew", 240, 255, 240}, {"hotpink", 255, 105, 180}, {"indianred", 205, 92, 92},
		{"indigo", 75, 0, 130}, {"ivory", 255, 255, 240}, {"khaki", 240, 230, 140}, {"lavender", 230, 230, 250},
		{"lavenderblush", 255, 240, 245}, {"lawngreen", 124, 252, 0}, {"lemonchiffon", 255, 250, 205}, {"lightblue", 173, 216, 230},
		{"lightcoral", 240, 128, 128}, {"lightcyan", 224, 255, 255}, {"lightgoldenrodyellow", 250, 250, 210}, {"lightgray", 211, 211, 211},
		{"lightgreen", 144, 238, 144}, {"lightgrey", 211, 211, 211}, {"lightpink", 255, 182, 193}, {"lightsalmon", 255, 160, 122},
		{"lightseagreen", 32, 178, 170}, {"lightskyblue", 135, 206, 250}, {"lightslategray", 119, 136, 153}, {"lightslategrey", 119, 136, 153},
		{"lightsteelblue", 176, 196, 222}, {"lightyellow", 255, 255, 224}, {"lime", 0, 255, 0}, {"limegreen", 50, 205, 50},
		{"linen", 250, 240, 230}, {"magenta", 255, 0, 255}, {"maroon", 128, 0, 0}, {"mediumaquamarine", 102, 205, 170},
		{"mediumblue", 0, 0, 205}, {"mediumorchid", 186, 85, 211}, {"mediumpurple", 147, 112, 219}, {"mediumseagreen", 60, 179, 113},
		{"mediumslateblue", 123, 104, 238}, {"mediumspringgreen", 0, 250, 154}, {"mediumturquoise", 72, 209, 204}, {"mediumvioletred", 199, 21, 133},
		{"midnightblue", 25, 25, 112}, {"mintcream", 245, 255, 250}, {"mistyrose", 255, 228, 225}, {"moccasin", 255, 228, 181},
		{"navajowhite", 255, 222, 173}, {"navy", 0, 0, 128}, {"oldlace", 253, 245, 230}, {"olive", 128, 128, 0},
		{"olivedrab", 107, 142, 35}, {"orange", 255, 165, 0}, {"orangered", 255, 69, 0}, {"orchid", 218, 112, 214},
		{"palegoldenrod", 238, 232, 170}, {"palegreen", 152, 251, 152}, {"paleturquoise", 175, 238, 238}, {"palevioletred", 219, 112, 147},
		{"papayawhip", 255, 239, 213}, {"peachpuff", 255, 218, 185}, {"peru", 205, 133, 63}, {"pink", 255, 192, 203},
		{"plum", 221, 160, 221}, {"powderblue", 176, 224, 230}, {"purple", 128, 0, 128}, {"red", 255, 0, 0},
		{"rosybrown", 188, 143, 143}, {"royalblue", 65, 105, 225}, {"saddlebrown", 139, 69, 19}, {"salmon", 250, 128, 114},
		{"sandybrown", 244, 164, 96}, {"seagreen", 46, 139, 87}, {"seashell", 255, 245, 238}, {"sienna", 160, 82, 45},
		{"silver", 192, 192, 192}, {"skyblue", 135, 206, 235}, {"slateblue", 106, 90, 205}, {"slategray", 112, 128, 144},
		{"slategrey", 112, 128, 144}, {"snow", 255, 250, 250}, {"springgreen", 0, 255, 127}, {"steelblue", 70, 130, 180},
		{"tan", 210, 180, 140}, {"teal", 0, 128, 128}, {"thistle", 216, 191, 216}, {"tomato", 255, 99, 71},
		{"turquoise", 64, 224, 208}, {"violet", 238, 130, 238}, {"wheat", 245, 222, 179}, {"white", 255, 255, 255},
		{"whitesmoke", 245, 245, 245}, {"yellow", 255, 255, 0}, {"yellowgreen", 154, 205, 50}
	};
	int i;

	if ( (sText == NULL) || (pColor == NULL) ) {
		return 0;
	}
	if ( __xgeSvgColorNameEquals(sText, "transparent") ) {
		*pColor = XGE_COLOR_RGBA(0, 0, 0, 0);
		return 1;
	}
	for ( i = 0; i < (int)(sizeof(sColors) / sizeof(sColors[0])); i++ ) {
		if ( __xgeSvgColorNameEquals(sText, sColors[i].sName) ) {
			*pColor = XGE_COLOR_RGBA(sColors[i].r, sColors[i].g, sColors[i].b, 255);
			return 1;
		}
	}
	return 0;
}

static int __xgeSvgColorByteFromDouble(double fValue, int bPercent, int bAlpha)
{
	double fByte;
	int iByte;

	if ( !__xgeSvgDoubleIsFiniteFloat(fValue) ) {
		return -1;
	}
	if ( bAlpha ) {
		if ( bPercent ) {
			fByte = fValue * 255.0 / 100.0;
		} else if ( fValue <= 1.0 ) {
			fByte = fValue * 255.0;
		} else {
			fByte = fValue;
		}
	} else {
		if ( bPercent ) {
			fByte = fValue * 255.0 / 100.0;
		} else {
			fByte = fValue;
		}
	}
	if ( fByte < 0.0 ) fByte = 0.0;
	if ( fByte > 255.0 ) fByte = 255.0;
	iByte = (int)(fByte + 0.5);
	if ( iByte < 0 ) iByte = 0;
	if ( iByte > 255 ) iByte = 255;
	return iByte;
}

static int __xgeSvgParseThorvgColorComponent(const char** ppText, int* pValue)
{
	const char* p;
	const char* pEnd;
	double fValue;
	double fByte;
	int bPercent;

	if ( (ppText == NULL) || (*ppText == NULL) || (pValue == NULL) ) {
		return 0;
	}
	p = *ppText;
	__xgeSvgSkipSpaces(&p);
	if ( !__xgeSvgParseNumberAt(p, &pEnd, &fValue) || !__xgeSvgDoubleIsFiniteFloat(fValue) ) {
		return 0;
	}
	p = pEnd;
	bPercent = 0;
	if ( *p == '%' ) {
		bPercent = 1;
		p++;
	}
	fByte = bPercent ? (255.0 * fValue / 100.0) : fValue;
	if ( (fByte < 0.0) || (fByte > 255.0) ) {
		return 0;
	}
	__xgeSvgSkipSpaces(&p);
	*pValue = (int)floor(fByte + 0.5);
	*ppText = p;
	return 1;
}

static int __xgeSvgRgbFunctionRecognized(const char* sText)
{
	size_t iLen;

	if ( sText == NULL ) {
		return 0;
	}
	iLen = strlen(sText);
	return (iLen >= 10u) &&
	       (__xgeSvgAsciiLower((unsigned char)sText[0]) == 'r') &&
	       (__xgeSvgAsciiLower((unsigned char)sText[1]) == 'g') &&
	       (__xgeSvgAsciiLower((unsigned char)sText[2]) == 'b') &&
	       (sText[3] == '(') && (sText[iLen - 1u] == ')');
}

static int __xgeSvgParseRgbColorComponents(const char* sText, uint32_t* pColor)
{
	const char* p;
	int r;
	int g;
	int b;

	if ( !__xgeSvgRgbFunctionRecognized(sText) ) {
		return 0;
	}
	p = sText + 4;
	if ( !__xgeSvgParseThorvgColorComponent(&p, &r) || (*p != ',') ) return 0;
	p++;
	if ( !__xgeSvgParseThorvgColorComponent(&p, &g) || (*p != ',') ) return 0;
	p++;
	if ( !__xgeSvgParseThorvgColorComponent(&p, &b) || (*p != ')') || (p[1] != '\0') ) return 0;
	if ( pColor != NULL ) {
		*pColor = XGE_COLOR_RGBA(r, g, b, 255);
	}
	return 1;
}

static int __xgeSvgParseRgbColor(const char* sText, uint32_t* pColor)
{
	uint32_t iPrevious;
	uint32_t iParsed;

	if ( (pColor == NULL) || !__xgeSvgRgbFunctionRecognized(sText) ) {
		return 0;
	}
	iPrevious = *pColor;
	*pColor = XGE_COLOR_RGBA((iPrevious >> 24) & 0xffu, (iPrevious >> 16) & 0xffu, (iPrevious >> 8) & 0xffu, 255);
	if ( __xgeSvgParseRgbColorComponents(sText, &iParsed) ) {
		*pColor = iParsed;
	}
	return 1;
}

static void __xgeSvgHslToRgbThorvg(float h, float s, float l, uint32_t* pColor)
{
	float v;
	float p;
	float sv;
	float f;
	float vsf;
	float t;
	float q;
	float r;
	float g;
	float b;
	unsigned char i;

	if ( pColor == NULL ) {
		return;
	}
	if ( fabsf(s) <= XGE_SVG_EPSILON ) {
		int c = (int)lrint((double)l * 255.0);
		*pColor = XGE_COLOR_RGBA(c, c, c, 255);
		return;
	}
	if ( fabsf(h - 360.0f) <= XGE_SVG_EPSILON ) {
		h = 0.0f;
	} else {
		h = fmodf(h, 360.0f);
		if ( h < 0.0f ) h += 360.0f;
		h /= 60.0f;
	}
	v = (l <= 0.5f) ? (l * (1.0f + s)) : (l + s - (l * s));
	p = l + l - v;
	sv = (fabsf(v) <= XGE_SVG_EPSILON) ? 0.0f : (v - p) / v;
	i = (unsigned char)h;
	f = h - (float)i;
	vsf = v * sv * f;
	t = p + vsf;
	q = v - vsf;
	switch ( i ) {
		case 0: r = v; g = t; b = p; break;
		case 1: r = q; g = v; b = p; break;
		case 2: r = p; g = v; b = t; break;
		case 3: r = p; g = q; b = v; break;
		case 4: r = t; g = p; b = v; break;
		case 5: r = v; g = p; b = q; break;
		default: r = 0.0f; g = 0.0f; b = 0.0f; break;
	}
	*pColor = XGE_COLOR_RGBA(
		(int)lrint((double)r * 255.0),
		(int)lrint((double)g * 255.0),
		(int)lrint((double)b * 255.0),
		255);
}

static int __xgeSvgParseThorvgHslComponent(const char** ppText, float* pValue)
{
	const char* p;
	const char* pEnd;
	double fValue;
	int bPercent;

	if ( (ppText == NULL) || (*ppText == NULL) || (pValue == NULL) ) {
		return 0;
	}
	p = *ppText;
	__xgeSvgSkipSpaces(&p);
	if ( !__xgeSvgParseNumberAt(p, &pEnd, &fValue) ) {
		return 0;
	}
	p = pEnd;
	bPercent = 0;
	if ( *p == '%' ) {
		bPercent = 1;
		p++;
	}
	if ( !bPercent ) {
		return 0;
	}
	fValue /= 100.0;
	if ( fValue < 0.0 ) fValue = 0.0;
	if ( fValue > 1.0 ) fValue = 1.0;
	*pValue = (float)fValue;
	*ppText = p;
	return 1;
}

static int __xgeSvgParseHslColor(const char* sText, uint32_t* pColor)
{
	const char* p;
	const char* pEnd;
	size_t iLen;
	double fHue;
	float s;
	float l;

	if ( (sText == NULL) || (pColor == NULL) ) {
		return 0;
	}
	iLen = strlen(sText);
	if ( (iLen < 10u) ||
	     (__xgeSvgAsciiLower((unsigned char)sText[0]) != 'h') ||
	     (__xgeSvgAsciiLower((unsigned char)sText[1]) != 's') ||
	     (__xgeSvgAsciiLower((unsigned char)sText[2]) != 'l') ||
	     (sText[3] != '(') || (sText[iLen - 1u] != ')') ) {
		return 0;
	}
	p = sText + 4;
	__xgeSvgSkipSpaces(&p);
	if ( !__xgeSvgParseNumberAt(p, &pEnd, &fHue) || !__xgeSvgDoubleIsFiniteFloat(fHue) ) return 0;
	p = pEnd;
	__xgeSvgSkipSpaces(&p);
	if ( *p == ',' ) {
		p++;
	}
	__xgeSvgSkipSpaces(&p);
	if ( !__xgeSvgParseThorvgHslComponent(&p, &s) ) return 0;
	__xgeSvgSkipSpaces(&p);
	if ( *p == ',' ) {
		p++;
	}
	__xgeSvgSkipSpaces(&p);
	if ( !__xgeSvgParseThorvgHslComponent(&p, &l) || (*p != ')') || (p[1] != '\0') ) return 0;
	__xgeSvgHslToRgbThorvg((float)fHue, s, l, pColor);
	return 1;
}

static int __xgeSvgParseHexColor(const char* sText, uint32_t* pColor)
{
	int iNibbles[8];
	const char* p;
	int iCount;
	int r;
	int g;
	int b;
	int a;

	if ( (sText == NULL) || (pColor == NULL) || (sText[0] != '#') ) {
		return 0;
	}
	p = sText + 1;
	iCount = 0;
	while ( __xgeSvgIsHexChar((unsigned char)*p) && (iCount < 8) ) {
		iNibbles[iCount++] = __xgeSvgHexNibble(*p);
		p++;
	}
	if ( __xgeSvgIsHexChar((unsigned char)*p) ) {
		return 0;
	}
	__xgeSvgSkipSpaces(&p);
	if ( *p != '\0' ) {
		return 0;
	}
	if ( iCount == 3 || iCount == 4 ) {
		r = (iNibbles[0] << 4) | iNibbles[0];
		g = (iNibbles[1] << 4) | iNibbles[1];
		b = (iNibbles[2] << 4) | iNibbles[2];
		a = (iCount == 4) ? ((iNibbles[3] << 4) | iNibbles[3]) : 255;
		*pColor = XGE_COLOR_RGBA(r, g, b, a);
		return 1;
	}
	if ( iCount == 6 || iCount == 8 ) {
		r = (iNibbles[0] << 4) | iNibbles[1];
		g = (iNibbles[2] << 4) | iNibbles[3];
		b = (iNibbles[4] << 4) | iNibbles[5];
		a = (iCount == 8) ? ((iNibbles[6] << 4) | iNibbles[7]) : 255;
		*pColor = XGE_COLOR_RGBA(r, g, b, a);
		return 1;
	}
	return 0;
}

static int __xgeSvgParseColorThorvg(const char* sText, uint32_t* pColor)
{
	size_t iLen;
	int i;

	if ( (sText == NULL) || (pColor == NULL) ) {
		return 0;
	}
	iLen = strlen(sText);
	if ( iLen == 0u || __xgeSvgIsSpace((unsigned char)sText[0]) || __xgeSvgIsSpace((unsigned char)sText[iLen - 1u]) ) {
		return 0;
	}
	if ( (iLen == 4u || iLen == 7u) && (sText[0] == '#') ) {
		for ( i = 1; i < (int)iLen; i++ ) {
			if ( !__xgeSvgIsHexChar((unsigned char)sText[i]) ) {
				return 1;
			}
		}
		(void)__xgeSvgParseHexColor(sText, pColor);
		return 1;
	}
	if ( __xgeSvgRgbFunctionRecognized(sText) ) {
		return __xgeSvgParseRgbColor(sText, pColor);
	}
	if ( __xgeSvgParseHslColor(sText, pColor) ) {
		return 1;
	}
	if ( __xgeSvgColorNameEquals(sText, "transparent") ) {
		return 0;
	}
	return __xgeSvgParseNamedColor(sText, pColor);
}

static int __xgeSvgParseColor(const char* sText, uint32_t* pColor)
{
	if ( (sText == NULL) || (pColor == NULL) ) {
		return 0;
	}
	__xgeSvgSkipSpaces(&sText);
	if ( __xgeSvgColorNameEquals(sText, "none") ) {
		*pColor = XGE_COLOR_RGBA(0, 0, 0, 0);
		return 1;
	}
	if ( sText[0] == '#' ) {
		return __xgeSvgParseHexColor(sText, pColor);
	}
	if ( __xgeSvgParseRgbColor(sText, pColor) ) {
		return 1;
	}
	if ( __xgeSvgParseHslColor(sText, pColor) ) {
		return 1;
	}
	return __xgeSvgParseNamedColor(sText, pColor);
}

static int __xgeSvgParseFourFloats(const char* sText, float* pA, float* pB, float* pC, float* pD)
{
	const char* p;
	const char* pEnd;
	double fValue;
	float* arrValues[4];
	int i;

	if ( (sText == NULL) || (pA == NULL) || (pB == NULL) || (pC == NULL) || (pD == NULL) ) {
		return 0;
	}
	arrValues[0] = pA;
	arrValues[1] = pB;
	arrValues[2] = pC;
	arrValues[3] = pD;
	p = sText;
	for ( i = 0; i < 4; i++ ) {
		while ( __xgeSvgIsSpace((unsigned char)*p) || (*p == ',') ) p++;
		if ( !__xgeSvgParseNumberAt(p, &pEnd, &fValue) ) {
			return 0;
		}
		if ( !__xgeSvgDoubleToFloat(fValue, arrValues[i]) ) {
			return 0;
		}
		p = pEnd;
	}
	while ( __xgeSvgIsSpace((unsigned char)*p) ) p++;
	if ( *p != '\0' ) {
		return 0;
	}
	return 1;
}

static xge_shape_ex_matrix_t __xgeSvgMatrixIdentity(void)
{
	xge_shape_ex_matrix_t tMatrix;

	if ( xgeShapeExMatrixIdentity(&tMatrix) != XGE_OK ) {
		memset(&tMatrix, 0, sizeof(tMatrix));
		tMatrix.fA = 1.0f;
		tMatrix.fD = 1.0f;
	}
	return tMatrix;
}

static xge_shape_ex_matrix_t __xgeSvgMatrixTranslate(float fX, float fY)
{
	xge_shape_ex_matrix_t tMatrix;

	if ( xgeShapeExMatrixTranslate(&tMatrix, fX, fY) != XGE_OK ) {
		return __xgeSvgMatrixIdentity();
	}
	return tMatrix;
}

static xge_shape_ex_matrix_t __xgeSvgMatrixScale(float fX, float fY)
{
	xge_shape_ex_matrix_t tMatrix;

	if ( xgeShapeExMatrixScale(&tMatrix, fX, fY) != XGE_OK ) {
		return __xgeSvgMatrixIdentity();
	}
	return tMatrix;
}

static xge_shape_ex_matrix_t __xgeSvgMatrixRotate(float fDegrees)
{
	xge_shape_ex_matrix_t tMatrix;
	float fRadians = fDegrees * (3.14159265358979323846f / 180.0f);

	if ( xgeShapeExMatrixRotate(&tMatrix, fRadians) != XGE_OK ) {
		return __xgeSvgMatrixIdentity();
	}
	return tMatrix;
}

static xge_shape_ex_matrix_t __xgeSvgMatrixSkewX(float fDegrees)
{
	xge_shape_ex_matrix_t tMatrix;
	float fRadians = fDegrees * (3.14159265358979323846f / 180.0f);

	if ( xgeShapeExMatrixSkew(&tMatrix, fRadians, 0.0f) != XGE_OK ) {
		return __xgeSvgMatrixIdentity();
	}
	return tMatrix;
}

static xge_shape_ex_matrix_t __xgeSvgMatrixSkewY(float fDegrees)
{
	xge_shape_ex_matrix_t tMatrix;
	float fRadians = fDegrees * (3.14159265358979323846f / 180.0f);

	if ( xgeShapeExMatrixSkew(&tMatrix, 0.0f, fRadians) != XGE_OK ) {
		return __xgeSvgMatrixIdentity();
	}
	return tMatrix;
}

static xge_shape_ex_matrix_t __xgeSvgMatrixMul(xge_shape_ex_matrix_t tParent, xge_shape_ex_matrix_t tLocal)
{
	xge_shape_ex_matrix_t tOut = tParent;

	if ( xgeShapeExMatrixMultiply(&tOut, &tParent, &tLocal) != XGE_OK ) {
		return __xgeSvgMatrixIdentity();
	}
	return tOut;
}

static int __xgeSvgMatrixInvert(xge_shape_ex_matrix_t tMatrix, xge_shape_ex_matrix_t* pOut)
{
	return xgeShapeExMatrixInvert(pOut, &tMatrix) == XGE_OK;
}

static int __xgeSvgMatrixIsIdentity(xge_shape_ex_matrix_t tMatrix)
{
	return fabsf(tMatrix.fA - 1.0f) <= XGE_SVG_EPSILON &&
		fabsf(tMatrix.fB) <= XGE_SVG_EPSILON &&
		fabsf(tMatrix.fC) <= XGE_SVG_EPSILON &&
		fabsf(tMatrix.fD - 1.0f) <= XGE_SVG_EPSILON &&
		fabsf(tMatrix.fE) <= XGE_SVG_EPSILON &&
		fabsf(tMatrix.fF) <= XGE_SVG_EPSILON;
}

static void __xgeSvgPaintRuntimeStateEnsure(xge_svg_draw_item_t* pItem)
{
	if ( (pItem == NULL) || pItem->bRuntimeStateInitialized ) return;
	pItem->tRuntimeTransform = __xgeSvgMatrixIdentity();
	pItem->fRuntimeScaleX = 1.0f;
	pItem->fRuntimeScaleY = 1.0f;
	pItem->fRuntimeOpacity = 1.0f;
	pItem->iRuntimeVisible = 1;
	pItem->iRuntimeBlend = XGE_BLEND_ALPHA;
	pItem->bRuntimeStateInitialized = 1;
}

static void __xgeSvgPaintRuntimeStateCopy(
	xge_svg_draw_item_t* pDst,
	const xge_svg_draw_item_t* pSrc
)
{
	if ( (pDst == NULL) || (pSrc == NULL) ) return;
	pDst->tRuntimeTransform = pSrc->tRuntimeTransform;
	pDst->fRuntimeTranslateX = pSrc->fRuntimeTranslateX;
	pDst->fRuntimeTranslateY = pSrc->fRuntimeTranslateY;
	pDst->fRuntimeScaleX = pSrc->fRuntimeScaleX;
	pDst->fRuntimeScaleY = pSrc->fRuntimeScaleY;
	pDst->fRuntimeRotateRadians = pSrc->fRuntimeRotateRadians;
	pDst->fRuntimeOpacity = pSrc->fRuntimeOpacity;
	pDst->iRuntimeBlend = pSrc->iRuntimeBlend;
	pDst->bRuntimeStateInitialized = pSrc->bRuntimeStateInitialized;
	pDst->bRuntimeTransform = pSrc->bRuntimeTransform;
	pDst->bRuntimeTransformOverride = pSrc->bRuntimeTransformOverride;
	pDst->bRuntimeOpacity = pSrc->bRuntimeOpacity;
	pDst->bRuntimeVisible = pSrc->bRuntimeVisible;
	pDst->iRuntimeVisible = pSrc->iRuntimeVisible;
	pDst->bRuntimeBlend = pSrc->bRuntimeBlend;
	pDst->bRuntimeMaskSet = pSrc->bRuntimeMaskSet;
	pDst->iRuntimeMaskMethod = pSrc->iRuntimeMaskMethod;
	pDst->iRuntimeMaskTargetType = pSrc->iRuntimeMaskTargetType;
	pDst->bRuntimeClipSet = pSrc->bRuntimeClipSet;
}

static int __xgeSvgPaintRuntimeCompositionClone(
	xge_svg_draw_item_t* pDst,
	const xge_svg_draw_item_t* pSrc
)
{
	xge_shape_ex_scene_child_t tChild;
	int iRet;

	if ( (pDst == NULL) || (pSrc == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( pSrc->pRuntimeClipOwnerScene != NULL ) {
		iRet = xgeShapeExSceneClone(
			pSrc->pRuntimeClipOwnerScene, &pDst->pRuntimeClipOwnerScene
		);
		if ( iRet != XGE_OK ) return iRet;
		iRet = xgeShapeExSceneChildGetAt(pDst->pRuntimeClipOwnerScene, 0, &tChild);
		if ( (iRet != XGE_OK) || (tChild.iType != XGE_SHAPE_EX_SCENE_CHILD_SHAPE) ||
		     (tChild.pShape == NULL) ) {
			return iRet != XGE_OK ? iRet : XGE_ERROR_INVALID_STATE;
		}
		pDst->pRuntimeClipShape = tChild.pShape;
	}
	if ( pSrc->pRuntimeMaskOwnerScene != NULL ) {
		iRet = xgeShapeExSceneClone(
			pSrc->pRuntimeMaskOwnerScene, &pDst->pRuntimeMaskOwnerScene
		);
		if ( iRet != XGE_OK ) return iRet;
		iRet = xgeShapeExSceneChildGetAt(pDst->pRuntimeMaskOwnerScene, 0, &tChild);
		if ( iRet != XGE_OK ) return iRet;
		if ( pSrc->iRuntimeMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_SHAPE ) {
			if ( (tChild.iType != XGE_SHAPE_EX_SCENE_CHILD_SHAPE) || (tChild.pShape == NULL) ) {
				return XGE_ERROR_INVALID_STATE;
			}
			pDst->pRuntimeMaskShape = tChild.pShape;
		} else if ( pSrc->iRuntimeMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_SCENE ) {
			if ( (tChild.iType != XGE_SHAPE_EX_SCENE_CHILD_SCENE) || (tChild.pScene == NULL) ) {
				return XGE_ERROR_INVALID_STATE;
			}
			pDst->pRuntimeMaskTargetScene = tChild.pScene;
		}
	}
	return XGE_OK;
}

static int __xgeSvgPaintBaseTransformGet(
	const xge_svg_draw_item_t* pItem,
	xge_shape_ex_matrix_t* pMatrix
)
{
	if ( (pItem == NULL) || (pMatrix == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( pItem->pIdentityShape != NULL ) {
		return xgeShapeExTransformGet(pItem->pIdentityShape, pMatrix);
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_SHAPE) && (pItem->u.pShape != NULL) ) {
		return xgeShapeExTransformGet(pItem->u.pShape, pMatrix);
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_SCENE) && (pItem->u.pScene != NULL) ) {
		return xgeShapeExSceneTransformGet(pItem->u.pScene, pMatrix);
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_GROUP) && (pItem->u.pGroup != NULL) ) {
		*pMatrix = pItem->u.pGroup->bPaintTransform ?
			pItem->u.pGroup->tPaintTransform : __xgeSvgMatrixIdentity();
		return XGE_OK;
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_TEXT ) {
		*pMatrix = pItem->u.tText.tStyle.tTransform;
		return XGE_OK;
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE ) {
		*pMatrix = pItem->u.tImage.tStyle.tTransform;
		return XGE_OK;
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE ) {
		*pMatrix = pItem->u.tRaster.tStyle.tTransform;
		return XGE_OK;
	}
	*pMatrix = __xgeSvgMatrixIdentity();
	return XGE_OK;
}

static int __xgeSvgPaintRuntimeParent(
	const xge_svg_draw_item_t* pItem,
	xge_shape_ex_matrix_t tParent,
	xge_shape_ex_matrix_t* pOut
)
{
	xge_shape_ex_matrix_t tBase;
	xge_shape_ex_matrix_t tInverse;
	xge_shape_ex_matrix_t tDelta;
	int iRet;

	if ( (pItem == NULL) || (pOut == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	*pOut = tParent;
	if ( !pItem->bRuntimeTransform ) return XGE_OK;
	iRet = __xgeSvgPaintBaseTransformGet(pItem, &tBase);
	if ( iRet != XGE_OK ) return iRet;
	if ( !__xgeSvgMatrixInvert(tBase, &tInverse) ) return XGE_ERROR_INVALID_STATE;
	tDelta = __xgeSvgMatrixMul(pItem->tRuntimeTransform, tInverse);
	*pOut = __xgeSvgMatrixMul(tParent, tDelta);
	return XGE_OK;
}

static int __xgeSvgPaintHierarchyRuntimeParent(
	xge_svg pSvg,
	xge_shape_ex_matrix_t tParent,
	xge_shape_ex_matrix_t* pOut
)
{
	xge_svg_draw_item_t* arrPaints[4];
	int i;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pOut == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	arrPaints[0] = &pSvg->tPicturePaint;
	arrPaints[1] = &pSvg->tRootPaint;
	arrPaints[2] = &pSvg->tContentPaint;
	arrPaints[3] = &pSvg->tDocumentPaint;
	*pOut = tParent;
	for ( i = 0; i < 4; i++ ) {
		iRet = __xgeSvgPaintRuntimeParent(arrPaints[i], *pOut, pOut);
		if ( iRet != XGE_OK ) return iRet;
	}
	return XGE_OK;
}

static xge_shape_ex_matrix_t __xgeSvgPictureSizeMatrix(xge_svg pSvg)
{
	xge_shape_ex_matrix_t tMatrix = __xgeSvgMatrixIdentity();

	if ( !__xgeSvgValid(pSvg) || !pSvg->bLoaded ||
	     (fabsf(pSvg->fIntrinsicWidth) <= XGE_SVG_EPSILON) ||
	     (fabsf(pSvg->fIntrinsicHeight) <= XGE_SVG_EPSILON) ) {
		return tMatrix;
	}
	tMatrix.fA = pSvg->fWidth / pSvg->fIntrinsicWidth;
	tMatrix.fD = pSvg->fHeight / pSvg->fIntrinsicHeight;
	return tMatrix;
}

static int __xgeSvgPaintRetainedHierarchyParent(
	xge_svg pSvg,
	xge_shape_ex_matrix_t tParent,
	xge_shape_ex_matrix_t* pOut
)
{
	xge_svg_draw_item_t* arrPaints[4];
	int i;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pOut == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	arrPaints[0] = &pSvg->tPicturePaint;
	arrPaints[1] = &pSvg->tRootPaint;
	arrPaints[2] = &pSvg->tContentPaint;
	arrPaints[3] = &pSvg->tDocumentPaint;
	*pOut = tParent;
	for ( i = 0; i < 4; i++ ) {
		iRet = __xgeSvgPaintRuntimeParent(arrPaints[i], *pOut, pOut);
		if ( iRet != XGE_OK ) return iRet;
		if ( i == 0 ) {
			*pOut = __xgeSvgMatrixMul(*pOut, __xgeSvgPictureSizeMatrix(pSvg));
		}
	}
	return XGE_OK;
}

static void __xgeSvgPaintHierarchyRuntimeState(
	xge_svg pSvg,
	float* pOpacity,
	int* pVisible,
	int* pBlend,
	int* pBlendSet
)
{
	xge_svg_draw_item_t* arrPaints[4];
	int i;

	*pOpacity = 1.0f;
	*pVisible = 1;
	*pBlend = XGE_BLEND_ALPHA;
	*pBlendSet = 0;
	arrPaints[0] = &pSvg->tPicturePaint;
	arrPaints[1] = &pSvg->tRootPaint;
	arrPaints[2] = &pSvg->tContentPaint;
	arrPaints[3] = &pSvg->tDocumentPaint;
	for ( i = 0; i < 4; i++ ) {
		xge_svg_draw_item_t* pPaint = arrPaints[i];

		if ( pPaint->bRuntimeVisible && !pPaint->iRuntimeVisible ) *pVisible = 0;
		if ( pPaint->bRuntimeOpacity ) *pOpacity *= pPaint->fRuntimeOpacity;
		if ( pPaint->bRuntimeBlend ) {
			*pBlend = pPaint->iRuntimeBlend;
			*pBlendSet = 1;
		}
	}
}

static void __xgeSvgPaintRuntimeTransformRebuild(xge_svg_draw_item_t* pItem)
{
	xge_shape_ex_matrix_t tLocal;

	if ( pItem == NULL ) return;
	__xgeSvgPaintRuntimeStateEnsure(pItem);
	pItem->tRuntimeTransform = __xgeSvgMatrixIdentity();
	pItem->tRuntimeTransform.fE = pItem->fRuntimeTranslateX;
	pItem->tRuntimeTransform.fF = pItem->fRuntimeTranslateY;
	if ( pItem->fRuntimeRotateRadians != 0.0f ) {
		xgeShapeExMatrixRotate(&tLocal, pItem->fRuntimeRotateRadians);
		pItem->tRuntimeTransform = __xgeSvgMatrixMul(pItem->tRuntimeTransform, tLocal);
	}
	if ( (pItem->fRuntimeScaleX != 1.0f) || (pItem->fRuntimeScaleY != 1.0f) ) {
		xgeShapeExMatrixScale(&tLocal, pItem->fRuntimeScaleX, pItem->fRuntimeScaleY);
		pItem->tRuntimeTransform = __xgeSvgMatrixMul(pItem->tRuntimeTransform, tLocal);
	}
	pItem->bRuntimeTransform = 1;
}

static xge_vec2_t __xgeSvgMatrixPoint(xge_shape_ex_matrix_t tMatrix, xge_vec2_t tPoint)
{
	xge_vec2_t tOut;

	if ( xgeShapeExMatrixPoint(&tOut, &tMatrix, tPoint) != XGE_OK ) {
		return tPoint;
	}
	return tOut;
}

static xge_rect_t __xgeSvgMatrixRectBounds(xge_shape_ex_matrix_t tMatrix, xge_rect_t tRect)
{
	xge_rect_t tOut;

	if ( xgeShapeExMatrixRectBounds(&tOut, &tMatrix, tRect) != XGE_OK ) {
		return tRect;
	}
	return tOut;
}

static int __xgeSvgMatrixRectBoundsInverse(xge_shape_ex_matrix_t tMatrix, xge_rect_t tRect, xge_rect_t* pOut)
{
	xge_shape_ex_matrix_t tInverse;

	if ( pOut == NULL ) {
		return 0;
	}
	if ( xgeShapeExMatrixInvert(&tInverse, &tMatrix) != XGE_OK ) {
		return 0;
	}
	if ( xgeShapeExMatrixRectBounds(pOut, &tInverse, tRect) != XGE_OK ) {
		return 0;
	}
	return 1;
}

static int __xgeSvgClipRectMapToItem(xge_shape_ex_matrix_t tItemTransform, xge_rect_t tClip, int bHasExistingClip, xge_rect_t tExistingClip, xge_rect_t* pOut)
{
	xge_rect_t tLocalClip;

	if ( pOut == NULL ) {
		return 0;
	}
	if ( !__xgeSvgMatrixRectBoundsInverse(tItemTransform, tClip, &tLocalClip) ) {
		return 0;
	}
	if ( bHasExistingClip ) {
		tLocalClip = __xgeSvgRectIntersect(tLocalClip, tExistingClip);
	}
	if ( (tLocalClip.fW <= 0.0f) || (tLocalClip.fH <= 0.0f) ) {
		return 0;
	}
	*pOut = tLocalClip;
	return 1;
}

static int __xgeSvgApplyViewportClipToShape(xge_shape_ex pShape, const xge_svg_style_t* pStyle)
{
	xge_rect_t tClip;

	if ( (pShape == NULL) || (pStyle == NULL) || !pStyle->bViewportClip ) {
		return 1;
	}
	if ( !__xgeSvgClipRectMapToItem(pShape->tTransform, pStyle->tViewportClip, pShape->bClipRect, pShape->tClipRect, &tClip) ) {
		return 0;
	}
	xgeShapeExClipRectSet(pShape, tClip);
	return 1;
}

static int __xgeSvgApplyViewportClipToText(xge_svg_text_item_t* pText)
{
	xge_rect_t tClip;

	if ( (pText == NULL) || !pText->tStyle.bViewportClip ) {
		return 1;
	}
	if ( !__xgeSvgClipRectMapToItem(pText->tStyle.tTransform, pText->tStyle.tViewportClip, pText->bClipRect, pText->tClipRect, &tClip) ) {
		return 0;
	}
	pText->bClipRect = 1;
	pText->tClipRect = tClip;
	pText->tStyle.bViewportClip = 0;
	return 1;
}

static int __xgeSvgApplyViewportClipToImage(xge_svg_image_item_t* pImage)
{
	xge_rect_t tClip;

	if ( (pImage == NULL) || !pImage->tStyle.bViewportClip ) {
		return 1;
	}
	if ( !__xgeSvgClipRectMapToItem(pImage->tStyle.tTransform, pImage->tStyle.tViewportClip, pImage->bClipRect, pImage->tClipRect, &tClip) ) {
		return 0;
	}
	pImage->bClipRect = 1;
	pImage->tClipRect = tClip;
	pImage->tStyle.bViewportClip = 0;
	return 1;
}

static int __xgeSvgApplyViewportClipToRaster(xge_svg_raster_item_t* pImage)
{
	xge_rect_t tClip;

	if ( (pImage == NULL) || !pImage->tStyle.bViewportClip ) {
		return 1;
	}
	if ( !__xgeSvgClipRectMapToItem(pImage->tStyle.tTransform, pImage->tStyle.tViewportClip, pImage->bClipRect, pImage->tClipRect, &tClip) ) {
		return 0;
	}
	pImage->bClipRect = 1;
	pImage->tClipRect = tClip;
	pImage->tStyle.bViewportClip = 0;
	return 1;
}

static int __xgeSvgRangeEqualsText(const char* pStart, const char* pEnd, const char* sText)
{
	size_t iLen;

	if ( (pStart == NULL) || (pEnd == NULL) || (sText == NULL) || (pEnd < pStart) ) {
		return 0;
	}
	iLen = strlen(sText);
	return ((size_t)(pEnd - pStart) == iLen) && (strncmp(pStart, sText, iLen) == 0);
}

static void __xgeSvgParsePreserveAspectRatioFields(const char* sValue, int* pAlignX, int* pAlignY, int* pMeetOrSlice)
{
	const char* p;
	const char* pValueStart;
	const char* pStart;
	const char* pEnd;
	int iAlignX;
	int iAlignY;

	if ( pAlignX != NULL ) *pAlignX = XGE_SVG_ASPECT_ALIGN_MID;
	if ( pAlignY != NULL ) *pAlignY = XGE_SVG_ASPECT_ALIGN_MID;
	if ( pMeetOrSlice != NULL ) *pMeetOrSlice = XGE_SVG_ASPECT_MEET;
	if ( (sValue == NULL) || (sValue[0] == '\0') ) {
		return;
	}
	p = sValue;
	__xgeSvgSkipSpaces(&p);
	pStart = p;
	pEnd = p + strlen(p);
	while ( (pEnd > pStart) && __xgeSvgIsSpace((unsigned char)pEnd[-1]) ) {
		pEnd--;
	}
	if ( __xgeSvgRangeEqualsText(pStart, pEnd, "none") ) {
		if ( pMeetOrSlice != NULL ) *pMeetOrSlice = XGE_SVG_ASPECT_NONE;
		return;
	}
	pValueStart = pStart;
	iAlignX = XGE_SVG_ASPECT_ALIGN_MID;
	iAlignY = XGE_SVG_ASPECT_ALIGN_MID;
	if ( strncmp(pStart, "xMinYMin", 8) == 0 ) {
		iAlignX = XGE_SVG_ASPECT_ALIGN_MIN;
		iAlignY = XGE_SVG_ASPECT_ALIGN_MIN;
		p = pStart + 8;
	} else if ( strncmp(pStart, "xMidYMin", 8) == 0 ) {
		iAlignX = XGE_SVG_ASPECT_ALIGN_MID;
		iAlignY = XGE_SVG_ASPECT_ALIGN_MIN;
		p = pStart + 8;
	} else if ( strncmp(pStart, "xMaxYMin", 8) == 0 ) {
		iAlignX = XGE_SVG_ASPECT_ALIGN_MAX;
		iAlignY = XGE_SVG_ASPECT_ALIGN_MIN;
		p = pStart + 8;
	} else if ( strncmp(pStart, "xMinYMid", 8) == 0 ) {
		iAlignX = XGE_SVG_ASPECT_ALIGN_MIN;
		iAlignY = XGE_SVG_ASPECT_ALIGN_MID;
		p = pStart + 8;
	} else if ( strncmp(pStart, "xMidYMid", 8) == 0 ) {
		iAlignX = XGE_SVG_ASPECT_ALIGN_MID;
		iAlignY = XGE_SVG_ASPECT_ALIGN_MID;
		p = pStart + 8;
	} else if ( strncmp(pStart, "xMaxYMid", 8) == 0 ) {
		iAlignX = XGE_SVG_ASPECT_ALIGN_MAX;
		iAlignY = XGE_SVG_ASPECT_ALIGN_MID;
		p = pStart + 8;
	} else if ( strncmp(pStart, "xMinYMax", 8) == 0 ) {
		iAlignX = XGE_SVG_ASPECT_ALIGN_MIN;
		iAlignY = XGE_SVG_ASPECT_ALIGN_MAX;
		p = pStart + 8;
	} else if ( strncmp(pStart, "xMidYMax", 8) == 0 ) {
		iAlignX = XGE_SVG_ASPECT_ALIGN_MID;
		iAlignY = XGE_SVG_ASPECT_ALIGN_MAX;
		p = pStart + 8;
	} else if ( strncmp(pStart, "xMaxYMax", 8) == 0 ) {
		iAlignX = XGE_SVG_ASPECT_ALIGN_MAX;
		iAlignY = XGE_SVG_ASPECT_ALIGN_MAX;
		p = pStart + 8;
	} else {
		iAlignX = XGE_SVG_ASPECT_ALIGN_MID;
		iAlignY = XGE_SVG_ASPECT_ALIGN_MID;
		p = pValueStart;
	}
	if ( pAlignX != NULL ) *pAlignX = iAlignX;
	if ( pAlignY != NULL ) *pAlignY = iAlignY;
	__xgeSvgSkipSpaces(&p);
	pStart = p;
	pEnd = p + strlen(p);
	while ( (pEnd > pStart) && __xgeSvgIsSpace((unsigned char)pEnd[-1]) ) {
		pEnd--;
	}
	if ( (pMeetOrSlice != NULL) && __xgeSvgRangeEqualsText(pStart, pEnd, "slice") ) {
		*pMeetOrSlice = XGE_SVG_ASPECT_SLICE;
	}
}

static xge_rect_t __xgeSvgAspectViewport(xge_rect_t tViewBox, xge_rect_t tDst, int iAlignX, int iAlignY, int iMeetOrSlice)
{
	float fScaleX;
	float fScaleY;
	float fScale;
	xge_rect_t tOut;

	if ( (tViewBox.fW <= 0.0f) || (tViewBox.fH <= 0.0f) || (iMeetOrSlice == XGE_SVG_ASPECT_NONE) ) {
		return tDst;
	}
	fScaleX = tDst.fW / tViewBox.fW;
	fScaleY = tDst.fH / tViewBox.fH;
	fScale = (iMeetOrSlice == XGE_SVG_ASPECT_SLICE) ? fmaxf(fScaleX, fScaleY) : fminf(fScaleX, fScaleY);
	tOut.fW = tViewBox.fW * fScale;
	tOut.fH = tViewBox.fH * fScale;
	tOut.fX = tDst.fX;
	tOut.fY = tDst.fY;
	if ( iAlignX == XGE_SVG_ASPECT_ALIGN_MID ) tOut.fX += (tDst.fW - tOut.fW) * 0.5f;
	else if ( iAlignX == XGE_SVG_ASPECT_ALIGN_MAX ) tOut.fX += (tDst.fW - tOut.fW);
	if ( iAlignY == XGE_SVG_ASPECT_ALIGN_MID ) tOut.fY += (tDst.fH - tOut.fH) * 0.5f;
	else if ( iAlignY == XGE_SVG_ASPECT_ALIGN_MAX ) tOut.fY += (tDst.fH - tOut.fH);
	return tOut;
}

static xge_shape_ex_matrix_t __xgeSvgViewBoxMatrix(xge_rect_t tViewBox, xge_rect_t tViewport)
{
	xge_shape_ex_matrix_t tMatrix = __xgeSvgMatrixIdentity();
	float fScaleX;
	float fScaleY;

	if ( (tViewBox.fW <= 0.0f) || (tViewBox.fH <= 0.0f) ) {
		return tMatrix;
	}
	fScaleX = tViewport.fW / tViewBox.fW;
	fScaleY = tViewport.fH / tViewBox.fH;
	tMatrix.fA = fScaleX;
	tMatrix.fD = fScaleY;
	tMatrix.fE = tViewport.fX - tViewBox.fX * fScaleX;
	tMatrix.fF = tViewport.fY - tViewBox.fY * fScaleY;
	return tMatrix;
}

static void __xgeSvgSkipTransformSeparators(const char** ppText)
{
	if ( ppText == NULL ) {
		return;
	}
	while ( __xgeSvgIsSpace((unsigned char)**ppText) || (**ppText == ',') ) {
		(*ppText)++;
	}
}

static int __xgeSvgTransformNameEquals(const char* pText, const char* sName)
{
	if ( (pText == NULL) || (sName == NULL) ) {
		return 0;
	}
	while ( *sName != '\0' ) {
		if ( *pText != *sName ) {
			return 0;
		}
		pText++;
		sName++;
	}
	return 1;
}

static int __xgeSvgParseAngleUnit(const char** ppText, float* pValue)
{
	const char* p;

	if ( (ppText == NULL) || (*ppText == NULL) || (pValue == NULL) ) {
		return 0;
	}
	p = *ppText;
	if ( (__xgeSvgCssAsciiLower((unsigned char)p[0]) == 'd') &&
	     (__xgeSvgCssAsciiLower((unsigned char)p[1]) == 'e') &&
	     (__xgeSvgCssAsciiLower((unsigned char)p[2]) == 'g') ) {
		*ppText = p + 3;
		return 1;
	}
	if ( (__xgeSvgCssAsciiLower((unsigned char)p[0]) == 'r') &&
	     (__xgeSvgCssAsciiLower((unsigned char)p[1]) == 'a') &&
	     (__xgeSvgCssAsciiLower((unsigned char)p[2]) == 'd') ) {
		*pValue = *pValue * (180.0f / 3.14159265358979323846f);
		*ppText = p + 3;
		return 1;
	}
	if ( (__xgeSvgCssAsciiLower((unsigned char)p[0]) == 'g') &&
	     (__xgeSvgCssAsciiLower((unsigned char)p[1]) == 'r') &&
	     (__xgeSvgCssAsciiLower((unsigned char)p[2]) == 'a') &&
	     (__xgeSvgCssAsciiLower((unsigned char)p[3]) == 'd') ) {
		*pValue = *pValue * 0.9f;
		*ppText = p + 4;
		return 1;
	}
	if ( (__xgeSvgCssAsciiLower((unsigned char)p[0]) == 't') &&
	     (__xgeSvgCssAsciiLower((unsigned char)p[1]) == 'u') &&
	     (__xgeSvgCssAsciiLower((unsigned char)p[2]) == 'r') &&
	     (__xgeSvgCssAsciiLower((unsigned char)p[3]) == 'n') ) {
		*pValue = *pValue * 360.0f;
		*ppText = p + 4;
		return 1;
	}
	return 0;
}

static int __xgeSvgParseAngleDegreesStrict(const char* sText, float* pValue)
{
	const char* p;
	const char* pEnd;
	double fValue;

	if ( (sText == NULL) || (pValue == NULL) ) {
		return 0;
	}
	p = sText;
	while ( __xgeSvgIsSpace((unsigned char)*p) ) p++;
	if ( !__xgeSvgParseNumberAt(p, &pEnd, &fValue) ) {
		return 0;
	}
	if ( !__xgeSvgDoubleToFloat(fValue, pValue) ) {
		return 0;
	}
	p = pEnd;
	while ( __xgeSvgIsSpace((unsigned char)*p) ) p++;
	if ( (*p != '\0') && !__xgeSvgParseAngleUnit(&p, pValue) ) {
		return 0;
	}
	while ( __xgeSvgIsSpace((unsigned char)*p) ) p++;
	return *p == '\0';
}

static int __xgeSvgParseTransformArgs(const char* sText, const char** ppNext, float* pValues, int iCapacity, int* pCount)
{
	const char* p;
	const char* pEnd;
	double fValue;
	int iCount;

	if ( (sText == NULL) || (ppNext == NULL) || (pValues == NULL) || (iCapacity <= 0) || (pCount == NULL) ) {
		return 0;
	}
	p = sText;
	while ( __xgeSvgIsSpace((unsigned char)*p) ) p++;
	if ( *p != '(' ) {
		return 0;
	}
	p++;
	iCount = 0;
	for ( ;; ) {
		__xgeSvgSkipTransformSeparators(&p);
		if ( *p == ')' ) {
			p++;
			break;
		}
		if ( *p == '\0' || iCount >= iCapacity ) {
			return 0;
		}
		if ( !__xgeSvgParseNumberAt(p, &pEnd, &fValue) ) {
			return 0;
		}
		if ( !__xgeSvgDoubleToFloat(fValue, &pValues[iCount]) ) {
			return 0;
		}
		p = pEnd;
		iCount++;
	}
	*ppNext = p;
	*pCount = iCount;
	return 1;
}

static int __xgeSvgParseTransform(const char* sText, xge_shape_ex_matrix_t* pMatrix)
{
	xge_shape_ex_matrix_t tOut;
	const char* p;

	if ( (sText == NULL) || (pMatrix == NULL) ) {
		return 0;
	}
	tOut = __xgeSvgMatrixIdentity();
	p = sText;
	while ( *p != '\0' ) {
		float arrArgs[6];
		const char* pNext;
		int iArgCount;

		__xgeSvgSkipTransformSeparators(&p);
		if ( *p == '\0' ) {
			break;
		}
		if ( __xgeSvgTransformNameEquals(p, "matrix") ) {
			xge_shape_ex_matrix_t tM;

			p += 6;
			if ( !__xgeSvgParseTransformArgs(p, &pNext, arrArgs, 6, &iArgCount) || (iArgCount != 6) ) return 0;
			tM.fA = arrArgs[0]; tM.fB = arrArgs[1]; tM.fC = arrArgs[2]; tM.fD = arrArgs[3]; tM.fE = arrArgs[4]; tM.fF = arrArgs[5];
			tOut = __xgeSvgMatrixMul(tOut, tM);
			p = pNext;
		} else if ( __xgeSvgTransformNameEquals(p, "translate") ) {
			p += 9;
			if ( !__xgeSvgParseTransformArgs(p, &pNext, arrArgs, 6, &iArgCount) || (iArgCount < 1) || (iArgCount > 2) ) return 0;
			tOut = __xgeSvgMatrixMul(tOut, __xgeSvgMatrixTranslate(arrArgs[0], iArgCount == 2 ? arrArgs[1] : 0.0f));
			p = pNext;
		} else if ( __xgeSvgTransformNameEquals(p, "scale") ) {
			p += 5;
			if ( !__xgeSvgParseTransformArgs(p, &pNext, arrArgs, 6, &iArgCount) || (iArgCount < 1) || (iArgCount > 2) ) return 0;
			tOut = __xgeSvgMatrixMul(tOut, __xgeSvgMatrixScale(arrArgs[0], iArgCount == 2 ? arrArgs[1] : arrArgs[0]));
			p = pNext;
		} else if ( __xgeSvgTransformNameEquals(p, "rotate") ) {
			p += 6;
			if ( !__xgeSvgParseTransformArgs(p, &pNext, arrArgs, 6, &iArgCount) || ((iArgCount != 1) && (iArgCount != 3)) ) return 0;
			if ( iArgCount == 3 ) {
				tOut = __xgeSvgMatrixMul(tOut, __xgeSvgMatrixTranslate(arrArgs[1], arrArgs[2]));
				tOut = __xgeSvgMatrixMul(tOut, __xgeSvgMatrixRotate(arrArgs[0]));
				tOut = __xgeSvgMatrixMul(tOut, __xgeSvgMatrixTranslate(-arrArgs[1], -arrArgs[2]));
			} else {
				tOut = __xgeSvgMatrixMul(tOut, __xgeSvgMatrixRotate(arrArgs[0]));
			}
			p = pNext;
		} else if ( __xgeSvgTransformNameEquals(p, "skewX") ) {
			p += 5;
			if ( !__xgeSvgParseTransformArgs(p, &pNext, arrArgs, 6, &iArgCount) || (iArgCount != 1) ) return 0;
			tOut = __xgeSvgMatrixMul(tOut, __xgeSvgMatrixSkewX(arrArgs[0]));
			p = pNext;
		} else if ( __xgeSvgTransformNameEquals(p, "skewY") ) {
			p += 5;
			if ( !__xgeSvgParseTransformArgs(p, &pNext, arrArgs, 6, &iArgCount) || (iArgCount != 1) ) return 0;
			tOut = __xgeSvgMatrixMul(tOut, __xgeSvgMatrixSkewY(arrArgs[0]));
			p = pNext;
		} else {
			return 0;
		}
	}
	*pMatrix = tOut;
	return 1;
}

static int __xgeSvgParseGradientSpread(const char* sText, int* pSpread)
{
	if ( (sText == NULL) || (pSpread == NULL) ) {
		return 0;
	}
	if ( strcmp(sText, "reflect") == 0 ) {
		*pSpread = XGE_SHAPE_EX_GRADIENT_SPREAD_REFLECT;
		return 1;
	}
	if ( strcmp(sText, "repeat") == 0 ) {
		*pSpread = XGE_SHAPE_EX_GRADIENT_SPREAD_REPEAT;
		return 1;
	}
	if ( strcmp(sText, "pad") == 0 ) {
		*pSpread = XGE_SHAPE_EX_GRADIENT_SPREAD_PAD;
		return 1;
	}
	return 0;
}

static int __xgeSvgParseGradientUnits(const char* sText, int* pUnits)
{
	return __xgeSvgParseUserObjectUnits(sText, XGE_SHAPE_EX_GRADIENT_USER_SPACE, XGE_SHAPE_EX_GRADIENT_OBJECT_BOUNDING_BOX, pUnits);
}

static int __xgeSvgParsePatternUnits(const char* sText, int* pUnits)
{
	return __xgeSvgParseUserObjectUnits(sText, XGE_SVG_PATTERN_USER_SPACE, XGE_SVG_PATTERN_OBJECT_BOUNDING_BOX, pUnits);
}

static int __xgeSvgParseDashArrayEx(const char* sText, float fPercentRef, float fFontSize, float* pPattern, int* pCount)
{
	const char* p;
	const char* pEnd;
	float arrPattern[XGE_SVG_DASH_MAX];
	int iCount;
	float fTotal;
	int i;

	if ( (sText == NULL) || (pPattern == NULL) || (pCount == NULL) ) {
		return 0;
	}
	while ( (*sText == ' ') || (*sText == '\t') || (*sText == '\r') || (*sText == '\n') ) sText++;
	if ( __xgeSvgColorNameEquals(sText, "none") ) {
		*pCount = 0;
		return 1;
	}
	p = sText;
	iCount = 0;
	fTotal = 0.0f;
	while ( *p != '\0' ) {
		float fValue;

		while ( (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') || (*p == ',') ) p++;
		if ( *p == '\0' ) {
			break;
		}
		{
			double fParsed;

			if ( !__xgeSvgParseNumberAt(p, &pEnd, &fParsed) || !__xgeSvgDoubleToFloat(fParsed, &fValue) ) {
				break;
			}
			p = pEnd;
			if ( *p == '%' ) {
				fValue = fValue * fPercentRef / 100.0f;
				p++;
			}
		}
		if ( !isfinite(fValue) ) {
			break;
		}
		if ( fValue < 0.0f ) {
			*pCount = 0;
			return 1;
		}
		if ( iCount < XGE_SVG_DASH_MAX ) {
			arrPattern[iCount++] = fValue;
			fTotal += fValue;
		}
	}
	if ( fTotal <= 0.0f ) {
		*pCount = 0;
		return 1;
	}
	if ( (iCount & 1) != 0 ) {
		int iOriginalCount = iCount;
		int iCopyCount = XGE_SVG_DASH_MAX - iCount;

		if ( iCopyCount > iOriginalCount ) {
			iCopyCount = iOriginalCount;
		}
		for ( i = 0; i < iCopyCount; i++ ) {
			arrPattern[iCount++] = arrPattern[i];
		}
		if ( (iCount & 1) != 0 ) {
			iCount--;
		}
	}
	for ( i = 0; i < iCount; i++ ) {
		pPattern[i] = arrPattern[i];
	}
	*pCount = iCount;
	(void)fFontSize;
	return 1;
}

static float __xgeSvgParseDashOffsetThorvg(const char* sText, float fPercentRef, float fFontSize)
{
	const char* p;
	char* pEnd;
	float fValue;

	if ( sText == NULL ) {
		return 0.0f;
	}
	p = sText;
	while ( (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') ) p++;
	fValue = strtof(p, &pEnd);
	if ( (pEnd != p) && !isfinite(fValue) ) {
		return fValue;
	}
	return __xgeSvgParseLengthEx(sText, fPercentRef, fFontSize, &fValue) ? fValue : 0.0f;
}

static int __xgeSvgPaintOrderTokenEquals(const char* pStart, const char* pEnd, const char* sToken)
{
	if ( (pStart == NULL) || (pEnd == NULL) || (sToken == NULL) || (pEnd < pStart) ) {
		return 0;
	}
	return __xgeSvgRangeEqualsTextExact(pStart, pEnd, sToken);
}

static void __xgeSvgPaintOrderDefault(int pOrder[XGE_SVG_PAINT_COMPONENT_COUNT])
{
	if ( pOrder == NULL ) {
		return;
	}
	pOrder[0] = XGE_SVG_PAINT_FILL;
	pOrder[1] = XGE_SVG_PAINT_STROKE;
	pOrder[2] = XGE_SVG_PAINT_MARKERS;
}

static int __xgeSvgPaintOrderIndex(const int pOrder[XGE_SVG_PAINT_COMPONENT_COUNT], int iComponent)
{
	int i;

	if ( pOrder == NULL ) {
		return -1;
	}
	for ( i = 0; i < XGE_SVG_PAINT_COMPONENT_COUNT; i++ ) {
		if ( pOrder[i] == iComponent ) {
			return i;
		}
	}
	return -1;
}

static int __xgeSvgPaintOrderStrokeFirst(const int pOrder[XGE_SVG_PAINT_COMPONENT_COUNT])
{
	int iFill;
	int iStroke;

	iFill = __xgeSvgPaintOrderIndex(pOrder, XGE_SVG_PAINT_FILL);
	iStroke = __xgeSvgPaintOrderIndex(pOrder, XGE_SVG_PAINT_STROKE);
	if ( (iFill < 0) || (iStroke < 0) ) {
		return 0;
	}
	return iStroke < iFill;
}

static void __xgeSvgStyleUpdatePaintOrderDerived(xge_svg_style_t* pStyle)
{
	if ( pStyle == NULL ) {
		return;
	}
	pStyle->bStrokeFirst = __xgeSvgPaintOrderStrokeFirst(pStyle->iPaintOrder);
}

static int __xgeSvgParsePaintOrder(const char* sText, int pOrder[XGE_SVG_PAINT_COMPONENT_COUNT])
{
	const char* p;
	int arrSeen[XGE_SVG_PAINT_COMPONENT_COUNT];
	int arrParsed[XGE_SVG_PAINT_COMPONENT_COUNT];
	int iCount;
	int bSawToken;

	if ( (sText == NULL) || (pOrder == NULL) ) {
		return 0;
	}
	p = sText;
	memset(arrSeen, 0, sizeof(arrSeen));
	bSawToken = 0;
	iCount = 0;
	while ( *p != '\0' ) {
		const char* pStart;
		const char* pEnd;
		int iComponent;

		while ( __xgeSvgIsSpace((unsigned char)*p) || (*p == ',') ) {
			p++;
		}
		if ( *p == '\0' ) {
			break;
		}
		pStart = p;
		while ( (*p != '\0') && !__xgeSvgIsSpace((unsigned char)*p) && (*p != ',') ) {
			p++;
		}
		pEnd = p;
		if ( __xgeSvgPaintOrderTokenEquals(pStart, pEnd, "normal") ) {
			const char* q;

			if ( bSawToken ) {
				return 0;
			}
			q = p;
			while ( __xgeSvgIsSpace((unsigned char)*q) || (*q == ',') ) {
				q++;
			}
			if ( *q != '\0' ) {
				return 0;
			}
			__xgeSvgPaintOrderDefault(pOrder);
			return 1;
		}
		if ( __xgeSvgPaintOrderTokenEquals(pStart, pEnd, "fill") ) {
			iComponent = XGE_SVG_PAINT_FILL;
		} else if ( __xgeSvgPaintOrderTokenEquals(pStart, pEnd, "stroke") ) {
			iComponent = XGE_SVG_PAINT_STROKE;
		} else if ( __xgeSvgPaintOrderTokenEquals(pStart, pEnd, "markers") ) {
			iComponent = XGE_SVG_PAINT_MARKERS;
		} else {
			return 0;
		}
		if ( arrSeen[iComponent] || (iCount >= XGE_SVG_PAINT_COMPONENT_COUNT) ) {
			return 0;
		}
		arrSeen[iComponent] = 1;
		arrParsed[iCount++] = iComponent;
		bSawToken = 1;
	}
	if ( !bSawToken ) {
		return 0;
	}
	if ( !arrSeen[XGE_SVG_PAINT_FILL] ) {
		arrParsed[iCount++] = XGE_SVG_PAINT_FILL;
	}
	if ( !arrSeen[XGE_SVG_PAINT_STROKE] ) {
		arrParsed[iCount++] = XGE_SVG_PAINT_STROKE;
	}
	if ( !arrSeen[XGE_SVG_PAINT_MARKERS] ) {
		arrParsed[iCount++] = XGE_SVG_PAINT_MARKERS;
	}
	memcpy(pOrder, arrParsed, sizeof(arrParsed));
	return 1;
}

static int __xgeSvgParseBlendMode(const char* sText, int* pBlend)
{
	const char* pStart;
	const char* pEnd;

	if ( (sText == NULL) || (pBlend == NULL) ) {
		return 0;
	}
	pStart = sText;
	while ( __xgeSvgIsSpace((unsigned char)*pStart) ) pStart++;
	pEnd = pStart + strlen(pStart);
	while ( (pEnd > pStart) && __xgeSvgIsSpace((unsigned char)pEnd[-1]) ) pEnd--;
	*pBlend = XGE_BLEND_ALPHA;
	if ( pStart == pEnd ) {
		return 0;
	}
	if ( __xgeSvgCssRangeEqualsText(pStart, pEnd, "normal") ) {
		return 1;
	}
	if ( __xgeSvgCssRangeEqualsText(pStart, pEnd, "multiply") ) {
		*pBlend = XGE_BLEND_MULTIPLY;
		return 1;
	}
	if ( __xgeSvgCssRangeEqualsText(pStart, pEnd, "screen") ) {
		*pBlend = XGE_BLEND_SCREEN;
		return 1;
	}
	if ( __xgeSvgCssRangeEqualsText(pStart, pEnd, "darken") ) {
		*pBlend = XGE_BLEND_DARKEN;
		return 1;
	}
	if ( __xgeSvgCssRangeEqualsText(pStart, pEnd, "lighten") ) {
		*pBlend = XGE_BLEND_LIGHTEN;
		return 1;
	}
	if ( __xgeSvgCssRangeEqualsText(pStart, pEnd, "overlay") ) {
		*pBlend = XGE_BLEND_OVERLAY;
		return 1;
	}
	if ( __xgeSvgCssRangeEqualsText(pStart, pEnd, "color-dodge") ) {
		*pBlend = XGE_BLEND_COLOR_DODGE;
		return 1;
	}
	if ( __xgeSvgCssRangeEqualsText(pStart, pEnd, "color-burn") ) {
		*pBlend = XGE_BLEND_COLOR_BURN;
		return 1;
	}
	if ( __xgeSvgCssRangeEqualsText(pStart, pEnd, "hard-light") ) {
		*pBlend = XGE_BLEND_HARD_LIGHT;
		return 1;
	}
	if ( __xgeSvgCssRangeEqualsText(pStart, pEnd, "soft-light") ) {
		*pBlend = XGE_BLEND_SOFT_LIGHT;
		return 1;
	}
	if ( __xgeSvgCssRangeEqualsText(pStart, pEnd, "difference") ) {
		*pBlend = XGE_BLEND_DIFFERENCE;
		return 1;
	}
	if ( __xgeSvgCssRangeEqualsText(pStart, pEnd, "exclusion") ) {
		*pBlend = XGE_BLEND_EXCLUSION;
		return 1;
	}
	if ( __xgeSvgCssRangeEqualsText(pStart, pEnd, "hue") ) {
		*pBlend = XGE_BLEND_HUE;
		return 1;
	}
	if ( __xgeSvgCssRangeEqualsText(pStart, pEnd, "saturation") ) {
		*pBlend = XGE_BLEND_SATURATION;
		return 1;
	}
	if ( __xgeSvgCssRangeEqualsText(pStart, pEnd, "color") ) {
		*pBlend = XGE_BLEND_COLOR;
		return 1;
	}
	if ( __xgeSvgCssRangeEqualsText(pStart, pEnd, "luminosity") ) {
		*pBlend = XGE_BLEND_LUMINOSITY;
		return 1;
	}
	return 0;
}

static xge_svg_style_t __xgeSvgStyleDefault(void)
{
	xge_svg_style_t tStyle;

	memset(&tStyle, 0, sizeof(tStyle));
	tStyle.iCurrentColor = XGE_COLOR_RGBA(0, 0, 0, 255);
	tStyle.iFillColor = XGE_COLOR_RGBA(0, 0, 0, 255);
	tStyle.iStrokeColor = XGE_COLOR_RGBA(0, 0, 0, 0);
	tStyle.iStopColor = XGE_COLOR_RGBA(0, 0, 0, 255);
	tStyle.fOpacity = 1.0f;
	tStyle.fFillOpacity = 1.0f;
	tStyle.fStrokeOpacity = 1.0f;
	tStyle.fStopOpacity = 1.0f;
	tStyle.fStrokeWidth = 1.0f;
	tStyle.fMiterLimit = 4.0f;
	tStyle.fFontSize = XGE_SVG_DEFAULT_FONT_SIZE;
	tStyle.iFillRule = XGE_SHAPE_EX_FILL_NON_ZERO;
	tStyle.iLineCap = XGE_SHAPE_EX_CAP_BUTT;
	tStyle.iLineJoin = XGE_SHAPE_EX_JOIN_MITER;
	tStyle.iTextAnchor = XGE_SVG_TEXT_ANCHOR_START;
	tStyle.iTextSpace = XGE_SVG_TEXT_SPACE_DEFAULT;
	tStyle.iBlend = XGE_BLEND_ALPHA;
	tStyle.iMaskType = XGE_SVG_MASK_TYPE_LUMINANCE;
	__xgeSvgPaintOrderDefault(tStyle.iPaintOrder);
	__xgeSvgStyleUpdatePaintOrderDerived(&tStyle);
	tStyle.bVisible = 1;
	tStyle.bVisibility = 1;
	tStyle.tTransform = __xgeSvgMatrixIdentity();
	return tStyle;
}

static void __xgeSvgFontFamilyCopy(char sOut[XGE_SVG_FONT_FAMILY_MAX], const char* sFamily)
{
	size_t iLength;

	if ( sOut == NULL ) return;
	if ( sFamily == NULL ) {
		sOut[0] = '\0';
		return;
	}
	iLength = strlen(sFamily);
	if ( iLength >= XGE_SVG_FONT_FAMILY_MAX ) iLength = XGE_SVG_FONT_FAMILY_MAX - 1;
	memcpy(sOut, sFamily, iLength);
	sOut[iLength] = '\0';
}

static void __xgeSvgStyleResetStopPaint(xge_svg_style_t* pStyle)
{
	if ( pStyle == NULL ) {
		return;
	}
	pStyle->iStopColor = XGE_COLOR_RGBA(0, 0, 0, 255);
	pStyle->fStopOpacity = 1.0f;
	pStyle->bStopColorSet = 0;
	pStyle->bStopOpacitySet = 0;
}

static int __xgeSvgStylePropertyCopyEx(const char* pTag, const char* pTagEnd, const char* sStyle, const char* sName, char* sOut, int iOutCapacity, int bAllowAttrs, int iImportanceMode, int bFirstDeclaration)
{
	if ( __xgeSvgStyleCopyEx(sStyle, sName, sOut, iOutCapacity, iImportanceMode, bFirstDeclaration) ) {
		return 1;
	}
	if ( bAllowAttrs && (pTag != NULL) && (pTagEnd != NULL) ) {
		return __xgeSvgAttrCopy(pTag, pTagEnd, sName, sOut, iOutCapacity);
	}
	if ( (sOut != NULL) && (iOutCapacity > 0) ) {
		sOut[0] = '\0';
	}
	return 0;
}

static int __xgeSvgStylePropertyCopyValidEx(const char* pTag, const char* pTagEnd, const char* sStyle, const char* sName, char* sOut, int iOutCapacity, int bAllowAttrs, int iImportanceMode, int bFirstDeclaration, xge_svg_style_value_valid_fn pfnValid, void* pUser)
{
	if ( __xgeSvgStyleCopyValidEx(sStyle, sName, sOut, iOutCapacity, iImportanceMode, bFirstDeclaration, pfnValid, pUser) ) {
		return 1;
	}
	if ( bAllowAttrs && (pTag != NULL) && (pTagEnd != NULL) ) {
		if ( __xgeSvgAttrCopy(pTag, pTagEnd, sName, sOut, iOutCapacity) ) {
			if ( (pfnValid == NULL) || pfnValid(sOut, pUser) ) {
				return 1;
			}
			if ( (sOut != NULL) && (iOutCapacity > 0) ) {
				sOut[0] = '\0';
			}
		}
	}
	if ( (sOut != NULL) && (iOutCapacity > 0) ) {
		sOut[0] = '\0';
	}
	return 0;
}

static int __xgeSvgStyleValueIsInherit(const char* sValue)
{
	return (sValue != NULL) && (strcmp(sValue, "inherit") == 0);
}

static int __xgeSvgStyleValueEquals(const char* sValue, const char* sKeyword)
{
	return (sValue != NULL) && (sKeyword != NULL) && (strcmp(sValue, sKeyword) == 0);
}

static int __xgeSvgStyleValueValidColor(const char* sValue, void* pUser)
{
	uint32_t iColor = XGE_COLOR_RGBA(0, 0, 0, 255);

	(void)pUser;
	return __xgeSvgStyleValueIsInherit(sValue) ||
		((sValue != NULL) && (strcmp(sValue, "currentColor") == 0)) ||
		__xgeSvgParseColor(sValue, &iColor);
}

static int __xgeSvgStyleValueValidPaint(const char* sValue, void* pUser)
{
	char sId[XGE_SVG_ID_MAX];
	uint32_t iColor = XGE_COLOR_RGBA(0, 0, 0, 255);
	int bUrlHandled;

	(void)pUser;
	if ( __xgeSvgRgbFunctionRecognized(sValue) ) {
		return __xgeSvgParseRgbColorComponents(sValue, NULL);
	}
	return __xgeSvgStyleValueIsInherit(sValue) ||
		((sValue != NULL) && (strcmp(sValue, "currentColor") == 0)) ||
		__xgeSvgUrlIdCopyFromValueThorvg(sValue, sId, sizeof(sId), &bUrlHandled) ||
		bUrlHandled ||
		__xgeSvgParseColor(sValue, &iColor);
}

static int __xgeSvgStylePropertyCopyPaintEx(const char* pTag, const char* pTagEnd, const char* sStyle, const char* sName, char* sOut, int iOutCapacity, int bAllowAttrs, int iImportanceMode)
{
	if ( __xgeSvgStyleCopyValidEx(sStyle, sName, sOut, iOutCapacity, iImportanceMode, 0, __xgeSvgStyleValueValidPaint, NULL) ) {
		return 1;
	}
	if ( __xgeSvgStyleCopyEx(sStyle, sName, sOut, iOutCapacity, iImportanceMode, 0) ) {
		return 1;
	}
	if ( bAllowAttrs && (pTag != NULL) && (pTagEnd != NULL) ) {
		return __xgeSvgAttrCopy(pTag, pTagEnd, sName, sOut, iOutCapacity);
	}
	if ( (sOut != NULL) && (iOutCapacity > 0) ) {
		sOut[0] = '\0';
	}
	return 0;
}

static int __xgeSvgStyleValueValidOpacity(const char* sValue, void* pUser)
{
	float fValue;

	(void)pUser;
	return __xgeSvgStyleValueIsInherit(sValue) || __xgeSvgParseOpacity(sValue, &fValue);
}

typedef struct xge_svg_dash_array_validation_t {
	float fPercentRef;
	float fFontSize;
} xge_svg_dash_array_validation_t;

static int __xgeSvgStyleValueValidDashArray(const char* sValue, void* pUser)
{
	xge_svg_dash_array_validation_t* pValidation;
	float arrPattern[XGE_SVG_DASH_MAX];
	int iDashCount;

	if ( (sValue == NULL) || (pUser == NULL) ) {
		return 0;
	}
	if ( __xgeSvgStyleValueIsInherit(sValue) || __xgeSvgStyleValueEquals(sValue, "none") ) {
		return 1;
	}
	pValidation = (xge_svg_dash_array_validation_t*)pUser;
	iDashCount = 0;
	if ( !__xgeSvgParseDashArrayEx(sValue, pValidation->fPercentRef, pValidation->fFontSize, arrPattern, &iDashCount) ) {
		return 0;
	}
	return iDashCount > 0;
}

static int __xgeSvgStylePropertyCopyDashArrayEx(const char* pTag, const char* pTagEnd, const char* sStyle, char* sOut, int iOutCapacity, int bAllowAttrs, int iImportanceMode, float fPercentRef, float fFontSize)
{
	xge_svg_dash_array_validation_t tValidation;

	tValidation.fPercentRef = fPercentRef;
	tValidation.fFontSize = fFontSize;
	if ( __xgeSvgStyleCopyValidEx(sStyle, "stroke-dasharray", sOut, iOutCapacity, iImportanceMode, 0, __xgeSvgStyleValueValidDashArray, &tValidation) ) {
		return 1;
	}
	if ( bAllowAttrs && (pTag != NULL) && (pTagEnd != NULL) ) {
		return __xgeSvgAttrCopy(pTag, pTagEnd, "stroke-dasharray", sOut, iOutCapacity);
	}
	if ( (sOut != NULL) && (iOutCapacity > 0) ) {
		sOut[0] = '\0';
	}
	return 0;
}

static int __xgeSvgParseMiterLimitThorvg(const char* sValue, float* pValue)
{
	float fValue;

	if ( (sValue == NULL) || (pValue == NULL) ) {
		return 0;
	}
	fValue = __xgeSvgToFloatThorvg(sValue, NULL);
	if ( fValue < 0.0f ) {
		return 0;
	}
	/* ThorVG accepts NaN and +Inf here. ShapeEx keeps a finite public contract. */
	if ( !isfinite(fValue) ) {
		fValue = 3.402823466e+38F;
	}
	*pValue = fValue;
	return 1;
}

static int __xgeSvgStyleValueValidUrlNone(const char* sValue, void* pUser)
{
	char sId[XGE_SVG_ID_MAX];

	(void)pUser;
	return __xgeSvgStyleValueIsInherit(sValue) ||
		__xgeSvgStyleValueEquals(sValue, "none") ||
		__xgeSvgUrlIdCopyFromValue(sValue, sId, sizeof(sId));
}

static void __xgeSvgStyleApplyProperties(xge_svg pSvg, xge_svg_style_t* pStyle, const xge_svg_style_t* pInheritedStyle, const char* pTag, const char* pTagEnd, const char* sStyle, int bAllowAttrs, int bAllowTransform, int iImportanceMode, int bFirstDeclaration, xge_shape_ex_matrix_t tBaseTransform, float fBaseOpacity, int bBaseVisible, float fBaseFontSize, int iApplyMode)
{
	char sValue[XGE_SVG_ATTR_MAX];
	float fValue;
	float fLengthRef;
	int bText;
	int bTspan;

	if ( pStyle == NULL ) {
		return;
	}
	fLengthRef = __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_OTHER);
	bText = __xgeSvgTagNameEquals(pTag, "text");
	bTspan = __xgeSvgTagNameEquals(pTag, "tspan");
	if ( bAllowAttrs && (iApplyMode != XGE_SVG_STYLE_APPLY_EXCEPT_FONT_SIZE) &&
	     (bText || bTspan) ) {
		if ( __xgeSvgAttrCopy(pTag, pTagEnd, "font-size", sValue, sizeof(sValue)) ) {
			(void)__xgeSvgParseFontSize(sValue, fBaseFontSize, &fValue);
			if ( bTspan && (fValue <= 0.0f) ) {
				pStyle->fFontSize = pInheritedStyle != NULL ? pInheritedStyle->fFontSize : 0.0f;
			} else {
				pStyle->fFontSize = fValue;
			}
		}
	}
	if ( iApplyMode == XGE_SVG_STYLE_APPLY_FONT_SIZE ) {
		return;
	}
	if ( bAllowAttrs && (bText || bTspan) &&
	     __xgeSvgAttrCopy(pTag, pTagEnd, "font-family", sValue, sizeof(sValue)) ) {
		__xgeSvgFontFamilyCopy(pStyle->sFontFamily, sValue);
	}
	if ( __xgeSvgStylePropertyCopyValidEx(pTag, pTagEnd, sStyle, "color", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, 0, __xgeSvgStyleValueValidColor, NULL) &&
	     (strcmp(sValue, "currentColor") != 0) ) {
		int bColorChanged;

		bColorChanged = 0;
		if ( __xgeSvgStyleValueIsInherit(sValue) ) {
			pStyle->iCurrentColor = pInheritedStyle != NULL ? pInheritedStyle->iCurrentColor : XGE_COLOR_RGBA(0, 0, 0, 255);
			pStyle->bColorSet = pInheritedStyle != NULL ? pInheritedStyle->bColorSet : 0;
			bColorChanged = 1;
		} else if ( __xgeSvgParseColor(sValue, &pStyle->iCurrentColor) ) {
			pStyle->bColorSet = 1;
			bColorChanged = 1;
		}
		if ( bColorChanged ) {
			if ( pStyle->bFillCurrentColor ) {
				pStyle->iFillColor = pStyle->iCurrentColor;
			}
			if ( pStyle->bStrokeCurrentColor ) {
				pStyle->iStrokeColor = pStyle->iCurrentColor;
			}
		}
	}
	if ( __xgeSvgStylePropertyCopyEx(pTag, pTagEnd, sStyle, "display", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, bFirstDeclaration) ) {
		if ( __xgeSvgStyleValueEquals(sValue, "none") ) {
			pStyle->bVisible = 0;
		} else {
			pStyle->bVisible = bBaseVisible;
		}
	}
	if ( __xgeSvgStylePropertyCopyEx(pTag, pTagEnd, sStyle, "mix-blend-mode", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, bFirstDeclaration) ) {
		pStyle->iBlend = XGE_BLEND_ALPHA;
		__xgeSvgParseBlendMode(sValue, &pStyle->iBlend);
		pStyle->bBlendSet = 1;
	}
	if ( __xgeSvgStylePropertyCopyEx(pTag, pTagEnd, sStyle, "mask-type", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, bFirstDeclaration) ) {
		pStyle->iMaskType = __xgeSvgStyleValueEquals(sValue, "Alpha") ?
			XGE_SVG_MASK_TYPE_ALPHA : XGE_SVG_MASK_TYPE_LUMINANCE;
	}
	if ( __xgeSvgStylePropertyCopyEx(pTag, pTagEnd, sStyle, "text-anchor", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, bFirstDeclaration) ) {
		if ( __xgeSvgStyleValueEquals(sValue, "middle") ) pStyle->iTextAnchor = XGE_SVG_TEXT_ANCHOR_MIDDLE;
		else if ( __xgeSvgStyleValueEquals(sValue, "end") ) pStyle->iTextAnchor = XGE_SVG_TEXT_ANCHOR_END;
		else pStyle->iTextAnchor = XGE_SVG_TEXT_ANCHOR_START;
	}
	if ( bAllowAttrs && __xgeSvgAttrCopy(pTag, pTagEnd, "xml:space", sValue, sizeof(sValue)) ) {
		if ( __xgeSvgStyleValueEquals(sValue, "preserve") ) pStyle->iTextSpace = XGE_SVG_TEXT_SPACE_PRESERVE;
		else if ( __xgeSvgStyleValueEquals(sValue, "default") ) pStyle->iTextSpace = XGE_SVG_TEXT_SPACE_DEFAULT;
	}
	if ( __xgeSvgStylePropertyCopyValidEx(pTag, pTagEnd, sStyle, "stop-color", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, 0, __xgeSvgStyleValueValidColor, NULL) ) {
		if ( __xgeSvgStyleValueIsInherit(sValue) ) {
			pStyle->iStopColor = pInheritedStyle != NULL ? pInheritedStyle->iStopColor : XGE_COLOR_RGBA(0, 0, 0, 255);
			pStyle->bStopColorSet = pInheritedStyle != NULL ? pInheritedStyle->bStopColorSet : 0;
		} else if ( strcmp(sValue, "currentColor") == 0 ) {
			pStyle->iStopColor = pStyle->iCurrentColor;
			pStyle->bStopColorSet = 1;
		} else if ( __xgeSvgParseColor(sValue, &pStyle->iStopColor) ) {
			pStyle->bStopColorSet = 1;
		}
	}
	if ( __xgeSvgStylePropertyCopyValidEx(pTag, pTagEnd, sStyle, "stop-opacity", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, 0, __xgeSvgStyleValueValidOpacity, NULL) ) {
		if ( __xgeSvgStyleValueIsInherit(sValue) ) {
			pStyle->fStopOpacity = pInheritedStyle != NULL ? pInheritedStyle->fStopOpacity : 1.0f;
			pStyle->bStopOpacitySet = pInheritedStyle != NULL ? pInheritedStyle->bStopOpacitySet : 0;
		} else if ( __xgeSvgParseOpacity(sValue, &fValue) ) {
			pStyle->fStopOpacity = fValue;
			pStyle->bStopOpacitySet = 1;
		}
	}
	if ( __xgeSvgStylePropertyCopyPaintEx(pTag, pTagEnd, sStyle, "fill", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode) ) {
		uint32_t iColor = pStyle->iFillColor;
		char sPaintId[XGE_SVG_ID_MAX];
		int bPaintUrlHandled;
		if ( __xgeSvgStyleValueIsInherit(sValue) ) {
			pStyle->iFillColor = XGE_COLOR_RGBA(0, 0, 0, 255);
			pStyle->bFillSet = 1;
			pStyle->bFillCurrentColor = 0;
			pStyle->sFillGradientId[0] = '\0';
		} else if ( __xgeSvgUrlIdCopyFromValueThorvg(sValue, sPaintId, sizeof(sPaintId), &bPaintUrlHandled) ) {
			pStyle->bFillSet = 1;
			pStyle->bFillCurrentColor = 0;
			strcpy(pStyle->sFillGradientId, sPaintId);
			pStyle->iFillColor = XGE_COLOR_RGBA(0, 0, 0, 0);
		} else if ( bPaintUrlHandled ) {
			pStyle->bFillSet = 1;
			pStyle->bFillCurrentColor = 0;
			pStyle->sFillGradientId[0] = '\0';
			pStyle->iFillColor = XGE_COLOR_RGBA(0, 0, 0, 255);
		} else if ( strcmp(sValue, "currentColor") == 0 ) {
			pStyle->bFillSet = 1;
			pStyle->sFillGradientId[0] = '\0';
			pStyle->iFillColor = pStyle->iCurrentColor;
			pStyle->bFillCurrentColor = 1;
		} else if ( __xgeSvgParseColor(sValue, &iColor) ) {
			pStyle->bFillSet = 1;
			pStyle->bFillCurrentColor = 0;
			pStyle->sFillGradientId[0] = '\0';
			pStyle->iFillColor = iColor;
		} else {
			pStyle->bFillSet = 1;
			pStyle->bFillCurrentColor = 0;
			pStyle->sFillGradientId[0] = '\0';
			pStyle->iFillColor = XGE_COLOR_RGBA(0, 0, 0, 255);
		}
	}
	if ( __xgeSvgStylePropertyCopyPaintEx(pTag, pTagEnd, sStyle, "stroke", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode) ) {
		uint32_t iColor = pStyle->iStrokeColor;
		char sPaintId[XGE_SVG_ID_MAX];
		int bPaintUrlHandled;
		if ( __xgeSvgStyleValueIsInherit(sValue) ) {
			pStyle->iStrokeColor = XGE_COLOR_RGBA(0, 0, 0, 0);
			pStyle->bStrokeSet = 1;
			pStyle->bStrokeCurrentColor = 0;
			pStyle->sStrokeGradientId[0] = '\0';
		} else if ( __xgeSvgUrlIdCopyFromValueThorvg(sValue, sPaintId, sizeof(sPaintId), &bPaintUrlHandled) ) {
			pStyle->bStrokeSet = 1;
			pStyle->bStrokeCurrentColor = 0;
			strcpy(pStyle->sStrokeGradientId, sPaintId);
			pStyle->iStrokeColor = XGE_COLOR_RGBA(0, 0, 0, 0);
		} else if ( bPaintUrlHandled ) {
			pStyle->bStrokeSet = 1;
			pStyle->bStrokeCurrentColor = 0;
			pStyle->sStrokeGradientId[0] = '\0';
			pStyle->iStrokeColor = XGE_COLOR_RGBA(0, 0, 0, 255);
		} else if ( strcmp(sValue, "currentColor") == 0 ) {
			pStyle->bStrokeSet = 1;
			pStyle->sStrokeGradientId[0] = '\0';
			pStyle->iStrokeColor = pStyle->iCurrentColor;
			pStyle->bStrokeCurrentColor = 1;
		} else if ( __xgeSvgParseColor(sValue, &iColor) ) {
			pStyle->bStrokeSet = 1;
			pStyle->bStrokeCurrentColor = 0;
			pStyle->sStrokeGradientId[0] = '\0';
			pStyle->iStrokeColor = iColor;
		} else {
			pStyle->bStrokeSet = 1;
			pStyle->bStrokeCurrentColor = 0;
			pStyle->sStrokeGradientId[0] = '\0';
			pStyle->iStrokeColor = XGE_COLOR_RGBA(0, 0, 0, 0);
		}
	}
	if ( __xgeSvgStylePropertyCopyEx(pTag, pTagEnd, sStyle, "opacity", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, bFirstDeclaration) ) {
		pStyle->fOpacity = fBaseOpacity * __xgeSvgParseOpacityThorvg(sValue);
	}
	if ( __xgeSvgStylePropertyCopyEx(pTag, pTagEnd, sStyle, "fill-opacity", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, 0) ) {
		pStyle->fFillOpacity = __xgeSvgParseOpacityThorvgEx(sValue, &pStyle->bFillOpacityNonFinite);
		pStyle->bFillOpacitySet = 1;
	}
	if ( __xgeSvgStylePropertyCopyEx(pTag, pTagEnd, sStyle, "stroke-opacity", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, 0) ) {
		pStyle->fStrokeOpacity = __xgeSvgParseOpacityThorvgEx(sValue, &pStyle->bStrokeOpacityNonFinite);
		pStyle->bStrokeOpacitySet = 1;
	}
	if ( __xgeSvgStylePropertyCopyEx(pTag, pTagEnd, sStyle, "stroke-width", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, 0) ) {
		pStyle->fStrokeWidth = __xgeSvgParseLengthEx(sValue, fLengthRef, pStyle->fFontSize, &fValue) ? fValue : 0.0f;
		pStyle->bStrokeWidthSet = 1;
	}
	if ( __xgeSvgStylePropertyCopyEx(pTag, pTagEnd, sStyle, "stroke-miterlimit", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, 0) ) {
		if ( __xgeSvgStyleValueIsInherit(sValue) ) {
			pStyle->fMiterLimit = pInheritedStyle != NULL ? pInheritedStyle->fMiterLimit : 4.0f;
		} else if ( __xgeSvgParseMiterLimitThorvg(sValue, &fValue) ) {
			pStyle->fMiterLimit = fValue;
		}
	}
	if ( __xgeSvgStylePropertyCopyDashArrayEx(pTag, pTagEnd, sStyle, sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, fLengthRef, pStyle->fFontSize) ) {
		if ( __xgeSvgStyleValueIsInherit(sValue) ) {
			pStyle->iDashCount = 0;
		} else {
			__xgeSvgParseDashArrayEx(sValue, fLengthRef, pStyle->fFontSize, pStyle->fDashPattern, &pStyle->iDashCount);
		}
	}
	{
		float fDashOffsetRef;
		fDashOffsetRef = __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_X);
		if ( __xgeSvgStylePropertyCopyEx(pTag, pTagEnd, sStyle, "stroke-dashoffset", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, 0) ) {
			pStyle->fDashOffset = __xgeSvgParseDashOffsetThorvg(sValue, fDashOffsetRef, pStyle->fFontSize);
		}
	}
	if ( __xgeSvgStylePropertyCopyValidEx(pTag, pTagEnd, sStyle, "clip-path", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, 0, __xgeSvgStyleValueValidUrlNone, NULL) ) {
		char sClipId[XGE_SVG_ID_MAX];
		if ( __xgeSvgStyleValueIsInherit(sValue) ) {
			if ( pInheritedStyle != NULL ) strcpy(pStyle->sClipId, pInheritedStyle->sClipId);
			else pStyle->sClipId[0] = '\0';
		} else if ( __xgeSvgStyleValueEquals(sValue, "none") ) {
			if ( pInheritedStyle != NULL ) strcpy(pStyle->sClipId, pInheritedStyle->sClipId);
			else pStyle->sClipId[0] = '\0';
		} else if ( __xgeSvgUrlIdCopyFromValue(sValue, sClipId, sizeof(sClipId)) ) {
			strcpy(pStyle->sClipId, sClipId);
		}
	}
	if ( __xgeSvgStylePropertyCopyValidEx(pTag, pTagEnd, sStyle, "mask", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, 0, __xgeSvgStyleValueValidUrlNone, NULL) ) {
		char sMaskId[XGE_SVG_ID_MAX];
		if ( __xgeSvgStyleValueIsInherit(sValue) ) {
			if ( pInheritedStyle != NULL ) strcpy(pStyle->sMaskId, pInheritedStyle->sMaskId);
			else pStyle->sMaskId[0] = '\0';
		} else if ( __xgeSvgStyleValueEquals(sValue, "none") ) {
			if ( pInheritedStyle != NULL ) strcpy(pStyle->sMaskId, pInheritedStyle->sMaskId);
			else pStyle->sMaskId[0] = '\0';
		} else if ( __xgeSvgUrlIdCopyFromValue(sValue, sMaskId, sizeof(sMaskId)) ) {
			strcpy(pStyle->sMaskId, sMaskId);
		}
	}
	if ( __xgeSvgStylePropertyCopyValidEx(pTag, pTagEnd, sStyle, "filter", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, 0, __xgeSvgStyleValueValidUrlNone, NULL) ) {
		char sFilterId[XGE_SVG_ID_MAX];
		if ( __xgeSvgStyleValueIsInherit(sValue) ) {
			if ( pInheritedStyle != NULL ) strcpy(pStyle->sFilterId, pInheritedStyle->sFilterId);
			else pStyle->sFilterId[0] = '\0';
		} else if ( __xgeSvgStyleValueEquals(sValue, "none") ) {
			pStyle->sFilterId[0] = '\0';
		} else if ( __xgeSvgUrlIdCopyFromValue(sValue, sFilterId, sizeof(sFilterId)) ) {
			strcpy(pStyle->sFilterId, sFilterId);
		}
	}
	if ( __xgeSvgStylePropertyCopyEx(pTag, pTagEnd, sStyle, "fill-rule", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, 0) ) {
		if ( __xgeSvgStyleValueEquals(sValue, "evenodd") ) pStyle->iFillRule = XGE_SHAPE_EX_FILL_EVEN_ODD;
		else pStyle->iFillRule = XGE_SHAPE_EX_FILL_NON_ZERO;
		pStyle->bFillRuleSet = 1;
	}
	if ( __xgeSvgStylePropertyCopyEx(pTag, pTagEnd, sStyle, "paint-order", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, bFirstDeclaration) ) {
		int arrPaintOrder[XGE_SVG_PAINT_COMPONENT_COUNT];

		if ( __xgeSvgParsePaintOrder(sValue, arrPaintOrder) ) {
			memcpy(pStyle->iPaintOrder, arrPaintOrder, sizeof(arrPaintOrder));
			__xgeSvgStyleUpdatePaintOrderDerived(pStyle);
		} else {
			__xgeSvgPaintOrderDefault(pStyle->iPaintOrder);
			__xgeSvgStyleUpdatePaintOrderDerived(pStyle);
		}
	}
	if ( __xgeSvgStylePropertyCopyEx(pTag, pTagEnd, sStyle, "stroke-linecap", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, 0) ) {
		if ( __xgeSvgStyleValueEquals(sValue, "butt") ) pStyle->iLineCap = XGE_SHAPE_EX_CAP_BUTT;
		else if ( __xgeSvgStyleValueEquals(sValue, "round") ) pStyle->iLineCap = XGE_SHAPE_EX_CAP_ROUND;
		else if ( __xgeSvgStyleValueEquals(sValue, "square") ) pStyle->iLineCap = XGE_SHAPE_EX_CAP_SQUARE;
		else pStyle->iLineCap = XGE_SHAPE_EX_CAP_BUTT;
	}
	if ( __xgeSvgStylePropertyCopyEx(pTag, pTagEnd, sStyle, "stroke-linejoin", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, 0) ) {
		if ( __xgeSvgStyleValueEquals(sValue, "miter") ) pStyle->iLineJoin = XGE_SHAPE_EX_JOIN_MITER;
		else if ( __xgeSvgStyleValueEquals(sValue, "round") ) pStyle->iLineJoin = XGE_SHAPE_EX_JOIN_ROUND;
		else if ( __xgeSvgStyleValueEquals(sValue, "bevel") ) pStyle->iLineJoin = XGE_SHAPE_EX_JOIN_BEVEL;
		else pStyle->iLineJoin = XGE_SHAPE_EX_JOIN_MITER;
	}
	if ( bAllowTransform && __xgeSvgStylePropertyCopyEx(pTag, pTagEnd, sStyle, "transform", sValue, sizeof(sValue), bAllowAttrs, iImportanceMode, 0) ) {
		xge_shape_ex_matrix_t tTransform;
		if ( __xgeSvgParseTransform(sValue, &tTransform) ) {
			pStyle->tTransform = __xgeSvgMatrixMul(tBaseTransform, tTransform);
		}
	}
}

static void __xgeSvgStyleApplyDeclaration(xge_svg pSvg, xge_svg_style_t* pStyle, const xge_svg_style_t* pInheritedStyle, const char* sStyle, int iImportanceMode, xge_shape_ex_matrix_t tBaseTransform, float fBaseOpacity, int bBaseVisible, float fBaseFontSize, int iApplyMode)
{
	__xgeSvgStyleApplyProperties(pSvg, pStyle, pInheritedStyle, NULL, NULL, sStyle, 0, 1, iImportanceMode, 1, tBaseTransform, fBaseOpacity, bBaseVisible, fBaseFontSize, iApplyMode);
}

static void __xgeSvgStyleApplyPresentationAttrs(xge_svg pSvg, xge_svg_style_t* pStyle, const xge_svg_style_t* pInheritedStyle, const char* pTag, const char* pTagEnd, xge_shape_ex_matrix_t tBaseTransform, float fBaseOpacity, int bBaseVisible, float fBaseFontSize, int iApplyMode)
{
	if ( (pStyle == NULL) || (pTag == NULL) || (pTagEnd == NULL) ) {
		return;
	}
	__xgeSvgStyleApplyProperties(pSvg, pStyle, pInheritedStyle, pTag, pTagEnd, NULL, 1, 1, XGE_SVG_STYLE_IMPORTANCE_ANY, 0, tBaseTransform, fBaseOpacity, bBaseVisible, fBaseFontSize, iApplyMode);
}

static void __xgeSvgStyleApplyInlineStyle(xge_svg pSvg, xge_svg_style_t* pStyle, const xge_svg_style_t* pInheritedStyle, const char* pTag, const char* pTagEnd, int iImportanceMode, xge_shape_ex_matrix_t tBaseTransform, float fBaseOpacity, int bBaseVisible, float fBaseFontSize, int iApplyMode)
{
	char sStyle[XGE_SVG_ATTR_MAX];

	if ( (pStyle == NULL) || (pTag == NULL) || (pTagEnd == NULL) ) {
		return;
	}
	sStyle[0] = '\0';
	__xgeSvgAttrCopy(pTag, pTagEnd, "style", sStyle, sizeof(sStyle));
	__xgeSvgStyleApplyProperties(pSvg, pStyle, pInheritedStyle, NULL, NULL, sStyle, 0, 1, iImportanceMode, 0, tBaseTransform, fBaseOpacity, bBaseVisible, fBaseFontSize, iApplyMode);
}

static int __xgeSvgClassListContains(const char* sClassList, const char* sName)
{
	const char* p;
	int iNameLen;

	if ( (sClassList == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return 0;
	}
	iNameLen = (int)strlen(sName);
	p = sClassList;
	while ( *p != '\0' ) {
		const char* pStart;
		const char* pEnd;
		while ( (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') ) {
			p++;
		}
		pStart = p;
		while ( (*p != '\0') && (*p != ' ') && (*p != '\t') && (*p != '\r') && (*p != '\n') ) {
			p++;
		}
		pEnd = p;
		if ( ((int)(pEnd - pStart) == iNameLen) && (strncmp(pStart, sName, (size_t)iNameLen) == 0) ) {
			return 1;
		}
	}
	return 0;
}

static int __xgeSvgCssCharEquals(char a, char b, int bCaseInsensitive)
{
	if ( bCaseInsensitive ) {
		a = (char)__xgeSvgCssAsciiLower((unsigned char)a);
		b = (char)__xgeSvgCssAsciiLower((unsigned char)b);
	}
	return a == b;
}

static int __xgeSvgStyleRuleMatches(xge_svg pSvg, const xge_svg_style_rule_t* pRule, const char* pTag, const char* pTagEnd)
{
	char sValue[XGE_SVG_ATTR_MAX];
	char sTag[XGE_SVG_ATTR_MAX];
	char sClass[XGE_SVG_ATTR_MAX];

	if ( !__xgeSvgValid(pSvg) || (pRule == NULL) || (pRule->sName == NULL) || (pRule->sStyle == NULL) || (pTag == NULL) || (pTagEnd == NULL) ) {
		return 0;
	}
	if ( pRule->iSelectorType == XGE_SVG_STYLE_SELECTOR_TAG ) {
		return __xgeSvgTagNameEquals(pTag, pRule->sName);
	}
	if ( pRule->iSelectorType == XGE_SVG_STYLE_SELECTOR_CLASS ) {
		return __xgeSvgAttrCopy(pTag, pTagEnd, "class", sValue, sizeof(sValue)) && __xgeSvgClassListContains(sValue, pRule->sName);
	}
	if ( pRule->iSelectorType == XGE_SVG_STYLE_SELECTOR_TAG_CLASS ) {
		const char* pDot = strchr(pRule->sName, '.');
		int iTagLen;
		int iClassLen;

		if ( pDot == NULL ) {
			return 0;
		}
		iTagLen = (int)(pDot - pRule->sName);
		iClassLen = (int)strlen(pDot + 1);
		if ( (iTagLen <= 0) || (iTagLen >= (int)sizeof(sTag)) || (iClassLen <= 0) || (iClassLen >= (int)sizeof(sClass)) ) {
			return 0;
		}
		memcpy(sTag, pRule->sName, (size_t)iTagLen);
		sTag[iTagLen] = '\0';
		memcpy(sClass, pDot + 1, (size_t)iClassLen);
		sClass[iClassLen] = '\0';
		return __xgeSvgTagNameEquals(pTag, sTag) &&
		       __xgeSvgAttrCopy(pTag, pTagEnd, "class", sValue, sizeof(sValue)) &&
		       __xgeSvgClassListContains(sValue, sClass);
	}
	return 0;
}

static int __xgeSvgClassTokenSeenBefore(const char* sClassList, const char* pToken, int iTokenLen)
{
	const char* p;

	if ( (sClassList == NULL) || (pToken == NULL) || (iTokenLen <= 0) ) {
		return 0;
	}
	p = sClassList;
	while ( p < pToken ) {
		const char* pStart;
		const char* pEnd;

		while ( (p < pToken) && ((*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n')) ) p++;
		pStart = p;
		while ( (p < pToken) && (*p != ' ') && (*p != '\t') && (*p != '\r') && (*p != '\n') ) p++;
		pEnd = p;
		if ( ((int)(pEnd - pStart) == iTokenLen) && (strncmp(pStart, pToken, (size_t)iTokenLen) == 0) ) {
			return 1;
		}
	}
	return 0;
}

static int __xgeSvgStyleRuleMatchesClassToken(const xge_svg_style_rule_t* pRule, int iSelectorType, const char* pTag, const char* pToken, int iTokenLen)
{
	char sTag[XGE_SVG_ATTR_MAX];
	const char* pDot;
	int iTagLen;
	int iClassLen;

	if ( (pRule == NULL) || (pRule->iSelectorType != iSelectorType) || (pRule->sName == NULL) ||
	     (pToken == NULL) || (iTokenLen <= 0) ) {
		return 0;
	}
	if ( iSelectorType == XGE_SVG_STYLE_SELECTOR_CLASS ) {
		return ((int)strlen(pRule->sName) == iTokenLen) &&
		       (strncmp(pRule->sName, pToken, (size_t)iTokenLen) == 0);
	}
	if ( iSelectorType != XGE_SVG_STYLE_SELECTOR_TAG_CLASS ) {
		return 0;
	}
	pDot = strchr(pRule->sName, '.');
	if ( pDot == NULL ) {
		return 0;
	}
	iTagLen = (int)(pDot - pRule->sName);
	iClassLen = (int)strlen(pDot + 1);
	if ( (iTagLen <= 0) || (iTagLen >= (int)sizeof(sTag)) ) {
		return 0;
	}
	memcpy(sTag, pRule->sName, (size_t)iTagLen);
	sTag[iTagLen] = '\0';
	return (iClassLen == iTokenLen) &&
	       (strncmp(pDot + 1, pToken, (size_t)iTokenLen) == 0) &&
	       __xgeSvgTagNameEquals(pTag, sTag);
}

static void __xgeSvgStyleApplyCssRule(xge_svg pSvg, xge_svg_style_t* pStyle, const xge_svg_style_t* pInheritedStyle, const xge_svg_style_rule_t* pRule, int iImportanceMode, xge_shape_ex_matrix_t tBaseTransform, float fBaseOpacity, int bBaseVisible, float fBaseFontSize, int iApplyMode)
{
	char sDashArray[XGE_SVG_ATTR_MAX];
	float arrDashPattern[XGE_SVG_DASH_MAX];
	float arrPreviousDashPattern[XGE_SVG_DASH_MAX];
	float fLengthRef;
	int iDashCount;
	int iPreviousDashCount;
	int bDashArray;

	if ( (pStyle == NULL) || (pRule == NULL) || (pRule->sStyle == NULL) ) {
		return;
	}
	iPreviousDashCount = pStyle->iDashCount;
	memcpy(arrPreviousDashPattern, pStyle->fDashPattern, sizeof(arrPreviousDashPattern));
	bDashArray = __xgeSvgStyleCopyEx(pRule->sStyle, "stroke-dasharray", sDashArray, sizeof(sDashArray), iImportanceMode, 0);
	__xgeSvgStyleApplyDeclaration(pSvg, pStyle, pInheritedStyle, pRule->sStyle, iImportanceMode, tBaseTransform, fBaseOpacity, bBaseVisible, fBaseFontSize, iApplyMode);
	if ( !bDashArray ) {
		return;
	}
	fLengthRef = __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_OTHER);
	iDashCount = 0;
	__xgeSvgParseDashArrayEx(sDashArray, fLengthRef, pStyle->fFontSize, arrDashPattern, &iDashCount);
	if ( iDashCount <= 0 ) {
		pStyle->iDashCount = iPreviousDashCount;
		memcpy(pStyle->fDashPattern, arrPreviousDashPattern, sizeof(arrPreviousDashPattern));
	}
}

static void __xgeSvgStyleApplyCss(xge_svg pSvg, xge_svg_style_t* pStyle, const xge_svg_style_t* pInheritedStyle, const char* pTag, const char* pTagEnd, int iImportanceMode, xge_shape_ex_matrix_t tBaseTransform, float fBaseOpacity, int bBaseVisible, float fBaseFontSize, int iApplyMode)
{
	char sClassList[XGE_SVG_ATTR_MAX];
	char sFilterId[XGE_SVG_ID_MAX];
	const char* pClass;
	float fDashOffset;
	float fMiterLimit;
	int iMaskType;
	int i;

	if ( !__xgeSvgValid(pSvg) || (pStyle == NULL) || (pTag == NULL) || (pTagEnd == NULL) ) {
		return;
	}
	fDashOffset = pStyle->fDashOffset;
	fMiterLimit = pStyle->fMiterLimit;
	strcpy(sFilterId, pStyle->sFilterId);
	iMaskType = pStyle->iMaskType;
	for ( i = 0; i < pSvg->iStyleRuleCount; i++ ) {
		if ( (pSvg->pStyleRules[i].iSelectorType == XGE_SVG_STYLE_SELECTOR_TAG) &&
		     __xgeSvgStyleRuleMatches(pSvg, &pSvg->pStyleRules[i], pTag, pTagEnd) ) {
			__xgeSvgStyleApplyCssRule(pSvg, pStyle, pInheritedStyle, &pSvg->pStyleRules[i], iImportanceMode, tBaseTransform, fBaseOpacity, bBaseVisible, fBaseFontSize, iApplyMode);
			break;
		}
	}
	sClassList[0] = '\0';
	(void)__xgeSvgAttrCopy(pTag, pTagEnd, "class", sClassList, sizeof(sClassList));
	pClass = sClassList;
	while ( *pClass != '\0' ) {
		const char* pToken;
		int iTokenLen;

		while ( (*pClass == ' ') || (*pClass == '\t') || (*pClass == '\r') || (*pClass == '\n') ) pClass++;
		pToken = pClass;
		while ( (*pClass != '\0') && (*pClass != ' ') && (*pClass != '\t') && (*pClass != '\r') && (*pClass != '\n') ) pClass++;
		iTokenLen = (int)(pClass - pToken);
		if ( (iTokenLen <= 0) || __xgeSvgClassTokenSeenBefore(sClassList, pToken, iTokenLen) ) {
			continue;
		}
		for ( i = 0; i < pSvg->iStyleRuleCount; i++ ) {
			if ( __xgeSvgStyleRuleMatchesClassToken(&pSvg->pStyleRules[i], XGE_SVG_STYLE_SELECTOR_CLASS, pTag, pToken, iTokenLen) ) {
				__xgeSvgStyleApplyCssRule(pSvg, pStyle, pInheritedStyle, &pSvg->pStyleRules[i], iImportanceMode, tBaseTransform, fBaseOpacity, bBaseVisible, fBaseFontSize, iApplyMode);
				break;
			}
		}
		for ( i = 0; i < pSvg->iStyleRuleCount; i++ ) {
			if ( __xgeSvgStyleRuleMatchesClassToken(&pSvg->pStyleRules[i], XGE_SVG_STYLE_SELECTOR_TAG_CLASS, pTag, pToken, iTokenLen) ) {
				__xgeSvgStyleApplyCssRule(pSvg, pStyle, pInheritedStyle, &pSvg->pStyleRules[i], iImportanceMode, tBaseTransform, fBaseOpacity, bBaseVisible, fBaseFontSize, iApplyMode);
				break;
			}
		}
	}
	/* ThorVG's CSS node copier omits these parsed properties. */
	pStyle->fDashOffset = fDashOffset;
	pStyle->fMiterLimit = fMiterLimit;
	strcpy(pStyle->sFilterId, sFilterId);
	pStyle->iMaskType = iMaskType;
}

static void __xgeSvgStyleThorvgUrlApplyValue(const char* sValue, char* sId)
{
	char sParsedId[XGE_SVG_ID_MAX];
	int bHandled;

	if ( (sValue == NULL) || (sId == NULL) ) {
		return;
	}
	if ( __xgeSvgUrlIdCopyFromValueThorvg(sValue, sParsedId, sizeof(sParsedId), &bHandled) ) {
		strcpy(sId, sParsedId);
	} else if ( bHandled ) {
		sId[0] = '\0';
	}
}

static void __xgeSvgStyleThorvgUrlApplyDeclaration(const char* sStyle, const char* sName, char* sId)
{
	if ( (sStyle == NULL) || (sName == NULL) || (sId == NULL) ) {
		return;
	}
	__xgeSvgStyleReplayThorvgUrl(sStyle, sName, sId, XGE_SVG_ID_MAX, NULL);
}

static void __xgeSvgStyleThorvgUrlApplyRule(const xge_svg_style_rule_t* pRule, char* sClipId, char* sMaskId)
{
	if ( (pRule == NULL) || (pRule->sStyle == NULL) ) {
		return;
	}
	__xgeSvgStyleThorvgUrlApplyDeclaration(pRule->sStyle, "clip-path", sClipId);
	__xgeSvgStyleThorvgUrlApplyDeclaration(pRule->sStyle, "mask", sMaskId);
}

static int __xgeSvgStyleThorvgUrlClassesAvailable(xge_svg pSvg, const char* pTag, const char* sClassList)
{
	const char* pClass;
	int i;

	if ( !__xgeSvgValid(pSvg) || (pTag == NULL) || (sClassList == NULL) ) {
		return 0;
	}
	pClass = sClassList;
	while ( *pClass != '\0' ) {
		const char* pToken;
		int iTokenLen;
		int bFound;

		while ( (*pClass == ' ') || (*pClass == '\t') || (*pClass == '\r') || (*pClass == '\n') ) pClass++;
		pToken = pClass;
		while ( (*pClass != '\0') && (*pClass != ' ') && (*pClass != '\t') && (*pClass != '\r') && (*pClass != '\n') ) pClass++;
		iTokenLen = (int)(pClass - pToken);
		if ( (iTokenLen <= 0) || __xgeSvgClassTokenSeenBefore(sClassList, pToken, iTokenLen) ) {
			continue;
		}
		bFound = 0;
		for ( i = 0; i < pSvg->iStyleRuleCount; i++ ) {
			const xge_svg_style_rule_t* pRule = &pSvg->pStyleRules[i];

			if ( (pRule->pSourcePosition < pTag) &&
			     (__xgeSvgStyleRuleMatchesClassToken(pRule, XGE_SVG_STYLE_SELECTOR_CLASS, pTag, pToken, iTokenLen) ||
			      __xgeSvgStyleRuleMatchesClassToken(pRule, XGE_SVG_STYLE_SELECTOR_TAG_CLASS, pTag, pToken, iTokenLen)) ) {
				bFound = 1;
				break;
			}
		}
		if ( !bFound ) return 0;
	}
	return 1;
}

static void __xgeSvgStyleThorvgUrlApplyClasses(xge_svg pSvg, const char* pTag, const char* sClassList, int bBeforeElementOnly, char* sClipId, char* sMaskId)
{
	const char* pClass;
	int i;

	if ( !__xgeSvgValid(pSvg) || (pTag == NULL) || (sClassList == NULL) || (sClipId == NULL) || (sMaskId == NULL) ) {
		return;
	}
	pClass = sClassList;
	while ( *pClass != '\0' ) {
		const char* pToken;
		int iTokenLen;

		while ( (*pClass == ' ') || (*pClass == '\t') || (*pClass == '\r') || (*pClass == '\n') ) pClass++;
		pToken = pClass;
		while ( (*pClass != '\0') && (*pClass != ' ') && (*pClass != '\t') && (*pClass != '\r') && (*pClass != '\n') ) pClass++;
		iTokenLen = (int)(pClass - pToken);
		if ( (iTokenLen <= 0) || __xgeSvgClassTokenSeenBefore(sClassList, pToken, iTokenLen) ) {
			continue;
		}
		for ( i = 0; i < pSvg->iStyleRuleCount; i++ ) {
			const xge_svg_style_rule_t* pRule = &pSvg->pStyleRules[i];

			if ( (!bBeforeElementOnly || (pRule->pSourcePosition < pTag)) &&
			     __xgeSvgStyleRuleMatchesClassToken(pRule, XGE_SVG_STYLE_SELECTOR_CLASS, pTag, pToken, iTokenLen) ) {
				__xgeSvgStyleThorvgUrlApplyRule(pRule, sClipId, sMaskId);
				break;
			}
		}
		for ( i = 0; i < pSvg->iStyleRuleCount; i++ ) {
			const xge_svg_style_rule_t* pRule = &pSvg->pStyleRules[i];

			if ( (!bBeforeElementOnly || (pRule->pSourcePosition < pTag)) &&
			     __xgeSvgStyleRuleMatchesClassToken(pRule, XGE_SVG_STYLE_SELECTOR_TAG_CLASS, pTag, pToken, iTokenLen) ) {
				__xgeSvgStyleThorvgUrlApplyRule(pRule, sClipId, sMaskId);
				break;
			}
		}
	}
}

static void __xgeSvgStyleResolveThorvgUrlOrder(xge_svg pSvg, xge_svg_style_t* pStyle, const char* pTag, const char* pTagEnd)
{
	const char* pCursor;
	char sName[64];
	char sValue[XGE_SVG_ATTR_MAX];
	char sClipId[XGE_SVG_ID_MAX];
	char sMaskId[XGE_SVG_ID_MAX];
	char sFilterId[XGE_SVG_ID_MAX];
	char sDeferredClasses[XGE_SVG_ATTR_MAX];
	int i;

	if ( !__xgeSvgValid(pSvg) || (pStyle == NULL) || (pTag == NULL) || (pTagEnd == NULL) ) {
		return;
	}
	strcpy(sClipId, pStyle->sClipId);
	strcpy(sMaskId, pStyle->sMaskId);
	strcpy(sFilterId, pStyle->sFilterId);
	sDeferredClasses[0] = '\0';
	pCursor = pTag;
	while ( __xgeSvgAttrNext(&pCursor, pTagEnd, sName, sizeof(sName), sValue, sizeof(sValue)) ) {
		if ( strcmp(sName, "clip-path") == 0 ) {
			__xgeSvgStyleThorvgUrlApplyValue(sValue, sClipId);
		} else if ( strcmp(sName, "mask") == 0 ) {
			__xgeSvgStyleThorvgUrlApplyValue(sValue, sMaskId);
		} else if ( strcmp(sName, "filter") == 0 ) {
			__xgeSvgStyleThorvgUrlApplyValue(sValue, sFilterId);
		} else if ( strcmp(sName, "style") == 0 ) {
			__xgeSvgStyleThorvgUrlApplyDeclaration(sValue, "clip-path", sClipId);
			__xgeSvgStyleThorvgUrlApplyDeclaration(sValue, "mask", sMaskId);
			__xgeSvgStyleThorvgUrlApplyDeclaration(sValue, "filter", sFilterId);
		} else if ( strcmp(sName, "class") == 0 ) {
			if ( __xgeSvgStyleThorvgUrlClassesAvailable(pSvg, pTag, sValue) ) {
				__xgeSvgStyleThorvgUrlApplyClasses(pSvg, pTag, sValue, 1, sClipId, sMaskId);
			} else {
				snprintf(sDeferredClasses, sizeof(sDeferredClasses), "%s", sValue);
				sDeferredClasses[sizeof(sDeferredClasses) - 1] = '\0';
			}
		}
	}
	if ( sDeferredClasses[0] != '\0' ) {
		__xgeSvgStyleThorvgUrlApplyClasses(pSvg, pTag, sDeferredClasses, 0, sClipId, sMaskId);
	}
	/* ThorVG applies tag selectors after parsing every element attribute. */
	for ( i = 0; i < pSvg->iStyleRuleCount; i++ ) {
		if ( (pSvg->pStyleRules[i].iSelectorType == XGE_SVG_STYLE_SELECTOR_TAG) &&
		     __xgeSvgStyleRuleMatches(pSvg, &pSvg->pStyleRules[i], pTag, pTagEnd) ) {
			__xgeSvgStyleThorvgUrlApplyRule(&pSvg->pStyleRules[i], sClipId, sMaskId);
			break;
		}
	}
	strcpy(pStyle->sClipId, sClipId);
	strcpy(pStyle->sMaskId, sMaskId);
	strcpy(pStyle->sFilterId, sFilterId);
}

static void __xgeSvgStyleThorvgPaintUrlApplyValue(xge_svg_style_t* pStyle, const char* sValue, int bStroke)
{
	char sPaintId[XGE_SVG_ID_MAX];
	int bHandled;

	if ( (pStyle == NULL) || (sValue == NULL) ) {
		return;
	}
	if ( __xgeSvgUrlIdCopyFromValueThorvg(sValue, sPaintId, sizeof(sPaintId), &bHandled) ) {
		if ( bStroke ) {
			pStyle->bStrokeSet = 1;
			pStyle->bStrokeCurrentColor = 0;
			strcpy(pStyle->sStrokeGradientId, sPaintId);
			pStyle->iStrokeColor = XGE_COLOR_RGBA(0, 0, 0, 0);
		} else {
			pStyle->bFillSet = 1;
			pStyle->bFillCurrentColor = 0;
			strcpy(pStyle->sFillGradientId, sPaintId);
			pStyle->iFillColor = XGE_COLOR_RGBA(0, 0, 0, 0);
		}
	} else if ( bHandled ) {
		if ( bStroke ) {
			pStyle->bStrokeSet = 1;
			pStyle->bStrokeCurrentColor = 0;
			pStyle->sStrokeGradientId[0] = '\0';
			pStyle->iStrokeColor = XGE_COLOR_RGBA(0, 0, 0, 255);
		} else {
			pStyle->bFillSet = 1;
			pStyle->bFillCurrentColor = 0;
			pStyle->sFillGradientId[0] = '\0';
			pStyle->iFillColor = XGE_COLOR_RGBA(0, 0, 0, 255);
		}
	}
}

static void __xgeSvgStyleResolveThorvgDuplicatePaintUrls(xge_svg_style_t* pStyle, const char* pTag, const char* pTagEnd)
{
	const char* pCursor;
	char sName[64];
	char sValue[XGE_SVG_ATTR_MAX];
	int iFillCount;
	int iStrokeCount;

	if ( (pStyle == NULL) || (pTag == NULL) || (pTagEnd == NULL) ) {
		return;
	}
	iFillCount = 0;
	iStrokeCount = 0;
	pCursor = pTag;
	while ( __xgeSvgAttrNext(&pCursor, pTagEnd, sName, sizeof(sName), sValue, sizeof(sValue)) ) {
		if ( strcmp(sName, "fill") == 0 ) iFillCount++;
		else if ( strcmp(sName, "stroke") == 0 ) iStrokeCount++;
	}
	if ( (iFillCount <= 1) && (iStrokeCount <= 1) ) {
		return;
	}
	pCursor = pTag;
	while ( __xgeSvgAttrNext(&pCursor, pTagEnd, sName, sizeof(sName), sValue, sizeof(sValue)) ) {
		if ( (iFillCount > 1) && (strcmp(sName, "fill") == 0) ) {
			__xgeSvgStyleThorvgPaintUrlApplyValue(pStyle, sValue, 0);
		} else if ( (iStrokeCount > 1) && (strcmp(sName, "stroke") == 0) ) {
			__xgeSvgStyleThorvgPaintUrlApplyValue(pStyle, sValue, 1);
		}
	}
}

static int __xgeSvgClipValueDeclaresId(const char* sValue, const char* sClipId)
{
	char sParsedId[XGE_SVG_ID_MAX];
	int bHandled;

	if ( (sValue == NULL) || (sClipId == NULL) || (sClipId[0] == '\0') ) {
		return 0;
	}
	bHandled = 0;
	return __xgeSvgUrlIdCopyFromValueThorvg(
		sValue, sParsedId, sizeof(sParsedId), &bHandled
	) && (strcmp(sParsedId, sClipId) == 0);
}

static int __xgeSvgStyleDeclaresClipId(const char* sStyle, const char* sClipId)
{
	char sParsedId[XGE_SVG_ID_MAX];

	if ( (sStyle == NULL) || (sClipId == NULL) || (sClipId[0] == '\0') ) {
		return 0;
	}
	sParsedId[0] = '\0';
	__xgeSvgStyleThorvgUrlApplyDeclaration(sStyle, "clip-path", sParsedId);
	return strcmp(sParsedId, sClipId) == 0;
}

static int __xgeSvgElementDeclaresClipPath(
	xge_svg pSvg,
	const char* pTag,
	const char* pTagEnd,
	const char* sClipId
)
{
	char sStyle[XGE_SVG_ATTR_MAX];
	char sValue[XGE_SVG_ATTR_MAX];
	int i;

	if ( !__xgeSvgValid(pSvg) || (pTag == NULL) || (pTagEnd == NULL) ||
	     (sClipId == NULL) || (sClipId[0] == '\0') ) {
		return 0;
	}
	if ( __xgeSvgAttrCopy(pTag, pTagEnd, "clip-path", sValue, sizeof(sValue)) &&
	     __xgeSvgClipValueDeclaresId(sValue, sClipId) ) {
		return 1;
	}
	if ( __xgeSvgAttrCopy(pTag, pTagEnd, "style", sStyle, sizeof(sStyle)) &&
	     __xgeSvgStyleDeclaresClipId(sStyle, sClipId) ) {
		return 1;
	}
	for ( i = 0; i < pSvg->iStyleRuleCount; i++ ) {
		if ( __xgeSvgStyleRuleMatches(pSvg, &pSvg->pStyleRules[i], pTag, pTagEnd) &&
		     __xgeSvgStyleDeclaresClipId(pSvg->pStyleRules[i].sStyle, sClipId) ) {
			return 1;
		}
	}
	return 0;
}

static xge_svg_style_t __xgeSvgStyleResolveMode(xge_svg pSvg, const xge_svg_style_t* pParentStyle, const char* pTag, const char* pTagEnd, int bApplyCss)
{
	xge_svg_style_t tStyle;
	xge_svg_style_t tDefaultStyle;
	const xge_svg_style_t* pInheritedStyle;
	xge_shape_ex_matrix_t tBaseTransform;
	float fBaseOpacity;
	float fBaseFontSize;
	int bBaseVisible;

	(void)__xgeSvgElementNodeEnsure(pSvg, pTag, pTagEnd, NULL);
	if ( pParentStyle != NULL ) {
		tStyle = *pParentStyle;
		pInheritedStyle = pParentStyle;
		tStyle.sFilterId[0] = '\0';
		tStyle.iBlend = XGE_BLEND_ALPHA;
		tStyle.bBlendSet = 0;
	} else {
		tDefaultStyle = __xgeSvgStyleDefault();
		tStyle = tDefaultStyle;
		pInheritedStyle = &tDefaultStyle;
	}
	if ( __xgeSvgTagNameEquals(pTag, "text") ) {
		tStyle.fFontSize = XGE_SVG_DEFAULT_FONT_SIZE;
		tStyle.sFontFamily[0] = '\0';
	}
	if ( __xgeSvgTagNameEquals(pTag, "mask") ) {
		tStyle.iMaskType = XGE_SVG_MASK_TYPE_LUMINANCE;
	}
	tStyle.bStrokeNonScaling = 0;
	tBaseTransform = tStyle.tTransform;
	fBaseOpacity = tStyle.fOpacity;
	fBaseFontSize = tStyle.fFontSize;
	bBaseVisible = tStyle.bVisible;
	__xgeSvgStyleApplyPresentationAttrs(pSvg, &tStyle, pInheritedStyle, pTag, pTagEnd, tBaseTransform, fBaseOpacity, bBaseVisible, fBaseFontSize, XGE_SVG_STYLE_APPLY_FONT_SIZE);
	if ( bApplyCss ) __xgeSvgStyleApplyCss(pSvg, &tStyle, pInheritedStyle, pTag, pTagEnd, XGE_SVG_STYLE_IMPORTANCE_NORMAL, tBaseTransform, fBaseOpacity, bBaseVisible, fBaseFontSize, XGE_SVG_STYLE_APPLY_FONT_SIZE);
	__xgeSvgStyleApplyInlineStyle(pSvg, &tStyle, pInheritedStyle, pTag, pTagEnd, XGE_SVG_STYLE_IMPORTANCE_NORMAL, tBaseTransform, fBaseOpacity, bBaseVisible, fBaseFontSize, XGE_SVG_STYLE_APPLY_FONT_SIZE);
	if ( bApplyCss ) __xgeSvgStyleApplyCss(pSvg, &tStyle, pInheritedStyle, pTag, pTagEnd, XGE_SVG_STYLE_IMPORTANCE_IMPORTANT, tBaseTransform, fBaseOpacity, bBaseVisible, fBaseFontSize, XGE_SVG_STYLE_APPLY_FONT_SIZE);
	__xgeSvgStyleApplyInlineStyle(pSvg, &tStyle, pInheritedStyle, pTag, pTagEnd, XGE_SVG_STYLE_IMPORTANCE_IMPORTANT, tBaseTransform, fBaseOpacity, bBaseVisible, fBaseFontSize, XGE_SVG_STYLE_APPLY_FONT_SIZE);
	__xgeSvgStyleApplyPresentationAttrs(pSvg, &tStyle, pInheritedStyle, pTag, pTagEnd, tBaseTransform, fBaseOpacity, bBaseVisible, fBaseFontSize, XGE_SVG_STYLE_APPLY_EXCEPT_FONT_SIZE);
	if ( bApplyCss ) __xgeSvgStyleApplyCss(pSvg, &tStyle, pInheritedStyle, pTag, pTagEnd, XGE_SVG_STYLE_IMPORTANCE_NORMAL, tBaseTransform, fBaseOpacity, bBaseVisible, fBaseFontSize, XGE_SVG_STYLE_APPLY_EXCEPT_FONT_SIZE);
	__xgeSvgStyleApplyInlineStyle(pSvg, &tStyle, pInheritedStyle, pTag, pTagEnd, XGE_SVG_STYLE_IMPORTANCE_NORMAL, tBaseTransform, fBaseOpacity, bBaseVisible, fBaseFontSize, XGE_SVG_STYLE_APPLY_EXCEPT_FONT_SIZE);
	if ( bApplyCss ) __xgeSvgStyleApplyCss(pSvg, &tStyle, pInheritedStyle, pTag, pTagEnd, XGE_SVG_STYLE_IMPORTANCE_IMPORTANT, tBaseTransform, fBaseOpacity, bBaseVisible, fBaseFontSize, XGE_SVG_STYLE_APPLY_EXCEPT_FONT_SIZE);
	__xgeSvgStyleApplyInlineStyle(pSvg, &tStyle, pInheritedStyle, pTag, pTagEnd, XGE_SVG_STYLE_IMPORTANCE_IMPORTANT, tBaseTransform, fBaseOpacity, bBaseVisible, fBaseFontSize, XGE_SVG_STYLE_APPLY_EXCEPT_FONT_SIZE);
	if ( bApplyCss ) __xgeSvgStyleResolveThorvgUrlOrder(pSvg, &tStyle, pTag, pTagEnd);
	__xgeSvgStyleResolveThorvgDuplicatePaintUrls(&tStyle, pTag, pTagEnd);
	tStyle.bClipPathLocal = __xgeSvgElementDeclaresClipPath(
		pSvg, pTag, pTagEnd, tStyle.sClipId
	);
	return tStyle;
}

static xge_svg_style_t __xgeSvgStyleResolve(xge_svg pSvg, const xge_svg_style_t* pParentStyle, const char* pTag, const char* pTagEnd)
{
	return __xgeSvgStyleResolveMode(pSvg, pParentStyle, pTag, pTagEnd, 1);
}

static xge_svg_style_t __xgeSvgStyleResolveMaskContent(xge_svg pSvg, const xge_svg_style_t* pParentStyle, const char* pTag, const char* pTagEnd)
{
	return __xgeSvgStyleResolveMode(pSvg, pParentStyle, pTag, pTagEnd, 0);
}

static xge_svg_clip_path_t* __xgeSvgClipPathGetById(xge_svg pSvg, const char* sClipId)
{
	int iClip;

	if ( !__xgeSvgValid(pSvg) || (sClipId == NULL) || (sClipId[0] == '\0') ) {
		return NULL;
	}
	iClip = __xgeSvgClipPathFind(pSvg, sClipId);
	if ( (iClip < 0) ||
	     ((!pSvg->pClipPaths[iClip].bHasRect || (pSvg->pClipPaths[iClip].iRectCount <= 0)) &&
	      (!pSvg->pClipPaths[iClip].bHasShape || (pSvg->pClipPaths[iClip].iShapeCount <= 0))) ) {
		return NULL;
	}
	return &pSvg->pClipPaths[iClip];
}

static int __xgeSvgClipPathResolveRect(xge_svg_clip_path_t* pClip, int iRect, xge_shape_ex pShape, xge_rect_t* pRect)
{
	xge_rect_t tClip;

	if ( (pClip == NULL) || (pShape == NULL) || (pRect == NULL) || (iRect < 0) || (iRect >= pClip->iRectCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tClip = pClip->pRects[iRect];
	if ( pClip->iUnits == XGE_SVG_CLIP_OBJECT_BOUNDING_BOX ) {
		xge_rect_t tBounds;

		if ( xgeShapeExGetBounds(pShape, 0.0f, &tBounds) != XGE_OK ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		tClip.fX = tBounds.fX + tClip.fX * tBounds.fW;
		tClip.fY = tBounds.fY + tClip.fY * tBounds.fH;
		tClip.fW = tClip.fW * tBounds.fW;
		tClip.fH = tClip.fH * tBounds.fH;
	}
	*pRect = tClip;
	return XGE_OK;
}

static int __xgeSvgApplyClipShapes(xge_svg_clip_path_t* pClip, xge_shape_ex pShape)
{
	xge_rect_t tBounds;
	xge_shape_ex_matrix_t tBoundsMatrix;
	int bHasBounds;
	int i;
	int iRet;

	if ( (pClip == NULL) || (pShape == NULL) || (pClip->iShapeCount <= 0) ) {
		return XGE_OK;
	}
	bHasBounds = 0;
	memset(&tBounds, 0, sizeof(tBounds));
	tBoundsMatrix = __xgeSvgMatrixIdentity();
	if ( pClip->iUnits == XGE_SVG_CLIP_OBJECT_BOUNDING_BOX ) {
		iRet = xgeShapeExGetBounds(pShape, 0.0f, &tBounds);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		if ( (tBounds.fW <= 0.0f) || (tBounds.fH <= 0.0f) ) {
			return XGE_OK;
		}
		tBoundsMatrix = __xgeSvgMatrixMul(__xgeSvgMatrixTranslate(tBounds.fX, tBounds.fY), __xgeSvgMatrixScale(tBounds.fW, tBounds.fH));
		bHasBounds = 1;
	}
	for ( i = 0; i < pClip->iShapeCount; i++ ) {
		xge_shape_ex pClipShape;

		if ( pClip->pShapes[i] == NULL ) {
			continue;
		}
		pClipShape = NULL;
		iRet = xgeShapeExClone(pClip->pShapes[i], &pClipShape);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		if ( bHasBounds ) {
			xge_shape_ex_matrix_t tTransform;

			tTransform = __xgeSvgMatrixMul(tBoundsMatrix, pClipShape->tTransform);
			xgeShapeExTransformSet(pClipShape, &tTransform);
		}
		iRet = xgeShapeExClipShapeAdd(pShape, pClipShape);
		xgeShapeExDestroy(pClipShape);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	return XGE_OK;
}

static int __xgeSvgApplyClipRectsAsShapes(xge_svg_clip_path_t* pClip, xge_shape_ex pShape)
{
	int i;
	int iRet;

	if ( (pClip == NULL) || (pShape == NULL) || (pClip->iRectCount <= 0) ) {
		return XGE_OK;
	}
	for ( i = 0; i < pClip->iRectCount; i++ ) {
		xge_shape_ex pClipShape;
		xge_rect_t tClip;

		iRet = __xgeSvgClipPathResolveRect(pClip, i, pShape, &tClip);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		if ( (tClip.fW <= 0.0f) || (tClip.fH <= 0.0f) ) {
			continue;
		}
		pClipShape = NULL;
		iRet = xgeShapeExCreate(&pClipShape);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		iRet = xgeShapeExAppendRect(pClipShape, tClip.fX, tClip.fY, tClip.fW, tClip.fH, 0.0f, 0.0f, 1);
		if ( iRet == XGE_OK ) {
			iRet = xgeShapeExClipShapeAdd(pShape, pClipShape);
		}
		xgeShapeExDestroy(pClipShape);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	return XGE_OK;
}

static int __xgeSvgApplyClipPath(xge_svg pSvg, xge_shape_ex pShape, const char* sClipId)
{
	xge_svg_clip_path_t* pClip;
	xge_rect_t tClip;
	int iRet;

	pClip = __xgeSvgClipPathGetById(pSvg, sClipId);
	if ( (pClip == NULL) || (pShape == NULL) ) {
		return XGE_OK;
	}
	if ( (pClip->iShapeCount <= 0) && (pClip->iRectCount == 1) ) {
		if ( __xgeSvgClipPathResolveRect(pClip, 0, pShape, &tClip) != XGE_OK ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		xgeShapeExClipRectSet(pShape, tClip);
	} else if ( (pClip->iShapeCount <= 0) && (pClip->iRectCount > 1) ) {
		iRet = __xgeSvgApplyClipRectsAsShapes(pClip, pShape);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	iRet = __xgeSvgApplyClipShapes(pClip, pShape);
	return iRet;
}

static float __xgeSvgGradientResolvedLength(xge_svg pSvg, float fValue, int iUnits, int iBasis, int bPercentage)
{
	if ( bPercentage && (iUnits == XGE_SHAPE_EX_GRADIENT_USER_SPACE) ) {
		return fValue * __xgeSvgLengthPercentRef(pSvg, iBasis);
	}
	return fValue;
}

static void __xgeSvgLinearGradientResolvedValues(
	xge_svg pSvg, const xge_svg_linear_gradient_t* pGradient,
	float* pX1, float* pY1, float* pX2, float* pY2
)
{
	*pX1 = __xgeSvgGradientResolvedLength(pSvg, pGradient->fX1, pGradient->iUnits, XGE_SVG_LENGTH_BASIS_X, (pGradient->iPercentFlags & XGE_SVG_GRADIENT_HAS_X1) != 0);
	*pY1 = __xgeSvgGradientResolvedLength(pSvg, pGradient->fY1, pGradient->iUnits, XGE_SVG_LENGTH_BASIS_Y, (pGradient->iPercentFlags & XGE_SVG_GRADIENT_HAS_Y1) != 0);
	*pX2 = __xgeSvgGradientResolvedLength(pSvg, pGradient->fX2, pGradient->iUnits, XGE_SVG_LENGTH_BASIS_X, (pGradient->iPercentFlags & XGE_SVG_GRADIENT_HAS_X2) != 0);
	*pY2 = __xgeSvgGradientResolvedLength(pSvg, pGradient->fY2, pGradient->iUnits, XGE_SVG_LENGTH_BASIS_Y, (pGradient->iPercentFlags & XGE_SVG_GRADIENT_HAS_Y2) != 0);
}

static void __xgeSvgRadialGradientResolvedValues(
	xge_svg pSvg, const xge_svg_radial_gradient_t* pGradient,
	float* pCX, float* pCY, float* pR, float* pFX, float* pFY, float* pFR
)
{
	*pCX = __xgeSvgGradientResolvedLength(pSvg, pGradient->fCX, pGradient->iUnits, XGE_SVG_LENGTH_BASIS_X, (pGradient->iPercentFlags & XGE_SVG_GRADIENT_HAS_CX) != 0);
	*pCY = __xgeSvgGradientResolvedLength(pSvg, pGradient->fCY, pGradient->iUnits, XGE_SVG_LENGTH_BASIS_Y, (pGradient->iPercentFlags & XGE_SVG_GRADIENT_HAS_CY) != 0);
	*pR = __xgeSvgGradientResolvedLength(pSvg, pGradient->fR, pGradient->iUnits, XGE_SVG_LENGTH_BASIS_OTHER, (pGradient->iPercentFlags & XGE_SVG_GRADIENT_HAS_R) != 0);
	*pFX = __xgeSvgGradientResolvedLength(pSvg, pGradient->fFX, pGradient->iUnits, XGE_SVG_LENGTH_BASIS_X, (pGradient->iPercentFlags & XGE_SVG_GRADIENT_HAS_FX) != 0);
	*pFY = __xgeSvgGradientResolvedLength(pSvg, pGradient->fFY, pGradient->iUnits, XGE_SVG_LENGTH_BASIS_Y, (pGradient->iPercentFlags & XGE_SVG_GRADIENT_HAS_FY) != 0);
	*pFR = __xgeSvgGradientResolvedLength(pSvg, pGradient->fFR, pGradient->iUnits, XGE_SVG_LENGTH_BASIS_OTHER, (pGradient->iPercentFlags & XGE_SVG_GRADIENT_HAS_FR) != 0);
}

static float __xgeSvgPaintOpacityThorvg(float fOpacity, int bNonFinite, int bGradient)
{
	if ( bNonFinite ) {
		return bGradient ? (128.0f / 255.0f) : 0.0f;
	}
	return fOpacity;
}

static void __xgeSvgApplyShapeStyle(xge_svg pSvg, xge_shape_ex pShape, const xge_svg_style_t* pStyle)
{
	float fFillOpacity;
	float fStrokeOpacity;
	int bFillPattern;
	int bStrokePattern;
	int bFillGradient;
	int bStrokeGradient;

	if ( (pShape == NULL) || (pStyle == NULL) ) {
		return;
	}
	bFillPattern = (pStyle->sFillGradientId[0] != '\0') && (__xgeSvgPatternFind(pSvg, pStyle->sFillGradientId) >= 0);
	bStrokePattern = (pStyle->sStrokeGradientId[0] != '\0') && (__xgeSvgPatternFind(pSvg, pStyle->sStrokeGradientId) >= 0);
	if ( (pStyle->sFillGradientId[0] != '\0') || (pStyle->sStrokeGradientId[0] != '\0') ) {
		__xgeSvgResolveGradients(pSvg);
	}
	bFillGradient = !bFillPattern && (pStyle->sFillGradientId[0] != '\0') &&
		((__xgeSvgLinearGradientFind(pSvg, pStyle->sFillGradientId) >= 0) ||
		 (__xgeSvgRadialGradientFind(pSvg, pStyle->sFillGradientId) >= 0));
	bStrokeGradient = !bStrokePattern && (pStyle->sStrokeGradientId[0] != '\0') &&
		((__xgeSvgLinearGradientFind(pSvg, pStyle->sStrokeGradientId) >= 0) ||
		 (__xgeSvgRadialGradientFind(pSvg, pStyle->sStrokeGradientId) >= 0));
	fFillOpacity = pStyle->fOpacity * __xgeSvgPaintOpacityThorvg(
		pStyle->fFillOpacity, pStyle->bFillOpacityNonFinite, bFillGradient
	);
	fStrokeOpacity = pStyle->fOpacity * __xgeSvgPaintOpacityThorvg(
		pStyle->fStrokeOpacity, pStyle->bStrokeOpacityNonFinite, bStrokeGradient
	);
	xgeShapeExFillColor(pShape, bFillPattern ? XGE_COLOR_RGBA(0, 0, 0, 0) : __xgeSvgColorAlpha(pStyle->iFillColor, fFillOpacity));
	if ( (pStyle->sFillGradientId[0] != '\0') && !bFillPattern ) {
		int iGradient = __xgeSvgLinearGradientFind(pSvg, pStyle->sFillGradientId);
		__xgeSvgResolveGradients(pSvg);
		iGradient = __xgeSvgLinearGradientFind(pSvg, pStyle->sFillGradientId);
		if ( (iGradient >= 0) && (pSvg->pLinearGradients[iGradient].iStopCount > 0) ) {
			xge_svg_linear_gradient_t* pGradient = &pSvg->pLinearGradients[iGradient];
			xge_shape_ex_color_stop_t arrStops[64];
			xge_shape_ex_color_stop_t* pStops;
			float fX1, fY1, fX2, fY2;
			int i;

			pStops = (pGradient->iStopCount <= (int)(sizeof(arrStops) / sizeof(arrStops[0]))) ? arrStops :
				(xge_shape_ex_color_stop_t*)xrtMalloc((size_t)pGradient->iStopCount * sizeof(*pStops));
			if ( pStops != NULL ) {
				for ( i = 0; i < pGradient->iStopCount; i++ ) {
					pStops[i] = pGradient->pStops[i];
					pStops[i].iColor = __xgeSvgColorAlpha(pStops[i].iColor, fFillOpacity);
				}
				__xgeSvgLinearGradientResolvedValues(pSvg, pGradient, &fX1, &fY1, &fX2, &fY2);
				if ( xgeShapeExFillLinearGradient(pShape, fX1, fY1, fX2, fY2, pGradient->iUnits, pStops, pGradient->iStopCount) == XGE_OK ) {
					xgeShapeExFillGradientSpread(pShape, pGradient->iSpread);
					xgeShapeExFillGradientTransformSet(pShape, &pGradient->tTransform);
				}
				if ( pStops != arrStops ) {
					xrtFree(pStops);
				}
			}
		} else {
			iGradient = __xgeSvgRadialGradientFind(pSvg, pStyle->sFillGradientId);
			if ( (iGradient >= 0) && (pSvg->pRadialGradients[iGradient].iStopCount > 0) ) {
				xge_svg_radial_gradient_t* pGradient = &pSvg->pRadialGradients[iGradient];
				xge_shape_ex_color_stop_t arrStops[64];
				xge_shape_ex_color_stop_t* pStops;
				float fCX, fCY, fR, fFX, fFY, fFR;
				int i;

				pStops = (pGradient->iStopCount <= (int)(sizeof(arrStops) / sizeof(arrStops[0]))) ? arrStops :
					(xge_shape_ex_color_stop_t*)xrtMalloc((size_t)pGradient->iStopCount * sizeof(*pStops));
				if ( pStops != NULL ) {
					for ( i = 0; i < pGradient->iStopCount; i++ ) {
						pStops[i] = pGradient->pStops[i];
						pStops[i].iColor = __xgeSvgColorAlpha(pStops[i].iColor, fFillOpacity);
					}
					__xgeSvgRadialGradientResolvedValues(pSvg, pGradient, &fCX, &fCY, &fR, &fFX, &fFY, &fFR);
					if ( xgeShapeExFillRadialGradientEx(pShape, fCX, fCY, fR, fFX, fFY, fFR, pGradient->iUnits, pStops, pGradient->iStopCount) == XGE_OK ) {
						xgeShapeExFillGradientSpread(pShape, pGradient->iSpread);
						xgeShapeExFillGradientTransformSet(pShape, &pGradient->tTransform);
					}
					if ( pStops != arrStops ) {
						xrtFree(pStops);
					}
				}
			}
		}
	}
	xgeShapeExStrokeColor(pShape, bStrokePattern ? XGE_COLOR_RGBA(0, 0, 0, 0) : __xgeSvgColorAlpha(pStyle->iStrokeColor, fStrokeOpacity));
	if ( (pStyle->sStrokeGradientId[0] != '\0') && !bStrokePattern ) {
		int iGradient;

		__xgeSvgResolveGradients(pSvg);
		iGradient = __xgeSvgLinearGradientFind(pSvg, pStyle->sStrokeGradientId);
		if ( (iGradient >= 0) && (pSvg->pLinearGradients[iGradient].iStopCount > 0) ) {
			xge_svg_linear_gradient_t* pGradient = &pSvg->pLinearGradients[iGradient];
			xge_shape_ex_color_stop_t arrStops[64];
			xge_shape_ex_color_stop_t* pStops;
			float fX1, fY1, fX2, fY2;
			int i;

			pStops = (pGradient->iStopCount <= (int)(sizeof(arrStops) / sizeof(arrStops[0]))) ? arrStops :
				(xge_shape_ex_color_stop_t*)xrtMalloc((size_t)pGradient->iStopCount * sizeof(*pStops));
			if ( pStops != NULL ) {
				for ( i = 0; i < pGradient->iStopCount; i++ ) {
					pStops[i] = pGradient->pStops[i];
					pStops[i].iColor = __xgeSvgColorAlpha(pStops[i].iColor, fStrokeOpacity);
				}
				__xgeSvgLinearGradientResolvedValues(pSvg, pGradient, &fX1, &fY1, &fX2, &fY2);
				if ( xgeShapeExStrokeLinearGradient(pShape, fX1, fY1, fX2, fY2, pGradient->iUnits, pStops, pGradient->iStopCount) == XGE_OK ) {
					xgeShapeExStrokeGradientSpread(pShape, pGradient->iSpread);
					xgeShapeExStrokeGradientTransformSet(pShape, &pGradient->tTransform);
				}
				if ( pStops != arrStops ) {
					xrtFree(pStops);
				}
			}
		} else {
			iGradient = __xgeSvgRadialGradientFind(pSvg, pStyle->sStrokeGradientId);
			if ( (iGradient >= 0) && (pSvg->pRadialGradients[iGradient].iStopCount > 0) ) {
				xge_svg_radial_gradient_t* pGradient = &pSvg->pRadialGradients[iGradient];
				xge_shape_ex_color_stop_t arrStops[64];
				xge_shape_ex_color_stop_t* pStops;
				float fCX, fCY, fR, fFX, fFY, fFR;
				int i;

				pStops = (pGradient->iStopCount <= (int)(sizeof(arrStops) / sizeof(arrStops[0]))) ? arrStops :
					(xge_shape_ex_color_stop_t*)xrtMalloc((size_t)pGradient->iStopCount * sizeof(*pStops));
				if ( pStops != NULL ) {
					for ( i = 0; i < pGradient->iStopCount; i++ ) {
						pStops[i] = pGradient->pStops[i];
						pStops[i].iColor = __xgeSvgColorAlpha(pStops[i].iColor, fStrokeOpacity);
					}
					__xgeSvgRadialGradientResolvedValues(pSvg, pGradient, &fCX, &fCY, &fR, &fFX, &fFY, &fFR);
					if ( xgeShapeExStrokeRadialGradientEx(pShape, fCX, fCY, fR, fFX, fFY, fFR, pGradient->iUnits, pStops, pGradient->iStopCount) == XGE_OK ) {
						xgeShapeExStrokeGradientSpread(pShape, pGradient->iSpread);
						xgeShapeExStrokeGradientTransformSet(pShape, &pGradient->tTransform);
					}
					if ( pStops != arrStops ) {
						xrtFree(pStops);
					}
				}
			}
		}
	}
	xgeShapeExStrokeWidth(pShape, pStyle->fStrokeWidth);
	xgeShapeExStrokeMiterLimit(pShape, pStyle->fMiterLimit);
	xgeShapeExStrokeNonScaling(pShape, pStyle->bStrokeNonScaling);
	xgeShapeExStrokeDash(pShape, pStyle->fDashPattern, pStyle->iDashCount, pStyle->fDashOffset);
	xgeShapeExFillRule(pShape, pStyle->iFillRule);
	xgeShapeExStrokeCap(pShape, pStyle->iLineCap);
	xgeShapeExStrokeJoin(pShape, pStyle->iLineJoin);
	xgeShapeExPaintOrder(pShape, pStyle->bStrokeFirst);
	xgeShapeExVisible(pShape, pStyle->bVisible && pStyle->bVisibility);
	if ( pStyle->bBlendSet ) {
		xgeShapeExBlend(pShape, pStyle->iBlend);
	} else {
		xgeShapeExBlendClear(pShape);
	}
	xgeShapeExTransformSet(pShape, &pStyle->tTransform);
}

static int __xgeSvgApplyFillGradientPaintToShape(xge_svg pSvg, xge_shape_ex pShape, const xge_svg_style_t* pStyle, float fPaintOpacity)
{
	int iGradient;

	if ( !__xgeSvgValid(pSvg) || (pShape == NULL) || (pStyle == NULL) || (pStyle->sFillGradientId[0] == '\0') ) {
		return 0;
	}
	__xgeSvgResolveGradients(pSvg);
	iGradient = __xgeSvgLinearGradientFind(pSvg, pStyle->sFillGradientId);
	if ( (iGradient >= 0) && (pSvg->pLinearGradients[iGradient].iStopCount > 0) ) {
		xge_svg_linear_gradient_t* pGradient = &pSvg->pLinearGradients[iGradient];
		xge_shape_ex_color_stop_t arrStops[64];
		xge_shape_ex_color_stop_t* pStops;
		float fX1, fY1, fX2, fY2;
		int i;
		int bApplied;

		pStops = (pGradient->iStopCount <= (int)(sizeof(arrStops) / sizeof(arrStops[0]))) ? arrStops :
			(xge_shape_ex_color_stop_t*)xrtMalloc((size_t)pGradient->iStopCount * sizeof(*pStops));
		if ( pStops == NULL ) {
			return 0;
		}
		for ( i = 0; i < pGradient->iStopCount; i++ ) {
			pStops[i] = pGradient->pStops[i];
			pStops[i].iColor = __xgeSvgColorAlpha(pStops[i].iColor, fPaintOpacity);
		}
		__xgeSvgLinearGradientResolvedValues(pSvg, pGradient, &fX1, &fY1, &fX2, &fY2);
		bApplied = xgeShapeExFillLinearGradient(pShape, fX1, fY1, fX2, fY2, pGradient->iUnits, pStops, pGradient->iStopCount) == XGE_OK;
		if ( bApplied ) {
			xgeShapeExFillGradientSpread(pShape, pGradient->iSpread);
			xgeShapeExFillGradientTransformSet(pShape, &pGradient->tTransform);
		}
		if ( pStops != arrStops ) {
			xrtFree(pStops);
		}
		return bApplied;
	}
	iGradient = __xgeSvgRadialGradientFind(pSvg, pStyle->sFillGradientId);
	if ( (iGradient >= 0) && (pSvg->pRadialGradients[iGradient].iStopCount > 0) ) {
		xge_svg_radial_gradient_t* pGradient = &pSvg->pRadialGradients[iGradient];
		xge_shape_ex_color_stop_t arrStops[64];
		xge_shape_ex_color_stop_t* pStops;
		float fCX, fCY, fR, fFX, fFY, fFR;
		int i;
		int bApplied;

		pStops = (pGradient->iStopCount <= (int)(sizeof(arrStops) / sizeof(arrStops[0]))) ? arrStops :
			(xge_shape_ex_color_stop_t*)xrtMalloc((size_t)pGradient->iStopCount * sizeof(*pStops));
		if ( pStops == NULL ) {
			return 0;
		}
		for ( i = 0; i < pGradient->iStopCount; i++ ) {
			pStops[i] = pGradient->pStops[i];
			pStops[i].iColor = __xgeSvgColorAlpha(pStops[i].iColor, fPaintOpacity);
		}
		__xgeSvgRadialGradientResolvedValues(pSvg, pGradient, &fCX, &fCY, &fR, &fFX, &fFY, &fFR);
		bApplied = xgeShapeExFillRadialGradientEx(pShape, fCX, fCY, fR, fFX, fFY, fFR, pGradient->iUnits, pStops, pGradient->iStopCount) == XGE_OK;
		if ( bApplied ) {
			xgeShapeExFillGradientSpread(pShape, pGradient->iSpread);
			xgeShapeExFillGradientTransformSet(pShape, &pGradient->tTransform);
		}
		if ( pStops != arrStops ) {
			xrtFree(pStops);
		}
		return bApplied;
	}
	return 0;
}

static int __xgeSvgApplyStrokeGradientPaintToShape(xge_svg pSvg, xge_shape_ex pShape, const xge_svg_style_t* pStyle, float fPaintOpacity)
{
	int iGradient;

	if ( !__xgeSvgValid(pSvg) || (pShape == NULL) || (pStyle == NULL) || (pStyle->sStrokeGradientId[0] == '\0') ) {
		return 0;
	}
	__xgeSvgResolveGradients(pSvg);
	iGradient = __xgeSvgLinearGradientFind(pSvg, pStyle->sStrokeGradientId);
	if ( (iGradient >= 0) && (pSvg->pLinearGradients[iGradient].iStopCount > 0) ) {
		xge_svg_linear_gradient_t* pGradient = &pSvg->pLinearGradients[iGradient];
		xge_shape_ex_color_stop_t arrStops[64];
		xge_shape_ex_color_stop_t* pStops;
		float fX1, fY1, fX2, fY2;
		int i;
		int bApplied;

		pStops = (pGradient->iStopCount <= (int)(sizeof(arrStops) / sizeof(arrStops[0]))) ? arrStops :
			(xge_shape_ex_color_stop_t*)xrtMalloc((size_t)pGradient->iStopCount * sizeof(*pStops));
		if ( pStops == NULL ) {
			return 0;
		}
		for ( i = 0; i < pGradient->iStopCount; i++ ) {
			pStops[i] = pGradient->pStops[i];
			pStops[i].iColor = __xgeSvgColorAlpha(pStops[i].iColor, fPaintOpacity);
		}
		__xgeSvgLinearGradientResolvedValues(pSvg, pGradient, &fX1, &fY1, &fX2, &fY2);
		bApplied = xgeShapeExStrokeLinearGradient(pShape, fX1, fY1, fX2, fY2, pGradient->iUnits, pStops, pGradient->iStopCount) == XGE_OK;
		if ( bApplied ) {
			xgeShapeExStrokeGradientSpread(pShape, pGradient->iSpread);
			xgeShapeExStrokeGradientTransformSet(pShape, &pGradient->tTransform);
		}
		if ( pStops != arrStops ) {
			xrtFree(pStops);
		}
		return bApplied;
	}
	iGradient = __xgeSvgRadialGradientFind(pSvg, pStyle->sStrokeGradientId);
	if ( (iGradient >= 0) && (pSvg->pRadialGradients[iGradient].iStopCount > 0) ) {
		xge_svg_radial_gradient_t* pGradient = &pSvg->pRadialGradients[iGradient];
		xge_shape_ex_color_stop_t arrStops[64];
		xge_shape_ex_color_stop_t* pStops;
		float fCX, fCY, fR, fFX, fFY, fFR;
		int i;
		int bApplied;

		pStops = (pGradient->iStopCount <= (int)(sizeof(arrStops) / sizeof(arrStops[0]))) ? arrStops :
			(xge_shape_ex_color_stop_t*)xrtMalloc((size_t)pGradient->iStopCount * sizeof(*pStops));
		if ( pStops == NULL ) {
			return 0;
		}
		for ( i = 0; i < pGradient->iStopCount; i++ ) {
			pStops[i] = pGradient->pStops[i];
			pStops[i].iColor = __xgeSvgColorAlpha(pStops[i].iColor, fPaintOpacity);
		}
		__xgeSvgRadialGradientResolvedValues(pSvg, pGradient, &fCX, &fCY, &fR, &fFX, &fFY, &fFR);
		bApplied = xgeShapeExStrokeRadialGradientEx(pShape, fCX, fCY, fR, fFX, fFY, fFR, pGradient->iUnits, pStops, pGradient->iStopCount) == XGE_OK;
		if ( bApplied ) {
			xgeShapeExStrokeGradientSpread(pShape, pGradient->iSpread);
			xgeShapeExStrokeGradientTransformSet(pShape, &pGradient->tTransform);
		}
		if ( pStops != arrStops ) {
			xrtFree(pStops);
		}
		return bApplied;
	}
	return 0;
}

static int __xgeSvgPendingShapeResourcesResolve(xge_svg pSvg)
{
	int i;
	int iRet;

	if ( !__xgeSvgValid(pSvg) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < pSvg->iPendingShapeResourceCount; i++ ) {
		xge_svg_pending_shape_resource_t* pResource = &pSvg->pPendingShapeResources[i];
		xge_shape_ex pShape = pResource->pShape;

		if ( pShape == NULL ) {
			continue;
		}
		if ( (pResource->iFlags & XGE_SVG_PENDING_SHAPE_FILL_GRADIENT) != 0 ) {
			(void)__xgeSvgApplyFillGradientPaintToShape(
				pSvg, pShape, &pResource->tStyle,
				pResource->tStyle.fOpacity * pResource->tStyle.fFillOpacity
			);
		}
		if ( (pResource->iFlags & XGE_SVG_PENDING_SHAPE_STROKE_GRADIENT) != 0 ) {
			(void)__xgeSvgApplyStrokeGradientPaintToShape(
				pSvg, pShape, &pResource->tStyle,
				pResource->tStyle.fOpacity * pResource->tStyle.fStrokeOpacity
			);
		}
		if ( (pResource->iFlags & XGE_SVG_PENDING_SHAPE_CLIP) != 0 ) {
			xge_svg_clip_path_t* pClip = __xgeSvgClipPathGetById(pSvg, pResource->tStyle.sClipId);

			if ( pClip != NULL ) {
				pShape->bAxisAlignedFillAntialias = pResource->tStyle.bClipPathLocal;
				iRet = __xgeSvgApplyClipPath(pSvg, pShape, pResource->tStyle.sClipId);
				if ( iRet != XGE_OK ) {
					return iRet;
				}
			} else if ( __xgeSvgClipPathFind(pSvg, pResource->tStyle.sClipId) >= 0 ) {
				iRet = xgeShapeExVisible(pShape, 0);
				if ( iRet != XGE_OK ) {
					return iRet;
				}
			}
		}
	}
	__xgeSvgPendingShapeResourcesClear(pSvg);
	return XGE_OK;
}

static int __xgeSvgParsePointList(const char* sPoints, xge_vec2_t** ppPoints, int* pCount)
{
	const char* p;
	const char* pEnd;
	xge_vec2_t* pPoints;
	double fValue;
	int iCount;
	int iCapacity;
	float fX;
	float fY;

	if ( (sPoints == NULL) || (ppPoints == NULL) || (pCount == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppPoints = NULL;
	*pCount = 0;
	p = sPoints;
	pPoints = NULL;
	iCount = 0;
	iCapacity = 0;
	while ( *p != '\0' ) {
		while ( (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') || (*p == ',') ) p++;
		if ( *p == '\0' ) break;
		if ( !__xgeSvgParseNumberAt(p, &pEnd, &fValue) ) {
			break;
		}
		if ( !__xgeSvgDoubleToFloat(fValue, &fX) ) {
			break;
		}
		p = pEnd;
		while ( (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') || (*p == ',') ) p++;
		if ( !__xgeSvgParseNumberAt(p, &pEnd, &fValue) ) {
			break;
		}
		if ( !__xgeSvgDoubleToFloat(fValue, &fY) ) {
			break;
		}
		p = pEnd;
		if ( iCount >= iCapacity ) {
			int iNewCapacity = iCapacity > 0 ? iCapacity * 2 : 8;
			xge_vec2_t* pNewPoints = (xge_vec2_t*)xrtRealloc(pPoints, (size_t)iNewCapacity * sizeof(*pNewPoints));
			if ( pNewPoints == NULL ) {
				xrtFree(pPoints);
				return XGE_ERROR_OUT_OF_MEMORY;
			}
			pPoints = pNewPoints;
			iCapacity = iNewCapacity;
		}
		pPoints[iCount].fX = fX;
		pPoints[iCount].fY = fY;
		iCount++;
	}
	*ppPoints = pPoints;
	*pCount = iCount;
	return XGE_OK;
}

static int __xgeSvgAppendPointList(xge_shape_ex pShape, const xge_vec2_t* pPoints, int iCount, int bClose)
{
	if ( (pShape == NULL) || (pPoints == NULL) || (iCount <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	return bClose ? xgeShapeExAppendPolygon(pShape, pPoints, iCount) :
	                xgeShapeExAppendPolyline(pShape, pPoints, iCount);
}

static int __xgeSvgCreatePointListShape(const char* sPoints, int bClose, xge_shape_ex* ppShape)
{
	xge_shape_ex pShape;
	xge_vec2_t* pPoints;
	int iCount;
	int iRet;

	if ( (sPoints == NULL) || (ppShape == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppShape = NULL;
	pPoints = NULL;
	iCount = 0;
	iRet = __xgeSvgParsePointList(sPoints, &pPoints, &iCount);
	if ( iRet != XGE_OK ) {
		xrtFree(pPoints);
		return iRet;
	}
	if ( iCount <= 0 ) {
		xrtFree(pPoints);
		return XGE_OK;
	}
	iRet = xgeShapeExCreate(&pShape);
	if ( iRet != XGE_OK ) {
		xrtFree(pPoints);
		return iRet;
	}
	iRet = __xgeSvgAppendPointList(pShape, pPoints, iCount, bClose);
	xrtFree(pPoints);
	if ( iRet != XGE_OK ) {
		xgeShapeExDestroy(pShape);
		return iRet;
	}
	*ppShape = pShape;
	return iRet;
}

static int __xgeSvgAddShapeStyled(xge_svg pSvg, xge_shape_ex pShape, int iDefIndex, const xge_svg_style_t* pStyle)
{
	xge_svg_draw_item_t* pItem;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pShape == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iDefIndex >= 0 ) {
		if ( iDefIndex >= pSvg->iDefCount ) {
			xgeShapeExDestroy(pShape);
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		iRet = __xgeSvgDefAddShape(&pSvg->pDefs[iDefIndex], pShape, pStyle);
	} else {
		iRet = __xgeSvgDrawItemReserve(pSvg);
		if ( iRet == XGE_OK ) {
			pItem = &pSvg->pItems[pSvg->iItemCount];
			memset(pItem, 0, sizeof(*pItem));
			pItem->iType = XGE_SVG_DRAW_ITEM_SCENE;
			pItem->iBlend = pStyle != NULL ? pStyle->iBlend : XGE_BLEND_ALPHA;
			pItem->bBlendSet = pStyle != NULL ? pStyle->bBlendSet : 0;
			iRet = xgeShapeExSceneCreate(&pItem->u.pScene);
			if ( iRet == XGE_OK ) {
				iRet = xgeShapeExSceneAdd(pItem->u.pScene, pShape);
			}
			if ( iRet == XGE_OK ) {
				uint32_t iId = 0;

				xgeShapeExIdGet(pShape, &iId);
				pItem->iId = iId;
				if ( pStyle != NULL ) {
					pItem->fPaintOpacity = pStyle->fOpacity;
					pItem->bPaintOpacity = 1;
				}
				pItem->pIdentityShape = pShape;
				pSvg->iItemCount++;
			} else {
				__xgeSvgDrawItemReset(pItem);
			}
		}
	}
	xgeShapeExDestroy(pShape);
	return iRet;
}

static int __xgeSvgAddShape(xge_svg pSvg, xge_shape_ex pShape, int iDefIndex)
{
	return __xgeSvgAddShapeStyled(pSvg, pShape, iDefIndex, NULL);
}

static xge_rect_t __xgeSvgRectIntersect(xge_rect_t a, xge_rect_t b)
{
	float fLeft = fmaxf(a.fX, b.fX);
	float fTop = fmaxf(a.fY, b.fY);
	float fRight = fminf(a.fX + a.fW, b.fX + b.fW);
	float fBottom = fminf(a.fY + a.fH, b.fY + b.fH);
	xge_rect_t tOut;

	tOut.fX = fLeft;
	tOut.fY = fTop;
	tOut.fW = fRight - fLeft;
	tOut.fH = fBottom - fTop;
	if ( tOut.fW < 0.0f ) tOut.fW = 0.0f;
	if ( tOut.fH < 0.0f ) tOut.fH = 0.0f;
	return tOut;
}

static xge_rect_t __xgeSvgPixelCenterClipRect(xge_rect_t tRect)
{
	float fLeft = ceilf(tRect.fX - 0.5f);
	float fTop = ceilf(tRect.fY - 0.5f);
	float fRight = ceilf(tRect.fX + tRect.fW - 0.5f);
	float fBottom = ceilf(tRect.fY + tRect.fH - 0.5f);

	if ( fRight < fLeft ) fRight = fLeft;
	if ( fBottom < fTop ) fBottom = fTop;
	return (xge_rect_t){fLeft, fTop, fRight - fLeft, fBottom - fTop};
}

static xge_rect_t __xgeSvgObjectRectToBounds(xge_rect_t tRect, xge_rect_t tBounds)
{
	tRect.fX = tBounds.fX + tRect.fX * tBounds.fW;
	tRect.fY = tBounds.fY + tRect.fY * tBounds.fH;
	tRect.fW = tRect.fW * tBounds.fW;
	tRect.fH = tRect.fH * tBounds.fH;
	return tRect;
}

static xge_rect_t __xgeSvgMaskRegionResolveBounds(const xge_svg_mask_t* pMask, xge_rect_t tBounds)
{
	xge_rect_t tRegion;

	if ( pMask == NULL ) {
		return tBounds;
	}
	tRegion = pMask->tRegion;
	if ( pMask->iUnits == XGE_SVG_MASK_OBJECT_BOUNDING_BOX ) {
		tRegion = __xgeSvgObjectRectToBounds(tRegion, tBounds);
	}
	return tRegion;
}

static int __xgeSvgShapeClipRectIntersect(xge_shape_ex pShape, xge_rect_t tClip)
{
	if ( pShape == NULL ) {
		return 0;
	}
	if ( pShape->bClipRect ) {
		tClip = __xgeSvgRectIntersect(pShape->tClipRect, tClip);
	}
	if ( (tClip.fW <= 0.0f) || (tClip.fH <= 0.0f) ) {
		return 0;
	}
	xgeShapeExClipRectSet(pShape, tClip);
	return 1;
}

static int __xgeSvgTextClipRectIntersect(xge_svg_text_item_t* pText, xge_rect_t tClip)
{
	if ( pText == NULL ) {
		return 0;
	}
	if ( pText->bClipRect ) {
		tClip = __xgeSvgRectIntersect(pText->tClipRect, tClip);
	}
	if ( (tClip.fW <= 0.0f) || (tClip.fH <= 0.0f) ) {
		return 0;
	}
	pText->bClipRect = 1;
	pText->tClipRect = tClip;
	return 1;
}

static int __xgeSvgImageClipRectIntersect(xge_svg_image_item_t* pImage, xge_rect_t tClip)
{
	if ( pImage == NULL ) {
		return 0;
	}
	if ( pImage->bClipRect ) {
		tClip = __xgeSvgRectIntersect(pImage->tClipRect, tClip);
	}
	if ( (tClip.fW <= 0.0f) || (tClip.fH <= 0.0f) ) {
		return 0;
	}
	pImage->bClipRect = 1;
	pImage->tClipRect = tClip;
	return 1;
}

static int __xgeSvgRasterClipRectIntersect(xge_svg_raster_item_t* pImage, xge_rect_t tClip)
{
	if ( pImage == NULL ) {
		return 0;
	}
	if ( pImage->bClipRect ) {
		tClip = __xgeSvgRectIntersect(pImage->tClipRect, tClip);
	}
	if ( (tClip.fW <= 0.0f) || (tClip.fH <= 0.0f) ) {
		return 0;
	}
	pImage->bClipRect = 1;
	pImage->tClipRect = tClip;
	return 1;
}

static int __xgeSvgShapeClipRectMapIntersect(xge_shape_ex pShape, xge_rect_t tClip)
{
	xge_rect_t tLocalClip;

	if ( pShape == NULL ) {
		return 0;
	}
	if ( !__xgeSvgClipRectMapToItem(pShape->tTransform, tClip, pShape->bClipRect, pShape->tClipRect, &tLocalClip) ) {
		return 0;
	}
	xgeShapeExClipRectSet(pShape, tLocalClip);
	return 1;
}

static int __xgeSvgTextClipRectMapIntersect(xge_svg_text_item_t* pText, xge_rect_t tClip)
{
	xge_rect_t tLocalClip;

	if ( pText == NULL ) {
		return 0;
	}
	if ( !__xgeSvgClipRectMapToItem(pText->tStyle.tTransform, tClip, pText->bClipRect, pText->tClipRect, &tLocalClip) ) {
		return 0;
	}
	pText->bClipRect = 1;
	pText->tClipRect = tLocalClip;
	return 1;
}

static int __xgeSvgImageClipRectMapIntersect(xge_svg_image_item_t* pImage, xge_rect_t tClip)
{
	xge_rect_t tLocalClip;

	if ( pImage == NULL ) {
		return 0;
	}
	if ( !__xgeSvgClipRectMapToItem(pImage->tStyle.tTransform, tClip, pImage->bClipRect, pImage->tClipRect, &tLocalClip) ) {
		return 0;
	}
	pImage->bClipRect = 1;
	pImage->tClipRect = tLocalClip;
	return 1;
}

static int __xgeSvgRasterClipRectMapIntersect(xge_svg_raster_item_t* pImage, xge_rect_t tClip)
{
	xge_rect_t tLocalClip;

	if ( pImage == NULL ) {
		return 0;
	}
	if ( !__xgeSvgClipRectMapToItem(pImage->tStyle.tTransform, tClip, pImage->bClipRect, pImage->tClipRect, &tLocalClip) ) {
		return 0;
	}
	pImage->bClipRect = 1;
	pImage->tClipRect = tLocalClip;
	return 1;
}

static int __xgeSvgApplyMaskShapeToShape(const xge_svg_mask_t* pMask, const xge_svg_mask_shape_t* pMaskShape, xge_shape_ex pTarget);
static int __xgeSvgMaskShapeResolveForBounds(const xge_svg_mask_t* pMask, const xge_svg_mask_shape_t* pMaskShape, xge_rect_t tBounds, xge_shape_ex* ppShape);

static int __xgeSvgMaskMethod(const xge_svg_mask_t* pMask)
{
	return (pMask != NULL) && (pMask->iType == XGE_SVG_MASK_TYPE_ALPHA) ?
		XGE_SHAPE_EX_MASK_ALPHA : XGE_SHAPE_EX_MASK_LUMA;
}

static int __xgeSvgItemMaskMethod(int iMethod)
{
	return iMethod == XGE_SHAPE_EX_MASK_LUMA ? XGE_SHAPE_EX_MASK_LUMA : XGE_SHAPE_EX_MASK_ALPHA;
}

static int __xgeSvgMaskSceneResolve(const xge_svg_mask_t* pMask, xge_rect_t tBounds, xge_shape_ex_scene* ppScene)
{
	xge_shape_ex_scene pScene;
	xge_rect_t tRegion;
	int iRet;
	int i;

	if ( (pMask == NULL) || (ppScene == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppScene = NULL;
	iRet = xgeShapeExSceneCreate(&pScene);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	for ( i = 0; i < pMask->iShapeCount; i++ ) {
		const xge_svg_mask_shape_t* pMaskShape = &pMask->pShapes[i];
		xge_shape_ex pShape = NULL;

		if ( pMaskShape->pShape == NULL ) {
			continue;
		}
		iRet = __xgeSvgMaskShapeResolveForBounds(pMask, pMaskShape, tBounds, &pShape);
		if ( iRet != XGE_OK ) {
			xgeShapeExSceneDestroy(pScene);
			return iRet;
		}
		if ( pShape == NULL ) {
			continue;
		}
		iRet = xgeShapeExSceneAdd(pScene, pShape);
		xgeShapeExDestroy(pShape);
		if ( iRet != XGE_OK ) {
			xgeShapeExSceneDestroy(pScene);
			return iRet;
		}
	}
	tRegion = __xgeSvgMaskRegionResolveBounds(pMask, tBounds);
	if ( (tRegion.fW <= 0.0f) || (tRegion.fH <= 0.0f) ) {
		xgeShapeExSceneDestroy(pScene);
		return XGE_OK;
	}
	iRet = xgeShapeExSceneClipRectSet(pScene, tRegion);
	if ( iRet != XGE_OK ) {
		xgeShapeExSceneDestroy(pScene);
		return iRet;
	}
	*ppScene = pScene;
	return XGE_OK;
}

static int __xgeSvgAddStyledShapeMasked(xge_svg pSvg, xge_shape_ex pShape, const xge_svg_style_t* pStyle, int iDefIndex)
{
	xge_svg_mask_t* pMask;
	xge_shape_ex_scene pMaskScene;
	xge_rect_t tBounds;
	int iMask;
	int iMaskCount;
	int iRet;
	int iFirstItem;
	int bLogicalComposition;

	if ( !__xgeSvgValid(pSvg) || (pShape == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iFirstItem = (iDefIndex < 0) ? pSvg->iItemCount : -1;
	bLogicalComposition = (iDefIndex < 0) && (pStyle != NULL) &&
		(pStyle->bClipPathLocal || (pStyle->sMaskId[0] != '\0'));
	if ( (pStyle == NULL) || (pStyle->sMaskId[0] == '\0') ) {
		iRet = __xgeSvgAddShapeStyled(pSvg, pShape, iDefIndex, pStyle);
		if ( (iRet == XGE_OK) && bLogicalComposition &&
		     (pSvg->iItemCount > iFirstItem) ) {
			uint32_t iId = pSvg->pItems[iFirstItem].iId;

			pSvg->pItems[iFirstItem].iId = 0;
			iRet = __xgeSvgDrawItemsWrapGroup(
				pSvg, iFirstItem, 0, XGE_BLEND_ALPHA, 1.0f, NULL,
				iId, 1, 1.0f, NULL
			);
		}
		return iRet;
	}
	iMask = __xgeSvgMaskFind(pSvg, pStyle->sMaskId);
	if ( iMask < 0 ) {
		iRet = __xgeSvgAddShapeStyled(pSvg, pShape, iDefIndex, pStyle);
		if ( (iRet == XGE_OK) && (iDefIndex < 0) &&
		     (pSvg->iItemCount > iFirstItem) ) {
			uint32_t iId = pSvg->pItems[iFirstItem].iId;

			pSvg->pItems[iFirstItem].iId = 0;
			iRet = __xgeSvgDrawItemsWrapGroup(
				pSvg, iFirstItem, 0, XGE_BLEND_ALPHA, 1.0f,
				pStyle->sMaskId, iId, 1, 1.0f, NULL
			);
		}
		return iRet;
	}
	pMask = &pSvg->pMasks[iMask];
	if ( pMask->iShapeCount <= 0 ) {
		xgeShapeExDestroy(pShape);
		return XGE_OK;
	}
	memset(&tBounds, 0, sizeof(tBounds));
	if ( (pMask->iUnits == XGE_SVG_MASK_OBJECT_BOUNDING_BOX) || (pMask->iContentUnits == XGE_SVG_MASK_OBJECT_BOUNDING_BOX) ) {
		iRet = xgeShapeExGetBounds(pShape, 0.0f, &tBounds);
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pShape);
			return iRet;
		}
		if ( (tBounds.fW <= 0.0f) || (tBounds.fH <= 0.0f) ) {
			xgeShapeExDestroy(pShape);
			return XGE_OK;
		}
	}
	pMaskScene = NULL;
	iRet = __xgeSvgMaskSceneResolve(pMask, tBounds, &pMaskScene);
	if ( iRet != XGE_OK ) {
		xgeShapeExDestroy(pShape);
		return iRet;
	}
	if ( pMaskScene == NULL ) {
		xgeShapeExDestroy(pShape);
		return XGE_OK;
	}
	iMaskCount = 0;
	iRet = xgeShapeExSceneGetCount(pMaskScene, &iMaskCount);
	if ( (iRet == XGE_OK) && (iMaskCount <= 0) ) {
		xgeShapeExSceneDestroy(pMaskScene);
		xgeShapeExDestroy(pShape);
		return XGE_OK;
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeShapeExMaskSceneSet(pShape, pMaskScene, __xgeSvgMaskMethod(pMask));
	}
	xgeShapeExSceneDestroy(pMaskScene);
	if ( iRet != XGE_OK ) {
		xgeShapeExDestroy(pShape);
		return iRet;
	}
	iRet = __xgeSvgAddShapeStyled(pSvg, pShape, iDefIndex, pStyle);
	if ( (iRet == XGE_OK) && bLogicalComposition &&
	     (pSvg->iItemCount > iFirstItem) ) {
		uint32_t iId = pSvg->pItems[iFirstItem].iId;

		pSvg->pItems[iFirstItem].iId = 0;
		iRet = __xgeSvgDrawItemsWrapGroup(
			pSvg, iFirstItem, 0, XGE_BLEND_ALPHA, 1.0f, NULL,
			iId, 1, 1.0f, NULL
		);
	}
	return iRet;
}

static int __xgeSvgAddTextItemCopy(xge_svg pSvg, const xge_svg_text_item_t* pText, int iDefIndex)
{
	xge_svg_draw_item_t* pItem;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pText == NULL) || (pText->sText == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pText->sText[0] == '\0' ) {
		return XGE_OK;
	}
	if ( iDefIndex >= 0 ) {
		if ( iDefIndex >= pSvg->iDefCount ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		return __xgeSvgDefAddText(&pSvg->pDefs[iDefIndex], pText);
	}
	iRet = __xgeSvgDrawItemReserve(pSvg);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pItem = &pSvg->pItems[pSvg->iItemCount];
	memset(pItem, 0, sizeof(*pItem));
	pItem->iType = XGE_SVG_DRAW_ITEM_TEXT;
	pItem->u.tText = *pText;
	pItem->u.tText.sText = __xgeStrDup(pText->sText);
	if ( pItem->u.tText.sText == NULL ) {
		memset(pItem, 0, sizeof(*pItem));
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( pItem->u.tText.pMaskScene != NULL ) {
		iRet = xgeShapeExSceneAddRef(pItem->u.tText.pMaskScene);
		if ( iRet != XGE_OK ) {
			__xgeSvgDrawItemReset(pItem);
			return iRet;
		}
	}
	pSvg->iItemCount++;
	return XGE_OK;
}

static int __xgeSvgAddImageItemCopy(xge_svg pSvg, const xge_svg_image_item_t* pImage, int iDefIndex)
{
	xge_svg_draw_item_t* pItem;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pImage == NULL) || !__xgeSvgValid(pImage->pSvg) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (pImage->tRect.fW <= 0.0f) || (pImage->tRect.fH <= 0.0f) ) {
		return XGE_OK;
	}
	if ( iDefIndex >= 0 ) {
		if ( iDefIndex >= pSvg->iDefCount ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		return __xgeSvgDefAddImage(&pSvg->pDefs[iDefIndex], pImage);
	}
	iRet = __xgeSvgDrawItemReserve(pSvg);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pItem = &pSvg->pItems[pSvg->iItemCount];
	memset(pItem, 0, sizeof(*pItem));
	pItem->iType = XGE_SVG_DRAW_ITEM_SVG_IMAGE;
	pItem->u.tImage = *pImage;
	iRet = xgeSvgAddRef(pItem->u.tImage.pSvg);
	if ( iRet != XGE_OK ) {
		__xgeSvgDrawItemReset(pItem);
		return iRet;
	}
	if ( pItem->u.tImage.pMaskScene != NULL ) {
		iRet = xgeShapeExSceneAddRef(pItem->u.tImage.pMaskScene);
		if ( iRet != XGE_OK ) {
			__xgeSvgDrawItemReset(pItem);
			return iRet;
		}
	}
	pSvg->iItemCount++;
	return XGE_OK;
}

static int __xgeSvgAddRasterItemCopy(xge_svg pSvg, const xge_svg_raster_item_t* pImage, int iDefIndex)
{
	xge_svg_draw_item_t* pItem;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pImage == NULL) || (pImage->pRaster == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (pImage->tRect.fW <= 0.0f) || (pImage->tRect.fH <= 0.0f) ) {
		return XGE_OK;
	}
	if ( iDefIndex >= 0 ) {
		if ( iDefIndex >= pSvg->iDefCount ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		return __xgeSvgDefAddRasterImage(&pSvg->pDefs[iDefIndex], pImage);
	}
	iRet = __xgeSvgDrawItemReserve(pSvg);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pItem = &pSvg->pItems[pSvg->iItemCount];
	memset(pItem, 0, sizeof(*pItem));
	pItem->iType = XGE_SVG_DRAW_ITEM_RASTER_IMAGE;
	pItem->u.tRaster = *pImage;
	iRet = __xgeSvgRasterImageAddRef(pItem->u.tRaster.pRaster);
	if ( iRet <= 0 ) {
		__xgeSvgDrawItemReset(pItem);
		return iRet;
	}
	if ( pItem->u.tRaster.pMaskScene != NULL ) {
		iRet = xgeShapeExSceneAddRef(pItem->u.tRaster.pMaskScene);
		if ( iRet != XGE_OK ) {
			__xgeSvgDrawItemReset(pItem);
			return iRet;
		}
	}
	pSvg->iItemCount++;
	return XGE_OK;
}

static xge_rect_t __xgeSvgTextMaskBounds(const xge_svg_text_item_t* pText)
{
	xge_rect_t tBounds;
	float fWidth;
	float fHeight;

	memset(&tBounds, 0, sizeof(tBounds));
	if ( (pText == NULL) || (pText->tStyle.fFontSize <= 0.0f) ) {
		return tBounds;
	}
	fHeight = pText->tStyle.fFontSize;
	fWidth = pText->fAnchorAdvance;
	if ( fWidth <= 0.0f && pText->sText != NULL ) {
		fWidth = (float)strlen(pText->sText) * fHeight * 0.5f;
	}
	tBounds.fX = pText->fX;
	if ( pText->tStyle.iTextAnchor == XGE_SVG_TEXT_ANCHOR_MIDDLE ) {
		tBounds.fX -= fWidth * 0.5f;
	} else if ( pText->tStyle.iTextAnchor == XGE_SVG_TEXT_ANCHOR_END ) {
		tBounds.fX -= fWidth;
	}
	tBounds.fY = pText->fY - fHeight;
	tBounds.fW = fWidth;
	tBounds.fH = fHeight;
	return tBounds;
}

static int __xgeSvgApplyMaskShapeToShape(const xge_svg_mask_t* pMask, const xge_svg_mask_shape_t* pMaskShape, xge_shape_ex pTarget)
{
	xge_shape_ex pClipShape;
	int iRet;

	if ( (pMask == NULL) || (pMaskShape == NULL) || (pMaskShape->pShape == NULL) || (pTarget == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pMask->iContentUnits == XGE_SVG_MASK_OBJECT_BOUNDING_BOX ) {
		xge_rect_t tBounds;
		xge_shape_ex_matrix_t tBoundsMatrix;
		xge_shape_ex_matrix_t tTransform;

		iRet = xgeShapeExGetBounds(pTarget, 0.0f, &tBounds);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		if ( (tBounds.fW <= 0.0f) || (tBounds.fH <= 0.0f) ) {
			return XGE_OK;
		}
		iRet = xgeShapeExClone(pMaskShape->pShape, &pClipShape);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		tBoundsMatrix = __xgeSvgMatrixMul(__xgeSvgMatrixTranslate(tBounds.fX, tBounds.fY), __xgeSvgMatrixScale(tBounds.fW, tBounds.fH));
		tTransform = __xgeSvgMatrixMul(tBoundsMatrix, pClipShape->tTransform);
		xgeShapeExTransformSet(pClipShape, &tTransform);
		iRet = xgeShapeExClipShapeAdd(pTarget, pClipShape);
		xgeShapeExDestroy(pClipShape);
		return iRet;
	}
	iRet = xgeShapeExClone(pMaskShape->pShape, &pClipShape);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = xgeShapeExClipShapeAdd(pTarget, pClipShape);
	xgeShapeExDestroy(pClipShape);
	return iRet;
}

static int __xgeSvgMaskShapeResolveForBounds(const xge_svg_mask_t* pMask, const xge_svg_mask_shape_t* pMaskShape, xge_rect_t tBounds, xge_shape_ex* ppShape)
{
	xge_shape_ex pShape;
	int iRet;

	if ( ppShape == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppShape = NULL;
	if ( (pMask == NULL) || (pMaskShape == NULL) || (pMaskShape->pShape == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pMask->iContentUnits == XGE_SVG_MASK_OBJECT_BOUNDING_BOX ) {
		xge_shape_ex_matrix_t tBoundsMatrix;
		xge_shape_ex_matrix_t tTransform;

		if ( (tBounds.fW <= 0.0f) || (tBounds.fH <= 0.0f) ) {
			return XGE_OK;
		}
		iRet = xgeShapeExClone(pMaskShape->pShape, &pShape);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		tBoundsMatrix = __xgeSvgMatrixMul(__xgeSvgMatrixTranslate(tBounds.fX, tBounds.fY), __xgeSvgMatrixScale(tBounds.fW, tBounds.fH));
		tTransform = __xgeSvgMatrixMul(tBoundsMatrix, pShape->tTransform);
		xgeShapeExTransformSet(pShape, &tTransform);
		*ppShape = pShape;
		return XGE_OK;
	}
	iRet = xgeShapeExClone(pMaskShape->pShape, &pShape);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	*ppShape = pShape;
	return XGE_OK;
}

static int __xgeSvgAddTextItemMasked(xge_svg pSvg, const xge_svg_text_item_t* pText, int iDefIndex)
{
	xge_svg_mask_t* pMask;
	xge_rect_t tBounds;
	xge_shape_ex_scene pMaskScene;
	xge_svg_text_item_t tCopy;
	int iMask;
	int iMaskCount;
	int iRet;
	xge_svg_text_item_t tViewportClipped;

	if ( !__xgeSvgValid(pSvg) || (pText == NULL) || (pText->sText == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iDefIndex < 0) && (pText->tStyle.sFilterId[0] != '\0') ) {
		xge_rect_t tFilterRegion;
		xge_rect_t tFilterOutputRegion;
		float fBlurX;
		float fBlurY;
		int iFilterState;
		int iFilterRegion;

		tBounds = __xgeSvgTextMaskBounds(pText);
		iFilterRegion = __xgeSvgFilterRegionForBounds(pSvg, pText->tStyle.sFilterId, tBounds, &tFilterRegion);
		if ( iFilterRegion < 0 ) {
			return XGE_OK;
		}
		iFilterState = __xgeSvgFilterGaussianBlurForBounds(
			pSvg, pText->tStyle.sFilterId, tBounds, &fBlurX, &fBlurY, &tFilterOutputRegion
		);
		if ( iFilterState == 1 ) {
			xge_svg_text_item_t tIdentity = *pText;

			tIdentity.tStyle.sFilterId[0] = '\0';
			if ( !__xgeSvgTextClipRectMapIntersect(&tIdentity, tFilterOutputRegion) ) {
				return XGE_OK;
			}
			if ( (iFilterRegion > 0) && !__xgeSvgTextClipRectMapIntersect(&tIdentity, tFilterRegion) ) {
				return XGE_OK;
			}
			return __xgeSvgAddTextItemMasked(pSvg, &tIdentity, iDefIndex);
		}
		if ( iFilterState == 2 ) {
			xge_svg_text_item_t tBlur = *pText;
			int iFilterIndex = __xgeSvgFilterFind(pSvg, pText->tStyle.sFilterId);
			int iFirstItem = pSvg->iItemCount;

			if ( iFilterIndex < 0 ) {
				return XGE_OK;
			}
			tBlur.tStyle.sFilterId[0] = '\0';
			iRet = __xgeSvgAddTextItemMasked(pSvg, &tBlur, -1);
			if ( iRet != XGE_OK ) {
				return iRet;
			}
			return __xgeSvgDrawItemsWrapFilter(
				pSvg, iFirstItem, iFilterIndex, tBounds, tFilterOutputRegion,
				pText->tStyle.tTransform
			);
		}
	}
	if ( pText->tStyle.bViewportClip ) {
		tViewportClipped = *pText;
		if ( !__xgeSvgApplyViewportClipToText(&tViewportClipped) ) {
			return XGE_OK;
		}
		pText = &tViewportClipped;
	}
	if ( (pText->pMaskScene != NULL) || (pText->tStyle.sMaskId[0] == '\0') ) {
		return __xgeSvgAddTextItemCopy(pSvg, pText, iDefIndex);
	}
	iMask = __xgeSvgMaskFind(pSvg, pText->tStyle.sMaskId);
	if ( iMask < 0 ) {
		return __xgeSvgAddTextItemCopy(pSvg, pText, iDefIndex);
	}
	pMask = &pSvg->pMasks[iMask];
	if ( pMask->iShapeCount <= 0 ) {
		return XGE_OK;
	}
	tBounds = __xgeSvgTextMaskBounds(pText);
	pMaskScene = NULL;
	iRet = __xgeSvgMaskSceneResolve(pMask, tBounds, &pMaskScene);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( pMaskScene == NULL ) {
		return XGE_OK;
	}
	iMaskCount = 0;
	iRet = xgeShapeExSceneGetCount(pMaskScene, &iMaskCount);
	if ( (iRet == XGE_OK) && (iMaskCount > 0) ) {
		tCopy = *pText;
		tCopy.pMaskScene = pMaskScene;
		tCopy.iMaskMethod = __xgeSvgMaskMethod(pMask);
		iRet = __xgeSvgAddTextItemCopy(pSvg, &tCopy, iDefIndex);
	}
	xgeShapeExSceneDestroy(pMaskScene);
	return iRet;
}

static int __xgeSvgAddImageItemMasked(xge_svg pSvg, const xge_svg_image_item_t* pImage, int iDefIndex)
{
	xge_svg_mask_t* pMask;
	xge_svg_image_item_t tViewportClipped;
	xge_shape_ex_scene pMaskScene;
	xge_svg_image_item_t tCopy;
	int iMask;
	int iMaskCount;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pImage == NULL) || !__xgeSvgValid(pImage->pSvg) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iDefIndex < 0) && (pImage->tStyle.sFilterId[0] != '\0') ) {
		xge_rect_t tFilterRegion;
		xge_rect_t tFilterOutputRegion;
		float fBlurX;
		float fBlurY;
		int iFilterState;
		int iFilterRegion;

		iFilterRegion = __xgeSvgFilterRegionForBounds(pSvg, pImage->tStyle.sFilterId, pImage->tRect, &tFilterRegion);
		if ( iFilterRegion < 0 ) {
			return XGE_OK;
		}
		iFilterState = __xgeSvgFilterGaussianBlurForBounds(
			pSvg, pImage->tStyle.sFilterId, pImage->tRect, &fBlurX, &fBlurY, &tFilterOutputRegion
		);
		if ( iFilterState == 1 ) {
			xge_svg_image_item_t tIdentity = *pImage;

			tIdentity.tStyle.sFilterId[0] = '\0';
			if ( !__xgeSvgImageClipRectMapIntersect(&tIdentity, tFilterOutputRegion) ) {
				return XGE_OK;
			}
			if ( (iFilterRegion > 0) && !__xgeSvgImageClipRectMapIntersect(&tIdentity, tFilterRegion) ) {
				return XGE_OK;
			}
			return __xgeSvgAddImageItemMasked(pSvg, &tIdentity, iDefIndex);
		}
		if ( iFilterState == 2 ) {
			xge_svg_image_item_t tBlur = *pImage;
			int iFilterIndex = __xgeSvgFilterFind(pSvg, pImage->tStyle.sFilterId);
			int iFirstItem = pSvg->iItemCount;

			if ( iFilterIndex < 0 ) {
				return XGE_OK;
			}
			tBlur.tStyle.sFilterId[0] = '\0';
			iRet = __xgeSvgAddImageItemMasked(pSvg, &tBlur, -1);
			if ( iRet != XGE_OK ) {
				return iRet;
			}
			return __xgeSvgDrawItemsWrapFilter(
				pSvg, iFirstItem, iFilterIndex, pImage->tRect, tFilterOutputRegion,
				pImage->tStyle.tTransform
			);
		}
	}
	if ( pImage->tStyle.bViewportClip ) {
		tViewportClipped = *pImage;
		if ( !__xgeSvgApplyViewportClipToImage(&tViewportClipped) ) {
			return XGE_OK;
		}
		pImage = &tViewportClipped;
	}
	if ( (pImage->pMaskScene != NULL) || (pImage->tStyle.sMaskId[0] == '\0') ) {
		return __xgeSvgAddImageItemCopy(pSvg, pImage, iDefIndex);
	}
	iMask = __xgeSvgMaskFind(pSvg, pImage->tStyle.sMaskId);
	if ( iMask < 0 ) {
		return __xgeSvgAddImageItemCopy(pSvg, pImage, iDefIndex);
	}
	pMask = &pSvg->pMasks[iMask];
	if ( pMask->iShapeCount <= 0 ) {
		return XGE_OK;
	}
	pMaskScene = NULL;
	iRet = __xgeSvgMaskSceneResolve(pMask, pImage->tRect, &pMaskScene);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( pMaskScene == NULL ) {
		return XGE_OK;
	}
	iMaskCount = 0;
	iRet = xgeShapeExSceneGetCount(pMaskScene, &iMaskCount);
	if ( (iRet == XGE_OK) && (iMaskCount > 0) ) {
		tCopy = *pImage;
		tCopy.pMaskScene = pMaskScene;
		tCopy.iMaskMethod = __xgeSvgMaskMethod(pMask);
		iRet = __xgeSvgAddImageItemCopy(pSvg, &tCopy, iDefIndex);
	}
	xgeShapeExSceneDestroy(pMaskScene);
	return iRet;
}

static int __xgeSvgAddRasterItemMasked(xge_svg pSvg, const xge_svg_raster_item_t* pImage, int iDefIndex)
{
	xge_svg_mask_t* pMask;
	xge_svg_raster_item_t tViewportClipped;
	xge_shape_ex_scene pMaskScene;
	xge_svg_raster_item_t tCopy;
	int iMask;
	int iMaskCount;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pImage == NULL) || (pImage->pRaster == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iDefIndex < 0) && (pImage->tStyle.sFilterId[0] != '\0') ) {
		xge_rect_t tFilterRegion;
		xge_rect_t tFilterOutputRegion;
		float fBlurX;
		float fBlurY;
		int iFilterState;
		int iFilterRegion;

		iFilterRegion = __xgeSvgFilterRegionForBounds(pSvg, pImage->tStyle.sFilterId, pImage->tRect, &tFilterRegion);
		if ( iFilterRegion < 0 ) {
			return XGE_OK;
		}
		iFilterState = __xgeSvgFilterGaussianBlurForBounds(
			pSvg, pImage->tStyle.sFilterId, pImage->tRect, &fBlurX, &fBlurY, &tFilterOutputRegion
		);
		if ( iFilterState == 1 ) {
			xge_svg_raster_item_t tIdentity = *pImage;

			tIdentity.tStyle.sFilterId[0] = '\0';
			if ( !__xgeSvgRasterClipRectMapIntersect(&tIdentity, tFilterOutputRegion) ) {
				return XGE_OK;
			}
			if ( (iFilterRegion > 0) && !__xgeSvgRasterClipRectMapIntersect(&tIdentity, tFilterRegion) ) {
				return XGE_OK;
			}
			return __xgeSvgAddRasterItemMasked(pSvg, &tIdentity, iDefIndex);
		}
		if ( iFilterState == 2 ) {
			xge_svg_raster_item_t tBlur = *pImage;
			int iFilterIndex = __xgeSvgFilterFind(pSvg, pImage->tStyle.sFilterId);
			int iFirstItem = pSvg->iItemCount;

			if ( iFilterIndex < 0 ) {
				return XGE_OK;
			}
			tBlur.tStyle.sFilterId[0] = '\0';
			iRet = __xgeSvgAddRasterItemMasked(pSvg, &tBlur, -1);
			if ( iRet != XGE_OK ) {
				return iRet;
			}
			return __xgeSvgDrawItemsWrapFilter(
				pSvg, iFirstItem, iFilterIndex, pImage->tRect, tFilterOutputRegion,
				pImage->tStyle.tTransform
			);
		}
	}
	if ( pImage->tStyle.bViewportClip ) {
		tViewportClipped = *pImage;
		if ( !__xgeSvgApplyViewportClipToRaster(&tViewportClipped) ) {
			return XGE_OK;
		}
		pImage = &tViewportClipped;
	}
	if ( (pImage->pMaskScene != NULL) || (pImage->tStyle.sMaskId[0] == '\0') ) {
		return __xgeSvgAddRasterItemCopy(pSvg, pImage, iDefIndex);
	}
	iMask = __xgeSvgMaskFind(pSvg, pImage->tStyle.sMaskId);
	if ( iMask < 0 ) {
		return __xgeSvgAddRasterItemCopy(pSvg, pImage, iDefIndex);
	}
	pMask = &pSvg->pMasks[iMask];
	if ( pMask->iShapeCount <= 0 ) {
		return XGE_OK;
	}
	pMaskScene = NULL;
	iRet = __xgeSvgMaskSceneResolve(pMask, pImage->tRect, &pMaskScene);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( pMaskScene == NULL ) {
		return XGE_OK;
	}
	iMaskCount = 0;
	iRet = xgeShapeExSceneGetCount(pMaskScene, &iMaskCount);
	if ( (iRet == XGE_OK) && (iMaskCount > 0) ) {
		tCopy = *pImage;
		tCopy.pMaskScene = pMaskScene;
		tCopy.iMaskMethod = __xgeSvgMaskMethod(pMask);
		iRet = __xgeSvgAddRasterItemCopy(pSvg, &tCopy, iDefIndex);
	}
	xgeShapeExSceneDestroy(pMaskScene);
	return iRet;
}

static xge_svg_pattern_t* __xgeSvgPatternGetForPaint(xge_svg pSvg, const char* sPaintId)
{
	int iPattern;

	if ( !__xgeSvgValid(pSvg) || (sPaintId == NULL) || (sPaintId[0] == '\0') ) {
		return NULL;
	}
	if ( (__xgeSvgLinearGradientFind(pSvg, sPaintId) >= 0) ||
	     (__xgeSvgRadialGradientFind(pSvg, sPaintId) >= 0) ) {
		return NULL;
	}
	iPattern = __xgeSvgPatternFind(pSvg, sPaintId);
	if ( iPattern >= 0 ) {
		if ( __xgeSvgResolvePattern(pSvg, iPattern, 0) != XGE_OK ) {
			return NULL;
		}
	}
	if ( (iPattern < 0) || (pSvg->pPatterns[iPattern].iDefIndex < 0) ||
	     (pSvg->pPatterns[iPattern].iDefIndex >= pSvg->iDefCount) ||
	     (pSvg->pPatterns[iPattern].tRect.fW <= 0.0f) ||
	     (pSvg->pPatterns[iPattern].tRect.fH <= 0.0f) ) {
		return NULL;
	}
	return &pSvg->pPatterns[iPattern];
}

static xge_rect_t __xgeSvgPatternRectToUser(const xge_svg_pattern_t* pPattern, xge_rect_t tBounds)
{
	xge_rect_t tRect;

	tRect = pPattern->tRect;
	if ( pPattern->iUnits == XGE_SVG_PATTERN_OBJECT_BOUNDING_BOX ) {
		tRect.fX = tBounds.fX + tRect.fX * tBounds.fW;
		tRect.fY = tBounds.fY + tRect.fY * tBounds.fH;
		tRect.fW = tRect.fW * tBounds.fW;
		tRect.fH = tRect.fH * tBounds.fH;
	}
	return tRect;
}

static float __xgeSvgPatternFirstTile(float fStart, float fSize, float fLimit)
{
	if ( fSize <= 0.0f ) {
		return fStart;
	}
	while ( fStart > fLimit ) {
		fStart -= fSize;
	}
	while ( (fStart + fSize) <= fLimit ) {
		fStart += fSize;
	}
	return fStart;
}

static xge_shape_ex_matrix_t __xgeSvgPatternTileMatrix(xge_svg pSvg, const xge_svg_pattern_t* pPattern, xge_rect_t tTile, xge_rect_t tObjectBounds)
{
	xge_shape_ex_matrix_t tBase;
	xge_svg_def_t* pDef;

	pDef = &pSvg->pDefs[pPattern->iDefIndex];
	if ( pDef->bHasViewBox ) {
		xge_rect_t tViewport = __xgeSvgAspectViewport(pDef->tViewBox, tTile, pDef->iAspectAlignX, pDef->iAspectAlignY, pDef->iAspectMeetOrSlice);
		tBase = __xgeSvgViewBoxMatrix(pDef->tViewBox, tViewport);
	} else {
		tBase = __xgeSvgMatrixTranslate(tTile.fX, tTile.fY);
		if ( pPattern->iContentUnits == XGE_SVG_PATTERN_OBJECT_BOUNDING_BOX ) {
			tBase = __xgeSvgMatrixMul(tBase, __xgeSvgMatrixScale(tObjectBounds.fW, tObjectBounds.fH));
		}
	}
	return __xgeSvgMatrixMul(pPattern->tTransform, tBase);
}

static xge_rect_t __xgeSvgRectInflate(xge_rect_t tRect, float fMargin)
{
	if ( fMargin <= 0.0f ) {
		return tRect;
	}
	tRect.fX -= fMargin;
	tRect.fY -= fMargin;
	tRect.fW += fMargin * 2.0f;
	tRect.fH += fMargin * 2.0f;
	return tRect;
}

static void __xgeSvgPatternContentBoundsUnion(xge_rect_t* pOut, int* pHasBounds, xge_rect_t tBounds)
{
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;

	if ( (pOut == NULL) || (pHasBounds == NULL) || (tBounds.fW <= 0.0f) || (tBounds.fH <= 0.0f) ) {
		return;
	}
	if ( !*pHasBounds ) {
		*pOut = tBounds;
		*pHasBounds = 1;
		return;
	}
	fLeft = fminf(pOut->fX, tBounds.fX);
	fTop = fminf(pOut->fY, tBounds.fY);
	fRight = fmaxf(pOut->fX + pOut->fW, tBounds.fX + tBounds.fW);
	fBottom = fmaxf(pOut->fY + pOut->fH, tBounds.fY + tBounds.fH);
	pOut->fX = fLeft;
	pOut->fY = fTop;
	pOut->fW = fRight - fLeft;
	pOut->fH = fBottom - fTop;
}

static int __xgeSvgPatternContentBounds(xge_svg pSvg, const xge_svg_pattern_t* pPattern, xge_rect_t tObjectBounds, xge_rect_t tTileBase, xge_rect_t* pBounds)
{
	xge_svg_def_t* pDef;
	xge_shape_ex_matrix_t tTileMatrix;
	xge_rect_t tOut;
	int bHasBounds;
	int i;

	if ( !__xgeSvgValid(pSvg) || (pPattern == NULL) || (pBounds == NULL) ||
	     (pPattern->iDefIndex < 0) || (pPattern->iDefIndex >= pSvg->iDefCount) ) {
		return 0;
	}
	pDef = &pSvg->pDefs[pPattern->iDefIndex];
	tTileMatrix = __xgeSvgPatternTileMatrix(pSvg, pPattern, tTileBase, tObjectBounds);
	memset(&tOut, 0, sizeof(tOut));
	bHasBounds = 0;
	for ( i = 0; i < pDef->iShapeCount; i++ ) {
		xge_rect_t tBounds;

		if ( (pDef->pShapes[i] != NULL) && (xgeShapeExGetBounds(pDef->pShapes[i], 0.25f, &tBounds) == XGE_OK) ) {
			tBounds = __xgeSvgMatrixRectBounds(tTileMatrix, tBounds);
			__xgeSvgPatternContentBoundsUnion(&tOut, &bHasBounds, tBounds);
		}
	}
	for ( i = 0; i < pDef->iTextCount; i++ ) {
		xge_rect_t tBounds;
		xge_shape_ex_matrix_t tMatrix;

		tBounds = __xgeSvgTextMaskBounds(&pDef->pTexts[i]);
		if ( pDef->pTexts[i].bClipRect ) {
			tBounds = __xgeSvgRectIntersect(tBounds, pDef->pTexts[i].tClipRect);
		}
		tMatrix = __xgeSvgMatrixMul(tTileMatrix, pDef->pTexts[i].tStyle.tTransform);
		tBounds = __xgeSvgMatrixRectBounds(tMatrix, tBounds);
		__xgeSvgPatternContentBoundsUnion(&tOut, &bHasBounds, tBounds);
	}
	for ( i = 0; i < pDef->iImageCount; i++ ) {
		xge_rect_t tBounds;
		xge_shape_ex_matrix_t tMatrix;

		tBounds = pDef->pImages[i].tRect;
		if ( pDef->pImages[i].bClipRect ) {
			tBounds = __xgeSvgRectIntersect(tBounds, pDef->pImages[i].tClipRect);
		}
		tMatrix = __xgeSvgMatrixMul(tTileMatrix, pDef->pImages[i].tStyle.tTransform);
		tBounds = __xgeSvgMatrixRectBounds(tMatrix, tBounds);
		__xgeSvgPatternContentBoundsUnion(&tOut, &bHasBounds, tBounds);
	}
	for ( i = 0; i < pDef->iRasterCount; i++ ) {
		xge_rect_t tBounds;
		xge_shape_ex_matrix_t tMatrix;

		tBounds = pDef->pRasters[i].tRect;
		if ( pDef->pRasters[i].bClipRect ) {
			tBounds = __xgeSvgRectIntersect(tBounds, pDef->pRasters[i].tClipRect);
		}
		tMatrix = __xgeSvgMatrixMul(tTileMatrix, pDef->pRasters[i].tStyle.tTransform);
		tBounds = __xgeSvgMatrixRectBounds(tMatrix, tBounds);
		__xgeSvgPatternContentBoundsUnion(&tOut, &bHasBounds, tBounds);
	}
	*pBounds = tOut;
	return bHasBounds;
}

static int __xgeSvgPatternTargetMaskCreate(xge_shape_ex pTargetShape, int bStroke, xge_shape_ex_matrix_t tItemTransform, xge_shape_ex* ppMaskShape)
{
	xge_shape_ex pMaskShape;
	xge_shape_ex_matrix_t tInverse;
	xge_shape_ex_matrix_t tRelative;
	int iRet;

	if ( (pTargetShape == NULL) || (ppMaskShape == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppMaskShape = NULL;
	if ( !__xgeSvgMatrixInvert(tItemTransform, &tInverse) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = xgeShapeExClone(pTargetShape, &pMaskShape);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	tRelative = __xgeSvgMatrixMul(tInverse, pTargetShape->tTransform);
	xgeShapeExTransformSet(pMaskShape, &tRelative);
	if ( bStroke ) {
		xgeShapeExFillColor(pMaskShape, XGE_COLOR_RGBA(255, 255, 255, 0));
		xgeShapeExStrokeColor(pMaskShape, XGE_COLOR_RGBA(255, 255, 255, 255));
	} else {
		xgeShapeExFillColor(pMaskShape, XGE_COLOR_RGBA(255, 255, 255, 255));
		xgeShapeExStrokeColor(pMaskShape, XGE_COLOR_RGBA(255, 255, 255, 0));
	}
	pMaskShape->bStencilPaint = 1;
	*ppMaskShape = pMaskShape;
	return XGE_OK;
}

static int __xgeSvgPatternClipShapeToTarget(xge_shape_ex pShape, xge_shape_ex pTargetShape, int bStroke, xge_shape_ex_matrix_t tItemTransform)
{
	xge_shape_ex pMaskShape;
	int iRet;

	pMaskShape = NULL;
	iRet = __xgeSvgPatternTargetMaskCreate(pTargetShape, bStroke, tItemTransform, &pMaskShape);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = xgeShapeExClipShapeAdd(pShape, pMaskShape);
	xgeShapeExDestroy(pMaskShape);
	return iRet;
}

static int __xgeSvgPatternItemMaskSceneCreate(xge_shape_ex_scene pExistingMask, xge_shape_ex pTargetShape, int bStroke, xge_shape_ex_matrix_t tItemTransform, xge_shape_ex_scene* ppMaskScene)
{
	xge_shape_ex pTargetMask;
	xge_shape_ex_scene pCombined;
	int iRet;

	if ( ppMaskScene == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppMaskScene = NULL;
	pTargetMask = NULL;
	pCombined = NULL;
	iRet = __xgeSvgPatternTargetMaskCreate(pTargetShape, bStroke, tItemTransform, &pTargetMask);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( pExistingMask == NULL ) {
		iRet = xgeShapeExSceneCreate(&pCombined);
		if ( iRet == XGE_OK ) {
			iRet = xgeShapeExSceneAdd(pCombined, pTargetMask);
		}
	} else {
		iRet = xgeShapeExSceneClone(pExistingMask, &pCombined);
		if ( iRet == XGE_OK ) {
			iRet = xgeShapeExSceneClipShapeAdd(pCombined, pTargetMask);
		}
	}
	xgeShapeExDestroy(pTargetMask);
	if ( iRet != XGE_OK ) {
		xgeShapeExSceneDestroy(pCombined);
		return iRet;
	}
	*ppMaskScene = pCombined;
	return XGE_OK;
}

static int __xgeSvgAddPatternShapeTiles(xge_svg pSvg, const xge_svg_pattern_t* pPattern, xge_shape_ex pTargetShape, const xge_svg_style_t* pStyle, int iDefIndex, xge_rect_t tObjectBounds, xge_rect_t tTargetBounds, xge_rect_t tTileBase, float fPaintOpacity, int bStrokePattern)
{
	xge_svg_def_t* pDef;
	xge_rect_t tTargetClip;
	float fX;
	float fY;
	float fRight;
	float fBottom;
	int i;
	int iRet;

	if ( (tTileBase.fW <= 0.0f) || (tTileBase.fH <= 0.0f) ) {
		return XGE_OK;
	}
	pDef = &pSvg->pDefs[pPattern->iDefIndex];
	if ( (pDef->iShapeCount <= 0) && (pDef->iTextCount <= 0) && (pDef->iImageCount <= 0) && (pDef->iRasterCount <= 0) ) {
		return XGE_OK;
	}
	tTargetClip = tTargetBounds;
	if ( pTargetShape->bClipRect ) {
		tTargetClip = __xgeSvgRectIntersect(tTargetClip, pTargetShape->tClipRect);
	}
	if ( (tTargetClip.fW <= 0.0f) || (tTargetClip.fH <= 0.0f) ) {
		return XGE_OK;
	}
	fRight = tTargetClip.fX + tTargetClip.fW;
	fBottom = tTargetClip.fY + tTargetClip.fH;
	if ( pDef->bOverflowVisible ) {
		xge_rect_t tContentBounds;
		if ( __xgeSvgPatternContentBounds(pSvg, pPattern, tObjectBounds, tTileBase, &tContentBounds) ) {
			float fTileRight = tTileBase.fX + tTileBase.fW;
			float fTileBottom = tTileBase.fY + tTileBase.fH;
			float fOverflowLeft = fmaxf(0.0f, tTileBase.fX - tContentBounds.fX);
			float fOverflowTop = fmaxf(0.0f, tTileBase.fY - tContentBounds.fY);
			float fOverflowRight = fmaxf(0.0f, (tContentBounds.fX + tContentBounds.fW) - fTileRight);
			float fOverflowBottom = fmaxf(0.0f, (tContentBounds.fY + tContentBounds.fH) - fTileBottom);

			fRight += fOverflowLeft;
			fBottom += fOverflowTop;
			tTargetClip.fX -= fOverflowRight;
			tTargetClip.fY -= fOverflowBottom;
			tTargetClip.fW += fOverflowRight + fOverflowLeft;
			tTargetClip.fH += fOverflowBottom + fOverflowTop;
		}
	}
	fY = __xgeSvgPatternFirstTile(tTileBase.fY, tTileBase.fH, tTargetClip.fY);
	while ( fY < fBottom ) {
		fX = __xgeSvgPatternFirstTile(tTileBase.fX, tTileBase.fW, tTargetClip.fX);
		while ( fX < fRight ) {
			xge_rect_t tTile;
			xge_rect_t tClip;
			xge_shape_ex_matrix_t tTileMatrix;
			xge_svg_style_t tMaskStyle;

			tTile.fX = fX;
			tTile.fY = fY;
			tTile.fW = tTileBase.fW;
			tTile.fH = tTileBase.fH;
			tClip = pDef->bOverflowVisible ? tTargetClip : __xgeSvgRectIntersect(tTargetClip, tTile);
			if ( (tClip.fW > 0.0f) && (tClip.fH > 0.0f) ) {
				tTileMatrix = __xgeSvgPatternTileMatrix(pSvg, pPattern, tTile, tObjectBounds);
				tMaskStyle = *pStyle;
				tMaskStyle.sFillGradientId[0] = '\0';
				tMaskStyle.sStrokeGradientId[0] = '\0';
				for ( i = 0; i < pDef->iShapeCount; i++ ) {
					xge_shape_ex pClone;
					xge_shape_ex_matrix_t tShapeTransform;
					xge_rect_t tShapeClip;

					iRet = xgeShapeExClone(pDef->pShapes[i], &pClone);
					if ( iRet != XGE_OK ) {
						return iRet;
					}
					tShapeTransform = __xgeSvgMatrixMul(tTileMatrix, pClone->tTransform);
					if ( !__xgeSvgMatrixRectBoundsInverse(tShapeTransform, tClip, &tShapeClip) ) {
						xgeShapeExDestroy(pClone);
						continue;
					}
					xgeShapeExTransformSet(pClone, &tShapeTransform);
					xgeShapeExOpacity(pClone, pClone->fOpacity * pStyle->fOpacity * fPaintOpacity);
					if ( pClone->bClipRect ) {
						tShapeClip = __xgeSvgRectIntersect(tShapeClip, pClone->tClipRect);
					}
					xgeShapeExClipRectSet(pClone, tShapeClip);
					iRet = __xgeSvgPatternClipShapeToTarget(pClone, pTargetShape, bStrokePattern, tShapeTransform);
					if ( iRet != XGE_OK ) {
						xgeShapeExDestroy(pClone);
						return iRet;
					}
					iRet = __xgeSvgAddStyledShapeMasked(pSvg, pClone, &tMaskStyle, iDefIndex);
					if ( iRet != XGE_OK ) {
						return iRet;
					}
				}
				for ( i = 0; i < pDef->iTextCount; i++ ) {
					xge_svg_text_item_t tText;
					xge_rect_t tTextClip;
					xge_shape_ex_scene pOwnedMaskScene;

					tText = pDef->pTexts[i];
					tText.tStyle.tTransform = __xgeSvgMatrixMul(tTileMatrix, tText.tStyle.tTransform);
					tText.tStyle.fOpacity *= pStyle->fOpacity * fPaintOpacity;
					if ( !pStyle->bVisible || !pStyle->bVisibility ) {
						tText.tStyle.bVisible = 0;
					}
					if ( !__xgeSvgMatrixRectBoundsInverse(tText.tStyle.tTransform, tClip, &tTextClip) ) {
						continue;
					}
					if ( tText.bClipRect ) {
						tTextClip = __xgeSvgRectIntersect(tTextClip, tText.tClipRect);
					}
					tText.bClipRect = 1;
					tText.tClipRect = tTextClip;
					pOwnedMaskScene = NULL;
					if ( tText.pMaskScene == NULL ) tText.iMaskMethod = XGE_SHAPE_EX_MASK_ALPHA;
					iRet = __xgeSvgPatternItemMaskSceneCreate(tText.pMaskScene, pTargetShape, bStrokePattern, tText.tStyle.tTransform, &pOwnedMaskScene);
					if ( iRet != XGE_OK ) {
						return iRet;
					}
					tText.pMaskScene = pOwnedMaskScene;
					iRet = __xgeSvgAddTextItemMasked(pSvg, &tText, iDefIndex);
					xgeShapeExSceneDestroy(pOwnedMaskScene);
					if ( iRet != XGE_OK ) {
						return iRet;
					}
				}
				for ( i = 0; i < pDef->iImageCount; i++ ) {
					xge_svg_image_item_t tImage;
					xge_rect_t tImageClip;
					xge_shape_ex_scene pOwnedMaskScene;

					tImage = pDef->pImages[i];
					tImage.tStyle.tTransform = __xgeSvgMatrixMul(tTileMatrix, tImage.tStyle.tTransform);
					tImage.tStyle.fOpacity *= pStyle->fOpacity * fPaintOpacity;
					if ( !pStyle->bVisible || !pStyle->bVisibility ) {
						tImage.tStyle.bVisible = 0;
					}
					if ( !__xgeSvgMatrixRectBoundsInverse(tImage.tStyle.tTransform, tClip, &tImageClip) ) {
						continue;
					}
					if ( tImage.bClipRect ) {
						tImageClip = __xgeSvgRectIntersect(tImageClip, tImage.tClipRect);
					}
					tImage.bClipRect = 1;
					tImage.tClipRect = tImageClip;
					pOwnedMaskScene = NULL;
					if ( tImage.pMaskScene == NULL ) tImage.iMaskMethod = XGE_SHAPE_EX_MASK_ALPHA;
					iRet = __xgeSvgPatternItemMaskSceneCreate(tImage.pMaskScene, pTargetShape, bStrokePattern, tImage.tStyle.tTransform, &pOwnedMaskScene);
					if ( iRet != XGE_OK ) {
						return iRet;
					}
					tImage.pMaskScene = pOwnedMaskScene;
					iRet = __xgeSvgAddImageItemMasked(pSvg, &tImage, iDefIndex);
					xgeShapeExSceneDestroy(pOwnedMaskScene);
					if ( iRet != XGE_OK ) {
						return iRet;
					}
				}
				for ( i = 0; i < pDef->iRasterCount; i++ ) {
					xge_svg_raster_item_t tImage;
					xge_rect_t tImageClip;
					xge_shape_ex_scene pOwnedMaskScene;

					tImage = pDef->pRasters[i];
					tImage.tStyle.tTransform = __xgeSvgMatrixMul(tTileMatrix, tImage.tStyle.tTransform);
					tImage.tStyle.fOpacity *= pStyle->fOpacity * fPaintOpacity;
					if ( !pStyle->bVisible || !pStyle->bVisibility ) {
						tImage.tStyle.bVisible = 0;
					}
					if ( !__xgeSvgMatrixRectBoundsInverse(tImage.tStyle.tTransform, tClip, &tImageClip) ) {
						continue;
					}
					if ( tImage.bClipRect ) {
						tImageClip = __xgeSvgRectIntersect(tImageClip, tImage.tClipRect);
					}
					tImage.bClipRect = 1;
					tImage.tClipRect = tImageClip;
					pOwnedMaskScene = NULL;
					if ( tImage.pMaskScene == NULL ) tImage.iMaskMethod = XGE_SHAPE_EX_MASK_ALPHA;
					iRet = __xgeSvgPatternItemMaskSceneCreate(tImage.pMaskScene, pTargetShape, bStrokePattern, tImage.tStyle.tTransform, &pOwnedMaskScene);
					if ( iRet != XGE_OK ) {
						return iRet;
					}
					tImage.pMaskScene = pOwnedMaskScene;
					iRet = __xgeSvgAddRasterItemMasked(pSvg, &tImage, iDefIndex);
					xgeShapeExSceneDestroy(pOwnedMaskScene);
					if ( iRet != XGE_OK ) {
						return iRet;
					}
				}
			}
			fX += tTileBase.fW;
		}
		fY += tTileBase.fH;
	}
	return XGE_OK;
}

static int __xgeSvgAddPatternFill(xge_svg pSvg, xge_shape_ex pShape, const xge_svg_style_t* pStyle, int iDefIndex)
{
	(void)pSvg;
	(void)pShape;
	(void)pStyle;
	(void)iDefIndex;
	return XGE_OK;
}

static int __xgeSvgHasPatternStroke(xge_svg pSvg, const xge_svg_style_t* pStyle)
{
	(void)pSvg;
	(void)pStyle;
	return 0;
}

static int __xgeSvgAddPatternStroke(xge_svg pSvg, xge_shape_ex pShape, const xge_svg_style_t* pStyle, int iDefIndex)
{
	xge_svg_pattern_t* pPattern;
	xge_rect_t tBounds;
	xge_rect_t tStrokeBounds;
	xge_rect_t tTileBase;
	float fStrokeScale;
	float fMargin;
	int iRet;

	if ( !__xgeSvgHasPatternStroke(pSvg, pStyle) ) {
		return XGE_OK;
	}
	pPattern = __xgeSvgPatternGetForPaint(pSvg, pStyle->sStrokeGradientId);
	if ( pPattern == NULL ) {
		return XGE_OK;
	}
	iRet = xgeShapeExGetBounds(pShape, 0.25f, &tBounds);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( (tBounds.fW <= 0.0f) || (tBounds.fH <= 0.0f) ) {
		return XGE_OK;
	}
	if ( pShape->bStrokeNonScaling || (xgeShapeExMatrixStrokeScale(&fStrokeScale, &pShape->tTransform) != XGE_OK) ) {
		fStrokeScale = 1.0f;
	}
	fMargin = (pStyle->fStrokeWidth * fStrokeScale * 0.5f) + 1.0f;
	tStrokeBounds = __xgeSvgRectInflate(tBounds, fMargin);
	tTileBase = __xgeSvgPatternRectToUser(pPattern, tBounds);
	return __xgeSvgAddPatternShapeTiles(pSvg, pPattern, pShape, pStyle, iDefIndex, tBounds, tStrokeBounds, tTileBase, pStyle->fStrokeOpacity, 1);
}

static int __xgeSvgAddStyledShape(xge_svg pSvg, xge_shape_ex pShape, const xge_svg_style_t* pStyle, int iDefIndex)
{
	xge_svg_clip_path_t* pClip;
	xge_svg_style_t tViewportClippedStyle;
	xge_shape_ex pStrokePatternShape;
	int bStrokePattern;
	int iPendingFlags;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pShape == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iDefIndex < 0) && (pStyle != NULL) && (pStyle->sFilterId[0] != '\0') ) {
		xge_rect_t tBounds;
		xge_rect_t tFilterRegion;
		xge_rect_t tFilterOutputRegion;
		float fBlurX;
		float fBlurY;
		int iFilterState;
		int iFilterRegion;

		if ( xgeShapeExGetBounds(pShape, 0.0f, &tBounds) == XGE_OK ) {
			iFilterRegion = __xgeSvgFilterRegionForBounds(pSvg, pStyle->sFilterId, tBounds, &tFilterRegion);
			if ( iFilterRegion < 0 ) {
				xgeShapeExDestroy(pShape);
				return XGE_OK;
			}
			iFilterState = __xgeSvgFilterGaussianBlurForBounds(
				pSvg, pStyle->sFilterId, tBounds, &fBlurX, &fBlurY, &tFilterOutputRegion
			);
			if ( iFilterState == 1 ) {
				xge_svg_style_t tIdentityStyle = *pStyle;

				tIdentityStyle.sFilterId[0] = '\0';
				if ( !__xgeSvgShapeClipRectMapIntersect(pShape, tFilterOutputRegion) ) {
					xgeShapeExDestroy(pShape);
					return XGE_OK;
				}
				if ( (iFilterRegion > 0) && !__xgeSvgShapeClipRectMapIntersect(pShape, tFilterRegion) ) {
					xgeShapeExDestroy(pShape);
					return XGE_OK;
				}
				return __xgeSvgAddStyledShape(pSvg, pShape, &tIdentityStyle, iDefIndex);
			}
			if ( iFilterState == 2 ) {
				xge_svg_style_t tBlurStyle = *pStyle;
				xge_shape_ex_matrix_t tFilterTransform = pShape->tTransform;
				int iFilterIndex = __xgeSvgFilterFind(pSvg, pStyle->sFilterId);
				int iFirstItem = pSvg->iItemCount;

				if ( iFilterIndex < 0 ) {
					xgeShapeExDestroy(pShape);
					return XGE_OK;
				}
				tBlurStyle.sFilterId[0] = '\0';
				iRet = __xgeSvgAddStyledShape(pSvg, pShape, &tBlurStyle, -1);
				if ( iRet != XGE_OK ) {
					return iRet;
				}
				iRet = __xgeSvgDrawItemsWrapFilter(
					pSvg, iFirstItem, iFilterIndex, tBounds, tFilterOutputRegion,
					tFilterTransform
				);
				if ( (iRet == XGE_OK) && (pSvg->iItemCount == (iFirstItem + 1)) &&
				     (pSvg->pItems[iFirstItem].iType == XGE_SVG_DRAW_ITEM_FILTER) &&
				     (pSvg->pItems[iFirstItem].u.pFilter != NULL) &&
				     (pSvg->pItems[iFirstItem].u.pFilter->iItemCount == 1) ) {
					xge_svg_draw_item_t* pChild = &pSvg->pItems[iFirstItem].u.pFilter->pItems[0];

					pSvg->pItems[iFirstItem].iId = pChild->iId;
					pChild->iId = 0;
				}
				return iRet;
			}
		}
	}
	if ( !__xgeSvgApplyViewportClipToShape(pShape, pStyle) ) {
		xgeShapeExDestroy(pShape);
		return XGE_OK;
	}
	if ( (pStyle != NULL) && pStyle->bViewportClip ) {
		tViewportClippedStyle = *pStyle;
		tViewportClippedStyle.bViewportClip = 0;
		pStyle = &tViewportClippedStyle;
	}
	iPendingFlags = 0;
	if ( pStyle != NULL ) {
		if ( (pStyle->sFillGradientId[0] != '\0') &&
		     (__xgeSvgPatternFind(pSvg, pStyle->sFillGradientId) < 0) &&
		     (__xgeSvgLinearGradientFind(pSvg, pStyle->sFillGradientId) < 0) &&
		     (__xgeSvgRadialGradientFind(pSvg, pStyle->sFillGradientId) < 0) ) {
			iPendingFlags |= XGE_SVG_PENDING_SHAPE_FILL_GRADIENT;
		}
		if ( (pStyle->sStrokeGradientId[0] != '\0') &&
		     (__xgeSvgPatternFind(pSvg, pStyle->sStrokeGradientId) < 0) &&
		     (__xgeSvgLinearGradientFind(pSvg, pStyle->sStrokeGradientId) < 0) &&
		     (__xgeSvgRadialGradientFind(pSvg, pStyle->sStrokeGradientId) < 0) ) {
			iPendingFlags |= XGE_SVG_PENDING_SHAPE_STROKE_GRADIENT;
		}
		if ( (pStyle->sClipId[0] != '\0') && (__xgeSvgClipPathFind(pSvg, pStyle->sClipId) < 0) ) {
			iPendingFlags |= XGE_SVG_PENDING_SHAPE_CLIP;
		}
	}
	if ( iPendingFlags != 0 ) {
		iRet = __xgeSvgPendingShapeResourceAdd(pSvg, pShape, pStyle, iPendingFlags);
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pShape);
			return iRet;
		}
	}
	pClip = (pStyle != NULL) ? __xgeSvgClipPathGetById(pSvg, pStyle->sClipId) : NULL;
	if ( (pStyle != NULL) && (pStyle->sClipId[0] != '\0') &&
	     (__xgeSvgClipPathFind(pSvg, pStyle->sClipId) >= 0) && (pClip == NULL) ) {
		xgeShapeExDestroy(pShape);
		return XGE_OK;
	}
	if ( pClip != NULL ) {
		pShape->bAxisAlignedFillAntialias = pStyle->bClipPathLocal;
		iRet = __xgeSvgApplyClipPath(pSvg, pShape, pStyle->sClipId);
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pShape);
			return iRet;
		}
	}
	pStrokePatternShape = NULL;
	bStrokePattern = __xgeSvgHasPatternStroke(pSvg, pStyle);
	if ( bStrokePattern ) {
		iRet = xgeShapeExClone(pShape, &pStrokePatternShape);
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pShape);
			return iRet;
		}
	}
	if ( bStrokePattern && pStyle->bStrokeFirst ) {
		iRet = __xgeSvgAddPatternStroke(pSvg, pStrokePatternShape, pStyle, iDefIndex);
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pStrokePatternShape);
			xgeShapeExDestroy(pShape);
			return iRet;
		}
	}
	iRet = __xgeSvgAddPatternFill(pSvg, pShape, pStyle, iDefIndex);
	if ( iRet != XGE_OK ) {
		xgeShapeExDestroy(pStrokePatternShape);
		xgeShapeExDestroy(pShape);
		return iRet;
	}
	if ( bStrokePattern ) {
		xgeShapeExStrokeColor(pShape, XGE_COLOR_RGBA(0, 0, 0, 0));
	}
	iRet = __xgeSvgAddStyledShapeMasked(pSvg, pShape, pStyle, iDefIndex);
	if ( iRet != XGE_OK ) {
		xgeShapeExDestroy(pStrokePatternShape);
		return iRet;
	}
	if ( bStrokePattern && !pStyle->bStrokeFirst ) {
		iRet = __xgeSvgAddPatternStroke(pSvg, pStrokePatternShape, pStyle, iDefIndex);
	}
	xgeShapeExDestroy(pStrokePatternShape);
	return iRet;
}

static int __xgeSvgMainIdDefIndex(xge_svg pSvg, const char* pTag, const char* pTagEnd, int bInDefs, int iCurrentDef, int* pDefIndex)
{
	char sId[XGE_SVG_ATTR_MAX];

	if ( pDefIndex != NULL ) {
		*pDefIndex = -1;
	}
	if ( !__xgeSvgValid(pSvg) || (pTag == NULL) || (pTagEnd == NULL) || (pDefIndex == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( bInDefs || (iCurrentDef >= 0) ) {
		return XGE_OK;
	}
	if ( !__xgeSvgAttrCopy(pTag, pTagEnd, "id", sId, sizeof(sId)) || (sId[0] == '\0') ) {
		return XGE_OK;
	}
	return __xgeSvgDefGetOrCreate(pSvg, sId, pDefIndex);
}

static int __xgeSvgAddStyledShapeWithMainIds(xge_svg pSvg, xge_shape_ex pShape, const xge_svg_style_t* pStyle, int iDefIndex, int iMainIdDefIndex, int iExtraMainIdDefIndex);

static int __xgeSvgAddStyledShapeWithMainId(xge_svg pSvg, xge_shape_ex pShape, const xge_svg_style_t* pStyle, int iDefIndex, int iMainIdDefIndex)
{
	return __xgeSvgAddStyledShapeWithMainIds(pSvg, pShape, pStyle, iDefIndex, iMainIdDefIndex, -1);
}

static int __xgeSvgAddStyledShapeWithMainIds(xge_svg pSvg, xge_shape_ex pShape, const xge_svg_style_t* pStyle, int iDefIndex, int iMainIdDefIndex, int iExtraMainIdDefIndex)
{
	xge_shape_ex pMirror;
	xge_shape_ex pExtraMirror;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pShape == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pMirror = NULL;
	pExtraMirror = NULL;
	if ( (iDefIndex < 0) && (iMainIdDefIndex >= 0) ) {
		iRet = xgeShapeExClone(pShape, &pMirror);
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pShape);
			return iRet;
		}
	}
	if ( (iDefIndex < 0) && (iExtraMainIdDefIndex >= 0) && (iExtraMainIdDefIndex != iMainIdDefIndex) ) {
		iRet = xgeShapeExClone(pShape, &pExtraMirror);
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pMirror);
			xgeShapeExDestroy(pShape);
			return iRet;
		}
	}
	if ( (iDefIndex < 0) && (iMainIdDefIndex >= 0) && (iMainIdDefIndex < pSvg->iDefCount) &&
	     (pSvg->pDefs[iMainIdDefIndex].sId != NULL) ) {
		xgeShapeExId(pShape, xgeShapeExIdFromName(pSvg->pDefs[iMainIdDefIndex].sId));
	}
	iRet = __xgeSvgAddStyledShape(pSvg, pShape, pStyle, iDefIndex);
	if ( iRet == XGE_OK && pMirror != NULL ) {
		iRet = __xgeSvgAddStyledShape(pSvg, pMirror, pStyle, iMainIdDefIndex);
		pMirror = NULL;
	}
	if ( iRet == XGE_OK && pExtraMirror != NULL ) {
		iRet = __xgeSvgAddStyledShape(pSvg, pExtraMirror, pStyle, iExtraMainIdDefIndex);
		pExtraMirror = NULL;
	}
	xgeShapeExDestroy(pMirror);
	xgeShapeExDestroy(pExtraMirror);
	return iRet;
}

static int __xgeSvgAddStyledShapeWithLocalMainId(xge_svg pSvg, xge_shape_ex pShape, const xge_svg_style_t* pStyle, int iDefIndex, int iMainIdDefIndex, int iExtraMainIdDefIndex, xge_shape_ex_matrix_t tParentTransform)
{
	int iRet;

	iRet = __xgeSvgAddStyledShapeWithMainIds(pSvg, pShape, pStyle, iDefIndex, iMainIdDefIndex, iExtraMainIdDefIndex);
	if ( (iRet == XGE_OK) && (iDefIndex < 0) && (iMainIdDefIndex >= 0) ) {
		iRet = __xgeSvgDefRemoveParentTransform(pSvg, iMainIdDefIndex, tParentTransform);
	}
	return iRet;
}

static int __xgeSvgShapeTargetDef(xge_svg pSvg, const char* pTag, const char* pTagEnd, int bInDefs, int iCurrentDef, int* pError)
{
	char sId[XGE_SVG_ATTR_MAX];
	int iDefIndex;
	int iRet;

	if ( pError != NULL ) {
		*pError = XGE_OK;
	}
	if ( !bInDefs ) {
		return -1;
	}
	if ( iCurrentDef >= 0 ) {
		return iCurrentDef;
	}
	if ( !__xgeSvgAttrCopy(pTag, pTagEnd, "id", sId, sizeof(sId)) || (sId[0] == '\0') ) {
		return -2;
	}
	iRet = __xgeSvgDefGetOrCreate(pSvg, sId, &iDefIndex);
	if ( iRet != XGE_OK ) {
		if ( pError != NULL ) {
			*pError = iRet;
		}
		return -2;
	}
	return iDefIndex;
}

static int __xgeSvgAddTextObject(xge_svg pSvg, const xge_svg_style_t* pStyle, const char* sText, float fX, float fY, float fAnchorAdvance, int iDefIndex)
{
	xge_svg_text_item_t tText;

	if ( !__xgeSvgValid(pSvg) || (pStyle == NULL) || (sText == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (sText[0] == '\0') || (pStyle->fFontSize <= 0.0f) ) {
		return XGE_OK;
	}
	if ( iDefIndex >= pSvg->iDefCount ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(&tText, 0, sizeof(tText));
	tText.sText = (char*)sText;
	tText.tStyle = *pStyle;
	tText.fX = fX;
	tText.fY = fY;
	tText.fAnchorAdvance = fAnchorAdvance;
	return __xgeSvgAddTextItemMasked(pSvg, &tText, iDefIndex);
}

static char* __xgeSvgTextNormalize(char* sText, int iTextSpace)
{
	char* pRead;
	char* pWrite;
	int bSawSpace;
	int bPendingSpace;

	if ( sText == NULL ) {
		return NULL;
	}
	if ( iTextSpace == XGE_SVG_TEXT_SPACE_PRESERVE ) {
		for ( pWrite = sText; *pWrite != '\0'; pWrite++ ) {
			if ( (*pWrite == '\t') || (*pWrite == '\r') || (*pWrite == '\n') || (*pWrite == '\f') ) {
				*pWrite = ' ';
			}
		}
		return sText;
	}
	pRead = sText;
	pWrite = sText;
	bSawSpace = 0;
	bPendingSpace = 0;
	while ( *pRead != '\0' ) {
		unsigned char c = (unsigned char)*pRead++;

		if ( __xgeSvgIsSpace(c) ) {
			bPendingSpace = 1;
			continue;
		}
		if ( bPendingSpace && bSawSpace ) {
			*pWrite++ = ' ';
		}
		*pWrite++ = (char)c;
		bSawSpace = 1;
		bPendingSpace = 0;
	}
	*pWrite = '\0';
	return sText;
}

static char* __xgeSvgTextDecode(const char* pTextStart, const char* pTextEnd, int iTextSpace)
{
	char* sText;
	size_t iCapacity;

	if ( (pTextStart == NULL) || (pTextEnd == NULL) || (pTextEnd < pTextStart) ) {
		return NULL;
	}
	if ( iTextSpace != XGE_SVG_TEXT_SPACE_PRESERVE ) {
		__xgeSvgTrimRange(&pTextStart, &pTextEnd);
	}
	if ( pTextEnd <= pTextStart ) {
		return __xgeStrDup("");
	}
	iCapacity = (size_t)(pTextEnd - pTextStart) + 1u;
	sText = (char*)xrtMalloc(iCapacity);
	if ( sText == NULL ) {
		return NULL;
	}
	memcpy(sText, pTextStart, iCapacity - 1u);
	sText[iCapacity - 1u] = '\0';
	return __xgeSvgTextNormalize(sText, iTextSpace);
}

static float __xgeSvgTextApproxAdvanceEx(xge_svg pSvg, const char* sText, const xge_svg_style_t* pStyle, int bUseRunBearingAdvance)
{
	xge_font pFont;
	const char* pExact;
	uint32_t iCodepoint;
	const unsigned char* p;
	float fAdvance;
	float fBoundsMin;
	float fBoundsMax;
	float fFirstMin;
	float fPen;
	float fRightBearings;
	float fFontSize;
	int bHasGlyphBounds;

	if ( (sText == NULL) || (pStyle == NULL) ) {
		return 0.0f;
	}
	fFontSize = pStyle->fFontSize;
	if ( fFontSize <= 0.0f ) {
		return 0.0f;
	}
	pFont = NULL;
	if ( __xgeSvgValid(pSvg) && (__xgeSvgFontCacheGet(pSvg, pStyle->sFontFamily, fFontSize, &pFont) == XGE_OK) && (pFont != NULL) ) {
		pExact = sText;
		fAdvance = 0.0f;
		fBoundsMin = 0.0f;
		fBoundsMax = 0.0f;
		fFirstMin = 0.0f;
		fPen = 0.0f;
		fRightBearings = 0.0f;
		bHasGlyphBounds = 0;
		while ( *pExact != '\0' ) {
			xge_glyph_metrics_t tMetrics;

			if ( xgeTextUTF8Next(&pExact, &iCodepoint) != XGE_OK || iCodepoint == '\n' ) {
				break;
			}
			if ( xgeFontGlyphGet(pFont, iCodepoint, &tMetrics) == XGE_OK ) {
				float fGlyphMin = fPen + tMetrics.fX0;
				float fGlyphMax = fPen + tMetrics.fX1;

				if ( !bHasGlyphBounds ) {
					fBoundsMin = fGlyphMin;
					fBoundsMax = fGlyphMax;
					fFirstMin = tMetrics.fX0;
					bHasGlyphBounds = 1;
				} else {
					if ( fGlyphMin < fBoundsMin ) fBoundsMin = fGlyphMin;
					if ( fGlyphMax > fBoundsMax ) fBoundsMax = fGlyphMax;
				}
				fRightBearings += tMetrics.fAdvanceX - tMetrics.fX1;
				fAdvance += tMetrics.fAdvanceX;
				fPen += tMetrics.fAdvanceX;
			}
		}
		if ( bUseRunBearingAdvance && bHasGlyphBounds ) {
			return (fBoundsMax - fBoundsMin) + fFirstMin + fRightBearings;
		}
		return fAdvance;
	}
	p = (const unsigned char*)sText;
	fAdvance = 0.0f;
	while ( *p != '\0' ) {
		if ( *p < 0x80u ) {
			fAdvance += (*p == ' ' || *p == '\t') ? fFontSize * 0.32f : fFontSize * 0.56f;
			p++;
		} else {
			fAdvance += fFontSize;
			if ( (*p & 0xE0u) == 0xC0u ) p += 2;
			else if ( (*p & 0xF0u) == 0xE0u ) p += 3;
			else if ( (*p & 0xF8u) == 0xF0u ) p += 4;
			else p++;
		}
	}
	return fAdvance;
}

static int __xgeSvgAddDecodedTextRun(xge_svg pSvg, const xge_svg_style_t* pStyle, const char* pTextStart, const char* pTextEnd, float fX, float fY, float fAnchorAdvance, int iTargetDef, int iMainIdDefIndex, int iExtraMainIdDefIndex, int bUseRunBearingAdvance, int bEmit, float* pAdvance)
{
	char* sText;
	float fAdvance;
	int iRet;

	if ( pAdvance != NULL ) {
		*pAdvance = 0.0f;
	}
	if ( !__xgeSvgValid(pSvg) || (pStyle == NULL) || (pTextStart == NULL) || (pTextEnd == NULL) || (pTextEnd < pTextStart) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	sText = __xgeSvgTextDecode(pTextStart, pTextEnd, pStyle->iTextSpace);
	if ( sText == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	fAdvance = __xgeSvgTextApproxAdvanceEx(pSvg, sText, pStyle, bUseRunBearingAdvance);
	iRet = XGE_OK;
	if ( bEmit ) {
		iRet = __xgeSvgAddTextObject(pSvg, pStyle, sText, fX, fY, fAnchorAdvance, iTargetDef);
		if ( (iRet == XGE_OK) && (iTargetDef < 0) && (iMainIdDefIndex >= 0) ) {
			iRet = __xgeSvgAddTextObject(pSvg, pStyle, sText, fX, fY, fAnchorAdvance, iMainIdDefIndex);
		}
		if ( (iRet == XGE_OK) && (iTargetDef < 0) && (iExtraMainIdDefIndex >= 0) && (iExtraMainIdDefIndex != iMainIdDefIndex) ) {
			iRet = __xgeSvgAddTextObject(pSvg, pStyle, sText, fX, fY, fAnchorAdvance, iExtraMainIdDefIndex);
		}
	}
	if ( (iRet == XGE_OK) && (pAdvance != NULL) ) {
		*pAdvance = fAdvance;
	}
	xrtFree(sText);
	return iRet;
}

static const char* __xgeSvgFindCloseElement(const char* pStart, const char* sName, const char** ppTagEnd)
{
	const char* p;
	int iDepth;

	if ( ppTagEnd != NULL ) {
		*ppTagEnd = NULL;
	}
	if ( (pStart == NULL) || (sName == NULL) ) {
		return NULL;
	}
	p = pStart;
	iDepth = 0;
	while ( (p = strchr(p, '<')) != NULL ) {
		const char* pTagEnd = __xgeSvgFindTagEnd(p);

		if ( pTagEnd == NULL ) {
			return NULL;
		}
		if ( __xgeSvgIsCloseTagName(p, sName) ) {
			if ( iDepth <= 0 ) {
				if ( ppTagEnd != NULL ) {
					*ppTagEnd = pTagEnd;
				}
				return p;
			}
			iDepth--;
		} else if ( __xgeSvgTagNameEquals(p, sName) && !__xgeSvgIsSelfClosingTag(p, pTagEnd) ) {
			iDepth++;
		}
		p = pTagEnd + 1;
	}
	return NULL;
}

static int __xgeSvgElementRangeEnd(const char* pTag, const char* pTagEnd, const char* pLimit, const char** ppElementEnd)
{
	char sName[XGE_SVG_ATTR_MAX];

	if ( ppElementEnd != NULL ) {
		*ppElementEnd = NULL;
	}
	if ( (pTag == NULL) || (pTagEnd == NULL) || (pLimit == NULL) || (ppElementEnd == NULL) || (pTag >= pLimit) ) {
		return 0;
	}
	if ( __xgeSvgIsSelfClosingTag(pTag, pTagEnd) ) {
		*ppElementEnd = pTagEnd + 1;
		return 1;
	}
	if ( !__xgeSvgTagNameCopy(pTag, sName, sizeof(sName)) ) {
		return 0;
	}
	{
		const char* pCloseEnd;
		const char* pClose = __xgeSvgFindCloseElement(pTagEnd + 1, sName, &pCloseEnd);

		if ( (pClose == NULL) || (pClose >= pLimit) || (pCloseEnd == NULL) ) {
			return 0;
		}
		*ppElementEnd = pCloseEnd + 1;
		return 1;
	}
}

static int __xgeSvgParseTextChildrenPass(xge_svg pSvg, const char* pTextStart, const char* pTextEnd, const xge_svg_style_t* pTextStyle, float fTextX, float fTextY, float fAnchorAdvance, int iTargetDef, int iMainIdDefIndex, int iExtraMainIdDefIndex, int bUseRunBearingAdvance, int bEmit, float* pTotalAdvance, int* pRunCount)
{
	const char* p;
	float fCursorX;
	float fCursorY;
	int iRet;

	if ( pTotalAdvance != NULL ) {
		*pTotalAdvance = 0.0f;
	}
	if ( pRunCount != NULL ) {
		*pRunCount = 0;
	}
	if ( !__xgeSvgValid(pSvg) || (pTextStart == NULL) || (pTextEnd == NULL) || (pTextEnd < pTextStart) || (pTextStyle == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	p = pTextStart;
	fCursorX = fTextX;
	fCursorY = fTextY;
	while ( p < pTextEnd ) {
		const char* pTag;

		pTag = p;
		while ( (pTag < pTextEnd) && (*pTag != '<') ) {
			pTag++;
		}
		if ( pTag > p ) {
			float fAdvance;

			iRet = __xgeSvgAddDecodedTextRun(pSvg, pTextStyle, p, pTag, fCursorX, fCursorY, fAnchorAdvance, iTargetDef, iMainIdDefIndex, iExtraMainIdDefIndex, bUseRunBearingAdvance, bEmit, &fAdvance);
			if ( iRet != XGE_OK ) {
				return iRet;
			}
			if ( (fAdvance > 0.0f) && (pRunCount != NULL) ) {
				(*pRunCount)++;
			}
			fCursorX += fAdvance;
		}
		if ( pTag >= pTextEnd ) {
			break;
		}
		{
			const char* pTagEnd = __xgeSvgFindTagEnd(pTag);
			int bSelfClosing;

			if ( (pTagEnd == NULL) || (pTagEnd > pTextEnd) ) {
				break;
			}
			bSelfClosing = __xgeSvgIsSelfClosingTag(pTag, pTagEnd);
			if ( __xgeSvgTagNameEquals(pTag, "tspan") ) {
				const char* pTSpanTextStart;
				const char* pTSpanTextEnd;
				const char* pTSpanCloseEnd;
				xge_svg_style_t tSpanStyle;
				float fRunX;
				float fRunY;
				float fDelta;
				float fAdvance;
				int iSpanRunCount;
				int bSeparateSpanRun;

				tSpanStyle = __xgeSvgStyleResolve(pSvg, pTextStyle, pTag, pTagEnd);
				fRunX = fCursorX;
				fRunY = fCursorY;
				fRunX = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "x", XGE_SVG_LENGTH_BASIS_X, tSpanStyle.fFontSize, fRunX);
				fRunY = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "y", XGE_SVG_LENGTH_BASIS_Y, tSpanStyle.fFontSize, fRunY);
				if ( __xgeSvgAttrLengthCopyEx(pTag, pTagEnd, "dx", __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_X), tSpanStyle.fFontSize, &fDelta) ) {
					fRunX += fDelta;
				}
				if ( __xgeSvgAttrLengthCopyEx(pTag, pTagEnd, "dy", __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_Y), tSpanStyle.fFontSize, &fDelta) ) {
					fRunY += fDelta;
				}
				bSeparateSpanRun = (memcmp(&tSpanStyle, pTextStyle, sizeof(tSpanStyle)) != 0) ||
				                   (fabsf(fRunX - fCursorX) > XGE_SVG_EPSILON) || (fabsf(fRunY - fCursorY) > XGE_SVG_EPSILON);
				pTSpanTextStart = pTagEnd + 1;
				pTSpanTextEnd = pTSpanTextStart;
				pTSpanCloseEnd = NULL;
				fAdvance = 0.0f;
				iSpanRunCount = 0;
				if ( !bSelfClosing ) {
					const char* pClose = __xgeSvgFindCloseElement(pTagEnd + 1, "tspan", &pTSpanCloseEnd);
					if ( (pClose != NULL) && (pClose <= pTextEnd) ) {
						pTSpanTextEnd = pClose;
					}
				}
				if ( pTSpanTextEnd > pTSpanTextStart ) {
					iRet = __xgeSvgParseTextChildrenPass(pSvg, pTSpanTextStart, pTSpanTextEnd, &tSpanStyle, fRunX, fRunY, fAnchorAdvance, iTargetDef, iMainIdDefIndex, iExtraMainIdDefIndex, bSeparateSpanRun, bEmit, &fAdvance, &iSpanRunCount);
					if ( iRet != XGE_OK ) {
						return iRet;
					}
					if ( pRunCount != NULL ) {
						*pRunCount += iSpanRunCount;
					}
				}
				fCursorX = fRunX + fAdvance;
				fCursorY = fRunY;
				p = (pTSpanCloseEnd != NULL) ? (pTSpanCloseEnd + 1) : (pTagEnd + 1);
				continue;
			}
			if ( __xgeSvgTagNameEquals(pTag, "textPath") ) {
				const char* pPathTextStart;
				const char* pPathTextEnd;
				const char* pPathCloseEnd;
				float fPathAdvance;
				int iPathRunCount;

				pPathTextStart = pTagEnd + 1;
				pPathTextEnd = pPathTextStart;
				pPathCloseEnd = NULL;
				fPathAdvance = 0.0f;
				iPathRunCount = 0;
				if ( !bSelfClosing ) {
					const char* pClose = __xgeSvgFindCloseElement(pTagEnd + 1, "textPath", &pPathCloseEnd);
					if ( (pClose != NULL) && (pClose <= pTextEnd) ) {
						pPathTextEnd = pClose;
					}
				}
				if ( pPathTextEnd > pPathTextStart ) {
					iRet = __xgeSvgParseTextChildrenPass(pSvg, pPathTextStart, pPathTextEnd, pTextStyle, fCursorX, fCursorY, fAnchorAdvance, iTargetDef, iMainIdDefIndex, iExtraMainIdDefIndex, bUseRunBearingAdvance, bEmit, &fPathAdvance, &iPathRunCount);
					if ( iRet != XGE_OK ) {
						return iRet;
					}
					if ( pRunCount != NULL ) {
						*pRunCount += iPathRunCount;
					}
				}
				fCursorX += fPathAdvance;
				p = (pPathCloseEnd != NULL) ? (pPathCloseEnd + 1) : (pTagEnd + 1);
				continue;
			}
			p = pTagEnd + 1;
		}
	}
	if ( pTotalAdvance != NULL ) {
		*pTotalAdvance = fCursorX - fTextX;
		if ( *pTotalAdvance < 0.0f ) {
			*pTotalAdvance = 0.0f;
		}
	}
	return XGE_OK;
}

static int __xgeSvgParseTextChildren(xge_svg pSvg, const char* pTextStart, const char* pTextEnd, const xge_svg_style_t* pTextStyle, float fTextX, float fTextY, int iTargetDef, int iMainIdDefIndex, int iExtraMainIdDefIndex)
{
	float fTotalAdvance;
	float fAnchorAdvance;
	int iRunCount;
	int iRet;

	iRet = __xgeSvgParseTextChildrenPass(pSvg, pTextStart, pTextEnd, pTextStyle, fTextX, fTextY, 0.0f, iTargetDef, iMainIdDefIndex, iExtraMainIdDefIndex, 1, 0, &fTotalAdvance, &iRunCount);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	fAnchorAdvance = (iRunCount > 1) ? fTotalAdvance : 0.0f;
	return __xgeSvgParseTextChildrenPass(pSvg, pTextStart, pTextEnd, pTextStyle, fTextX, fTextY, fAnchorAdvance, iTargetDef, iMainIdDefIndex, iExtraMainIdDefIndex, 1, 1, NULL, NULL);
}

static int __xgeSvgParseText(xge_svg pSvg, const char* pTag, const char* pTagEnd, const char* pTextStart, const char* pTextEnd, const xge_svg_style_t* pParentStyle, int bInDefs, int iCurrentDef, int iGroupMainIdDefIndex)
{
	xge_svg_style_t tStyle;
	float fX;
	float fY;
	float fDelta;
	int iTargetDef;
	int iOwnMainIdDefIndex;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pTag == NULL) || (pTagEnd == NULL) || (pParentStyle == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (pTextStart == NULL) || (pTextEnd == NULL) || (pTextEnd < pTextStart) ) {
		return XGE_OK;
	}
	iTargetDef = __xgeSvgShapeTargetDef(pSvg, pTag, pTagEnd, bInDefs, iCurrentDef, &iRet);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( iTargetDef == -2 ) {
		return XGE_OK;
	}
	iRet = __xgeSvgMainIdDefIndex(pSvg, pTag, pTagEnd, bInDefs, iCurrentDef, &iOwnMainIdDefIndex);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	tStyle = __xgeSvgStyleResolve(pSvg, pParentStyle, pTag, pTagEnd);
	fX = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "x", XGE_SVG_LENGTH_BASIS_X, tStyle.fFontSize, 0.0f);
	fY = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "y", XGE_SVG_LENGTH_BASIS_Y, tStyle.fFontSize, 0.0f);
	if ( __xgeSvgAttrLengthCopyEx(pTag, pTagEnd, "dx", __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_X), tStyle.fFontSize, &fDelta) ) {
		fX += fDelta;
	}
	if ( __xgeSvgAttrLengthCopyEx(pTag, pTagEnd, "dy", __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_Y), tStyle.fFontSize, &fDelta) ) {
		fY += fDelta;
	}
	return __xgeSvgParseTextChildren(pSvg, pTextStart, pTextEnd, &tStyle, fX, fY, iTargetDef, iOwnMainIdDefIndex, iGroupMainIdDefIndex);
}

static int __xgeSvgBase64Value(char c)
{
	if ( (c >= 'A') && (c <= 'Z') ) return c - 'A';
	if ( (c >= 'a') && (c <= 'z') ) return c - 'a' + 26;
	if ( (c >= '0') && (c <= '9') ) return c - '0' + 52;
	if ( c == '+' ) return 62;
	if ( c == '/' ) return 63;
	return -1;
}

static int __xgeSvgDecodeBase64(const char* sText, unsigned char** ppData, int* pSize)
{
	unsigned char* pData;
	size_t iLen;
	size_t iCapacity;
	int iValue;
	int iBits;
	int iCount;

	if ( (sText == NULL) || (ppData == NULL) || (pSize == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppData = NULL;
	*pSize = 0;
	iLen = strlen(sText);
	iCapacity = (iLen * 3u) / 4u + 4u;
	pData = (unsigned char*)xrtMalloc(iCapacity);
	if ( pData == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iValue = 0;
	iBits = 0;
	iCount = 0;
	while ( *sText != '\0' ) {
		int iDigit;

		if ( __xgeSvgIsSpace((unsigned char)*sText) ) {
			sText++;
			continue;
		}
		if ( *sText == '=' ) {
			break;
		}
		iDigit = __xgeSvgBase64Value(*sText);
		if ( iDigit < 0 ) {
			xrtFree(pData);
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		iValue = (iValue << 6) | iDigit;
		iBits += 6;
		if ( iBits >= 8 ) {
			iBits -= 8;
			pData[iCount++] = (unsigned char)((iValue >> iBits) & 0xFF);
		}
		sText++;
	}
	*ppData = pData;
	*pSize = iCount;
	return XGE_OK;
}

static int __xgeSvgDecodeUriData(const char* sText, char** ppText, int* pSize)
{
	char* sOut;
	int iLen;
	int iRead;
	int iWrite;

	if ( (sText == NULL) || (ppText == NULL) || (pSize == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppText = NULL;
	*pSize = 0;
	iLen = (int)strlen(sText);
	sOut = (char*)xrtMalloc((size_t)iLen + 1u);
	if ( sOut == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iRead = 0;
	iWrite = 0;
	while ( iRead < iLen ) {
		if ( (sText[iRead] == '%') &&
		     ((iRead + 2) < iLen) &&
		     __xgeSvgIsHexChar((unsigned char)sText[iRead + 1]) &&
		     __xgeSvgIsHexChar((unsigned char)sText[iRead + 2]) ) {
			sOut[iWrite++] = (char)((__xgeSvgHexNibble(sText[iRead + 1]) << 4) |
			                        __xgeSvgHexNibble(sText[iRead + 2]));
			iRead += 3;
		} else {
			sOut[iWrite++] = sText[iRead++];
		}
	}
	sOut[iWrite] = '\0';
	*ppText = sOut;
	*pSize = iWrite;
	return XGE_OK;
}

static int __xgeSvgRangeStartsWithTextNoCase(const char* pStart, const char* pEnd, const char* sText)
{
	while ( *sText != '\0' ) {
		if ( pStart >= pEnd ) {
			return 0;
		}
		if ( __xgeSvgAsciiLower((unsigned char)*pStart) != __xgeSvgAsciiLower((unsigned char)*sText) ) {
			return 0;
		}
		pStart++;
		sText++;
	}
	return 1;
}

static int __xgeSvgDataImagePayload(const char* sHref, const char* sMime, int bAllowUtf8, int bAllowBase64, const char** ppPayload, int* pEncoding)
{
	const char* sPrefix = "data:image/";
	const char* pEnd;
	const char* pMimeStart;
	const char* pMimeEnd;
	const char* p;
	size_t iPrefixLen;

	if ( (sHref == NULL) || (sMime == NULL) || (ppPayload == NULL) || (pEncoding == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppPayload = NULL;
	*pEncoding = 0;
	pEnd = sHref + strlen(sHref);
	iPrefixLen = strlen(sPrefix);
	if ( !__xgeSvgRangeStartsWithTextNoCase(sHref, pEnd, sPrefix) ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	pMimeStart = sHref + iPrefixLen;
	pMimeEnd = pMimeStart;
	while ( (pMimeEnd < pEnd) && (*pMimeEnd != ';') && (*pMimeEnd != ',') ) {
		pMimeEnd++;
	}
	if ( !__xgeSvgCssRangeEqualsText(pMimeStart, pMimeEnd, sMime) ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	p = pMimeEnd;
	while ( (p < pEnd) && (*p != ',') ) {
		while ( (p < pEnd) && (*p != ';') ) {
			p++;
		}
		if ( p >= pEnd ) {
			return XGE_ERROR_UNSUPPORTED;
		}
		p++;
		if ( bAllowBase64 && __xgeSvgRangeStartsWithTextNoCase(p, pEnd, "base64,") ) {
			*ppPayload = p + strlen("base64,");
			*pEncoding = XGE_SVG_DATA_IMAGE_BASE64;
			return XGE_OK;
		}
		if ( bAllowUtf8 && __xgeSvgRangeStartsWithTextNoCase(p, pEnd, "utf8,") ) {
			*ppPayload = p + strlen("utf8,");
			*pEncoding = XGE_SVG_DATA_IMAGE_UTF8;
			return XGE_OK;
		}
	}
	if ( (p < pEnd) && (*p == ',') ) {
		if ( !bAllowUtf8 ) {
			return XGE_ERROR_UNSUPPORTED;
		}
		*ppPayload = p + 1;
		*pEncoding = XGE_SVG_DATA_IMAGE_UTF8;
		return XGE_OK;
	}
	return XGE_ERROR_UNSUPPORTED;
}

static int __xgeSvgIsDataImageUri(const char* sHref)
{
	if ( sHref == NULL ) {
		return 0;
	}
	return __xgeSvgRangeStartsWithTextNoCase(sHref, sHref + strlen(sHref), "data:image/");
}

static int __xgeSvgDecodeSvgDataUri(const char* sHref, char** ppSvgText, int* pSize)
{
	const char* pPayload;
	int iEncoding;
	int iRet;

	if ( (sHref == NULL) || (ppSvgText == NULL) || (pSize == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppSvgText = NULL;
	*pSize = 0;
	iEncoding = 0;
	pPayload = NULL;
	iRet = __xgeSvgDataImagePayload(sHref, "svg+xml", 1, 1, &pPayload, &iEncoding);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( iEncoding == XGE_SVG_DATA_IMAGE_BASE64 ) {
		unsigned char* pData;
		int iRet;

		pData = NULL;
		iRet = __xgeSvgDecodeBase64(pPayload, &pData, pSize);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		*ppSvgText = (char*)xrtMalloc((size_t)(*pSize) + 1u);
		if ( *ppSvgText == NULL ) {
			xrtFree(pData);
			*pSize = 0;
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		memcpy(*ppSvgText, pData, (size_t)(*pSize));
		(*ppSvgText)[*pSize] = '\0';
		xrtFree(pData);
		return XGE_OK;
	}
	return __xgeSvgDecodeUriData(pPayload, ppSvgText, pSize);
}

static int __xgeSvgDecodeRasterImageDataUri(const char* sHref, unsigned char** ppData, int* pSize)
{
	const char* pPayload;
	int iEncoding;
	int iRet;

	if ( (sHref == NULL) || (ppData == NULL) || (pSize == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppData = NULL;
	*pSize = 0;
	pPayload = NULL;
	iEncoding = 0;
	iRet = __xgeSvgDataImagePayload(sHref, "png", 0, 1, &pPayload, &iEncoding);
	if ( iRet == XGE_ERROR_UNSUPPORTED ) iRet = __xgeSvgDataImagePayload(sHref, "jpeg", 0, 1, &pPayload, &iEncoding);
	if ( iRet == XGE_ERROR_UNSUPPORTED ) iRet = __xgeSvgDataImagePayload(sHref, "jpg", 0, 1, &pPayload, &iEncoding);
	if ( iRet == XGE_ERROR_UNSUPPORTED ) iRet = __xgeSvgDataImagePayload(sHref, "webp", 0, 1, &pPayload, &iEncoding);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	return __xgeSvgDecodeBase64(pPayload, ppData, pSize);
}

static int __xgeSvgLoadExternalImage(xge_svg pSvg, const char* sHref, xge_svg_raster_image_t** ppRaster)
{
	xge_resource_t tResource;
	char* sResolved;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (sHref == NULL) || (ppRaster == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppRaster = NULL;
	sResolved = __xgeSvgResolveHrefUri(pSvg, sHref);
	if ( sResolved == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( (sResolved[0] == '\0') || (sResolved[0] == '#') ) {
		xrtFree(sResolved);
		return XGE_OK;
	}
	memset(&tResource, 0, sizeof(tResource));
	iRet = xgeResourceLoad(sResolved, &tResource);
	if ( iRet != XGE_OK ) {
		xrtFree(sResolved);
		return XGE_OK;
	}
	iRet = __xgeSvgRasterImageCreateFromMemory(tResource.pData, tResource.iSize, ppRaster);
	xgeResourceFree(&tResource);
	xrtFree(sResolved);
	if ( iRet != XGE_OK ) {
		__xgeSvgRasterImageDestroy(*ppRaster);
		*ppRaster = NULL;
		return XGE_OK;
	}
	return XGE_OK;
}

static int __xgeSvgAddImageItem(xge_svg pSvg, const xge_svg_style_t* pStyle, xge_svg pChildSvg, xge_rect_t tRect)
{
	xge_svg_draw_item_t* pItem;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pStyle == NULL) || !__xgeSvgValid(pChildSvg) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XGE_OK;
	}
	iRet = __xgeSvgDrawItemReserve(pSvg);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pItem = &pSvg->pItems[pSvg->iItemCount];
	memset(pItem, 0, sizeof(*pItem));
	pItem->iType = XGE_SVG_DRAW_ITEM_SVG_IMAGE;
	xgeSvgAddRef(pChildSvg);
	pItem->u.tImage.pSvg = pChildSvg;
	pItem->u.tImage.tStyle = *pStyle;
	pItem->u.tImage.tRect = tRect;
	pSvg->iItemCount++;
	return XGE_OK;
}

static int __xgeSvgMatrixHasOffDiagonal(xge_shape_ex_matrix_t tMatrix)
{
	return (fabsf(tMatrix.fB) > XGE_SVG_EPSILON) || (fabsf(tMatrix.fC) > XGE_SVG_EPSILON);
}

static void __xgeSvgSuppressVectorImageStrokes(xge_svg pSvg);

static void __xgeSvgSuppressSceneStrokes(xge_shape_ex_scene pScene)
{
	int iCount;
	int i;

	if ( (pScene == NULL) || (xgeShapeExSceneGetCount(pScene, &iCount) != XGE_OK) ) {
		return;
	}
	for ( i = 0; i < iCount; i++ ) {
		xge_shape_ex pShape = NULL;

		if ( xgeShapeExSceneGetAt(pScene, i, &pShape) == XGE_OK ) {
			xgeShapeExStrokeWidth(pShape, 0.0f);
		}
	}
}

static void __xgeSvgSuppressDrawItemStrokes(xge_svg_draw_item_t* pItem)
{
	int i;

	if ( pItem == NULL ) {
		return;
	}
	xgeShapeExSceneDestroy(pItem->pRuntimeMaskOwnerScene);
	xgeShapeExSceneDestroy(pItem->pRuntimeClipOwnerScene);
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_SHAPE ) {
		xgeShapeExStrokeWidth(pItem->u.pShape, 0.0f);
	} else if ( pItem->iType == XGE_SVG_DRAW_ITEM_SCENE ) {
		__xgeSvgSuppressSceneStrokes(pItem->u.pScene);
	} else if ( pItem->iType == XGE_SVG_DRAW_ITEM_FILTER ) {
		if ( pItem->u.pFilter != NULL ) {
			for ( i = 0; i < pItem->u.pFilter->iItemCount; i++ ) {
				__xgeSvgSuppressDrawItemStrokes(&pItem->u.pFilter->pItems[i]);
			}
		}
	} else if ( pItem->iType == XGE_SVG_DRAW_ITEM_GROUP ) {
		if ( pItem->u.pGroup != NULL ) {
			for ( i = 0; i < pItem->u.pGroup->iItemCount; i++ ) {
				__xgeSvgSuppressDrawItemStrokes(&pItem->u.pGroup->pItems[i]);
			}
		}
	} else if ( pItem->iType == XGE_SVG_DRAW_ITEM_TEXT ) {
		pItem->u.tText.tStyle.fStrokeWidth = 0.0f;
		pItem->u.tText.tStyle.iStrokeColor = XGE_COLOR_RGBA(0, 0, 0, 0);
	} else if ( pItem->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE ) {
		__xgeSvgSuppressVectorImageStrokes(pItem->u.tImage.pSvg);
	}
}

static void __xgeSvgSuppressVectorImageStrokes(xge_svg pSvg)
{
	int i;
	int j;

	if ( !__xgeSvgValid(pSvg) ) {
		return;
	}
	for ( i = 0; i < pSvg->iItemCount; i++ ) {
		__xgeSvgSuppressDrawItemStrokes(&pSvg->pItems[i]);
	}
	for ( i = 0; i < pSvg->iDefCount; i++ ) {
		xge_svg_def_t* pDef = &pSvg->pDefs[i];

		for ( j = 0; j < pDef->iShapeCount; j++ ) {
			xgeShapeExStrokeWidth(pDef->pShapes[j], 0.0f);
			pDef->pShapeStyles[j].fStrokeWidth = 0.0f;
			pDef->pShapeStyles[j].iStrokeColor = XGE_COLOR_RGBA(0, 0, 0, 0);
		}
		for ( j = 0; j < pDef->iTextCount; j++ ) {
			pDef->pTexts[j].tStyle.fStrokeWidth = 0.0f;
			pDef->pTexts[j].tStyle.iStrokeColor = XGE_COLOR_RGBA(0, 0, 0, 0);
		}
		for ( j = 0; j < pDef->iImageCount; j++ ) {
			__xgeSvgSuppressVectorImageStrokes(pDef->pImages[j].pSvg);
		}
	}
	for ( i = 0; i < pSvg->iMaskCount; i++ ) {
		for ( j = 0; j < pSvg->pMasks[i].iShapeCount; j++ ) {
			xgeShapeExStrokeWidth(pSvg->pMasks[i].pShapes[j].pShape, 0.0f);
		}
	}
}

static int __xgeSvgAddSvgImage(xge_svg pSvg, const xge_svg_style_t* pStyle, xge_svg pChildSvg, xge_rect_t tRect, int iDefIndex)
{
	xge_svg_image_item_t tImage;

	if ( !__xgeSvgValid(pSvg) || (pStyle == NULL) || !__xgeSvgValid(pChildSvg) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XGE_OK;
	}
	if ( iDefIndex >= pSvg->iDefCount ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( __xgeSvgMatrixHasOffDiagonal(pStyle->tTransform) ) {
		__xgeSvgSuppressVectorImageStrokes(pChildSvg);
	}
	memset(&tImage, 0, sizeof(tImage));
	tImage.pSvg = pChildSvg;
	tImage.tStyle = *pStyle;
	tImage.tRect = tRect;
	return __xgeSvgAddImageItemMasked(pSvg, &tImage, iDefIndex);
}

static int __xgeSvgAddRasterImage(xge_svg pSvg, const xge_svg_style_t* pStyle, xge_svg_raster_image_t* pRaster, xge_rect_t tRect, const char* sAspect, int iDefIndex)
{
	xge_svg_raster_item_t tImage;

	if ( !__xgeSvgValid(pSvg) || (pStyle == NULL) || (pRaster == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XGE_OK;
	}
	if ( iDefIndex >= pSvg->iDefCount ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(&tImage, 0, sizeof(tImage));
	tImage.pRaster = pRaster;
	tImage.tStyle = *pStyle;
	tImage.tRect = tRect;
	tImage.iColor = XGE_COLOR_RGBA(255, 255, 255, 255);
	tImage.iAspectAlignX = XGE_SVG_ASPECT_ALIGN_MID;
	tImage.iAspectAlignY = XGE_SVG_ASPECT_ALIGN_MID;
	tImage.iAspectMeetOrSlice = XGE_SVG_ASPECT_MEET;
	if ( (sAspect != NULL) && (sAspect[0] != '\0') ) {
		__xgeSvgParsePreserveAspectRatioFields(sAspect, &tImage.iAspectAlignX, &tImage.iAspectAlignY, &tImage.iAspectMeetOrSlice);
	}
	return __xgeSvgAddRasterItemMasked(pSvg, &tImage, iDefIndex);
}

static int __xgeSvgAddHrefImage(xge_svg pSvg, const xge_svg_style_t* pStyle, const char* sHref, xge_rect_t tRect, const char* sAspect, int iDefIndex)
{
	char* sSvgText;
	unsigned char* pImageData;
	xge_svg pChildSvg;
	xge_svg_raster_image_t* pRaster;
	int iSize;
	int bDataImage;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pStyle == NULL) || (sHref == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (sHref[0] == '\0') || (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XGE_OK;
	}
	if ( __xgeSvgHrefIsLocalRef(sHref) ) {
		char sId[XGE_SVG_ID_MAX];
		int iSourceDef;

		if ( !__xgeSvgHrefIdCopyFromString(sHref, sId, sizeof(sId)) ) {
			return XGE_OK;
		}
		iSourceDef = __xgeSvgDefFind(pSvg, sId);
		if ( iSourceDef < 0 ) {
			return XGE_OK;
		}
		return __xgeSvgApplyUseInstanceWithAspect(pSvg, iSourceDef, pStyle, iDefIndex, tRect.fX, tRect.fY, tRect.fW, tRect.fH, 1, 1, sAspect);
	}
	if ( __xgeSvgHrefHasFragment(sHref) ) {
		return XGE_OK;
	}
	bDataImage = __xgeSvgIsDataImageUri(sHref);
	sSvgText = NULL;
	iSize = 0;
	iRet = __xgeSvgDecodeSvgDataUri(sHref, &sSvgText, &iSize);
	if ( iRet == XGE_ERROR_UNSUPPORTED ) {
		pImageData = NULL;
		pRaster = NULL;
		iSize = 0;
		iRet = __xgeSvgDecodeRasterImageDataUri(sHref, &pImageData, &iSize);
		if ( iRet == XGE_ERROR_UNSUPPORTED ) {
			if ( bDataImage ) {
				return XGE_OK;
			}
			pRaster = NULL;
			iRet = __xgeSvgLoadExternalImage(pSvg, sHref, &pRaster);
			if ( (iRet == XGE_OK) && (pRaster != NULL) ) {
				iRet = __xgeSvgAddRasterImage(pSvg, pStyle, pRaster, tRect, sAspect, iDefIndex);
			}
			__xgeSvgRasterImageDestroy(pRaster);
			return iRet;
		}
		if ( iRet == XGE_OK ) {
			iRet = __xgeSvgRasterImageCreateFromMemory(pImageData, iSize, &pRaster);
		}
		if ( iRet != XGE_OK ) {
			xrtFree(pImageData);
			if ( bDataImage && (iRet != XGE_ERROR_OUT_OF_MEMORY) ) {
				return XGE_OK;
			}
			return iRet;
		}
		iRet = __xgeSvgAddRasterImage(pSvg, pStyle, pRaster, tRect, sAspect, iDefIndex);
		__xgeSvgRasterImageDestroy(pRaster);
		xrtFree(pImageData);
		return iRet;
	}
	if ( iRet != XGE_OK ) {
		if ( bDataImage && (iRet != XGE_ERROR_OUT_OF_MEMORY) ) {
			return XGE_OK;
		}
		return iRet;
	}
	pChildSvg = NULL;
	iRet = xgeSvgCreate(&pChildSvg);
	if ( iRet == XGE_OK ) {
		iRet = __xgeSvgLoadMemoryEx(pChildSvg, sSvgText, iSize, pSvg->sBaseDir);
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeSvgSetPreserveAspectRatio(pChildSvg, "none");
	}
	if ( iRet == XGE_OK ) {
		iRet = __xgeSvgAddSvgImage(pSvg, pStyle, pChildSvg, tRect, iDefIndex);
	}
	xgeSvgDestroy(pChildSvg);
	xrtFree(sSvgText);
	return iRet;
}

static int __xgeSvgParseImage(xge_svg pSvg, const char* pTag, const char* pTagEnd, const xge_svg_style_t* pParentStyle, int bInDefs, int iCurrentDef, int iGroupMainIdDefIndex)
{
	char sHref[XGE_SVG_ATTR_MAX];
	char sAspect[XGE_SVG_ATTR_MAX];
	char* sSvgText;
	unsigned char* pImageData;
	xge_svg pChildSvg;
	xge_svg_raster_image_t* pRaster;
	xge_svg_style_t tStyle;
	xge_rect_t tRect;
	int iSize;
	int iTargetDef;
	int iOwnMainIdDefIndex;
	int bDataImage;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pTag == NULL) || (pTagEnd == NULL) || (pParentStyle == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iTargetDef = __xgeSvgShapeTargetDef(pSvg, pTag, pTagEnd, bInDefs, iCurrentDef, &iRet);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( iTargetDef == -2 ) {
		return XGE_OK;
	}
	iRet = __xgeSvgMainIdDefIndex(pSvg, pTag, pTagEnd, bInDefs, iCurrentDef, &iOwnMainIdDefIndex);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( !__xgeSvgHrefCopyTrimmed(pTag, pTagEnd, sHref, sizeof(sHref)) ) {
		return XGE_OK;
	}
	bDataImage = __xgeSvgIsDataImageUri(sHref);
	sAspect[0] = '\0';
	__xgeSvgAttrCopy(pTag, pTagEnd, "preserveAspectRatio", sAspect, sizeof(sAspect));
	tStyle = __xgeSvgStyleResolve(pSvg, pParentStyle, pTag, pTagEnd);
	tRect.fX = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "x", XGE_SVG_LENGTH_BASIS_X, tStyle.fFontSize, 0.0f);
	tRect.fY = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "y", XGE_SVG_LENGTH_BASIS_Y, tStyle.fFontSize, 0.0f);
	tRect.fW = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "width", XGE_SVG_LENGTH_BASIS_X, tStyle.fFontSize, 0.0f);
	tRect.fH = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "height", XGE_SVG_LENGTH_BASIS_Y, tStyle.fFontSize, 0.0f);
	if ( !__xgeSvgHrefIsLocalRef(sHref) && __xgeSvgHrefHasFragment(sHref) ) {
		return XGE_OK;
	}
	sSvgText = NULL;
	iSize = 0;
	iRet = __xgeSvgDecodeSvgDataUri(sHref, &sSvgText, &iSize);
	if ( iRet == XGE_ERROR_UNSUPPORTED ) {
		pImageData = NULL;
		pRaster = NULL;
		iSize = 0;
		iRet = __xgeSvgDecodeRasterImageDataUri(sHref, &pImageData, &iSize);
		if ( iRet == XGE_ERROR_UNSUPPORTED ) {
			if ( bDataImage ) {
				return XGE_OK;
			}
			pRaster = NULL;
			iRet = __xgeSvgLoadExternalImage(pSvg, sHref, &pRaster);
			if ( (iRet == XGE_OK) && (pRaster != NULL) ) {
				iRet = __xgeSvgAddRasterImage(pSvg, &tStyle, pRaster, tRect, sAspect, iTargetDef);
				if ( (iRet == XGE_OK) && (iTargetDef < 0) && (iOwnMainIdDefIndex >= 0) ) {
					iRet = __xgeSvgAddRasterImage(pSvg, &tStyle, pRaster, tRect, sAspect, iOwnMainIdDefIndex);
				}
				if ( (iRet == XGE_OK) && (iTargetDef < 0) && (iGroupMainIdDefIndex >= 0) && (iGroupMainIdDefIndex != iOwnMainIdDefIndex) ) {
					iRet = __xgeSvgAddRasterImage(pSvg, &tStyle, pRaster, tRect, sAspect, iGroupMainIdDefIndex);
				}
			}
			__xgeSvgRasterImageDestroy(pRaster);
			return iRet;
		}
		if ( iRet == XGE_OK ) {
			iRet = __xgeSvgRasterImageCreateFromMemory(pImageData, iSize, &pRaster);
		}
		if ( iRet != XGE_OK ) {
			xrtFree(pImageData);
			if ( bDataImage && (iRet != XGE_ERROR_OUT_OF_MEMORY) ) {
				return XGE_OK;
			}
			return iRet;
		}
		if ( iRet == XGE_OK ) {
			iRet = __xgeSvgAddRasterImage(pSvg, &tStyle, pRaster, tRect, sAspect, iTargetDef);
		}
		if ( (iRet == XGE_OK) && (iTargetDef < 0) && (iOwnMainIdDefIndex >= 0) ) {
			iRet = __xgeSvgAddRasterImage(pSvg, &tStyle, pRaster, tRect, sAspect, iOwnMainIdDefIndex);
		}
		if ( (iRet == XGE_OK) && (iTargetDef < 0) && (iGroupMainIdDefIndex >= 0) && (iGroupMainIdDefIndex != iOwnMainIdDefIndex) ) {
			iRet = __xgeSvgAddRasterImage(pSvg, &tStyle, pRaster, tRect, sAspect, iGroupMainIdDefIndex);
		}
		__xgeSvgRasterImageDestroy(pRaster);
		xrtFree(pImageData);
		return iRet;
	}
	if ( iRet != XGE_OK ) {
		if ( bDataImage && (iRet != XGE_ERROR_OUT_OF_MEMORY) ) {
			return XGE_OK;
		}
		return iRet;
	}
	pChildSvg = NULL;
	iRet = xgeSvgCreate(&pChildSvg);
	if ( iRet == XGE_OK ) {
		iRet = __xgeSvgLoadMemoryEx(pChildSvg, sSvgText, iSize, pSvg->sBaseDir);
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeSvgSetPreserveAspectRatio(pChildSvg, "none");
	}
	if ( iRet == XGE_OK ) {
		iRet = __xgeSvgAddSvgImage(pSvg, &tStyle, pChildSvg, tRect, iTargetDef);
	}
	if ( (iRet == XGE_OK) && (iTargetDef < 0) && (iOwnMainIdDefIndex >= 0) ) {
		iRet = __xgeSvgAddSvgImage(pSvg, &tStyle, pChildSvg, tRect, iOwnMainIdDefIndex);
	}
	if ( (iRet == XGE_OK) && (iTargetDef < 0) && (iGroupMainIdDefIndex >= 0) && (iGroupMainIdDefIndex != iOwnMainIdDefIndex) ) {
		iRet = __xgeSvgAddSvgImage(pSvg, &tStyle, pChildSvg, tRect, iGroupMainIdDefIndex);
	}
	xgeSvgDestroy(pChildSvg);
	xrtFree(sSvgText);
	return iRet;
}

static int __xgeSvgParseElement(xge_svg pSvg, const char* pTag, const char* pTagEnd, const xge_svg_style_t* pParentStyle, int bInDefs, int iCurrentDef, int iGroupMainIdDefIndex)
{
	char sValue[XGE_SVG_ATTR_MAX];
	xge_svg_style_t tStyle;
	xge_shape_ex pShape;
	int iTargetDef;
	int iOwnMainIdDefIndex;
	int iRet;

	if ( pParentStyle == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iTargetDef = __xgeSvgShapeTargetDef(pSvg, pTag, pTagEnd, bInDefs, iCurrentDef, &iRet);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( iTargetDef == -2 ) {
		return XGE_OK;
	}
	iRet = __xgeSvgMainIdDefIndex(pSvg, pTag, pTagEnd, bInDefs, iCurrentDef, &iOwnMainIdDefIndex);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	tStyle = __xgeSvgStyleResolve(pSvg, pParentStyle, pTag, pTagEnd);
	if ( __xgeSvgTagNameEquals(pTag, "path") ) {
		iRet = __xgeSvgPathShapeCreate(pTag, pTagEnd, &pShape);
		if ( (iRet != XGE_OK) || (pShape == NULL) ) return iRet;
		__xgeSvgApplyShapeStyle(pSvg, pShape, &tStyle);
		return __xgeSvgAddStyledShapeWithLocalMainId(pSvg, pShape, &tStyle, iTargetDef, iOwnMainIdDefIndex, iGroupMainIdDefIndex, pParentStyle->tTransform);
	}
	if ( __xgeSvgTagNameEquals(pTag, "rect") ) {
		float fX = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "x", XGE_SVG_LENGTH_BASIS_X, tStyle.fFontSize, 0.0f);
		float fY = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "y", XGE_SVG_LENGTH_BASIS_Y, tStyle.fFontSize, 0.0f);
		float fW = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "width", XGE_SVG_LENGTH_BASIS_X, tStyle.fFontSize, 0.0f);
		float fH = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "height", XGE_SVG_LENGTH_BASIS_Y, tStyle.fFontSize, 0.0f);
		float fRX = 0.0f;
		float fRY = 0.0f;
		int bHasRX = __xgeSvgAttrLengthCopyEx(pTag, pTagEnd, "rx", __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_X), tStyle.fFontSize, &fRX);
		int bHasRY = __xgeSvgAttrLengthCopyEx(pTag, pTagEnd, "ry", __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_Y), tStyle.fFontSize, &fRY);
		int bInvalidSingleRadius = (bHasRX && !bHasRY && (fRX < 0.0f)) ||
			(!bHasRX && bHasRY && (fRY < 0.0f));

		__xgeSvgNormalizeRectRadii(&fRX, &fRY, &bHasRX, &bHasRY);
		if ( (fW <= 0.0f) || (fH <= 0.0f) ) {
			return XGE_OK;
		}

		iRet = xgeShapeExCreate(&pShape);
		if ( iRet != XGE_OK ) return iRet;
		iRet = __xgeShapeExAppendSvgRect(pShape, fX, fY, fW, fH, fRX, fRY);
		if ( iRet == XGE_OK ) {
			pShape->bAxisAlignedFillAntialias = bInvalidSingleRadius;
			__xgeSvgApplyShapeStyle(pSvg, pShape, &tStyle);
			return __xgeSvgAddStyledShapeWithLocalMainId(pSvg, pShape, &tStyle, iTargetDef, iOwnMainIdDefIndex, iGroupMainIdDefIndex, pParentStyle->tTransform);
		}
		xgeShapeExDestroy(pShape);
		return iRet;
	}
	if ( __xgeSvgTagNameEquals(pTag, "circle") || __xgeSvgTagNameEquals(pTag, "ellipse") ) {
		float fCX = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "cx", XGE_SVG_LENGTH_BASIS_X, tStyle.fFontSize, 0.0f);
		float fCY = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "cy", XGE_SVG_LENGTH_BASIS_Y, tStyle.fFontSize, 0.0f);
		float fRX;
		float fRY;

		__xgeSvgCircleEllipseRadii(pSvg, pTag, pTagEnd, 0, tStyle.fFontSize, &fRX, &fRY);
		if ( (fRX <= 0.0f) || (fRY <= 0.0f) ) {
			return XGE_OK;
		}
		iRet = xgeShapeExCreate(&pShape);
		if ( iRet != XGE_OK ) return iRet;
		iRet = __xgeShapeExAppendSvgEllipse(pShape, fCX, fCY, fRX, fRY);
		if ( iRet == XGE_OK ) {
			__xgeSvgApplyShapeStyle(pSvg, pShape, &tStyle);
			return __xgeSvgAddStyledShapeWithLocalMainId(pSvg, pShape, &tStyle, iTargetDef, iOwnMainIdDefIndex, iGroupMainIdDefIndex, pParentStyle->tTransform);
		}
		xgeShapeExDestroy(pShape);
		return iRet;
	}
	if ( __xgeSvgTagNameEquals(pTag, "line") ) {
		float fX1 = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "x1", XGE_SVG_LENGTH_BASIS_X, tStyle.fFontSize, 0.0f);
		float fY1 = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "y1", XGE_SVG_LENGTH_BASIS_Y, tStyle.fFontSize, 0.0f);
		float fX2 = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "x2", XGE_SVG_LENGTH_BASIS_X, tStyle.fFontSize, 0.0f);
		float fY2 = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "y2", XGE_SVG_LENGTH_BASIS_Y, tStyle.fFontSize, 0.0f);

		iRet = xgeShapeExCreate(&pShape);
		if ( iRet != XGE_OK ) return iRet;
		iRet = xgeShapeExAppendLine(pShape, fX1, fY1, fX2, fY2);
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pShape);
			return iRet;
		}
		__xgeSvgApplyShapeStyle(pSvg, pShape, &tStyle);
		return __xgeSvgAddStyledShapeWithLocalMainId(pSvg, pShape, &tStyle, iTargetDef, iOwnMainIdDefIndex, iGroupMainIdDefIndex, pParentStyle->tTransform);
	}
	if ( __xgeSvgTagNameEquals(pTag, "polygon") || __xgeSvgTagNameEquals(pTag, "polyline") ) {
		int bClose = __xgeSvgTagNameEquals(pTag, "polygon");
		xge_vec2_t* pPoints;
		int iPointCount;

		if ( !__xgeSvgAttrCopy(pTag, pTagEnd, "points", sValue, sizeof(sValue)) ) {
			return XGE_OK;
		}
		pPoints = NULL;
		iPointCount = 0;
		iRet = __xgeSvgParsePointList(sValue, &pPoints, &iPointCount);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		if ( iPointCount <= 0 ) {
			xrtFree(pPoints);
			return XGE_OK;
		}
		iRet = xgeShapeExCreate(&pShape);
		if ( iRet != XGE_OK ) {
			xrtFree(pPoints);
			return iRet;
		}
		iRet = __xgeSvgAppendPointList(pShape, pPoints, iPointCount, bClose);
		if ( iRet == XGE_OK ) {
			__xgeSvgApplyShapeStyle(pSvg, pShape, &tStyle);
			iRet = __xgeSvgAddStyledShapeWithLocalMainId(pSvg, pShape, &tStyle, iTargetDef, iOwnMainIdDefIndex, iGroupMainIdDefIndex, pParentStyle->tTransform);
			pShape = NULL;
		}
		if ( (iRet != XGE_OK) && (pShape != NULL) ) {
			xgeShapeExDestroy(pShape);
		}
		xrtFree(pPoints);
		return iRet;
	}
	return XGE_OK;
}

static void __xgeSvgApplyUsePaintToShape(xge_svg pSvg, xge_shape_ex pShape, const xge_svg_style_t* pSourceStyle, const xge_svg_style_t* pUseStyle, xge_svg_style_t* pAddStyle)
{
	float fEffectiveFillOpacity;
	float fEffectiveStrokeOpacity;
	int bEffectiveFillOpacityNonFinite;
	int bEffectiveStrokeOpacityNonFinite;
	int bInheritFillOpacity;
	int bInheritStrokeOpacity;
	int bSourceFillGradient;
	int bSourceStrokeGradient;
	int bUseFillGradient;
	int bUseStrokeGradient;

	if ( pAddStyle != NULL ) {
		pAddStyle->sFillGradientId[0] = '\0';
		pAddStyle->sStrokeGradientId[0] = '\0';
	}
	if ( !__xgeSvgValid(pSvg) || (pShape == NULL) || (pSourceStyle == NULL) || (pUseStyle == NULL) || (pAddStyle == NULL) ) {
		return;
	}
	__xgeSvgResolveGradients(pSvg);
	bSourceFillGradient = (pSourceStyle->sFillGradientId[0] != '\0') &&
		((__xgeSvgLinearGradientFind(pSvg, pSourceStyle->sFillGradientId) >= 0) ||
		 (__xgeSvgRadialGradientFind(pSvg, pSourceStyle->sFillGradientId) >= 0));
	bSourceStrokeGradient = (pSourceStyle->sStrokeGradientId[0] != '\0') &&
		((__xgeSvgLinearGradientFind(pSvg, pSourceStyle->sStrokeGradientId) >= 0) ||
		 (__xgeSvgRadialGradientFind(pSvg, pSourceStyle->sStrokeGradientId) >= 0));
	bUseFillGradient = (pUseStyle->sFillGradientId[0] != '\0') &&
		((__xgeSvgLinearGradientFind(pSvg, pUseStyle->sFillGradientId) >= 0) ||
		 (__xgeSvgRadialGradientFind(pSvg, pUseStyle->sFillGradientId) >= 0));
	bUseStrokeGradient = (pUseStyle->sStrokeGradientId[0] != '\0') &&
		((__xgeSvgLinearGradientFind(pSvg, pUseStyle->sStrokeGradientId) >= 0) ||
		 (__xgeSvgRadialGradientFind(pSvg, pUseStyle->sStrokeGradientId) >= 0));
	bInheritFillOpacity = !pSourceStyle->bFillOpacitySet && pUseStyle->bFillOpacitySet;
	bInheritStrokeOpacity = !pSourceStyle->bStrokeOpacitySet && pUseStyle->bStrokeOpacitySet;
	fEffectiveFillOpacity = bInheritFillOpacity ? pUseStyle->fFillOpacity : pSourceStyle->fFillOpacity;
	fEffectiveStrokeOpacity = bInheritStrokeOpacity ? pUseStyle->fStrokeOpacity : pSourceStyle->fStrokeOpacity;
	bEffectiveFillOpacityNonFinite = bInheritFillOpacity ? pUseStyle->bFillOpacityNonFinite : pSourceStyle->bFillOpacityNonFinite;
	bEffectiveStrokeOpacityNonFinite = bInheritStrokeOpacity ? pUseStyle->bStrokeOpacityNonFinite : pSourceStyle->bStrokeOpacityNonFinite;
	pAddStyle->fFillOpacity = fEffectiveFillOpacity;
	pAddStyle->fStrokeOpacity = fEffectiveStrokeOpacity;
	pAddStyle->bFillOpacitySet = pSourceStyle->bFillOpacitySet || pUseStyle->bFillOpacitySet;
	pAddStyle->bStrokeOpacitySet = pSourceStyle->bStrokeOpacitySet || pUseStyle->bStrokeOpacitySet;
	pAddStyle->bFillOpacityNonFinite = bEffectiveFillOpacityNonFinite;
	pAddStyle->bStrokeOpacityNonFinite = bEffectiveStrokeOpacityNonFinite;
	if ( pSourceStyle->bStrokeWidthSet ) {
		pAddStyle->fStrokeWidth = pSourceStyle->fStrokeWidth;
		pAddStyle->bStrokeWidthSet = 1;
	} else if ( pUseStyle->bStrokeWidthSet ) {
		xgeShapeExStrokeWidth(pShape, pUseStyle->fStrokeWidth);
		pAddStyle->fStrokeWidth = pUseStyle->fStrokeWidth;
		pAddStyle->bStrokeWidthSet = 1;
	} else {
		pAddStyle->fStrokeWidth = pSourceStyle->fStrokeWidth;
		pAddStyle->bStrokeWidthSet = 0;
	}
	if ( (pSourceStyle->sFillGradientId[0] != '\0') &&
	     (__xgeSvgPatternFind(pSvg, pSourceStyle->sFillGradientId) < 0) &&
	     (__xgeSvgLinearGradientFind(pSvg, pSourceStyle->sFillGradientId) < 0) &&
	     (__xgeSvgRadialGradientFind(pSvg, pSourceStyle->sFillGradientId) < 0) ) {
		strcpy(pAddStyle->sFillGradientId, pSourceStyle->sFillGradientId);
	}
	if ( (pSourceStyle->sStrokeGradientId[0] != '\0') &&
	     (__xgeSvgPatternFind(pSvg, pSourceStyle->sStrokeGradientId) < 0) &&
	     (__xgeSvgLinearGradientFind(pSvg, pSourceStyle->sStrokeGradientId) < 0) &&
	     (__xgeSvgRadialGradientFind(pSvg, pSourceStyle->sStrokeGradientId) < 0) ) {
		strcpy(pAddStyle->sStrokeGradientId, pSourceStyle->sStrokeGradientId);
	}
	if ( !pSourceStyle->bFillSet && pUseStyle->bFillSet ) {
		float fPaintOpacity = __xgeSvgPaintOpacityThorvg(
			pUseStyle->fFillOpacity, pUseStyle->bFillOpacityNonFinite, bUseFillGradient
		);
		if ( pUseStyle->sFillGradientId[0] == '\0' ) {
			xgeShapeExFillColor(pShape, __xgeSvgColorAlpha(pUseStyle->iFillColor, fPaintOpacity));
		} else if ( __xgeSvgPatternFind(pSvg, pUseStyle->sFillGradientId) >= 0 ) {
			xgeShapeExFillColor(pShape, XGE_COLOR_RGBA(0, 0, 0, 0));
		} else if ( !__xgeSvgApplyFillGradientPaintToShape(pSvg, pShape, pUseStyle, fPaintOpacity) ) {
			xgeShapeExFillColor(pShape, __xgeSvgColorAlpha(pUseStyle->iFillColor, fPaintOpacity));
		}
	} else if ( pSourceStyle->bFillCurrentColor && !pSourceStyle->bColorSet && pUseStyle->bColorSet ) {
		float fPaintOpacity = pSourceStyle->fOpacity * __xgeSvgPaintOpacityThorvg(
			fEffectiveFillOpacity, bEffectiveFillOpacityNonFinite, 0
		);
		xgeShapeExFillColor(pShape, __xgeSvgColorAlpha(pUseStyle->iCurrentColor, fPaintOpacity));
	}
	if ( pSourceStyle->bFillSet && bInheritFillOpacity ) {
		float fPaintOpacity = pSourceStyle->fOpacity * __xgeSvgPaintOpacityThorvg(
			fEffectiveFillOpacity, bEffectiveFillOpacityNonFinite, bSourceFillGradient
		);
		if ( bSourceFillGradient ) {
			(void)__xgeSvgApplyFillGradientPaintToShape(pSvg, pShape, pSourceStyle, fPaintOpacity);
		} else if ( __xgeSvgPatternFind(pSvg, pSourceStyle->sFillGradientId) >= 0 ) {
			xgeShapeExFillColor(pShape, XGE_COLOR_RGBA(0, 0, 0, 0));
		} else {
			uint32_t iFillColor = (pSourceStyle->bFillCurrentColor && !pSourceStyle->bColorSet && pUseStyle->bColorSet) ?
				pUseStyle->iCurrentColor : pSourceStyle->iFillColor;
			xgeShapeExFillColor(pShape, __xgeSvgColorAlpha(iFillColor, fPaintOpacity));
		}
	}
	if ( !pSourceStyle->bStrokeSet && pUseStyle->bStrokeSet ) {
		float fPaintOpacity = __xgeSvgPaintOpacityThorvg(
			pUseStyle->fStrokeOpacity, pUseStyle->bStrokeOpacityNonFinite, bUseStrokeGradient
		);
		xgeShapeExStrokeWidth(pShape, pUseStyle->fStrokeWidth);
		if ( pUseStyle->sStrokeGradientId[0] == '\0' ) {
			xgeShapeExStrokeColor(pShape, __xgeSvgColorAlpha(pUseStyle->iStrokeColor, fPaintOpacity));
		} else if ( __xgeSvgPatternFind(pSvg, pUseStyle->sStrokeGradientId) >= 0 ) {
			xgeShapeExStrokeColor(pShape, XGE_COLOR_RGBA(0, 0, 0, 0));
		} else if ( !__xgeSvgApplyStrokeGradientPaintToShape(pSvg, pShape, pUseStyle, fPaintOpacity) ) {
			xgeShapeExStrokeColor(pShape, __xgeSvgColorAlpha(pUseStyle->iStrokeColor, fPaintOpacity));
		}
	} else if ( pSourceStyle->bStrokeCurrentColor && !pSourceStyle->bColorSet && pUseStyle->bColorSet ) {
		float fPaintOpacity = pSourceStyle->fOpacity * __xgeSvgPaintOpacityThorvg(
			fEffectiveStrokeOpacity, bEffectiveStrokeOpacityNonFinite, 0
		);
		xgeShapeExStrokeColor(pShape, __xgeSvgColorAlpha(pUseStyle->iCurrentColor, fPaintOpacity));
	}
	if ( pSourceStyle->bStrokeSet && bInheritStrokeOpacity ) {
		float fPaintOpacity = pSourceStyle->fOpacity * __xgeSvgPaintOpacityThorvg(
			fEffectiveStrokeOpacity, bEffectiveStrokeOpacityNonFinite, bSourceStrokeGradient
		);
		if ( bSourceStrokeGradient ) {
			(void)__xgeSvgApplyStrokeGradientPaintToShape(pSvg, pShape, pSourceStyle, fPaintOpacity);
		} else if ( __xgeSvgPatternFind(pSvg, pSourceStyle->sStrokeGradientId) >= 0 ) {
			xgeShapeExStrokeColor(pShape, XGE_COLOR_RGBA(0, 0, 0, 0));
		} else {
			uint32_t iStrokeColor = (pSourceStyle->bStrokeCurrentColor && !pSourceStyle->bColorSet && pUseStyle->bColorSet) ?
				pUseStyle->iCurrentColor : pSourceStyle->iStrokeColor;
			xgeShapeExStrokeColor(pShape, __xgeSvgColorAlpha(iStrokeColor, fPaintOpacity));
		}
	}
	if ( pSourceStyle->bBlendSet ) {
		pAddStyle->iBlend = pSourceStyle->iBlend;
		pAddStyle->bBlendSet = 1;
	}
}

static void __xgeSvgApplyUsePaintToStyle(xge_svg_style_t* pTargetStyle, const xge_svg_style_t* pUseStyle)
{
	if ( (pTargetStyle == NULL) || (pUseStyle == NULL) ) {
		return;
	}
	if ( !pTargetStyle->bFillSet && pUseStyle->bFillSet && (pUseStyle->sFillGradientId[0] == '\0') ) {
		pTargetStyle->iFillColor = pUseStyle->iFillColor;
		pTargetStyle->sFillGradientId[0] = '\0';
		pTargetStyle->bFillSet = 1;
		pTargetStyle->bFillCurrentColor = pUseStyle->bFillCurrentColor;
	} else if ( pTargetStyle->bFillCurrentColor && !pTargetStyle->bColorSet && pUseStyle->bColorSet ) {
		pTargetStyle->iFillColor = pUseStyle->iCurrentColor;
	}
	if ( !pTargetStyle->bStrokeSet && pUseStyle->bStrokeSet && (pUseStyle->sStrokeGradientId[0] == '\0') ) {
		pTargetStyle->iStrokeColor = pUseStyle->iStrokeColor;
		pTargetStyle->sStrokeGradientId[0] = '\0';
		pTargetStyle->bStrokeSet = 1;
		pTargetStyle->bStrokeCurrentColor = pUseStyle->bStrokeCurrentColor;
	} else if ( pTargetStyle->bStrokeCurrentColor && !pTargetStyle->bColorSet && pUseStyle->bColorSet ) {
		pTargetStyle->iStrokeColor = pUseStyle->iCurrentColor;
	}
	if ( !pTargetStyle->bFillOpacitySet && pUseStyle->bFillOpacitySet ) {
		pTargetStyle->fFillOpacity = pUseStyle->fFillOpacity;
		pTargetStyle->bFillOpacitySet = 1;
		pTargetStyle->bFillOpacityNonFinite = pUseStyle->bFillOpacityNonFinite;
	}
	if ( !pTargetStyle->bStrokeOpacitySet && pUseStyle->bStrokeOpacitySet ) {
		pTargetStyle->fStrokeOpacity = pUseStyle->fStrokeOpacity;
		pTargetStyle->bStrokeOpacitySet = 1;
		pTargetStyle->bStrokeOpacityNonFinite = pUseStyle->bStrokeOpacityNonFinite;
	}
	if ( !pTargetStyle->bStrokeWidthSet && pUseStyle->bStrokeWidthSet ) {
		pTargetStyle->fStrokeWidth = pUseStyle->fStrokeWidth;
		pTargetStyle->bStrokeWidthSet = 1;
	}
	if ( !pTargetStyle->bBlendSet && pUseStyle->bBlendSet ) {
		pTargetStyle->iBlend = pUseStyle->iBlend;
		pTargetStyle->bBlendSet = 1;
	}
}

static int __xgeSvgShapeMaskTransformCompose(xge_shape_ex pShape, xge_shape_ex_matrix_t tParent)
{
	xge_shape_ex pMaskShape;
	xge_shape_ex_scene pMaskScene;
	xge_shape_ex_matrix_t tMaskTransform;
	int iMaskMethod;
	int iMaskTargetType;
	int iRet;

	if ( pShape == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pMaskShape = NULL;
	pMaskScene = NULL;
	iMaskMethod = XGE_SHAPE_EX_MASK_NONE;
	iMaskTargetType = XGE_SHAPE_EX_MASK_TARGET_NONE;
	iRet = xgeShapeExMaskGet(
		pShape, &iMaskMethod, &iMaskTargetType, &pMaskShape, &pMaskScene
	);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( iMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_SHAPE ) {
		iRet = xgeShapeExTransformGet(pMaskShape, &tMaskTransform);
		if ( iRet == XGE_OK ) {
			tMaskTransform = __xgeSvgMatrixMul(tParent, tMaskTransform);
			iRet = xgeShapeExTransformSet(pMaskShape, &tMaskTransform);
		}
	} else if ( iMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_SCENE ) {
		iRet = xgeShapeExSceneTransformGet(pMaskScene, &tMaskTransform);
		if ( iRet == XGE_OK ) {
			tMaskTransform = __xgeSvgMatrixMul(tParent, tMaskTransform);
			iRet = xgeShapeExSceneTransformSet(pMaskScene, &tMaskTransform);
		}
	}
	return iRet;
}

static int __xgeSvgApplyUseDefItems(
	xge_svg pSvg,
	const xge_svg_def_t* pSourceDef,
	const xge_svg_def_item_t* pItems,
	int iItemCount,
	const xge_svg_style_t* pStyle,
	const xge_svg_style_t* pChildUseStyle,
	int iTargetDef,
	int bUseClip,
	xge_rect_t tUseClip
)
{
	int i;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pSourceDef == NULL) || (pStyle == NULL) ||
	     (pChildUseStyle == NULL) || ((iItemCount > 0) && (pItems == NULL)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < iItemCount; i++ ) {
		const xge_svg_def_item_t* pItem = &pItems[i];

		if ( pItem->iType == XGE_SVG_DRAW_ITEM_SCENE ) {
			xge_shape_ex pClone;
			xge_shape_ex_matrix_t tShapeTransform;
			const xge_svg_style_t* pSourceStyle;
			xge_svg_style_t tItemStyle;
			int iIndex = pItem->u.iIndex;

			if ( (iIndex < 0) || (iIndex >= pSourceDef->iShapeCount) ) {
				return XGE_ERROR_INVALID_ARGUMENT;
			}
			iRet = xgeShapeExClone(pSourceDef->pShapes[iIndex], &pClone);
			if ( iRet != XGE_OK ) return iRet;
			pSourceStyle = (pSourceDef->pShapeStyles != NULL) ? &pSourceDef->pShapeStyles[iIndex] : NULL;
			tItemStyle = *pStyle;
			__xgeSvgApplyUsePaintToShape(pSvg, pClone, pSourceStyle, pChildUseStyle, &tItemStyle);
			if ( (pSourceStyle != NULL) && (pSourceStyle->sFilterId[0] != '\0') ) {
				strcpy(tItemStyle.sFilterId, pSourceStyle->sFilterId);
			}
			tShapeTransform = __xgeSvgMatrixMul(tItemStyle.tTransform, pClone->tTransform);
			if ( bUseClip ) {
				xge_rect_t tShapeClip;

				if ( !__xgeSvgClipRectMapToItem(tShapeTransform, tUseClip, pClone->bClipRect, pClone->tClipRect, &tShapeClip) ) {
					xgeShapeExDestroy(pClone);
					continue;
				}
				xgeShapeExClipRectSet(pClone, tShapeClip);
			}
			iRet = __xgeSvgShapeMaskTransformCompose(pClone, tItemStyle.tTransform);
			if ( iRet != XGE_OK ) {
				xgeShapeExDestroy(pClone);
				return iRet;
			}
			xgeShapeExTransformSet(pClone, &tShapeTransform);
			xgeShapeExOpacity(pClone, pClone->fOpacity * tItemStyle.fOpacity);
			if ( !tItemStyle.bVisible || !tItemStyle.bVisibility ) xgeShapeExVisible(pClone, 0);
			iRet = __xgeSvgAddStyledShape(pSvg, pClone, &tItemStyle, iTargetDef);
			if ( iRet != XGE_OK ) return iRet;
		} else if ( pItem->iType == XGE_SVG_DRAW_ITEM_TEXT ) {
			xge_svg_text_item_t tText;
			int iIndex = pItem->u.iIndex;

			if ( (iIndex < 0) || (iIndex >= pSourceDef->iTextCount) ) return XGE_ERROR_INVALID_ARGUMENT;
			tText = pSourceDef->pTexts[iIndex];
			__xgeSvgApplyUsePaintToStyle(&tText.tStyle, pChildUseStyle);
			tText.tStyle.tTransform = __xgeSvgMatrixMul(pStyle->tTransform, tText.tStyle.tTransform);
			tText.tStyle.fOpacity *= pStyle->fOpacity;
			if ( !pStyle->bVisible || !pStyle->bVisibility ) tText.tStyle.bVisible = 0;
			if ( bUseClip ) {
				xge_rect_t tTextClip;

				if ( !__xgeSvgClipRectMapToItem(tText.tStyle.tTransform, tUseClip, tText.bClipRect, tText.tClipRect, &tTextClip) ) continue;
				tText.bClipRect = 1;
				tText.tClipRect = tTextClip;
			}
			iRet = __xgeSvgAddTextItemMasked(pSvg, &tText, iTargetDef);
			if ( iRet != XGE_OK ) return iRet;
		} else if ( pItem->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE ) {
			xge_svg_image_item_t tImage;
			int iIndex = pItem->u.iIndex;

			if ( (iIndex < 0) || (iIndex >= pSourceDef->iImageCount) ) return XGE_ERROR_INVALID_ARGUMENT;
			tImage = pSourceDef->pImages[iIndex];
			tImage.tStyle.tTransform = __xgeSvgMatrixMul(pStyle->tTransform, tImage.tStyle.tTransform);
			tImage.tStyle.fOpacity *= pStyle->fOpacity;
			if ( !pStyle->bVisible || !pStyle->bVisibility ) tImage.tStyle.bVisible = 0;
			if ( bUseClip ) {
				xge_rect_t tImageClip;

				if ( !__xgeSvgClipRectMapToItem(tImage.tStyle.tTransform, tUseClip, tImage.bClipRect, tImage.tClipRect, &tImageClip) ) continue;
				tImage.bClipRect = 1;
				tImage.tClipRect = tImageClip;
			}
			iRet = __xgeSvgAddImageItemMasked(pSvg, &tImage, iTargetDef);
			if ( iRet != XGE_OK ) return iRet;
		} else if ( pItem->iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE ) {
			xge_svg_raster_item_t tImage;
			int iIndex = pItem->u.iIndex;

			if ( (iIndex < 0) || (iIndex >= pSourceDef->iRasterCount) ) return XGE_ERROR_INVALID_ARGUMENT;
			tImage = pSourceDef->pRasters[iIndex];
			tImage.tStyle.tTransform = __xgeSvgMatrixMul(pStyle->tTransform, tImage.tStyle.tTransform);
			tImage.tStyle.fOpacity *= pStyle->fOpacity;
			if ( !pStyle->bVisible || !pStyle->bVisibility ) tImage.tStyle.bVisible = 0;
			if ( bUseClip ) {
				xge_rect_t tImageClip;

				if ( !__xgeSvgClipRectMapToItem(tImage.tStyle.tTransform, tUseClip, tImage.bClipRect, tImage.tClipRect, &tImageClip) ) continue;
				tImage.bClipRect = 1;
				tImage.tClipRect = tImageClip;
			}
			iRet = __xgeSvgAddRasterItemMasked(pSvg, &tImage, iTargetDef);
			if ( iRet != XGE_OK ) return iRet;
		} else if ( pItem->iType == XGE_SVG_DRAW_ITEM_GROUP ) {
			const xge_svg_def_group_item_t* pGroup = pItem->u.pGroup;
			xge_shape_ex_matrix_t tFilterTransform;
			int iFirstItem;
			int bHasGroupFilter;
			int iTargetShapeStart;
			int iTargetTextStart;
			int iTargetImageStart;
			int iTargetRasterStart;

			if ( pGroup == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
			iFirstItem = (iTargetDef < 0) ? pSvg->iItemCount : pSvg->pDefs[iTargetDef].iItemCount;
			iTargetShapeStart = (iTargetDef >= 0) ? pSvg->pDefs[iTargetDef].iShapeCount : 0;
			iTargetTextStart = (iTargetDef >= 0) ? pSvg->pDefs[iTargetDef].iTextCount : 0;
			iTargetImageStart = (iTargetDef >= 0) ? pSvg->pDefs[iTargetDef].iImageCount : 0;
			iTargetRasterStart = (iTargetDef >= 0) ? pSvg->pDefs[iTargetDef].iRasterCount : 0;
			iRet = __xgeSvgApplyUseDefItems(
				pSvg, pSourceDef, pGroup->pItems, pGroup->iItemCount,
				pStyle, pChildUseStyle, iTargetDef, bUseClip, tUseClip
			);
			if ( iRet != XGE_OK ) return iRet;
			tFilterTransform = __xgeSvgMatrixMul(pStyle->tTransform, pGroup->tFilterTransform);
			if ( (iTargetDef >= 0) && pSvg->pDefs[iTargetDef].bPatternContent ) {
				xge_svg_def_t* pTargetDef = &pSvg->pDefs[iTargetDef];
				int j;

				for ( j = iTargetShapeStart; j < pTargetDef->iShapeCount; j++ ) {
					xgeShapeExOpacity(pTargetDef->pShapes[j], pTargetDef->pShapes[j]->fOpacity * pGroup->fOpacity);
				}
				for ( j = iTargetTextStart; j < pTargetDef->iTextCount; j++ ) {
					pTargetDef->pTexts[j].tStyle.fOpacity *= pGroup->fOpacity;
				}
				for ( j = iTargetImageStart; j < pTargetDef->iImageCount; j++ ) {
					pTargetDef->pImages[j].tStyle.fOpacity *= pGroup->fOpacity;
				}
				for ( j = iTargetRasterStart; j < pTargetDef->iRasterCount; j++ ) {
					pTargetDef->pRasters[j].tStyle.fOpacity *= pGroup->fOpacity;
				}
			} else if ( iTargetDef >= 0 ) {
					iRet = __xgeSvgDefItemsWrapGroup(
						&pSvg->pDefs[iTargetDef], iFirstItem,
						pGroup->bBlendSet, pGroup->iBlend, pGroup->fOpacity,
						pGroup->sMaskId,
						pGroup->sFilterId, tFilterTransform, 1
				);
				if ( iRet != XGE_OK ) return iRet;
			} else {
				bHasGroupFilter = 0;
				if ( pGroup->sFilterId[0] != '\0' ) {
					int iGroupFilter = __xgeSvgFilterFind(pSvg, pGroup->sFilterId);

					if ( (iGroupFilter >= 0) && (pSvg->pFilters[iGroupFilter].iPrimitiveCount > 0) ) {
						iRet = __xgeSvgWrapGroupFilter(pSvg, iFirstItem, iGroupFilter, tFilterTransform);
						if ( iRet != XGE_OK ) return iRet;
						bHasGroupFilter = 1;
					}
				}
				if ( !bHasGroupFilter || pGroup->bBlendSet || (pGroup->sMaskId[0] != '\0') ) {
					iRet = __xgeSvgDrawItemsWrapGroup(
						pSvg, iFirstItem, pGroup->bBlendSet, pGroup->iBlend,
						bHasGroupFilter ? 1.0f : pGroup->fOpacity,
						pGroup->sMaskId, 0, 0, pGroup->fOpacity, NULL
					);
					if ( iRet != XGE_OK ) return iRet;
				}
			}
		} else {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
	}
	return XGE_OK;
}

static int __xgeSvgApplyUseInstance(xge_svg pSvg, int iDefIndex, const xge_svg_style_t* pUseStyle, int iTargetDef, float fX, float fY, float fW, float fH, int bHasWidth, int bHasHeight, xge_shape_ex_matrix_t* pLogicalTransform)
{
	xge_svg_style_t tStyle;
	xge_svg_style_t tChildUseStyle;
	xge_shape_ex_matrix_t tUseTransform;
	xge_rect_t tUseClip;
	char sUseFilterId[XGE_SVG_ID_MAX];
	int iFirstItem;
	int iSourceShapeCount;
	int iUseBlend;
	int bUseBlendSet;
	float fUseOpacity;
	int bUseClip;
	int i;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pUseStyle == NULL) || (iDefIndex < 0) || (iDefIndex >= pSvg->iDefCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tStyle = *pUseStyle;
	iUseBlend = tStyle.iBlend;
	bUseBlendSet = tStyle.bBlendSet;
	fUseOpacity = tStyle.fOpacity;
	if ( fUseOpacity < 0.0f ) fUseOpacity = 0.0f;
	if ( fUseOpacity > 1.0f ) fUseOpacity = 1.0f;
	tStyle.iBlend = XGE_BLEND_ALPHA;
	tStyle.bBlendSet = 0;
	tStyle.fOpacity = 1.0f;
	tChildUseStyle = tStyle;
	strcpy(sUseFilterId, tStyle.sFilterId);
	tStyle.sFilterId[0] = '\0';
	iFirstItem = (iTargetDef < 0) ? pSvg->iItemCount : -1;
	bUseClip = 0;
	memset(&tUseClip, 0, sizeof(tUseClip));
	if ( pSvg->pDefs[iDefIndex].bHasViewBox ) {
		xge_rect_t tDst;
		xge_rect_t tViewport;

		if ( !bHasWidth ) {
			fW = pSvg->pDefs[iDefIndex].bHasWidth ? pSvg->pDefs[iDefIndex].fWidth : pSvg->pDefs[iDefIndex].tViewBox.fW;
		}
		if ( !bHasHeight ) {
			fH = pSvg->pDefs[iDefIndex].bHasHeight ? pSvg->pDefs[iDefIndex].fHeight : pSvg->pDefs[iDefIndex].tViewBox.fH;
		}
		if ( (fW <= 0.0f) || (fH <= 0.0f) ) {
			return XGE_OK;
		}
		tDst.fX = fX;
		tDst.fY = fY;
		tDst.fW = fW;
		tDst.fH = fH;
		tViewport = __xgeSvgAspectViewport(pSvg->pDefs[iDefIndex].tViewBox, tDst,
			pSvg->pDefs[iDefIndex].iAspectAlignX,
			pSvg->pDefs[iDefIndex].iAspectAlignY,
			pSvg->pDefs[iDefIndex].iAspectMeetOrSlice);
		if ( !pSvg->pDefs[iDefIndex].bOverflowVisible ) {
			tUseClip = __xgeSvgMatrixRectBounds(pUseStyle->tTransform, tDst);
			bUseClip = (tUseClip.fW > 0.0f) && (tUseClip.fH > 0.0f);
		}
		tUseTransform = __xgeSvgViewBoxMatrix(pSvg->pDefs[iDefIndex].tViewBox, tViewport);
		tStyle.tTransform = __xgeSvgMatrixMul(tStyle.tTransform, tUseTransform);
	} else if ( (fabsf(fX) > 0.0f) || (fabsf(fY) > 0.0f) ) {
		tUseTransform = __xgeSvgMatrixTranslate(fX, fY);
		tStyle.tTransform = __xgeSvgMatrixMul(tStyle.tTransform, tUseTransform);
	}
	if ( pLogicalTransform != NULL ) *pLogicalTransform = tStyle.tTransform;
	if ( pSvg->pDefs[iDefIndex].iItemCount > 0 ) {
		iRet = __xgeSvgApplyUseDefItems(
			pSvg, &pSvg->pDefs[iDefIndex],
			pSvg->pDefs[iDefIndex].pItems, pSvg->pDefs[iDefIndex].iItemCount,
			&tStyle, &tChildUseStyle, iTargetDef, bUseClip, tUseClip
		);
		if ( iRet != XGE_OK ) return iRet;
	} else {
	iSourceShapeCount = pSvg->pDefs[iDefIndex].iShapeCount;
	for ( i = 0; i < iSourceShapeCount; i++ ) {
		xge_shape_ex pClone;
		xge_shape_ex_matrix_t tShapeTransform;
		const xge_svg_style_t* pSourceStyle;
		xge_svg_style_t tItemStyle;

		iRet = xgeShapeExClone(pSvg->pDefs[iDefIndex].pShapes[i], &pClone);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		pSourceStyle = (pSvg->pDefs[iDefIndex].pShapeStyles != NULL) ? &pSvg->pDefs[iDefIndex].pShapeStyles[i] : NULL;
		tItemStyle = tStyle;
		__xgeSvgApplyUsePaintToShape(pSvg, pClone, pSourceStyle, &tChildUseStyle, &tItemStyle);
		if ( (pSourceStyle != NULL) && (pSourceStyle->sFilterId[0] != '\0') ) {
			strcpy(tItemStyle.sFilterId, pSourceStyle->sFilterId);
		}
		tShapeTransform = __xgeSvgMatrixMul(tItemStyle.tTransform, pClone->tTransform);
		if ( bUseClip ) {
			xge_rect_t tShapeClip;

			if ( !__xgeSvgClipRectMapToItem(tShapeTransform, tUseClip, pClone->bClipRect, pClone->tClipRect, &tShapeClip) ) {
				xgeShapeExDestroy(pClone);
				continue;
			}
			xgeShapeExClipRectSet(pClone, tShapeClip);
		}
		iRet = __xgeSvgShapeMaskTransformCompose(pClone, tItemStyle.tTransform);
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pClone);
			return iRet;
		}
		xgeShapeExTransformSet(pClone, &tShapeTransform);
		xgeShapeExOpacity(pClone, pClone->fOpacity * tItemStyle.fOpacity);
		if ( !tItemStyle.bVisible || !tItemStyle.bVisibility ) {
			xgeShapeExVisible(pClone, 0);
		}
		iRet = __xgeSvgAddStyledShape(pSvg, pClone, &tItemStyle, iTargetDef);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	for ( i = 0; i < pSvg->pDefs[iDefIndex].iTextCount; i++ ) {
		xge_svg_text_item_t tText;

		tText = pSvg->pDefs[iDefIndex].pTexts[i];
		__xgeSvgApplyUsePaintToStyle(&tText.tStyle, &tChildUseStyle);
		tText.tStyle.tTransform = __xgeSvgMatrixMul(tStyle.tTransform, tText.tStyle.tTransform);
		tText.tStyle.fOpacity *= tStyle.fOpacity;
		if ( !tStyle.bVisible || !tStyle.bVisibility ) {
			tText.tStyle.bVisible = 0;
		}
		if ( bUseClip ) {
			xge_rect_t tTextClip;

			if ( !__xgeSvgClipRectMapToItem(tText.tStyle.tTransform, tUseClip, tText.bClipRect, tText.tClipRect, &tTextClip) ) {
				continue;
			}
			tText.bClipRect = 1;
			tText.tClipRect = tTextClip;
		}
		iRet = __xgeSvgAddTextItemMasked(pSvg, &tText, iTargetDef);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	for ( i = 0; i < pSvg->pDefs[iDefIndex].iImageCount; i++ ) {
		xge_svg_image_item_t tImage;

		tImage = pSvg->pDefs[iDefIndex].pImages[i];
		tImage.tStyle.tTransform = __xgeSvgMatrixMul(tStyle.tTransform, tImage.tStyle.tTransform);
		tImage.tStyle.fOpacity *= tStyle.fOpacity;
		if ( !tStyle.bVisible || !tStyle.bVisibility ) {
			tImage.tStyle.bVisible = 0;
		}
		if ( bUseClip ) {
			xge_rect_t tImageClip;

			if ( !__xgeSvgClipRectMapToItem(tImage.tStyle.tTransform, tUseClip, tImage.bClipRect, tImage.tClipRect, &tImageClip) ) {
				continue;
			}
			tImage.bClipRect = 1;
			tImage.tClipRect = tImageClip;
		}
		iRet = __xgeSvgAddImageItemMasked(pSvg, &tImage, iTargetDef);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	for ( i = 0; i < pSvg->pDefs[iDefIndex].iRasterCount; i++ ) {
		xge_svg_raster_item_t tImage;

		tImage = pSvg->pDefs[iDefIndex].pRasters[i];
		tImage.tStyle.tTransform = __xgeSvgMatrixMul(tStyle.tTransform, tImage.tStyle.tTransform);
		tImage.tStyle.fOpacity *= tStyle.fOpacity;
		if ( !tStyle.bVisible || !tStyle.bVisibility ) {
			tImage.tStyle.bVisible = 0;
		}
		if ( bUseClip ) {
			xge_rect_t tImageClip;

			if ( !__xgeSvgClipRectMapToItem(tImage.tStyle.tTransform, tUseClip, tImage.bClipRect, tImage.tClipRect, &tImageClip) ) {
				continue;
			}
			tImage.bClipRect = 1;
			tImage.tClipRect = tImageClip;
		}
		iRet = __xgeSvgAddRasterItemMasked(pSvg, &tImage, iTargetDef);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	}
	if ( iTargetDef < 0 ) {
		xge_svg_def_t* pDef = &pSvg->pDefs[iDefIndex];

		if ( (pDef->iItemCount <= 0) && (pDef->sGroupFilterId[0] != '\0') ) {
			int iGroupFilter = __xgeSvgFilterFind(pSvg, pDef->sGroupFilterId);

			if ( iGroupFilter >= 0 ) {
				xge_shape_ex_matrix_t tFilterTransform = __xgeSvgMatrixMul(
					tStyle.tTransform, pDef->tGroupFilterTransform
				);

				iRet = __xgeSvgWrapGroupFilter(pSvg, iFirstItem, iGroupFilter, tFilterTransform);
				if ( iRet != XGE_OK ) return iRet;
			}
		}
		if ( sUseFilterId[0] != '\0' ) {
			int iUseFilter = __xgeSvgFilterFind(pSvg, sUseFilterId);

			if ( iUseFilter >= 0 ) {
				iRet = __xgeSvgWrapGroupFilter(pSvg, iFirstItem, iUseFilter, tStyle.tTransform);
				if ( iRet != XGE_OK ) return iRet;
			}
		}
		iRet = __xgeSvgDrawItemsWrapGroup(
			pSvg, iFirstItem, bUseBlendSet, iUseBlend, fUseOpacity, NULL, 0, 0,
			fUseOpacity, NULL
		);
		if ( iRet != XGE_OK ) return iRet;
	}
	return XGE_OK;
}

static int __xgeSvgApplyUseInstanceWithAspect(xge_svg pSvg, int iDefIndex, const xge_svg_style_t* pUseStyle, int iTargetDef, float fX, float fY, float fW, float fH, int bHasWidth, int bHasHeight, const char* sAspect)
{
	int iOldAspectAlignX;
	int iOldAspectAlignY;
	int iOldAspectMeetOrSlice;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (iDefIndex < 0) || (iDefIndex >= pSvg->iDefCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (sAspect == NULL) || (sAspect[0] == '\0') || !pSvg->pDefs[iDefIndex].bHasViewBox ) {
		return __xgeSvgApplyUseInstance(pSvg, iDefIndex, pUseStyle, iTargetDef, fX, fY, fW, fH, bHasWidth, bHasHeight, NULL);
	}
	iOldAspectAlignX = pSvg->pDefs[iDefIndex].iAspectAlignX;
	iOldAspectAlignY = pSvg->pDefs[iDefIndex].iAspectAlignY;
	iOldAspectMeetOrSlice = pSvg->pDefs[iDefIndex].iAspectMeetOrSlice;
	__xgeSvgParsePreserveAspectRatioFields(sAspect,
		&pSvg->pDefs[iDefIndex].iAspectAlignX,
		&pSvg->pDefs[iDefIndex].iAspectAlignY,
		&pSvg->pDefs[iDefIndex].iAspectMeetOrSlice);
	iRet = __xgeSvgApplyUseInstance(pSvg, iDefIndex, pUseStyle, iTargetDef, fX, fY, fW, fH, bHasWidth, bHasHeight, NULL);
	pSvg->pDefs[iDefIndex].iAspectAlignX = iOldAspectAlignX;
	pSvg->pDefs[iDefIndex].iAspectAlignY = iOldAspectAlignY;
	pSvg->pDefs[iDefIndex].iAspectMeetOrSlice = iOldAspectMeetOrSlice;
	return iRet;
}

static int __xgeSvgApplyUseInstanceWithMainIds(xge_svg pSvg, int iDefIndex, const xge_svg_style_t* pUseStyle, int iTargetDef, int iMainIdDefIndex, int iExtraMainIdDefIndex, float fX, float fY, float fW, float fH, int bHasWidth, int bHasHeight);

static int __xgeSvgApplyUseInstanceWithMainId(xge_svg pSvg, int iDefIndex, const xge_svg_style_t* pUseStyle, int iTargetDef, int iMainIdDefIndex, float fX, float fY, float fW, float fH, int bHasWidth, int bHasHeight)
{
	return __xgeSvgApplyUseInstanceWithMainIds(pSvg, iDefIndex, pUseStyle, iTargetDef, iMainIdDefIndex, -1, fX, fY, fW, fH, bHasWidth, bHasHeight);
}

static int __xgeSvgApplyUseInstanceWithMainIds(xge_svg pSvg, int iDefIndex, const xge_svg_style_t* pUseStyle, int iTargetDef, int iMainIdDefIndex, int iExtraMainIdDefIndex, float fX, float fY, float fW, float fH, int bHasWidth, int bHasHeight)
{
	xge_shape_ex_matrix_t tLogicalTransform;
	int iFirstItem;
	int iRet;

	iFirstItem = (iTargetDef < 0) ? pSvg->iItemCount : -1;
	tLogicalTransform = __xgeSvgMatrixIdentity();
	iRet = __xgeSvgApplyUseInstance(
		pSvg, iDefIndex, pUseStyle, iTargetDef, fX, fY, fW, fH,
		bHasWidth, bHasHeight, iTargetDef < 0 ? &tLogicalTransform : NULL
	);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( iTargetDef < 0 ) {
		uint32_t iId = 0;

		if ( (iMainIdDefIndex >= 0) && (iMainIdDefIndex < pSvg->iDefCount) &&
		     (pSvg->pDefs[iMainIdDefIndex].sId != NULL) ) {
			iId = xgeShapeExIdFromName(pSvg->pDefs[iMainIdDefIndex].sId);
		}
		iRet = __xgeSvgDrawItemsWrapGroup(
			pSvg, iFirstItem, 0, XGE_BLEND_ALPHA, 1.0f, NULL, iId, 1,
			pUseStyle->fOpacity, &tLogicalTransform
		);
		if ( iRet != XGE_OK ) return iRet;
	}
	if ( (iTargetDef < 0) && (iMainIdDefIndex >= 0) ) {
		iRet = __xgeSvgApplyUseInstance(pSvg, iDefIndex, pUseStyle, iMainIdDefIndex, fX, fY, fW, fH, bHasWidth, bHasHeight, NULL);
	}
	if ( (iRet == XGE_OK) && (iTargetDef < 0) && (iExtraMainIdDefIndex >= 0) && (iExtraMainIdDefIndex != iMainIdDefIndex) ) {
		iRet = __xgeSvgApplyUseInstance(pSvg, iDefIndex, pUseStyle, iExtraMainIdDefIndex, fX, fY, fW, fH, bHasWidth, bHasHeight, NULL);
	}
	return iRet;
}

static int __xgeSvgUseReferencesOpenAncestor(xge_svg pSvg, const char* sId)
{
	char sAncestorId[XGE_SVG_ID_MAX];
	int i;

	if ( !__xgeSvgValid(pSvg) || (sId == NULL) || (sId[0] == '\0') ) {
		return 0;
	}
	for ( i = pSvg->iElementStackTop - 1; i >= 0; i-- ) {
		const xge_svg_element_stack_item_t* pItem = &pSvg->arrElementStack[i];

		if ( __xgeSvgAttrCopy(pItem->pTag, pItem->pTagEnd, "id", sAncestorId, sizeof(sAncestorId)) &&
		     (strcmp(sAncestorId, sId) == 0) ) {
			return 1;
		}
	}
	return 0;
}

static int __xgeSvgDefHasPendingUse(xge_svg pSvg, int iDefIndex)
{
	int i;

	if ( !__xgeSvgValid(pSvg) || (iDefIndex < 0) ) {
		return 0;
	}
	for ( i = 0; i < pSvg->iPendingUseCount; i++ ) {
		if ( !pSvg->pPendingUses[i].bResolved && (pSvg->pPendingUses[i].iTargetDef == iDefIndex) ) {
			return 1;
		}
	}
	return 0;
}

static int __xgeSvgDefHasPendingShapeResource(xge_svg pSvg, int iDefIndex)
{
	int i;
	int j;

	if ( !__xgeSvgValid(pSvg) || (iDefIndex < 0) || (iDefIndex >= pSvg->iDefCount) ) {
		return 0;
	}
	for ( i = 0; i < pSvg->pDefs[iDefIndex].iShapeCount; i++ ) {
		xge_shape_ex pShape = pSvg->pDefs[iDefIndex].pShapes[i];

		for ( j = 0; j < pSvg->iPendingShapeResourceCount; j++ ) {
			if ( pSvg->pPendingShapeResources[j].pShape == pShape ) {
				return 1;
			}
		}
	}
	return 0;
}

static int __xgeSvgParseUse(xge_svg pSvg, const char* pTag, const char* pTagEnd, const xge_svg_style_t* pParentStyle, int bInDefs, int iCurrentDef, int iGroupMainIdDefIndex)
{
	char sId[XGE_SVG_ATTR_MAX];
	xge_svg_style_t tStyle;
	float fX;
	float fY;
	float fW;
	float fH;
	int bHasWidth;
	int bHasHeight;
	int iDefIndex;
	int iTargetDef;
	int iOwnMainIdDefIndex;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pTag == NULL) || (pTagEnd == NULL) || (pParentStyle == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( !__xgeSvgUseHrefIdCopy(pTag, pTagEnd, sId, sizeof(sId)) ) {
		return XGE_OK;
	}
	if ( __xgeSvgUseReferencesOpenAncestor(pSvg, sId) ) {
		return XGE_OK;
	}
	iTargetDef = __xgeSvgShapeTargetDef(pSvg, pTag, pTagEnd, bInDefs, iCurrentDef, &iRet);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( iTargetDef == -2 ) {
		return XGE_OK;
	}
	iRet = __xgeSvgMainIdDefIndex(pSvg, pTag, pTagEnd, bInDefs, iCurrentDef, &iOwnMainIdDefIndex);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	tStyle = __xgeSvgStyleResolve(pSvg, pParentStyle, pTag, pTagEnd);
	fX = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "x", XGE_SVG_LENGTH_BASIS_X, tStyle.fFontSize, 0.0f);
	fY = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "y", XGE_SVG_LENGTH_BASIS_Y, tStyle.fFontSize, 0.0f);
	fW = 0.0f;
	fH = 0.0f;
	bHasWidth = __xgeSvgAttrLengthCopyEx(pTag, pTagEnd, "width", __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_X), tStyle.fFontSize, &fW);
	bHasHeight = __xgeSvgAttrLengthCopyEx(pTag, pTagEnd, "height", __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_Y), tStyle.fFontSize, &fH);
	iDefIndex = __xgeSvgDefFind(pSvg, sId);
	if ( (iDefIndex < 0) || (iTargetDef >= 0) || __xgeSvgDefHasPendingUse(pSvg, iDefIndex) ||
	     __xgeSvgDefHasPendingShapeResource(pSvg, iDefIndex) ) {
		return __xgeSvgPendingUseAdd(pSvg, sId, &tStyle, iTargetDef, iOwnMainIdDefIndex, iGroupMainIdDefIndex, fX, fY, fW, fH, bHasWidth, bHasHeight);
	}
	return __xgeSvgApplyUseInstanceWithMainIds(pSvg, iDefIndex, &tStyle, iTargetDef, iOwnMainIdDefIndex, iGroupMainIdDefIndex, fX, fY, fW, fH, bHasWidth, bHasHeight);
}

static int __xgeSvgFinalizeDefPendingUses(xge_svg pSvg, int iDefIndex, int* pDefStates)
{
	int i;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (iDefIndex < 0) || (iDefIndex >= pSvg->iDefCount) || (pDefStates == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (pDefStates[iDefIndex] == 2) || (pDefStates[iDefIndex] < 0) ) {
		return XGE_OK;
	}
	if ( pDefStates[iDefIndex] == 1 ) {
		pDefStates[iDefIndex] = -1;
		return XGE_OK;
	}
	pDefStates[iDefIndex] = 1;
	for ( i = 0; i < pSvg->iPendingUseCount; i++ ) {
		xge_svg_pending_use_t* pUse = &pSvg->pPendingUses[i];
		int iSourceDef;

		if ( pUse->bResolved || (pUse->iTargetDef != iDefIndex) ) {
			continue;
		}
		iSourceDef = __xgeSvgDefFind(pSvg, pUse->sId);
		if ( iSourceDef < 0 ) {
			continue;
		}
		if ( iSourceDef == iDefIndex ) {
			pDefStates[iDefIndex] = -1;
			break;
		}
		iRet = __xgeSvgFinalizeDefPendingUses(pSvg, iSourceDef, pDefStates);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		if ( pDefStates[iSourceDef] < 0 ) {
			pDefStates[iDefIndex] = -1;
			break;
		}
	}
	if ( pDefStates[iDefIndex] < 0 ) {
		return XGE_OK;
	}
	for ( i = 0; i < pSvg->iPendingUseCount; i++ ) {
		xge_svg_pending_use_t* pUse = &pSvg->pPendingUses[i];
		int iSourceDef;

		if ( pUse->bResolved || (pUse->iTargetDef != iDefIndex) ) {
			continue;
		}
		iSourceDef = __xgeSvgDefFind(pSvg, pUse->sId);
		if ( iSourceDef < 0 ) {
			continue;
		}
		iRet = __xgeSvgApplyUseInstanceWithMainIds(pSvg, iSourceDef, &pUse->tStyle, pUse->iTargetDef, pUse->iMainIdDefIndex, pUse->iExtraMainIdDefIndex, pUse->fX, pUse->fY, pUse->fW, pUse->fH, pUse->bHasWidth, pUse->bHasHeight);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		pUse->bResolved = 1;
	}
	pDefStates[iDefIndex] = 2;
	return XGE_OK;
}

static int __xgeSvgResolvePendingUses(xge_svg pSvg)
{
	int* pDefStates;
	int i;
	int iRet;

	if ( !__xgeSvgValid(pSvg) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pSvg->iPendingUseCount <= 0 ) {
		return XGE_OK;
	}
	pDefStates = (int*)xrtCalloc((size_t)(pSvg->iDefCount > 0 ? pSvg->iDefCount : 1), sizeof(*pDefStates));
	if ( pDefStates == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	for ( i = 0; i < pSvg->iDefCount; i++ ) {
		iRet = __xgeSvgFinalizeDefPendingUses(pSvg, i, pDefStates);
		if ( iRet != XGE_OK ) {
			xrtFree(pDefStates);
			return iRet;
		}
	}
	for ( i = 0; i < pSvg->iPendingUseCount; i++ ) {
		xge_svg_pending_use_t* pUse = &pSvg->pPendingUses[i];
		int iSourceDef;
		int iFirstItem;

		if ( pUse->bResolved || (pUse->iTargetDef >= 0) ) {
			continue;
		}
		iSourceDef = __xgeSvgDefFind(pSvg, pUse->sId);
		if ( iSourceDef < 0 ) {
			continue;
		}
		iRet = __xgeSvgFinalizeDefPendingUses(pSvg, iSourceDef, pDefStates);
		if ( iRet != XGE_OK ) {
			xrtFree(pDefStates);
			return iRet;
		}
		if ( pDefStates[iSourceDef] < 0 ) {
			pUse->bResolved = 1;
			continue;
		}
		iFirstItem = pSvg->iItemCount;
		iRet = __xgeSvgApplyUseInstanceWithMainIds(pSvg, iSourceDef, &pUse->tStyle, pUse->iTargetDef, pUse->iMainIdDefIndex, pUse->iExtraMainIdDefIndex, pUse->fX, pUse->fY, pUse->fW, pUse->fH, pUse->bHasWidth, pUse->bHasHeight);
		if ( iRet != XGE_OK ) {
			xrtFree(pDefStates);
			return iRet;
		}
		iRet = __xgeSvgReplacePendingUseMainItems(pSvg, pUse->iSerial, iFirstItem);
		if ( iRet != XGE_OK ) {
			xrtFree(pDefStates);
			return iRet;
		}
		pUse->bResolved = 1;
	}
	for ( i = 0; i < pSvg->iPendingUseCount; i++ ) {
		xge_svg_pending_use_t* pUse = &pSvg->pPendingUses[i];

		if ( pUse->iTargetDef >= 0 ) continue;
		iRet = __xgeSvgReplacePendingUseMainItems(
			pSvg, pUse->iSerial, pSvg->iItemCount
		);
		if ( iRet != XGE_OK ) {
			xrtFree(pDefStates);
			return iRet;
		}
	}
	xrtFree(pDefStates);
	__xgeSvgPendingUsesClear(pSvg);
	return XGE_OK;
}

static int __xgeSvgGradientLengthCopy(xge_svg pSvg, const char* pTag, const char* pTagEnd, const char* sName, int iUnits, int iBasis, float fFontSize, float* pValue, int* pPercentage)
{
	if ( !__xgeSvgValid(pSvg) || (pTag == NULL) || (pTagEnd == NULL) || (sName == NULL) ||
	     (pValue == NULL) || (pPercentage == NULL) ) {
		return 0;
	}
	(void)iUnits;
	(void)iBasis;
	return __xgeSvgAttrLengthCopyPercentEx(pTag, pTagEnd, sName, 1.0f, fFontSize, pValue, pPercentage);
}

static int __xgeSvgPatternLengthCopy(xge_svg pSvg, const char* pTag, const char* pTagEnd, const char* sName, int iUnits, int iBasis, float fFontSize, float* pValue)
{
	float fPercentRef;

	if ( !__xgeSvgValid(pSvg) || (pTag == NULL) || (pTagEnd == NULL) || (sName == NULL) || (pValue == NULL) ) {
		return 0;
	}
	fPercentRef = (iUnits == XGE_SVG_PATTERN_USER_SPACE) ? __xgeSvgLengthPercentRef(pSvg, iBasis) : 1.0f;
	return __xgeSvgAttrLengthCopyEx(pTag, pTagEnd, sName, fPercentRef, fFontSize, pValue);
}

static int __xgeSvgParseLinearGradient(xge_svg pSvg, const char* pTag, const char* pTagEnd, const xge_svg_style_t* pParentStyle, int bUseParentCurrentColor, int* pGradientIndex)
{
	char sId[XGE_SVG_ATTR_MAX];
	char sValue[XGE_SVG_ATTR_MAX];
	int bPercentage;
	int iIndex;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pTag == NULL) || (pTagEnd == NULL) || (pGradientIndex == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pGradientIndex = -1;
	if ( !__xgeSvgAttrCopy(pTag, pTagEnd, "id", sId, sizeof(sId)) || (sId[0] == '\0') ) {
		return XGE_OK;
	}
	iRet = __xgeSvgLinearGradientGetOrCreate(pSvg, sId, &iIndex);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	xrtFree(pSvg->pLinearGradients[iIndex].sHrefId);
	pSvg->pLinearGradients[iIndex].sHrefId = NULL;
	pSvg->pLinearGradients[iIndex].fX1 = 0.0f;
	pSvg->pLinearGradients[iIndex].fY1 = 0.0f;
	pSvg->pLinearGradients[iIndex].fX2 = 1.0f;
	pSvg->pLinearGradients[iIndex].fY2 = 0.0f;
	pSvg->pLinearGradients[iIndex].iUnits = XGE_SHAPE_EX_GRADIENT_OBJECT_BOUNDING_BOX;
	pSvg->pLinearGradients[iIndex].iSpread = XGE_SHAPE_EX_GRADIENT_SPREAD_PAD;
	pSvg->pLinearGradients[iIndex].tStyle = __xgeSvgStyleResolve(pSvg, pParentStyle, pTag, pTagEnd);
	pSvg->pLinearGradients[iIndex].iStopCurrentColor = (bUseParentCurrentColor && (pParentStyle != NULL)) ? pParentStyle->iCurrentColor : XGE_COLOR_RGBA(0, 0, 0, 255);
	pSvg->pLinearGradients[iIndex].bStopCurrentColorSet = (bUseParentCurrentColor && (pParentStyle != NULL)) ? pParentStyle->bColorSet : 0;
	__xgeSvgStyleResetStopPaint(&pSvg->pLinearGradients[iIndex].tStyle);
	pSvg->pLinearGradients[iIndex].tTransform = __xgeSvgMatrixIdentity();
	pSvg->pLinearGradients[iIndex].iFlags = 0;
	pSvg->pLinearGradients[iIndex].iPercentFlags = XGE_SVG_GRADIENT_HAS_X1 | XGE_SVG_GRADIENT_HAS_Y1 |
		XGE_SVG_GRADIENT_HAS_X2 | XGE_SVG_GRADIENT_HAS_Y2;
	pSvg->pLinearGradients[iIndex].iStopCount = 0;
	if ( __xgeSvgHrefIdCopy(pTag, pTagEnd, sValue, sizeof(sValue)) ) {
		pSvg->pLinearGradients[iIndex].sHrefId = __xgeStrDup(sValue);
		if ( pSvg->pLinearGradients[iIndex].sHrefId == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
	}
	if ( __xgeSvgAttrCopy(pTag, pTagEnd, "gradientUnits", sValue, sizeof(sValue)) ) {
		int iUnits;
		if ( __xgeSvgParseGradientUnits(sValue, &iUnits) ) {
			pSvg->pLinearGradients[iIndex].iUnits = iUnits;
		}
		pSvg->pLinearGradients[iIndex].iFlags |= XGE_SVG_GRADIENT_HAS_UNITS;
	}
	if ( __xgeSvgGradientLengthCopy(pSvg, pTag, pTagEnd, "x1", pSvg->pLinearGradients[iIndex].iUnits, XGE_SVG_LENGTH_BASIS_X, pSvg->pLinearGradients[iIndex].tStyle.fFontSize, &pSvg->pLinearGradients[iIndex].fX1, &bPercentage) ) {
		pSvg->pLinearGradients[iIndex].iFlags |= XGE_SVG_GRADIENT_HAS_X1;
		if ( bPercentage ) pSvg->pLinearGradients[iIndex].iPercentFlags |= XGE_SVG_GRADIENT_HAS_X1;
		else pSvg->pLinearGradients[iIndex].iPercentFlags &= ~XGE_SVG_GRADIENT_HAS_X1;
	}
	if ( __xgeSvgGradientLengthCopy(pSvg, pTag, pTagEnd, "y1", pSvg->pLinearGradients[iIndex].iUnits, XGE_SVG_LENGTH_BASIS_Y, pSvg->pLinearGradients[iIndex].tStyle.fFontSize, &pSvg->pLinearGradients[iIndex].fY1, &bPercentage) ) {
		pSvg->pLinearGradients[iIndex].iFlags |= XGE_SVG_GRADIENT_HAS_Y1;
		if ( bPercentage ) pSvg->pLinearGradients[iIndex].iPercentFlags |= XGE_SVG_GRADIENT_HAS_Y1;
		else pSvg->pLinearGradients[iIndex].iPercentFlags &= ~XGE_SVG_GRADIENT_HAS_Y1;
	}
	if ( __xgeSvgGradientLengthCopy(pSvg, pTag, pTagEnd, "x2", pSvg->pLinearGradients[iIndex].iUnits, XGE_SVG_LENGTH_BASIS_X, pSvg->pLinearGradients[iIndex].tStyle.fFontSize, &pSvg->pLinearGradients[iIndex].fX2, &bPercentage) ) {
		pSvg->pLinearGradients[iIndex].iFlags |= XGE_SVG_GRADIENT_HAS_X2;
		if ( bPercentage ) pSvg->pLinearGradients[iIndex].iPercentFlags |= XGE_SVG_GRADIENT_HAS_X2;
		else pSvg->pLinearGradients[iIndex].iPercentFlags &= ~XGE_SVG_GRADIENT_HAS_X2;
	}
	if ( __xgeSvgGradientLengthCopy(pSvg, pTag, pTagEnd, "y2", pSvg->pLinearGradients[iIndex].iUnits, XGE_SVG_LENGTH_BASIS_Y, pSvg->pLinearGradients[iIndex].tStyle.fFontSize, &pSvg->pLinearGradients[iIndex].fY2, &bPercentage) ) {
		pSvg->pLinearGradients[iIndex].iFlags |= XGE_SVG_GRADIENT_HAS_Y2;
		if ( bPercentage ) pSvg->pLinearGradients[iIndex].iPercentFlags |= XGE_SVG_GRADIENT_HAS_Y2;
		else pSvg->pLinearGradients[iIndex].iPercentFlags &= ~XGE_SVG_GRADIENT_HAS_Y2;
	}
	if ( __xgeSvgAttrCopy(pTag, pTagEnd, "spreadMethod", sValue, sizeof(sValue)) ) {
		int iSpread;
		if ( __xgeSvgParseGradientSpread(sValue, &iSpread) ) {
			pSvg->pLinearGradients[iIndex].iSpread = iSpread;
		}
		pSvg->pLinearGradients[iIndex].iFlags |= XGE_SVG_GRADIENT_HAS_SPREAD;
	}
	if ( __xgeSvgAttrCopy(pTag, pTagEnd, "gradientTransform", sValue, sizeof(sValue)) ) {
		xge_shape_ex_matrix_t tTransform;

		if ( __xgeSvgParseTransform(sValue, &tTransform) ) {
			pSvg->pLinearGradients[iIndex].tTransform = tTransform;
			pSvg->pLinearGradients[iIndex].iFlags |= XGE_SVG_GRADIENT_HAS_TRANSFORM;
		}
	}
	*pGradientIndex = iIndex;
	return XGE_OK;
}

static int __xgeSvgParseRadialGradient(xge_svg pSvg, const char* pTag, const char* pTagEnd, const xge_svg_style_t* pParentStyle, int bUseParentCurrentColor, int* pGradientIndex)
{
	char sId[XGE_SVG_ATTR_MAX];
	char sValue[XGE_SVG_ATTR_MAX];
	int bPercentage;
	int iIndex;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pTag == NULL) || (pTagEnd == NULL) || (pGradientIndex == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pGradientIndex = -1;
	if ( !__xgeSvgAttrCopy(pTag, pTagEnd, "id", sId, sizeof(sId)) || (sId[0] == '\0') ) {
		return XGE_OK;
	}
	iRet = __xgeSvgRadialGradientGetOrCreate(pSvg, sId, &iIndex);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	xrtFree(pSvg->pRadialGradients[iIndex].sHrefId);
	pSvg->pRadialGradients[iIndex].sHrefId = NULL;
	pSvg->pRadialGradients[iIndex].fCX = 0.5f;
	pSvg->pRadialGradients[iIndex].fCY = 0.5f;
	pSvg->pRadialGradients[iIndex].fR = 0.5f;
	pSvg->pRadialGradients[iIndex].fFX = 0.5f;
	pSvg->pRadialGradients[iIndex].fFY = 0.5f;
	pSvg->pRadialGradients[iIndex].fFR = 0.0f;
	pSvg->pRadialGradients[iIndex].iUnits = XGE_SHAPE_EX_GRADIENT_OBJECT_BOUNDING_BOX;
	pSvg->pRadialGradients[iIndex].iSpread = XGE_SHAPE_EX_GRADIENT_SPREAD_PAD;
	pSvg->pRadialGradients[iIndex].tStyle = __xgeSvgStyleResolve(pSvg, pParentStyle, pTag, pTagEnd);
	pSvg->pRadialGradients[iIndex].iStopCurrentColor = (bUseParentCurrentColor && (pParentStyle != NULL)) ? pParentStyle->iCurrentColor : XGE_COLOR_RGBA(0, 0, 0, 255);
	pSvg->pRadialGradients[iIndex].bStopCurrentColorSet = (bUseParentCurrentColor && (pParentStyle != NULL)) ? pParentStyle->bColorSet : 0;
	__xgeSvgStyleResetStopPaint(&pSvg->pRadialGradients[iIndex].tStyle);
	pSvg->pRadialGradients[iIndex].tTransform = __xgeSvgMatrixIdentity();
	pSvg->pRadialGradients[iIndex].iFlags = 0;
	pSvg->pRadialGradients[iIndex].iPercentFlags = XGE_SVG_GRADIENT_HAS_CX | XGE_SVG_GRADIENT_HAS_CY |
		XGE_SVG_GRADIENT_HAS_R | XGE_SVG_GRADIENT_HAS_FX | XGE_SVG_GRADIENT_HAS_FY | XGE_SVG_GRADIENT_HAS_FR;
	pSvg->pRadialGradients[iIndex].iStopCount = 0;
	if ( __xgeSvgHrefIdCopy(pTag, pTagEnd, sValue, sizeof(sValue)) ) {
		pSvg->pRadialGradients[iIndex].sHrefId = __xgeStrDup(sValue);
		if ( pSvg->pRadialGradients[iIndex].sHrefId == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
	}
	if ( __xgeSvgAttrCopy(pTag, pTagEnd, "gradientUnits", sValue, sizeof(sValue)) ) {
		int iUnits;
		if ( __xgeSvgParseGradientUnits(sValue, &iUnits) ) {
			pSvg->pRadialGradients[iIndex].iUnits = iUnits;
		}
		pSvg->pRadialGradients[iIndex].iFlags |= XGE_SVG_GRADIENT_HAS_UNITS;
	}
	if ( __xgeSvgGradientLengthCopy(pSvg, pTag, pTagEnd, "cx", pSvg->pRadialGradients[iIndex].iUnits, XGE_SVG_LENGTH_BASIS_X, pSvg->pRadialGradients[iIndex].tStyle.fFontSize, &pSvg->pRadialGradients[iIndex].fCX, &bPercentage) ) {
		pSvg->pRadialGradients[iIndex].iFlags |= XGE_SVG_GRADIENT_HAS_CX;
		if ( bPercentage ) pSvg->pRadialGradients[iIndex].iPercentFlags |= XGE_SVG_GRADIENT_HAS_CX;
		else pSvg->pRadialGradients[iIndex].iPercentFlags &= ~XGE_SVG_GRADIENT_HAS_CX;
	}
	if ( __xgeSvgGradientLengthCopy(pSvg, pTag, pTagEnd, "cy", pSvg->pRadialGradients[iIndex].iUnits, XGE_SVG_LENGTH_BASIS_Y, pSvg->pRadialGradients[iIndex].tStyle.fFontSize, &pSvg->pRadialGradients[iIndex].fCY, &bPercentage) ) {
		pSvg->pRadialGradients[iIndex].iFlags |= XGE_SVG_GRADIENT_HAS_CY;
		if ( bPercentage ) pSvg->pRadialGradients[iIndex].iPercentFlags |= XGE_SVG_GRADIENT_HAS_CY;
		else pSvg->pRadialGradients[iIndex].iPercentFlags &= ~XGE_SVG_GRADIENT_HAS_CY;
	}
	if ( __xgeSvgGradientLengthCopy(pSvg, pTag, pTagEnd, "r", pSvg->pRadialGradients[iIndex].iUnits, XGE_SVG_LENGTH_BASIS_OTHER, pSvg->pRadialGradients[iIndex].tStyle.fFontSize, &pSvg->pRadialGradients[iIndex].fR, &bPercentage) ) {
		pSvg->pRadialGradients[iIndex].iFlags |= XGE_SVG_GRADIENT_HAS_R;
		if ( bPercentage ) pSvg->pRadialGradients[iIndex].iPercentFlags |= XGE_SVG_GRADIENT_HAS_R;
		else pSvg->pRadialGradients[iIndex].iPercentFlags &= ~XGE_SVG_GRADIENT_HAS_R;
	}
	if ( __xgeSvgGradientLengthCopy(pSvg, pTag, pTagEnd, "fr", pSvg->pRadialGradients[iIndex].iUnits, XGE_SVG_LENGTH_BASIS_OTHER, pSvg->pRadialGradients[iIndex].tStyle.fFontSize, &pSvg->pRadialGradients[iIndex].fFR, &bPercentage) ) {
		pSvg->pRadialGradients[iIndex].iFlags |= XGE_SVG_GRADIENT_HAS_FR;
		if ( bPercentage ) pSvg->pRadialGradients[iIndex].iPercentFlags |= XGE_SVG_GRADIENT_HAS_FR;
		else pSvg->pRadialGradients[iIndex].iPercentFlags &= ~XGE_SVG_GRADIENT_HAS_FR;
	}
	if ( __xgeSvgGradientLengthCopy(pSvg, pTag, pTagEnd, "fx", pSvg->pRadialGradients[iIndex].iUnits, XGE_SVG_LENGTH_BASIS_X, pSvg->pRadialGradients[iIndex].tStyle.fFontSize, &pSvg->pRadialGradients[iIndex].fFX, &bPercentage) ) {
		pSvg->pRadialGradients[iIndex].iFlags |= XGE_SVG_GRADIENT_HAS_FX;
		if ( bPercentage ) pSvg->pRadialGradients[iIndex].iPercentFlags |= XGE_SVG_GRADIENT_HAS_FX;
		else pSvg->pRadialGradients[iIndex].iPercentFlags &= ~XGE_SVG_GRADIENT_HAS_FX;
	} else if ( pSvg->pRadialGradients[iIndex].sHrefId == NULL ) {
		pSvg->pRadialGradients[iIndex].fFX = pSvg->pRadialGradients[iIndex].fCX;
		pSvg->pRadialGradients[iIndex].iPercentFlags = (pSvg->pRadialGradients[iIndex].iPercentFlags & ~XGE_SVG_GRADIENT_HAS_FX) |
			(pSvg->pRadialGradients[iIndex].iPercentFlags & XGE_SVG_GRADIENT_HAS_CX ? XGE_SVG_GRADIENT_HAS_FX : 0);
	}
	if ( __xgeSvgGradientLengthCopy(pSvg, pTag, pTagEnd, "fy", pSvg->pRadialGradients[iIndex].iUnits, XGE_SVG_LENGTH_BASIS_Y, pSvg->pRadialGradients[iIndex].tStyle.fFontSize, &pSvg->pRadialGradients[iIndex].fFY, &bPercentage) ) {
		pSvg->pRadialGradients[iIndex].iFlags |= XGE_SVG_GRADIENT_HAS_FY;
		if ( bPercentage ) pSvg->pRadialGradients[iIndex].iPercentFlags |= XGE_SVG_GRADIENT_HAS_FY;
		else pSvg->pRadialGradients[iIndex].iPercentFlags &= ~XGE_SVG_GRADIENT_HAS_FY;
	} else if ( pSvg->pRadialGradients[iIndex].sHrefId == NULL ) {
		pSvg->pRadialGradients[iIndex].fFY = pSvg->pRadialGradients[iIndex].fCY;
		pSvg->pRadialGradients[iIndex].iPercentFlags = (pSvg->pRadialGradients[iIndex].iPercentFlags & ~XGE_SVG_GRADIENT_HAS_FY) |
			(pSvg->pRadialGradients[iIndex].iPercentFlags & XGE_SVG_GRADIENT_HAS_CY ? XGE_SVG_GRADIENT_HAS_FY : 0);
	}
	if ( __xgeSvgAttrCopy(pTag, pTagEnd, "spreadMethod", sValue, sizeof(sValue)) ) {
		int iSpread;
		if ( __xgeSvgParseGradientSpread(sValue, &iSpread) ) {
			pSvg->pRadialGradients[iIndex].iSpread = iSpread;
		}
		pSvg->pRadialGradients[iIndex].iFlags |= XGE_SVG_GRADIENT_HAS_SPREAD;
	}
	if ( __xgeSvgAttrCopy(pTag, pTagEnd, "gradientTransform", sValue, sizeof(sValue)) ) {
		xge_shape_ex_matrix_t tTransform;

		if ( __xgeSvgParseTransform(sValue, &tTransform) ) {
			pSvg->pRadialGradients[iIndex].tTransform = tTransform;
			pSvg->pRadialGradients[iIndex].iFlags |= XGE_SVG_GRADIENT_HAS_TRANSFORM;
		}
	}
	*pGradientIndex = iIndex;
	return XGE_OK;
}

static int __xgeSvgParseGradientStopData(xge_svg pSvg, uint32_t iStopCurrentColor, int bStopCurrentColorSet, const char* pTag, const char* pTagEnd, xge_shape_ex_color_stop_t* pStop)
{
	typedef struct xge_svg_stop_parse_state_t {
		float fOffset;
		uint32_t iColor;
		float fOpacity;
		uint32_t iCurrentColor;
		int bCurrentColorSet;
		int bStyleStopColorSet;
		int bStyleStopOpacitySet;
	} xge_svg_stop_parse_state_t;
	xge_svg_stop_parse_state_t tState;
	const char* pCursor;
	char sName[XGE_SVG_ATTR_MAX];
	char sValue[XGE_SVG_ATTR_MAX];
	char sStyleValue[XGE_SVG_ATTR_MAX];
	uint32_t iColor;

	if ( !__xgeSvgValid(pSvg) || (pTag == NULL) || (pTagEnd == NULL) || (pStop == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tState.fOffset = 0.0f;
	tState.iColor = XGE_COLOR_RGBA(0, 0, 0, 255);
	tState.fOpacity = 1.0f;
	tState.iCurrentColor = iStopCurrentColor;
	tState.bCurrentColorSet = bStopCurrentColorSet;
	tState.bStyleStopColorSet = 0;
	tState.bStyleStopOpacitySet = 0;
	pCursor = pTag;
	while ( __xgeSvgAttrNext(&pCursor, pTagEnd, sName, sizeof(sName), sValue, sizeof(sValue)) ) {
		if ( strcmp(sName, "offset") == 0 ) {
			tState.fOffset = __xgeSvgParseStopOffset(sValue);
		} else if ( strcmp(sName, "stop-opacity") == 0 ) {
			if ( !tState.bStyleStopOpacitySet ) {
				tState.fOpacity = __xgeSvgParseOpacityThorvg(sValue);
			}
		} else if ( strcmp(sName, "stop-color") == 0 ) {
			if ( strcmp(sValue, "currentColor") == 0 ) {
				if ( tState.bCurrentColorSet ) {
					tState.iColor = tState.iCurrentColor;
				}
			} else if ( !tState.bStyleStopColorSet ) {
				iColor = tState.iColor;
				if ( __xgeSvgParseColor(sValue, &iColor) ) {
					tState.iColor = iColor;
				}
			}
		} else if ( strcmp(sName, "style") == 0 ) {
			const char* pStyle;

			pStyle = sValue;
			while ( *pStyle != '\0' ) {
				const char* pNameStart;
				const char* pNameEnd;
				const char* pValueStart;
				const char* pValueEnd;
				int bImportant;

				for ( ;; ) {
					while ( __xgeSvgCssIsSpace(*pStyle) || (*pStyle == ';') ) pStyle++;
					if ( (pStyle[0] == '/') && (pStyle[1] == '*') ) {
						pStyle += 2;
						while ( (pStyle[0] != '\0') && !((pStyle[0] == '*') && (pStyle[1] == '/')) ) pStyle++;
						if ( pStyle[0] != '\0' ) pStyle += 2;
						continue;
					}
					break;
				}
				if ( *pStyle == '\0' ) {
					break;
				}
				pNameStart = pStyle;
				while ( (*pStyle != '\0') && (*pStyle != ':') && (*pStyle != ';') ) pStyle++;
				pNameEnd = pStyle;
				__xgeSvgTrimRange(&pNameStart, &pNameEnd);
				if ( *pStyle != ':' ) {
					while ( (*pStyle != '\0') && (*pStyle != ';') ) pStyle++;
					continue;
				}
				pStyle++;
				pValueStart = pStyle;
				while ( (*pStyle != '\0') && (*pStyle != ';') ) pStyle++;
				pValueEnd = pStyle;
				if ( !__xgeSvgStyleValueCleanCopy(pValueStart, pValueEnd, sStyleValue, sizeof(sStyleValue), &bImportant) ) {
					continue;
				}
				(void)bImportant;
				if ( __xgeSvgRangeEqualsTextExact(pNameStart, pNameEnd, "stop-opacity") ) {
					tState.fOpacity = __xgeSvgParseOpacityThorvg(sStyleValue);
					tState.bStyleStopOpacitySet = 1;
				} else if ( __xgeSvgRangeEqualsTextExact(pNameStart, pNameEnd, "stop-color") ) {
					if ( strcmp(sStyleValue, "currentColor") == 0 ) {
						if ( tState.bCurrentColorSet ) {
							tState.iColor = tState.iCurrentColor;
						}
					} else {
						iColor = tState.iColor;
						if ( __xgeSvgParseColor(sStyleValue, &iColor) ) {
							tState.iColor = iColor;
							tState.bStyleStopColorSet = 1;
						}
					}
				}
			}
		}
	}
	pStop->fOffset = tState.fOffset;
	pStop->iColor = __xgeSvgColorAlpha(tState.iColor, tState.fOpacity);
	return XGE_OK;
}

static int __xgeSvgParseGradientStop(xge_svg pSvg, int iGradientIndex, const char* pTag, const char* pTagEnd)
{
	xge_shape_ex_color_stop_t tStop;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (iGradientIndex < 0) || (iGradientIndex >= pSvg->iLinearGradientCount) || (pTag == NULL) || (pTagEnd == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeSvgParseGradientStopData(
		pSvg,
		pSvg->pLinearGradients[iGradientIndex].iStopCurrentColor,
		pSvg->pLinearGradients[iGradientIndex].bStopCurrentColorSet,
		pTag,
		pTagEnd,
		&tStop);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	return __xgeSvgLinearGradientAddStop(&pSvg->pLinearGradients[iGradientIndex], tStop);
}

static int __xgeSvgParseRadialGradientStop(xge_svg pSvg, int iGradientIndex, const char* pTag, const char* pTagEnd)
{
	xge_shape_ex_color_stop_t tStop;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (iGradientIndex < 0) || (iGradientIndex >= pSvg->iRadialGradientCount) || (pTag == NULL) || (pTagEnd == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeSvgParseGradientStopData(
		pSvg,
		pSvg->pRadialGradients[iGradientIndex].iStopCurrentColor,
		pSvg->pRadialGradients[iGradientIndex].bStopCurrentColorSet,
		pTag,
		pTagEnd,
		&tStop);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	return __xgeSvgRadialGradientAddStop(&pSvg->pRadialGradients[iGradientIndex], tStop);
}

static void __xgeSvgParseRoot(xge_svg pSvg, const char* pTag, const char* pTagEnd, float fFontSize)
{
	char sValue[XGE_SVG_ATTR_MAX];
	float fX;
	float fY;
	float fW;
	float fH;

	if ( __xgeSvgAttrCopy(pTag, pTagEnd, "viewBox", sValue, sizeof(sValue)) &&
	     __xgeSvgParseFourFloats(sValue, &fX, &fY, &fW, &fH) && (fW > 0.0f) && (fH > 0.0f) ) {
		pSvg->tViewBox.fX = fX;
		pSvg->tViewBox.fY = fY;
		pSvg->tViewBox.fW = fW;
		pSvg->tViewBox.fH = fH;
		pSvg->bHasViewBox = 1;
	}
	pSvg->fWidth = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "width", XGE_SVG_LENGTH_BASIS_X, fFontSize, pSvg->bHasViewBox ? pSvg->tViewBox.fW : 0.0f);
	pSvg->fHeight = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "height", XGE_SVG_LENGTH_BASIS_Y, fFontSize, pSvg->bHasViewBox ? pSvg->tViewBox.fH : 0.0f);
	if ( __xgeSvgAttrCopy(pTag, pTagEnd, "preserveAspectRatio", sValue, sizeof(sValue)) ) {
		xgeSvgSetPreserveAspectRatio(pSvg, sValue);
	}
}

static int __xgeSvgOverflowIsVisible(const char* pTag, const char* pTagEnd)
{
	char sStyle[XGE_SVG_ATTR_MAX];
	char sValue[XGE_SVG_ATTR_MAX];
	const char* pStart;
	const char* pEnd;

	if ( (pTag == NULL) || (pTagEnd == NULL) ) {
		return 0;
	}
	sStyle[0] = '\0';
	__xgeSvgAttrCopy(pTag, pTagEnd, "style", sStyle, sizeof(sStyle));
	if ( !__xgeSvgAttrOrStyleCopy(pTag, pTagEnd, sStyle, "overflow", sValue, sizeof(sValue)) ) {
		return 0;
	}
	pStart = sValue;
	pEnd = sValue + strlen(sValue);
	while ( (pStart < pEnd) && __xgeSvgIsSpace((unsigned char)*pStart) ) pStart++;
	while ( (pEnd > pStart) && __xgeSvgIsSpace((unsigned char)pEnd[-1]) ) pEnd--;
	return __xgeSvgCssRangeEqualsText(pStart, pEnd, "visible");
}

static int __xgeSvgOverflowIsSpecified(const char* pTag, const char* pTagEnd)
{
	char sStyle[XGE_SVG_ATTR_MAX];
	char sValue[XGE_SVG_ATTR_MAX];

	if ( (pTag == NULL) || (pTagEnd == NULL) ) {
		return 0;
	}
	sStyle[0] = '\0';
	__xgeSvgAttrCopy(pTag, pTagEnd, "style", sStyle, sizeof(sStyle));
	return __xgeSvgAttrOrStyleCopy(pTag, pTagEnd, sStyle, "overflow", sValue, sizeof(sValue));
}

static xge_svg_style_t __xgeSvgResolveClipMaskContainerStyle(xge_svg pSvg, const xge_svg_style_t* pParentStyle, const char* pTag, const char* pTagEnd)
{
	return __xgeSvgStyleResolve(pSvg, pParentStyle, pTag, pTagEnd);
}

static void __xgeSvgParseSymbolMeta(xge_svg pSvg, int iDefIndex, const char* pTag, const char* pTagEnd)
{
	char sValue[XGE_SVG_ATTR_MAX];
	float fX;
	float fY;
	float fW;
	float fH;

	if ( !__xgeSvgValid(pSvg) || (iDefIndex < 0) || (iDefIndex >= pSvg->iDefCount) || (pTag == NULL) || (pTagEnd == NULL) ) {
		return;
	}
	pSvg->pDefs[iDefIndex].bOverflowVisible = 0;
	pSvg->pDefs[iDefIndex].fWidth = 0.0f;
	pSvg->pDefs[iDefIndex].fHeight = 0.0f;
	pSvg->pDefs[iDefIndex].bHasWidth = __xgeSvgAttrLengthCopyEx(
		pTag, pTagEnd, "width", __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_X),
		XGE_SVG_DEFAULT_FONT_SIZE, &pSvg->pDefs[iDefIndex].fWidth
	);
	pSvg->pDefs[iDefIndex].bHasHeight = __xgeSvgAttrLengthCopyEx(
		pTag, pTagEnd, "height", __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_Y),
		XGE_SVG_DEFAULT_FONT_SIZE, &pSvg->pDefs[iDefIndex].fHeight
	);
	if ( pSvg->pDefs[iDefIndex].fWidth <= 0.0f ) pSvg->pDefs[iDefIndex].bHasWidth = 0;
	if ( pSvg->pDefs[iDefIndex].fHeight <= 0.0f ) pSvg->pDefs[iDefIndex].bHasHeight = 0;
	if ( __xgeSvgAttrCopy(pTag, pTagEnd, "viewBox", sValue, sizeof(sValue)) &&
	     __xgeSvgParseFourFloats(sValue, &fX, &fY, &fW, &fH) && (fW > 0.0f) && (fH > 0.0f) ) {
		pSvg->pDefs[iDefIndex].tViewBox.fX = fX;
		pSvg->pDefs[iDefIndex].tViewBox.fY = fY;
		pSvg->pDefs[iDefIndex].tViewBox.fW = fW;
		pSvg->pDefs[iDefIndex].tViewBox.fH = fH;
		pSvg->pDefs[iDefIndex].bHasViewBox = 1;
	} else if ( pSvg->pDefs[iDefIndex].bHasWidth && pSvg->pDefs[iDefIndex].bHasHeight ) {
		fW = pSvg->pDefs[iDefIndex].fWidth;
		fH = pSvg->pDefs[iDefIndex].fHeight;
		pSvg->pDefs[iDefIndex].tViewBox.fX = 0.0f;
		pSvg->pDefs[iDefIndex].tViewBox.fY = 0.0f;
		pSvg->pDefs[iDefIndex].tViewBox.fW = fW;
		pSvg->pDefs[iDefIndex].tViewBox.fH = fH;
		pSvg->pDefs[iDefIndex].bHasViewBox = 1;
	}
	if ( __xgeSvgAttrCopy(pTag, pTagEnd, "preserveAspectRatio", sValue, sizeof(sValue)) ) {
		__xgeSvgParsePreserveAspectRatioFields(sValue,
			&pSvg->pDefs[iDefIndex].iAspectAlignX,
			&pSvg->pDefs[iDefIndex].iAspectAlignY,
			&pSvg->pDefs[iDefIndex].iAspectMeetOrSlice);
	}
	if ( __xgeSvgAttrCopy(pTag, pTagEnd, "overflow", sValue, sizeof(sValue)) &&
	     __xgeSvgCssRangeEqualsText(sValue, sValue + strlen(sValue), "visible") ) {
		pSvg->pDefs[iDefIndex].bOverflowVisible = 1;
	}
}

static int __xgeSvgParsePattern(xge_svg pSvg, const char* pTag, const char* pTagEnd, const xge_svg_style_t* pParentStyle, int* pDefIndex)
{
	char sId[XGE_SVG_ATTR_MAX];
	char sDefId[XGE_SVG_ATTR_MAX];
	char sValue[XGE_SVG_ATTR_MAX];
	xge_svg_pattern_t* pPattern;
	float fX;
	float fY;
	float fW;
	float fH;
	float fFontSize;
	int iPatternIndex;
	int iDefIndex;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pTag == NULL) || (pTagEnd == NULL) || (pDefIndex == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pDefIndex = -1;
	if ( !__xgeSvgAttrCopy(pTag, pTagEnd, "id", sId, sizeof(sId)) || (sId[0] == '\0') ) {
		return XGE_OK;
	}
	{
		int iDefIdLen = snprintf(sDefId, sizeof(sDefId), "__pattern__:%s", sId);
		if ( (iDefIdLen < 0) || (iDefIdLen >= (int)sizeof(sDefId)) ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
	}
	iRet = __xgeSvgPatternGetOrCreate(pSvg, sId, &iPatternIndex);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = __xgeSvgDefGetOrCreate(pSvg, sDefId, &iDefIndex);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pPattern = &pSvg->pPatterns[iPatternIndex];
	pPattern->iDefIndex = iDefIndex;
	pSvg->pDefs[iDefIndex].bPatternContent = 1;
	fFontSize = pParentStyle != NULL ? pParentStyle->fFontSize : XGE_SVG_DEFAULT_FONT_SIZE;
	xrtFree(pPattern->sHrefId);
	pPattern->sHrefId = NULL;
	pPattern->iFlags = 0;
	pPattern->tRect.fX = 0.0f;
	pPattern->tRect.fY = 0.0f;
	pPattern->tRect.fW = 0.0f;
	pPattern->tRect.fH = 0.0f;
	pPattern->iUnits = XGE_SVG_PATTERN_OBJECT_BOUNDING_BOX;
	pPattern->iContentUnits = XGE_SVG_PATTERN_USER_SPACE;
	if ( __xgeSvgHrefIdCopy(pTag, pTagEnd, sValue, sizeof(sValue)) ) {
		pPattern->sHrefId = __xgeStrDup(sValue);
		if ( pPattern->sHrefId == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
	}
	if ( __xgeSvgAttrCopy(pTag, pTagEnd, "patternUnits", sValue, sizeof(sValue)) ) {
		int iUnits;
		if ( __xgeSvgParsePatternUnits(sValue, &iUnits) ) {
			pPattern->iUnits = iUnits;
		}
		pPattern->iFlags |= XGE_SVG_PATTERN_HAS_UNITS;
	}
	if ( __xgeSvgAttrCopy(pTag, pTagEnd, "patternContentUnits", sValue, sizeof(sValue)) ) {
		int iUnits;
		if ( __xgeSvgParsePatternUnits(sValue, &iUnits) ) {
			pPattern->iContentUnits = iUnits;
		}
		pPattern->iFlags |= XGE_SVG_PATTERN_HAS_CONTENT_UNITS;
	}
	if ( __xgeSvgPatternLengthCopy(pSvg, pTag, pTagEnd, "x", pPattern->iUnits, XGE_SVG_LENGTH_BASIS_X, fFontSize, &pPattern->tRect.fX) ) {
		pPattern->iFlags |= XGE_SVG_PATTERN_HAS_X;
	}
	if ( __xgeSvgPatternLengthCopy(pSvg, pTag, pTagEnd, "y", pPattern->iUnits, XGE_SVG_LENGTH_BASIS_Y, fFontSize, &pPattern->tRect.fY) ) {
		pPattern->iFlags |= XGE_SVG_PATTERN_HAS_Y;
	}
	if ( __xgeSvgPatternLengthCopy(pSvg, pTag, pTagEnd, "width", pPattern->iUnits, XGE_SVG_LENGTH_BASIS_X, fFontSize, &pPattern->tRect.fW) ) {
		pPattern->iFlags |= XGE_SVG_PATTERN_HAS_WIDTH;
	}
	if ( __xgeSvgPatternLengthCopy(pSvg, pTag, pTagEnd, "height", pPattern->iUnits, XGE_SVG_LENGTH_BASIS_Y, fFontSize, &pPattern->tRect.fH) ) {
		pPattern->iFlags |= XGE_SVG_PATTERN_HAS_HEIGHT;
	}
	pPattern->tTransform = __xgeSvgMatrixIdentity();
	if ( __xgeSvgAttrCopy(pTag, pTagEnd, "patternTransform", sValue, sizeof(sValue)) ) {
		if ( __xgeSvgParseTransform(sValue, &pPattern->tTransform) ) {
			pPattern->iFlags |= XGE_SVG_PATTERN_HAS_TRANSFORM;
		}
	}
	__xgeSvgDefShapesClear(&pSvg->pDefs[iDefIndex]);
	pSvg->pDefs[iDefIndex].bOverflowVisible = 0;
	pSvg->pDefs[iDefIndex].bHasViewBox = 0;
	pSvg->pDefs[iDefIndex].iAspectAlignX = XGE_SVG_ASPECT_ALIGN_MID;
	pSvg->pDefs[iDefIndex].iAspectAlignY = XGE_SVG_ASPECT_ALIGN_MID;
	pSvg->pDefs[iDefIndex].iAspectMeetOrSlice = XGE_SVG_ASPECT_MEET;
	if ( __xgeSvgAttrCopy(pTag, pTagEnd, "viewBox", sValue, sizeof(sValue)) &&
	     __xgeSvgParseFourFloats(sValue, &fX, &fY, &fW, &fH) && (fW > 0.0f) && (fH > 0.0f) ) {
		pSvg->pDefs[iDefIndex].tViewBox.fX = fX;
		pSvg->pDefs[iDefIndex].tViewBox.fY = fY;
		pSvg->pDefs[iDefIndex].tViewBox.fW = fW;
		pSvg->pDefs[iDefIndex].tViewBox.fH = fH;
		pSvg->pDefs[iDefIndex].bHasViewBox = 1;
		pPattern->iFlags |= XGE_SVG_PATTERN_HAS_VIEWBOX;
	}
	if ( __xgeSvgAttrCopy(pTag, pTagEnd, "preserveAspectRatio", sValue, sizeof(sValue)) ) {
		__xgeSvgParsePreserveAspectRatioFields(sValue,
			&pSvg->pDefs[iDefIndex].iAspectAlignX,
			&pSvg->pDefs[iDefIndex].iAspectAlignY,
			&pSvg->pDefs[iDefIndex].iAspectMeetOrSlice);
		pPattern->iFlags |= XGE_SVG_PATTERN_HAS_ASPECT;
	}
	if ( __xgeSvgOverflowIsSpecified(pTag, pTagEnd) ) {
		pSvg->pDefs[iDefIndex].bOverflowVisible = __xgeSvgOverflowIsVisible(pTag, pTagEnd) ? 1 : 0;
		pPattern->iFlags |= XGE_SVG_PATTERN_HAS_OVERFLOW;
	}
	*pDefIndex = iDefIndex;
	return XGE_OK;
}

static int __xgeSvgParseClipPath(xge_svg pSvg, const char* pTag, const char* pTagEnd, int* pClipIndex)
{
	char sId[XGE_SVG_ATTR_MAX];
	char sValue[XGE_SVG_ATTR_MAX];
	int iIndex;
	int iRet;
	int i;

	if ( !__xgeSvgValid(pSvg) || (pTag == NULL) || (pTagEnd == NULL) || (pClipIndex == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pClipIndex = -1;
	if ( !__xgeSvgAttrCopy(pTag, pTagEnd, "id", sId, sizeof(sId)) || (sId[0] == '\0') ) {
		return XGE_OK;
	}
	iRet = __xgeSvgClipPathGetOrCreate(pSvg, sId, &iIndex);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	xrtFree(pSvg->pClipPaths[iIndex].pRects);
	pSvg->pClipPaths[iIndex].pRects = NULL;
	pSvg->pClipPaths[iIndex].iRectCount = 0;
	pSvg->pClipPaths[iIndex].iRectCapacity = 0;
	for ( i = 0; i < pSvg->pClipPaths[iIndex].iShapeCount; i++ ) {
		xgeShapeExDestroy(pSvg->pClipPaths[iIndex].pShapes[i]);
	}
	xrtFree(pSvg->pClipPaths[iIndex].pShapes);
	pSvg->pClipPaths[iIndex].pShapes = NULL;
	pSvg->pClipPaths[iIndex].iShapeCount = 0;
	pSvg->pClipPaths[iIndex].iShapeCapacity = 0;
	pSvg->pClipPaths[iIndex].iUnits = XGE_SVG_CLIP_USER_SPACE;
	pSvg->pClipPaths[iIndex].bHasRect = 0;
	pSvg->pClipPaths[iIndex].bHasShape = 0;
	if ( __xgeSvgAttrCopy(pTag, pTagEnd, "clipPathUnits", sValue, sizeof(sValue)) ) {
		int iUnits;
		if ( __xgeSvgParseUserObjectUnits(sValue, XGE_SVG_CLIP_USER_SPACE, XGE_SVG_CLIP_OBJECT_BOUNDING_BOX, &iUnits) ) {
			pSvg->pClipPaths[iIndex].iUnits = iUnits;
		}
	}
	*pClipIndex = iIndex;
	return XGE_OK;
}

static int __xgeSvgParseClipRect(xge_svg pSvg, int iClipIndex, const char* pTag, const char* pTagEnd, xge_shape_ex_matrix_t tParentTransform, const xge_svg_style_t* pParentStyle)
{
	xge_rect_t tRect;
	xge_svg_style_t tStyle;

	(void)tParentTransform;
	if ( !__xgeSvgValid(pSvg) || (iClipIndex < 0) || (iClipIndex >= pSvg->iClipPathCount) || (pTag == NULL) || (pTagEnd == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( !__xgeSvgTagNameEquals(pTag, "rect") ) {
		return XGE_OK;
	}
	tStyle = (pParentStyle != NULL) ? __xgeSvgStyleResolve(pSvg, pParentStyle, pTag, pTagEnd) : __xgeSvgStyleDefault();
	if ( !tStyle.bVisible || !tStyle.bVisibility ) {
		return XGE_OK;
	}
	if ( pSvg->pClipPaths[iClipIndex].iUnits == XGE_SVG_CLIP_OBJECT_BOUNDING_BOX ) {
		tRect.fX = __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "x", 1.0f, tStyle.fFontSize, 0.0f);
		tRect.fY = __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "y", 1.0f, tStyle.fFontSize, 0.0f);
		tRect.fW = __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "width", 1.0f, tStyle.fFontSize, 0.0f);
		tRect.fH = __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "height", 1.0f, tStyle.fFontSize, 0.0f);
	} else {
		tRect.fX = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "x", XGE_SVG_LENGTH_BASIS_X, tStyle.fFontSize, 0.0f);
		tRect.fY = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "y", XGE_SVG_LENGTH_BASIS_Y, tStyle.fFontSize, 0.0f);
		tRect.fW = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "width", XGE_SVG_LENGTH_BASIS_X, tStyle.fFontSize, 0.0f);
		tRect.fH = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "height", XGE_SVG_LENGTH_BASIS_Y, tStyle.fFontSize, 0.0f);
	}
	if ( tRect.fW < 0.0f ) tRect.fW = 0.0f;
	if ( tRect.fH < 0.0f ) tRect.fH = 0.0f;
	tRect = __xgeSvgMatrixRectBounds(tStyle.tTransform, tRect);
	if ( tStyle.bViewportClip ) {
		tRect = __xgeSvgRectIntersect(tRect, tStyle.tViewportClip);
	}
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XGE_OK;
	}
	return __xgeSvgClipPathAddRect(&pSvg->pClipPaths[iClipIndex], tRect);
}

static int __xgeSvgParseClipShapeFinish(xge_svg pSvg, int iClipIndex, xge_shape_ex pShape, const xge_svg_style_t* pStyle, xge_shape_ex_matrix_t tTransform)
{
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (iClipIndex < 0) || (iClipIndex >= pSvg->iClipPathCount) || (pShape == NULL) ) {
		xgeShapeExDestroy(pShape);
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (pStyle != NULL) && (!pStyle->bVisible || !pStyle->bVisibility) ) {
		xgeShapeExDestroy(pShape);
		return XGE_OK;
	}
	xgeShapeExTransformSet(pShape, &tTransform);
	if ( !__xgeSvgApplyViewportClipToShape(pShape, pStyle) ) {
		xgeShapeExDestroy(pShape);
		return XGE_OK;
	}
	iRet = __xgeSvgClipPathAddShape(&pSvg->pClipPaths[iClipIndex], pShape);
	xgeShapeExDestroy(pShape);
	return iRet;
}

static int __xgeSvgUseTransformForDef(xge_svg pSvg, int iDefIndex, const xge_svg_style_t* pStyle, const char* pTag, const char* pTagEnd, xge_shape_ex_matrix_t tBaseTransform, xge_shape_ex_matrix_t* pTransform, int* pSkip)
{
	xge_shape_ex_matrix_t tUseTransform;
	float fX;
	float fY;
	float fW;
	float fH;
	int bHasWidth;
	int bHasHeight;

	if ( (pTransform == NULL) || (pSkip == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pTransform = tBaseTransform;
	*pSkip = 0;
	if ( !__xgeSvgValid(pSvg) || (iDefIndex < 0) || (iDefIndex >= pSvg->iDefCount) || (pStyle == NULL) || (pTag == NULL) || (pTagEnd == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	fX = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "x", XGE_SVG_LENGTH_BASIS_X, pStyle->fFontSize, 0.0f);
	fY = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "y", XGE_SVG_LENGTH_BASIS_Y, pStyle->fFontSize, 0.0f);
	fW = 0.0f;
	fH = 0.0f;
	bHasWidth = __xgeSvgAttrLengthCopyEx(pTag, pTagEnd, "width", __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_X), pStyle->fFontSize, &fW);
	bHasHeight = __xgeSvgAttrLengthCopyEx(pTag, pTagEnd, "height", __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_Y), pStyle->fFontSize, &fH);
	if ( pSvg->pDefs[iDefIndex].bHasViewBox ) {
		xge_rect_t tDst;
		xge_rect_t tViewport;

		if ( !bHasWidth ) {
			fW = pSvg->pDefs[iDefIndex].bHasWidth ? pSvg->pDefs[iDefIndex].fWidth : pSvg->pDefs[iDefIndex].tViewBox.fW;
		}
		if ( !bHasHeight ) {
			fH = pSvg->pDefs[iDefIndex].bHasHeight ? pSvg->pDefs[iDefIndex].fHeight : pSvg->pDefs[iDefIndex].tViewBox.fH;
		}
		if ( (fW <= 0.0f) || (fH <= 0.0f) ) {
			*pSkip = 1;
			return XGE_OK;
		}
		tDst.fX = fX;
		tDst.fY = fY;
		tDst.fW = fW;
		tDst.fH = fH;
		tViewport = __xgeSvgAspectViewport(pSvg->pDefs[iDefIndex].tViewBox, tDst,
			pSvg->pDefs[iDefIndex].iAspectAlignX,
			pSvg->pDefs[iDefIndex].iAspectAlignY,
			pSvg->pDefs[iDefIndex].iAspectMeetOrSlice);
		tUseTransform = __xgeSvgViewBoxMatrix(pSvg->pDefs[iDefIndex].tViewBox, tViewport);
		*pTransform = __xgeSvgMatrixMul(*pTransform, tUseTransform);
	} else if ( (fabsf(fX) > 0.0f) || (fabsf(fY) > 0.0f) ) {
		tUseTransform = __xgeSvgMatrixTranslate(fX, fY);
		*pTransform = __xgeSvgMatrixMul(*pTransform, tUseTransform);
	}
	return XGE_OK;
}

static int __xgeSvgAddUseShapesToClipPath(xge_svg pSvg, int iClipIndex, int iDefIndex, const xge_svg_style_t* pStyle, xge_shape_ex_matrix_t tUseTransform)
{
	int i;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (iClipIndex < 0) || (iClipIndex >= pSvg->iClipPathCount) || (iDefIndex < 0) || (iDefIndex >= pSvg->iDefCount) || (pStyle == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < pSvg->pDefs[iDefIndex].iShapeCount; i++ ) {
		xge_shape_ex pClone;
		xge_shape_ex_matrix_t tShapeTransform;

		iRet = xgeShapeExClone(pSvg->pDefs[iDefIndex].pShapes[i], &pClone);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		tShapeTransform = __xgeSvgMatrixMul(tUseTransform, pClone->tTransform);
		iRet = xgeShapeExTransformSet(pClone, &tShapeTransform);
		if ( iRet == XGE_OK ) {
			iRet = __xgeSvgClipPathAddShape(&pSvg->pClipPaths[iClipIndex], pClone);
		}
		xgeShapeExDestroy(pClone);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	return XGE_OK;
}

static int __xgeSvgMaskSuppressGradientPaint(xge_shape_ex pShape)
{
	int iFillType;
	int iStrokeType;
	int iRet;

	if ( pShape == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = xgeShapeExFillTypeGet(pShape, &iFillType);
	if ( iRet != XGE_OK ) return iRet;
	if ( (iFillType == XGE_SHAPE_EX_PAINT_LINEAR_GRADIENT) ||
	     (iFillType == XGE_SHAPE_EX_PAINT_RADIAL_GRADIENT) ) {
		iRet = xgeShapeExFillColor(pShape, XGE_COLOR_RGBA(0, 0, 0, 0));
		if ( iRet != XGE_OK ) return iRet;
	}
	iRet = xgeShapeExStrokeTypeGet(pShape, &iStrokeType);
	if ( iRet != XGE_OK ) return iRet;
	if ( (iStrokeType == XGE_SHAPE_EX_PAINT_LINEAR_GRADIENT) ||
	     (iStrokeType == XGE_SHAPE_EX_PAINT_RADIAL_GRADIENT) ) {
		iRet = xgeShapeExStrokeColor(pShape, XGE_COLOR_RGBA(0, 0, 0, 0));
		if ( iRet != XGE_OK ) return iRet;
	}
	return XGE_OK;
}

static int __xgeSvgAddUseShapesToMask(xge_svg pSvg, int iMaskIndex, int iDefIndex, const xge_svg_style_t* pStyle, xge_shape_ex_matrix_t tUseTransform)
{
	int i;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (iMaskIndex < 0) || (iMaskIndex >= pSvg->iMaskCount) || (iDefIndex < 0) || (iDefIndex >= pSvg->iDefCount) || (pStyle == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < pSvg->pDefs[iDefIndex].iShapeCount; i++ ) {
		xge_shape_ex pClone;
		xge_shape_ex_matrix_t tShapeTransform;
		const xge_svg_style_t* pSourceStyle;
		xge_svg_style_t tItemStyle;

		iRet = xgeShapeExClone(pSvg->pDefs[iDefIndex].pShapes[i], &pClone);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		pSourceStyle = pSvg->pDefs[iDefIndex].pShapeStyles != NULL ?
			&pSvg->pDefs[iDefIndex].pShapeStyles[i] : NULL;
		tItemStyle = *pStyle;
		__xgeSvgApplyUsePaintToShape(pSvg, pClone, pSourceStyle, pStyle, &tItemStyle);
		iRet = __xgeSvgMaskSuppressGradientPaint(pClone);
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pClone);
			return iRet;
		}
		tShapeTransform = __xgeSvgMatrixMul(tUseTransform, pClone->tTransform);
		iRet = xgeShapeExTransformSet(pClone, &tShapeTransform);
		if ( (iRet == XGE_OK) && pStyle->bFillRuleSet ) {
			iRet = xgeShapeExFillRule(pClone, pStyle->iFillRule);
		}
		if ( iRet == XGE_OK ) {
			iRet = xgeShapeExOpacity(pClone, pClone->fOpacity * pStyle->fOpacity);
		}
		if ( (iRet == XGE_OK) && (!pStyle->bVisible || !pStyle->bVisibility) ) {
			iRet = xgeShapeExVisible(pClone, 0);
		}
		if ( iRet == XGE_OK ) {
			iRet = __xgeSvgMaskAddShape(&pSvg->pMasks[iMaskIndex], pClone);
		}
		xgeShapeExDestroy(pClone);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	return XGE_OK;
}

static int __xgeSvgParseClipShape(xge_svg pSvg, int iClipIndex, const char* pTag, const char* pTagEnd, xge_shape_ex_matrix_t tParentTransform, const xge_svg_style_t* pParentStyle)
{
	char sValue[XGE_SVG_ATTR_MAX];
	xge_svg_style_t tStyle;
	xge_shape_ex_matrix_t tTransform;
	xge_shape_ex pShape;
	int bObjectBounds;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (iClipIndex < 0) || (iClipIndex >= pSvg->iClipPathCount) || (pTag == NULL) || (pTagEnd == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)tParentTransform;
	if ( __xgeSvgTagNameEquals(pTag, "rect") ) {
		iRet = __xgeSvgParseClipRect(pSvg, iClipIndex, pTag, pTagEnd, tParentTransform, pParentStyle);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	tStyle = (pParentStyle != NULL) ? __xgeSvgStyleResolve(pSvg, pParentStyle, pTag, pTagEnd) : __xgeSvgStyleDefault();
	if ( !tStyle.bVisible || !tStyle.bVisibility ) {
		return XGE_OK;
	}
	tTransform = tStyle.tTransform;
	bObjectBounds = (pSvg->pClipPaths[iClipIndex].iUnits == XGE_SVG_CLIP_OBJECT_BOUNDING_BOX);
	pShape = NULL;
	if ( __xgeSvgTagNameEquals(pTag, "use") ) {
		int iDefIndex;
		int bSkip;

		if ( !__xgeSvgHrefIdCopy(pTag, pTagEnd, sValue, sizeof(sValue)) ) {
			return XGE_OK;
		}
		iDefIndex = __xgeSvgDefFind(pSvg, sValue);
		if ( iDefIndex < 0 ) {
			return XGE_OK;
		}
		iRet = __xgeSvgUseTransformForDef(pSvg, iDefIndex, &tStyle, pTag, pTagEnd, tTransform, &tTransform, &bSkip);
		if ( (iRet != XGE_OK) || bSkip ) {
			return iRet;
		}
		return __xgeSvgAddUseShapesToClipPath(pSvg, iClipIndex, iDefIndex, &tStyle, tTransform);
	}
	if ( __xgeSvgTagNameEquals(pTag, "path") ) {
		iRet = __xgeSvgPathShapeCreate(pTag, pTagEnd, &pShape);
		if ( (iRet != XGE_OK) || (pShape == NULL) ) return iRet;
		return __xgeSvgParseClipShapeFinish(pSvg, iClipIndex, pShape, &tStyle, tTransform);
	}
	if ( __xgeSvgTagNameEquals(pTag, "rect") ) {
		float fX = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "x", 1.0f, tStyle.fFontSize, 0.0f) :
			__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "x", XGE_SVG_LENGTH_BASIS_X, tStyle.fFontSize, 0.0f);
		float fY = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "y", 1.0f, tStyle.fFontSize, 0.0f) :
			__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "y", XGE_SVG_LENGTH_BASIS_Y, tStyle.fFontSize, 0.0f);
		float fW = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "width", 1.0f, tStyle.fFontSize, 0.0f) :
			__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "width", XGE_SVG_LENGTH_BASIS_X, tStyle.fFontSize, 0.0f);
		float fH = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "height", 1.0f, tStyle.fFontSize, 0.0f) :
			__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "height", XGE_SVG_LENGTH_BASIS_Y, tStyle.fFontSize, 0.0f);
		float fRX = 0.0f;
		float fRY = 0.0f;
		int bHasRX = bObjectBounds ? __xgeSvgAttrLengthCopyEx(pTag, pTagEnd, "rx", 1.0f, tStyle.fFontSize, &fRX) :
			__xgeSvgAttrLengthCopyEx(pTag, pTagEnd, "rx", __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_X), tStyle.fFontSize, &fRX);
		int bHasRY = bObjectBounds ? __xgeSvgAttrLengthCopyEx(pTag, pTagEnd, "ry", 1.0f, tStyle.fFontSize, &fRY) :
			__xgeSvgAttrLengthCopyEx(pTag, pTagEnd, "ry", __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_Y), tStyle.fFontSize, &fRY);

		__xgeSvgNormalizeRectRadii(&fRX, &fRY, &bHasRX, &bHasRY);
		if ( (fW <= 0.0f) || (fH <= 0.0f) ) {
			return XGE_OK;
		}
		iRet = xgeShapeExCreate(&pShape);
		if ( iRet != XGE_OK ) return iRet;
		iRet = __xgeShapeExAppendSvgRect(pShape, fX, fY, fW, fH, fRX, fRY);
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pShape);
			return iRet;
		}
		return __xgeSvgParseClipShapeFinish(pSvg, iClipIndex, pShape, &tStyle, tTransform);
	}
	if ( __xgeSvgTagNameEquals(pTag, "circle") || __xgeSvgTagNameEquals(pTag, "ellipse") ) {
		float fCX = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "cx", 1.0f, tStyle.fFontSize, 0.0f) :
			__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "cx", XGE_SVG_LENGTH_BASIS_X, tStyle.fFontSize, 0.0f);
		float fCY = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "cy", 1.0f, tStyle.fFontSize, 0.0f) :
			__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "cy", XGE_SVG_LENGTH_BASIS_Y, tStyle.fFontSize, 0.0f);
		float fRX;
		float fRY;

		__xgeSvgCircleEllipseRadii(pSvg, pTag, pTagEnd, bObjectBounds, tStyle.fFontSize, &fRX, &fRY);
		if ( (fRX <= 0.0f) || (fRY <= 0.0f) ) {
			return XGE_OK;
		}
		iRet = xgeShapeExCreate(&pShape);
		if ( iRet != XGE_OK ) return iRet;
		iRet = __xgeShapeExAppendSvgEllipse(pShape, fCX, fCY, fRX, fRY);
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pShape);
			return iRet;
		}
		return __xgeSvgParseClipShapeFinish(pSvg, iClipIndex, pShape, &tStyle, tTransform);
	}
	if ( __xgeSvgTagNameEquals(pTag, "line") ) {
		float fX1 = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "x1", 1.0f, tStyle.fFontSize, 0.0f) :
			__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "x1", XGE_SVG_LENGTH_BASIS_X, tStyle.fFontSize, 0.0f);
		float fY1 = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "y1", 1.0f, tStyle.fFontSize, 0.0f) :
			__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "y1", XGE_SVG_LENGTH_BASIS_Y, tStyle.fFontSize, 0.0f);
		float fX2 = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "x2", 1.0f, tStyle.fFontSize, 0.0f) :
			__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "x2", XGE_SVG_LENGTH_BASIS_X, tStyle.fFontSize, 0.0f);
		float fY2 = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "y2", 1.0f, tStyle.fFontSize, 0.0f) :
			__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "y2", XGE_SVG_LENGTH_BASIS_Y, tStyle.fFontSize, 0.0f);

		iRet = xgeShapeExCreate(&pShape);
		if ( iRet != XGE_OK ) return iRet;
		iRet = xgeShapeExAppendLine(pShape, fX1, fY1, fX2, fY2);
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pShape);
			return iRet;
		}
		return __xgeSvgParseClipShapeFinish(pSvg, iClipIndex, pShape, &tStyle, tTransform);
	}
	if ( __xgeSvgTagNameEquals(pTag, "polygon") || __xgeSvgTagNameEquals(pTag, "polyline") ) {
		int bClose = __xgeSvgTagNameEquals(pTag, "polygon");

		if ( !__xgeSvgAttrCopy(pTag, pTagEnd, "points", sValue, sizeof(sValue)) ) {
			return XGE_OK;
		}
		iRet = __xgeSvgCreatePointListShape(sValue, bClose, &pShape);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		if ( pShape == NULL ) {
			return XGE_OK;
		}
		return __xgeSvgParseClipShapeFinish(pSvg, iClipIndex, pShape, &tStyle, tTransform);
	}
	return XGE_OK;
}

static int __xgeSvgParseMask(xge_svg pSvg, const char* pTag, const char* pTagEnd, int* pMaskIndex)
{
	char sId[XGE_SVG_ATTR_MAX];
	char sValue[XGE_SVG_ATTR_MAX];
	int iIndex;
	int i;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pTag == NULL) || (pTagEnd == NULL) || (pMaskIndex == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pMaskIndex = -1;
	if ( !__xgeSvgAttrCopy(pTag, pTagEnd, "id", sId, sizeof(sId)) || (sId[0] == '\0') ) {
		return XGE_OK;
	}
	iRet = __xgeSvgMaskGetOrCreate(pSvg, sId, &iIndex);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	for ( i = 0; i < pSvg->pMasks[iIndex].iShapeCount; i++ ) {
		xgeShapeExDestroy(pSvg->pMasks[iIndex].pShapes[i].pShape);
	}
	xrtFree(pSvg->pMasks[iIndex].pShapes);
	pSvg->pMasks[iIndex].pShapes = NULL;
	pSvg->pMasks[iIndex].iShapeCount = 0;
	pSvg->pMasks[iIndex].iShapeCapacity = 0;
	pSvg->pMasks[iIndex].iUnits = XGE_SVG_MASK_OBJECT_BOUNDING_BOX;
	pSvg->pMasks[iIndex].iContentUnits = XGE_SVG_MASK_USER_SPACE;
	pSvg->pMasks[iIndex].iType = XGE_SVG_MASK_TYPE_LUMINANCE;
	pSvg->pMasks[iIndex].tRegion = (xge_rect_t){-0.1f, -0.1f, 1.2f, 1.2f};
	{
		xge_svg_style_t tMaskStyle = __xgeSvgStyleResolve(pSvg, NULL, pTag, pTagEnd);
		pSvg->pMasks[iIndex].iType = tMaskStyle.iMaskType;
	}
	if ( __xgeSvgAttrCopy(pTag, pTagEnd, "maskUnits", sValue, sizeof(sValue)) ) {
		int iUnits;
		if ( __xgeSvgParseUserObjectUnits(sValue, XGE_SVG_MASK_USER_SPACE, XGE_SVG_MASK_OBJECT_BOUNDING_BOX, &iUnits) ) {
			pSvg->pMasks[iIndex].iUnits = iUnits;
		}
	}
	if ( pSvg->pMasks[iIndex].iUnits == XGE_SVG_MASK_OBJECT_BOUNDING_BOX ) {
		xge_svg_style_t tMaskStyle = __xgeSvgStyleDefault();

		pSvg->pMasks[iIndex].tRegion.fX = __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "x", 1.0f, tMaskStyle.fFontSize, -0.1f);
		pSvg->pMasks[iIndex].tRegion.fY = __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "y", 1.0f, tMaskStyle.fFontSize, -0.1f);
		pSvg->pMasks[iIndex].tRegion.fW = __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "width", 1.0f, tMaskStyle.fFontSize, 1.2f);
		pSvg->pMasks[iIndex].tRegion.fH = __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "height", 1.0f, tMaskStyle.fFontSize, 1.2f);
	} else {
		xge_svg_style_t tMaskStyle = __xgeSvgStyleDefault();
		float fRefX = __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_X);
		float fRefY = __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_Y);

		pSvg->pMasks[iIndex].tRegion.fX = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "x", XGE_SVG_LENGTH_BASIS_X, tMaskStyle.fFontSize, -0.1f * fRefX);
		pSvg->pMasks[iIndex].tRegion.fY = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "y", XGE_SVG_LENGTH_BASIS_Y, tMaskStyle.fFontSize, -0.1f * fRefY);
		pSvg->pMasks[iIndex].tRegion.fW = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "width", XGE_SVG_LENGTH_BASIS_X, tMaskStyle.fFontSize, 1.2f * fRefX);
		pSvg->pMasks[iIndex].tRegion.fH = __xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "height", XGE_SVG_LENGTH_BASIS_Y, tMaskStyle.fFontSize, 1.2f * fRefY);
	}
	if ( pSvg->pMasks[iIndex].tRegion.fW < 0.0f ) pSvg->pMasks[iIndex].tRegion.fW = 0.0f;
	if ( pSvg->pMasks[iIndex].tRegion.fH < 0.0f ) pSvg->pMasks[iIndex].tRegion.fH = 0.0f;
	if ( __xgeSvgAttrCopy(pTag, pTagEnd, "maskContentUnits", sValue, sizeof(sValue)) ) {
		int iUnits;
		if ( __xgeSvgParseUserObjectUnits(sValue, XGE_SVG_MASK_USER_SPACE, XGE_SVG_MASK_OBJECT_BOUNDING_BOX, &iUnits) ) {
			pSvg->pMasks[iIndex].iContentUnits = iUnits;
		}
	}
	*pMaskIndex = iIndex;
	return XGE_OK;
}

static int __xgeSvgParseMaskShapeFinish(xge_svg pSvg, int iMaskIndex, xge_shape_ex pShape, const xge_svg_style_t* pStyle, xge_shape_ex_matrix_t tTransform)
{
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (iMaskIndex < 0) || (iMaskIndex >= pSvg->iMaskCount) || (pShape == NULL) ) {
		xgeShapeExDestroy(pShape);
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (pStyle != NULL) && (!pStyle->bVisible || !pStyle->bVisibility) ) {
		xgeShapeExDestroy(pShape);
		return XGE_OK;
	}
	if ( pStyle != NULL ) {
		__xgeSvgApplyShapeStyle(pSvg, pShape, pStyle);
		xgeShapeExFillRule(pShape, pStyle->iFillRule);
	}
	iRet = __xgeSvgMaskSuppressGradientPaint(pShape);
	if ( iRet != XGE_OK ) {
		xgeShapeExDestroy(pShape);
		return iRet;
	}
	xgeShapeExTransformSet(pShape, &tTransform);
	if ( !__xgeSvgApplyViewportClipToShape(pShape, pStyle) ) {
		xgeShapeExDestroy(pShape);
		return XGE_OK;
	}
	iRet = __xgeSvgMaskAddShape(&pSvg->pMasks[iMaskIndex], pShape);
	xgeShapeExDestroy(pShape);
	return iRet;
}

static int __xgeSvgParseMaskShape(xge_svg pSvg, int iMaskIndex, const char* pTag, const char* pTagEnd, xge_shape_ex_matrix_t tParentTransform, const xge_svg_style_t* pParentStyle)
{
	char sValue[XGE_SVG_ATTR_MAX];
	xge_svg_style_t tStyle;
	xge_shape_ex_matrix_t tTransform;
	xge_shape_ex pShape;
	int bObjectBounds;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (iMaskIndex < 0) || (iMaskIndex >= pSvg->iMaskCount) || (pTag == NULL) || (pTagEnd == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)tParentTransform;
	tStyle = (pParentStyle != NULL) ? __xgeSvgStyleResolveMaskContent(pSvg, pParentStyle, pTag, pTagEnd) : __xgeSvgStyleDefault();
	if ( !tStyle.bVisible || !tStyle.bVisibility ) {
		return XGE_OK;
	}
	tTransform = tStyle.tTransform;
	bObjectBounds = (pSvg->pMasks[iMaskIndex].iContentUnits == XGE_SVG_MASK_OBJECT_BOUNDING_BOX);
	pShape = NULL;
	if ( __xgeSvgTagNameEquals(pTag, "use") ) {
		int iDefIndex;
		int bSkip;

		if ( !__xgeSvgHrefIdCopy(pTag, pTagEnd, sValue, sizeof(sValue)) ) {
			return XGE_OK;
		}
		iDefIndex = __xgeSvgDefFind(pSvg, sValue);
		if ( iDefIndex < 0 ) {
			return XGE_OK;
		}
		iRet = __xgeSvgUseTransformForDef(pSvg, iDefIndex, &tStyle, pTag, pTagEnd, tTransform, &tTransform, &bSkip);
		if ( (iRet != XGE_OK) || bSkip ) {
			return iRet;
		}
		return __xgeSvgAddUseShapesToMask(pSvg, iMaskIndex, iDefIndex, &tStyle, tTransform);
	}
	if ( __xgeSvgTagNameEquals(pTag, "rect") ) {
		float fRX = 0.0f;
		float fRY = 0.0f;
		int bHasRX = bObjectBounds ? __xgeSvgAttrLengthCopyEx(pTag, pTagEnd, "rx", 1.0f, tStyle.fFontSize, &fRX) :
			__xgeSvgAttrLengthCopyEx(pTag, pTagEnd, "rx", __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_X), tStyle.fFontSize, &fRX);
		int bHasRY = bObjectBounds ? __xgeSvgAttrLengthCopyEx(pTag, pTagEnd, "ry", 1.0f, tStyle.fFontSize, &fRY) :
			__xgeSvgAttrLengthCopyEx(pTag, pTagEnd, "ry", __xgeSvgLengthPercentRef(pSvg, XGE_SVG_LENGTH_BASIS_Y), tStyle.fFontSize, &fRY);

		__xgeSvgNormalizeRectRadii(&fRX, &fRY, &bHasRX, &bHasRY);
		{
			float fX = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "x", 1.0f, tStyle.fFontSize, 0.0f) :
				__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "x", XGE_SVG_LENGTH_BASIS_X, tStyle.fFontSize, 0.0f);
			float fY = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "y", 1.0f, tStyle.fFontSize, 0.0f) :
				__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "y", XGE_SVG_LENGTH_BASIS_Y, tStyle.fFontSize, 0.0f);
			float fW = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "width", 1.0f, tStyle.fFontSize, 0.0f) :
				__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "width", XGE_SVG_LENGTH_BASIS_X, tStyle.fFontSize, 0.0f);
			float fH = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "height", 1.0f, tStyle.fFontSize, 0.0f) :
				__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "height", XGE_SVG_LENGTH_BASIS_Y, tStyle.fFontSize, 0.0f);

			if ( (fW <= 0.0f) || (fH <= 0.0f) ) {
				return XGE_OK;
			}
			iRet = xgeShapeExCreate(&pShape);
			if ( iRet != XGE_OK ) return iRet;
			iRet = __xgeShapeExAppendSvgRect(pShape, fX, fY, fW, fH, fRX, fRY);
		}
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pShape);
			return iRet;
		}
		return __xgeSvgParseMaskShapeFinish(pSvg, iMaskIndex, pShape, &tStyle, tTransform);
	}
	if ( __xgeSvgTagNameEquals(pTag, "path") ) {
		iRet = __xgeSvgPathShapeCreate(pTag, pTagEnd, &pShape);
		if ( (iRet != XGE_OK) || (pShape == NULL) ) return iRet;
		return __xgeSvgParseMaskShapeFinish(pSvg, iMaskIndex, pShape, &tStyle, tTransform);
	}
	if ( __xgeSvgTagNameEquals(pTag, "circle") || __xgeSvgTagNameEquals(pTag, "ellipse") ) {
		float fCX = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "cx", 1.0f, tStyle.fFontSize, 0.0f) :
			__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "cx", XGE_SVG_LENGTH_BASIS_X, tStyle.fFontSize, 0.0f);
		float fCY = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "cy", 1.0f, tStyle.fFontSize, 0.0f) :
			__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "cy", XGE_SVG_LENGTH_BASIS_Y, tStyle.fFontSize, 0.0f);
		float fRX;
		float fRY;

		__xgeSvgCircleEllipseRadii(pSvg, pTag, pTagEnd, bObjectBounds, tStyle.fFontSize, &fRX, &fRY);
		if ( (fRX <= 0.0f) || (fRY <= 0.0f) ) {
			return XGE_OK;
		}
		iRet = xgeShapeExCreate(&pShape);
		if ( iRet != XGE_OK ) return iRet;
		iRet = __xgeShapeExAppendSvgEllipse(pShape, fCX, fCY, fRX, fRY);
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pShape);
			return iRet;
		}
		return __xgeSvgParseMaskShapeFinish(pSvg, iMaskIndex, pShape, &tStyle, tTransform);
	}
	if ( __xgeSvgTagNameEquals(pTag, "line") ) {
		float fX1 = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "x1", 1.0f, tStyle.fFontSize, 0.0f) :
			__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "x1", XGE_SVG_LENGTH_BASIS_X, tStyle.fFontSize, 0.0f);
		float fY1 = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "y1", 1.0f, tStyle.fFontSize, 0.0f) :
			__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "y1", XGE_SVG_LENGTH_BASIS_Y, tStyle.fFontSize, 0.0f);
		float fX2 = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "x2", 1.0f, tStyle.fFontSize, 0.0f) :
			__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "x2", XGE_SVG_LENGTH_BASIS_X, tStyle.fFontSize, 0.0f);
		float fY2 = bObjectBounds ? __xgeSvgAttrLengthRefEx(pTag, pTagEnd, "y2", 1.0f, tStyle.fFontSize, 0.0f) :
			__xgeSvgAttrLengthEx(pSvg, pTag, pTagEnd, "y2", XGE_SVG_LENGTH_BASIS_Y, tStyle.fFontSize, 0.0f);

		iRet = xgeShapeExCreate(&pShape);
		if ( iRet != XGE_OK ) return iRet;
		iRet = xgeShapeExAppendLine(pShape, fX1, fY1, fX2, fY2);
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pShape);
			return iRet;
		}
		return __xgeSvgParseMaskShapeFinish(pSvg, iMaskIndex, pShape, &tStyle, tTransform);
	}
	if ( __xgeSvgTagNameEquals(pTag, "polygon") || __xgeSvgTagNameEquals(pTag, "polyline") ) {
		int bClose = __xgeSvgTagNameEquals(pTag, "polygon");

		if ( !__xgeSvgAttrCopy(pTag, pTagEnd, "points", sValue, sizeof(sValue)) ) {
			return XGE_OK;
		}
		iRet = __xgeSvgCreatePointListShape(sValue, bClose, &pShape);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		if ( pShape == NULL ) {
			return XGE_OK;
		}
		return __xgeSvgParseMaskShapeFinish(pSvg, iMaskIndex, pShape, &tStyle, tTransform);
	}
	return XGE_OK;
}

static int __xgeSvgDrawItemsClone(
	const xge_svg_draw_item_t* pSource,
	int iCount,
	xge_svg_draw_item_t** ppClone
);

static int __xgeSvgDrawItemClone(
	const xge_svg_draw_item_t* pSource,
	xge_svg_draw_item_t* pClone
)
{
	xge_shape_ex_scene_child_t tChild;
	int iRet;

	if ( (pSource == NULL) || (pClone == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pClone, 0, sizeof(*pClone));
	pClone->iId = pSource->iId;
	pClone->iType = pSource->iType;
	pClone->iBlend = pSource->iBlend;
	pClone->bBlendSet = pSource->bBlendSet;
	pClone->fPaintOpacity = pSource->fPaintOpacity;
	pClone->bPaintOpacity = pSource->bPaintOpacity;
	__xgeSvgPaintRuntimeStateCopy(pClone, pSource);
	iRet = __xgeSvgPaintRuntimeCompositionClone(pClone, pSource);
	if ( iRet != XGE_OK ) return iRet;

	if ( pSource->iType == XGE_SVG_DRAW_ITEM_SHAPE ) {
		if ( pSource->u.pShape == NULL ) return XGE_OK;
		return xgeShapeExClone(pSource->u.pShape, &pClone->u.pShape);
	}
	if ( pSource->iType == XGE_SVG_DRAW_ITEM_SCENE ) {
		if ( pSource->u.pScene == NULL ) return XGE_OK;
		iRet = xgeShapeExSceneClone(pSource->u.pScene, &pClone->u.pScene);
		if ( iRet != XGE_OK ) return iRet;
		if ( pSource->pIdentityShape != NULL ) {
			iRet = xgeShapeExSceneChildGetAt(pClone->u.pScene, 0, &tChild);
			if ( (iRet != XGE_OK) || (tChild.iType != XGE_SHAPE_EX_SCENE_CHILD_SHAPE) ||
			     (tChild.pShape == NULL) ) {
				return iRet != XGE_OK ? iRet : XGE_ERROR_INVALID_STATE;
			}
			pClone->pIdentityShape = tChild.pShape;
		}
		return XGE_OK;
	}
	if ( pSource->iType == XGE_SVG_DRAW_ITEM_FILTER ) {
		const xge_svg_filter_effect_t* pSourceEffect = pSource->u.pFilter;
		xge_svg_filter_effect_t* pEffect;

		if ( pSourceEffect == NULL ) return XGE_OK;
		pEffect = (xge_svg_filter_effect_t*)xrtCalloc(1, sizeof(*pEffect));
		if ( pEffect == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
		*pEffect = *pSourceEffect;
		pEffect->pItems = NULL;
		pEffect->iItemCount = 0;
		pClone->u.pFilter = pEffect;
		iRet = __xgeSvgDrawItemsClone(
			pSourceEffect->pItems, pSourceEffect->iItemCount, &pEffect->pItems
		);
		if ( iRet != XGE_OK ) return iRet;
		pEffect->iItemCount = pSourceEffect->iItemCount;
		return XGE_OK;
	}
	if ( pSource->iType == XGE_SVG_DRAW_ITEM_GROUP ) {
		const xge_svg_group_item_t* pSourceGroup = pSource->u.pGroup;
		xge_svg_group_item_t* pGroup;

		if ( pSourceGroup == NULL ) return XGE_OK;
		pGroup = (xge_svg_group_item_t*)xrtCalloc(1, sizeof(*pGroup));
		if ( pGroup == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
		*pGroup = *pSourceGroup;
		pGroup->pItems = NULL;
		pGroup->pMaskScene = NULL;
		pGroup->iItemCount = 0;
		pClone->u.pGroup = pGroup;
		iRet = __xgeSvgDrawItemsClone(
			pSourceGroup->pItems, pSourceGroup->iItemCount, &pGroup->pItems
		);
		if ( iRet != XGE_OK ) return iRet;
		pGroup->iItemCount = pSourceGroup->iItemCount;
		if ( pSourceGroup->pMaskScene != NULL ) {
			iRet = xgeShapeExSceneClone(pSourceGroup->pMaskScene, &pGroup->pMaskScene);
			if ( iRet != XGE_OK ) return iRet;
		}
		return XGE_OK;
	}
	if ( pSource->iType == XGE_SVG_DRAW_ITEM_TEXT ) {
		pClone->u.tText = pSource->u.tText;
		pClone->u.tText.sText = NULL;
		pClone->u.tText.pMaskScene = NULL;
		if ( pSource->u.tText.sText != NULL ) {
			pClone->u.tText.sText = __xgeStrDup(pSource->u.tText.sText);
			if ( pClone->u.tText.sText == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
		}
		if ( pSource->u.tText.pMaskScene != NULL ) {
			iRet = xgeShapeExSceneClone(
				pSource->u.tText.pMaskScene, &pClone->u.tText.pMaskScene
			);
			if ( iRet != XGE_OK ) return iRet;
		}
		return XGE_OK;
	}
	if ( pSource->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE ) {
		pClone->u.tImage = pSource->u.tImage;
		pClone->u.tImage.pSvg = NULL;
		pClone->u.tImage.pMaskScene = NULL;
		if ( pSource->u.tImage.pSvg != NULL ) {
			iRet = xgeSvgAddRef(pSource->u.tImage.pSvg);
			if ( iRet != XGE_OK ) return iRet;
			pClone->u.tImage.pSvg = pSource->u.tImage.pSvg;
		}
		if ( pSource->u.tImage.pMaskScene != NULL ) {
			iRet = xgeShapeExSceneClone(
				pSource->u.tImage.pMaskScene, &pClone->u.tImage.pMaskScene
			);
			if ( iRet != XGE_OK ) return iRet;
		}
		return XGE_OK;
	}
	if ( pSource->iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE ) {
		pClone->u.tRaster = pSource->u.tRaster;
		pClone->u.tRaster.pRaster = NULL;
		pClone->u.tRaster.pMaskScene = NULL;
		if ( pSource->u.tRaster.pRaster != NULL ) {
			iRet = __xgeSvgRasterImageAddRef(pSource->u.tRaster.pRaster);
			if ( iRet <= 0 ) return XGE_ERROR_INVALID_STATE;
			pClone->u.tRaster.pRaster = pSource->u.tRaster.pRaster;
		}
		if ( pSource->u.tRaster.pMaskScene != NULL ) {
			iRet = xgeShapeExSceneClone(
				pSource->u.tRaster.pMaskScene, &pClone->u.tRaster.pMaskScene
			);
			if ( iRet != XGE_OK ) return iRet;
		}
		return XGE_OK;
	}
	if ( pSource->iType == XGE_SVG_DRAW_ITEM_PENDING_USE ) {
		pClone->u.iPendingUseSerial = pSource->u.iPendingUseSerial;
		return XGE_OK;
	}
	return XGE_ERROR_UNSUPPORTED;
}

static int __xgeSvgDrawItemsClone(
	const xge_svg_draw_item_t* pSource,
	int iCount,
	xge_svg_draw_item_t** ppClone
)
{
	xge_svg_draw_item_t* pItems;
	int i;
	int iRet;

	if ( (ppClone == NULL) || (iCount < 0) || ((iCount > 0) && (pSource == NULL)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppClone = NULL;
	if ( iCount == 0 ) return XGE_OK;
	pItems = (xge_svg_draw_item_t*)xrtCalloc((size_t)iCount, sizeof(*pItems));
	if ( pItems == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	for ( i = 0; i < iCount; i++ ) {
		iRet = __xgeSvgDrawItemClone(&pSource[i], &pItems[i]);
		if ( iRet != XGE_OK ) {
			int j;

			for ( j = 0; j <= i; j++ ) __xgeSvgDrawItemReset(&pItems[j]);
			xrtFree(pItems);
			return iRet;
		}
	}
	*ppClone = pItems;
	return XGE_OK;
}

static int __xgeSvgCloneDefNames(xge_svg pSource, xge_svg pClone)
{
	int i;

	if ( pSource->iDefCount <= 0 ) return XGE_OK;
	pClone->pDefs = (xge_svg_def_t*)xrtCalloc(
		(size_t)pSource->iDefCount, sizeof(*pClone->pDefs)
	);
	if ( pClone->pDefs == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	pClone->iDefCount = pSource->iDefCount;
	pClone->iDefCapacity = pSource->iDefCount;
	for ( i = 0; i < pSource->iDefCount; i++ ) {
		const xge_svg_def_t* pSourceDef = &pSource->pDefs[i];
		xge_svg_def_t* pDef = &pClone->pDefs[i];

		memcpy(pDef->sGroupFilterId, pSourceDef->sGroupFilterId, sizeof(pDef->sGroupFilterId));
		pDef->tViewBox = pSourceDef->tViewBox;
		pDef->fWidth = pSourceDef->fWidth;
		pDef->fHeight = pSourceDef->fHeight;
		pDef->bPatternContent = pSourceDef->bPatternContent;
		pDef->bHasViewBox = pSourceDef->bHasViewBox;
		pDef->bHasWidth = pSourceDef->bHasWidth;
		pDef->bHasHeight = pSourceDef->bHasHeight;
		pDef->bOverflowVisible = pSourceDef->bOverflowVisible;
		pDef->iAspectAlignX = pSourceDef->iAspectAlignX;
		pDef->iAspectAlignY = pSourceDef->iAspectAlignY;
		pDef->iAspectMeetOrSlice = pSourceDef->iAspectMeetOrSlice;
		pDef->tGroupFilterTransform = pSourceDef->tGroupFilterTransform;
		if ( pSourceDef->sId != NULL ) {
			pDef->sId = __xgeStrDup(pSourceDef->sId);
			if ( pDef->sId == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
		}
	}
	return XGE_OK;
}

static int __xgeSvgCloneMasks(xge_svg pSource, xge_svg pClone)
{
	int i;
	int j;
	int iRet;

	if ( pSource->iMaskCount <= 0 ) return XGE_OK;
	pClone->pMasks = (xge_svg_mask_t*)xrtCalloc(
		(size_t)pSource->iMaskCount, sizeof(*pClone->pMasks)
	);
	if ( pClone->pMasks == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	pClone->iMaskCount = pSource->iMaskCount;
	pClone->iMaskCapacity = pSource->iMaskCount;
	for ( i = 0; i < pSource->iMaskCount; i++ ) {
		const xge_svg_mask_t* pSourceMask = &pSource->pMasks[i];
		xge_svg_mask_t* pMask = &pClone->pMasks[i];

		pMask->tRegion = pSourceMask->tRegion;
		pMask->iUnits = pSourceMask->iUnits;
		pMask->iContentUnits = pSourceMask->iContentUnits;
		pMask->iType = pSourceMask->iType;
		if ( pSourceMask->sId != NULL ) {
			pMask->sId = __xgeStrDup(pSourceMask->sId);
			if ( pMask->sId == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
		}
		if ( pSourceMask->iShapeCount <= 0 ) continue;
		pMask->pShapes = (xge_svg_mask_shape_t*)xrtCalloc(
			(size_t)pSourceMask->iShapeCount, sizeof(*pMask->pShapes)
		);
		if ( pMask->pShapes == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
		pMask->iShapeCount = pSourceMask->iShapeCount;
		pMask->iShapeCapacity = pSourceMask->iShapeCount;
		for ( j = 0; j < pSourceMask->iShapeCount; j++ ) {
			iRet = xgeShapeExClone(
				pSourceMask->pShapes[j].pShape, &pMask->pShapes[j].pShape
			);
			if ( iRet != XGE_OK ) return iRet;
		}
	}
	return XGE_OK;
}

static int __xgeSvgCloneFilters(xge_svg pSource, xge_svg pClone)
{
	int i;

	if ( pSource->iFilterCount <= 0 ) return XGE_OK;
	pClone->pFilters = (xge_svg_filter_t*)xrtCalloc(
		(size_t)pSource->iFilterCount, sizeof(*pClone->pFilters)
	);
	if ( pClone->pFilters == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	pClone->iFilterCount = pSource->iFilterCount;
	pClone->iFilterCapacity = pSource->iFilterCount;
	for ( i = 0; i < pSource->iFilterCount; i++ ) {
		const xge_svg_filter_t* pSourceFilter = &pSource->pFilters[i];
		xge_svg_filter_t* pFilter = &pClone->pFilters[i];

		pFilter->tRegion = pSourceFilter->tRegion;
		pFilter->iFilterUnits = pSourceFilter->iFilterUnits;
		pFilter->iUnits = pSourceFilter->iUnits;
		if ( pSourceFilter->sId != NULL ) {
			pFilter->sId = __xgeStrDup(pSourceFilter->sId);
			if ( pFilter->sId == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
		}
		if ( pSourceFilter->iPrimitiveCount <= 0 ) continue;
		pFilter->pPrimitives = (xge_svg_filter_primitive_t*)xrtMalloc(
			(size_t)pSourceFilter->iPrimitiveCount * sizeof(*pFilter->pPrimitives)
		);
		if ( pFilter->pPrimitives == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
		memcpy(
			pFilter->pPrimitives, pSourceFilter->pPrimitives,
			(size_t)pSourceFilter->iPrimitiveCount * sizeof(*pFilter->pPrimitives)
		);
		pFilter->iPrimitiveCount = pSourceFilter->iPrimitiveCount;
		pFilter->iPrimitiveCapacity = pSourceFilter->iPrimitiveCount;
	}
	return XGE_OK;
}

static int __xgeSvgCloneFontFaces(xge_svg pSource, xge_svg pClone)
{
	int i;
	int iRet;

	if ( pSource->iFontFaceCount <= 0 ) return XGE_OK;
	pClone->pFontFaces = (xge_svg_font_face_t*)xrtCalloc(
		(size_t)pSource->iFontFaceCount, sizeof(*pClone->pFontFaces)
	);
	if ( pClone->pFontFaces == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	pClone->iFontFaceCount = pSource->iFontFaceCount;
	pClone->iFontFaceCapacity = pSource->iFontFaceCount;
	for ( i = 0; i < pSource->iFontFaceCount; i++ ) {
		const xge_svg_font_face_t* pSourceFace = &pSource->pFontFaces[i];
		xge_svg_font_face_t* pFace = &pClone->pFontFaces[i];

		pFace->bLoadAttempted = pSourceFace->bLoadAttempted;
		if ( pSourceFace->sFamily != NULL ) {
			pFace->sFamily = __xgeStrDup(pSourceFace->sFamily);
			if ( pFace->sFamily == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
		}
		if ( (pSourceFace->pData != NULL) && (pSourceFace->iSize > 0) ) {
			pFace->pData = (unsigned char*)xrtMalloc((size_t)pSourceFace->iSize);
			if ( pFace->pData == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
			memcpy(pFace->pData, pSourceFace->pData, (size_t)pSourceFace->iSize);
			pFace->iSize = pSourceFace->iSize;
		} else if ( pSourceFace->pFace != NULL ) {
			iRet = xgeFontFaceAddRef(pSourceFace->pFace);
			if ( iRet <= 0 ) return XGE_ERROR_INVALID_STATE;
			pFace->pFace = pSourceFace->pFace;
		}
	}
	return XGE_OK;
}

int xgeSvgCreate(xge_svg* ppSvg)
{
	xge_svg pSvg;

	if ( ppSvg == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pSvg = (xge_svg)xrtCalloc(1, sizeof(*pSvg));
	if ( pSvg == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pSvg->iMagic = XGE_SVG_MAGIC;
	pSvg->iRefCount = 1;
	pSvg->tPicturePaint.iType = XGE_SVG_PAINT_PICTURE;
	pSvg->tPicturePaint.pOwner = pSvg;
	pSvg->tRootPaint.iType = XGE_SVG_PAINT_SCENE;
	pSvg->tRootPaint.pOwner = pSvg;
	pSvg->tRootPaint.pParentPaint = &pSvg->tPicturePaint;
	pSvg->tContentPaint.iType = XGE_SVG_PAINT_SCENE;
	pSvg->tContentPaint.pOwner = pSvg;
	pSvg->tContentPaint.pParentPaint = &pSvg->tRootPaint;
	pSvg->tDocumentPaint.iType = XGE_SVG_PAINT_SCENE;
	pSvg->tDocumentPaint.pOwner = pSvg;
	pSvg->tDocumentPaint.pParentPaint = &pSvg->tContentPaint;
	pSvg->iAspectAlignX = XGE_SVG_ASPECT_ALIGN_MID;
	pSvg->iAspectAlignY = XGE_SVG_ASPECT_ALIGN_MID;
	pSvg->iAspectMeetOrSlice = XGE_SVG_ASPECT_MEET;
	pSvg->iCurrentElementNode = -1;
	memset(pSvg->arrLastChildByDepth, 0xff, sizeof(pSvg->arrLastChildByDepth));
	*ppSvg = pSvg;
	return XGE_OK;
}

int xgeSvgClone(xge_svg pSvg, xge_svg* ppClone)
{
	xge_svg pClone;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (ppClone == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppClone = NULL;
	iRet = xgeSvgCreate(&pClone);
	if ( iRet != XGE_OK ) return iRet;

	pClone->tViewBox = pSvg->tViewBox;
	pClone->bHasViewBox = pSvg->bHasViewBox;
	pClone->fIntrinsicWidth = pSvg->fIntrinsicWidth;
	pClone->fIntrinsicHeight = pSvg->fIntrinsicHeight;
	pClone->fWidth = pSvg->fWidth;
	pClone->fHeight = pSvg->fHeight;
	pClone->bLoaded = pSvg->bLoaded;
	pClone->fOriginX = pSvg->fOriginX;
	pClone->fOriginY = pSvg->fOriginY;
	pClone->iAspectAlignX = pSvg->iAspectAlignX;
	pClone->iAspectAlignY = pSvg->iAspectAlignY;
	pClone->iAspectMeetOrSlice = pSvg->iAspectMeetOrSlice;
	__xgeSvgPaintRuntimeStateCopy(&pClone->tPicturePaint, &pSvg->tPicturePaint);
	__xgeSvgPaintRuntimeStateCopy(&pClone->tRootPaint, &pSvg->tRootPaint);
	__xgeSvgPaintRuntimeStateCopy(&pClone->tContentPaint, &pSvg->tContentPaint);
	__xgeSvgPaintRuntimeStateCopy(&pClone->tDocumentPaint, &pSvg->tDocumentPaint);
	iRet = __xgeSvgPaintRuntimeCompositionClone(&pClone->tPicturePaint, &pSvg->tPicturePaint);
	if ( iRet != XGE_OK ) goto fail;
	iRet = __xgeSvgPaintRuntimeCompositionClone(&pClone->tRootPaint, &pSvg->tRootPaint);
	if ( iRet != XGE_OK ) goto fail;
	iRet = __xgeSvgPaintRuntimeCompositionClone(&pClone->tContentPaint, &pSvg->tContentPaint);
	if ( iRet != XGE_OK ) goto fail;
	iRet = __xgeSvgPaintRuntimeCompositionClone(&pClone->tDocumentPaint, &pSvg->tDocumentPaint);
	if ( iRet != XGE_OK ) goto fail;
	if ( pSvg->sBaseDir != NULL ) {
		pClone->sBaseDir = __xgeStrDup(pSvg->sBaseDir);
		if ( pClone->sBaseDir == NULL ) {
			iRet = XGE_ERROR_OUT_OF_MEMORY;
			goto fail;
		}
	}
	iRet = __xgeSvgCloneDefNames(pSvg, pClone);
	if ( iRet != XGE_OK ) goto fail;
	iRet = __xgeSvgCloneMasks(pSvg, pClone);
	if ( iRet != XGE_OK ) goto fail;
	iRet = __xgeSvgCloneFilters(pSvg, pClone);
	if ( iRet != XGE_OK ) goto fail;
	iRet = __xgeSvgCloneFontFaces(pSvg, pClone);
	if ( iRet != XGE_OK ) goto fail;
	iRet = __xgeSvgDrawItemsClone(pSvg->pItems, pSvg->iItemCount, &pClone->pItems);
	if ( iRet != XGE_OK ) goto fail;
	pClone->iItemCount = pSvg->iItemCount;
	pClone->iItemCapacity = pSvg->iItemCount;
	__xgeSvgPaintTreeBind(pClone);
	*ppClone = pClone;
	return XGE_OK;

fail:
	xgeSvgDestroy(pClone);
	return iRet;
}

void xgeSvgDestroy(xge_svg pSvg)
{
	if ( !__xgeSvgValid(pSvg) ) {
		return;
	}
	pSvg->iRefCount--;
	if ( pSvg->iRefCount > 0 ) {
		return;
	}
	__xgeSvgSyntheticPaintReset(&pSvg->tPicturePaint);
	__xgeSvgSyntheticPaintReset(&pSvg->tRootPaint);
	__xgeSvgSyntheticPaintReset(&pSvg->tContentPaint);
	__xgeSvgSyntheticPaintReset(&pSvg->tDocumentPaint);
	__xgeSvgDefsClear(pSvg);
	__xgeSvgPatternsClear(pSvg);
	__xgeSvgClipPathsClear(pSvg);
	__xgeSvgMasksClear(pSvg);
	__xgeSvgFiltersClear(pSvg);
	__xgeSvgLinearGradientsClear(pSvg);
	__xgeSvgRadialGradientsClear(pSvg);
	__xgeSvgStyleRulesClear(pSvg);
	__xgeSvgPendingUsesClear(pSvg);
	__xgeSvgPendingShapeResourcesClear(pSvg);
	__xgeSvgDrawItemsClear(pSvg);
	__xgeSvgFontCacheClear(pSvg);
	__xgeSvgElementNodesClear(pSvg);
	xrtFree(pSvg->sBaseDir);
	pSvg->sBaseDir = NULL;
	pSvg->iMagic = 0;
	xrtFree(pSvg);
}

int xgeSvgAddRef(xge_svg pSvg)
{
	if ( !__xgeSvgValid(pSvg) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pSvg->iRefCount++;
	return XGE_OK;
}

int xgeSvgRefCountGet(xge_svg pSvg, int* pRefCount)
{
	if ( !__xgeSvgValid(pSvg) || (pRefCount == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pRefCount = pSvg->iRefCount;
	return XGE_OK;
}

int xgeSvgClear(xge_svg pSvg)
{
	if ( !__xgeSvgValid(pSvg) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	__xgeSvgSyntheticPaintReset(&pSvg->tPicturePaint);
	__xgeSvgSyntheticPaintReset(&pSvg->tRootPaint);
	__xgeSvgSyntheticPaintReset(&pSvg->tContentPaint);
	__xgeSvgSyntheticPaintReset(&pSvg->tDocumentPaint);
	__xgeSvgDrawItemsClear(pSvg);
	__xgeSvgDefsClear(pSvg);
	__xgeSvgPatternsClear(pSvg);
	__xgeSvgClipPathsClear(pSvg);
	__xgeSvgMasksClear(pSvg);
	__xgeSvgFiltersClear(pSvg);
	__xgeSvgLinearGradientsClear(pSvg);
	__xgeSvgRadialGradientsClear(pSvg);
	__xgeSvgStyleRulesClear(pSvg);
	__xgeSvgPendingUsesClear(pSvg);
	__xgeSvgPendingShapeResourcesClear(pSvg);
	__xgeSvgFontCacheClear(pSvg);
	__xgeSvgFontFacesClear(pSvg);
	__xgeSvgElementNodesClear(pSvg);
	xrtFree(pSvg->sBaseDir);
	pSvg->sBaseDir = NULL;
	pSvg->bHasViewBox = 0;
	pSvg->fIntrinsicWidth = 0.0f;
	pSvg->fIntrinsicHeight = 0.0f;
	pSvg->fWidth = 0.0f;
	pSvg->fHeight = 0.0f;
	pSvg->bLoaded = 0;
	pSvg->fOriginX = 0.0f;
	pSvg->fOriginY = 0.0f;
	pSvg->tViewBox.fX = 0.0f;
	pSvg->tViewBox.fY = 0.0f;
	pSvg->tViewBox.fW = 0.0f;
	pSvg->tViewBox.fH = 0.0f;
	pSvg->iAspectAlignX = XGE_SVG_ASPECT_ALIGN_MID;
	pSvg->iAspectAlignY = XGE_SVG_ASPECT_ALIGN_MID;
	pSvg->iAspectMeetOrSlice = XGE_SVG_ASPECT_MEET;
	pSvg->iElementStackTop = 0;
	pSvg->iCurrentElementNode = -1;
	memset(pSvg->arrLastChildByDepth, 0xff, sizeof(pSvg->arrLastChildByDepth));
	return XGE_OK;
}

static int __xgeSvgFiltersPreScan(xge_svg pSvg, const char* sText, int iSize)
{
	const char* p;
	const char* pEnd;
	int iFilterIndex;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (sText == NULL) || (iSize <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pEnd = sText + iSize;
	p = sText;
	while ( (p = strchr(p, '<')) != NULL ) {
		const char* pTagEnd = __xgeSvgFindTagEnd(p);

		if ( (pTagEnd == NULL) || (pTagEnd >= pEnd) ) {
			break;
		}
		if ( (p[1] != '/') && __xgeSvgTagNameIsRawContentSkip(p) ) {
			const char* pElementEnd;

			if ( __xgeSvgElementRangeEnd(p, pTagEnd, pEnd, &pElementEnd) ) p = pElementEnd;
			else p = pTagEnd + 1;
			continue;
		}
		if ( (p[1] != '/') && __xgeSvgTagNameEquals(p, "svg") ) {
			xge_svg_style_t tDefaultStyle = __xgeSvgStyleDefault();
			xge_svg_style_t tRootStyle = __xgeSvgStyleResolve(pSvg, &tDefaultStyle, p, pTagEnd);

			__xgeSvgParseRoot(pSvg, p, pTagEnd, tRootStyle.fFontSize);
			break;
		}
		p = pTagEnd + 1;
	}

	iFilterIndex = -1;
	p = sText;
	while ( (p = strchr(p, '<')) != NULL ) {
		const char* pTagEnd = __xgeSvgFindTagEnd(p);
		int bSelfClosing;

		if ( (pTagEnd == NULL) || (pTagEnd >= pEnd) ) {
			break;
		}
		bSelfClosing = __xgeSvgIsSelfClosingTag(p, pTagEnd);
		if ( (p[1] != '/') && __xgeSvgTagNameIsRawContentSkip(p) ) {
			const char* pElementEnd;

			if ( __xgeSvgElementRangeEnd(p, pTagEnd, pEnd, &pElementEnd) ) p = pElementEnd;
			else p = pTagEnd + 1;
			continue;
		}
		if ( __xgeSvgIsCloseTagName(p, "filter") ) {
			iFilterIndex = -1;
		} else if ( iFilterIndex >= 0 ) {
			if ( __xgeSvgTagNameEquals(p, "feGaussianBlur") ) {
				iRet = __xgeSvgParseFilterFeGaussianBlur(pSvg, iFilterIndex, p, pTagEnd);
				if ( iRet != XGE_OK ) return iRet;
			}
		} else if ( __xgeSvgTagNameEquals(p, "filter") ) {
			iRet = __xgeSvgParseFilter(pSvg, p, pTagEnd, &iFilterIndex);
			if ( iRet != XGE_OK ) return iRet;
			if ( bSelfClosing ) iFilterIndex = -1;
		}
		p = pTagEnd + 1;
	}
	pSvg->iCurrentElementNode = -1;
	return XGE_OK;
}

static int __xgeSvgLoadMemoryEx(xge_svg pSvg, const void* pData, int iSize, const char* sBaseDir)
{
	char* sText;
	const char* p;
	xge_svg_style_t arrStack[XGE_SVG_STACK_MAX];
	int arrDefStack[XGE_SVG_STACK_MAX];
	int arrMainIdDefStack[XGE_SVG_STACK_MAX];
	int arrOwnGroupDefStack[XGE_SVG_STACK_MAX];
	int arrGroupCopyParentDefStack[XGE_SVG_STACK_MAX];
	int arrDefsStyleBaseStackTop[XGE_SVG_STACK_MAX];
	xge_shape_ex_matrix_t arrMainIdNormalizeMatrixStack[XGE_SVG_STACK_MAX];
	int arrMainIdNormalizeStack[XGE_SVG_STACK_MAX];
	int arrMainIdNormalizeCloseStack[XGE_SVG_STACK_MAX];
	int arrMainIdNormalizeStartDefStack[XGE_SVG_STACK_MAX];
	int arrGroupFilterFirstItemStack[XGE_SVG_STACK_MAX];
	int arrGroupFilterIndexStack[XGE_SVG_STACK_MAX];
	xge_shape_ex_matrix_t arrGroupFilterTransformStack[XGE_SVG_STACK_MAX];
	int arrGroupCompositeFirstItemStack[XGE_SVG_STACK_MAX];
	int arrGroupCompositeBlendStack[XGE_SVG_STACK_MAX];
	int arrGroupCompositeBlendSetStack[XGE_SVG_STACK_MAX];
	float arrGroupCompositeOpacityStack[XGE_SVG_STACK_MAX];
	char arrGroupMaskIdStack[XGE_SVG_STACK_MAX][XGE_SVG_ID_MAX];
	int arrGroupDefIndexStack[XGE_SVG_STACK_MAX];
	int arrGroupDefFirstItemStack[XGE_SVG_STACK_MAX];
	char arrGroupDefFilterIdStack[XGE_SVG_STACK_MAX][XGE_SVG_ID_MAX];
	int arrGroupClipFirstItemStack[XGE_SVG_STACK_MAX];
	int arrGroupClipCompositionStack[XGE_SVG_STACK_MAX];
	uint32_t arrGroupPaintIdStack[XGE_SVG_STACK_MAX];
	int arrGroupPaintFirstItemStack[XGE_SVG_STACK_MAX];
	int arrGroupPaintForceStack[XGE_SVG_STACK_MAX];
	int iStackTop;
	int iDefsDepth;
	int iLinearGradientIndex;
	int iRadialGradientIndex;
	int iIgnoredGradientDepth;
	int iClipPathIndex;
	int iClipTransformTop;
	xge_shape_ex_matrix_t arrClipTransformStack[XGE_SVG_STACK_MAX];
	xge_svg_style_t arrClipStyleStack[XGE_SVG_STACK_MAX];
	int iClipStyleTop;
	int iClipIgnoredGroupDepth;
	int iMaskIndex;
	int iMaskTransformTop;
	xge_shape_ex_matrix_t arrMaskTransformStack[XGE_SVG_STACK_MAX];
	xge_svg_style_t arrMaskStyleStack[XGE_SVG_STACK_MAX];
	int iMaskStyleTop;
	int iFilterIndex;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pData == NULL) || (iSize <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = xgeSvgClear(pSvg);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = __xgeSvgBaseDirSet(pSvg, sBaseDir);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	sText = (char*)xrtMalloc((size_t)iSize + 1u);
	if ( sText == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	memcpy(sText, pData, (size_t)iSize);
	sText[iSize] = '\0';
	arrStack[0] = __xgeSvgStyleDefault();
	arrDefStack[0] = -1;
	arrMainIdDefStack[0] = -1;
	arrOwnGroupDefStack[0] = -1;
	arrGroupCopyParentDefStack[0] = -1;
	arrDefsStyleBaseStackTop[0] = 0;
	arrMainIdNormalizeMatrixStack[0] = __xgeSvgMatrixIdentity();
	arrMainIdNormalizeStack[0] = 0;
	arrMainIdNormalizeCloseStack[0] = 0;
	arrMainIdNormalizeStartDefStack[0] = 0;
	arrGroupFilterFirstItemStack[0] = -1;
	arrGroupFilterIndexStack[0] = -1;
	arrGroupFilterTransformStack[0] = __xgeSvgMatrixIdentity();
	arrGroupCompositeFirstItemStack[0] = -1;
	arrGroupCompositeBlendStack[0] = XGE_BLEND_ALPHA;
	arrGroupCompositeBlendSetStack[0] = 0;
	arrGroupCompositeOpacityStack[0] = 1.0f;
	arrGroupMaskIdStack[0][0] = '\0';
	arrGroupDefIndexStack[0] = -1;
	arrGroupDefFirstItemStack[0] = -1;
	arrGroupDefFilterIdStack[0][0] = '\0';
	arrGroupClipFirstItemStack[0] = -1;
	arrGroupClipCompositionStack[0] = 0;
	arrGroupPaintIdStack[0] = 0;
	arrGroupPaintFirstItemStack[0] = -1;
	arrGroupPaintForceStack[0] = 0;
	iStackTop = 0;
	iDefsDepth = 0;
	iLinearGradientIndex = -1;
	iRadialGradientIndex = -1;
	iIgnoredGradientDepth = 0;
	iClipPathIndex = -1;
	iClipTransformTop = 0;
	arrClipTransformStack[0] = __xgeSvgMatrixIdentity();
	iClipStyleTop = 0;
	arrClipStyleStack[0] = arrStack[0];
	iClipIgnoredGroupDepth = 0;
	iMaskIndex = -1;
	iMaskTransformTop = 0;
	arrMaskTransformStack[0] = __xgeSvgMatrixIdentity();
	iMaskStyleTop = 0;
	arrMaskStyleStack[0] = arrStack[0];
	iFilterIndex = -1;
	pSvg->iElementStackTop = 0;
	iRet = __xgeSvgParseStyleBlocks(pSvg, sText);
	if ( iRet != XGE_OK ) {
		__xgeSvgElementNodesClear(pSvg);
		xrtFree(sText);
		return iRet;
	}
	iRet = __xgeSvgElementNodesPreScan(pSvg, sText);
	if ( iRet != XGE_OK ) {
		__xgeSvgElementNodesClear(pSvg);
		xrtFree(sText);
		return iRet;
	}
	iRet = __xgeSvgFiltersPreScan(pSvg, sText, iSize);
	if ( iRet != XGE_OK ) {
		__xgeSvgElementNodesClear(pSvg);
		xrtFree(sText);
		return iRet;
	}
	p = sText;
	while ( (p = strchr(p, '<')) != NULL ) {
		const char* pTagEnd = __xgeSvgFindTagEnd(p);
		int bSelfClosing;
		if ( pTagEnd == NULL ) {
			break;
		}
		bSelfClosing = __xgeSvgIsSelfClosingTag(p, pTagEnd);
		if ( iIgnoredGradientDepth > 0 ) {
			if ( __xgeSvgIsCloseTagName(p, "linearGradient") || __xgeSvgIsCloseTagName(p, "radialGradient") ) {
				iIgnoredGradientDepth--;
			} else if ( !bSelfClosing && (__xgeSvgTagNameEquals(p, "linearGradient") || __xgeSvgTagNameEquals(p, "radialGradient")) ) {
				iIgnoredGradientDepth++;
			}
			p = pTagEnd + 1;
			continue;
		}
		if ( ((iLinearGradientIndex >= 0) || (iRadialGradientIndex >= 0)) &&
		     (__xgeSvgTagNameEquals(p, "linearGradient") || __xgeSvgTagNameEquals(p, "radialGradient")) ) {
			if ( !bSelfClosing ) {
				iIgnoredGradientDepth = 1;
			}
			p = pTagEnd + 1;
			continue;
		}
		if ( (p[1] != '/') && __xgeSvgTagNameIsRawContentSkip(p) ) {
			const char* pElementEnd;

			if ( __xgeSvgElementRangeEnd(p, pTagEnd, sText + iSize, &pElementEnd) ) {
				p = pElementEnd;
			} else {
				p = pTagEnd + 1;
			}
			continue;
		}
		if ( __xgeSvgIsCloseTagName(p, "clipPath") ) {
			iClipPathIndex = -1;
			iClipTransformTop = 0;
			arrClipTransformStack[0] = __xgeSvgMatrixIdentity();
			iClipStyleTop = 0;
			arrClipStyleStack[0] = arrStack[iStackTop];
			iClipIgnoredGroupDepth = 0;
			p = pTagEnd + 1;
			continue;
		}
		if ( (iClipPathIndex >= 0) && __xgeSvgIsCloseClipMaskContainer(p) ) {
			if ( iClipIgnoredGroupDepth > 0 ) {
				iClipIgnoredGroupDepth--;
				p = pTagEnd + 1;
				continue;
			}
			if ( iClipTransformTop > 0 ) {
				iClipTransformTop--;
			}
			if ( iClipStyleTop > 0 ) {
				iClipStyleTop--;
			}
			p = pTagEnd + 1;
			continue;
		}
		if ( __xgeSvgIsCloseTagName(p, "mask") ) {
			iMaskIndex = -1;
			iMaskTransformTop = 0;
			arrMaskTransformStack[0] = __xgeSvgMatrixIdentity();
			iMaskStyleTop = 0;
			arrMaskStyleStack[0] = arrStack[iStackTop];
			p = pTagEnd + 1;
			continue;
		}
		if ( __xgeSvgIsCloseTagName(p, "filter") ) {
			if ( iFilterIndex >= 0 ) {
				__xgeSvgElementStackPop(pSvg, "filter");
			}
			iFilterIndex = -1;
			p = pTagEnd + 1;
			continue;
		}
		if ( (iMaskIndex >= 0) && __xgeSvgIsCloseClipMaskContainer(p) ) {
			if ( iMaskTransformTop > 0 ) {
				iMaskTransformTop--;
			}
			if ( iMaskStyleTop > 0 ) {
				iMaskStyleTop--;
			}
			p = pTagEnd + 1;
			continue;
		}
		if ( __xgeSvgIsCloseTagName(p, "linearGradient") ) {
			iLinearGradientIndex = -1;
			__xgeSvgElementStackPop(pSvg, "linearGradient");
			p = pTagEnd + 1;
			continue;
		}
		if ( __xgeSvgIsCloseTagName(p, "radialGradient") ) {
			iRadialGradientIndex = -1;
			__xgeSvgElementStackPop(pSvg, "radialGradient");
			p = pTagEnd + 1;
			continue;
		}
		if ( __xgeSvgIsCloseTagName(p, "defs") ) {
			if ( iDefsDepth > 0 ) {
				iDefsDepth--;
			}
			__xgeSvgElementStackPop(pSvg, "defs");
			p = pTagEnd + 1;
			continue;
		}
		if ( iMaskIndex >= 0 ) {
			if ( __xgeSvgTagNameIsClipMaskContainer(p) ) {
				if ( !bSelfClosing ) {
					xge_svg_style_t tGroupStyle;

					if ( (iMaskTransformTop >= (XGE_SVG_STACK_MAX - 1)) || (iMaskStyleTop >= (XGE_SVG_STACK_MAX - 1)) ) {
						__xgeSvgElementNodesClear(pSvg);
						xrtFree(sText);
						return XGE_ERROR_OUT_OF_MEMORY;
					}
					tGroupStyle = __xgeSvgStyleResolveMaskContent(pSvg, &arrMaskStyleStack[iMaskStyleTop], p, pTagEnd);
					arrMaskTransformStack[++iMaskTransformTop] = tGroupStyle.tTransform;
					arrMaskStyleStack[++iMaskStyleTop] = tGroupStyle;
				}
				p = pTagEnd + 1;
				continue;
			}
			iRet = __xgeSvgParseMaskShape(pSvg, iMaskIndex, p, pTagEnd, arrMaskTransformStack[iMaskTransformTop], &arrMaskStyleStack[iMaskStyleTop]);
			if ( iRet != XGE_OK ) {
				__xgeSvgElementNodesClear(pSvg);
				xrtFree(sText);
				return iRet;
			}
			p = pTagEnd + 1;
			continue;
		}
		if ( iFilterIndex >= 0 ) {
			if ( __xgeSvgTagNameEquals(p, "feGaussianBlur") ) {
				iRet = __xgeSvgParseFilterFeGaussianBlur(pSvg, iFilterIndex, p, pTagEnd);
				if ( iRet != XGE_OK ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return iRet;
				}
			}
			p = pTagEnd + 1;
			continue;
		}
		if ( iClipPathIndex >= 0 ) {
			if ( __xgeSvgTagNameIsClipMaskContainer(p) ) {
				if ( !bSelfClosing ) {
					iClipIgnoredGroupDepth++;
				}
				p = pTagEnd + 1;
				continue;
			}
			if ( iClipIgnoredGroupDepth > 0 ) {
				p = pTagEnd + 1;
				continue;
			}
			iRet = __xgeSvgParseClipShape(pSvg, iClipPathIndex, p, pTagEnd, arrClipTransformStack[iClipTransformTop], &arrClipStyleStack[iClipStyleTop]);
			if ( iRet != XGE_OK ) {
				__xgeSvgElementNodesClear(pSvg);
				xrtFree(sText);
				return iRet;
			}
			if ( __xgeSvgTagNameEquals(p, "clipPath") && !bSelfClosing ) {
				iClipPathIndex = -1;
			}
			p = pTagEnd + 1;
			continue;
		}
		if ( __xgeSvgTagNameEquals(p, "clipPath") ) {
			iRet = __xgeSvgParseClipPath(pSvg, p, pTagEnd, &iClipPathIndex);
			if ( iRet != XGE_OK ) {
				__xgeSvgElementNodesClear(pSvg);
				xrtFree(sText);
				return iRet;
			}
			iClipTransformTop = 0;
			arrClipTransformStack[0] = __xgeSvgMatrixIdentity();
			iClipStyleTop = 0;
			arrClipStyleStack[0] = __xgeSvgStyleResolve(pSvg, &arrStack[iStackTop], p, pTagEnd);
			iClipIgnoredGroupDepth = 0;
			if ( bSelfClosing ) {
				iClipPathIndex = -1;
			}
			p = pTagEnd + 1;
			continue;
		}
		if ( __xgeSvgTagNameEquals(p, "mask") ) {
			iRet = __xgeSvgParseMask(pSvg, p, pTagEnd, &iMaskIndex);
			if ( iRet != XGE_OK ) {
				__xgeSvgElementNodesClear(pSvg);
				xrtFree(sText);
				return iRet;
			}
			iMaskTransformTop = 0;
			arrMaskTransformStack[0] = __xgeSvgMatrixIdentity();
			iMaskStyleTop = 0;
			arrMaskStyleStack[0] = __xgeSvgStyleResolve(pSvg, &arrStack[iStackTop], p, pTagEnd);
			if ( bSelfClosing ) {
				iMaskIndex = -1;
			}
			p = pTagEnd + 1;
			continue;
		}
		if ( __xgeSvgTagNameEquals(p, "filter") ) {
			iRet = __xgeSvgParseFilter(pSvg, p, pTagEnd, &iFilterIndex);
			if ( iRet != XGE_OK ) {
				__xgeSvgElementNodesClear(pSvg);
				xrtFree(sText);
				return iRet;
			}
			if ( bSelfClosing ) {
				iFilterIndex = -1;
			} else if ( iFilterIndex >= 0 ) {
				iRet = __xgeSvgElementStackPush(pSvg, p, pTagEnd);
				if ( iRet != XGE_OK ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return iRet;
				}
			}
			p = pTagEnd + 1;
			continue;
		}
		if ( iLinearGradientIndex >= 0 ) {
			if ( __xgeSvgTagNameEquals(p, "stop") ) {
				iRet = __xgeSvgParseGradientStop(pSvg, iLinearGradientIndex, p, pTagEnd);
				if ( iRet != XGE_OK ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return iRet;
				}
			}
			p = pTagEnd + 1;
			continue;
		}
		if ( iRadialGradientIndex >= 0 ) {
			if ( __xgeSvgTagNameEquals(p, "stop") ) {
				iRet = __xgeSvgParseRadialGradientStop(pSvg, iRadialGradientIndex, p, pTagEnd);
				if ( iRet != XGE_OK ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return iRet;
				}
			}
			p = pTagEnd + 1;
			continue;
		}
		if ( __xgeSvgTagNameEquals(p, "defs") ) {
			if ( !bSelfClosing ) {
				if ( iDefsDepth >= XGE_SVG_STACK_MAX ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return XGE_ERROR_OUT_OF_MEMORY;
				}
				iRet = __xgeSvgElementStackPush(pSvg, p, pTagEnd);
				if ( iRet != XGE_OK ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return iRet;
				}
				arrDefsStyleBaseStackTop[iDefsDepth++] = iStackTop;
			}
			p = pTagEnd + 1;
			continue;
		}
		if ( __xgeSvgTagNameEquals(p, "linearGradient") ) {
			int bUseParentCurrentColor = (iDefsDepth <= 0) || (iStackTop > arrDefsStyleBaseStackTop[iDefsDepth - 1]);

			iRet = __xgeSvgParseLinearGradient(pSvg, p, pTagEnd, &arrStack[iStackTop], bUseParentCurrentColor, &iLinearGradientIndex);
			if ( iRet != XGE_OK ) {
				__xgeSvgElementNodesClear(pSvg);
				xrtFree(sText);
				return iRet;
			}
			if ( bSelfClosing ) {
				iLinearGradientIndex = -1;
			} else {
				iRet = __xgeSvgElementStackPush(pSvg, p, pTagEnd);
				if ( iRet != XGE_OK ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return iRet;
				}
			}
			p = pTagEnd + 1;
			continue;
		}
		if ( __xgeSvgTagNameEquals(p, "radialGradient") ) {
			int bUseParentCurrentColor = (iDefsDepth <= 0) || (iStackTop > arrDefsStyleBaseStackTop[iDefsDepth - 1]);

			iRet = __xgeSvgParseRadialGradient(pSvg, p, pTagEnd, &arrStack[iStackTop], bUseParentCurrentColor, &iRadialGradientIndex);
			if ( iRet != XGE_OK ) {
				__xgeSvgElementNodesClear(pSvg);
				xrtFree(sText);
				return iRet;
			}
			if ( bSelfClosing ) {
				iRadialGradientIndex = -1;
			} else {
				iRet = __xgeSvgElementStackPush(pSvg, p, pTagEnd);
				if ( iRet != XGE_OK ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return iRet;
				}
			}
			p = pTagEnd + 1;
			continue;
		}
		if ( __xgeSvgIsCloseGroupContainer(p) ||
		     __xgeSvgIsCloseTagName(p, "symbol") ||
		     __xgeSvgIsCloseTagName(p, "pattern") ||
		     __xgeSvgIsCloseTagName(p, "svg") ) {
			int bHasGroupFilter = (iStackTop > 0) && (arrGroupFilterIndexStack[iStackTop] >= 0);

			if ( (iStackTop > 0) && arrGroupClipCompositionStack[iStackTop] &&
			     (arrGroupClipFirstItemStack[iStackTop] >= 0) &&
			     (arrGroupMaskIdStack[iStackTop][0] == '\0') ) {
				iRet = __xgeSvgDrawItemsWrapGroup(
					pSvg, arrGroupClipFirstItemStack[iStackTop], 0,
					XGE_BLEND_ALPHA, 1.0f, NULL, 0, 1, 1.0f, NULL
				);
				if ( (iRet == XGE_OK) &&
				     (arrGroupClipFirstItemStack[iStackTop] < pSvg->iItemCount) &&
				     (pSvg->pItems[arrGroupClipFirstItemStack[iStackTop]].iType == XGE_SVG_DRAW_ITEM_GROUP) &&
				     (pSvg->pItems[arrGroupClipFirstItemStack[iStackTop]].u.pGroup != NULL) ) {
					pSvg->pItems[arrGroupClipFirstItemStack[iStackTop]].u.pGroup->bPaintClipComposition = 1;
				}
				if ( iRet != XGE_OK ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return iRet;
				}
			}

			if ( (iStackTop > 0) && (arrGroupFilterIndexStack[iStackTop] >= 0) ) {
				iRet = __xgeSvgWrapGroupFilter(
					pSvg,
					arrGroupFilterFirstItemStack[iStackTop],
					arrGroupFilterIndexStack[iStackTop],
					arrGroupFilterTransformStack[iStackTop]
				);
				if ( iRet != XGE_OK ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return iRet;
				}
			}
			if ( (iStackTop > 0) && (arrGroupCompositeFirstItemStack[iStackTop] >= 0) ) {
				if ( !bHasGroupFilter || arrGroupCompositeBlendSetStack[iStackTop] ||
				     (arrGroupMaskIdStack[iStackTop][0] != '\0') ) {
					iRet = __xgeSvgDrawItemsWrapGroup(
						pSvg,
						arrGroupCompositeFirstItemStack[iStackTop],
						arrGroupCompositeBlendSetStack[iStackTop],
						arrGroupCompositeBlendStack[iStackTop],
						/* ThorVG's nested post-effect scene drops the source scene opacity. */
						bHasGroupFilter ? 1.0f : arrGroupCompositeOpacityStack[iStackTop],
						arrGroupMaskIdStack[iStackTop], 0, 0,
						arrGroupCompositeOpacityStack[iStackTop], NULL
					);
				} else {
					iRet = XGE_OK;
				}
				if ( iRet != XGE_OK ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return iRet;
				}
			}
			if ( (iStackTop == 1) && __xgeSvgIsCloseTagName(p, "svg") &&
			     !bHasGroupFilter &&
			     (arrGroupCompositeFirstItemStack[iStackTop] >= 0) &&
			     (arrGroupCompositeOpacityStack[iStackTop] < (1.0f - XGE_SVG_EPSILON)) ) {
				iRet = __xgeSvgDrawItemsWrapGroup(
					pSvg,
					arrGroupCompositeFirstItemStack[iStackTop],
					0,
					XGE_BLEND_ALPHA,
					arrGroupCompositeOpacityStack[iStackTop],
					NULL, 0, 0, arrGroupCompositeOpacityStack[iStackTop], NULL
				);
				if ( iRet != XGE_OK ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return iRet;
				}
			}
			if ( (iStackTop > 0) && arrGroupPaintForceStack[iStackTop] ) {
				iRet = __xgeSvgDrawItemsWrapGroup(
					pSvg, arrGroupPaintFirstItemStack[iStackTop], 0,
					XGE_BLEND_ALPHA, 1.0f, NULL, arrGroupPaintIdStack[iStackTop], 1,
					arrGroupCompositeOpacityStack[iStackTop], &arrStack[iStackTop].tTransform
				);
				if ( (iRet == XGE_OK) &&
				     (arrGroupPaintFirstItemStack[iStackTop] >= 0) &&
				     (arrGroupPaintFirstItemStack[iStackTop] < pSvg->iItemCount) &&
				     (pSvg->pItems[arrGroupPaintFirstItemStack[iStackTop]].iType == XGE_SVG_DRAW_ITEM_GROUP) &&
				     (pSvg->pItems[arrGroupPaintFirstItemStack[iStackTop]].u.pGroup != NULL) ) {
					xge_svg_group_item_t* pPaintGroup =
						pSvg->pItems[arrGroupPaintFirstItemStack[iStackTop]].u.pGroup;

					pPaintGroup->bPaintOwnsClipComposition =
						arrGroupClipCompositionStack[iStackTop] ? 1 : 0;
					pPaintGroup->bPaintOwnsMaskComposition =
						arrGroupMaskIdStack[iStackTop][0] != '\0' ? 1 : 0;
				}
				if ( iRet != XGE_OK ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return iRet;
				}
			}
			if ( (iStackTop > 0) &&
			     (arrGroupDefIndexStack[iStackTop] >= 0) &&
			     (arrGroupDefIndexStack[iStackTop] < pSvg->iDefCount) &&
			     (arrGroupDefFirstItemStack[iStackTop] >= 0) ) {
				iRet = __xgeSvgDefItemsWrapGroup(
					&pSvg->pDefs[arrGroupDefIndexStack[iStackTop]],
					arrGroupDefFirstItemStack[iStackTop],
					arrGroupCompositeBlendSetStack[iStackTop],
					arrGroupCompositeBlendStack[iStackTop],
					arrGroupCompositeOpacityStack[iStackTop],
					arrGroupMaskIdStack[iStackTop],
					arrGroupDefFilterIdStack[iStackTop],
					arrGroupFilterTransformStack[iStackTop], 1
				);
				if ( iRet != XGE_OK ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return iRet;
				}
			}
			if ( __xgeSvgIsCloseGroupContainer(p) && (iStackTop > 0) ) {
				int iChildDef = arrMainIdDefStack[iStackTop];
				int iOwnGroupDef = arrOwnGroupDefStack[iStackTop];
				int iParentDef = arrGroupCopyParentDefStack[iStackTop];

				if ( (iChildDef >= 0) && (iChildDef < pSvg->iDefCount) &&
				     (iParentDef >= 0) && (iParentDef < pSvg->iDefCount) &&
				     (iChildDef != iParentDef) ) {
					iRet = __xgeSvgDefCopyContent(&pSvg->pDefs[iParentDef], &pSvg->pDefs[iChildDef]);
					if ( iRet != XGE_OK ) {
						__xgeSvgElementNodesClear(pSvg);
						xrtFree(sText);
						return iRet;
					}
				}
				if ( (iOwnGroupDef >= 0) && (arrDefStack[iStackTop] < 0) ) {
					iRet = __xgeSvgDefRemoveParentTransform(pSvg, iOwnGroupDef, arrStack[iStackTop - 1].tTransform);
					if ( iRet != XGE_OK ) {
						__xgeSvgElementNodesClear(pSvg);
						xrtFree(sText);
						return iRet;
					}
				}
			}
			if ( (iStackTop > 0) && arrMainIdNormalizeCloseStack[iStackTop] ) {
				int iNormalizeDef;
				int iStartDef = arrMainIdNormalizeStartDefStack[iStackTop];

				if ( iStartDef < 0 ) {
					iStartDef = 0;
				}
				if ( iStartDef > pSvg->iDefCount ) {
					iStartDef = pSvg->iDefCount;
				}
				for ( iNormalizeDef = iStartDef; iNormalizeDef < pSvg->iDefCount; iNormalizeDef++ ) {
					iRet = __xgeSvgDefPrependTransform(&pSvg->pDefs[iNormalizeDef], arrMainIdNormalizeMatrixStack[iStackTop]);
					if ( iRet != XGE_OK ) {
						__xgeSvgElementNodesClear(pSvg);
						xrtFree(sText);
						return iRet;
					}
				}
			}
			if ( iStackTop > 0 ) {
				iStackTop--;
			}
			if ( __xgeSvgIsCloseTagName(p, "g") ) {
				__xgeSvgElementStackPop(pSvg, "g");
			} else if ( __xgeSvgIsCloseTagName(p, "a") ) {
				__xgeSvgElementStackPop(pSvg, "a");
			} else if ( __xgeSvgIsCloseTagName(p, "symbol") ) {
				__xgeSvgElementStackPop(pSvg, "symbol");
			} else if ( __xgeSvgIsCloseTagName(p, "pattern") ) {
				__xgeSvgElementStackPop(pSvg, "pattern");
			} else {
				__xgeSvgElementStackPop(pSvg, "svg");
			}
		} else if ( __xgeSvgTagNameEquals(p, "svg") && (iStackTop == 0) ) {
			xge_svg_style_t tStyle;
			float fRootOpacity;
			int iRootCompositeFirstItem;
			int iRootFilterFirstItem;
			int iRootFilterIndex;
			xge_shape_ex_matrix_t tRootFilterTransform;
			char sRootMaskId[XGE_SVG_ID_MAX];
			char sRootId[XGE_SVG_ATTR_MAX];
			uint32_t iRootPaintId;

			tStyle = __xgeSvgStyleResolve(pSvg, &arrStack[iStackTop], p, pTagEnd);
			iRootPaintId = (__xgeSvgAttrCopy(p, pTagEnd, "id", sRootId, sizeof(sRootId)) &&
				(sRootId[0] != '\0')) ? xgeShapeExIdFromName(sRootId) : 0;
			strcpy(sRootMaskId, tStyle.sMaskId);
			__xgeSvgParseRoot(pSvg, p, pTagEnd, tStyle.fFontSize);
			fRootOpacity = tStyle.fOpacity;
			if ( fRootOpacity < 0.0f ) fRootOpacity = 0.0f;
			if ( fRootOpacity > 1.0f ) fRootOpacity = 1.0f;
			iRootCompositeFirstItem = (tStyle.bBlendSet || (fRootOpacity < (1.0f - XGE_SVG_EPSILON)) ||
				(sRootMaskId[0] != '\0')) ? pSvg->iItemCount : -1;
			iRootFilterFirstItem = -1;
			iRootFilterIndex = -1;
			tRootFilterTransform = tStyle.tTransform;
			if ( tStyle.sFilterId[0] != '\0' ) {
				int iResolvedFilter = __xgeSvgFilterFind(pSvg, tStyle.sFilterId);

				if ( (iResolvedFilter >= 0) && (pSvg->pFilters[iResolvedFilter].iPrimitiveCount > 0) ) {
					iRootFilterFirstItem = pSvg->iItemCount;
					iRootFilterIndex = iResolvedFilter;
				}
				tStyle.sFilterId[0] = '\0';
			}
			tStyle.fOpacity = 1.0f;
			tStyle.sMaskId[0] = '\0';
			if ( !bSelfClosing ) {
				if ( iStackTop >= (XGE_SVG_STACK_MAX - 1) ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return XGE_ERROR_OUT_OF_MEMORY;
				}
				arrStack[++iStackTop] = tStyle;
				arrDefStack[iStackTop] = arrDefStack[iStackTop - 1];
				arrMainIdDefStack[iStackTop] = arrMainIdDefStack[iStackTop - 1];
				arrOwnGroupDefStack[iStackTop] = -1;
				arrGroupCopyParentDefStack[iStackTop] = -1;
				arrMainIdNormalizeMatrixStack[iStackTop] = arrMainIdNormalizeMatrixStack[iStackTop - 1];
				arrMainIdNormalizeStack[iStackTop] = arrMainIdNormalizeStack[iStackTop - 1];
				arrMainIdNormalizeCloseStack[iStackTop] = 0;
				arrMainIdNormalizeStartDefStack[iStackTop] = arrMainIdNormalizeStartDefStack[iStackTop - 1];
				arrGroupFilterFirstItemStack[iStackTop] = iRootFilterFirstItem;
				arrGroupFilterIndexStack[iStackTop] = iRootFilterIndex;
				arrGroupFilterTransformStack[iStackTop] = tRootFilterTransform;
				arrGroupCompositeFirstItemStack[iStackTop] = iRootCompositeFirstItem;
				arrGroupCompositeBlendStack[iStackTop] = tStyle.iBlend;
				arrGroupCompositeBlendSetStack[iStackTop] = tStyle.bBlendSet;
				arrGroupCompositeOpacityStack[iStackTop] = fRootOpacity;
				strcpy(arrGroupMaskIdStack[iStackTop], sRootMaskId);
				arrGroupDefIndexStack[iStackTop] = -1;
				arrGroupDefFirstItemStack[iStackTop] = -1;
				arrGroupDefFilterIdStack[iStackTop][0] = '\0';
				arrGroupClipFirstItemStack[iStackTop] = -1;
				arrGroupClipCompositionStack[iStackTop] = 0;
				arrGroupPaintIdStack[iStackTop] = iRootPaintId;
				arrGroupPaintFirstItemStack[iStackTop] = pSvg->iItemCount;
				arrGroupPaintForceStack[iStackTop] = 1;
				iRet = __xgeSvgElementStackPush(pSvg, p, pTagEnd);
				if ( iRet != XGE_OK ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return iRet;
				}
			}
		} else if ( __xgeSvgTagNameIsGroupContainer(p) || __xgeSvgTagNameEquals(p, "symbol") ) {
			xge_svg_style_t tStyle;
			xge_svg_style_t tDefinitionRootStyle;
			const xge_svg_style_t* pContainerParentStyle = &arrStack[iStackTop];
			int iDefIndex = arrDefStack[iStackTop];
			int iGroupMainIdDefIndex = arrMainIdDefStack[iStackTop];
			int iGroupCopyParentDefIndex = -1;
			int iOwnGroupDefIndex = -1;
			int iGroupDefIndex = -1;
			int iGroupDefFirstItem = -1;
			int bCapture = (iDefsDepth > 0) || (iDefIndex >= 0) || __xgeSvgTagNameEquals(p, "symbol");
			int bCaptureGroupEffects = !bCapture || (iDefIndex < 0) ||
				(iDefIndex >= pSvg->iDefCount) || !pSvg->pDefs[iDefIndex].bPatternContent;
			int iGroupFilterFirstItem = -1;
			int iGroupFilterIndex = -1;
			xge_shape_ex_matrix_t tGroupFilterTransform = __xgeSvgMatrixIdentity();
			int iGroupCompositeFirstItem = -1;
			int iGroupCompositeBlend = XGE_BLEND_ALPHA;
			int bGroupCompositeBlendSet = 0;
			float fGroupCompositeOpacity = 1.0f;
			char sId[XGE_SVG_ATTR_MAX];
			char sGroupFilterId[XGE_SVG_ID_MAX];
			char sGroupMaskId[XGE_SVG_ID_MAX];
			uint32_t iGroupPaintId = 0;
			int iGroupPaintFirstItem = -1;
			int bGroupPaintForce = 0;
			int iGroupClipFirstItem = -1;
			int bGroupClipComposition = 0;
			sId[0] = '\0';
			if ( !bCapture && __xgeSvgTagNameIsGroupContainer(p) ) {
				bGroupPaintForce = 1;
				iGroupPaintFirstItem = pSvg->iItemCount;
			}
			if ( __xgeSvgTagNameEquals(p, "symbol") ) {
				tDefinitionRootStyle = __xgeSvgStyleDefault();
				pContainerParentStyle = &tDefinitionRootStyle;
			}
			tStyle = __xgeSvgStyleResolve(pSvg, pContainerParentStyle, p, pTagEnd);
			if ( !bCapture && tStyle.bClipPathLocal && (tStyle.sClipId[0] != '\0') ) {
				iGroupClipFirstItem = pSvg->iItemCount;
				bGroupClipComposition = 1;
			}
			strcpy(sGroupMaskId, tStyle.sMaskId);
			if ( bCaptureGroupEffects ) {
				float fParentOpacity = pContainerParentStyle->fOpacity;

				if ( fParentOpacity > XGE_SVG_EPSILON ) {
					fGroupCompositeOpacity = tStyle.fOpacity / fParentOpacity;
				} else {
					fGroupCompositeOpacity = 0.0f;
				}
				if ( fGroupCompositeOpacity < 0.0f ) fGroupCompositeOpacity = 0.0f;
				if ( fGroupCompositeOpacity > 1.0f ) fGroupCompositeOpacity = 1.0f;
				iGroupCompositeBlend = tStyle.iBlend;
				bGroupCompositeBlendSet = tStyle.bBlendSet;
				if ( !bCapture && (bGroupCompositeBlendSet ||
				     (fGroupCompositeOpacity < (1.0f - XGE_SVG_EPSILON)) ||
				     (sGroupMaskId[0] != '\0')) ) {
					iGroupCompositeFirstItem = pSvg->iItemCount;
				}
				tStyle.fOpacity = fParentOpacity;
			}
			tStyle.sMaskId[0] = '\0';
			sGroupFilterId[0] = '\0';
			if ( tStyle.sFilterId[0] != '\0' ) {
				int iResolvedFilter;

				strcpy(sGroupFilterId, tStyle.sFilterId);
				tGroupFilterTransform = tStyle.tTransform;
				iResolvedFilter = __xgeSvgFilterFind(pSvg, tStyle.sFilterId);
				if ( !bCapture && (iResolvedFilter >= 0) &&
				     (pSvg->pFilters[iResolvedFilter].iPrimitiveCount > 0) ) {
					iGroupFilterFirstItem = pSvg->iItemCount;
					iGroupFilterIndex = iResolvedFilter;
				}
				tStyle.sFilterId[0] = '\0';
			}
			if ( bCapture && __xgeSvgAttrCopy(p, pTagEnd, "id", sId, sizeof(sId)) && (sId[0] != '\0') ) {
				iRet = __xgeSvgDefGetOrCreate(pSvg, sId, &iDefIndex);
				if ( iRet != XGE_OK ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return iRet;
				}
				iOwnGroupDefIndex = iDefIndex;
				if ( __xgeSvgTagNameEquals(p, "symbol") ) {
					__xgeSvgParseSymbolMeta(pSvg, iDefIndex, p, pTagEnd);
				}
			} else if ( !bCapture && __xgeSvgTagNameIsGroupContainer(p) && __xgeSvgAttrCopy(p, pTagEnd, "id", sId, sizeof(sId)) && (sId[0] != '\0') ) {
				int iParentGroupMainIdDefIndex = iGroupMainIdDefIndex;

				iGroupPaintId = xgeShapeExIdFromName(sId);
				iRet = __xgeSvgDefGetOrCreate(pSvg, sId, &iGroupMainIdDefIndex);
				if ( iRet != XGE_OK ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return iRet;
				}
				iOwnGroupDefIndex = iGroupMainIdDefIndex;
				if ( (iParentGroupMainIdDefIndex >= 0) && (iParentGroupMainIdDefIndex != iGroupMainIdDefIndex) ) {
					iGroupCopyParentDefIndex = iParentGroupMainIdDefIndex;
				}
			}
			if ( (iOwnGroupDefIndex >= 0) && (iOwnGroupDefIndex < pSvg->iDefCount) ) {
				strcpy(pSvg->pDefs[iOwnGroupDefIndex].sGroupFilterId, sGroupFilterId);
				pSvg->pDefs[iOwnGroupDefIndex].tGroupFilterTransform = tGroupFilterTransform;
			}
			iGroupDefIndex = bCapture ? (bCaptureGroupEffects ? iDefIndex : -1) : iGroupMainIdDefIndex;
			if ( (iGroupDefIndex >= 0) && (iGroupDefIndex < pSvg->iDefCount) ) {
				iGroupDefFirstItem = pSvg->pDefs[iGroupDefIndex].iItemCount;
			}
			if ( !bSelfClosing ) {
				if ( iStackTop >= (XGE_SVG_STACK_MAX - 1) ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return XGE_ERROR_OUT_OF_MEMORY;
				}
				arrStack[++iStackTop] = tStyle;
				arrDefStack[iStackTop] = iDefIndex;
				arrMainIdDefStack[iStackTop] = iGroupMainIdDefIndex;
				arrOwnGroupDefStack[iStackTop] = iOwnGroupDefIndex;
				arrGroupCopyParentDefStack[iStackTop] = iGroupCopyParentDefIndex;
				arrMainIdNormalizeMatrixStack[iStackTop] = arrMainIdNormalizeMatrixStack[iStackTop - 1];
				arrMainIdNormalizeStack[iStackTop] = arrMainIdNormalizeStack[iStackTop - 1];
				arrMainIdNormalizeCloseStack[iStackTop] = 0;
				arrMainIdNormalizeStartDefStack[iStackTop] = arrMainIdNormalizeStartDefStack[iStackTop - 1];
				arrGroupFilterFirstItemStack[iStackTop] = iGroupFilterFirstItem;
				arrGroupFilterIndexStack[iStackTop] = iGroupFilterIndex;
				arrGroupFilterTransformStack[iStackTop] = tGroupFilterTransform;
				arrGroupCompositeFirstItemStack[iStackTop] = iGroupCompositeFirstItem;
				arrGroupCompositeBlendStack[iStackTop] = iGroupCompositeBlend;
				arrGroupCompositeBlendSetStack[iStackTop] = bGroupCompositeBlendSet;
				arrGroupCompositeOpacityStack[iStackTop] = fGroupCompositeOpacity;
				strcpy(arrGroupMaskIdStack[iStackTop], sGroupMaskId);
				arrGroupDefIndexStack[iStackTop] = iGroupDefIndex;
				arrGroupDefFirstItemStack[iStackTop] = iGroupDefFirstItem;
				strcpy(arrGroupDefFilterIdStack[iStackTop], sGroupFilterId);
				arrGroupClipFirstItemStack[iStackTop] = iGroupClipFirstItem;
				arrGroupClipCompositionStack[iStackTop] = bGroupClipComposition;
				arrGroupPaintIdStack[iStackTop] = iGroupPaintId;
				arrGroupPaintFirstItemStack[iStackTop] = iGroupPaintFirstItem;
				arrGroupPaintForceStack[iStackTop] = bGroupPaintForce;
				iRet = __xgeSvgElementStackPush(pSvg, p, pTagEnd);
				if ( iRet != XGE_OK ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return iRet;
				}
			}
		} else if ( __xgeSvgTagNameEquals(p, "marker") ) {
			const char* pElementEnd;

			if ( !bSelfClosing && __xgeSvgElementRangeEnd(p, pTagEnd, sText + iSize, &pElementEnd) ) {
				p = pElementEnd;
				continue;
			}
		} else if ( __xgeSvgTagNameEquals(p, "pattern") ) {
			xge_svg_style_t tStyle;
			int iDefIndex;

			tStyle = __xgeSvgStyleResolve(pSvg, &arrStack[iStackTop], p, pTagEnd);
			iDefIndex = -1;
			iRet = __xgeSvgParsePattern(pSvg, p, pTagEnd, &tStyle, &iDefIndex);
			if ( iRet != XGE_OK ) {
				__xgeSvgElementNodesClear(pSvg);
				xrtFree(sText);
				return iRet;
			}
			if ( !bSelfClosing ) {
				if ( iStackTop >= (XGE_SVG_STACK_MAX - 1) ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return XGE_ERROR_OUT_OF_MEMORY;
				}
				arrStack[++iStackTop] = tStyle;
				arrDefStack[iStackTop] = iDefIndex;
				arrMainIdDefStack[iStackTop] = arrMainIdDefStack[iStackTop - 1];
				arrOwnGroupDefStack[iStackTop] = -1;
				arrGroupCopyParentDefStack[iStackTop] = -1;
				arrMainIdNormalizeMatrixStack[iStackTop] = arrMainIdNormalizeMatrixStack[iStackTop - 1];
				arrMainIdNormalizeStack[iStackTop] = 0;
				arrMainIdNormalizeCloseStack[iStackTop] = 0;
				arrMainIdNormalizeStartDefStack[iStackTop] = arrMainIdNormalizeStartDefStack[iStackTop - 1];
				arrGroupFilterFirstItemStack[iStackTop] = -1;
				arrGroupFilterIndexStack[iStackTop] = -1;
				arrGroupFilterTransformStack[iStackTop] = __xgeSvgMatrixIdentity();
				arrGroupCompositeFirstItemStack[iStackTop] = -1;
				arrGroupCompositeBlendStack[iStackTop] = XGE_BLEND_ALPHA;
				arrGroupCompositeBlendSetStack[iStackTop] = 0;
				arrGroupCompositeOpacityStack[iStackTop] = 1.0f;
				arrGroupMaskIdStack[iStackTop][0] = '\0';
				arrGroupDefIndexStack[iStackTop] = -1;
				arrGroupDefFirstItemStack[iStackTop] = -1;
				arrGroupDefFilterIdStack[iStackTop][0] = '\0';
				arrGroupPaintIdStack[iStackTop] = 0;
				arrGroupPaintFirstItemStack[iStackTop] = -1;
				arrGroupPaintForceStack[iStackTop] = 0;
				iRet = __xgeSvgElementStackPush(pSvg, p, pTagEnd);
				if ( iRet != XGE_OK ) {
					__xgeSvgElementNodesClear(pSvg);
					xrtFree(sText);
					return iRet;
				}
			}
		} else if ( __xgeSvgTagNameEquals(p, "use") ) {
			iRet = __xgeSvgParseUse(pSvg, p, pTagEnd, &arrStack[iStackTop], (iDefsDepth > 0) || (arrDefStack[iStackTop] >= 0), arrDefStack[iStackTop], arrMainIdDefStack[iStackTop]);
			if ( iRet != XGE_OK ) {
				__xgeSvgElementNodesClear(pSvg);
				xrtFree(sText);
				return iRet;
			}
		} else if ( __xgeSvgTagNameEquals(p, "image") ) {
			iRet = __xgeSvgParseImage(pSvg, p, pTagEnd, &arrStack[iStackTop], (iDefsDepth > 0) || (arrDefStack[iStackTop] >= 0), arrDefStack[iStackTop], arrMainIdDefStack[iStackTop]);
			if ( iRet != XGE_OK ) {
				__xgeSvgElementNodesClear(pSvg);
				xrtFree(sText);
				return iRet;
			}
		} else if ( __xgeSvgTagNameEquals(p, "text") ) {
			const char* pCloseText;
			const char* pCloseTextEnd;
			const char* pTextEnd;

			pCloseText = NULL;
			pCloseTextEnd = NULL;
			pTextEnd = pTagEnd;
			if ( !bSelfClosing ) {
				pCloseText = __xgeSvgFindCloseElement(pTagEnd + 1, "text", &pCloseTextEnd);
				if ( pCloseText != NULL ) {
					pTextEnd = pCloseText;
				}
			}
			iRet = __xgeSvgParseText(pSvg, p, pTagEnd, pTagEnd + 1, pTextEnd, &arrStack[iStackTop], (iDefsDepth > 0) || (arrDefStack[iStackTop] >= 0), arrDefStack[iStackTop], arrMainIdDefStack[iStackTop]);
			if ( iRet != XGE_OK ) {
				__xgeSvgElementNodesClear(pSvg);
				xrtFree(sText);
				return iRet;
			}
			if ( (pCloseText != NULL) && (pCloseTextEnd != NULL) ) {
				p = pCloseTextEnd + 1;
				continue;
			}
		} else {
			iRet = __xgeSvgParseElement(pSvg, p, pTagEnd, &arrStack[iStackTop], (iDefsDepth > 0) || (arrDefStack[iStackTop] >= 0), arrDefStack[iStackTop], arrMainIdDefStack[iStackTop]);
			if ( iRet != XGE_OK ) {
				__xgeSvgElementNodesClear(pSvg);
				xrtFree(sText);
				return iRet;
			}
		}
		p = pTagEnd + 1;
	}
	iRet = __xgeSvgResolveGradients(pSvg);
	if ( iRet != XGE_OK ) {
		__xgeSvgElementNodesClear(pSvg);
		xrtFree(sText);
		return iRet;
	}
	iRet = __xgeSvgPendingShapeResourcesResolve(pSvg);
	if ( iRet != XGE_OK ) {
		__xgeSvgElementNodesClear(pSvg);
		xrtFree(sText);
		return iRet;
	}
	iRet = __xgeSvgResolvePendingUses(pSvg);
	if ( iRet != XGE_OK ) {
		__xgeSvgElementNodesClear(pSvg);
		xrtFree(sText);
		return iRet;
	}
	__xgeSvgElementNodesClear(pSvg);
	xrtFree(sText);
	if ( !pSvg->bHasViewBox ) {
		xge_rect_t tContentBounds;
		int bWidthSet = pSvg->fWidth > 0.0f;
		int bHeightSet = pSvg->fHeight > 0.0f;

		memset(&tContentBounds, 0, sizeof(tContentBounds));
		iRet = __xgeSvgGetBoundsWithParent(
			pSvg, __xgeSvgMatrixIdentity(), 0.25f, &tContentBounds, 0
		);
		if ( iRet != XGE_OK ) return iRet;
		if ( !bWidthSet && !bHeightSet ) {
			pSvg->tViewBox = tContentBounds;
		} else {
			pSvg->tViewBox.fX = 0.0f;
			pSvg->tViewBox.fY = 0.0f;
			pSvg->tViewBox.fW = bWidthSet ? pSvg->fWidth : tContentBounds.fW;
			pSvg->tViewBox.fH = bHeightSet ? pSvg->fHeight : tContentBounds.fH;
		}
		if ( !bWidthSet ) pSvg->fWidth = pSvg->tViewBox.fW;
		if ( !bHeightSet ) pSvg->fHeight = pSvg->tViewBox.fH;
		pSvg->bHasViewBox = 1;
	}
	pSvg->fIntrinsicWidth = pSvg->fWidth;
	pSvg->fIntrinsicHeight = pSvg->fHeight;
	pSvg->bLoaded = 1;
	__xgeSvgPaintTreeBind(pSvg);
	return XGE_OK;
}

int xgeSvgLoadMemory(xge_svg pSvg, const void* pData, int iSize)
{
	return __xgeSvgLoadMemoryEx(pSvg, pData, iSize, NULL);
}

int xgeSvgLoad(xge_svg pSvg, const char* sURI)
{
	xge_resource_t tResource;
	char* sBaseDir;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (sURI == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	sBaseDir = __xgeSvgUriBaseDir(sURI);
	if ( sBaseDir == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iRet = xgeResourceLoad(sURI, &tResource);
	if ( iRet != XGE_OK ) {
		xrtFree(sBaseDir);
		return iRet;
	}
	iRet = __xgeSvgLoadMemoryEx(pSvg, tResource.pData, tResource.iSize, sBaseDir);
	xgeResourceFree(&tResource);
	xrtFree(sBaseDir);
	return iRet;
}

int xgeSvgLoadCached(const char* sURI, xge_svg* ppSvg)
{
	xge_svg_cache_entry_t* pEntry;
	xge_svg pSvg;
	int iRet;

	if ( (sURI == NULL) || (ppSvg == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	for ( pEntry = g_xgeSvgCacheHead; pEntry != NULL; pEntry = pEntry->pNext ) {
		if ( strcmp(pEntry->sURI, sURI) == 0 ) {
			xgeSvgAddRef(pEntry->pSvg);
			*ppSvg = pEntry->pSvg;
			return XGE_OK;
		}
	}
	iRet = xgeSvgCreate(&pSvg);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = xgeSvgLoad(pSvg, sURI);
	if ( iRet != XGE_OK ) {
		xgeSvgDestroy(pSvg);
		return iRet;
	}
	pEntry = (xge_svg_cache_entry_t*)xrtCalloc(1, sizeof(*pEntry));
	if ( pEntry == NULL ) {
		xgeSvgDestroy(pSvg);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pEntry->sURI = __xgeStrDup(sURI);
	if ( pEntry->sURI == NULL ) {
		xrtFree(pEntry);
		xgeSvgDestroy(pSvg);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	xgeSvgAddRef(pSvg);
	pEntry->pSvg = pSvg;
	pEntry->pNext = g_xgeSvgCacheHead;
	g_xgeSvgCacheHead = pEntry;
	*ppSvg = pSvg;
	return XGE_OK;
}

int xgeSvgCacheInvalidate(const char* sURI)
{
	xge_svg_cache_entry_t** ppEntry;

	if ( sURI == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	ppEntry = &g_xgeSvgCacheHead;
	while ( *ppEntry != NULL ) {
		xge_svg_cache_entry_t* pEntry = *ppEntry;
		if ( strcmp(pEntry->sURI, sURI) == 0 ) {
			*ppEntry = pEntry->pNext;
			xgeSvgDestroy(pEntry->pSvg);
			xrtFree(pEntry->sURI);
			xrtFree(pEntry);
			return XGE_OK;
		}
		ppEntry = &pEntry->pNext;
	}
	return XGE_ERROR_NOT_FOUND;
}

void xgeSvgCacheClear(void)
{
	xge_svg_cache_entry_t* pEntry = g_xgeSvgCacheHead;

	while ( pEntry != NULL ) {
		xge_svg_cache_entry_t* pNext = pEntry->pNext;
		xgeSvgDestroy(pEntry->pSvg);
		xrtFree(pEntry->sURI);
		xrtFree(pEntry);
		pEntry = pNext;
	}
	g_xgeSvgCacheHead = NULL;
}

static xge_svg_group_item_t* __xgeSvgPaintCompositeWrapper(
	const xge_svg_draw_item_t* pItem
)
{
	xge_svg_draw_item_t* pChild;

	if ( (pItem == NULL) || (pItem->iType != XGE_SVG_DRAW_ITEM_GROUP) ||
	     (pItem->u.pGroup == NULL) || !pItem->u.pGroup->bPaintTransform ||
	     (pItem->u.pGroup->iItemCount != 1) ) {
		return NULL;
	}
	pChild = &pItem->u.pGroup->pItems[0];
	if ( (pChild->iType != XGE_SVG_DRAW_ITEM_GROUP) || (pChild->iId != 0) ||
	     (pChild->u.pGroup == NULL) || pChild->u.pGroup->bPaintTransform ||
	     pChild->u.pGroup->bPaintClipComposition ||
	     pChild->u.pGroup->bMasked || (pChild->u.pGroup->sMaskId[0] != '\0') ) {
		return NULL;
	}
	return pChild->u.pGroup;
}

static void __xgeSvgPaintTreeBindItems(
	xge_svg pSvg,
	xge_svg_draw_item_t* pItems,
	int iItemCount,
	xge_svg_draw_item_t* pParent
)
{
	int i;

	if ( !__xgeSvgValid(pSvg) || (pItems == NULL) || (iItemCount <= 0) ) return;
	for ( i = 0; i < iItemCount; i++ ) {
		xge_svg_draw_item_t* pItem = &pItems[i];

		pItem->pOwner = pSvg;
		pItem->pParentPaint = pParent;
		if ( (pItem->iType == XGE_SVG_DRAW_ITEM_GROUP) && (pItem->u.pGroup != NULL) ) {
			xge_svg_group_item_t* pWrapper = __xgeSvgPaintCompositeWrapper(pItem);

			if ( pWrapper != NULL ) {
				xge_svg_draw_item_t* pWrapperItem = &pItem->u.pGroup->pItems[0];

				pWrapperItem->pOwner = pSvg;
				pWrapperItem->pParentPaint = pItem;
				__xgeSvgPaintTreeBindItems(
					pSvg, pWrapper->pItems, pWrapper->iItemCount, pItem
				);
			} else {
				__xgeSvgPaintTreeBindItems(
					pSvg, pItem->u.pGroup->pItems, pItem->u.pGroup->iItemCount, pItem
				);
			}
		} else if ( (pItem->iType == XGE_SVG_DRAW_ITEM_FILTER) && (pItem->u.pFilter != NULL) ) {
			__xgeSvgPaintTreeBindItems(
				pSvg, pItem->u.pFilter->pItems, pItem->u.pFilter->iItemCount, pItem
			);
		}
	}
}

static void __xgeSvgPaintTreeBind(xge_svg pSvg)
{
	if ( !__xgeSvgValid(pSvg) ) return;
	pSvg->tPicturePaint.iType = XGE_SVG_PAINT_PICTURE;
	pSvg->tPicturePaint.pOwner = pSvg;
	pSvg->tPicturePaint.pParentPaint = NULL;
	pSvg->tRootPaint.iType = XGE_SVG_PAINT_SCENE;
	pSvg->tRootPaint.pOwner = pSvg;
	pSvg->tRootPaint.pParentPaint = &pSvg->tPicturePaint;
	pSvg->tContentPaint.iType = XGE_SVG_PAINT_SCENE;
	pSvg->tContentPaint.pOwner = pSvg;
	pSvg->tContentPaint.pParentPaint = &pSvg->tRootPaint;
	pSvg->tDocumentPaint.iType = XGE_SVG_PAINT_SCENE;
	pSvg->tDocumentPaint.pOwner = pSvg;
	pSvg->tDocumentPaint.pParentPaint = &pSvg->tContentPaint;
	__xgeSvgPaintTreeBindItems(
		pSvg, pSvg->pItems, pSvg->iItemCount, &pSvg->tDocumentPaint
	);
}

static xge_svg_draw_item_t* __xgeSvgPaintFindById(
	xge_svg_draw_item_t* pItems,
	int iItemCount,
	uint32_t iId
)
{
	int i;

	if ( (pItems == NULL) || (iItemCount <= 0) || (iId == 0) ) return NULL;
	for ( i = 0; i < iItemCount; i++ ) {
		xge_svg_draw_item_t* pItem = &pItems[i];
		xge_svg_draw_item_t* pFound = NULL;

		if ( pItem->iId == iId ) return pItem;
		if ( (pItem->iType == XGE_SVG_DRAW_ITEM_GROUP) && (pItem->u.pGroup != NULL) ) {
			pFound = __xgeSvgPaintFindById(pItem->u.pGroup->pItems, pItem->u.pGroup->iItemCount, iId);
		} else if ( (pItem->iType == XGE_SVG_DRAW_ITEM_FILTER) && (pItem->u.pFilter != NULL) ) {
			pFound = __xgeSvgPaintFindById(pItem->u.pFilter->pItems, pItem->u.pFilter->iItemCount, iId);
		}
		if ( pFound != NULL ) return pFound;
	}
	return NULL;
}

int xgeSvgPaintGetById(xge_svg pSvg, uint32_t iId, xge_svg_paint* ppPaint)
{
	xge_svg_draw_item_t* pPaint;

	if ( !__xgeSvgValid(pSvg) || (iId == 0) || (ppPaint == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppPaint = NULL;
	pPaint = __xgeSvgPaintFindById(pSvg->pItems, pSvg->iItemCount, iId);
	if ( pPaint == NULL ) return XGE_ERROR_NOT_FOUND;
	*ppPaint = pPaint;
	return XGE_OK;
}

int xgeSvgPaintGetByName(xge_svg pSvg, const char* sName, xge_svg_paint* ppPaint)
{
	if ( (sName == NULL) || (sName[0] == '\0') ) return XGE_ERROR_INVALID_ARGUMENT;
	return xgeSvgPaintGetById(pSvg, xgeShapeExIdFromName(sName), ppPaint);
}

int xgeSvgPaintGetPicture(xge_svg pSvg, xge_svg_paint* ppPaint)
{
	if ( !__xgeSvgValid(pSvg) || (ppPaint == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	*ppPaint = &pSvg->tPicturePaint;
	return XGE_OK;
}

int xgeSvgPaintIdGet(xge_svg_paint pPaint, uint32_t* pId)
{
	if ( (pPaint == NULL) || (pId == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	*pId = pPaint->iId;
	return XGE_OK;
}

int xgeSvgPaintNameGet(xge_svg_paint pPaint, const char** ppName)
{
	xge_svg pSvg;
	int i;

	if ( (pPaint == NULL) || (ppName == NULL) || !__xgeSvgValid(pPaint->pOwner) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppName = NULL;
	if ( pPaint->iId == 0 ) return XGE_ERROR_NOT_FOUND;
	pSvg = pPaint->pOwner;
	for ( i = 0; i < pSvg->iDefCount; i++ ) {
		const char* sName = pSvg->pDefs[i].sId;

		if ( (sName != NULL) &&
		     (xgeShapeExIdFromName(sName) == pPaint->iId) ) {
			*ppName = sName;
			return XGE_OK;
		}
	}
	return XGE_ERROR_NOT_FOUND;
}

int xgeSvgPaintTypeGet(xge_svg_paint pPaint, int* pType)
{
	if ( (pPaint == NULL) || (pType == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	*pType = ((pPaint->iType == XGE_SVG_DRAW_ITEM_SCENE) && (pPaint->pIdentityShape != NULL)) ?
		XGE_SVG_PAINT_SHAPE : pPaint->iType;
	return XGE_OK;
}

int xgeSvgPaintTransformSet(xge_svg_paint pPaint, const xge_shape_ex_matrix_t* pMatrix)
{
	if ( (pPaint == NULL) || !__xgeSvgValid(pPaint->pOwner) || (pMatrix == NULL) ||
	     !__xgeSvgFloatFinite(pMatrix->fA) || !__xgeSvgFloatFinite(pMatrix->fB) ||
	     !__xgeSvgFloatFinite(pMatrix->fC) || !__xgeSvgFloatFinite(pMatrix->fD) ||
	     !__xgeSvgFloatFinite(pMatrix->fE) || !__xgeSvgFloatFinite(pMatrix->fF) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	__xgeSvgPaintRuntimeStateEnsure(pPaint);
	pPaint->tRuntimeTransform = *pMatrix;
	pPaint->bRuntimeTransform = 1;
	pPaint->bRuntimeTransformOverride = 1;
	return XGE_OK;
}

int xgeSvgPaintTransformIdentity(xge_svg_paint pPaint)
{
	if ( (pPaint == NULL) || !__xgeSvgValid(pPaint->pOwner) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	__xgeSvgPaintRuntimeStateEnsure(pPaint);
	pPaint->fRuntimeTranslateX = 0.0f;
	pPaint->fRuntimeTranslateY = 0.0f;
	pPaint->fRuntimeScaleX = 1.0f;
	pPaint->fRuntimeScaleY = 1.0f;
	pPaint->fRuntimeRotateRadians = 0.0f;
	pPaint->tRuntimeTransform = __xgeSvgMatrixIdentity();
	pPaint->bRuntimeTransform = 1;
	pPaint->bRuntimeTransformOverride = 0;
	return XGE_OK;
}

int xgeSvgPaintTransformGet(xge_svg_paint pPaint, xge_shape_ex_matrix_t* pMatrix)
{
	if ( (pPaint == NULL) || (pMatrix == NULL) || !__xgeSvgValid(pPaint->pOwner) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pPaint->bRuntimeTransform ) {
		*pMatrix = pPaint->tRuntimeTransform;
		return XGE_OK;
	}
	return __xgeSvgPaintBaseTransformGet(pPaint, pMatrix);
}

static int __xgeSvgPaintTransformComponentAllowed(xge_svg_paint pPaint)
{
	xge_shape_ex_matrix_t tBase;
	int iRet;

	if ( pPaint->bRuntimeTransformOverride ) return XGE_ERROR_INVALID_STATE;
	if ( pPaint->bRuntimeTransform ) return XGE_OK;
	iRet = __xgeSvgPaintBaseTransformGet(pPaint, &tBase);
	if ( iRet != XGE_OK ) return iRet;
	return __xgeSvgMatrixIsIdentity(tBase) ? XGE_OK : XGE_ERROR_INVALID_STATE;
}

int xgeSvgPaintTransformTranslate(xge_svg_paint pPaint, float fTX, float fTY)
{
	int iRet;

	if ( (pPaint == NULL) || !__xgeSvgValid(pPaint->pOwner) ||
	     !__xgeSvgFloatFinite(fTX) || !__xgeSvgFloatFinite(fTY) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeSvgPaintTransformComponentAllowed(pPaint);
	if ( iRet != XGE_OK ) return iRet;
	__xgeSvgPaintRuntimeStateEnsure(pPaint);
	pPaint->fRuntimeTranslateX = fTX;
	pPaint->fRuntimeTranslateY = fTY;
	__xgeSvgPaintRuntimeTransformRebuild(pPaint);
	return XGE_OK;
}

int xgeSvgPaintTransformScale(xge_svg_paint pPaint, float fSX, float fSY)
{
	int iRet;

	if ( (pPaint == NULL) || !__xgeSvgValid(pPaint->pOwner) ||
	     !__xgeSvgFloatFinite(fSX) || !__xgeSvgFloatFinite(fSY) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeSvgPaintTransformComponentAllowed(pPaint);
	if ( iRet != XGE_OK ) return iRet;
	__xgeSvgPaintRuntimeStateEnsure(pPaint);
	pPaint->fRuntimeScaleX = fSX;
	pPaint->fRuntimeScaleY = fSY;
	__xgeSvgPaintRuntimeTransformRebuild(pPaint);
	return XGE_OK;
}

int xgeSvgPaintTransformRotate(xge_svg_paint pPaint, float fRadians)
{
	int iRet;

	if ( (pPaint == NULL) || !__xgeSvgValid(pPaint->pOwner) ||
	     !__xgeSvgFloatFinite(fRadians) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeSvgPaintTransformComponentAllowed(pPaint);
	if ( iRet != XGE_OK ) return iRet;
	__xgeSvgPaintRuntimeStateEnsure(pPaint);
	pPaint->fRuntimeRotateRadians = fRadians;
	__xgeSvgPaintRuntimeTransformRebuild(pPaint);
	return XGE_OK;
}

int xgeSvgPaintOpacitySet(xge_svg_paint pPaint, float fOpacity)
{
	int iRet = XGE_OK;

	if ( (pPaint == NULL) || !__xgeSvgValid(pPaint->pOwner) ||
	     !__xgeSvgFloatFinite(fOpacity) || (fOpacity < 0.0f) || (fOpacity > 1.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pPaint->pIdentityShape != NULL ) {
		iRet = xgeShapeExOpacity(pPaint->pIdentityShape, fOpacity);
	} else if ( (pPaint->iType == XGE_SVG_DRAW_ITEM_SHAPE) && (pPaint->u.pShape != NULL) ) {
		iRet = xgeShapeExOpacity(pPaint->u.pShape, fOpacity);
	} else if ( (pPaint->iType == XGE_SVG_DRAW_ITEM_SCENE) && (pPaint->u.pScene != NULL) ) {
		iRet = xgeShapeExSceneOpacity(pPaint->u.pScene, fOpacity);
	} else if ( (pPaint->iType == XGE_SVG_DRAW_ITEM_GROUP) && (pPaint->u.pGroup != NULL) ) {
		pPaint->u.pGroup->fOpacity = fOpacity;
		pPaint->u.pGroup->fPaintOpacity = fOpacity;
	} else if ( pPaint->iType == XGE_SVG_DRAW_ITEM_TEXT ) {
		pPaint->u.tText.tStyle.fOpacity = fOpacity;
	} else if ( pPaint->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE ) {
		pPaint->u.tImage.tStyle.fOpacity = fOpacity;
	} else if ( pPaint->iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE ) {
		pPaint->u.tRaster.tStyle.fOpacity = fOpacity;
	}
	if ( iRet != XGE_OK ) return iRet;
	__xgeSvgPaintRuntimeStateEnsure(pPaint);
	pPaint->fRuntimeOpacity = fOpacity;
	pPaint->bRuntimeOpacity = 1;
	return XGE_OK;
}

int xgeSvgPaintOpacityGet(xge_svg_paint pPaint, float* pOpacity)
{
	if ( (pPaint == NULL) || (pOpacity == NULL) || !__xgeSvgValid(pPaint->pOwner) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pPaint->bRuntimeOpacity ) {
		*pOpacity = pPaint->fRuntimeOpacity;
		return XGE_OK;
	}
	if ( pPaint->bPaintOpacity ) {
		*pOpacity = pPaint->fPaintOpacity;
		return XGE_OK;
	}
	if ( pPaint->pIdentityShape != NULL ) {
		return xgeShapeExOpacityGet(pPaint->pIdentityShape, pOpacity);
	}
	if ( (pPaint->iType == XGE_SVG_DRAW_ITEM_SHAPE) && (pPaint->u.pShape != NULL) ) {
		return xgeShapeExOpacityGet(pPaint->u.pShape, pOpacity);
	}
	if ( (pPaint->iType == XGE_SVG_DRAW_ITEM_SCENE) && (pPaint->u.pScene != NULL) ) {
		return xgeShapeExSceneOpacityGet(pPaint->u.pScene, pOpacity);
	}
	if ( (pPaint->iType == XGE_SVG_DRAW_ITEM_GROUP) && (pPaint->u.pGroup != NULL) ) {
		*pOpacity = pPaint->u.pGroup->fPaintOpacity;
		return XGE_OK;
	}
	if ( pPaint->iType == XGE_SVG_DRAW_ITEM_TEXT ) {
		*pOpacity = pPaint->u.tText.tStyle.fOpacity;
		return XGE_OK;
	}
	if ( pPaint->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE ) {
		*pOpacity = pPaint->u.tImage.tStyle.fOpacity;
		return XGE_OK;
	}
	if ( pPaint->iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE ) {
		*pOpacity = pPaint->u.tRaster.tStyle.fOpacity;
		return XGE_OK;
	}
	*pOpacity = 1.0f;
	return XGE_OK;
}

int xgeSvgPaintVisibleSet(xge_svg_paint pPaint, int bVisible)
{
	int iRet = XGE_OK;

	if ( (pPaint == NULL) || !__xgeSvgValid(pPaint->pOwner) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pPaint->pIdentityShape != NULL ) {
		iRet = xgeShapeExVisible(pPaint->pIdentityShape, bVisible);
	} else if ( (pPaint->iType == XGE_SVG_DRAW_ITEM_SHAPE) && (pPaint->u.pShape != NULL) ) {
		iRet = xgeShapeExVisible(pPaint->u.pShape, bVisible);
	} else if ( (pPaint->iType == XGE_SVG_DRAW_ITEM_SCENE) && (pPaint->u.pScene != NULL) ) {
		iRet = xgeShapeExSceneVisible(pPaint->u.pScene, bVisible);
	} else if ( pPaint->iType == XGE_SVG_DRAW_ITEM_TEXT ) {
		pPaint->u.tText.tStyle.bVisible = bVisible != 0;
		pPaint->u.tText.tStyle.bVisibility = bVisible != 0;
	} else if ( pPaint->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE ) {
		pPaint->u.tImage.tStyle.bVisible = bVisible != 0;
		pPaint->u.tImage.tStyle.bVisibility = bVisible != 0;
	} else if ( pPaint->iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE ) {
		pPaint->u.tRaster.tStyle.bVisible = bVisible != 0;
		pPaint->u.tRaster.tStyle.bVisibility = bVisible != 0;
	}
	if ( iRet != XGE_OK ) return iRet;
	__xgeSvgPaintRuntimeStateEnsure(pPaint);
	pPaint->iRuntimeVisible = bVisible != 0;
	pPaint->bRuntimeVisible = 1;
	return XGE_OK;
}

int xgeSvgPaintVisibleGet(xge_svg_paint pPaint, int* pVisible)
{
	if ( (pPaint == NULL) || (pVisible == NULL) || !__xgeSvgValid(pPaint->pOwner) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pPaint->bRuntimeVisible ) {
		*pVisible = pPaint->iRuntimeVisible;
		return XGE_OK;
	}
	if ( pPaint->pIdentityShape != NULL ) {
		return xgeShapeExVisibleGet(pPaint->pIdentityShape, pVisible);
	}
	if ( (pPaint->iType == XGE_SVG_DRAW_ITEM_SHAPE) && (pPaint->u.pShape != NULL) ) {
		return xgeShapeExVisibleGet(pPaint->u.pShape, pVisible);
	}
	if ( (pPaint->iType == XGE_SVG_DRAW_ITEM_SCENE) && (pPaint->u.pScene != NULL) ) {
		return xgeShapeExSceneVisibleGet(pPaint->u.pScene, pVisible);
	}
	if ( pPaint->iType == XGE_SVG_DRAW_ITEM_TEXT ) {
		*pVisible = pPaint->u.tText.tStyle.bVisible && pPaint->u.tText.tStyle.bVisibility;
		return XGE_OK;
	}
	if ( pPaint->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE ) {
		*pVisible = pPaint->u.tImage.tStyle.bVisible && pPaint->u.tImage.tStyle.bVisibility;
		return XGE_OK;
	}
	if ( pPaint->iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE ) {
		*pVisible = pPaint->u.tRaster.tStyle.bVisible && pPaint->u.tRaster.tStyle.bVisibility;
		return XGE_OK;
	}
	*pVisible = 1;
	return XGE_OK;
}

int xgeSvgPaintBlendSet(xge_svg_paint pPaint, int iBlend)
{
	int iRet = XGE_OK;

	if ( (pPaint == NULL) || !__xgeSvgValid(pPaint->pOwner) ||
	     (iBlend < XGE_BLEND_NONE) || (iBlend > XGE_BLEND_LUMINOSITY) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pPaint->pIdentityShape != NULL ) {
		iRet = xgeShapeExBlend(pPaint->pIdentityShape, iBlend);
	} else if ( (pPaint->iType == XGE_SVG_DRAW_ITEM_SHAPE) && (pPaint->u.pShape != NULL) ) {
		iRet = xgeShapeExBlend(pPaint->u.pShape, iBlend);
	} else if ( (pPaint->iType == XGE_SVG_DRAW_ITEM_SCENE) && (pPaint->u.pScene != NULL) ) {
		iRet = xgeShapeExSceneBlend(pPaint->u.pScene, iBlend);
	} else if ( pPaint->iType == XGE_SVG_DRAW_ITEM_TEXT ) {
		pPaint->u.tText.tStyle.iBlend = iBlend;
		pPaint->u.tText.tStyle.bBlendSet = 1;
	} else if ( pPaint->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE ) {
		pPaint->u.tImage.tStyle.iBlend = iBlend;
		pPaint->u.tImage.tStyle.bBlendSet = 1;
	} else if ( pPaint->iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE ) {
		pPaint->u.tRaster.tStyle.iBlend = iBlend;
		pPaint->u.tRaster.tStyle.bBlendSet = 1;
	} else if ( pPaint->iType == XGE_SVG_DRAW_ITEM_GROUP ) {
		pPaint->iBlend = iBlend;
		pPaint->bBlendSet = 1;
	}
	if ( iRet != XGE_OK ) return iRet;
	__xgeSvgPaintRuntimeStateEnsure(pPaint);
	pPaint->iRuntimeBlend = iBlend;
	pPaint->bRuntimeBlend = 1;
	return XGE_OK;
}

int xgeSvgPaintBlendGet(xge_svg_paint pPaint, int* pBlend)
{
	if ( (pPaint == NULL) || (pBlend == NULL) || !__xgeSvgValid(pPaint->pOwner) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pPaint->bRuntimeBlend ) {
		*pBlend = pPaint->iRuntimeBlend;
		return XGE_OK;
	}
	if ( pPaint->iType == XGE_SVG_DRAW_ITEM_TEXT ) {
		*pBlend = pPaint->u.tText.tStyle.bBlendSet ?
			pPaint->u.tText.tStyle.iBlend : XGE_BLEND_ALPHA;
		return XGE_OK;
	}
	if ( pPaint->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE ) {
		*pBlend = pPaint->u.tImage.tStyle.bBlendSet ?
			pPaint->u.tImage.tStyle.iBlend : XGE_BLEND_ALPHA;
		return XGE_OK;
	}
	if ( pPaint->iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE ) {
		*pBlend = pPaint->u.tRaster.tStyle.bBlendSet ?
			pPaint->u.tRaster.tStyle.iBlend : XGE_BLEND_ALPHA;
		return XGE_OK;
	}
	*pBlend = pPaint->bBlendSet ? pPaint->iBlend : XGE_BLEND_ALPHA;
	return XGE_OK;
}

static int __xgeSvgPaintMaskGetItem(
	xge_svg pSvg,
	const xge_svg_draw_item_t* pItem,
	int* pMethod,
	int* pTargetType,
	xge_shape_ex* ppTargetShape,
	xge_shape_ex_scene* ppTargetScene,
	int iDepth
)
{
	int iRet;
	int i;

	if ( (pItem == NULL) || (iDepth > XGE_SVG_BOUNDS_MAX_DEPTH) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pItem->pIdentityShape != NULL ) {
		return xgeShapeExMaskGet(
			pItem->pIdentityShape, pMethod, pTargetType,
			ppTargetShape, ppTargetScene
		);
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_SHAPE) && (pItem->u.pShape != NULL) ) {
		return xgeShapeExMaskGet(
			pItem->u.pShape, pMethod, pTargetType,
			ppTargetShape, ppTargetScene
		);
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_SCENE) && (pItem->u.pScene != NULL) ) {
		iRet = xgeShapeExSceneMaskGet(
			pItem->u.pScene, pMethod, pTargetType,
			ppTargetShape, ppTargetScene
		);
		if ( (iRet != XGE_OK) || (*pTargetType != XGE_SHAPE_EX_MASK_TARGET_NONE) ) return iRet;
		for ( i = 0; i < pItem->u.pScene->iChildCount; i++ ) {
			xge_shape_ex_scene_child_t* pChild = &pItem->u.pScene->pChildren[i];

			if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE ) {
				iRet = xgeShapeExMaskGet(
					pChild->pShape, pMethod, pTargetType,
					ppTargetShape, ppTargetScene
				);
			} else {
				iRet = xgeShapeExSceneMaskGet(
					pChild->pScene, pMethod, pTargetType,
					ppTargetShape, ppTargetScene
				);
			}
			if ( (iRet != XGE_OK) || (*pTargetType != XGE_SHAPE_EX_MASK_TARGET_NONE) ) return iRet;
		}
		return XGE_OK;
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_GROUP) && (pItem->u.pGroup != NULL) ) {
		xge_svg_group_item_t* pGroup = pItem->u.pGroup;

		if ( pGroup->sMaskId[0] != '\0' ) {
			xge_rect_t tBounds;
			int bHasBounds = 0;

			iRet = __xgeSvgGroupChildrenBoundsWithParent(
				pSvg, pGroup, __xgeSvgMatrixIdentity(), 0.25f,
				&tBounds, &bHasBounds, iDepth + 1
			);
			if ( iRet != XGE_OK ) return iRet;
			iRet = __xgeSvgGroupMaskSceneEnsure(pSvg, pGroup, tBounds);
			if ( iRet != XGE_OK ) return iRet;
			if ( pGroup->pMaskScene != NULL ) {
				*pMethod = pGroup->iMaskMethod;
				*pTargetType = XGE_SHAPE_EX_MASK_TARGET_SCENE;
				*ppTargetScene = pGroup->pMaskScene;
				return XGE_OK;
			}
		}
		for ( i = 0; i < pGroup->iItemCount; i++ ) {
			iRet = __xgeSvgPaintMaskGetItem(
				pSvg, &pGroup->pItems[i], pMethod, pTargetType,
				ppTargetShape, ppTargetScene, iDepth + 1
			);
			if ( (iRet != XGE_OK) || (*pTargetType != XGE_SHAPE_EX_MASK_TARGET_NONE) ) return iRet;
		}
		return XGE_OK;
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_FILTER) && (pItem->u.pFilter != NULL) ) {
		for ( i = 0; i < pItem->u.pFilter->iItemCount; i++ ) {
			iRet = __xgeSvgPaintMaskGetItem(
				pSvg, &pItem->u.pFilter->pItems[i], pMethod, pTargetType,
				ppTargetShape, ppTargetScene, iDepth + 1
			);
			if ( (iRet != XGE_OK) || (*pTargetType != XGE_SHAPE_EX_MASK_TARGET_NONE) ) return iRet;
		}
		return XGE_OK;
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_TEXT) && (pItem->u.tText.pMaskScene != NULL) ) {
		*pMethod = pItem->u.tText.iMaskMethod;
		*pTargetType = XGE_SHAPE_EX_MASK_TARGET_SCENE;
		*ppTargetScene = pItem->u.tText.pMaskScene;
	} else if ( (pItem->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE) && (pItem->u.tImage.pMaskScene != NULL) ) {
		*pMethod = pItem->u.tImage.iMaskMethod;
		*pTargetType = XGE_SHAPE_EX_MASK_TARGET_SCENE;
		*ppTargetScene = pItem->u.tImage.pMaskScene;
	} else if ( (pItem->iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE) && (pItem->u.tRaster.pMaskScene != NULL) ) {
		*pMethod = pItem->u.tRaster.iMaskMethod;
		*pTargetType = XGE_SHAPE_EX_MASK_TARGET_SCENE;
		*ppTargetScene = pItem->u.tRaster.pMaskScene;
	}
	return XGE_OK;
}

static void __xgeSvgPaintGroupMaskStateClear(xge_svg_group_item_t* pGroup)
{
	int i;

	if ( pGroup == NULL ) return;
	xgeShapeExSceneDestroy(pGroup->pMaskScene);
	pGroup->pMaskScene = NULL;
	pGroup->sMaskId[0] = '\0';
	pGroup->iMaskMethod = XGE_SHAPE_EX_MASK_NONE;
	pGroup->bMaskResolved = 0;
	pGroup->bMaskEmpty = 0;
	pGroup->bMasked = 0;
	for ( i = 0; i < pGroup->iItemCount; i++ ) {
		if ( __xgeSvgDrawItemHasMask(&pGroup->pItems[i]) ) {
			pGroup->bMasked = 1;
			break;
		}
	}
}

static int __xgeSvgPaintOwnedMaskClear(
	xge_svg_draw_item_t* pItem,
	int* pCleared,
	int iDepth
)
{
	int i;
	int iRet;

	if ( (pItem == NULL) || (pCleared == NULL) ||
	     (iDepth > XGE_SVG_BOUNDS_MAX_DEPTH) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_GROUP) && (pItem->u.pGroup != NULL) ) {
		xge_svg_group_item_t* pGroup = pItem->u.pGroup;

		if ( (pGroup->sMaskId[0] != '\0') || (pGroup->pMaskScene != NULL) ) {
			__xgeSvgPaintGroupMaskStateClear(pGroup);
			*pCleared = 1;
			return XGE_OK;
		}
		for ( i = 0; i < pGroup->iItemCount; i++ ) {
			iRet = __xgeSvgPaintOwnedMaskClear(
				&pGroup->pItems[i], pCleared, iDepth + 1
			);
			if ( (iRet != XGE_OK) || *pCleared ) return iRet;
		}
	} else if ( (pItem->iType == XGE_SVG_DRAW_ITEM_FILTER) &&
	            (pItem->u.pFilter != NULL) ) {
		for ( i = 0; i < pItem->u.pFilter->iItemCount; i++ ) {
			iRet = __xgeSvgPaintOwnedMaskClear(
				&pItem->u.pFilter->pItems[i], pCleared, iDepth + 1
			);
			if ( (iRet != XGE_OK) || *pCleared ) return iRet;
		}
	}
	return XGE_OK;
}

static int __xgeSvgPaintBaseMaskClear(xge_svg_draw_item_t* pPaint)
{
	int iTargetType = XGE_SHAPE_EX_MASK_TARGET_NONE;

	if ( pPaint == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( pPaint->pIdentityShape != NULL ) return xgeShapeExMaskClear(pPaint->pIdentityShape);
	if ( (pPaint->iType == XGE_SVG_DRAW_ITEM_SHAPE) && (pPaint->u.pShape != NULL) ) {
		return xgeShapeExMaskClear(pPaint->u.pShape);
	}
	if ( (pPaint->iType == XGE_SVG_DRAW_ITEM_SCENE) && (pPaint->u.pScene != NULL) ) {
		if ( xgeShapeExSceneMaskGet(
			pPaint->u.pScene, NULL, &iTargetType, NULL, NULL
		) != XGE_OK ) return XGE_ERROR_INVALID_STATE;
		if ( iTargetType != XGE_SHAPE_EX_MASK_TARGET_NONE ) {
			return xgeShapeExSceneMaskClear(pPaint->u.pScene);
		}
		return XGE_OK;
	}
	if ( (pPaint->iType == XGE_SVG_DRAW_ITEM_GROUP) && (pPaint->u.pGroup != NULL) ) {
		xge_svg_group_item_t* pGroup = pPaint->u.pGroup;

		if ( pGroup->bPaintOwnsMaskComposition ) {
			int bCleared = 0;
			int iRet = XGE_OK;
			int i;

			for ( i = 0; i < pGroup->iItemCount; i++ ) {
				iRet = __xgeSvgPaintOwnedMaskClear(
					&pGroup->pItems[i], &bCleared, 0
				);
				if ( (iRet != XGE_OK) || bCleared ) break;
			}
			if ( iRet != XGE_OK ) return iRet;
			pGroup->bPaintOwnsMaskComposition = 0;
			pGroup->bMasked = 0;
			for ( i = 0; i < pGroup->iItemCount; i++ ) {
				if ( __xgeSvgDrawItemHasMask(&pGroup->pItems[i]) ) {
					pGroup->bMasked = 1;
					break;
				}
			}
		} else if ( (pGroup->sMaskId[0] != '\0') || (pGroup->pMaskScene != NULL) ) {
			__xgeSvgPaintGroupMaskStateClear(pGroup);
		}
		return XGE_OK;
	}
	if ( (pPaint->iType == XGE_SVG_DRAW_ITEM_FILTER) && (pPaint->u.pFilter != NULL) ) {
		if ( pPaint->u.pFilter->iItemCount > 0 ) {
			return __xgeSvgPaintBaseMaskClear(&pPaint->u.pFilter->pItems[0]);
		}
		return XGE_OK;
	}
	if ( pPaint->iType == XGE_SVG_DRAW_ITEM_TEXT ) {
		xgeShapeExSceneDestroy(pPaint->u.tText.pMaskScene);
		pPaint->u.tText.pMaskScene = NULL;
		pPaint->u.tText.iMaskMethod = XGE_SHAPE_EX_MASK_NONE;
	} else if ( pPaint->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE ) {
		xgeShapeExSceneDestroy(pPaint->u.tImage.pMaskScene);
		pPaint->u.tImage.pMaskScene = NULL;
		pPaint->u.tImage.iMaskMethod = XGE_SHAPE_EX_MASK_NONE;
	} else if ( pPaint->iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE ) {
		xgeShapeExSceneDestroy(pPaint->u.tRaster.pMaskScene);
		pPaint->u.tRaster.pMaskScene = NULL;
		pPaint->u.tRaster.iMaskMethod = XGE_SHAPE_EX_MASK_NONE;
	}
	return XGE_OK;
}

static int __xgeSvgPaintMaskOwnerShapeCreate(
	xge_shape_ex pTarget,
	xge_shape_ex_scene* ppOwner
)
{
	xge_shape_ex_scene pOwner;
	int iRet;

	if ( (pTarget == NULL) || (ppOwner == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	*ppOwner = NULL;
	iRet = xgeShapeExSceneCreate(&pOwner);
	if ( iRet != XGE_OK ) return iRet;
	iRet = xgeShapeExSceneAdd(pOwner, pTarget);
	if ( iRet != XGE_OK ) {
		xgeShapeExSceneDestroy(pOwner);
		return iRet;
	}
	*ppOwner = pOwner;
	return XGE_OK;
}

static int __xgeSvgPaintMaskOwnerSceneCreate(
	xge_shape_ex_scene pTarget,
	xge_shape_ex_scene* ppOwner
)
{
	xge_shape_ex_scene pOwner;
	int iRet;

	if ( (pTarget == NULL) || (ppOwner == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	*ppOwner = NULL;
	iRet = xgeShapeExSceneCreate(&pOwner);
	if ( iRet != XGE_OK ) return iRet;
	iRet = xgeShapeExSceneAddScene(pOwner, pTarget);
	if ( iRet != XGE_OK ) {
		xgeShapeExSceneDestroy(pOwner);
		return iRet;
	}
	*ppOwner = pOwner;
	return XGE_OK;
}

int xgeSvgPaintMaskShapeSet(xge_svg_paint pPaint, xge_shape_ex pTarget, int iMethod)
{
	xge_shape_ex_scene pOwner;
	int iRet;

	if ( (pPaint == NULL) || !__xgeSvgValid(pPaint->pOwner) || (pTarget == NULL) ||
	     (iMethod < XGE_SHAPE_EX_MASK_ALPHA) || (iMethod > XGE_SHAPE_EX_MASK_DARKEN) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeSvgPaintMaskOwnerShapeCreate(pTarget, &pOwner);
	if ( iRet != XGE_OK ) return iRet;
	iRet = __xgeSvgPaintBaseMaskClear(pPaint);
	if ( iRet != XGE_OK ) {
		xgeShapeExSceneDestroy(pOwner);
		return iRet;
	}
	xgeShapeExSceneDestroy(pPaint->pRuntimeMaskOwnerScene);
	__xgeSvgPaintRuntimeStateEnsure(pPaint);
	pPaint->pRuntimeMaskOwnerScene = pOwner;
	pPaint->pRuntimeMaskShape = pTarget;
	pPaint->pRuntimeMaskTargetScene = NULL;
	pPaint->iRuntimeMaskMethod = iMethod;
	pPaint->iRuntimeMaskTargetType = XGE_SHAPE_EX_MASK_TARGET_SHAPE;
	pPaint->bRuntimeMaskSet = 1;
	return XGE_OK;
}

int xgeSvgPaintMaskSceneSet(xge_svg_paint pPaint, xge_shape_ex_scene pTarget, int iMethod)
{
	xge_shape_ex_scene pOwner;
	int iRet;

	if ( (pPaint == NULL) || !__xgeSvgValid(pPaint->pOwner) || (pTarget == NULL) ||
	     (iMethod < XGE_SHAPE_EX_MASK_ALPHA) || (iMethod > XGE_SHAPE_EX_MASK_DARKEN) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeSvgPaintMaskOwnerSceneCreate(pTarget, &pOwner);
	if ( iRet != XGE_OK ) return iRet;
	iRet = __xgeSvgPaintBaseMaskClear(pPaint);
	if ( iRet != XGE_OK ) {
		xgeShapeExSceneDestroy(pOwner);
		return iRet;
	}
	xgeShapeExSceneDestroy(pPaint->pRuntimeMaskOwnerScene);
	__xgeSvgPaintRuntimeStateEnsure(pPaint);
	pPaint->pRuntimeMaskOwnerScene = pOwner;
	pPaint->pRuntimeMaskShape = NULL;
	pPaint->pRuntimeMaskTargetScene = pTarget;
	pPaint->iRuntimeMaskMethod = iMethod;
	pPaint->iRuntimeMaskTargetType = XGE_SHAPE_EX_MASK_TARGET_SCENE;
	pPaint->bRuntimeMaskSet = 1;
	return XGE_OK;
}

int xgeSvgPaintMaskClear(xge_svg_paint pPaint)
{
	int iRet;

	if ( (pPaint == NULL) || !__xgeSvgValid(pPaint->pOwner) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeSvgPaintBaseMaskClear(pPaint);
	if ( iRet != XGE_OK ) return iRet;
	xgeShapeExSceneDestroy(pPaint->pRuntimeMaskOwnerScene);
	pPaint->pRuntimeMaskOwnerScene = NULL;
	pPaint->pRuntimeMaskShape = NULL;
	pPaint->pRuntimeMaskTargetScene = NULL;
	pPaint->iRuntimeMaskMethod = XGE_SHAPE_EX_MASK_NONE;
	pPaint->iRuntimeMaskTargetType = XGE_SHAPE_EX_MASK_TARGET_NONE;
	pPaint->bRuntimeMaskSet = 1;
	return XGE_OK;
}

int xgeSvgPaintMaskGet(xge_svg_paint pPaint, int* pMethod, int* pTargetType, xge_shape_ex* ppTargetShape, xge_shape_ex_scene* ppTargetScene)
{
	int iMethod;
	int iTargetType;
	xge_shape_ex pTargetShape;
	xge_shape_ex_scene pTargetScene;

	if ( (pPaint == NULL) || !__xgeSvgValid(pPaint->pOwner) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pPaint->bRuntimeMaskSet ) {
		if ( pMethod != NULL ) *pMethod = pPaint->iRuntimeMaskMethod;
		if ( pTargetType != NULL ) *pTargetType = pPaint->iRuntimeMaskTargetType;
		if ( ppTargetShape != NULL ) *ppTargetShape = pPaint->pRuntimeMaskShape;
		if ( ppTargetScene != NULL ) *ppTargetScene = pPaint->pRuntimeMaskTargetScene;
		return XGE_OK;
	}
	iMethod = XGE_SHAPE_EX_MASK_NONE;
	iTargetType = XGE_SHAPE_EX_MASK_TARGET_NONE;
	pTargetShape = NULL;
	pTargetScene = NULL;
	{
		int iRet = __xgeSvgPaintMaskGetItem(
			pPaint->pOwner, pPaint, &iMethod, &iTargetType,
			&pTargetShape, &pTargetScene, 0
		);
		if ( iRet != XGE_OK ) return iRet;
	}
	if ( pMethod != NULL ) *pMethod = iMethod;
	if ( pTargetType != NULL ) *pTargetType = iTargetType;
	if ( ppTargetShape != NULL ) *ppTargetShape = pTargetShape;
	if ( ppTargetScene != NULL ) *ppTargetScene = pTargetScene;
	return XGE_OK;
}

static int __xgeSvgPaintClipSource(
	const xge_svg_draw_item_t* pItem,
	xge_shape_ex* ppShape,
	xge_shape_ex_scene* ppScene,
	int iDepth
)
{
	int i;

	if ( (pItem == NULL) || (iDepth > XGE_SVG_BOUNDS_MAX_DEPTH) ) return XGE_ERROR_INVALID_ARGUMENT;
	*ppShape = NULL;
	*ppScene = NULL;
	if ( pItem->pIdentityShape != NULL ) {
		*ppShape = pItem->pIdentityShape;
		return XGE_OK;
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_SHAPE) && (pItem->u.pShape != NULL) ) {
		*ppShape = pItem->u.pShape;
		return XGE_OK;
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_SCENE) && (pItem->u.pScene != NULL) ) {
		int iCount = 0;
		int bRect = 0;
		xge_rect_t tRect;

		xgeShapeExSceneClipRectGet(pItem->u.pScene, &tRect, &bRect);
		xgeShapeExSceneClipShapeGetCount(pItem->u.pScene, &iCount);
		if ( bRect || (iCount > 0) ) {
			*ppScene = pItem->u.pScene;
			return XGE_OK;
		}
		for ( i = 0; i < pItem->u.pScene->iChildCount; i++ ) {
			xge_shape_ex_scene_child_t* pChild = &pItem->u.pScene->pChildren[i];
			if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE ) *ppShape = pChild->pShape;
			else *ppScene = pChild->pScene;
			return XGE_OK;
		}
		return XGE_OK;
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_GROUP) && (pItem->u.pGroup != NULL) ) {
		for ( i = 0; i < pItem->u.pGroup->iItemCount; i++ ) {
			__xgeSvgPaintClipSource(&pItem->u.pGroup->pItems[i], ppShape, ppScene, iDepth + 1);
			if ( (*ppShape != NULL) || (*ppScene != NULL) ) return XGE_OK;
		}
	} else if ( (pItem->iType == XGE_SVG_DRAW_ITEM_FILTER) && (pItem->u.pFilter != NULL) ) {
		for ( i = 0; i < pItem->u.pFilter->iItemCount; i++ ) {
			__xgeSvgPaintClipSource(&pItem->u.pFilter->pItems[i], ppShape, ppScene, iDepth + 1);
			if ( (*ppShape != NULL) || (*ppScene != NULL) ) return XGE_OK;
		}
	}
	return XGE_OK;
}

static int __xgeSvgPaintClipSubtreeClear(
	xge_svg_draw_item_t* pItem,
	int iDepth
)
{
	int i;
	int iRet;

	if ( (pItem == NULL) || (iDepth > XGE_SVG_BOUNDS_MAX_DEPTH) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_GROUP) &&
	     (pItem->u.pGroup != NULL) &&
	     pItem->u.pGroup->bPaintClipComposition ) {
		return XGE_OK;
	}
	if ( pItem->pIdentityShape != NULL ) {
		iRet = xgeShapeExClipClear(pItem->pIdentityShape);
		if ( iRet != XGE_OK ) return iRet;
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_SHAPE) && (pItem->u.pShape != NULL) ) {
		return xgeShapeExClipClear(pItem->u.pShape);
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_SCENE) && (pItem->u.pScene != NULL) ) {
		iRet = xgeShapeExSceneClipClear(pItem->u.pScene);
		if ( iRet != XGE_OK ) return iRet;
		for ( i = 0; i < pItem->u.pScene->iChildCount; i++ ) {
			xge_shape_ex_scene_child_t* pChild = &pItem->u.pScene->pChildren[i];

			if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE ) {
				iRet = xgeShapeExClipClear(pChild->pShape);
			} else {
				iRet = xgeShapeExSceneClipClear(pChild->pScene);
			}
			if ( iRet != XGE_OK ) return iRet;
		}
		return XGE_OK;
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_GROUP) && (pItem->u.pGroup != NULL) ) {
		for ( i = 0; i < pItem->u.pGroup->iItemCount; i++ ) {
			iRet = __xgeSvgPaintClipSubtreeClear(
				&pItem->u.pGroup->pItems[i], iDepth + 1
			);
			if ( iRet != XGE_OK ) return iRet;
		}
		return XGE_OK;
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_FILTER) && (pItem->u.pFilter != NULL) ) {
		for ( i = 0; i < pItem->u.pFilter->iItemCount; i++ ) {
			iRet = __xgeSvgPaintClipSubtreeClear(
				&pItem->u.pFilter->pItems[i], iDepth + 1
			);
			if ( iRet != XGE_OK ) return iRet;
		}
		return XGE_OK;
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_TEXT ) {
		pItem->u.tText.bClipRect = 0;
		memset(&pItem->u.tText.tClipRect, 0, sizeof(pItem->u.tText.tClipRect));
	} else if ( pItem->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE ) {
		pItem->u.tImage.bClipRect = 0;
		memset(&pItem->u.tImage.tClipRect, 0, sizeof(pItem->u.tImage.tClipRect));
	} else if ( pItem->iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE ) {
		pItem->u.tRaster.bClipRect = 0;
		memset(&pItem->u.tRaster.tClipRect, 0, sizeof(pItem->u.tRaster.tClipRect));
	}
	return XGE_OK;
}

static int __xgeSvgPaintOwnedClipClear(
	xge_svg_draw_item_t* pItem,
	int* pCleared,
	int iDepth
)
{
	int i;
	int iRet;

	if ( (pItem == NULL) || (pCleared == NULL) ||
	     (iDepth > XGE_SVG_BOUNDS_MAX_DEPTH) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_GROUP) && (pItem->u.pGroup != NULL) ) {
		xge_svg_group_item_t* pGroup = pItem->u.pGroup;

		if ( pGroup->bPaintClipComposition ) {
			for ( i = 0; i < pGroup->iItemCount; i++ ) {
				iRet = __xgeSvgPaintClipSubtreeClear(
					&pGroup->pItems[i], iDepth + 1
				);
				if ( iRet != XGE_OK ) return iRet;
			}
			pGroup->bPaintClipComposition = 0;
			*pCleared = 1;
			return XGE_OK;
		}
		for ( i = 0; i < pGroup->iItemCount; i++ ) {
			iRet = __xgeSvgPaintOwnedClipClear(
				&pGroup->pItems[i], pCleared, iDepth + 1
			);
			if ( (iRet != XGE_OK) || *pCleared ) return iRet;
		}
	} else if ( (pItem->iType == XGE_SVG_DRAW_ITEM_FILTER) &&
	            (pItem->u.pFilter != NULL) ) {
		for ( i = 0; i < pItem->u.pFilter->iItemCount; i++ ) {
			iRet = __xgeSvgPaintOwnedClipClear(
				&pItem->u.pFilter->pItems[i], pCleared, iDepth + 1
			);
			if ( (iRet != XGE_OK) || *pCleared ) return iRet;
		}
	}
	return XGE_OK;
}

static int __xgeSvgPaintBaseClipClear(xge_svg_paint pPaint)
{
	xge_shape_ex pShape;
	xge_shape_ex_scene pScene;
	int iRet = XGE_OK;

	if ( pPaint == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( (pPaint->iType == XGE_SVG_DRAW_ITEM_GROUP) && (pPaint->u.pGroup != NULL) &&
	     pPaint->u.pGroup->bPaintOwnsClipComposition ) {
		int bCleared = 0;
		int i;

		for ( i = 0; i < pPaint->u.pGroup->iItemCount; i++ ) {
			iRet = __xgeSvgPaintOwnedClipClear(
				&pPaint->u.pGroup->pItems[i], &bCleared, 0
			);
			if ( (iRet != XGE_OK) || bCleared ) break;
		}
		if ( iRet != XGE_OK ) return iRet;
		pPaint->u.pGroup->bPaintOwnsClipComposition = 0;
		return XGE_OK;
	}
	iRet = __xgeSvgPaintClipSource(pPaint, &pShape, &pScene, 0);
	if ( iRet != XGE_OK ) return iRet;
	if ( pShape != NULL ) return xgeShapeExClipClear(pShape);
	if ( pScene != NULL ) return xgeShapeExSceneClipClear(pScene);
	if ( pPaint->iType == XGE_SVG_DRAW_ITEM_TEXT ) {
		pPaint->u.tText.bClipRect = 0;
		memset(&pPaint->u.tText.tClipRect, 0, sizeof(pPaint->u.tText.tClipRect));
	} else if ( pPaint->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE ) {
		pPaint->u.tImage.bClipRect = 0;
		memset(&pPaint->u.tImage.tClipRect, 0, sizeof(pPaint->u.tImage.tClipRect));
	} else if ( pPaint->iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE ) {
		pPaint->u.tRaster.bClipRect = 0;
		memset(&pPaint->u.tRaster.tClipRect, 0, sizeof(pPaint->u.tRaster.tClipRect));
	}
	return XGE_OK;
}

int xgeSvgPaintClipShapeSet(xge_svg_paint pPaint, xge_shape_ex pClipShape)
{
	xge_shape_ex_scene pOwner;
	int iRet;

	if ( (pPaint == NULL) || !__xgeSvgValid(pPaint->pOwner) || (pClipShape == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeSvgPaintMaskOwnerShapeCreate(pClipShape, &pOwner);
	if ( iRet != XGE_OK ) return iRet;
	iRet = __xgeSvgPaintBaseClipClear(pPaint);
	if ( iRet != XGE_OK ) {
		xgeShapeExSceneDestroy(pOwner);
		return iRet;
	}
	xgeShapeExSceneDestroy(pPaint->pRuntimeClipOwnerScene);
	__xgeSvgPaintRuntimeStateEnsure(pPaint);
	pPaint->pRuntimeClipOwnerScene = pOwner;
	pPaint->pRuntimeClipShape = pClipShape;
	pPaint->bRuntimeClipSet = 1;
	return XGE_OK;
}

int xgeSvgPaintClipClear(xge_svg_paint pPaint)
{
	int iRet;

	if ( (pPaint == NULL) || !__xgeSvgValid(pPaint->pOwner) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeSvgPaintBaseClipClear(pPaint);
	if ( iRet != XGE_OK ) return iRet;
	xgeShapeExSceneDestroy(pPaint->pRuntimeClipOwnerScene);
	pPaint->pRuntimeClipOwnerScene = NULL;
	pPaint->pRuntimeClipShape = NULL;
	pPaint->bRuntimeClipSet = 1;
	return XGE_OK;
}

int xgeSvgPaintClipRectGet(xge_svg_paint pPaint, xge_rect_t* pRect, int* pEnabled)
{
	xge_shape_ex pShape;
	xge_shape_ex_scene pScene;
	int iRet;

	if ( (pPaint == NULL) || (pRect == NULL) || (pEnabled == NULL) ||
	     !__xgeSvgValid(pPaint->pOwner) ) return XGE_ERROR_INVALID_ARGUMENT;
	*pRect = (xge_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	*pEnabled = 0;
	if ( pPaint->bRuntimeClipSet ) return XGE_OK;
	iRet = __xgeSvgPaintClipSource(pPaint, &pShape, &pScene, 0);
	if ( iRet != XGE_OK ) return iRet;
	if ( pShape != NULL ) return xgeShapeExClipRectGet(pShape, pRect, pEnabled);
	if ( pScene != NULL ) return xgeShapeExSceneClipRectGet(pScene, pRect, pEnabled);
	if ( pPaint->iType == XGE_SVG_DRAW_ITEM_TEXT ) {
		*pRect = pPaint->u.tText.tClipRect;
		*pEnabled = pPaint->u.tText.bClipRect;
	} else if ( pPaint->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE ) {
		*pRect = pPaint->u.tImage.tClipRect;
		*pEnabled = pPaint->u.tImage.bClipRect;
	} else if ( pPaint->iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE ) {
		*pRect = pPaint->u.tRaster.tClipRect;
		*pEnabled = pPaint->u.tRaster.bClipRect;
	}
	return XGE_OK;
}

int xgeSvgPaintClipShapeGetCount(xge_svg_paint pPaint, int* pCount)
{
	xge_shape_ex pShape;
	xge_shape_ex_scene pScene;
	int iRet;

	if ( (pPaint == NULL) || (pCount == NULL) || !__xgeSvgValid(pPaint->pOwner) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pCount = 0;
	if ( pPaint->bRuntimeClipSet ) {
		*pCount = pPaint->pRuntimeClipShape != NULL ? 1 : 0;
		return XGE_OK;
	}
	iRet = __xgeSvgPaintClipSource(pPaint, &pShape, &pScene, 0);
	if ( iRet != XGE_OK ) return iRet;
	if ( pShape != NULL ) return xgeShapeExClipShapeGetCount(pShape, pCount);
	if ( pScene != NULL ) return xgeShapeExSceneClipShapeGetCount(pScene, pCount);
	return XGE_OK;
}

int xgeSvgPaintClipShapeGetAt(xge_svg_paint pPaint, int iIndex, xge_shape_ex* ppClipShape)
{
	return xgeSvgPaintClipShapeGetAtEx(pPaint, iIndex, ppClipShape, NULL);
}

int xgeSvgPaintClipShapeGetAtEx(xge_svg_paint pPaint, int iIndex, xge_shape_ex* ppClipShape, int* pMode)
{
	xge_shape_ex pShape;
	xge_shape_ex_scene pScene;
	int iRet;

	if ( (pPaint == NULL) || (ppClipShape == NULL) || !__xgeSvgValid(pPaint->pOwner) ||
	     (iIndex < 0) ) return XGE_ERROR_INVALID_ARGUMENT;
	*ppClipShape = NULL;
	if ( pPaint->bRuntimeClipSet ) {
		if ( (iIndex != 0) || (pPaint->pRuntimeClipShape == NULL) ) {
			return XGE_ERROR_NOT_FOUND;
		}
		*ppClipShape = pPaint->pRuntimeClipShape;
		if ( pMode != NULL ) *pMode = XGE_SHAPE_EX_CLIP_INTERSECT;
		return XGE_OK;
	}
	iRet = __xgeSvgPaintClipSource(pPaint, &pShape, &pScene, 0);
	if ( iRet != XGE_OK ) return iRet;
	if ( pShape != NULL ) return xgeShapeExClipShapeGetAtEx(pShape, iIndex, ppClipShape, pMode);
	if ( pScene != NULL ) return xgeShapeExSceneClipShapeGetAtEx(pScene, iIndex, ppClipShape, pMode);
	return XGE_ERROR_NOT_FOUND;
}

int xgeSvgPaintShapeGet(xge_svg_paint pPaint, xge_shape_ex* ppShape)
{
	if ( (pPaint == NULL) || (ppShape == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	*ppShape = NULL;
	if ( pPaint->pIdentityShape == NULL ) return XGE_ERROR_NOT_FOUND;
	*ppShape = pPaint->pIdentityShape;
	return XGE_OK;
}

int xgeSvgPaintOwnerGet(xge_svg_paint pPaint, xge_svg* ppSvg)
{
	if ( (pPaint == NULL) || (ppSvg == NULL) || !__xgeSvgValid(pPaint->pOwner) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppSvg = pPaint->pOwner;
	return XGE_OK;
}

int xgeSvgPaintParentGet(xge_svg_paint pPaint, xge_svg_paint* ppParent)
{
	if ( (pPaint == NULL) || (ppParent == NULL) || !__xgeSvgValid(pPaint->pOwner) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppParent = pPaint->pParentPaint;
	return XGE_OK;
}

static void __xgeSvgPaintChildren(
	xge_svg_paint pPaint,
	xge_svg_draw_item_t** ppItems,
	int* pCount
)
{
	*ppItems = NULL;
	*pCount = 0;
	if ( pPaint == &pPaint->pOwner->tRootPaint ) {
		*ppItems = &pPaint->pOwner->tContentPaint;
		*pCount = 1;
	} else if ( pPaint == &pPaint->pOwner->tContentPaint ) {
		*ppItems = &pPaint->pOwner->tDocumentPaint;
		*pCount = 1;
	} else if ( pPaint == &pPaint->pOwner->tDocumentPaint ) {
		*ppItems = pPaint->pOwner->pItems;
		*pCount = pPaint->pOwner->iItemCount;
	} else if ( (pPaint->iType == XGE_SVG_DRAW_ITEM_GROUP) && (pPaint->u.pGroup != NULL) ) {
		xge_svg_group_item_t* pWrapper = __xgeSvgPaintCompositeWrapper(pPaint);

		if ( pWrapper != NULL ) {
			*ppItems = pWrapper->pItems;
			*pCount = pWrapper->iItemCount;
		} else {
			*ppItems = pPaint->u.pGroup->pItems;
			*pCount = pPaint->u.pGroup->iItemCount;
		}
	} else if ( (pPaint->iType == XGE_SVG_DRAW_ITEM_FILTER) && (pPaint->u.pFilter != NULL) ) {
		*ppItems = pPaint->u.pFilter->pItems;
		*pCount = pPaint->u.pFilter->iItemCount;
	}
}

int xgeSvgPaintChildGetCount(xge_svg_paint pPaint, int* pCount)
{
	xge_svg_draw_item_t* pItems;

	if ( (pPaint == NULL) || (pCount == NULL) || !__xgeSvgValid(pPaint->pOwner) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	__xgeSvgPaintChildren(pPaint, &pItems, pCount);
	return XGE_OK;
}

int xgeSvgPaintChildGetAt(xge_svg_paint pPaint, int iIndex, xge_svg_paint* ppChild)
{
	xge_svg_draw_item_t* pItems;
	int iCount;

	if ( (pPaint == NULL) || (ppChild == NULL) || !__xgeSvgValid(pPaint->pOwner) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppChild = NULL;
	__xgeSvgPaintChildren(pPaint, &pItems, &iCount);
	if ( (iIndex < 0) || (iIndex >= iCount) ) return XGE_ERROR_NOT_FOUND;
	*ppChild = &pItems[iIndex];
	return XGE_OK;
}

static int __xgeSvgPaintTraverse(
	xge_svg_paint pPaint,
	xge_svg_paint_visit_proc onPaint,
	void* pUser,
	int iDepth
)
{
	xge_svg_draw_item_t* pItems;
	int iCount;
	int i;
	int iRet;

	if ( iDepth > XGE_SVG_BOUNDS_MAX_DEPTH ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( !onPaint(pPaint, pUser) ) return 1;
	__xgeSvgPaintChildren(pPaint, &pItems, &iCount);
	for ( i = 0; i < iCount; i++ ) {
		iRet = __xgeSvgPaintTraverse(&pItems[i], onPaint, pUser, iDepth + 1);
		if ( iRet != XGE_OK ) return iRet;
	}
	return XGE_OK;
}

int xgeSvgPaintTraverse(xge_svg_paint pPaint, xge_svg_paint_visit_proc onPaint, void* pUser)
{
	int iRet;

	if ( (pPaint == NULL) || (onPaint == NULL) || !__xgeSvgValid(pPaint->pOwner) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeSvgPaintTraverse(pPaint, onPaint, pUser, 0);
	return iRet == 1 ? XGE_OK : iRet;
}

static int __xgeSvgPictureGetBounds(xge_svg pSvg, xge_rect_t* pBounds)
{
	xge_shape_ex_matrix_t tMatrix = __xgeSvgMatrixIdentity();
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pBounds == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( !pSvg->bLoaded ) return XGE_ERROR_INVALID_STATE;
	iRet = __xgeSvgPaintRuntimeParent(&pSvg->tPicturePaint, tMatrix, &tMatrix);
	if ( iRet != XGE_OK ) return iRet;
	*pBounds = __xgeSvgMatrixRectBounds(
		tMatrix, (xge_rect_t){0.0f, 0.0f, pSvg->fWidth, pSvg->fHeight}
	);
	return XGE_OK;
}

static int __xgeSvgPictureGetOBB(xge_svg pSvg, xge_vec2_t* pPoints4)
{
	xge_shape_ex_matrix_t tMatrix = __xgeSvgMatrixIdentity();
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pPoints4 == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( !pSvg->bLoaded ) return XGE_ERROR_INVALID_STATE;
	iRet = __xgeSvgPaintRuntimeParent(&pSvg->tPicturePaint, tMatrix, &tMatrix);
	if ( iRet != XGE_OK ) return iRet;
	pPoints4[0] = __xgeSvgMatrixPoint(tMatrix, (xge_vec2_t){0.0f, 0.0f});
	pPoints4[1] = __xgeSvgMatrixPoint(tMatrix, (xge_vec2_t){pSvg->fWidth, 0.0f});
	pPoints4[2] = __xgeSvgMatrixPoint(tMatrix, (xge_vec2_t){pSvg->fWidth, pSvg->fHeight});
	pPoints4[3] = __xgeSvgMatrixPoint(tMatrix, (xge_vec2_t){0.0f, pSvg->fHeight});
	return XGE_OK;
}

static int __xgeSvgPaintRetainedAncestorParent(
	xge_svg_paint pPaint,
	xge_shape_ex_matrix_t* pParent
)
{
	xge_svg pSvg;
	xge_svg_draw_item_t* arrAncestors[XGE_SVG_STACK_MAX];
	xge_svg_draw_item_t* pAncestor;
	int iCount;
	int i;
	int iRet;

	if ( (pPaint == NULL) || (pParent == NULL) || !__xgeSvgValid(pPaint->pOwner) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pSvg = pPaint->pOwner;
	iRet = __xgeSvgPaintRetainedHierarchyParent(
		pSvg, __xgeSvgMatrixIdentity(), pParent
	);
	if ( iRet != XGE_OK ) return iRet;
	iCount = 0;
	pAncestor = pPaint->pParentPaint;
	while ( (pAncestor != NULL) &&
	        (pAncestor != &pSvg->tPicturePaint) &&
	        (pAncestor != &pSvg->tRootPaint) &&
	        (pAncestor != &pSvg->tContentPaint) &&
	        (pAncestor != &pSvg->tDocumentPaint) ) {
		if ( iCount >= XGE_SVG_STACK_MAX ) return XGE_ERROR_INVALID_STATE;
		arrAncestors[iCount++] = pAncestor;
		pAncestor = pAncestor->pParentPaint;
	}
	for ( i = iCount - 1; i >= 0; i-- ) {
		iRet = __xgeSvgPaintRuntimeParent(arrAncestors[i], *pParent, pParent);
		if ( iRet != XGE_OK ) return iRet;
	}
	return XGE_OK;
}

int xgeSvgPaintGetBounds(xge_svg_paint pPaint, float fTolerance, xge_rect_t* pBounds)
{
	xge_shape_ex_matrix_t tParent;
	int iRet;

	if ( (pPaint == NULL) || (pBounds == NULL) || !__xgeSvgValid(pPaint->pOwner) ||
	     !__xgeSvgFloatFinite(fTolerance) || (fTolerance < 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pPaint->iType == XGE_SVG_PAINT_PICTURE ) {
		return __xgeSvgPictureGetBounds(pPaint->pOwner, pBounds);
	}
	if ( (pPaint == &pPaint->pOwner->tRootPaint) ||
	     (pPaint == &pPaint->pOwner->tContentPaint) ||
	     (pPaint == &pPaint->pOwner->tDocumentPaint) ) {
		return xgeSvgGetBounds(pPaint->pOwner, fTolerance, pBounds);
	}
	iRet = __xgeSvgPaintRetainedAncestorParent(pPaint, &tParent);
	if ( iRet != XGE_OK ) return iRet;
	return __xgeSvgDrawItemBoundsWithParent(
		pPaint->pOwner, (xge_svg_draw_item_t*)pPaint,
		tParent, fTolerance, pBounds, 0
	);
}

static void __xgeSvgBoundsToOBB(xge_rect_t tBounds, xge_vec2_t* pPoints4)
{
	pPoints4[0] = (xge_vec2_t){tBounds.fX, tBounds.fY};
	pPoints4[1] = (xge_vec2_t){tBounds.fX + tBounds.fW, tBounds.fY};
	pPoints4[2] = (xge_vec2_t){tBounds.fX + tBounds.fW, tBounds.fY + tBounds.fH};
	pPoints4[3] = (xge_vec2_t){tBounds.fX, tBounds.fY + tBounds.fH};
}

static int __xgeSvgDrawItemGetOBB(
	xge_svg pSvg,
	const xge_svg_draw_item_t* pItem,
	float fTolerance,
	xge_vec2_t* pPoints4,
	int iDepth
)
{
	xge_rect_t tBounds;
	int iRet;
	int i;

	if ( !__xgeSvgValid(pSvg) || (pItem == NULL) || (pPoints4 == NULL) ||
	     (iDepth > XGE_SVG_BOUNDS_MAX_DEPTH) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pItem->bRuntimeTransform ) {
		xge_svg_draw_item_t tBaseItem = *pItem;
		xge_shape_ex_matrix_t tBase;
		xge_shape_ex_matrix_t tInverse;
		xge_shape_ex_matrix_t tDelta;

		tBaseItem.bRuntimeTransform = 0;
		iRet = __xgeSvgDrawItemGetOBB(
			pSvg, &tBaseItem, fTolerance, pPoints4, iDepth + 1
		);
		if ( iRet != XGE_OK ) return iRet;
		iRet = __xgeSvgPaintBaseTransformGet(pItem, &tBase);
		if ( iRet != XGE_OK ) return iRet;
		if ( !__xgeSvgMatrixInvert(tBase, &tInverse) ) return XGE_ERROR_INVALID_STATE;
		tDelta = __xgeSvgMatrixMul(pItem->tRuntimeTransform, tInverse);
		for ( i = 0; i < 4; i++ ) {
			pPoints4[i] = __xgeSvgMatrixPoint(tDelta, pPoints4[i]);
		}
		return XGE_OK;
	}
	if ( pItem->pIdentityShape != NULL ) {
		return xgeShapeExGetOBB(pItem->pIdentityShape, fTolerance, pPoints4);
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_SHAPE) && (pItem->u.pShape != NULL) ) {
		return xgeShapeExGetOBB(pItem->u.pShape, fTolerance, pPoints4);
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_SCENE) && (pItem->u.pScene != NULL) ) {
		return xgeShapeExSceneGetOBB(pItem->u.pScene, fTolerance, pPoints4);
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_GROUP) && (pItem->u.pGroup != NULL) &&
	     !pItem->u.pGroup->bMasked && !pItem->u.pGroup->bPaintTransform &&
	     (pItem->u.pGroup->iItemCount == 1) ) {
		return __xgeSvgDrawItemGetOBB(
			pSvg, &pItem->u.pGroup->pItems[0], fTolerance, pPoints4, iDepth + 1
		);
	}
	if ( (pItem->iType == XGE_SVG_DRAW_ITEM_GROUP) && (pItem->u.pGroup != NULL) &&
	     pItem->u.pGroup->bPaintTransform && !pItem->u.pGroup->bMasked ) {
		xge_svg_group_item_t* pGroup = pItem->u.pGroup;
		xge_shape_ex_matrix_t tInverse;
		xge_rect_t tLocalBounds;
		int bHasBounds = 0;

		if ( __xgeSvgMatrixInvert(pGroup->tPaintTransform, &tInverse) ) {
			memset(&tLocalBounds, 0, sizeof(tLocalBounds));
			for ( i = 0; i < pGroup->iItemCount; i++ ) {
				xge_vec2_t arrChild[4];
				int j;

				iRet = __xgeSvgDrawItemGetOBB(
					pSvg, &pGroup->pItems[i], fTolerance, arrChild, iDepth + 1
				);
				if ( iRet != XGE_OK ) return iRet;
				for ( j = 0; j < 4; j++ ) {
					xge_vec2_t tLocal = __xgeSvgMatrixPoint(tInverse, arrChild[j]);
					if ( !bHasBounds ) {
						tLocalBounds = (xge_rect_t){tLocal.fX, tLocal.fY, 0.0f, 0.0f};
						bHasBounds = 1;
					} else {
						float fMinX = fminf(tLocalBounds.fX, tLocal.fX);
						float fMinY = fminf(tLocalBounds.fY, tLocal.fY);
						float fMaxX = fmaxf(tLocalBounds.fX + tLocalBounds.fW, tLocal.fX);
						float fMaxY = fmaxf(tLocalBounds.fY + tLocalBounds.fH, tLocal.fY);
						tLocalBounds = (xge_rect_t){fMinX, fMinY, fMaxX - fMinX, fMaxY - fMinY};
					}
				}
			}
			if ( bHasBounds ) {
				__xgeSvgBoundsToOBB(tLocalBounds, pPoints4);
				for ( i = 0; i < 4; i++ ) {
					pPoints4[i] = __xgeSvgMatrixPoint(pGroup->tPaintTransform, pPoints4[i]);
				}
				return XGE_OK;
			}
		}
	}
	iRet = __xgeSvgDrawItemBoundsWithParent(
		pSvg, (xge_svg_draw_item_t*)pItem, __xgeSvgMatrixIdentity(),
		fTolerance, &tBounds, iDepth
	);
	if ( iRet != XGE_OK ) return iRet;
	__xgeSvgBoundsToOBB(tBounds, pPoints4);
	return XGE_OK;
}

int xgeSvgPaintGetOBB(xge_svg_paint pPaint, float fTolerance, xge_vec2_t* pPoints4)
{
	xge_shape_ex_matrix_t tParent;
	xge_rect_t tBounds;
	int iRet;
	int i;

	if ( (pPaint == NULL) || (pPoints4 == NULL) || !__xgeSvgValid(pPaint->pOwner) ||
	     !__xgeSvgFloatFinite(fTolerance) || (fTolerance < 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pPaint->iType == XGE_SVG_PAINT_PICTURE ) {
		return __xgeSvgPictureGetOBB(pPaint->pOwner, pPoints4);
	}
	if ( (pPaint == &pPaint->pOwner->tRootPaint) ||
	     (pPaint == &pPaint->pOwner->tContentPaint) ||
	     (pPaint == &pPaint->pOwner->tDocumentPaint) ) {
		iRet = xgeSvgGetBounds(pPaint->pOwner, fTolerance, &tBounds);
		if ( iRet != XGE_OK ) return iRet;
		__xgeSvgBoundsToOBB(tBounds, pPoints4);
		return XGE_OK;
	}
	iRet = __xgeSvgPaintRetainedAncestorParent(pPaint, &tParent);
	if ( iRet != XGE_OK ) return iRet;
	iRet = __xgeSvgDrawItemGetOBB(pPaint->pOwner, pPaint, fTolerance, pPoints4, 0);
	if ( iRet != XGE_OK ) return iRet;
	for ( i = 0; i < 4; i++ ) {
		pPoints4[i] = __xgeSvgMatrixPoint(tParent, pPoints4[i]);
	}
	return XGE_OK;
}

int xgeSvgPaintIntersects(xge_svg_paint pPaint, xge_rect_t tRect, float fTolerance, int* pIntersects)
{
	xge_svg pSvg;
	int i;
	int iRet;

	if ( (pPaint == NULL) || (pIntersects == NULL) || !__xgeSvgValid(pPaint->pOwner) ||
	     !__xgeSvgFloatFinite(tRect.fX) || !__xgeSvgFloatFinite(tRect.fY) ||
	     !__xgeSvgFloatFinite(tRect.fW) || !__xgeSvgFloatFinite(tRect.fH) ||
	     !__xgeSvgFloatFinite(fTolerance) || (fTolerance < 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pIntersects = 0;
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) return XGE_OK;
	pSvg = pPaint->pOwner;
	if ( (pPaint->iType == XGE_SVG_PAINT_PICTURE) ||
	     (pPaint == &pSvg->tRootPaint) ||
	     (pPaint == &pSvg->tContentPaint) ||
	     (pPaint == &pSvg->tDocumentPaint) ) {
		xge_shape_ex_matrix_t tParent = __xgeSvgMatrixIdentity();

		iRet = __xgeSvgPaintRetainedHierarchyParent(pSvg, tParent, &tParent);
		if ( iRet != XGE_OK ) return iRet;
		for ( i = pSvg->iItemCount - 1; i >= 0; i-- ) {
			iRet = __xgeSvgDrawItemIntersectsWithParent(
				pSvg, &pSvg->pItems[i], tParent,
				tRect, fTolerance, pIntersects, 0
			);
			if ( (iRet != XGE_OK) || *pIntersects ) return iRet;
		}
		return XGE_OK;
	}
	{
		xge_shape_ex_matrix_t tParent;

		iRet = __xgeSvgPaintRetainedAncestorParent(pPaint, &tParent);
		if ( iRet != XGE_OK ) return iRet;
		return __xgeSvgDrawItemIntersectsWithParent(
			pSvg, pPaint, tParent, tRect,
			fTolerance, pIntersects, 0
		);
	}
}

int xgeSvgSetSize(xge_svg pSvg, float fWidth, float fHeight)
{
	if ( !__xgeSvgValid(pSvg) || !__xgeSvgFloatFinite(fWidth) ||
	     !__xgeSvgFloatFinite(fHeight) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pSvg->fWidth = fWidth;
	pSvg->fHeight = fHeight;
	return XGE_OK;
}

int xgeSvgGetSize(xge_svg pSvg, float* pWidth, float* pHeight)
{
	if ( !__xgeSvgValid(pSvg) || ((pWidth == NULL) && (pHeight == NULL)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( !pSvg->bLoaded ) return XGE_ERROR_INVALID_STATE;
	if ( pWidth != NULL ) *pWidth = pSvg->fWidth;
	if ( pHeight != NULL ) *pHeight = pSvg->fHeight;
	return XGE_OK;
}

int xgeSvgSetOrigin(xge_svg pSvg, float fX, float fY)
{
	if ( !__xgeSvgValid(pSvg) || !__xgeSvgFloatFinite(fX) || !__xgeSvgFloatFinite(fY) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pSvg->fOriginX = fX;
	pSvg->fOriginY = fY;
	return XGE_OK;
}

int xgeSvgGetOrigin(xge_svg pSvg, float* pX, float* pY)
{
	if ( !__xgeSvgValid(pSvg) || ((pX == NULL) && (pY == NULL)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pX != NULL ) *pX = pSvg->fOriginX;
	if ( pY != NULL ) *pY = pSvg->fOriginY;
	return XGE_OK;
}

int xgeSvgGetViewBox(xge_svg pSvg, xge_rect_t* pViewBox)
{
	if ( !__xgeSvgValid(pSvg) || (pViewBox == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pViewBox = pSvg->tViewBox;
	return pSvg->bHasViewBox ? XGE_OK : XGE_ERROR_NOT_FOUND;
}

int xgeSvgSetPreserveAspectRatio(xge_svg pSvg, const char* sValue)
{
	if ( !__xgeSvgValid(pSvg) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	__xgeSvgParsePreserveAspectRatioFields(sValue, &pSvg->iAspectAlignX, &pSvg->iAspectAlignY, &pSvg->iAspectMeetOrSlice);
	return XGE_OK;
}

int xgeSvgGetPreserveAspectRatio(xge_svg pSvg, int* pAlignX, int* pAlignY, int* pMeetOrSlice)
{
	if ( !__xgeSvgValid(pSvg) ||
	     ((pAlignX == NULL) && (pAlignY == NULL) && (pMeetOrSlice == NULL)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pAlignX != NULL ) *pAlignX = pSvg->iAspectAlignX;
	if ( pAlignY != NULL ) *pAlignY = pSvg->iAspectAlignY;
	if ( pMeetOrSlice != NULL ) *pMeetOrSlice = pSvg->iAspectMeetOrSlice;
	return XGE_OK;
}

int xgeSvgGetDrawViewport(xge_svg pSvg, xge_rect_t tDst, xge_rect_t* pViewport)
{
	xge_rect_t tViewBox;

	if ( !__xgeSvgValid(pSvg) || (pViewport == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tDst.fX -= pSvg->fOriginX * tDst.fW;
	tDst.fY -= pSvg->fOriginY * tDst.fH;
	tViewBox = pSvg->tViewBox;
	*pViewport = __xgeSvgAspectViewport(tViewBox, tDst, pSvg->iAspectAlignX, pSvg->iAspectAlignY, pSvg->iAspectMeetOrSlice);
	return XGE_OK;
}

static int __xgeSvgDrawInternal(xge_svg pSvg, xge_rect_t tDst, float fTolerance, int bScreenSpace);
static int __xgeSvgDrawWithParentEx(xge_svg pSvg, xge_rect_t tDst, xge_shape_ex_matrix_t tParent, float fTolerance, int bScreenSpace, int bSuppressRuntimeComposite);
static int __xgeSvgDrawWithParent(xge_svg pSvg, xge_rect_t tDst, xge_shape_ex_matrix_t tParent, float fTolerance, int bScreenSpace);
static int __xgeSvgGetDrawBoundsInternal(xge_svg pSvg, xge_rect_t tDst, float fTolerance, xge_rect_t* pBounds, int iDepth);
static int __xgeSvgGetDrawBoundsWithParentInternal(xge_svg pSvg, xge_rect_t tDst, xge_shape_ex_matrix_t tParent, float fTolerance, xge_rect_t* pBounds, int iDepth);
static int __xgeSvgContainsPointWithParent(xge_svg pSvg, xge_shape_ex_matrix_t tParent, xge_vec2_t tPoint, float fTolerance, int* pContains, int iDepth);
static int __xgeSvgDrawContainsPointInternal(xge_svg pSvg, xge_rect_t tDst, xge_vec2_t tPoint, float fTolerance, int* pContains, int iDepth);
static int __xgeSvgDrawContainsPointWithParentInternal(xge_svg pSvg, xge_rect_t tDst, xge_shape_ex_matrix_t tParent, xge_vec2_t tPoint, float fTolerance, int* pContains, int iDepth);

static xge_vec2_t __xgeSvgMatrixVector(xge_shape_ex_matrix_t tMatrix, xge_vec2_t tVector)
{
	xge_vec2_t tOut;

	if ( xgeShapeExMatrixVector(&tOut, &tMatrix, tVector) != XGE_OK ) {
		tOut = tVector;
	}
	return tOut;
}

static float __xgeSvgTextActualAdvance(xge_font pFont, const char* sText, const xge_svg_style_t* pStyle, float fScale)
{
	xge_text_shape_desc_t tDesc;
	xge_glyph_run_t tRun;
	float fAdvance;
	int i;

	if ( (pFont == NULL) || (sText == NULL) || (pStyle == NULL) ) {
		return 0.0f;
	}
	(void)pStyle;
	(void)fScale;
	memset(&tDesc, 0, sizeof(tDesc));
	memset(&tRun, 0, sizeof(tRun));
	tDesc.iSize = sizeof(tDesc);
	tDesc.pFont = pFont;
	tDesc.sText = sText;
	tDesc.iTextSize = -1;
	tDesc.iFlags = XGE_TEXT_SHAPE_KERNING;
	if ( xgeTextShape(&tDesc, &tRun) != XGE_OK ) return 0.0f;
	fAdvance = 0.0f;
	for ( i = 0; i < tRun.iGlyphCount; i++ ) {
		if ( (tRun.pGlyphs[i].iFlags & XGE_GLYPH_POSITION_LINE_BREAK) != 0 ) break;
		fAdvance += tRun.pGlyphs[i].fAdvanceX;
	}
	xgeGlyphRunFree(&tRun);
	return fAdvance;
}

static int __xgeSvgDrawTextItem(xge_svg pSvg, const xge_svg_text_item_t* pText, xge_shape_ex_matrix_t tParent, float fTolerance, int bScreenSpace)
{
	xge_font pFont;
	xge_shape_ex pShape;
	xge_shape_ex_matrix_t tMatrix;
	xge_text_shape_desc_t tDesc;
	xge_glyph_run_t tRun;
	float fFontSize;
	float fAnchorWidth;
	float fTextAdvance;
	float fPenX;
	int iRet;
	int i;

	if ( (pText == NULL) || (pText->sText == NULL) || (pText->sText[0] == '\0') ) {
		return XGE_OK;
	}
	if ( !pText->tStyle.bVisible || !pText->tStyle.bVisibility ) {
		return XGE_OK;
	}
	fFontSize = pText->tStyle.fFontSize;
	if ( fFontSize <= 0.25f ) {
		return XGE_OK;
	}
	pFont = NULL;
	iRet = __xgeSvgFontCacheGet(pSvg, pText->tStyle.sFontFamily, fFontSize, &pFont);
	if ( iRet != XGE_OK ) {
		return XGE_OK;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	memset(&tRun, 0, sizeof(tRun));
	tDesc.iSize = sizeof(tDesc);
	tDesc.pFont = pFont;
	tDesc.sText = pText->sText;
	tDesc.iTextSize = -1;
	tDesc.iFlags = XGE_TEXT_SHAPE_KERNING;
	iRet = xgeTextShape(&tDesc, &tRun);
	if ( iRet != XGE_OK ) return iRet;
	fTextAdvance = 0.0f;
	for ( i = 0; i < tRun.iGlyphCount; i++ ) {
		if ( (tRun.pGlyphs[i].iFlags & XGE_GLYPH_POSITION_LINE_BREAK) != 0 ) break;
		fTextAdvance += tRun.pGlyphs[i].fAdvanceX;
	}
	fAnchorWidth = (pText->fAnchorAdvance > 0.0f) ? pText->fAnchorAdvance : fTextAdvance;
	fPenX = pText->fX;
	if ( pText->tStyle.iTextAnchor == XGE_SVG_TEXT_ANCHOR_MIDDLE ) {
		fPenX -= fAnchorWidth * 0.5f;
	} else if ( pText->tStyle.iTextAnchor == XGE_SVG_TEXT_ANCHOR_END ) {
		fPenX -= fAnchorWidth;
	}
	pShape = NULL;
	iRet = xgeShapeExCreate(&pShape);
	if ( iRet == XGE_OK ) {
		iRet = xgeGlyphRunAppendShapeEx(&tRun, pShape, fPenX, pText->fY - fFontSize);
	}
	if ( iRet == XGE_OK ) {
		__xgeSvgApplyShapeStyle(pSvg, pShape, &pText->tStyle);
		/* Blend and clipping are handled by the draw-item layer. */
		xgeShapeExBlendClear(pShape);
		tMatrix = __xgeSvgMatrixMul(tParent, pText->tStyle.tTransform);
		iRet = bScreenSpace ?
			xgeShapeExDrawPxEx(pShape, fTolerance, &tMatrix, 1.0f) :
			xgeShapeExDrawEx(pShape, fTolerance, &tMatrix, 1.0f);
	}
	xgeShapeExDestroy(pShape);
	xgeGlyphRunFree(&tRun);
	return iRet;
}

static int __xgeSvgDrawImageItem(const xge_svg_image_item_t* pImage, xge_shape_ex_matrix_t tParent, float fTolerance, int bScreenSpace)
{
	xge_shape_ex_matrix_t tMatrix;

	if ( (pImage == NULL) || !__xgeSvgValid(pImage->pSvg) ) {
		return XGE_OK;
	}
	if ( !pImage->tStyle.bVisible || !pImage->tStyle.bVisibility || (pImage->tStyle.fOpacity <= 0.0f) ) {
		return XGE_OK;
	}
	tMatrix = __xgeSvgMatrixMul(tParent, pImage->tStyle.tTransform);
	return __xgeSvgDrawWithParent(pImage->pSvg, pImage->tRect, tMatrix, fTolerance, bScreenSpace);
}

static int __xgeSvgDrawRasterImageItem(xge_svg_raster_item_t* pImage, xge_shape_ex_matrix_t tParent, int bScreenSpace)
{
	xge_shape_ex_matrix_t tMatrix;
	xge_draw_t tDraw;
	xge_rect_t tImageBox;
	xge_rect_t tLocalDst;
	xge_rect_t tDst;
	uint32_t iColor;
	int iAlpha;
	int iRet;

	if ( (pImage == NULL) || (pImage->pRaster == NULL) ) {
		return XGE_OK;
	}
	if ( !pImage->tStyle.bVisible || !pImage->tStyle.bVisibility || (pImage->tStyle.fOpacity <= 0.0f) ) {
		return XGE_OK;
	}
	if ( (pImage->pRaster->tImage.iWidth <= 0) || (pImage->pRaster->tImage.iHeight <= 0) ) {
		return XGE_OK;
	}
	iRet = __xgeSvgRasterImageEnsureTexture(pImage->pRaster);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	tMatrix = __xgeSvgMatrixMul(tParent, pImage->tStyle.tTransform);
	tImageBox.fX = 0.0f;
	tImageBox.fY = 0.0f;
	tImageBox.fW = (float)pImage->pRaster->tImage.iWidth;
	tImageBox.fH = (float)pImage->pRaster->tImage.iHeight;
	tLocalDst = __xgeSvgAspectViewport(tImageBox, pImage->tRect, pImage->iAspectAlignX, pImage->iAspectAlignY, pImage->iAspectMeetOrSlice);
	tDst = __xgeSvgMatrixRectBounds(tMatrix, tLocalDst);
	if ( (tDst.fW <= 0.0f) || (tDst.fH <= 0.0f) ) {
		return XGE_OK;
	}
	iColor = pImage->iColor;
	iAlpha = (int)(pImage->tStyle.fOpacity * (float)XGE_COLOR_GET_A(iColor) + 0.5f);
	if ( iAlpha <= 0 ) {
		return XGE_OK;
	}
	if ( iAlpha > 255 ) {
		iAlpha = 255;
	}
	memset(&tDraw, 0, sizeof(tDraw));
	tDraw.pTexture = &pImage->pRaster->tTexture;
	tDraw.tDst = tDst;
	tDraw.iColor = XGE_COLOR_RGBA(XGE_COLOR_GET_R(iColor), XGE_COLOR_GET_G(iColor), XGE_COLOR_GET_B(iColor), iAlpha);
	tDraw.iFlags = bScreenSpace ? XGE_DRAW_SCREEN_SPACE : 0;
	xgeDrawEx(&tDraw);
	return XGE_OK;
}

typedef struct xge_svg_mask_text_draw_context_t {
	xge_svg pSvg;
	const xge_svg_text_item_t* pText;
	float fTolerance;
} xge_svg_mask_text_draw_context_t;

typedef struct xge_svg_mask_image_draw_context_t {
	const xge_svg_image_item_t* pImage;
	float fTolerance;
} xge_svg_mask_image_draw_context_t;

typedef struct xge_svg_mask_raster_draw_context_t {
	const xge_svg_raster_item_t* pImage;
} xge_svg_mask_raster_draw_context_t;

static int __xgeSvgDrawMaskedTextSource(void* pUser, const xge_shape_ex_matrix_t* pParentMatrix)
{
	xge_svg_mask_text_draw_context_t* pContext = (xge_svg_mask_text_draw_context_t*)pUser;
	xge_svg_text_item_t tText;

	if ( (pContext == NULL) || !__xgeSvgValid(pContext->pSvg) ||
	     (pContext->pText == NULL) || (pParentMatrix == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tText = *pContext->pText;
	tText.tStyle.tTransform = __xgeSvgMatrixIdentity();
	tText.tStyle.bBlendSet = 0;
	tText.pMaskScene = NULL;
	return __xgeSvgDrawTextItem(pContext->pSvg, &tText, *pParentMatrix, pContext->fTolerance, 1);
}

static int __xgeSvgDrawMaskedImageSource(void* pUser, const xge_shape_ex_matrix_t* pParentMatrix)
{
	xge_svg_mask_image_draw_context_t* pContext = (xge_svg_mask_image_draw_context_t*)pUser;
	xge_svg_image_item_t tImage;

	if ( (pContext == NULL) || (pContext->pImage == NULL) || (pParentMatrix == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tImage = *pContext->pImage;
	tImage.tStyle.tTransform = __xgeSvgMatrixIdentity();
	tImage.tStyle.bBlendSet = 0;
	tImage.pMaskScene = NULL;
	return __xgeSvgDrawImageItem(&tImage, *pParentMatrix, pContext->fTolerance, 1);
}

static int __xgeSvgDrawMaskedRasterSource(void* pUser, const xge_shape_ex_matrix_t* pParentMatrix)
{
	xge_svg_mask_raster_draw_context_t* pContext = (xge_svg_mask_raster_draw_context_t*)pUser;
	xge_svg_raster_item_t tImage;

	if ( (pContext == NULL) || (pContext->pImage == NULL) || (pParentMatrix == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tImage = *pContext->pImage;
	tImage.tStyle.tTransform = __xgeSvgMatrixIdentity();
	tImage.tStyle.bBlendSet = 0;
	tImage.pMaskScene = NULL;
	return __xgeSvgDrawRasterImageItem(&tImage, *pParentMatrix, 1);
}

static xge_rect_t __xgeSvgRectUnion(xge_rect_t a, xge_rect_t b)
{
	float fLeft = fminf(a.fX, b.fX);
	float fTop = fminf(a.fY, b.fY);
	float fRight = fmaxf(a.fX + a.fW, b.fX + b.fW);
	float fBottom = fmaxf(a.fY + a.fH, b.fY + b.fH);
	xge_rect_t tOut;

	tOut.fX = fLeft;
	tOut.fY = fTop;
	tOut.fW = fRight - fLeft;
	tOut.fH = fBottom - fTop;
	if ( tOut.fW < 0.0f ) tOut.fW = 0.0f;
	if ( tOut.fH < 0.0f ) tOut.fH = 0.0f;
	return tOut;
}

static int __xgeSvgRectContainsPoint(xge_rect_t tRect, xge_vec2_t tPoint)
{
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return 0;
	}
	return (tPoint.fX >= tRect.fX) && (tPoint.fX <= (tRect.fX + tRect.fW)) &&
	       (tPoint.fY >= tRect.fY) && (tPoint.fY <= (tRect.fY + tRect.fH));
}

static int __xgeSvgBoundsUnion(xge_rect_t* pOut, int* pHasBounds, xge_rect_t tBounds)
{
	if ( (pOut == NULL) || (pHasBounds == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (tBounds.fW <= 0.0f) || (tBounds.fH <= 0.0f) ) {
		return XGE_OK;
	}
	if ( !*pHasBounds ) {
		*pOut = tBounds;
		*pHasBounds = 1;
	} else {
		*pOut = __xgeSvgRectUnion(*pOut, tBounds);
	}
	return XGE_OK;
}

static xge_rect_t __xgeSvgRectExpand(xge_rect_t tBounds, float fAmount)
{
	if ( fAmount <= 0.0f ) {
		return tBounds;
	}
	tBounds.fX -= fAmount;
	tBounds.fY -= fAmount;
	tBounds.fW += fAmount * 2.0f;
	tBounds.fH += fAmount * 2.0f;
	if ( tBounds.fW < 0.0f ) tBounds.fW = 0.0f;
	if ( tBounds.fH < 0.0f ) tBounds.fH = 0.0f;
	return tBounds;
}

static int __xgeSvgShapeStopsVisible(const xge_shape_ex_color_stop_t* pStops, int iStopCount)
{
	int i;

	if ( (pStops == NULL) || (iStopCount <= 0) ) {
		return 0;
	}
	for ( i = 0; i < iStopCount; i++ ) {
		if ( XGE_COLOR_GET_A(pStops[i].iColor) != 0u ) {
			return 1;
		}
	}
	return 0;
}

static int __xgeSvgShapeFillPaintVisible(xge_shape_ex pShape)
{
	const xge_shape_ex_color_stop_t* pStops;
	uint32_t iColor;
	float fX1;
	float fY1;
	float fX2;
	float fY2;
	float fR;
	int iStopCount;
	int iUnits;
	int iType;

	if ( xgeShapeExFillTypeGet(pShape, &iType) != XGE_OK ) {
		return 1;
	}
	if ( iType == XGE_SHAPE_EX_PAINT_SOLID ) {
		if ( xgeShapeExFillColorGet(pShape, &iColor) != XGE_OK ) {
			return 1;
		}
		return XGE_COLOR_GET_A(iColor) != 0u;
	}
	if ( iType == XGE_SHAPE_EX_PAINT_LINEAR_GRADIENT ) {
		pStops = NULL;
		iStopCount = 0;
		if ( xgeShapeExFillLinearGradientGet(pShape, &fX1, &fY1, &fX2, &fY2, &iUnits, &pStops, &iStopCount) != XGE_OK ) {
			return 1;
		}
		return __xgeSvgShapeStopsVisible(pStops, iStopCount);
	}
	if ( iType == XGE_SHAPE_EX_PAINT_RADIAL_GRADIENT ) {
		pStops = NULL;
		iStopCount = 0;
		if ( xgeShapeExFillRadialGradientGet(pShape, &fX1, &fY1, &fR, &fX2, &fY2, &iUnits, &pStops, &iStopCount) != XGE_OK ) {
			return 1;
		}
		return __xgeSvgShapeStopsVisible(pStops, iStopCount);
	}
	return 1;
}

static int __xgeSvgShapeStrokePaintVisible(xge_shape_ex pShape)
{
	const xge_shape_ex_color_stop_t* pStops;
	uint32_t iColor;
	float fX1;
	float fY1;
	float fX2;
	float fY2;
	float fR;
	float fStrokeWidth;
	int iStopCount;
	int iUnits;
	int iType;

	if ( xgeShapeExStrokeWidthGet(pShape, &fStrokeWidth) != XGE_OK ) {
		return 1;
	}
	if ( fStrokeWidth <= 0.0f ) {
		return 0;
	}
	if ( xgeShapeExStrokeTypeGet(pShape, &iType) != XGE_OK ) {
		return 1;
	}
	if ( iType == XGE_SHAPE_EX_PAINT_SOLID ) {
		if ( xgeShapeExStrokeColorGet(pShape, &iColor) != XGE_OK ) {
			return 1;
		}
		return XGE_COLOR_GET_A(iColor) != 0u;
	}
	if ( iType == XGE_SHAPE_EX_PAINT_LINEAR_GRADIENT ) {
		pStops = NULL;
		iStopCount = 0;
		if ( xgeShapeExStrokeLinearGradientGet(pShape, &fX1, &fY1, &fX2, &fY2, &iUnits, &pStops, &iStopCount) != XGE_OK ) {
			return 1;
		}
		return __xgeSvgShapeStopsVisible(pStops, iStopCount);
	}
	if ( iType == XGE_SHAPE_EX_PAINT_RADIAL_GRADIENT ) {
		pStops = NULL;
		iStopCount = 0;
		if ( xgeShapeExStrokeRadialGradientGet(pShape, &fX1, &fY1, &fR, &fX2, &fY2, &iUnits, &pStops, &iStopCount) != XGE_OK ) {
			return 1;
		}
		return __xgeSvgShapeStopsVisible(pStops, iStopCount);
	}
	return 1;
}

static int __xgeSvgShapePaintVisible(xge_shape_ex pShape)
{
	return __xgeSvgShapeFillPaintVisible(pShape) || __xgeSvgShapeStrokePaintVisible(pShape);
}

static int __xgeSvgShapeBoundsTransformed(xge_shape_ex pShape, float fTolerance, xge_shape_ex_matrix_t tParent, int bRespectPaint, xge_rect_t* pBounds)
{
	xge_rect_t tBounds;
	int bVisible;
	float fOpacity;
	int iRet;

	if ( (pBounds == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pBounds, 0, sizeof(*pBounds));
	if ( pShape == NULL ) {
		return XGE_OK;
	}
	bVisible = 1;
	fOpacity = 1.0f;
	(void)xgeShapeExVisibleGet(pShape, &bVisible);
	(void)xgeShapeExOpacityGet(pShape, &fOpacity);
	if ( !bVisible || (fOpacity <= 0.0f) ) {
		return XGE_OK;
	}
	if ( bRespectPaint && !__xgeSvgShapePaintVisible(pShape) ) {
		return XGE_OK;
	}
	iRet = __xgeShapeExGetBoundsInternal(
		pShape, fTolerance, __xgeShapeExMatrixIdentity(), &tBounds
	);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	*pBounds = __xgeSvgMatrixRectBounds(tParent, tBounds);
	return XGE_OK;
}

static int __xgeSvgMaskColorVisible(uint32_t iColor, int iMethod)
{
	if ( XGE_COLOR_GET_A(iColor) == 0u ) return 0;
	if ( iMethod != XGE_SHAPE_EX_MASK_LUMA ) return 1;
	return (XGE_COLOR_GET_R(iColor) * 54u + XGE_COLOR_GET_G(iColor) * 182u + XGE_COLOR_GET_B(iColor) * 19u) != 0u;
}

static int __xgeSvgMaskStopsVisible(const xge_shape_ex_color_stop_t* pStops, int iStopCount, int iMethod)
{
	int i;

	for ( i = 0; (pStops != NULL) && (i < iStopCount); i++ ) {
		if ( __xgeSvgMaskColorVisible(pStops[i].iColor, iMethod) ) return 1;
	}
	return 0;
}

static int __xgeSvgMaskShapePaintVisible(xge_shape_ex pShape, int iMethod)
{
	const xge_shape_ex_color_stop_t* pStops;
	uint32_t iColor;
	float fOpacity;
	float fStrokeWidth;
	int iVisible;
	int iType;
	int iStopCount;

	if ( pShape == NULL ) return 0;
	iVisible = 0;
	fOpacity = 0.0f;
	if ( (xgeShapeExVisibleGet(pShape, &iVisible) != XGE_OK) || !iVisible ||
	     (xgeShapeExOpacityGet(pShape, &fOpacity) != XGE_OK) || (fOpacity <= 0.0f) ) {
		return 0;
	}
	iType = XGE_SHAPE_EX_FILL_SOLID;
	if ( xgeShapeExFillTypeGet(pShape, &iType) == XGE_OK ) {
		if ( iType == XGE_SHAPE_EX_FILL_SOLID ) {
			if ( (xgeShapeExFillColorGet(pShape, &iColor) == XGE_OK) && __xgeSvgMaskColorVisible(iColor, iMethod) ) return 1;
		} else {
			pStops = NULL;
			iStopCount = 0;
			if ( iType == XGE_SHAPE_EX_FILL_LINEAR_GRADIENT ) {
				xgeShapeExFillLinearGradientGet(pShape, NULL, NULL, NULL, NULL, NULL, &pStops, &iStopCount);
			} else if ( iType == XGE_SHAPE_EX_FILL_RADIAL_GRADIENT ) {
				xgeShapeExFillRadialGradientGet(pShape, NULL, NULL, NULL, NULL, NULL, NULL, &pStops, &iStopCount);
			}
			if ( __xgeSvgMaskStopsVisible(pStops, iStopCount, iMethod) ) return 1;
		}
	}
	fStrokeWidth = 0.0f;
	if ( (xgeShapeExStrokeWidthGet(pShape, &fStrokeWidth) != XGE_OK) || (fStrokeWidth <= 0.0f) ) return 0;
	iType = XGE_SHAPE_EX_FILL_SOLID;
	if ( xgeShapeExStrokeTypeGet(pShape, &iType) != XGE_OK ) return 0;
	if ( iType == XGE_SHAPE_EX_FILL_SOLID ) {
		return (xgeShapeExStrokeColorGet(pShape, &iColor) == XGE_OK) && __xgeSvgMaskColorVisible(iColor, iMethod);
	}
	pStops = NULL;
	iStopCount = 0;
	if ( iType == XGE_SHAPE_EX_FILL_LINEAR_GRADIENT ) {
		xgeShapeExStrokeLinearGradientGet(pShape, NULL, NULL, NULL, NULL, NULL, &pStops, &iStopCount);
	} else if ( iType == XGE_SHAPE_EX_FILL_RADIAL_GRADIENT ) {
		xgeShapeExStrokeRadialGradientGet(pShape, NULL, NULL, NULL, NULL, NULL, NULL, &pStops, &iStopCount);
	}
	return __xgeSvgMaskStopsVisible(pStops, iStopCount, iMethod);
}

static int __xgeSvgMaskScenePaintVisible(xge_shape_ex_scene pScene, int iMethod)
{
	xge_shape_ex pShape;
	int iCount;
	int i;

	if ( pScene == NULL || xgeShapeExSceneGetCount(pScene, &iCount) != XGE_OK ) return 0;
	for ( i = 0; i < iCount; i++ ) {
		pShape = NULL;
		if ( (xgeShapeExSceneGetAt(pScene, i, &pShape) == XGE_OK) &&
		     __xgeSvgMaskShapePaintVisible(pShape, iMethod) ) return 1;
	}
	return 0;
}

static int __xgeSvgSceneBoundsTransformed(xge_shape_ex_scene pScene, int iMethod, float fTolerance, xge_shape_ex_matrix_t tParent, xge_rect_t* pBounds)
{
	xge_shape_ex_matrix_t tSceneMatrix;
	xge_shape_ex_matrix_t tMatrix;
	xge_rect_t tOut;
	int bHasBounds;
	int bVisible;
	float fOpacity;
	int iCount;
	int iRet;
	int i;

	if ( (pBounds == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pBounds, 0, sizeof(*pBounds));
	if ( pScene == NULL ) {
		return XGE_OK;
	}
	iMethod = __xgeSvgItemMaskMethod(iMethod);
	if ( !__xgeSvgMaskScenePaintVisible(pScene, iMethod) ) return XGE_OK;
	bVisible = 1;
	fOpacity = 1.0f;
	if ( xgeShapeExSceneVisibleGet(pScene, &bVisible) != XGE_OK || !bVisible ) {
		return XGE_OK;
	}
	if ( xgeShapeExSceneOpacityGet(pScene, &fOpacity) != XGE_OK || (fOpacity <= 0.0f) ) {
		return XGE_OK;
	}
	iRet = xgeShapeExSceneTransformGet(pScene, &tSceneMatrix);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	tMatrix = __xgeSvgMatrixMul(tParent, tSceneMatrix);
	iRet = xgeShapeExSceneGetCount(pScene, &iCount);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	memset(&tOut, 0, sizeof(tOut));
	bHasBounds = 0;
	for ( i = 0; i < iCount; i++ ) {
		xge_shape_ex pShape;
		xge_rect_t tShapeBounds;

		pShape = NULL;
		iRet = xgeShapeExSceneGetAt(pScene, i, &pShape);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		iRet = __xgeSvgShapeBoundsTransformed(pShape, fTolerance, tMatrix, 1, &tShapeBounds);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		iRet = __xgeSvgBoundsUnion(&tOut, &bHasBounds, tShapeBounds);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	if ( bHasBounds ) {
		xge_rect_t tClipRect;
		int bClipRect;
		int iClipCount;

		bClipRect = 0;
		if ( xgeShapeExSceneClipRectGet(pScene, &tClipRect, &bClipRect) == XGE_OK && bClipRect ) {
			tOut = __xgeSvgRectIntersect(tOut, __xgeSvgMatrixRectBounds(tMatrix, tClipRect));
		}
		if ( xgeShapeExSceneClipShapeGetCount(pScene, &iClipCount) == XGE_OK ) {
			xge_rect_t tClipUnion;
			int bHasClipBounds;

			memset(&tClipUnion, 0, sizeof(tClipUnion));
			bHasClipBounds = 0;
			for ( i = 0; i < iClipCount; i++ ) {
				xge_shape_ex pClipShape;
				xge_rect_t tClipBounds;

				pClipShape = NULL;
				iRet = xgeShapeExSceneClipShapeGetAt(pScene, i, &pClipShape);
				if ( iRet != XGE_OK ) {
					return iRet;
				}
				iRet = __xgeSvgShapeBoundsTransformed(pClipShape, fTolerance, tMatrix, 0, &tClipBounds);
				if ( iRet != XGE_OK ) {
					return iRet;
				}
				iRet = __xgeSvgBoundsUnion(&tClipUnion, &bHasClipBounds, tClipBounds);
				if ( iRet != XGE_OK ) {
					return iRet;
				}
			}
			if ( iClipCount > 0 ) {
				if ( bHasClipBounds ) {
					tOut = __xgeSvgRectIntersect(tOut, tClipUnion);
				} else {
					memset(&tOut, 0, sizeof(tOut));
				}
			}
		}
	}
	*pBounds = bHasBounds ? tOut : (xge_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	return XGE_OK;
}

static int __xgeSvgApplyItemClipAndMask(xge_rect_t* pBounds, xge_shape_ex_matrix_t tMatrix, int bClipRect, xge_rect_t tClipRect, xge_shape_ex_scene pMaskScene, int iMaskMethod, float fTolerance)
{
	int iRet;

	if ( pBounds == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (pBounds->fW <= 0.0f) || (pBounds->fH <= 0.0f) ) {
		return XGE_OK;
	}
	if ( bClipRect ) {
		*pBounds = __xgeSvgRectIntersect(*pBounds, __xgeSvgMatrixRectBounds(tMatrix, tClipRect));
	}
	if ( pMaskScene != NULL ) {
		xge_rect_t tMaskBounds;

		iRet = __xgeSvgSceneBoundsTransformed(pMaskScene, iMaskMethod, fTolerance, tMatrix, &tMaskBounds);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		*pBounds = __xgeSvgRectIntersect(*pBounds, tMaskBounds);
	}
	return XGE_OK;
}

static int __xgeSvgTextLineBounds(xge_svg pSvg, const xge_svg_text_item_t* pText, xge_rect_t* pBounds)
{
	xge_font pFont;
	xge_font_metrics_t tFontMetrics;
	xge_rect_t tOut;
	const char* p;
	uint32_t iCodepoint;
	float fFontSize;
	float fTextAdvance;
	float fAnchorWidth;
	float fPenX;
	float fBaselineY;
	float fAscent;
	float fDescent;
	int bHasBounds;

	if ( !__xgeSvgValid(pSvg) || (pText == NULL) || (pBounds == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pBounds, 0, sizeof(*pBounds));
	fFontSize = pText->tStyle.fFontSize;
	if ( fFontSize <= 0.0f ) {
		return XGE_OK;
	}
	pFont = NULL;
	if ( __xgeSvgFontCacheGet(pSvg, pText->tStyle.sFontFamily, fFontSize, &pFont) != XGE_OK || pFont == NULL ) {
		*pBounds = __xgeSvgTextMaskBounds(pText);
		return XGE_OK;
	}
	memset(&tFontMetrics, 0, sizeof(tFontMetrics));
	if ( xgeFontGetMetrics(pFont, &tFontMetrics) != XGE_OK ) {
		*pBounds = __xgeSvgTextMaskBounds(pText);
		return XGE_OK;
	}
	fTextAdvance = __xgeSvgTextActualAdvance(pFont, pText->sText, &pText->tStyle, 1.0f);
	fAnchorWidth = (pText->fAnchorAdvance > 0.0f) ? pText->fAnchorAdvance : fTextAdvance;
	fPenX = pText->fX;
	fBaselineY = pText->fY - pText->tStyle.fFontSize + tFontMetrics.fAscent;
	if ( pText->tStyle.iTextAnchor == XGE_SVG_TEXT_ANCHOR_MIDDLE ) {
		fPenX -= fAnchorWidth * 0.5f;
	} else if ( pText->tStyle.iTextAnchor == XGE_SVG_TEXT_ANCHOR_END ) {
		fPenX -= fAnchorWidth;
	}
	fAscent = tFontMetrics.fAscent > 0.0f ? tFontMetrics.fAscent : fFontSize;
	fDescent = tFontMetrics.fDescent < 0.0f ? tFontMetrics.fDescent : 0.0f;
	memset(&tOut, 0, sizeof(tOut));
	bHasBounds = 0;
	p = pText->sText;
	while ( p != NULL && *p != '\0' ) {
		xge_glyph_metrics_t tMetrics;
		xge_rect_t tGlyphBounds;
		if ( xgeTextUTF8Next(&p, &iCodepoint) != XGE_OK ) {
			break;
		}
		if ( iCodepoint == '\n' ) {
			break;
		}
		if ( xgeFontGlyphGet(pFont, iCodepoint, &tMetrics) == XGE_OK ) {
			tGlyphBounds.fX = fPenX + tMetrics.fX0;
			tGlyphBounds.fY = fBaselineY + tMetrics.fY0;
			tGlyphBounds.fW = tMetrics.fX1 - tMetrics.fX0;
			tGlyphBounds.fH = tMetrics.fY1 - tMetrics.fY0;
			(void)__xgeSvgBoundsUnion(&tOut, &bHasBounds, tGlyphBounds);
			fPenX += tMetrics.fAdvanceX;
		}
	}
	if ( !bHasBounds && fTextAdvance > 0.0f ) {
		tOut.fX = (pText->tStyle.iTextAnchor == XGE_SVG_TEXT_ANCHOR_MIDDLE) ? pText->fX - fAnchorWidth * 0.5f :
		          (pText->tStyle.iTextAnchor == XGE_SVG_TEXT_ANCHOR_END) ? pText->fX - fAnchorWidth : pText->fX;
		tOut.fY = fBaselineY - fAscent;
		tOut.fW = fTextAdvance;
		tOut.fH = fAscent - fDescent;
		bHasBounds = 1;
	}
	*pBounds = bHasBounds ? tOut : (xge_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	return XGE_OK;
}

static int __xgeSvgTextItemBounds(xge_svg pSvg, const xge_svg_text_item_t* pText, xge_shape_ex_matrix_t tParent, float fTolerance, xge_rect_t* pBounds)
{
	xge_shape_ex_matrix_t tMatrix;
	xge_rect_t tBounds;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pText == NULL) || (pBounds == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pBounds, 0, sizeof(*pBounds));
	if ( (pText->sText == NULL) || (pText->sText[0] == '\0') || (pText->tStyle.fFontSize <= 0.0f) ||
	     !pText->tStyle.bVisible || !pText->tStyle.bVisibility ) {
		return XGE_OK;
	}
	if ( (pText->tStyle.fOpacity <= 0.0f) ||
	     (!__xgeSvgStyleHasPaintAlpha(&pText->tStyle, 0) &&
	      !__xgeSvgStyleHasPaintAlpha(&pText->tStyle, 1)) ) {
		return XGE_OK;
	}
	iRet = __xgeSvgTextLineBounds(pSvg, pText, &tBounds);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	tMatrix = __xgeSvgMatrixMul(tParent, pText->tStyle.tTransform);
	tBounds = __xgeSvgMatrixRectBounds(tMatrix, tBounds);
	iRet = __xgeSvgApplyItemClipAndMask(&tBounds, tMatrix, pText->bClipRect, pText->tClipRect, pText->pMaskScene, pText->iMaskMethod, fTolerance);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	*pBounds = tBounds;
	return XGE_OK;
}

static int __xgeSvgImageItemBounds(const xge_svg_image_item_t* pImage, xge_shape_ex_matrix_t tParent, float fTolerance, xge_rect_t* pBounds, int iDepth)
{
	xge_shape_ex_matrix_t tMatrix;
	int iRet;

	if ( pBounds == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pBounds, 0, sizeof(*pBounds));
	if ( (pImage == NULL) || !__xgeSvgValid(pImage->pSvg) ) {
		return XGE_OK;
	}
	if ( !pImage->tStyle.bVisible || !pImage->tStyle.bVisibility || (pImage->tStyle.fOpacity <= 0.0f) ) {
		return XGE_OK;
	}
	if ( iDepth > XGE_SVG_BOUNDS_MAX_DEPTH ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tMatrix = __xgeSvgMatrixMul(tParent, pImage->tStyle.tTransform);
	iRet = __xgeSvgGetDrawBoundsWithParentInternal(pImage->pSvg, pImage->tRect, tMatrix, fTolerance, pBounds, iDepth + 1);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	return __xgeSvgApplyItemClipAndMask(pBounds, tMatrix, pImage->bClipRect, pImage->tClipRect, pImage->pMaskScene, pImage->iMaskMethod, fTolerance);
}

static int __xgeSvgRasterImageItemBounds(const xge_svg_raster_item_t* pImage, xge_shape_ex_matrix_t tParent, float fTolerance, xge_rect_t* pBounds)
{
	xge_shape_ex_matrix_t tMatrix;
	xge_rect_t tImageBox;
	xge_rect_t tLocalDst;
	xge_rect_t tDst;
	uint32_t iColor;
	int iAlpha;
	int iRet;

	if ( pBounds == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pBounds, 0, sizeof(*pBounds));
	if ( (pImage == NULL) || (pImage->pRaster == NULL) ) {
		return XGE_OK;
	}
	if ( !pImage->tStyle.bVisible || !pImage->tStyle.bVisibility || (pImage->tStyle.fOpacity <= 0.0f) ) {
		return XGE_OK;
	}
	if ( (pImage->pRaster->tImage.iWidth <= 0) || (pImage->pRaster->tImage.iHeight <= 0) ) {
		return XGE_OK;
	}
	iColor = pImage->iColor;
	iAlpha = (int)(pImage->tStyle.fOpacity * (float)XGE_COLOR_GET_A(iColor) + 0.5f);
	if ( iAlpha <= 0 ) {
		return XGE_OK;
	}
	tMatrix = __xgeSvgMatrixMul(tParent, pImage->tStyle.tTransform);
	tImageBox.fX = 0.0f;
	tImageBox.fY = 0.0f;
	tImageBox.fW = (float)pImage->pRaster->tImage.iWidth;
	tImageBox.fH = (float)pImage->pRaster->tImage.iHeight;
	tLocalDst = __xgeSvgAspectViewport(tImageBox, pImage->tRect, pImage->iAspectAlignX, pImage->iAspectAlignY, pImage->iAspectMeetOrSlice);
	tDst = __xgeSvgMatrixRectBounds(tMatrix, tLocalDst);
	iRet = __xgeSvgApplyItemClipAndMask(&tDst, tMatrix, pImage->bClipRect, pImage->tClipRect, pImage->pMaskScene, pImage->iMaskMethod, fTolerance);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	*pBounds = tDst;
	return XGE_OK;
}

static int __xgeSvgShapeContainsPointTransformed(xge_shape_ex pShape, xge_shape_ex_matrix_t tParent, xge_vec2_t tPoint, float fTolerance, int* pContains)
{
	int bVisible;
	int bFillVisible;
	int bStrokeVisible;
	float fOpacity;

	if ( pContains == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pContains = 0;
	if ( pShape == NULL ) {
		return XGE_OK;
	}
	bVisible = 1;
	fOpacity = 1.0f;
	(void)xgeShapeExVisibleGet(pShape, &bVisible);
	(void)xgeShapeExOpacityGet(pShape, &fOpacity);
	if ( !bVisible || (fOpacity <= 0.0f) ) {
		return XGE_OK;
	}
	bFillVisible = __xgeSvgShapeFillPaintVisible(pShape);
	bStrokeVisible = __xgeSvgShapeStrokePaintVisible(pShape);
	if ( !bFillVisible && !bStrokeVisible ) {
		return XGE_OK;
	}
	return xgeShapeExContainsPointEx(pShape, tPoint.fX, tPoint.fY, fTolerance, &tParent, pContains);
}

static int __xgeSvgSceneContainsPointTransformed(xge_shape_ex_scene pScene, int iMethod, xge_shape_ex_matrix_t tParent, xge_vec2_t tPoint, float fTolerance, int* pContains)
{
	xge_shape_ex pHitShape;
	int iRet;

	if ( pContains == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pContains = 0;
	if ( pScene == NULL ) {
		return XGE_OK;
	}
	pHitShape = NULL;
	iRet = xgeShapeExSceneHitTestEx(pScene, tPoint.fX, tPoint.fY, fTolerance, &tParent, &pHitShape);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	*pContains = (pHitShape != NULL) && __xgeSvgMaskShapePaintVisible(pHitShape, __xgeSvgItemMaskMethod(iMethod));
	return XGE_OK;
}

static int __xgeSvgItemClipAndMaskContainsPoint(xge_shape_ex_matrix_t tMatrix, int bClipRect, xge_rect_t tClipRect, xge_shape_ex_scene pMaskScene, int iMaskMethod, xge_vec2_t tPoint, float fTolerance, int* pContains)
{
	int iRet;
	int bMaskContains;

	if ( pContains == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pContains = 1;
	if ( bClipRect && !__xgeSvgRectContainsPoint(__xgeSvgMatrixRectBounds(tMatrix, tClipRect), tPoint) ) {
		*pContains = 0;
		return XGE_OK;
	}
	if ( pMaskScene != NULL ) {
		bMaskContains = 0;
		iRet = __xgeSvgSceneContainsPointTransformed(pMaskScene, iMaskMethod, tMatrix, tPoint, fTolerance, &bMaskContains);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		if ( !bMaskContains ) {
			*pContains = 0;
		}
	}
	return XGE_OK;
}

static int __xgeSvgTextItemContainsPoint(xge_svg pSvg, const xge_svg_text_item_t* pText, xge_shape_ex_matrix_t tParent, xge_vec2_t tPoint, float fTolerance, int* pContains)
{
	xge_shape_ex_matrix_t tMatrix;
	xge_rect_t tBounds;
	int bClipContains;
	int iRet;

	if ( pContains == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pContains = 0;
	iRet = __xgeSvgTextItemBounds(pSvg, pText, tParent, fTolerance, &tBounds);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( !__xgeSvgRectContainsPoint(tBounds, tPoint) ) {
		return XGE_OK;
	}
	tMatrix = __xgeSvgMatrixMul(tParent, pText->tStyle.tTransform);
	bClipContains = 0;
	iRet = __xgeSvgItemClipAndMaskContainsPoint(
		tMatrix,
		pText->bClipRect,
		pText->tClipRect,
		pText->pMaskScene,
		pText->iMaskMethod,
		tPoint,
		fTolerance,
		&bClipContains
	);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	*pContains = bClipContains;
	return XGE_OK;
}

static int __xgeSvgImageItemContainsPoint(const xge_svg_image_item_t* pImage, xge_shape_ex_matrix_t tParent, xge_vec2_t tPoint, float fTolerance, int* pContains, int iDepth)
{
	xge_shape_ex_matrix_t tMatrix;
	int bClipContains;
	int iRet;

	if ( pContains == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pContains = 0;
	if ( (pImage == NULL) || !__xgeSvgValid(pImage->pSvg) ) {
		return XGE_OK;
	}
	if ( !pImage->tStyle.bVisible || !pImage->tStyle.bVisibility || (pImage->tStyle.fOpacity <= 0.0f) ) {
		return XGE_OK;
	}
	if ( iDepth > XGE_SVG_BOUNDS_MAX_DEPTH ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tMatrix = __xgeSvgMatrixMul(tParent, pImage->tStyle.tTransform);
	bClipContains = 0;
	iRet = __xgeSvgItemClipAndMaskContainsPoint(tMatrix, pImage->bClipRect, pImage->tClipRect, pImage->pMaskScene, pImage->iMaskMethod, tPoint, fTolerance, &bClipContains);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( !bClipContains ) {
		return XGE_OK;
	}
	return __xgeSvgDrawContainsPointWithParentInternal(pImage->pSvg, pImage->tRect, tMatrix, tPoint, fTolerance, pContains, iDepth + 1);
}

static int __xgeSvgRasterImageItemContainsPoint(const xge_svg_raster_item_t* pImage, xge_shape_ex_matrix_t tParent, xge_vec2_t tPoint, float fTolerance, int* pContains)
{
	const xge_image_t* pSource;
	const unsigned char* pPixel;
	xge_shape_ex_matrix_t tMatrix;
	xge_shape_ex_matrix_t tInverse;
	xge_rect_t tImageBox;
	xge_rect_t tLocalDst;
	xge_rect_t tDst;
	xge_vec2_t tLocalPoint;
	uint32_t iColor;
	float fU;
	float fV;
	int iAlpha;
	int iX;
	int iY;
	int bClipContains;
	int iRet;

	if ( pContains == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pContains = 0;
	if ( (pImage == NULL) || (pImage->pRaster == NULL) ) {
		return XGE_OK;
	}
	pSource = &pImage->pRaster->tImage;
	if ( !pImage->tStyle.bVisible || !pImage->tStyle.bVisibility || (pImage->tStyle.fOpacity <= 0.0f) ) {
		return XGE_OK;
	}
	if ( (pSource->iWidth <= 0) || (pSource->iHeight <= 0) || (pSource->iFormat != XGE_PIXEL_RGBA8) || (pSource->pPixels == NULL) || (pSource->iStride < pSource->iWidth * 4) ) {
		return XGE_OK;
	}
	iColor = pImage->iColor;
	iAlpha = (int)(pImage->tStyle.fOpacity * (float)XGE_COLOR_GET_A(iColor) + 0.5f);
	if ( iAlpha <= 0 ) {
		return XGE_OK;
	}
	tMatrix = __xgeSvgMatrixMul(tParent, pImage->tStyle.tTransform);
	tImageBox.fX = 0.0f;
	tImageBox.fY = 0.0f;
	tImageBox.fW = (float)pSource->iWidth;
	tImageBox.fH = (float)pSource->iHeight;
	tLocalDst = __xgeSvgAspectViewport(tImageBox, pImage->tRect, pImage->iAspectAlignX, pImage->iAspectAlignY, pImage->iAspectMeetOrSlice);
	if ( (tLocalDst.fW <= 0.0f) || (tLocalDst.fH <= 0.0f) ) {
		return XGE_OK;
	}
	tDst = __xgeSvgMatrixRectBounds(tMatrix, tLocalDst);
	if ( !__xgeSvgRectContainsPoint(tDst, tPoint) ) {
		return XGE_OK;
	}
	bClipContains = 0;
	iRet = __xgeSvgItemClipAndMaskContainsPoint(tMatrix, pImage->bClipRect, pImage->tClipRect, pImage->pMaskScene, pImage->iMaskMethod, tPoint, fTolerance, &bClipContains);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( !bClipContains ) {
		return XGE_OK;
	}
	if ( !__xgeSvgMatrixInvert(tMatrix, &tInverse) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tLocalPoint = __xgeSvgMatrixPoint(tInverse, tPoint);
	if ( !__xgeSvgRectContainsPoint(tLocalDst, tLocalPoint) ) {
		return XGE_OK;
	}
	fU = (tLocalPoint.fX - tLocalDst.fX) / tLocalDst.fW;
	fV = (tLocalPoint.fY - tLocalDst.fY) / tLocalDst.fH;
	iX = (int)floorf(fU * (float)pSource->iWidth);
	iY = (int)floorf(fV * (float)pSource->iHeight);
	if ( iX < 0 ) iX = 0;
	if ( iY < 0 ) iY = 0;
	if ( iX >= pSource->iWidth ) iX = pSource->iWidth - 1;
	if ( iY >= pSource->iHeight ) iY = pSource->iHeight - 1;
	pPixel = (const unsigned char*)pSource->pPixels + ((size_t)iY * (size_t)pSource->iStride) + ((size_t)iX * 4u);
	*pContains = pPixel[3] > 0;
	return XGE_OK;
}

static int __xgeSvgBoundsItemContainsPoint(xge_rect_t tBounds, xge_vec2_t tPoint, int* pContains)
{
	if ( pContains == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pContains = __xgeSvgRectContainsPoint(tBounds, tPoint);
	return XGE_OK;
}

static int __xgeSvgGroupChildrenBoundsWithParent(
	xge_svg pSvg,
	xge_svg_group_item_t* pGroup,
	xge_shape_ex_matrix_t tParent,
	float fTolerance,
	xge_rect_t* pBounds,
	int* pHasBounds,
	int iDepth
)
{
	xge_rect_t tOut;
	int bHasBounds;
	int i;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pGroup == NULL) || (pBounds == NULL) || (pHasBounds == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(&tOut, 0, sizeof(tOut));
	bHasBounds = 0;
	for ( i = 0; i < pGroup->iItemCount; i++ ) {
		xge_rect_t tChildBounds;

		iRet = __xgeSvgDrawItemBoundsWithParent(
			pSvg, &pGroup->pItems[i], tParent, fTolerance, &tChildBounds, iDepth
		);
		if ( iRet != XGE_OK ) return iRet;
		iRet = __xgeSvgBoundsUnion(&tOut, &bHasBounds, tChildBounds);
		if ( iRet != XGE_OK ) return iRet;
	}
	*pBounds = bHasBounds ? tOut : (xge_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	*pHasBounds = bHasBounds;
	return XGE_OK;
}

static int __xgeSvgGroupMaskSceneEnsure(
	xge_svg pSvg,
	xge_svg_group_item_t* pGroup,
	xge_rect_t tLocalBounds
)
{
	xge_svg_mask_t* pMask;
	int iMask;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pGroup == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( (pGroup->sMaskId[0] == '\0') || pGroup->bMaskResolved ) return XGE_OK;
	if ( __xgeSvgDrawItemsHavePendingUse(pGroup->pItems, pGroup->iItemCount) ) return XGE_OK;
	iMask = __xgeSvgMaskFind(pSvg, pGroup->sMaskId);
	if ( iMask < 0 ) {
		pGroup->sMaskId[0] = '\0';
		pGroup->bMaskResolved = 1;
		return XGE_OK;
	}
	pMask = &pSvg->pMasks[iMask];
	pGroup->iMaskMethod = __xgeSvgMaskMethod(pMask);
	if ( (tLocalBounds.fW <= 0.0f) || (tLocalBounds.fH <= 0.0f) ) {
		pGroup->bMaskResolved = 1;
		pGroup->bMaskEmpty = 1;
		return XGE_OK;
	}
	iRet = __xgeSvgMaskSceneResolve(pMask, tLocalBounds, &pGroup->pMaskScene);
	if ( iRet != XGE_OK ) return iRet;
	pGroup->bMaskResolved = 1;
	pGroup->bMaskEmpty = pGroup->pMaskScene == NULL;
	return XGE_OK;
}

static int __xgeSvgItemContainsPoint(xge_svg pSvg, const xge_svg_draw_item_t* pItem, xge_shape_ex_matrix_t tParent, xge_vec2_t tPoint, float fTolerance, int* pContains, int iDepth)
{
	xge_rect_t tBounds;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pItem == NULL) || (pContains == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeSvgPaintRuntimeParent(pItem, tParent, &tParent);
	if ( iRet != XGE_OK ) return iRet;
	*pContains = 0;
	memset(&tBounds, 0, sizeof(tBounds));
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_GROUP ) {
		xge_svg_group_item_t* pGroup;
		int i;

		if ( (pItem->u.pGroup == NULL) || (iDepth > XGE_SVG_BOUNDS_MAX_DEPTH) ) {
			return pItem->u.pGroup == NULL ? XGE_ERROR_INVALID_ARGUMENT : XGE_OK;
		}
		pGroup = pItem->u.pGroup;
		if ( pGroup->sMaskId[0] != '\0' ) {
			xge_rect_t tLocalBounds;
			int bHasLocalBounds;
			int bMaskContains;

			iRet = __xgeSvgGroupChildrenBoundsWithParent(
				pSvg, pGroup, __xgeSvgMatrixIdentity(), 0.0f,
				&tLocalBounds, &bHasLocalBounds, iDepth + 1
			);
			if ( iRet != XGE_OK ) return iRet;
			iRet = __xgeSvgGroupMaskSceneEnsure(pSvg, pGroup, tLocalBounds);
			if ( iRet != XGE_OK ) return iRet;
			if ( pGroup->bMaskEmpty ) return XGE_OK;
			if ( pGroup->pMaskScene != NULL ) {
				bMaskContains = 0;
				iRet = __xgeSvgSceneContainsPointTransformed(
					pGroup->pMaskScene, __xgeSvgItemMaskMethod(pGroup->iMaskMethod),
					tParent, tPoint, fTolerance, &bMaskContains
				);
				if ( iRet != XGE_OK ) return iRet;
				if ( !bMaskContains ) return XGE_OK;
			}
		}
		for ( i = pGroup->iItemCount - 1; i >= 0; i-- ) {
			int bChildContains = 0;
			iRet = __xgeSvgItemContainsPoint(
				pSvg, &pGroup->pItems[i], tParent, tPoint,
				fTolerance, &bChildContains, iDepth + 1
			);
			if ( iRet != XGE_OK ) return iRet;
			if ( bChildContains ) {
				*pContains = 1;
				return XGE_OK;
			}
		}
		return XGE_OK;
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_SCENE ) {
		return __xgeSvgSceneContainsPointTransformed(pItem->u.pScene, XGE_SHAPE_EX_MASK_ALPHA, tParent, tPoint, fTolerance, pContains);
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_SHAPE ) {
		return __xgeSvgShapeContainsPointTransformed(pItem->u.pShape, tParent, tPoint, fTolerance, pContains);
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_FILTER ) {
		tBounds = __xgeSvgMatrixRectBounds(
			tParent, __xgeSvgFilterEffectVisualBounds(pSvg, pItem->u.pFilter)
		);
		iRet = XGE_OK;
	} else if ( pItem->iType == XGE_SVG_DRAW_ITEM_TEXT ) {
		return __xgeSvgTextItemContainsPoint(pSvg, &pItem->u.tText, tParent, tPoint, fTolerance, pContains);
	} else if ( pItem->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE ) {
		return __xgeSvgImageItemContainsPoint(&pItem->u.tImage, tParent, tPoint, fTolerance, pContains, iDepth + 1);
	} else if ( pItem->iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE ) {
		return __xgeSvgRasterImageItemContainsPoint(&pItem->u.tRaster, tParent, tPoint, fTolerance, pContains);
	} else {
		iRet = XGE_OK;
	}
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	return __xgeSvgBoundsItemContainsPoint(tBounds, tPoint, pContains);
}

static int __xgeSvgDrawItemIntersectsWithParent(
	xge_svg pSvg,
	const xge_svg_draw_item_t* pItem,
	xge_shape_ex_matrix_t tParent,
	xge_rect_t tRect,
	float fTolerance,
	int* pIntersects,
	int iDepth
)
{
	xge_shape_ex_matrix_t tBoundsParent = tParent;
	xge_rect_t tBounds;
	int iRet;
	int i;

	if ( !__xgeSvgValid(pSvg) || (pItem == NULL) || (pIntersects == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeSvgPaintRuntimeParent(pItem, tParent, &tParent);
	if ( iRet != XGE_OK ) return iRet;
	*pIntersects = 0;
	if ( iDepth > XGE_SVG_BOUNDS_MAX_DEPTH ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_GROUP ) {
		if ( pItem->u.pGroup == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
		for ( i = pItem->u.pGroup->iItemCount - 1; i >= 0; i-- ) {
			iRet = __xgeSvgDrawItemIntersectsWithParent(
				pSvg, &pItem->u.pGroup->pItems[i], tParent, tRect,
				fTolerance, pIntersects, iDepth + 1
			);
			if ( (iRet != XGE_OK) || *pIntersects ) return iRet;
		}
		return XGE_OK;
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_SHAPE ) {
		return xgeShapeExIntersectsEx(
			pItem->u.pShape, tRect, fTolerance, &tParent, pIntersects
		);
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_SCENE ) {
		return xgeShapeExSceneIntersectsEx(
			pItem->u.pScene, tRect, fTolerance, &tParent, pIntersects
		);
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_FILTER ) {
		if ( pItem->u.pFilter == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
		for ( i = pItem->u.pFilter->iItemCount - 1; i >= 0; i-- ) {
			iRet = __xgeSvgDrawItemIntersectsWithParent(
				pSvg, &pItem->u.pFilter->pItems[i], tParent, tRect,
				fTolerance, pIntersects, iDepth + 1
			);
			if ( (iRet != XGE_OK) || *pIntersects ) return iRet;
		}
		return XGE_OK;
	}
	memset(&tBounds, 0, sizeof(tBounds));
	iRet = __xgeSvgDrawItemBoundsWithParent(
		pSvg, (xge_svg_draw_item_t*)pItem, tBoundsParent,
		fTolerance, &tBounds, iDepth + 1
	);
	if ( iRet != XGE_OK ) return iRet;
	*pIntersects = (tBounds.fW > 0.0f) && (tBounds.fH > 0.0f) &&
		(tRect.fX < tBounds.fX + tBounds.fW) &&
		(tRect.fX + tRect.fW > tBounds.fX) &&
		(tRect.fY < tBounds.fY + tBounds.fH) &&
		(tRect.fY + tRect.fH > tBounds.fY);
	return XGE_OK;
}

static int __xgeSvgContainsPointItemsWithParent(xge_svg pSvg, xge_shape_ex_matrix_t tParent, xge_vec2_t tPoint, float fTolerance, int* pContains, int iDepth)
{
	int i;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pContains == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iDepth > XGE_SVG_BOUNDS_MAX_DEPTH ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pContains = 0;
	for ( i = pSvg->iItemCount - 1; i >= 0; i-- ) {
		int bItemContains;

		iRet = __xgeSvgItemContainsPoint(pSvg, &pSvg->pItems[i], tParent, tPoint, fTolerance, &bItemContains, iDepth + 1);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		if ( bItemContains ) {
			*pContains = 1;
			return XGE_OK;
		}
	}
	return XGE_OK;
}

static int __xgeSvgDrawContainsPointWithParentInternal(xge_svg pSvg, xge_rect_t tDst, xge_shape_ex_matrix_t tParent, xge_vec2_t tPoint, float fTolerance, int* pContains, int iDepth)
{
	xge_shape_ex_matrix_t tInverse;
	xge_shape_ex_matrix_t tLocalMatrix;
	xge_shape_ex_matrix_t tMatrix;
	xge_rect_t tViewport;
	xge_rect_t tViewBox;
	xge_vec2_t tLocalPoint;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pContains == NULL) || !__xgeSvgFloatFinite(tPoint.fX) || !__xgeSvgFloatFinite(tPoint.fY) || !__xgeSvgFloatFinite(fTolerance) ||
	     !__xgeSvgFloatFinite(tDst.fX) || !__xgeSvgFloatFinite(tDst.fY) || !__xgeSvgFloatFinite(tDst.fW) || !__xgeSvgFloatFinite(tDst.fH) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pContains = 0;
	if ( iDepth > XGE_SVG_BOUNDS_MAX_DEPTH ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tViewBox = pSvg->tViewBox;
	if ( (tViewBox.fW <= 0.0f) || (tViewBox.fH <= 0.0f) || (tDst.fW <= 0.0f) || (tDst.fH <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeSvgPaintHierarchyRuntimeParent(pSvg, tParent, &tParent);
	if ( iRet != XGE_OK ) return iRet;
	if ( !__xgeSvgMatrixInvert(tParent, &tInverse) ) {
		return XGE_OK;
	}
	tLocalPoint = __xgeSvgMatrixPoint(tInverse, tPoint);
	if ( !__xgeSvgRectContainsPoint(tDst, tLocalPoint) ) {
		return XGE_OK;
	}
	iRet = xgeSvgGetDrawViewport(pSvg, tDst, &tViewport);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	tLocalMatrix = __xgeSvgViewBoxMatrix(tViewBox, tViewport);
	tMatrix = __xgeSvgMatrixMul(tParent, tLocalMatrix);
	return __xgeSvgContainsPointItemsWithParent(pSvg, tMatrix, tPoint, fTolerance, pContains, iDepth + 1);
}

static int __xgeSvgDrawContainsPointInternal(xge_svg pSvg, xge_rect_t tDst, xge_vec2_t tPoint, float fTolerance, int* pContains, int iDepth)
{
	return __xgeSvgDrawContainsPointWithParentInternal(
		pSvg, tDst, __xgeSvgMatrixIdentity(), tPoint, fTolerance, pContains, iDepth
	);
}

static int __xgeSvgDrawItemBoundsWithParent(
	xge_svg pSvg,
	xge_svg_draw_item_t* pItem,
	xge_shape_ex_matrix_t tParent,
	float fTolerance,
	xge_rect_t* pBounds,
	int iDepth
)
{
	int iRuntimeRet;

	if ( !__xgeSvgValid(pSvg) || (pItem == NULL) || (pBounds == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRuntimeRet = __xgeSvgPaintRuntimeParent(pItem, tParent, &tParent);
	if ( iRuntimeRet != XGE_OK ) return iRuntimeRet;
	memset(pBounds, 0, sizeof(*pBounds));
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_GROUP ) {
		xge_rect_t tOut;
		xge_svg_group_item_t* pGroup;
		int bHasBounds;
		int iRet;

		if ( (pItem->u.pGroup == NULL) || (iDepth > XGE_SVG_BOUNDS_MAX_DEPTH) ) {
			return pItem->u.pGroup == NULL ? XGE_ERROR_INVALID_ARGUMENT : XGE_OK;
		}
		pGroup = pItem->u.pGroup;
		iRet = __xgeSvgGroupChildrenBoundsWithParent(
			pSvg, pGroup, tParent, fTolerance, &tOut, &bHasBounds, iDepth + 1
		);
		if ( iRet != XGE_OK ) return iRet;
		if ( (pGroup->sMaskId[0] != '\0') && bHasBounds ) {
			xge_rect_t tLocalBounds;
			xge_rect_t tMaskBounds;
			int bHasLocalBounds;

			iRet = __xgeSvgGroupChildrenBoundsWithParent(
				pSvg, pGroup, __xgeSvgMatrixIdentity(), 0.0f,
				&tLocalBounds, &bHasLocalBounds, iDepth + 1
			);
			if ( iRet != XGE_OK ) return iRet;
			iRet = __xgeSvgGroupMaskSceneEnsure(pSvg, pGroup, tLocalBounds);
			if ( iRet != XGE_OK ) return iRet;
			if ( pGroup->bMaskEmpty ) {
				bHasBounds = 0;
			} else if ( pGroup->pMaskScene != NULL ) {
				iRet = __xgeSvgSceneBoundsTransformed(
					pGroup->pMaskScene, __xgeSvgItemMaskMethod(pGroup->iMaskMethod),
					fTolerance, tParent, &tMaskBounds
				);
				if ( iRet != XGE_OK ) return iRet;
				tOut = __xgeSvgRectIntersect(tOut, tMaskBounds);
				bHasBounds = (tOut.fW > 0.0f) && (tOut.fH > 0.0f);
			}
		}
		*pBounds = bHasBounds ? tOut : (xge_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
		return XGE_OK;
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_SCENE ) {
		return __xgeSvgSceneBoundsTransformed(pItem->u.pScene, XGE_SHAPE_EX_MASK_ALPHA, fTolerance, tParent, pBounds);
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_SHAPE ) {
		return __xgeSvgShapeBoundsTransformed(pItem->u.pShape, fTolerance, tParent, 1, pBounds);
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_FILTER ) {
		*pBounds = __xgeSvgMatrixRectBounds(
			tParent, __xgeSvgFilterEffectVisualBounds(pSvg, pItem->u.pFilter)
		);
		return XGE_OK;
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_TEXT ) {
		return __xgeSvgTextItemBounds(pSvg, &pItem->u.tText, tParent, fTolerance, pBounds);
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE ) {
		return __xgeSvgImageItemBounds(&pItem->u.tImage, tParent, fTolerance, pBounds, iDepth + 1);
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE ) {
		return __xgeSvgRasterImageItemBounds(&pItem->u.tRaster, tParent, fTolerance, pBounds);
	}
	return XGE_OK;
}

static int __xgeSvgContainsPointWithParent(xge_svg pSvg, xge_shape_ex_matrix_t tParent, xge_vec2_t tPoint, float fTolerance, int* pContains, int iDepth)
{
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pContains == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	iRet = __xgeSvgPaintRetainedHierarchyParent(pSvg, tParent, &tParent);
	if ( iRet != XGE_OK ) return iRet;
	return __xgeSvgContainsPointItemsWithParent(
		pSvg, tParent, tPoint, fTolerance, pContains, iDepth
	);
}

static int __xgeSvgFilterEffectRefreshPendingUseBounds(
	xge_svg pSvg,
	xge_svg_filter_effect_t* pEffect
)
{
	xge_rect_t tBounds;
	xge_rect_t tOutputRegion;
	float fBlurX;
	float fBlurY;
	int bHasBounds;
	int iFilterState;
	int i;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pEffect == NULL) ||
	     (pEffect->iFilterIndex < 0) || (pEffect->iFilterIndex >= pSvg->iFilterCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( !pEffect->bPendingUseBounds ||
	     __xgeSvgDrawItemsHavePendingUse(pEffect->pItems, pEffect->iItemCount) ) {
		return XGE_OK;
	}
	memset(&tBounds, 0, sizeof(tBounds));
	memset(&tOutputRegion, 0, sizeof(tOutputRegion));
	bHasBounds = 0;
	for ( i = 0; i < pEffect->iItemCount; i++ ) {
		xge_rect_t tItemBounds;

		iRet = __xgeSvgDrawItemBoundsWithParent(
			pSvg, &pEffect->pItems[i], __xgeSvgMatrixIdentity(), 0.0f,
			&tItemBounds, 0
		);
		if ( iRet != XGE_OK ) return iRet;
		iRet = __xgeSvgBoundsUnion(&tBounds, &bHasBounds, tItemBounds);
		if ( iRet != XGE_OK ) return iRet;
	}
	if ( bHasBounds ) {
		iFilterState = __xgeSvgFilterGaussianBlurForBounds(
			pSvg, pSvg->pFilters[pEffect->iFilterIndex].sId, tBounds,
			&fBlurX, &fBlurY, &tOutputRegion
		);
		pEffect->tObjectBounds = tBounds;
		if ( iFilterState > 0 ) {
			pEffect->tOutputRegion = tOutputRegion;
		} else {
			memset(&pEffect->tOutputRegion, 0, sizeof(pEffect->tOutputRegion));
		}
	} else {
		memset(&pEffect->tObjectBounds, 0, sizeof(pEffect->tObjectBounds));
		memset(&pEffect->tOutputRegion, 0, sizeof(pEffect->tOutputRegion));
	}
	pEffect->bPendingUseBounds = 0;
	return XGE_OK;
}

static int __xgeSvgWrapGroupFilter(
	xge_svg pSvg,
	int iFirstItem,
	int iFilterIndex,
	xge_shape_ex_matrix_t tTransform
)
{
	xge_svg_filter_t* pFilter;
	xge_rect_t tBounds;
	xge_rect_t tOutputRegion;
	float fBlurX;
	float fBlurY;
	int bHasBounds;
	int bHasPendingUse;
	int iFilterState;
	int iRet;
	int i;

	if ( !__xgeSvgValid(pSvg) || (iFirstItem < 0) || (iFirstItem > pSvg->iItemCount) ||
	     (iFilterIndex < 0) || (iFilterIndex >= pSvg->iFilterCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iFirstItem == pSvg->iItemCount ) {
		return XGE_OK;
	}
	pFilter = &pSvg->pFilters[iFilterIndex];
	if ( pFilter->iPrimitiveCount <= 0 ) {
		return XGE_OK;
	}
	bHasPendingUse = __xgeSvgDrawItemsHavePendingUse(
		pSvg->pItems + iFirstItem, pSvg->iItemCount - iFirstItem
	);
	memset(&tBounds, 0, sizeof(tBounds));
	memset(&tOutputRegion, 0, sizeof(tOutputRegion));
	bHasBounds = 0;
	for ( i = iFirstItem; i < pSvg->iItemCount; i++ ) {
		xge_rect_t tItemBounds;

		iRet = __xgeSvgDrawItemBoundsWithParent(
			pSvg, &pSvg->pItems[i], __xgeSvgMatrixIdentity(), 0.0f, &tItemBounds, 0
		);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		iRet = __xgeSvgBoundsUnion(&tBounds, &bHasBounds, tItemBounds);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	if ( !bHasBounds && !bHasPendingUse ) {
		return XGE_OK;
	}
	iFilterState = 0;
	if ( bHasBounds ) {
		iFilterState = __xgeSvgFilterGaussianBlurForBounds(
			pSvg, pFilter->sId, tBounds, &fBlurX, &fBlurY, &tOutputRegion
		);
		if ( (iFilterState <= 0) && !bHasPendingUse ) {
			return XGE_OK;
		}
	}
	if ( (tOutputRegion.fW <= 0.0f) || (tOutputRegion.fH <= 0.0f) ) {
		if ( bHasPendingUse ) {
			iRet = __xgeSvgDrawItemsWrapFilter(
				pSvg, iFirstItem, iFilterIndex, tBounds, tOutputRegion, tTransform
			);
			if ( (iRet == XGE_OK) && (pSvg->iItemCount > iFirstItem) &&
			     (pSvg->pItems[pSvg->iItemCount - 1].iType == XGE_SVG_DRAW_ITEM_FILTER) ) {
				pSvg->pItems[pSvg->iItemCount - 1].u.pFilter->bPendingUseBounds = 1;
			}
			return iRet;
		}
		for ( i = iFirstItem; i < pSvg->iItemCount; i++ ) {
			__xgeSvgDrawItemReset(&pSvg->pItems[i]);
		}
		pSvg->iItemCount = iFirstItem;
		return XGE_OK;
	}
	iRet = __xgeSvgDrawItemsWrapFilter(
		pSvg, iFirstItem, iFilterIndex, tBounds, tOutputRegion, tTransform
	);
	if ( (iRet == XGE_OK) && bHasPendingUse && (pSvg->iItemCount > iFirstItem) &&
	     (pSvg->pItems[pSvg->iItemCount - 1].iType == XGE_SVG_DRAW_ITEM_FILTER) ) {
		pSvg->pItems[pSvg->iItemCount - 1].u.pFilter->bPendingUseBounds = 1;
	}
	return iRet;
}

static int __xgeSvgGetBoundsItemsWithParent(xge_svg pSvg, xge_shape_ex_matrix_t tParent, float fTolerance, xge_rect_t* pBounds, int iDepth)
{
	xge_rect_t tOut;
	int bHasBounds;
	int iRet;
	int i;

	if ( !__xgeSvgValid(pSvg) || (pBounds == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iDepth > XGE_SVG_BOUNDS_MAX_DEPTH ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(&tOut, 0, sizeof(tOut));
	bHasBounds = 0;
	for ( i = 0; i < pSvg->iItemCount; i++ ) {
		xge_svg_draw_item_t* pItem = &pSvg->pItems[i];
		xge_rect_t tItemBounds;

		memset(&tItemBounds, 0, sizeof(tItemBounds));
		iRet = __xgeSvgDrawItemBoundsWithParent(
			pSvg, pItem, tParent, fTolerance, &tItemBounds, iDepth
		);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		iRet = __xgeSvgBoundsUnion(&tOut, &bHasBounds, tItemBounds);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	*pBounds = bHasBounds ? tOut : (xge_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	return XGE_OK;
}

static int __xgeSvgGetBoundsWithParent(xge_svg pSvg, xge_shape_ex_matrix_t tParent, float fTolerance, xge_rect_t* pBounds, int iDepth)
{
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pBounds == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	iRet = __xgeSvgPaintRetainedHierarchyParent(pSvg, tParent, &tParent);
	if ( iRet != XGE_OK ) return iRet;
	return __xgeSvgGetBoundsItemsWithParent(
		pSvg, tParent, fTolerance, pBounds, iDepth
	);
}

static int __xgeSvgGetDrawBoundsWithParentInternal(xge_svg pSvg, xge_rect_t tDst, xge_shape_ex_matrix_t tParent, float fTolerance, xge_rect_t* pBounds, int iDepth)
{
	xge_shape_ex_matrix_t tLocalMatrix;
	xge_shape_ex_matrix_t tMatrix;
	xge_rect_t tClip;
	xge_rect_t tViewport;
	xge_rect_t tViewBox;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pBounds == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pBounds, 0, sizeof(*pBounds));
	if ( iDepth > XGE_SVG_BOUNDS_MAX_DEPTH ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tViewBox = pSvg->tViewBox;
	if ( (tViewBox.fW <= 0.0f) || (tViewBox.fH <= 0.0f) || (tDst.fW <= 0.0f) || (tDst.fH <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = xgeSvgGetDrawViewport(pSvg, tDst, &tViewport);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = __xgeSvgPaintHierarchyRuntimeParent(pSvg, tParent, &tParent);
	if ( iRet != XGE_OK ) return iRet;
	tLocalMatrix = __xgeSvgViewBoxMatrix(tViewBox, tViewport);
	tMatrix = __xgeSvgMatrixMul(tParent, tLocalMatrix);
	iRet = __xgeSvgGetBoundsItemsWithParent(pSvg, tMatrix, fTolerance, pBounds, iDepth + 1);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	tClip = __xgeSvgMatrixRectBounds(tParent, tDst);
	*pBounds = __xgeSvgRectIntersect(*pBounds, tClip);
	return XGE_OK;
}

static int __xgeSvgGetDrawBoundsInternal(xge_svg pSvg, xge_rect_t tDst, float fTolerance, xge_rect_t* pBounds, int iDepth)
{
	return __xgeSvgGetDrawBoundsWithParentInternal(
		pSvg, tDst, __xgeSvgMatrixIdentity(), fTolerance, pBounds, iDepth
	);
}

static int __xgeSvgDrawClipBegin(xge_rect_t tClip, xge_rect_t* pOldClip, int* pOldClipEnabled, int* pClipApplied)
{
	int iRet;

	if ( (pOldClip == NULL) || (pOldClipEnabled == NULL) || (pClipApplied == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pOldClipEnabled = 0;
	*pClipApplied = 0;
	if ( (tClip.fW <= 0.0f) || (tClip.fH <= 0.0f) ) {
		return XGE_OK;
	}
	iRet = __xgeShapeAutoBatchFlush();
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	*pOldClip = xgeClipGet();
	*pOldClipEnabled = (pOldClip->fW > 0.0f) && (pOldClip->fH > 0.0f);
	if ( *pOldClipEnabled ) {
		tClip = __xgeSvgRectIntersect(*pOldClip, tClip);
	}
	tClip = __xgeSvgPixelCenterClipRect(tClip);
	xgeClipSet(tClip);
	*pClipApplied = 1;
	return XGE_OK;
}

static int __xgeSvgDrawClipEnd(xge_rect_t tOldClip, int bOldClipEnabled, int bClipApplied, int iRet)
{
	if ( bClipApplied ) {
		int iFlushRet = __xgeShapeAutoBatchFlush();

		if ( iRet == XGE_OK ) {
			iRet = iFlushRet;
		}
		if ( bOldClipEnabled ) {
			xgeClipSet(tOldClip);
		} else {
			xgeClipClear();
		}
	}
	return iRet;
}

static int __xgeSvgDrawBlendBegin(int bBlendSet, int iBlend, int* pOldBlend, int* pBlendApplied)
{
	int iOldBlend;
	int iRet;

	if ( (pOldBlend == NULL) || (pBlendApplied == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pOldBlend = XGE_BLEND_ALPHA;
	*pBlendApplied = 0;
	if ( !bBlendSet ) {
		return XGE_OK;
	}
	if ( (iBlend < XGE_BLEND_NONE) || (iBlend > XGE_BLEND_LUMINOSITY) ) {
		iBlend = XGE_BLEND_ALPHA;
	}
	iOldBlend = xgeBlendGet();
	*pOldBlend = iOldBlend;
	if ( iOldBlend == iBlend ) {
		return XGE_OK;
	}
	iRet = __xgeShapeAutoBatchFlush();
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	xgeBlendSet(iBlend);
	*pBlendApplied = 1;
	return XGE_OK;
}

static int __xgeSvgDrawBlendEnd(int iOldBlend, int bBlendApplied, int iRet)
{
	if ( bBlendApplied ) {
		int iFlushRet = __xgeShapeAutoBatchFlush();

		if ( iRet == XGE_OK ) {
			iRet = iFlushRet;
		}
		xgeBlendSet(iOldBlend);
	}
	return iRet;
}

static int __xgeSvgMaskOutputBlend(int bBlendSet, int iBlend, int bSuppressOwnBlend)
{
	if ( bSuppressOwnBlend || !bBlendSet || __xgeShapeExBlendNeedsComposite(iBlend) ||
	     (iBlend < XGE_BLEND_NONE) || (iBlend > XGE_BLEND_LUMINOSITY) ) {
		return XGE_BLEND_ALPHA;
	}
	return iBlend;
}

static int __xgeSvgGaussianRendererEnsure(void)
{
	char sHeader[128];
	char sVertex[1536];
	char sFragment[4096];
	int iRet;

	if ( g_xgeSvgGaussianRenderer.bInitialized ) {
		return XGE_OK;
	}
	if ( __xgeGraphicsShaderHeaderGet(XGE_GPU_BACKEND_NONE, sHeader, (int)sizeof(sHeader)) < 0 ) {
		return XGE_ERROR_GPU_FAILED;
	}
	snprintf(
		sVertex, sizeof(sVertex),
		"%s"
		"layout(location = 0) in vec4 aPosition;\n"
		"layout(location = 1) in vec2 aUV;\n"
		"uniform vec2 uResolution;\n"
		"out vec2 vUV;\n"
		"void main() {\n"
		"  vec2 pos = (aPosition.xy / uResolution) * 2.0 - 1.0;\n"
		"  pos.y = -pos.y;\n"
		"  gl_Position = vec4(pos, 0.0, 1.0);\n"
		"  vUV = aUV;\n"
		"}\n",
		sHeader
	);
	snprintf(
		sFragment, sizeof(sFragment),
		"%s"
		"in vec2 vUV;\n"
		"uniform sampler2D uTexture;\n"
		"uniform vec2 uDirection;\n"
		"uniform vec2 uTexelSize;\n"
		"uniform vec4 uRegion;\n"
		"uniform float uRadius;\n"
		"out vec4 FragColor;\n"
		"void main() {\n"
		"  vec2 regionMin = uRegion.xy;\n"
		"  vec2 regionMax = uRegion.zw;\n"
		"  if (vUV.x < regionMin.x || vUV.y < regionMin.y || vUV.x >= regionMax.x || vUV.y >= regionMax.y) {\n"
		"    FragColor = vec4(0.0);\n"
		"    return;\n"
		"  }\n"
		"  vec2 sampleMin = (ceil(regionMin / uTexelSize - vec2(0.5)) + vec2(0.5)) * uTexelSize;\n"
		"  vec2 sampleMax = (ceil(regionMax / uTexelSize - vec2(0.5)) - vec2(0.5)) * uTexelSize;\n"
		"  sampleMax = max(sampleMin, sampleMax);\n"
		"  int radius = int(uRadius + 0.5);\n"
		"  vec4 sum = vec4(0.0);\n"
		"  for (int i = -radius; i <= radius; ++i) {\n"
		"    vec2 coord = vUV + uDirection * float(i);\n"
		"    coord = clamp(coord, sampleMin, sampleMax);\n"
		"    sum += texture(uTexture, coord);\n"
		"  }\n"
		"  sum /= float(radius + radius + 1);\n"
		"  FragColor = floor(clamp(sum, 0.0, 1.0) * 255.0 + 0.0001) / 255.0;\n"
		"}\n",
		sHeader
	);
	iRet = xgeShaderCreate(&g_xgeSvgGaussianRenderer.tShader, sVertex, sFragment);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	g_xgeSvgGaussianRenderer.bInitialized = 1;
	return XGE_OK;
}

static int __xgeSvgGaussianPass(
	xge_render_target pDst,
	xge_render_target pSrc,
	int iRadius,
	int bHorizontal,
	xge_rect_t tRegionUv
)
{
	xge_pass_t tPass;
	xge_material_t tMaterial;
	xge_draw_t tDraw;
	int iRet;

	if ( (pDst == NULL) || (pSrc == NULL) || (iRadius < 0) ||
	     (pDst->iWidth <= 0) || (pDst->iHeight <= 0) ||
	     (pDst->iWidth != pSrc->iWidth) || (pDst->iHeight != pSrc->iHeight) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeSvgGaussianRendererEnsure();
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	memset(&tPass, 0, sizeof(tPass));
	xgePassInit(&tPass, pDst, XGE_PASS_CLEAR_COLOR, XGE_COLOR_RGBA(0, 0, 0, 0));
	iRet = xgePassBegin(&tPass);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = xgeShaderUniform2f(
		&g_xgeSvgGaussianRenderer.tShader,
		"uDirection",
		bHorizontal ? (1.0f / (float)pSrc->iWidth) : 0.0f,
		bHorizontal ? 0.0f : (1.0f / (float)pSrc->iHeight)
	);
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform2f(
			&g_xgeSvgGaussianRenderer.tShader,
			"uTexelSize",
			1.0f / (float)pSrc->iWidth,
			1.0f / (float)pSrc->iHeight
		);
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform4f(
			&g_xgeSvgGaussianRenderer.tShader,
			"uRegion",
			tRegionUv.fX,
			tRegionUv.fY,
			tRegionUv.fX + tRegionUv.fW,
			tRegionUv.fY + tRegionUv.fH
		);
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform1f(&g_xgeSvgGaussianRenderer.tShader, "uRadius", (float)iRadius);
	}
	if ( iRet == XGE_OK ) {
		xgeMaterialInit(&tMaterial);
		xgeMaterialSetShader(&tMaterial, &g_xgeSvgGaussianRenderer.tShader);
		xgeMaterialSetTexture(&tMaterial, xgeRenderTargetTexture(pSrc));
		xgeMaterialSetBlend(&tMaterial, XGE_BLEND_NONE);
		memset(&tDraw, 0, sizeof(tDraw));
		tDraw.pTexture = xgeRenderTargetTexture(pSrc);
		tDraw.tSrc = (xge_rect_t){0.0f, 0.0f, (float)pSrc->iWidth, (float)pSrc->iHeight};
		tDraw.tDst = (xge_rect_t){0.0f, 0.0f, (float)pDst->iWidth, (float)pDst->iHeight};
		tDraw.iColor = XGE_COLOR_RGBA(255, 255, 255, 255);
		tDraw.iFlags = XGE_DRAW_SCREEN_SPACE | XGE_DRAW_FLIP_Y;
		xgeMaterialDraw(&tMaterial, &tDraw);
		xgeMaterialFree(&tMaterial);
	}
	if ( tPass.bActive ) {
		int iEndRet = xgePassEnd(&tPass);

		if ( iRet == XGE_OK ) {
			iRet = iEndRet;
		}
	}
	return iRet;
}

static xge_shape_ex_matrix_t __xgeSvgDrawScreenMatrix(
	xge_shape_ex_matrix_t tParent,
	int bScreenSpace
)
{
	xge_shape_ex_matrix_t tScreen;
	xge_vec2_t tP0;
	xge_vec2_t tPX;
	xge_vec2_t tPY;

	if ( bScreenSpace ) {
		return tParent;
	}
	tP0 = xgeWorldToScreen(__xgeSvgMatrixPoint(tParent, (xge_vec2_t){0.0f, 0.0f}));
	tPX = xgeWorldToScreen(__xgeSvgMatrixPoint(tParent, (xge_vec2_t){1.0f, 0.0f}));
	tPY = xgeWorldToScreen(__xgeSvgMatrixPoint(tParent, (xge_vec2_t){0.0f, 1.0f}));
	tScreen.fA = tPX.fX - tP0.fX;
	tScreen.fB = tPX.fY - tP0.fY;
	tScreen.fC = tPY.fX - tP0.fX;
	tScreen.fD = tPY.fY - tP0.fY;
	tScreen.fE = tP0.fX;
	tScreen.fF = tP0.fY;
	return tScreen;
}

static int __xgeSvgDrawItemInternal(
	xge_svg pSvg,
	xge_svg_draw_item_t* pItem,
	xge_shape_ex_matrix_t tParent,
	float fTolerance,
	int bScreenSpace
);

static int __xgeSvgDrawItemInternalEx(
	xge_svg pSvg,
	xge_svg_draw_item_t* pItem,
	xge_shape_ex_matrix_t tParent,
	float fTolerance,
	int bScreenSpace,
	int bSuppressOwnBlend,
	int iSuppressRuntimeComposition
);

static int __xgeSvgDrawFilterItem(
	xge_svg pSvg,
	xge_svg_filter_effect_t* pEffect,
	xge_shape_ex_matrix_t tParent,
	float fTolerance,
	int bScreenSpace
)
{
	xge_render_target_t tTargetA;
	xge_render_target_t tTargetB;
	xge_render_target pTargetA;
	xge_render_target pTargetB;
	xge_render_target pCurrent;
	xge_render_target pScratch;
	xge_shape_ex_effect_target_set_t* pCachedTargets;
	xge_pass_t tSourcePass;
	xge_shape_ex_matrix_t tScreenParent;
	xge_shape_ex_matrix_t tLocalParent;
	xge_rect_t tProcessLocal;
	xge_rect_t tProcessScreen;
	xge_rect_t tOutputScreen;
	xge_rect_t tOldClip;
	xge_rect_t tEffectClip;
	xge_draw_t tDraw;
	xge_svg_filter_t* pFilter;
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;
	float fCanvasScale;
	float fLocalScaleX;
	float fLocalScaleY;
	int bOldClipEnabled;
	int iWidth;
	int iHeight;
	int bCachedTargets;
	int i;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pEffect == NULL) ||
	     (pEffect->iFilterIndex < 0) || (pEffect->iFilterIndex >= pSvg->iFilterCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pFilter = &pSvg->pFilters[pEffect->iFilterIndex];
	if ( (pEffect->iItemCount <= 0) || (pFilter->iPrimitiveCount <= 0) ) {
		return XGE_OK;
	}
	tScreenParent = __xgeSvgDrawScreenMatrix(tParent, bScreenSpace);
	tProcessLocal = __xgeSvgFilterEffectProcessRegion(pSvg, pEffect);
	tProcessScreen = __xgeSvgMatrixRectBounds(tScreenParent, tProcessLocal);
	tOutputScreen = __xgeSvgMatrixRectBounds(tScreenParent, pEffect->tOutputRegion);
	for ( i = 0; i < pFilter->iPrimitiveCount; i++ ) {
		float fStdX;
		float fStdY;

		__xgeSvgFilterPrimitiveBlurForBounds(
			pFilter, &pFilter->pPrimitives[i], pEffect->tObjectBounds, &fStdX, &fStdY
		);
		if ( (fStdX > XGE_SVG_EPSILON) || (fStdY > XGE_SVG_EPSILON) ) {
			tProcessScreen = tOutputScreen;
			break;
		}
	}
	if ( (tProcessScreen.fW <= 0.0f) || (tProcessScreen.fH <= 0.0f) ||
	     (tOutputScreen.fW <= 0.0f) || (tOutputScreen.fH <= 0.0f) ) {
		return XGE_OK;
	}
	/* ThorVG clips filter scenes with a fast-track axis-aligned rectangle. */
	fLeft = roundf(tProcessScreen.fX);
	fTop = roundf(tProcessScreen.fY);
	fRight = roundf(tProcessScreen.fX + tProcessScreen.fW);
	fBottom = roundf(tProcessScreen.fY + tProcessScreen.fH);
	iWidth = (int)(fRight - fLeft);
	iHeight = (int)(fBottom - fTop);
	if ( (iWidth <= 0) || (iHeight <= 0) ) {
		return XGE_OK;
	}
	if ( glGetIntegerv != NULL ) {
		GLint iMaxTextureSize = 0;

		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &iMaxTextureSize);
		if ( (iMaxTextureSize > 0) && ((iWidth > iMaxTextureSize) || (iHeight > iMaxTextureSize)) ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
	}
	memset(&tTargetA, 0, sizeof(tTargetA));
	memset(&tTargetB, 0, sizeof(tTargetB));
	memset(&tSourcePass, 0, sizeof(tSourcePass));
	pTargetA = &tTargetA;
	pTargetB = &tTargetB;
	pCachedTargets = NULL;
	bCachedTargets = 0;
	bOldClipEnabled = g_xge.bClipEnabled;
	tOldClip = xgeClipGet();
	iRet = __xgeShapeAutoBatchFlush();
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	xgeClipClear();
	pCachedTargets = __xgeShapeExEffectTargetCacheAcquire(iWidth, iHeight);
	bCachedTargets = pCachedTargets != NULL;
	if ( bCachedTargets ) {
		pTargetA = &pCachedTargets->tTargetA;
		pTargetB = &pCachedTargets->tTargetB;
	} else {
		iRet = xgeRenderTargetCreate(pTargetA, iWidth, iHeight);
		if ( iRet == XGE_OK ) {
			iRet = xgeRenderTargetCreate(pTargetB, iWidth, iHeight);
		}
	}
	if ( iRet == XGE_OK ) {
		xgePassInit(&tSourcePass, pTargetA, XGE_PASS_CLEAR_COLOR, XGE_COLOR_RGBA(0, 0, 0, 0));
		iRet = xgePassBegin(&tSourcePass);
	}
	if ( iRet == XGE_OK ) {
		tLocalParent = __xgeSvgMatrixMul(
			__xgeSvgMatrixTranslate(-fLeft, -fTop), tScreenParent
		);
		for ( i = 0; (i < pEffect->iItemCount) && (iRet == XGE_OK); i++ ) {
			iRet = __xgeSvgDrawItemInternal(
				pSvg, &pEffect->pItems[i], tLocalParent, fTolerance, 1
			);
		}
		if ( iRet == XGE_OK ) {
			iRet = __xgeShapeAutoBatchFlush();
		}
	}
	if ( tSourcePass.bActive ) {
		int iEndRet = xgePassEnd(&tSourcePass);

		if ( iRet == XGE_OK ) {
			iRet = iEndRet;
		}
	}
	pCurrent = pTargetA;
	pScratch = pTargetB;
	fCanvasScale = sqrtf(tScreenParent.fA * tScreenParent.fA + tScreenParent.fB * tScreenParent.fB);
	fLocalScaleX = fabsf(pEffect->tTransform.fA);
	fLocalScaleY = fabsf(pEffect->tTransform.fD);
	for ( i = 0; (i < pFilter->iPrimitiveCount) && (iRet == XGE_OK); i++ ) {
		xge_svg_filter_primitive_t* pPrimitive = &pFilter->pPrimitives[i];
		xge_svg_gaussian_kernel_t tKernel;
		xge_rect_t tPrimitiveUv;
		float fStdX;
		float fStdY;
		float fSigma;
		int bHorizontal;
		int bVertical;
		int j;

		tPrimitiveUv = (xge_rect_t){0.0f, 0.0f, 1.0f, 1.0f};
		if ( pPrimitive->iRegionFlags != 0 ) {
			xge_rect_t tFilterRegion;
			xge_rect_t tPrimitiveRegion;
			xge_rect_t tPrimitiveScreen;
			int iFilterRegionState;
			int iPrimitiveRegionState;

			iFilterRegionState = __xgeSvgFilterRegionForBounds(
				pSvg, pFilter->sId, pEffect->tObjectBounds, &tFilterRegion
			);
			iPrimitiveRegionState = (iFilterRegionState > 0) ?
				__xgeSvgFilterPrimitiveRegionForBounds(
					pFilter, pPrimitive, pEffect->tObjectBounds, tFilterRegion, &tPrimitiveRegion
				) : -1;
			if ( iPrimitiveRegionState > 0 ) {
				float fRegionLeft;
				float fRegionTop;
				float fRegionRight;
				float fRegionBottom;

				tPrimitiveRegion = __xgeSvgRectIntersect(tPrimitiveRegion, tFilterRegion);
				tPrimitiveScreen = __xgeSvgMatrixRectBounds(tScreenParent, tPrimitiveRegion);
				fRegionLeft = fmaxf(tPrimitiveScreen.fX, fLeft);
				fRegionTop = fmaxf(tPrimitiveScreen.fY, fTop);
				fRegionRight = fminf(tPrimitiveScreen.fX + tPrimitiveScreen.fW, fRight);
				fRegionBottom = fminf(tPrimitiveScreen.fY + tPrimitiveScreen.fH, fBottom);
				if ( (fRegionRight > fRegionLeft) && (fRegionBottom > fRegionTop) ) {
					tPrimitiveUv.fX = (fRegionLeft - fLeft) / (float)iWidth;
					tPrimitiveUv.fY = 1.0f - ((fRegionBottom - fTop) / (float)iHeight);
					tPrimitiveUv.fW = (fRegionRight - fRegionLeft) / (float)iWidth;
					tPrimitiveUv.fH = (fRegionBottom - fRegionTop) / (float)iHeight;
				} else {
					memset(&tPrimitiveUv, 0, sizeof(tPrimitiveUv));
				}
			} else {
				memset(&tPrimitiveUv, 0, sizeof(tPrimitiveUv));
			}
		}

		__xgeSvgFilterPrimitiveBlurForBounds(
			pFilter, pPrimitive, pEffect->tObjectBounds, &fStdX, &fStdY
		);
		bHorizontal = fStdX > XGE_SVG_EPSILON;
		bVertical = fStdY > XGE_SVG_EPSILON;
		if ( !bHorizontal && !bVertical ) {
			continue;
		}
		fSigma = bHorizontal ?
			(1.25f * (fStdX * fLocalScaleX)) * fCanvasScale :
			(1.25f * (fStdY * fLocalScaleY)) * fCanvasScale;
		if ( __xgeSvgGaussianKernelInit(fSigma, &tKernel) <= 0 ) {
			continue;
		}
		if ( bHorizontal ) {
			for ( j = 0; (j < tKernel.iLevel) && (iRet == XGE_OK); j++ ) {
				xge_render_target pSwap;

				iRet = __xgeSvgGaussianPass(
					pScratch, pCurrent, tKernel.arrRadius[j], 1,
					tPrimitiveUv
				);
				pSwap = pCurrent;
				pCurrent = pScratch;
				pScratch = pSwap;
			}
		}
		if ( bVertical && (iRet == XGE_OK) ) {
			for ( j = 0; (j < tKernel.iLevel) && (iRet == XGE_OK); j++ ) {
				xge_render_target pSwap;

				iRet = __xgeSvgGaussianPass(
					pScratch, pCurrent, tKernel.arrRadius[j], 0,
					tPrimitiveUv
				);
				pSwap = pCurrent;
				pCurrent = pScratch;
				pScratch = pSwap;
			}
		}
	}
	if ( bOldClipEnabled ) {
		xgeClipSet(tOldClip);
	} else {
		xgeClipClear();
	}
	if ( iRet == XGE_OK ) {
		tEffectClip = tOutputScreen;
		if ( bOldClipEnabled ) {
			tEffectClip = __xgeSvgRectIntersect(tEffectClip, tOldClip);
		}
		if ( (tEffectClip.fW > 0.0f) && (tEffectClip.fH > 0.0f) ) {
			tEffectClip = __xgeSvgPixelCenterClipRect(tEffectClip);
			xgeClipSet(tEffectClip);
			memset(&tDraw, 0, sizeof(tDraw));
			tDraw.pTexture = xgeRenderTargetTexture(pCurrent);
			tDraw.tSrc = (xge_rect_t){0.0f, 0.0f, (float)iWidth, (float)iHeight};
			tDraw.tDst = (xge_rect_t){fLeft, fTop, (float)iWidth, (float)iHeight};
			tDraw.iColor = XGE_COLOR_RGBA(255, 255, 255, 255);
			tDraw.iFlags = XGE_DRAW_SCREEN_SPACE | XGE_DRAW_FLIP_Y;
			xgeDrawEx(&tDraw);
			iRet = xgeFlush();
		}
	}
	if ( bOldClipEnabled ) {
		xgeClipSet(tOldClip);
	} else {
		xgeClipClear();
	}
	if ( bCachedTargets ) {
		__xgeShapeExEffectTargetCacheRelease(pCachedTargets);
	} else {
		xgeRenderTargetFree(&tTargetB);
		xgeRenderTargetFree(&tTargetA);
	}
	return iRet;
}

typedef struct xge_svg_blend_item_context_t {
	xge_svg pSvg;
	xge_svg_draw_item_t* pItem;
	float fTolerance;
} xge_svg_blend_item_context_t;

typedef struct xge_svg_group_source_context_t {
	xge_svg pSvg;
	xge_svg_group_item_t* pGroup;
	float fTolerance;
} xge_svg_group_source_context_t;

typedef struct xge_svg_runtime_composition_context_t {
	xge_svg pSvg;
	xge_svg_draw_item_t* pItem;
	float fTolerance;
	int bScreenSpace;
	int iSuppressRuntimeComposition;
} xge_svg_runtime_composition_context_t;

static int __xgeSvgDrawGroupSource(void* pUser, const xge_shape_ex_matrix_t* pParentMatrix)
{
	xge_svg_group_source_context_t* pContext = (xge_svg_group_source_context_t*)pUser;
	int i;

	if ( (pContext == NULL) || (pContext->pGroup == NULL) || (pParentMatrix == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < pContext->pGroup->iItemCount; i++ ) {
		int iRet = __xgeSvgDrawItemInternal(
			pContext->pSvg, &pContext->pGroup->pItems[i], *pParentMatrix,
			pContext->fTolerance, 1
		);

		if ( iRet != XGE_OK ) return iRet;
	}
	return XGE_OK;
}

static int __xgeSvgDrawItemBlendGet(xge_svg_draw_item_t* pItem, int* pBlend, int* pBlendSet)
{
	if ( (pItem == NULL) || (pBlend == NULL) || (pBlendSet == NULL) ) {
		return 0;
	}
	if ( pItem->bRuntimeBlend ) {
		*pBlend = pItem->iRuntimeBlend;
		*pBlendSet = 1;
		return 1;
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_TEXT ) {
		*pBlend = pItem->u.tText.tStyle.iBlend;
		*pBlendSet = pItem->u.tText.tStyle.bBlendSet;
		return 1;
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE ) {
		*pBlend = pItem->u.tImage.tStyle.iBlend;
		*pBlendSet = pItem->u.tImage.tStyle.bBlendSet;
		return 1;
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE ) {
		*pBlend = pItem->u.tRaster.tStyle.iBlend;
		*pBlendSet = pItem->u.tRaster.tStyle.bBlendSet;
		return 1;
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_GROUP ) {
		*pBlend = pItem->iBlend;
		*pBlendSet = pItem->bBlendSet;
		return 1;
	}
	return 0;
}

static int __xgeSvgBlendItemDraw(void* pUser, xge_shape_ex_matrix_t tLocalParent)
{
	xge_svg_blend_item_context_t* pContext = (xge_svg_blend_item_context_t*)pUser;
	return __xgeSvgDrawItemInternalEx(
		pContext->pSvg, pContext->pItem, tLocalParent, pContext->fTolerance, 1, 1, 0
	);
}

static int __xgeSvgRuntimeCompositionSourceDraw(
	void* pUser,
	const xge_shape_ex_matrix_t* pParentMatrix
)
{
	xge_svg_runtime_composition_context_t* pContext =
		(xge_svg_runtime_composition_context_t*)pUser;

	if ( (pContext == NULL) || (pParentMatrix == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	return __xgeSvgDrawItemInternalEx(
		pContext->pSvg, pContext->pItem, *pParentMatrix,
		pContext->fTolerance, pContext->bScreenSpace, 1,
		pContext->iSuppressRuntimeComposition
	);
}

static int __xgeSvgDrawItemInternal(
	xge_svg pSvg,
	xge_svg_draw_item_t* pItem,
	xge_shape_ex_matrix_t tParent,
	float fTolerance,
	int bScreenSpace
)
{
	return __xgeSvgDrawItemInternalEx(
		pSvg, pItem, tParent, fTolerance, bScreenSpace, 0, 0
	);
}

static int __xgeSvgDrawItemInternalEx(
	xge_svg pSvg,
	xge_svg_draw_item_t* pItem,
	xge_shape_ex_matrix_t tParent,
	float fTolerance,
	int bScreenSpace,
	int bSuppressOwnBlend,
	int iSuppressRuntimeComposition
)
{
	xge_shape_ex_matrix_t tBoundsParent = tParent;
	int iRet;

	if ( !__xgeSvgValid(pSvg) || (pItem == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pItem->bRuntimeVisible && !pItem->iRuntimeVisible ) return XGE_OK;
	if ( !bSuppressOwnBlend ) {
		iRet = __xgeSvgPaintRuntimeParent(pItem, tParent, &tParent);
		if ( iRet != XGE_OK ) return iRet;
	}
	if ( !bSuppressOwnBlend ) {
		int iBlend = XGE_BLEND_ALPHA;
		int iBlendSource = XGE_SHAPE_EX_BLEND_SOURCE_SCENE;
		int bBlendSet = 0;
		float fCompositeOpacity = 1.0f;
		int bNeedsComposite = 0;

		if ( __xgeSvgDrawItemBlendGet(pItem, &iBlend, &bBlendSet) ) {
			bNeedsComposite = bBlendSet && __xgeShapeExBlendNeedsComposite(iBlend);
			if ( (pItem->iType == XGE_SVG_DRAW_ITEM_GROUP) && (pItem->u.pGroup != NULL) ) {
				fCompositeOpacity = pItem->u.pGroup->fOpacity;
				if ( fCompositeOpacity < (1.0f - XGE_SVG_EPSILON) ) {
					bNeedsComposite = 1;
				}
			}
		}
		if ( pItem->bRuntimeOpacity && (pItem->iType == XGE_SVG_DRAW_ITEM_FILTER) ) {
			fCompositeOpacity = pItem->fRuntimeOpacity;
			if ( fCompositeOpacity < (1.0f - XGE_SVG_EPSILON) ) bNeedsComposite = 1;
		}
		if ( bNeedsComposite ) {
			xge_svg_blend_item_context_t tContext;
			xge_shape_ex_matrix_t tScreenParent = __xgeSvgDrawScreenMatrix(tParent, bScreenSpace);
			xge_rect_t tBounds;
			float fMaskedOpacity;

			iRet = __xgeSvgDrawItemBoundsWithParent(
				pSvg, pItem,
				__xgeSvgDrawScreenMatrix(tBoundsParent, bScreenSpace),
				fTolerance, &tBounds, 0
			);
			if ( iRet != XGE_OK ) {
				return iRet;
			}
			tContext.pSvg = pSvg;
			tContext.pItem = pItem;
			tContext.fTolerance = fTolerance;
			if ( bBlendSet && __xgeShapeExBlendNeedsComposite(iBlend) &&
			     (pItem->iType == XGE_SVG_DRAW_ITEM_GROUP) &&
			     (pItem->u.pGroup != NULL) && pItem->u.pGroup->bMasked ) {
				iBlendSource = XGE_SHAPE_EX_BLEND_SOURCE_MASKED_GROUP;
			} else if ( bBlendSet && __xgeShapeExBlendNeedsComposite(iBlend) &&
			            __xgeSvgDrawItemMaskedOpacity(pItem, &fMaskedOpacity) ) {
				iBlendSource = XGE_SHAPE_EX_BLEND_SOURCE_MASKED;
				fCompositeOpacity = fMaskedOpacity;
			} else if ( bBlendSet && __xgeShapeExBlendNeedsComposite(iBlend) &&
			            (g_xgeShapeExMaskCompositeSourceDepth > 0) ) {
				iBlendSource = XGE_SHAPE_EX_BLEND_SOURCE_INHERITED_OPACITY;
			}
			return __xgeShapeExCompositeBlend(
				bBlendSet ? iBlend : XGE_BLEND_ALPHA,
				iBlendSource, fCompositeOpacity, tBounds,
				__xgeSvgBlendItemDraw, &tContext, NULL, NULL, tScreenParent
			);
		}
	}
	if ( pItem->bRuntimeMaskSet && (pItem->pRuntimeMaskOwnerScene != NULL) &&
	     ((iSuppressRuntimeComposition & XGE_SVG_RUNTIME_COMPOSITION_MASK) == 0) ) {
		xge_svg_runtime_composition_context_t tContext;
		xge_svg_draw_item_t tBoundsItem = *pItem;
		xge_rect_t tSourceBounds;

		tBoundsItem.bRuntimeTransform = 0;
		tBoundsItem.bRuntimeMaskSet = 0;
		tBoundsItem.bRuntimeClipSet = 0;
		iRet = __xgeSvgDrawItemBoundsWithParent(
			pSvg, &tBoundsItem, __xgeSvgMatrixIdentity(),
			fTolerance, &tSourceBounds, 0
		);
		if ( iRet != XGE_OK ) return iRet;
		tContext.pSvg = pSvg;
		tContext.pItem = pItem;
		tContext.fTolerance = fTolerance;
		tContext.bScreenSpace = 1;
		tContext.iSuppressRuntimeComposition =
			iSuppressRuntimeComposition | XGE_SVG_RUNTIME_COMPOSITION_MASK;
		return xgeShapeExMaskCompositeScene(
			pItem->pRuntimeMaskOwnerScene,
			pItem->iRuntimeMaskMethod,
			XGE_BLEND_ALPHA,
			tSourceBounds,
			__xgeSvgRuntimeCompositionSourceDraw,
			&tContext,
			fTolerance,
			&tParent,
			bScreenSpace
		);
	}
	if ( pItem->bRuntimeClipSet && (pItem->pRuntimeClipShape != NULL) &&
	     ((iSuppressRuntimeComposition & XGE_SVG_RUNTIME_COMPOSITION_CLIP) == 0) ) {
		xge_svg_runtime_composition_context_t tContext;
		int bApplied = 0;

		iRet = bScreenSpace ?
			xgeShapeExStencilClipBeginPx(
				pItem->pRuntimeClipShape, fTolerance, &tParent, &bApplied
			) :
			xgeShapeExStencilClipBegin(
				pItem->pRuntimeClipShape, fTolerance, &tParent, &bApplied
			);
		if ( iRet != XGE_OK ) return iRet;
		tContext.pSvg = pSvg;
		tContext.pItem = pItem;
		tContext.fTolerance = fTolerance;
		tContext.bScreenSpace = bScreenSpace;
		tContext.iSuppressRuntimeComposition =
			iSuppressRuntimeComposition | XGE_SVG_RUNTIME_COMPOSITION_CLIP;
		iRet = __xgeSvgRuntimeCompositionSourceDraw(&tContext, &tParent);
		return xgeShapeExStencilClipEnd(bApplied, iRet);
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_GROUP ) {
		xge_svg_group_item_t* pGroup;
		int i;

		if ( pItem->u.pGroup == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
		pGroup = pItem->u.pGroup;
		if ( pGroup->sMaskId[0] != '\0' ) {
			xge_svg_group_source_context_t tContext;
			xge_rect_t tLocalBounds;
			int bHasBounds;

			iRet = __xgeSvgGroupChildrenBoundsWithParent(
				pSvg, pGroup, __xgeSvgMatrixIdentity(), 0.0f,
				&tLocalBounds, &bHasBounds, 1
			);
			if ( iRet != XGE_OK ) return iRet;
			iRet = __xgeSvgGroupMaskSceneEnsure(pSvg, pGroup, tLocalBounds);
			if ( iRet != XGE_OK ) return iRet;
			if ( pGroup->bMaskEmpty || !bHasBounds ) return XGE_OK;
			if ( pGroup->pMaskScene != NULL ) {
				tContext.pSvg = pSvg;
				tContext.pGroup = pGroup;
				tContext.fTolerance = fTolerance;
				return xgeShapeExMaskCompositeScene(
					pGroup->pMaskScene,
					__xgeSvgItemMaskMethod(pGroup->iMaskMethod),
					XGE_BLEND_ALPHA,
					tLocalBounds,
					__xgeSvgDrawGroupSource,
					&tContext,
					fTolerance,
					&tParent,
					bScreenSpace
				);
			}
		}
		for ( i = 0; i < pGroup->iItemCount; i++ ) {
			iRet = __xgeSvgDrawItemInternal(
				pSvg, &pGroup->pItems[i], tParent, fTolerance, bScreenSpace
			);
			if ( iRet != XGE_OK ) return iRet;
		}
		return XGE_OK;
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_FILTER ) {
		return __xgeSvgDrawFilterItem(
			pSvg, pItem->u.pFilter, tParent, fTolerance, bScreenSpace
		);
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_SCENE ) {
		return bScreenSpace ?
			xgeShapeExSceneDrawPxEx(pItem->u.pScene, fTolerance, &tParent, 1.0f) :
			xgeShapeExSceneDrawEx(pItem->u.pScene, fTolerance, &tParent, 1.0f);
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_SHAPE ) {
		int iOldBlend;
		int bBlendApplied;

		iRet = __xgeSvgDrawBlendBegin(pItem->bBlendSet, pItem->iBlend, &iOldBlend, &bBlendApplied);
		if ( iRet == XGE_OK ) {
			iRet = bScreenSpace ?
				xgeShapeExDrawPxEx(pItem->u.pShape, fTolerance, &tParent, 1.0f) :
				xgeShapeExDrawEx(pItem->u.pShape, fTolerance, &tParent, 1.0f);
			iRet = __xgeSvgDrawBlendEnd(iOldBlend, bBlendApplied, iRet);
		}
		return iRet;
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_TEXT ) {
		xge_svg_text_item_t* pText = &pItem->u.tText;
		xge_rect_t tOldClip;
		xge_shape_ex_matrix_t tItemMatrix;
		int bOldClipEnabled;
		int bClipApplied;
		int iOldBlend;
		int bBlendApplied;

		iRet = __xgeShapeAutoBatchFlush();
		if ( iRet != XGE_OK ) return iRet;
		tItemMatrix = __xgeSvgMatrixMul(tParent, pText->tStyle.tTransform);
		if ( pText->bClipRect ) {
			xge_rect_t tClip = __xgeSvgMatrixRectBounds(tItemMatrix, pText->tClipRect);

			iRet = __xgeSvgDrawClipBegin(tClip, &tOldClip, &bOldClipEnabled, &bClipApplied);
			if ( iRet != XGE_OK ) return iRet;
		} else {
			memset(&tOldClip, 0, sizeof(tOldClip));
			bOldClipEnabled = 0;
			bClipApplied = 0;
		}
		if ( pText->pMaskScene != NULL ) {
			xge_svg_mask_text_draw_context_t tContext;
			xge_rect_t tSourceBounds;

			iRet = __xgeSvgTextLineBounds(pSvg, pText, &tSourceBounds);
			if ( iRet == XGE_OK ) {
				tContext.pSvg = pSvg;
				tContext.pText = pText;
				tContext.fTolerance = fTolerance;
				iRet = xgeShapeExMaskCompositeScene(
					pText->pMaskScene,
					__xgeSvgItemMaskMethod(pText->iMaskMethod),
					__xgeSvgMaskOutputBlend(pText->tStyle.bBlendSet, pText->tStyle.iBlend, bSuppressOwnBlend),
					tSourceBounds,
					__xgeSvgDrawMaskedTextSource,
					&tContext,
					fTolerance,
					&tItemMatrix,
					bScreenSpace
				);
			}
		} else {
			iRet = __xgeSvgDrawBlendBegin(
				bSuppressOwnBlend ? 0 : pText->tStyle.bBlendSet,
				pText->tStyle.iBlend,
				&iOldBlend,
				&bBlendApplied
			);
			if ( iRet == XGE_OK ) {
				iRet = __xgeSvgDrawTextItem(pSvg, pText, tParent, fTolerance, bScreenSpace);
				iRet = __xgeSvgDrawBlendEnd(iOldBlend, bBlendApplied, iRet);
			}
		}
		return __xgeSvgDrawClipEnd(tOldClip, bOldClipEnabled, bClipApplied, iRet);
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_SVG_IMAGE ) {
		xge_svg_image_item_t* pImage = &pItem->u.tImage;
		xge_rect_t tOldClip;
		xge_shape_ex_matrix_t tItemMatrix;
		int bOldClipEnabled;
		int bClipApplied;
		int iOldBlend;
		int bBlendApplied;

		iRet = __xgeShapeAutoBatchFlush();
		if ( iRet != XGE_OK ) return iRet;
		tItemMatrix = __xgeSvgMatrixMul(tParent, pImage->tStyle.tTransform);
		if ( pImage->bClipRect ) {
			xge_rect_t tClip = __xgeSvgMatrixRectBounds(tItemMatrix, pImage->tClipRect);

			iRet = __xgeSvgDrawClipBegin(tClip, &tOldClip, &bOldClipEnabled, &bClipApplied);
			if ( iRet != XGE_OK ) return iRet;
		} else {
			memset(&tOldClip, 0, sizeof(tOldClip));
			bOldClipEnabled = 0;
			bClipApplied = 0;
		}
		if ( pImage->pMaskScene != NULL ) {
			xge_svg_mask_image_draw_context_t tContext;

			tContext.pImage = pImage;
			tContext.fTolerance = fTolerance;
			iRet = xgeShapeExMaskCompositeScene(
				pImage->pMaskScene,
				__xgeSvgItemMaskMethod(pImage->iMaskMethod),
				__xgeSvgMaskOutputBlend(pImage->tStyle.bBlendSet, pImage->tStyle.iBlend, bSuppressOwnBlend),
				pImage->tRect,
				__xgeSvgDrawMaskedImageSource,
				&tContext,
				fTolerance,
				&tItemMatrix,
				bScreenSpace
			);
		} else {
			iRet = __xgeSvgDrawBlendBegin(
				bSuppressOwnBlend ? 0 : pImage->tStyle.bBlendSet,
				pImage->tStyle.iBlend,
				&iOldBlend,
				&bBlendApplied
			);
			if ( iRet == XGE_OK ) {
				iRet = __xgeSvgDrawImageItem(pImage, tParent, fTolerance, bScreenSpace);
				iRet = __xgeSvgDrawBlendEnd(iOldBlend, bBlendApplied, iRet);
			}
		}
		return __xgeSvgDrawClipEnd(tOldClip, bOldClipEnabled, bClipApplied, iRet);
	}
	if ( pItem->iType == XGE_SVG_DRAW_ITEM_RASTER_IMAGE ) {
		xge_svg_raster_item_t* pImage = &pItem->u.tRaster;
		xge_rect_t tOldClip;
		xge_shape_ex_matrix_t tItemMatrix;
		int bOldClipEnabled;
		int bClipApplied;
		int iOldBlend;
		int bBlendApplied;

		iRet = __xgeShapeAutoBatchFlush();
		if ( iRet != XGE_OK ) return iRet;
		tItemMatrix = __xgeSvgMatrixMul(tParent, pImage->tStyle.tTransform);
		if ( pImage->bClipRect ) {
			xge_rect_t tClip = __xgeSvgMatrixRectBounds(tItemMatrix, pImage->tClipRect);

			iRet = __xgeSvgDrawClipBegin(tClip, &tOldClip, &bOldClipEnabled, &bClipApplied);
			if ( iRet != XGE_OK ) return iRet;
		} else {
			memset(&tOldClip, 0, sizeof(tOldClip));
			bOldClipEnabled = 0;
			bClipApplied = 0;
		}
		if ( pImage->pMaskScene != NULL ) {
			xge_svg_mask_raster_draw_context_t tContext;
			xge_rect_t tSourceBounds;

			tSourceBounds = pImage->tRect;
			if ( (pImage->pRaster != NULL) &&
			     (pImage->pRaster->tImage.iWidth > 0) && (pImage->pRaster->tImage.iHeight > 0) ) {
				xge_rect_t tImageBox;

				tImageBox.fX = 0.0f;
				tImageBox.fY = 0.0f;
				tImageBox.fW = (float)pImage->pRaster->tImage.iWidth;
				tImageBox.fH = (float)pImage->pRaster->tImage.iHeight;
				tSourceBounds = __xgeSvgAspectViewport(
					tImageBox,
					pImage->tRect,
					pImage->iAspectAlignX,
					pImage->iAspectAlignY,
					pImage->iAspectMeetOrSlice
				);
			}
			tContext.pImage = pImage;
			iRet = xgeShapeExMaskCompositeScene(
				pImage->pMaskScene,
				__xgeSvgItemMaskMethod(pImage->iMaskMethod),
				__xgeSvgMaskOutputBlend(pImage->tStyle.bBlendSet, pImage->tStyle.iBlend, bSuppressOwnBlend),
				tSourceBounds,
				__xgeSvgDrawMaskedRasterSource,
				&tContext,
				fTolerance,
				&tItemMatrix,
				bScreenSpace
			);
		} else {
			iRet = __xgeSvgDrawBlendBegin(
				bSuppressOwnBlend ? 0 : pImage->tStyle.bBlendSet,
				pImage->tStyle.iBlend,
				&iOldBlend,
				&bBlendApplied
			);
			if ( iRet == XGE_OK ) {
				iRet = __xgeSvgDrawRasterImageItem(pImage, tParent, bScreenSpace);
				iRet = __xgeSvgDrawBlendEnd(iOldBlend, bBlendApplied, iRet);
			}
		}
		return __xgeSvgDrawClipEnd(tOldClip, bOldClipEnabled, bClipApplied, iRet);
	}
	return XGE_OK;
}

typedef struct xge_svg_picture_blend_context_t {
	xge_svg pSvg;
	xge_rect_t tDst;
	float fTolerance;
} xge_svg_picture_blend_context_t;

typedef struct xge_svg_hierarchy_composition_context_t {
	xge_svg pSvg;
	float fTolerance;
	int bScreenSpace;
	unsigned int iSuppressed;
} xge_svg_hierarchy_composition_context_t;

static int __xgeSvgDrawHierarchyComposition(
	xge_svg pSvg,
	xge_shape_ex_matrix_t tParent,
	float fTolerance,
	int bScreenSpace,
	unsigned int iSuppressed
);

static int __xgeSvgDrawHierarchyCompositionSource(
	void* pUser,
	const xge_shape_ex_matrix_t* pParentMatrix
)
{
	xge_svg_hierarchy_composition_context_t* pContext =
		(xge_svg_hierarchy_composition_context_t*)pUser;

	if ( (pContext == NULL) || (pParentMatrix == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	return __xgeSvgDrawHierarchyComposition(
		pContext->pSvg, *pParentMatrix, pContext->fTolerance,
		pContext->bScreenSpace, pContext->iSuppressed
	);
}

static int __xgeSvgDrawHierarchyComposition(
	xge_svg pSvg,
	xge_shape_ex_matrix_t tParent,
	float fTolerance,
	int bScreenSpace,
	unsigned int iSuppressed
)
{
	xge_svg_draw_item_t* arrPaints[4];
	int i;
	int iRet;

	if ( !__xgeSvgValid(pSvg) ) return XGE_ERROR_INVALID_ARGUMENT;
	arrPaints[0] = &pSvg->tPicturePaint;
	arrPaints[1] = &pSvg->tRootPaint;
	arrPaints[2] = &pSvg->tContentPaint;
	arrPaints[3] = &pSvg->tDocumentPaint;
	for ( i = 0; i < 4; i++ ) {
		xge_svg_draw_item_t* pPaint = arrPaints[i];
		unsigned int iMaskBit = XGE_SVG_HIERARCHY_COMPOSITION_MASK(i);
		unsigned int iClipBit = XGE_SVG_HIERARCHY_COMPOSITION_CLIP(i);

		if ( pPaint->bRuntimeMaskSet && (pPaint->pRuntimeMaskOwnerScene != NULL) &&
		     ((iSuppressed & iMaskBit) == 0) ) {
			xge_svg_hierarchy_composition_context_t tContext;
			xge_rect_t tSourceBounds;

			iRet = __xgeSvgGetBoundsItemsWithParent(
				pSvg, __xgeSvgMatrixIdentity(), fTolerance, &tSourceBounds, 0
			);
			if ( iRet != XGE_OK ) return iRet;
			tContext.pSvg = pSvg;
			tContext.fTolerance = fTolerance;
			tContext.bScreenSpace = 1;
			tContext.iSuppressed = iSuppressed | iMaskBit;
			return xgeShapeExMaskCompositeScene(
				pPaint->pRuntimeMaskOwnerScene,
				pPaint->iRuntimeMaskMethod,
				XGE_BLEND_ALPHA,
				tSourceBounds,
				__xgeSvgDrawHierarchyCompositionSource,
				&tContext,
				fTolerance,
				&tParent,
				bScreenSpace
			);
		}
		if ( pPaint->bRuntimeClipSet && (pPaint->pRuntimeClipShape != NULL) &&
		     ((iSuppressed & iClipBit) == 0) ) {
			int bApplied = 0;

			iRet = bScreenSpace ?
				xgeShapeExStencilClipBeginPx(
					pPaint->pRuntimeClipShape, fTolerance, &tParent, &bApplied
				) :
				xgeShapeExStencilClipBegin(
					pPaint->pRuntimeClipShape, fTolerance, &tParent, &bApplied
				);
			if ( iRet != XGE_OK ) return iRet;
			iRet = __xgeSvgDrawHierarchyComposition(
				pSvg, tParent, fTolerance, bScreenSpace, iSuppressed | iClipBit
			);
			return xgeShapeExStencilClipEnd(bApplied, iRet);
		}
	}
	for ( i = 0; i < pSvg->iItemCount; i++ ) {
		iRet = __xgeSvgDrawItemInternal(
			pSvg, &pSvg->pItems[i], tParent, fTolerance, bScreenSpace
		);
		if ( iRet != XGE_OK ) return iRet;
	}
	return __xgeShapeAutoBatchFlush();
}

static int __xgeSvgDrawPictureBlend(void* pUser, xge_shape_ex_matrix_t tLocalParent)
{
	xge_svg_picture_blend_context_t* pContext =
		(xge_svg_picture_blend_context_t*)pUser;

	if ( pContext == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	return __xgeSvgDrawWithParentEx(
		pContext->pSvg, pContext->tDst, tLocalParent,
		pContext->fTolerance, 1, 1
	);
}

static int __xgeSvgDrawWithParentEx(xge_svg pSvg, xge_rect_t tDst, xge_shape_ex_matrix_t tParent, float fTolerance, int bScreenSpace, int bSuppressRuntimeComposite)
{
	xge_shape_ex_matrix_t tBoundsParent = tParent;
	xge_shape_ex_matrix_t tLocalMatrix;
	xge_shape_ex_matrix_t tMatrix;
	xge_shape_ex_matrix_t tScreenParent;
	xge_rect_t tClip;
	xge_rect_t tViewport;
	xge_rect_t tViewBox;
	xge_rect_t tOldRootClip;
	int bOldRootClipEnabled;
	int bRootClipApplied;
	int iRet;

	if ( !__xgeSvgValid(pSvg) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( !bSuppressRuntimeComposite ) {
		float fOpacity;
		int bVisible;
		int iBlend;
		int bBlendSet;

		__xgeSvgPaintHierarchyRuntimeState(
			pSvg, &fOpacity, &bVisible, &iBlend, &bBlendSet
		);
		if ( !bVisible || (fOpacity <= XGE_SVG_EPSILON) ) return XGE_OK;
		iRet = __xgeSvgPaintHierarchyRuntimeParent(pSvg, tParent, &tParent);
		if ( iRet != XGE_OK ) return iRet;
		if ( (fOpacity < (1.0f - XGE_SVG_EPSILON)) ||
		     (bBlendSet && __xgeShapeExBlendNeedsComposite(iBlend)) ) {
			xge_svg_picture_blend_context_t tContext;
			xge_shape_ex_matrix_t tScreenParent;
			xge_rect_t tBounds;

			iRet = __xgeSvgGetDrawBoundsWithParentInternal(
				pSvg, tDst, tBoundsParent, fTolerance, &tBounds, 0
			);
			if ( iRet != XGE_OK ) return iRet;
			tContext.pSvg = pSvg;
			tContext.tDst = tDst;
			tContext.fTolerance = fTolerance;
			tScreenParent = __xgeSvgDrawScreenMatrix(tParent, bScreenSpace);
			return __xgeShapeExCompositeBlend(
				bBlendSet ? iBlend : XGE_BLEND_ALPHA,
				XGE_SHAPE_EX_BLEND_SOURCE_SCENE,
				fOpacity, tBounds, __xgeSvgDrawPictureBlend, &tContext,
				NULL, NULL, tScreenParent
			);
		}
	} else {
		/* The composite callback already supplies the hierarchy-adjusted parent. */
	}
	tViewBox = pSvg->tViewBox;
	if ( (tViewBox.fW <= 0.0f) || (tViewBox.fH <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = xgeSvgGetDrawViewport(pSvg, tDst, &tViewport);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	tLocalMatrix = __xgeSvgViewBoxMatrix(tViewBox, tViewport);
	tMatrix = __xgeSvgMatrixMul(tParent, tLocalMatrix);
	tScreenParent = __xgeSvgDrawScreenMatrix(tParent, bScreenSpace);
	tClip = __xgeSvgMatrixRectBounds(tScreenParent, tDst);
	iRet = __xgeSvgDrawClipBegin(tClip, &tOldRootClip, &bOldRootClipEnabled, &bRootClipApplied);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = __xgeSvgDrawHierarchyComposition(
		pSvg, tMatrix, fTolerance, bScreenSpace, 0u
	);
	return __xgeSvgDrawClipEnd(tOldRootClip, bOldRootClipEnabled, bRootClipApplied, iRet);
}

static int __xgeSvgDrawWithParent(xge_svg pSvg, xge_rect_t tDst, xge_shape_ex_matrix_t tParent, float fTolerance, int bScreenSpace)
{
	return __xgeSvgDrawWithParentEx(
		pSvg, tDst, tParent, fTolerance, bScreenSpace, 0
	);
}

static int __xgeSvgDrawInternal(xge_svg pSvg, xge_rect_t tDst, float fTolerance, int bScreenSpace)
{
	return __xgeSvgDrawWithParent(pSvg, tDst, __xgeSvgMatrixIdentity(), fTolerance, bScreenSpace);
}

int xgeSvgGetBounds(xge_svg pSvg, float fTolerance, xge_rect_t* pBounds)
{
	if ( !__xgeSvgValid(pSvg) || (pBounds == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	return __xgeSvgGetBoundsWithParent(pSvg, __xgeSvgMatrixIdentity(), fTolerance, pBounds, 0);
}

int xgeSvgGetDrawBounds(xge_svg pSvg, xge_rect_t tDst, float fTolerance, xge_rect_t* pBounds)
{
	return __xgeSvgGetDrawBoundsInternal(pSvg, tDst, fTolerance, pBounds, 0);
}

int xgeSvgContainsPoint(xge_svg pSvg, float fX, float fY, float fTolerance, int* pContains)
{
	xge_vec2_t tPoint;

	if ( !__xgeSvgValid(pSvg) || (pContains == NULL) || !__xgeSvgFloatFinite(fX) || !__xgeSvgFloatFinite(fY) || !__xgeSvgFloatFinite(fTolerance) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tPoint.fX = fX;
	tPoint.fY = fY;
	return __xgeSvgContainsPointWithParent(pSvg, __xgeSvgMatrixIdentity(), tPoint, fTolerance, pContains, 0);
}

int xgeSvgDrawContainsPoint(xge_svg pSvg, xge_rect_t tDst, float fX, float fY, float fTolerance, int* pContains)
{
	xge_vec2_t tPoint;

	if ( !__xgeSvgValid(pSvg) || (pContains == NULL) || !__xgeSvgFloatFinite(fX) || !__xgeSvgFloatFinite(fY) || !__xgeSvgFloatFinite(fTolerance) ||
	     !__xgeSvgFloatFinite(tDst.fX) || !__xgeSvgFloatFinite(tDst.fY) || !__xgeSvgFloatFinite(tDst.fW) || !__xgeSvgFloatFinite(tDst.fH) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pContains = 0;
	tPoint.fX = fX;
	tPoint.fY = fY;
	return __xgeSvgDrawContainsPointInternal(pSvg, tDst, tPoint, fTolerance, pContains, 0);
}

int xgeSvgDraw(xge_svg pSvg, xge_rect_t tDst, float fTolerance)
{
	return __xgeSvgDrawInternal(pSvg, tDst, fTolerance, 0);
}

int xgeSvgDrawPx(xge_svg pSvg, xge_rect_t tDst, float fTolerance)
{
	return __xgeSvgDrawInternal(pSvg, tDst, fTolerance, 1);
}

int xgeSvgRasterize(const char* sURI, int iWidth, int iHeight, void* pPixels, int iStride)
{
	xge_resource_t tResource;
	char* sBaseDir;
	int iRet;

	if ( (sURI == NULL) || (iWidth <= 0) || (iHeight <= 0) || (pPixels == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	sBaseDir = __xgeSvgUriBaseDir(sURI);
	if ( sBaseDir == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iRet = xgeResourceLoad(sURI, &tResource);
	if ( iRet != XGE_OK ) {
		xrtFree(sBaseDir);
		return iRet;
	}
	iRet = __xgeSvgRasterizeMemoryEx(tResource.pData, tResource.iSize, iWidth, iHeight, pPixels, iStride, sBaseDir);
	xgeResourceFree(&tResource);
	xrtFree(sBaseDir);
	return iRet;
}

static int __xgeSvgRasterizeMemoryEx(const void* pData, int iSize, int iWidth, int iHeight, void* pPixels, int iStride, const char* sBaseDir)
{
	xge_svg pSvg;
	xge_render_target_t tTarget;
	xge_pass_t tPass;
	xge_rect_t tDst;
	int bStarted;
	int iRet;

	if ( (pData == NULL) || (iSize <= 0) || (iWidth <= 0) || (iHeight <= 0) || (pPixels == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iWidth > (INT32_MAX / 4) || iHeight > (INT32_MAX / (iWidth * 4)) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( iStride <= 0 ) {
		iStride = iWidth * 4;
	}
	if ( iStride < (iWidth * 4) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( g_xge.bSokolRunning == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}

	pSvg = NULL;
	memset(&tTarget, 0, sizeof(tTarget));
	memset(&tPass, 0, sizeof(tPass));
	bStarted = 0;
	iRet = xgeSvgCreate(&pSvg);
	if ( iRet == XGE_OK ) {
		iRet = __xgeSvgLoadMemoryEx(pSvg, pData, iSize, sBaseDir);
	}
	if ( iRet == XGE_OK ) {
		/* ThorVG Picture::size() maps the picture to the requested raster size. */
		iRet = xgeSvgSetPreserveAspectRatio(pSvg, "none");
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeRenderTargetCreate(&tTarget, iWidth, iHeight);
	}
	if ( iRet == XGE_OK ) {
		if ( g_xge.bRenderActive == 0 ) {
			iRet = xgeBegin();
			if ( iRet == XGE_OK ) {
				bStarted = 1;
			}
		}
	}
	if ( iRet == XGE_OK ) {
		xgePassInit(&tPass, &tTarget, XGE_PASS_CLEAR_COLOR, XGE_COLOR_RGBA(0, 0, 0, 0));
		iRet = xgePassBegin(&tPass);
	}
	if ( iRet == XGE_OK ) {
		tDst.fX = 0.0f;
		tDst.fY = 0.0f;
		tDst.fW = (float)iWidth;
		tDst.fH = (float)iHeight;
		iRet = xgeSvgDrawPx(pSvg, tDst, 0.5f);
	}
	if ( tPass.bActive ) {
		int iEndRet = xgePassEnd(&tPass);
		if ( iRet == XGE_OK ) {
			iRet = iEndRet;
		}
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeRenderTargetReadPixels(&tTarget, pPixels, iStride);
	}
	if ( bStarted != 0 ) {
		int iEndRet = xgeEnd();
		if ( iRet == XGE_OK ) {
			iRet = iEndRet;
		}
	}
	xgeRenderTargetFree(&tTarget);
	xgeSvgDestroy(pSvg);
	return iRet;
}

int xgeSvgRasterizeMemory(const void* pData, int iSize, int iWidth, int iHeight, void* pPixels, int iStride)
{
	return __xgeSvgRasterizeMemoryEx(pData, iSize, iWidth, iHeight, pPixels, iStride, NULL);
}

int xgeSvgTextureLoad(xge_texture pTexture, const char* sURI, int iWidth, int iHeight)
{
	xge_resource_t tResource;
	char* sBaseDir;
	int iRet;

	if ( (pTexture == NULL) || (sURI == NULL) || (iWidth <= 0) || (iHeight <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	sBaseDir = __xgeSvgUriBaseDir(sURI);
	if ( sBaseDir == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iRet = xgeResourceLoad(sURI, &tResource);
	if ( iRet != XGE_OK ) {
		xrtFree(sBaseDir);
		return iRet;
	}
	iRet = __xgeSvgTextureLoadMemoryEx(pTexture, tResource.pData, tResource.iSize, iWidth, iHeight, sBaseDir);
	xgeResourceFree(&tResource);
	xrtFree(sBaseDir);
	return iRet;
}

static int __xgeSvgTextureLoadMemoryEx(xge_texture pTexture, const void* pData, int iSize, int iWidth, int iHeight, const char* sBaseDir)
{
	unsigned char* pPixels;
	int iStride;
	int iRet;

	if ( (pTexture == NULL) || (pData == NULL) || (iSize <= 0) || (iWidth <= 0) || (iHeight <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iWidth > (INT32_MAX / 4) || iHeight > (INT32_MAX / (iWidth * 4)) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iStride = iWidth * 4;
	pPixels = (unsigned char*)xrtMalloc((size_t)iStride * (size_t)iHeight);
	if ( pPixels == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iRet = __xgeSvgRasterizeMemoryEx(pData, iSize, iWidth, iHeight, pPixels, iStride, sBaseDir);
	if ( iRet == XGE_OK ) {
		iRet = xgeTextureCreateRGBA(pTexture, iWidth, iHeight, pPixels);
	}
	xrtFree(pPixels);
	return iRet;
}

int xgeSvgTextureLoadMemory(xge_texture pTexture, const void* pData, int iSize, int iWidth, int iHeight)
{
	return __xgeSvgTextureLoadMemoryEx(pTexture, pData, iSize, iWidth, iHeight, NULL);
}
