#include <float.h>
#include <limits.h>
#include <stdarg.h>

#define XGE_SHAPE_EX_MAGIC 0x58475358u
#define XGE_SHAPE_EX_PATH_MEASURE_MAGIC 0x5847504Du
#define XGE_SHAPE_EX_SCENE_MAGIC 0x58475343u
#define XGE_SHAPE_EX_DEFAULT_TOLERANCE 0.35f
#define XGE_SHAPE_EX_FLATTEN_MAX_DEPTH 12
#define XGE_SHAPE_EX_STROKE_MIN_TOLERANCE 0.5f
#define XGE_SHAPE_EX_KAPPA 0.552284f
#define XGE_SHAPE_EX_PI 3.14159265358979323846f
#define XGE_SHAPE_EX_TAU 6.28318530717958647692f
#define XGE_SHAPE_EX_EPSILON 0.00001f
#define XGE_SHAPE_EX_EXPLICIT_JOIN_ANGLE (XGE_SHAPE_EX_PI / 3.0f)
#define XGE_SHAPE_EX_CLIP_BOUNDS_MAX_DEPTH 32
#define XGE_SHAPE_EX_FILL_SOLID 0
#define XGE_SHAPE_EX_FILL_LINEAR_GRADIENT 1
#define XGE_SHAPE_EX_FILL_RADIAL_GRADIENT 2
#define XGE_SHAPE_EX_PAINT_FILL 1
#define XGE_SHAPE_EX_PAINT_STROKE 2
#define XGE_SHAPE_EX_GRADIENT_TABLE_SIZE 1024
#define XGE_SHAPE_EX_AA_HALF_WIDTH 0.5f
#define XGE_SHAPE_EX_HALF_OPEN_THRESHOLD (1.0f / 256.0f)
#define XGE_SHAPE_EX_HALF_OPEN_BIAS (1.0f / 128.0f)
#define XGE_SHAPE_EX_STROKE_UNION_DOT -0.98f

typedef struct xge_shape_ex_transform_state_t {
	float fTranslateX;
	float fTranslateY;
	float fScaleX;
	float fScaleY;
	float fRotateRadians;
	float fSkewXRadians;
	float fSkewYRadians;
	int bOverride;
} xge_shape_ex_transform_state_t;

struct xge_shape_ex_t {
	uint32_t iMagic;
	int iRefCount;
	uint32_t iId;
	xge_shape_ex_scene pParentScene;
	uint8_t* pCommands;
	int iCommandCount;
	int iCommandCapacity;
	xge_vec2_t* pPoints;
	int iPointCount;
	int iPointCapacity;
	xge_vec2_t tCurrent;
	xge_vec2_t tStart;
	xge_vec2_t tLastCubicControl;
	xge_vec2_t tLastQuadControl;
	int bHasCurrent;
	int bHasStart;
	int iFillType;
	uint32_t iFillColor;
	int iStrokeType;
	uint32_t iStrokeColor;
	int bStrokeStateCreated;
	float fStrokeWidth;
	float fFillX1;
	float fFillY1;
	float fFillX2;
	float fFillY2;
	float fFillR;
	float fFillFX;
	float fFillFY;
	float fFillFR;
	int iFillGradientUnits;
	int iFillGradientSpread;
	xge_shape_ex_matrix_t tFillGradientTransform;
	xge_shape_ex_color_stop_t* pFillStops;
	xge_shape_ex_color_stop_t* pFillSourceStops;
	int iFillStopCount;
	int iFillStopCapacity;
	int iFillSourceStopCapacity;
	float fStrokeX1;
	float fStrokeY1;
	float fStrokeX2;
	float fStrokeY2;
	float fStrokeR;
	float fStrokeFX;
	float fStrokeFY;
	float fStrokeFR;
	int iStrokeGradientUnits;
	int iStrokeGradientSpread;
	xge_shape_ex_matrix_t tStrokeGradientTransform;
	xge_shape_ex_color_stop_t* pStrokeStops;
	xge_shape_ex_color_stop_t* pStrokeSourceStops;
	int iStrokeStopCount;
	int iStrokeStopCapacity;
	int iStrokeSourceStopCapacity;
	int iFillRule;
	int iLineJoin;
	int iLineCap;
	float fMiterLimit;
	int bStrokeNonScaling;
	float* pDashPattern;
	int iDashCount;
	float fDashOffset;
	int bTrimPath;
	float fTrimBegin;
	float fTrimEnd;
	int bTrimSimultaneous;
	int bStrokeFirst;
	int bStencilPaint;
	float fOpacity;
	int bVisible;
	int iBlend;
	int bBlendSet;
	int bCoverageAntialiasDisabled;
	int bAxisAlignedFillAntialias;
	int bStrokeUnionMaskDraw;
	int iMaskMethod;
	int iMaskTargetType;
	xge_shape_ex pMaskShape;
	xge_shape_ex_scene pMaskScene;
	int bClipRect;
	xge_rect_t tClipRect;
	xge_shape_ex* pClipShapes;
	int* pClipShapeModes;
	int iClipShapeCount;
	int iClipShapeCapacity;
	xge_shape_ex_matrix_t tTransform;
	xge_shape_ex_transform_state_t tTransformState;
};

struct xge_shape_ex_scene_t {
	uint32_t iMagic;
	int iRefCount;
	uint32_t iId;
	xge_shape_ex_scene pParentScene;
	xge_shape_ex_scene_child_t* pChildren;
	int iChildCount;
	int iChildCapacity;
	float fOpacity;
	int bVisible;
	int iBlend;
	int bBlendSet;
	int iMaskMethod;
	int iMaskTargetType;
	xge_shape_ex pMaskShape;
	xge_shape_ex_scene pMaskScene;
	int bClipRect;
	xge_rect_t tClipRect;
	xge_shape_ex* pClipShapes;
	int* pClipShapeModes;
	int iClipShapeCount;
	int iClipShapeCapacity;
	xge_shape_ex_scene_effect_t* pEffects;
	int iEffectCount;
	int iEffectCapacity;
	xge_shape_ex_matrix_t tTransform;
	xge_shape_ex_transform_state_t tTransformState;
};

typedef struct xge_shape_ex_flat_contour_t {
	int iStart;
	int iCount;
	int bClosed;
} xge_shape_ex_flat_contour_t;

typedef struct xge_shape_ex_flat_path_t {
	xge_vec2_t* pPoints;
	int iPointCount;
	int iPointCapacity;
	xge_shape_ex_flat_contour_t* pContours;
	int iContourCount;
	int iContourCapacity;
} xge_shape_ex_flat_path_t;

struct xge_shape_ex_path_measure_t {
	uint32_t iMagic;
	xge_shape_ex_flat_path_t tFlat;
	float fLength;
};

#define XGE_SHAPE_EX_BLEND_TARGET_CACHE_MAX 8

typedef struct xge_shape_ex_blend_target_set_t {
	xge_render_target_t tSource;
	xge_render_target_t tDestination;
	xge_render_target_t tCoverage;
	int bSourceValid;
	int bDestinationValid;
	int bCoverageValid;
} xge_shape_ex_blend_target_set_t;

typedef struct xge_shape_ex_blend_renderer_t {
	xge_shader_t tShader;
	xge_shape_ex_blend_target_set_t arrTargets[XGE_SHAPE_EX_BLEND_TARGET_CACHE_MAX];
	int bInitialized;
} xge_shape_ex_blend_renderer_t;

#define XGE_SHAPE_EX_MASK_TARGET_CACHE_MAX 8

typedef struct xge_shape_ex_mask_target_set_t {
	xge_render_target_t tSource;
	xge_render_target_t tMask;
	int bSourceValid;
	int bMaskValid;
} xge_shape_ex_mask_target_set_t;

typedef struct xge_shape_ex_mask_renderer_t {
	xge_shader_t tShader;
	xge_shape_ex_mask_target_set_t arrTargets[XGE_SHAPE_EX_MASK_TARGET_CACHE_MAX];
	int bInitialized;
} xge_shape_ex_mask_renderer_t;

typedef struct xge_shape_ex_gradient_renderer_t {
	xge_shader_t tShader;
	xge_texture_t tColorTable;
	xge_render_target_t tMaskTarget;
	uint64_t iColorTableHash;
	int bColorTableValid;
	int bMaskTargetValid;
	int bInitialized;
} xge_shape_ex_gradient_renderer_t;

#define XGE_SHAPE_EX_EFFECT_TARGET_CACHE_MAX 32
#define XGE_SHAPE_EX_EFFECT_TARGET_CACHE_BYTES_MAX (64u * 1024u * 1024u)

typedef struct xge_shape_ex_effect_target_set_t {
	xge_render_target_t tTargetA;
	xge_render_target_t tTargetB;
	xge_render_target_t tTargetC;
	int bTargetAValid;
	int bTargetBValid;
	int bTargetCValid;
	int bInUse;
	uint64_t iLastUse;
} xge_shape_ex_effect_target_set_t;

typedef struct xge_shape_ex_effect_renderer_t {
	xge_shader_t tBlurShader;
	xge_shader_t tColorShader;
	xge_shader_t tShadowShader;
	xge_shape_ex_effect_target_set_t arrTargets[XGE_SHAPE_EX_EFFECT_TARGET_CACHE_MAX];
	uint64_t iTargetUseSerial;
	size_t iTargetCacheBytes;
	int bInitialized;
} xge_shape_ex_effect_renderer_t;

typedef struct xge_shape_ex_stroke_union_renderer_t {
	xge_shader_t tShader;
	xge_render_target_t tMaskTarget;
	int bMaskTargetValid;
	int bInitialized;
} xge_shape_ex_stroke_union_renderer_t;

typedef struct xge_shape_ex_coverage_renderer_t {
	xge_render_target_t tTarget;
	xge_shape_vertex_t* pScaledVertices;
	int iScaledVertexCapacity;
	int bTargetValid;
} xge_shape_ex_coverage_renderer_t;

typedef int (*xge_shape_ex_blend_draw_proc)(void* pUser, xge_shape_ex_matrix_t tLocalParent);

static xge_shape_ex_blend_renderer_t g_xgeShapeExBlendRenderer;
static xge_shape_ex_mask_renderer_t g_xgeShapeExMaskRenderer;
static xge_shape_ex_gradient_renderer_t g_xgeShapeExGradientRenderer;
static xge_shape_ex_effect_renderer_t g_xgeShapeExEffectRenderer;
static xge_shape_ex_stroke_union_renderer_t g_xgeShapeExStrokeUnionRenderer;
static xge_shape_ex_coverage_renderer_t g_xgeShapeExCoverageRenderer;
static int g_xgeShapeExBlendCompositeSourceDepth;
static int g_xgeShapeExMaskCompositeSourceDepth;
static int g_xgeShapeExMaskCompositeTargetDepth;
static int g_xgeShapeExAlphaMaskTargetDepth;

static void __xgeShapeExBlendTargetSetFree(xge_shape_ex_blend_target_set_t* pTargets)
{
	if ( pTargets == NULL ) return;
	if ( pTargets->bCoverageValid ) {
		xgeRenderTargetFree(&pTargets->tCoverage);
	}
	if ( pTargets->bDestinationValid ) {
		xgeRenderTargetFree(&pTargets->tDestination);
	}
	if ( pTargets->bSourceValid ) {
		xgeRenderTargetFree(&pTargets->tSource);
	}
	memset(pTargets, 0, sizeof(*pTargets));
}

static int __xgeShapeExBlendTargetSetEnsure(
	xge_shape_ex_blend_target_set_t* pTargets,
	int iWidth,
	int iHeight,
	int bCoverage
)
{
	int bSizeMismatch;
	int iRet;

	if ( (pTargets == NULL) || (iWidth <= 0) || (iHeight <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	bSizeMismatch =
		(pTargets->bSourceValid &&
		 ((pTargets->tSource.iWidth != iWidth) || (pTargets->tSource.iHeight != iHeight))) ||
		(pTargets->bDestinationValid &&
		 ((pTargets->tDestination.iWidth != iWidth) || (pTargets->tDestination.iHeight != iHeight))) ||
		(pTargets->bCoverageValid &&
		 ((pTargets->tCoverage.iWidth != iWidth) || (pTargets->tCoverage.iHeight != iHeight)));
	if ( bSizeMismatch ) {
		__xgeShapeExBlendTargetSetFree(pTargets);
	}
	iRet = XGE_OK;
	if ( !pTargets->bSourceValid ) {
		iRet = xgeRenderTargetCreate(&pTargets->tSource, iWidth, iHeight);
		if ( iRet == XGE_OK ) pTargets->bSourceValid = 1;
	}
	if ( (iRet == XGE_OK) && !pTargets->bDestinationValid ) {
		iRet = xgeRenderTargetCreate(&pTargets->tDestination, iWidth, iHeight);
		if ( iRet == XGE_OK ) pTargets->bDestinationValid = 1;
	}
	if ( (iRet == XGE_OK) && bCoverage && !pTargets->bCoverageValid ) {
		iRet = xgeRenderTargetCreate(&pTargets->tCoverage, iWidth, iHeight);
		if ( iRet == XGE_OK ) pTargets->bCoverageValid = 1;
	}
	return iRet;
}

static void __xgeShapeExMaskTargetSetFree(xge_shape_ex_mask_target_set_t* pTargets)
{
	if ( pTargets == NULL ) return;
	if ( pTargets->bMaskValid ) {
		xgeRenderTargetFree(&pTargets->tMask);
	}
	if ( pTargets->bSourceValid ) {
		xgeRenderTargetFree(&pTargets->tSource);
	}
	memset(pTargets, 0, sizeof(*pTargets));
}

static int __xgeShapeExMaskTargetSetEnsure(
	xge_shape_ex_mask_target_set_t* pTargets,
	int iWidth,
	int iHeight
)
{
	int bSizeMismatch;
	int iRet;

	if ( (pTargets == NULL) || (iWidth <= 0) || (iHeight <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	bSizeMismatch =
		(pTargets->bSourceValid &&
		 ((pTargets->tSource.iWidth != iWidth) || (pTargets->tSource.iHeight != iHeight))) ||
		(pTargets->bMaskValid &&
		 ((pTargets->tMask.iWidth != iWidth) || (pTargets->tMask.iHeight != iHeight)));
	if ( bSizeMismatch ) {
		__xgeShapeExMaskTargetSetFree(pTargets);
	}
	iRet = XGE_OK;
	if ( !pTargets->bSourceValid ) {
		iRet = xgeRenderTargetCreate(&pTargets->tSource, iWidth, iHeight);
		if ( iRet == XGE_OK ) pTargets->bSourceValid = 1;
	}
	if ( (iRet == XGE_OK) && !pTargets->bMaskValid ) {
		iRet = xgeRenderTargetCreate(&pTargets->tMask, iWidth, iHeight);
		if ( iRet == XGE_OK ) pTargets->bMaskValid = 1;
	}
	return iRet;
}

static void __xgeShapeExEffectTargetSetFree(xge_shape_ex_effect_target_set_t* pTargets)
{
	if ( pTargets == NULL ) return;
	if ( pTargets->bTargetCValid ) {
		xgeRenderTargetFree(&pTargets->tTargetC);
	}
	if ( pTargets->bTargetBValid ) {
		xgeRenderTargetFree(&pTargets->tTargetB);
	}
	if ( pTargets->bTargetAValid ) {
		xgeRenderTargetFree(&pTargets->tTargetA);
	}
	memset(pTargets, 0, sizeof(*pTargets));
}

static size_t __xgeShapeExEffectTargetSetByteCost(
	const xge_shape_ex_effect_target_set_t* pTargets
)
{
	if ( (pTargets == NULL) || !pTargets->bTargetAValid ||
	     (pTargets->tTargetA.iWidth <= 0) || (pTargets->tTargetA.iHeight <= 0) ) {
		return 0;
	}
	return (size_t)pTargets->tTargetA.iWidth *
		(size_t)pTargets->tTargetA.iHeight * 16u;
}

static int __xgeShapeExEffectTargetSetEnsure(
	xge_shape_ex_effect_target_set_t* pTargets,
	int iWidth,
	int iHeight
)
{
	int bSizeMismatch;
	int iRet;

	if ( (pTargets == NULL) || (iWidth <= 0) || (iHeight <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	bSizeMismatch =
		(pTargets->bTargetAValid &&
		 ((pTargets->tTargetA.iWidth != iWidth) || (pTargets->tTargetA.iHeight != iHeight))) ||
		(pTargets->bTargetBValid &&
		 ((pTargets->tTargetB.iWidth != iWidth) || (pTargets->tTargetB.iHeight != iHeight))) ||
		(pTargets->bTargetCValid &&
		 ((pTargets->tTargetC.iWidth != iWidth) || (pTargets->tTargetC.iHeight != iHeight)));
	if ( bSizeMismatch ) {
		__xgeShapeExEffectTargetSetFree(pTargets);
	}
	iRet = XGE_OK;
	if ( !pTargets->bTargetAValid ) {
		iRet = xgeRenderTargetCreate(&pTargets->tTargetA, iWidth, iHeight);
		if ( iRet == XGE_OK ) pTargets->bTargetAValid = 1;
	}
	if ( (iRet == XGE_OK) && !pTargets->bTargetBValid ) {
		iRet = xgeRenderTargetCreate(&pTargets->tTargetB, iWidth, iHeight);
		if ( iRet == XGE_OK ) pTargets->bTargetBValid = 1;
	}
	if ( (iRet == XGE_OK) && !pTargets->bTargetCValid ) {
		iRet = xgeRenderTargetCreate(&pTargets->tTargetC, iWidth, iHeight);
		if ( iRet == XGE_OK ) pTargets->bTargetCValid = 1;
	}
	return iRet;
}

static void __xgeShapeExEffectTargetCacheEvict(xge_shape_ex_effect_target_set_t* pTargets)
{
	size_t iCost;

	if ( pTargets == NULL ) return;
	iCost = __xgeShapeExEffectTargetSetByteCost(pTargets);
	if ( g_xgeShapeExEffectRenderer.iTargetCacheBytes >= iCost ) {
		g_xgeShapeExEffectRenderer.iTargetCacheBytes -= iCost;
	} else {
		g_xgeShapeExEffectRenderer.iTargetCacheBytes = 0;
	}
	__xgeShapeExEffectTargetSetFree(pTargets);
}

static xge_shape_ex_effect_target_set_t* __xgeShapeExEffectTargetCacheAcquire(
	int iWidth,
	int iHeight
)
{
	xge_shape_ex_effect_target_set_t* pCandidate;
	xge_shape_ex_effect_target_set_t* pLRU;
	size_t iCost;
	int i;

	if ( (iWidth <= 0) || (iHeight <= 0) ||
	     ((size_t)iWidth > (SIZE_MAX / (size_t)iHeight / 16u)) ) {
		return NULL;
	}
	iCost = (size_t)iWidth * (size_t)iHeight * 16u;
	if ( iCost > XGE_SHAPE_EX_EFFECT_TARGET_CACHE_BYTES_MAX ) return NULL;
	g_xgeShapeExEffectRenderer.iTargetUseSerial++;
	if ( g_xgeShapeExEffectRenderer.iTargetUseSerial == 0 ) {
		for ( i = 0; i < XGE_SHAPE_EX_EFFECT_TARGET_CACHE_MAX; i++ ) {
			g_xgeShapeExEffectRenderer.arrTargets[i].iLastUse = 0;
		}
		g_xgeShapeExEffectRenderer.iTargetUseSerial = 1;
	}
	for ( i = 0; i < XGE_SHAPE_EX_EFFECT_TARGET_CACHE_MAX; i++ ) {
		xge_shape_ex_effect_target_set_t* pTargets =
			&g_xgeShapeExEffectRenderer.arrTargets[i];

		if ( !pTargets->bInUse && pTargets->bTargetAValid &&
		     (pTargets->tTargetA.iWidth == iWidth) &&
		     (pTargets->tTargetA.iHeight == iHeight) ) {
			pTargets->bInUse = 1;
			pTargets->iLastUse = g_xgeShapeExEffectRenderer.iTargetUseSerial;
			return pTargets;
		}
	}
	while ( iCost > (XGE_SHAPE_EX_EFFECT_TARGET_CACHE_BYTES_MAX -
	                  g_xgeShapeExEffectRenderer.iTargetCacheBytes) ) {
		pLRU = NULL;
		for ( i = 0; i < XGE_SHAPE_EX_EFFECT_TARGET_CACHE_MAX; i++ ) {
			xge_shape_ex_effect_target_set_t* pTargets =
				&g_xgeShapeExEffectRenderer.arrTargets[i];

			if ( pTargets->bInUse || !pTargets->bTargetAValid ) continue;
			if ( (pLRU == NULL) || (pTargets->iLastUse < pLRU->iLastUse) ) {
				pLRU = pTargets;
			}
		}
		if ( pLRU == NULL ) return NULL;
		__xgeShapeExEffectTargetCacheEvict(pLRU);
	}
	pCandidate = NULL;
	pLRU = NULL;
	for ( i = 0; i < XGE_SHAPE_EX_EFFECT_TARGET_CACHE_MAX; i++ ) {
		xge_shape_ex_effect_target_set_t* pTargets =
			&g_xgeShapeExEffectRenderer.arrTargets[i];

		if ( pTargets->bInUse ) continue;
		if ( !pTargets->bTargetAValid ) {
			pCandidate = pTargets;
			break;
		}
		if ( (pLRU == NULL) || (pTargets->iLastUse < pLRU->iLastUse) ) {
			pLRU = pTargets;
		}
	}
	if ( pCandidate == NULL ) pCandidate = pLRU;
	if ( pCandidate == NULL ) return NULL;
	if ( pCandidate->bTargetAValid ) {
		__xgeShapeExEffectTargetCacheEvict(pCandidate);
	}
	if ( __xgeShapeExEffectTargetSetEnsure(pCandidate, iWidth, iHeight) != XGE_OK ) {
		__xgeShapeExEffectTargetSetFree(pCandidate);
		return NULL;
	}
	g_xgeShapeExEffectRenderer.iTargetCacheBytes +=
		__xgeShapeExEffectTargetSetByteCost(pCandidate);
	pCandidate->bInUse = 1;
	pCandidate->iLastUse = g_xgeShapeExEffectRenderer.iTargetUseSerial;
	return pCandidate;
}

static void __xgeShapeExEffectTargetCacheRelease(
	xge_shape_ex_effect_target_set_t* pTargets
)
{
	if ( pTargets != NULL ) pTargets->bInUse = 0;
}

static void __xgeShapeExRendererUnit(void)
{
	int i;

	for ( i = 0; i < XGE_SHAPE_EX_BLEND_TARGET_CACHE_MAX; i++ ) {
		__xgeShapeExBlendTargetSetFree(&g_xgeShapeExBlendRenderer.arrTargets[i]);
	}
	for ( i = 0; i < XGE_SHAPE_EX_MASK_TARGET_CACHE_MAX; i++ ) {
		__xgeShapeExMaskTargetSetFree(&g_xgeShapeExMaskRenderer.arrTargets[i]);
	}
	for ( i = 0; i < XGE_SHAPE_EX_EFFECT_TARGET_CACHE_MAX; i++ ) {
		__xgeShapeExEffectTargetSetFree(&g_xgeShapeExEffectRenderer.arrTargets[i]);
	}
	if ( g_xgeShapeExGradientRenderer.bMaskTargetValid ) {
		xgeRenderTargetFree(&g_xgeShapeExGradientRenderer.tMaskTarget);
	}
	if ( g_xgeShapeExStrokeUnionRenderer.bMaskTargetValid ) {
		xgeRenderTargetFree(&g_xgeShapeExStrokeUnionRenderer.tMaskTarget);
	}
	if ( g_xgeShapeExCoverageRenderer.bTargetValid ) {
		xgeRenderTargetFree(&g_xgeShapeExCoverageRenderer.tTarget);
	}
	xrtFree(g_xgeShapeExCoverageRenderer.pScaledVertices);
	if ( g_xgeShapeExStrokeUnionRenderer.tShader.iRefCount > 0 ) {
		xgeShaderFree(&g_xgeShapeExStrokeUnionRenderer.tShader);
	}
	if ( g_xgeShapeExGradientRenderer.tColorTable.iRefCount > 0 ) {
		xgeTextureFree(&g_xgeShapeExGradientRenderer.tColorTable);
	}
	if ( g_xgeShapeExGradientRenderer.tShader.iRefCount > 0 ) {
		xgeShaderFree(&g_xgeShapeExGradientRenderer.tShader);
	}
	if ( g_xgeShapeExMaskRenderer.tShader.iRefCount > 0 ) {
		xgeShaderFree(&g_xgeShapeExMaskRenderer.tShader);
	}
	if ( g_xgeShapeExBlendRenderer.tShader.iRefCount > 0 ) {
		xgeShaderFree(&g_xgeShapeExBlendRenderer.tShader);
	}
	if ( g_xgeShapeExEffectRenderer.tShadowShader.iRefCount > 0 ) {
		xgeShaderFree(&g_xgeShapeExEffectRenderer.tShadowShader);
	}
	if ( g_xgeShapeExEffectRenderer.tColorShader.iRefCount > 0 ) {
		xgeShaderFree(&g_xgeShapeExEffectRenderer.tColorShader);
	}
	if ( g_xgeShapeExEffectRenderer.tBlurShader.iRefCount > 0 ) {
		xgeShaderFree(&g_xgeShapeExEffectRenderer.tBlurShader);
	}
	memset(&g_xgeShapeExGradientRenderer, 0, sizeof(g_xgeShapeExGradientRenderer));
	memset(&g_xgeShapeExMaskRenderer, 0, sizeof(g_xgeShapeExMaskRenderer));
	memset(&g_xgeShapeExBlendRenderer, 0, sizeof(g_xgeShapeExBlendRenderer));
	memset(&g_xgeShapeExEffectRenderer, 0, sizeof(g_xgeShapeExEffectRenderer));
	memset(&g_xgeShapeExStrokeUnionRenderer, 0, sizeof(g_xgeShapeExStrokeUnionRenderer));
	memset(&g_xgeShapeExCoverageRenderer, 0, sizeof(g_xgeShapeExCoverageRenderer));
	g_xgeShapeExBlendCompositeSourceDepth = 0;
	g_xgeShapeExMaskCompositeSourceDepth = 0;
	g_xgeShapeExMaskCompositeTargetDepth = 0;
	g_xgeShapeExAlphaMaskTargetDepth = 0;
}

static int __xgeShapeExFloatFinite(float fValue)
{
	return (fValue == fValue) && (fValue >= -FLT_MAX) && (fValue <= FLT_MAX);
}

static int __xgeShapeExCoordsFinite(float fX, float fY)
{
	return __xgeShapeExFloatFinite(fX) && __xgeShapeExFloatFinite(fY);
}

static int __xgeShapeExPointFinite(xge_vec2_t tPoint)
{
	return __xgeShapeExCoordsFinite(tPoint.fX, tPoint.fY);
}

static int __xgeShapeExRectFinite(xge_rect_t tRect)
{
	return __xgeShapeExCoordsFinite(tRect.fX, tRect.fY) && __xgeShapeExCoordsFinite(tRect.fW, tRect.fH);
}

static int __xgeShapeExMatrixFinite(const xge_shape_ex_matrix_t* pMatrix)
{
	return (pMatrix != NULL) &&
	       __xgeShapeExCoordsFinite(pMatrix->fA, pMatrix->fB) &&
	       __xgeShapeExCoordsFinite(pMatrix->fC, pMatrix->fD) &&
	       __xgeShapeExCoordsFinite(pMatrix->fE, pMatrix->fF);
}

static int __xgeShapeExStopsFinite(const xge_shape_ex_color_stop_t* pStops, int iStopCount)
{
	int i;

	if ( (pStops == NULL) || (iStopCount <= 0) ) {
		return 0;
	}
	for ( i = 0; i < iStopCount; i++ ) {
		if ( !__xgeShapeExFloatFinite(pStops[i].fOffset) ) {
			return 0;
		}
	}
	return 1;
}

typedef struct xge_shape_ex_path_snapshot_t {
	int iCommandCount;
	int iPointCount;
	xge_vec2_t tCurrent;
	xge_vec2_t tStart;
	xge_vec2_t tLastCubicControl;
	xge_vec2_t tLastQuadControl;
	int bHasCurrent;
	int bHasStart;
} xge_shape_ex_path_snapshot_t;

static void __xgeShapeExPathSnapshot(xge_shape_ex pShape, xge_shape_ex_path_snapshot_t* pSnapshot)
{
	if ( (pShape == NULL) || (pSnapshot == NULL) ) {
		return;
	}
	pSnapshot->iCommandCount = pShape->iCommandCount;
	pSnapshot->iPointCount = pShape->iPointCount;
	pSnapshot->tCurrent = pShape->tCurrent;
	pSnapshot->tStart = pShape->tStart;
	pSnapshot->tLastCubicControl = pShape->tLastCubicControl;
	pSnapshot->tLastQuadControl = pShape->tLastQuadControl;
	pSnapshot->bHasCurrent = pShape->bHasCurrent;
	pSnapshot->bHasStart = pShape->bHasStart;
}

static void __xgeShapeExPathRestore(xge_shape_ex pShape, const xge_shape_ex_path_snapshot_t* pSnapshot)
{
	if ( (pShape == NULL) || (pSnapshot == NULL) ) {
		return;
	}
	pShape->iCommandCount = pSnapshot->iCommandCount;
	pShape->iPointCount = pSnapshot->iPointCount;
	pShape->tCurrent = pSnapshot->tCurrent;
	pShape->tStart = pSnapshot->tStart;
	pShape->tLastCubicControl = pSnapshot->tLastCubicControl;
	pShape->tLastQuadControl = pSnapshot->tLastQuadControl;
	pShape->bHasCurrent = pSnapshot->bHasCurrent;
	pShape->bHasStart = pSnapshot->bHasStart;
}

static int __xgeShapeExPathHasSvgPrefix(xge_shape_ex pShape, const xge_shape_ex_path_snapshot_t* pSnapshot)
{
	if ( (pShape == NULL) || (pSnapshot == NULL) ) {
		return 0;
	}
	return (pShape->iCommandCount > pSnapshot->iCommandCount) &&
	       (pShape->pCommands != NULL) &&
	       (pShape->pCommands[pSnapshot->iCommandCount] == XGE_SHAPE_EX_CMD_MOVE_TO);
}

static int __xgeShapeExSvgPathFinish(xge_shape_ex pShape, const xge_shape_ex_path_snapshot_t* pSnapshot, int iRet)
{
	if ( iRet == XGE_OK ) {
		return XGE_OK;
	}
	if ( (iRet == XGE_ERROR_INVALID_ARGUMENT) && __xgeShapeExPathHasSvgPrefix(pShape, pSnapshot) ) {
		return XGE_OK;
	}
	__xgeShapeExPathRestore(pShape, pSnapshot);
	return iRet;
}

static xge_shape_ex_matrix_t __xgeShapeExMatrixIdentity(void)
{
	xge_shape_ex_matrix_t tMatrix;

	tMatrix.fA = 1.0f;
	tMatrix.fB = 0.0f;
	tMatrix.fC = 0.0f;
	tMatrix.fD = 1.0f;
	tMatrix.fE = 0.0f;
	tMatrix.fF = 0.0f;
	return tMatrix;
}

static xge_shape_ex_matrix_t __xgeShapeExMatrixMul(xge_shape_ex_matrix_t tParent, xge_shape_ex_matrix_t tLocal)
{
	xge_shape_ex_matrix_t tOut;

	tOut.fA = (tParent.fA * tLocal.fA) + (tParent.fC * tLocal.fB);
	tOut.fB = (tParent.fB * tLocal.fA) + (tParent.fD * tLocal.fB);
	tOut.fC = (tParent.fA * tLocal.fC) + (tParent.fC * tLocal.fD);
	tOut.fD = (tParent.fB * tLocal.fC) + (tParent.fD * tLocal.fD);
	tOut.fE = (tParent.fA * tLocal.fE) + (tParent.fC * tLocal.fF) + tParent.fE;
	tOut.fF = (tParent.fB * tLocal.fE) + (tParent.fD * tLocal.fF) + tParent.fF;
	return tOut;
}

static xge_vec2_t __xgeShapeExMatrixPoint(xge_shape_ex_matrix_t tMatrix, xge_vec2_t tPoint)
{
	xge_vec2_t tOut;

	tOut.fX = (tMatrix.fA * tPoint.fX) + (tMatrix.fC * tPoint.fY) + tMatrix.fE;
	tOut.fY = (tMatrix.fB * tPoint.fX) + (tMatrix.fD * tPoint.fY) + tMatrix.fF;
	return tOut;
}

static xge_rect_t __xgeShapeExMatrixRectBounds(xge_shape_ex_matrix_t tMatrix, xge_rect_t tRect)
{
	xge_vec2_t tP[4];
	float fMinX;
	float fMinY;
	float fMaxX;
	float fMaxY;
	int i;

	tP[0].fX = tRect.fX; tP[0].fY = tRect.fY;
	tP[1].fX = tRect.fX + tRect.fW; tP[1].fY = tRect.fY;
	tP[2].fX = tRect.fX + tRect.fW; tP[2].fY = tRect.fY + tRect.fH;
	tP[3].fX = tRect.fX; tP[3].fY = tRect.fY + tRect.fH;
	for ( i = 0; i < 4; i++ ) {
		tP[i] = __xgeShapeExMatrixPoint(tMatrix, tP[i]);
	}
	fMinX = fMaxX = tP[0].fX;
	fMinY = fMaxY = tP[0].fY;
	for ( i = 1; i < 4; i++ ) {
		if ( tP[i].fX < fMinX ) fMinX = tP[i].fX;
		if ( tP[i].fY < fMinY ) fMinY = tP[i].fY;
		if ( tP[i].fX > fMaxX ) fMaxX = tP[i].fX;
		if ( tP[i].fY > fMaxY ) fMaxY = tP[i].fY;
	}
	tRect.fX = fMinX;
	tRect.fY = fMinY;
	tRect.fW = fMaxX - fMinX;
	tRect.fH = fMaxY - fMinY;
	return tRect;
}

static xge_rect_t __xgeShapeExRectIntersect(xge_rect_t a, xge_rect_t b)
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

static xge_rect_t __xgeShapeExPixelCenterClipRect(xge_rect_t tRect)
{
	float fLeft = ceilf(tRect.fX - 0.5f);
	float fTop = ceilf(tRect.fY - 0.5f);
	float fRight = ceilf(tRect.fX + tRect.fW - 0.5f);
	float fBottom = ceilf(tRect.fY + tRect.fH - 0.5f);

	if ( fRight < fLeft ) fRight = fLeft;
	if ( fBottom < fTop ) fBottom = fTop;
	return (xge_rect_t){fLeft, fTop, fRight - fLeft, fBottom - fTop};
}

static xge_rect_t __xgeShapeExRectUnion(xge_rect_t a, xge_rect_t b)
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

int xgeShapeExMatrixIdentity(xge_shape_ex_matrix_t* pMatrix)
{
	if ( pMatrix == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pMatrix = __xgeShapeExMatrixIdentity();
	return XGE_OK;
}

int xgeShapeExMatrixMultiply(xge_shape_ex_matrix_t* pOut, const xge_shape_ex_matrix_t* pParent, const xge_shape_ex_matrix_t* pLocal)
{
	if ( (pOut == NULL) || (pParent == NULL) || (pLocal == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pOut = __xgeShapeExMatrixMul(*pParent, *pLocal);
	return XGE_OK;
}

int xgeShapeExMatrixTranslate(xge_shape_ex_matrix_t* pMatrix, float fTX, float fTY)
{
	if ( (pMatrix == NULL) || !__xgeShapeExCoordsFinite(fTX, fTY) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pMatrix->fA = 1.0f;
	pMatrix->fB = 0.0f;
	pMatrix->fC = 0.0f;
	pMatrix->fD = 1.0f;
	pMatrix->fE = fTX;
	pMatrix->fF = fTY;
	return XGE_OK;
}

int xgeShapeExMatrixScale(xge_shape_ex_matrix_t* pMatrix, float fSX, float fSY)
{
	if ( (pMatrix == NULL) || !__xgeShapeExCoordsFinite(fSX, fSY) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pMatrix->fA = fSX;
	pMatrix->fB = 0.0f;
	pMatrix->fC = 0.0f;
	pMatrix->fD = fSY;
	pMatrix->fE = 0.0f;
	pMatrix->fF = 0.0f;
	return XGE_OK;
}

int xgeShapeExMatrixRotate(xge_shape_ex_matrix_t* pMatrix, float fRadians)
{
	float fS;
	float fC;

	if ( (pMatrix == NULL) || !__xgeShapeExFloatFinite(fRadians) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	fS = sinf(fRadians);
	fC = cosf(fRadians);
	pMatrix->fA = fC;
	pMatrix->fB = fS;
	pMatrix->fC = -fS;
	pMatrix->fD = fC;
	pMatrix->fE = 0.0f;
	pMatrix->fF = 0.0f;
	return XGE_OK;
}

int xgeShapeExMatrixSkew(xge_shape_ex_matrix_t* pMatrix, float fXRadians, float fYRadians)
{
	if ( (pMatrix == NULL) || !__xgeShapeExCoordsFinite(fXRadians, fYRadians) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pMatrix->fA = 1.0f;
	pMatrix->fB = tanf(fYRadians);
	pMatrix->fC = tanf(fXRadians);
	pMatrix->fD = 1.0f;
	pMatrix->fE = 0.0f;
	pMatrix->fF = 0.0f;
	return XGE_OK;
}

static void __xgeShapeExTransformStateReset(xge_shape_ex_transform_state_t* pState)
{
	if ( pState == NULL ) return;
	memset(pState, 0, sizeof(*pState));
	pState->fScaleX = 1.0f;
	pState->fScaleY = 1.0f;
}

static void __xgeShapeExTransformStateRebuild(const xge_shape_ex_transform_state_t* pState, xge_shape_ex_matrix_t* pMatrix)
{
	xge_shape_ex_matrix_t tLocal;

	if ( (pState == NULL) || (pMatrix == NULL) ) return;
	*pMatrix = __xgeShapeExMatrixIdentity();
	pMatrix->fE = pState->fTranslateX;
	pMatrix->fF = pState->fTranslateY;
	if ( pState->fRotateRadians != 0.0f ) {
		xgeShapeExMatrixRotate(&tLocal, pState->fRotateRadians);
		*pMatrix = __xgeShapeExMatrixMul(*pMatrix, tLocal);
	}
	if ( (pState->fSkewXRadians != 0.0f) || (pState->fSkewYRadians != 0.0f) ) {
		xgeShapeExMatrixSkew(&tLocal, pState->fSkewXRadians, pState->fSkewYRadians);
		*pMatrix = __xgeShapeExMatrixMul(*pMatrix, tLocal);
	}
	if ( (pState->fScaleX != 1.0f) || (pState->fScaleY != 1.0f) ) {
		xgeShapeExMatrixScale(&tLocal, pState->fScaleX, pState->fScaleY);
		*pMatrix = __xgeShapeExMatrixMul(*pMatrix, tLocal);
	}
}

static int __xgeShapeExMatrixInverse(xge_shape_ex_matrix_t tMatrix, xge_shape_ex_matrix_t* pInverse)
{
	float fDet;
	xge_shape_ex_matrix_t tOut;

	if ( pInverse == NULL ) {
		return 0;
	}
	fDet = (tMatrix.fA * tMatrix.fD) - (tMatrix.fB * tMatrix.fC);
	if ( fabsf(fDet) <= XGE_SHAPE_EX_EPSILON ) {
		return 0;
	}
	tOut.fA = tMatrix.fD / fDet;
	tOut.fB = -tMatrix.fB / fDet;
	tOut.fC = -tMatrix.fC / fDet;
	tOut.fD = tMatrix.fA / fDet;
	tOut.fE = ((tMatrix.fC * tMatrix.fF) - (tMatrix.fD * tMatrix.fE)) / fDet;
	tOut.fF = ((tMatrix.fB * tMatrix.fE) - (tMatrix.fA * tMatrix.fF)) / fDet;
	*pInverse = tOut;
	return 1;
}

int xgeShapeExMatrixInvert(xge_shape_ex_matrix_t* pOut, const xge_shape_ex_matrix_t* pMatrix)
{
	if ( (pOut == NULL) || (pMatrix == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( !__xgeShapeExMatrixInverse(*pMatrix, pOut) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	return XGE_OK;
}

int xgeShapeExMatrixPoint(xge_vec2_t* pOut, const xge_shape_ex_matrix_t* pMatrix, xge_vec2_t tPoint)
{
	if ( (pOut == NULL) || (pMatrix == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pOut = __xgeShapeExMatrixPoint(*pMatrix, tPoint);
	return XGE_OK;
}

int xgeShapeExMatrixVector(xge_vec2_t* pOut, const xge_shape_ex_matrix_t* pMatrix, xge_vec2_t tVector)
{
	if ( (pOut == NULL) || (pMatrix == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pOut->fX = (pMatrix->fA * tVector.fX) + (pMatrix->fC * tVector.fY);
	pOut->fY = (pMatrix->fB * tVector.fX) + (pMatrix->fD * tVector.fY);
	return XGE_OK;
}

int xgeShapeExMatrixRectBounds(xge_rect_t* pOut, const xge_shape_ex_matrix_t* pMatrix, xge_rect_t tRect)
{
	if ( (pOut == NULL) || (pMatrix == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pOut = __xgeShapeExMatrixRectBounds(*pMatrix, tRect);
	return XGE_OK;
}

static float __xgeShapeExMatrixStrokeScale(xge_shape_ex_matrix_t tMatrix)
{
	float fSX;
	float fSY;
	float fScale;

	fSX = sqrtf((tMatrix.fA * tMatrix.fA) + (tMatrix.fB * tMatrix.fB));
	fSY = sqrtf((tMatrix.fC * tMatrix.fC) + (tMatrix.fD * tMatrix.fD));
	fScale = sqrtf(fSX * fSY);
	if ( fScale <= XGE_SHAPE_EX_EPSILON ) {
		fScale = 1.0f;
	}
	return fScale;
}

int xgeShapeExMatrixStrokeScale(float* pScale, const xge_shape_ex_matrix_t* pMatrix)
{
	if ( (pScale == NULL) || (pMatrix == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pScale = __xgeShapeExMatrixStrokeScale(*pMatrix);
	return XGE_OK;
}

static int __xgeShapeExValid(xge_shape_ex pShape)
{
	return (pShape != NULL) && (pShape->iMagic == XGE_SHAPE_EX_MAGIC);
}

static void __xgeShapeExEnsureStrokeState(xge_shape_ex pShape)
{
	if ( pShape->bStrokeStateCreated ) return;
	pShape->bStrokeStateCreated = 1;
	if ( !pShape->bTrimPath ) pShape->bTrimSimultaneous = 1;
}

static int __xgeShapeExPathMeasureValid(xge_shape_ex_path_measure pMeasure)
{
	return (pMeasure != NULL) && (pMeasure->iMagic == XGE_SHAPE_EX_PATH_MEASURE_MAGIC);
}

static int __xgeShapeExSceneValid(xge_shape_ex_scene pScene)
{
	return (pScene != NULL) && (pScene->iMagic == XGE_SHAPE_EX_SCENE_MAGIC);
}

static int __xgeShapeExSceneChildValid(const xge_shape_ex_scene_child_t* pChild)
{
	if ( pChild == NULL ) return 0;
	if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE ) {
		return __xgeShapeExValid(pChild->pShape) && (pChild->pScene == NULL);
	}
	if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SCENE ) {
		return __xgeShapeExSceneValid(pChild->pScene) && (pChild->pShape == NULL);
	}
	return 0;
}

static int __xgeShapeExShapeDecorationsDetached(xge_shape_ex pShape)
{
	int i;

	if ( !__xgeShapeExValid(pShape) ) return 0;
	for ( i = 0; i < pShape->iClipShapeCount; i++ ) {
		if ( !__xgeShapeExValid(pShape->pClipShapes[i]) ||
		     (pShape->pClipShapes[i]->pParentScene != NULL) ) return 0;
	}
	if ( pShape->iMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_SHAPE ) {
		return __xgeShapeExValid(pShape->pMaskShape) && (pShape->pMaskShape->pParentScene == NULL);
	}
	if ( pShape->iMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_SCENE ) {
		return __xgeShapeExSceneValid(pShape->pMaskScene) && (pShape->pMaskScene->pParentScene == NULL);
	}
	return pShape->iMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_NONE;
}

static int __xgeShapeExSceneDecorationsDetached(xge_shape_ex_scene pScene)
{
	int i;

	if ( !__xgeShapeExSceneValid(pScene) ) return 0;
	for ( i = 0; i < pScene->iClipShapeCount; i++ ) {
		if ( !__xgeShapeExValid(pScene->pClipShapes[i]) ||
		     (pScene->pClipShapes[i]->pParentScene != NULL) ) return 0;
	}
	if ( pScene->iMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_SHAPE ) {
		return __xgeShapeExValid(pScene->pMaskShape) && (pScene->pMaskShape->pParentScene == NULL);
	}
	if ( pScene->iMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_SCENE ) {
		return __xgeShapeExSceneValid(pScene->pMaskScene) && (pScene->pMaskScene->pParentScene == NULL);
	}
	return pScene->iMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_NONE;
}

static void __xgeShapeExShapeDecorationsParentReplace(
	xge_shape_ex pShape,
	xge_shape_ex_scene pExpected,
	xge_shape_ex_scene pReplacement
)
{
	int i;

	if ( !__xgeShapeExValid(pShape) ) return;
	for ( i = 0; i < pShape->iClipShapeCount; i++ ) {
		if ( __xgeShapeExValid(pShape->pClipShapes[i]) &&
		     (pShape->pClipShapes[i]->pParentScene == pExpected) ) {
			pShape->pClipShapes[i]->pParentScene = pReplacement;
		}
	}
	if ( (pShape->iMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_SHAPE) &&
	     __xgeShapeExValid(pShape->pMaskShape) &&
	     (pShape->pMaskShape->pParentScene == pExpected) ) {
		pShape->pMaskShape->pParentScene = pReplacement;
	} else if ( (pShape->iMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_SCENE) &&
	            __xgeShapeExSceneValid(pShape->pMaskScene) &&
	            (pShape->pMaskScene->pParentScene == pExpected) ) {
		pShape->pMaskScene->pParentScene = pReplacement;
	}
}

static void __xgeShapeExSceneDecorationsParentReplace(
	xge_shape_ex_scene pScene,
	xge_shape_ex_scene pExpected,
	xge_shape_ex_scene pReplacement
)
{
	int i;

	if ( !__xgeShapeExSceneValid(pScene) ) return;
	for ( i = 0; i < pScene->iClipShapeCount; i++ ) {
		if ( __xgeShapeExValid(pScene->pClipShapes[i]) &&
		     (pScene->pClipShapes[i]->pParentScene == pExpected) ) {
			pScene->pClipShapes[i]->pParentScene = pReplacement;
		}
	}
	if ( (pScene->iMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_SHAPE) &&
	     __xgeShapeExValid(pScene->pMaskShape) &&
	     (pScene->pMaskShape->pParentScene == pExpected) ) {
		pScene->pMaskShape->pParentScene = pReplacement;
	} else if ( (pScene->iMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_SCENE) &&
	            __xgeShapeExSceneValid(pScene->pMaskScene) &&
	            (pScene->pMaskScene->pParentScene == pExpected) ) {
		pScene->pMaskScene->pParentScene = pReplacement;
	}
}

static void __xgeShapeExSceneChildRelease(xge_shape_ex_scene pOwner, xge_shape_ex_scene_child_t* pChild)
{
	if ( pChild == NULL ) return;
	if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE ) {
		__xgeShapeExShapeDecorationsParentReplace(pChild->pShape, pOwner, NULL);
		if ( pChild->pShape->pParentScene == pOwner ) pChild->pShape->pParentScene = NULL;
		xgeShapeExDestroy(pChild->pShape);
	} else if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SCENE ) {
		__xgeShapeExSceneDecorationsParentReplace(pChild->pScene, pOwner, NULL);
		if ( pChild->pScene->pParentScene == pOwner ) pChild->pScene->pParentScene = NULL;
		xgeShapeExSceneDestroy(pChild->pScene);
	}
	memset(pChild, 0, sizeof(*pChild));
}

static int __xgeShapeExBlendValid(int iBlend)
{
	return (iBlend >= XGE_BLEND_NONE) && (iBlend <= XGE_BLEND_LUMINOSITY);
}

static int __xgeShapeExMaskMethodValid(int iMethod)
{
	return (iMethod >= XGE_SHAPE_EX_MASK_ALPHA) && (iMethod <= XGE_SHAPE_EX_MASK_DARKEN);
}

static int __xgeShapeExEffectTypeValid(int iType)
{
	return (iType >= XGE_SHAPE_EX_EFFECT_GAUSSIAN_BLUR) && (iType <= XGE_SHAPE_EX_EFFECT_TRITONE);
}

static void __xgeShapeExMaskTargetRelease(int iTargetType, xge_shape_ex pShape, xge_shape_ex_scene pScene)
{
	if ( iTargetType == XGE_SHAPE_EX_MASK_TARGET_SHAPE ) {
		xgeShapeExDestroy(pShape);
	} else if ( iTargetType == XGE_SHAPE_EX_MASK_TARGET_SCENE ) {
		xgeShapeExSceneDestroy(pScene);
	}
}

static void __xgeShapeExMaskClearInternal(xge_shape_ex pShape)
{
	int iTargetType;
	xge_shape_ex pTargetShape;
	xge_shape_ex_scene pTargetScene;

	if ( pShape == NULL ) return;
	iTargetType = pShape->iMaskTargetType;
	pTargetShape = pShape->pMaskShape;
	pTargetScene = pShape->pMaskScene;
	pShape->iMaskMethod = XGE_SHAPE_EX_MASK_NONE;
	pShape->iMaskTargetType = XGE_SHAPE_EX_MASK_TARGET_NONE;
	pShape->pMaskShape = NULL;
	pShape->pMaskScene = NULL;
	if ( (iTargetType == XGE_SHAPE_EX_MASK_TARGET_SHAPE) && __xgeShapeExValid(pTargetShape) &&
	     (pTargetShape->pParentScene == pShape->pParentScene) ) {
		pTargetShape->pParentScene = NULL;
	} else if ( (iTargetType == XGE_SHAPE_EX_MASK_TARGET_SCENE) && __xgeShapeExSceneValid(pTargetScene) &&
	            (pTargetScene->pParentScene == pShape->pParentScene) ) {
		pTargetScene->pParentScene = NULL;
	}
	__xgeShapeExMaskTargetRelease(iTargetType, pTargetShape, pTargetScene);
}

static void __xgeShapeExSceneMaskClearInternal(xge_shape_ex_scene pScene)
{
	int iTargetType;
	xge_shape_ex pTargetShape;
	xge_shape_ex_scene pTargetScene;

	if ( pScene == NULL ) return;
	iTargetType = pScene->iMaskTargetType;
	pTargetShape = pScene->pMaskShape;
	pTargetScene = pScene->pMaskScene;
	pScene->iMaskMethod = XGE_SHAPE_EX_MASK_NONE;
	pScene->iMaskTargetType = XGE_SHAPE_EX_MASK_TARGET_NONE;
	pScene->pMaskShape = NULL;
	pScene->pMaskScene = NULL;
	if ( (iTargetType == XGE_SHAPE_EX_MASK_TARGET_SHAPE) && __xgeShapeExValid(pTargetShape) &&
	     (pTargetShape->pParentScene == pScene->pParentScene) ) {
		pTargetShape->pParentScene = NULL;
	} else if ( (iTargetType == XGE_SHAPE_EX_MASK_TARGET_SCENE) && __xgeShapeExSceneValid(pTargetScene) &&
	            (pTargetScene->pParentScene == pScene->pParentScene) ) {
		pTargetScene->pParentScene = NULL;
	}
	__xgeShapeExMaskTargetRelease(iTargetType, pTargetShape, pTargetScene);
}

static int __xgeShapeExPaintReachesInternal(
	int iNodeType,
	const void* pNode,
	int iTargetType,
	const void* pTarget,
	const void** ppStack,
	int* pTypeStack,
	int iDepth
)
{
	int i;

	if ( (pNode == NULL) || (pTarget == NULL) ) return 0;
	if ( (iNodeType == iTargetType) && (pNode == pTarget) ) return 1;
	if ( iDepth >= XGE_SHAPE_EX_CLIP_BOUNDS_MAX_DEPTH ) return 1;
	for ( i = 0; i < iDepth; i++ ) {
		if ( (pTypeStack[i] == iNodeType) && (ppStack[i] == pNode) ) return 0;
	}
	ppStack[iDepth] = pNode;
	pTypeStack[iDepth] = iNodeType;
	iDepth++;
	if ( iNodeType == XGE_SHAPE_EX_MASK_TARGET_SHAPE ) {
		xge_shape_ex pShape = (xge_shape_ex)pNode;

		if ( !__xgeShapeExValid(pShape) ) return 0;
		for ( i = 0; i < pShape->iClipShapeCount; i++ ) {
			if ( __xgeShapeExPaintReachesInternal(
				XGE_SHAPE_EX_MASK_TARGET_SHAPE, pShape->pClipShapes[i], iTargetType, pTarget,
				ppStack, pTypeStack, iDepth
			) ) return 1;
		}
		if ( pShape->iMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_SHAPE ) {
			return __xgeShapeExPaintReachesInternal(
				XGE_SHAPE_EX_MASK_TARGET_SHAPE, pShape->pMaskShape, iTargetType, pTarget,
				ppStack, pTypeStack, iDepth
			);
		}
		if ( pShape->iMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_SCENE ) {
			return __xgeShapeExPaintReachesInternal(
				XGE_SHAPE_EX_MASK_TARGET_SCENE, pShape->pMaskScene, iTargetType, pTarget,
				ppStack, pTypeStack, iDepth
			);
		}
		return 0;
	}
	if ( iNodeType == XGE_SHAPE_EX_MASK_TARGET_SCENE ) {
		xge_shape_ex_scene pScene = (xge_shape_ex_scene)pNode;

		if ( !__xgeShapeExSceneValid(pScene) ) return 0;
		for ( i = 0; i < pScene->iChildCount; i++ ) {
			const xge_shape_ex_scene_child_t* pChild = &pScene->pChildren[i];
			int iChildType;
			const void* pChildNode;

			if ( !__xgeShapeExSceneChildValid(pChild) ) continue;
			iChildType = pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE ?
				XGE_SHAPE_EX_MASK_TARGET_SHAPE : XGE_SHAPE_EX_MASK_TARGET_SCENE;
			pChildNode = pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE ?
				(const void*)pChild->pShape : (const void*)pChild->pScene;
			if ( __xgeShapeExPaintReachesInternal(
				iChildType, pChildNode, iTargetType, pTarget,
				ppStack, pTypeStack, iDepth
			) ) return 1;
		}
		for ( i = 0; i < pScene->iClipShapeCount; i++ ) {
			if ( __xgeShapeExPaintReachesInternal(
				XGE_SHAPE_EX_MASK_TARGET_SHAPE, pScene->pClipShapes[i], iTargetType, pTarget,
				ppStack, pTypeStack, iDepth
			) ) return 1;
		}
		if ( pScene->iMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_SHAPE ) {
			return __xgeShapeExPaintReachesInternal(
				XGE_SHAPE_EX_MASK_TARGET_SHAPE, pScene->pMaskShape, iTargetType, pTarget,
				ppStack, pTypeStack, iDepth
			);
		}
		if ( pScene->iMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_SCENE ) {
			return __xgeShapeExPaintReachesInternal(
				XGE_SHAPE_EX_MASK_TARGET_SCENE, pScene->pMaskScene, iTargetType, pTarget,
				ppStack, pTypeStack, iDepth
			);
		}
	}
	return 0;
}

static int __xgeShapeExPaintReaches(int iNodeType, const void* pNode, int iTargetType, const void* pTarget)
{
	const void* arrStack[XGE_SHAPE_EX_CLIP_BOUNDS_MAX_DEPTH];
	int arrTypeStack[XGE_SHAPE_EX_CLIP_BOUNDS_MAX_DEPTH];

	return __xgeShapeExPaintReachesInternal(
		iNodeType, pNode, iTargetType, pTarget, arrStack, arrTypeStack, 0
	);
}

static uint32_t __xgeShapeExColorOpacity(uint32_t iColor, float fOpacity)
{
	int iA;

	if ( fOpacity < 0.0f ) fOpacity = 0.0f;
	if ( fOpacity > 1.0f ) fOpacity = 1.0f;
	iA = (int)((float)XGE_COLOR_GET_A(iColor) * fOpacity + 0.5f);
	if ( iA < 0 ) iA = 0;
	if ( iA > 255 ) iA = 255;
	return XGE_COLOR_RGBA(XGE_COLOR_GET_R(iColor), XGE_COLOR_GET_G(iColor), XGE_COLOR_GET_B(iColor), iA);
}

static int __xgeShapeExCommandPointCount(uint8_t iCommand)
{
	switch ( iCommand ) {
		case XGE_SHAPE_EX_CMD_CLOSE:
			return 0;
		case XGE_SHAPE_EX_CMD_MOVE_TO:
		case XGE_SHAPE_EX_CMD_LINE_TO:
			return 1;
		case XGE_SHAPE_EX_CMD_QUAD_TO:
			return 2;
		case XGE_SHAPE_EX_CMD_CUBIC_TO:
			return 3;
		default:
			return -1;
	}
}

static uint32_t __xgeShapeExColorLerp(uint32_t iA, uint32_t iB, float fT)
{
	int r;
	int g;
	int b;
	int a;

	if ( fT < 0.0f ) fT = 0.0f;
	if ( fT > 1.0f ) fT = 1.0f;
	r = (int)((float)XGE_COLOR_GET_R(iA) + ((float)XGE_COLOR_GET_R(iB) - (float)XGE_COLOR_GET_R(iA)) * fT + 0.5f);
	g = (int)((float)XGE_COLOR_GET_G(iA) + ((float)XGE_COLOR_GET_G(iB) - (float)XGE_COLOR_GET_G(iA)) * fT + 0.5f);
	b = (int)((float)XGE_COLOR_GET_B(iA) + ((float)XGE_COLOR_GET_B(iB) - (float)XGE_COLOR_GET_B(iA)) * fT + 0.5f);
	a = (int)((float)XGE_COLOR_GET_A(iA) + ((float)XGE_COLOR_GET_A(iB) - (float)XGE_COLOR_GET_A(iA)) * fT + 0.5f);
	if ( r < 0 ) r = 0;
	if ( r > 255 ) r = 255;
	if ( g < 0 ) g = 0;
	if ( g > 255 ) g = 255;
	if ( b < 0 ) b = 0;
	if ( b > 255 ) b = 255;
	if ( a < 0 ) a = 0;
	if ( a > 255 ) a = 255;
	return XGE_COLOR_RGBA(r, g, b, a);
}

static float __xgeShapeExGradientSpreadT(int iSpread, float fT)
{
	if ( iSpread == XGE_SHAPE_EX_GRADIENT_SPREAD_REPEAT ) {
		fT = fT - floorf(fT);
		if ( fT < 0.0f ) fT += 1.0f;
		return fT;
	}
	if ( iSpread == XGE_SHAPE_EX_GRADIENT_SPREAD_REFLECT ) {
		fT = fT - floorf(fT * 0.5f) * 2.0f;
		if ( fT < 0.0f ) fT += 2.0f;
		return fT <= 1.0f ? fT : (2.0f - fT);
	}
	if ( fT < 0.0f ) return 0.0f;
	if ( fT > 1.0f ) return 1.0f;
	return fT;
}

static xge_vec2_t __xgeShapeExGradientSamplePoint(int iUnits, xge_shape_ex_matrix_t tGradientTransform, xge_vec2_t tPoint, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, int* pOk)
{
	xge_vec2_t tSample;
	xge_shape_ex_matrix_t tInverse;

	if ( pOk != NULL ) {
		*pOk = 1;
	}
	if ( iUnits == XGE_SHAPE_EX_GRADIENT_USER_SPACE ) {
		if ( !__xgeShapeExMatrixInverse(tMatrix, &tInverse) ) {
			if ( pOk != NULL ) {
				*pOk = 0;
			}
			return tPoint;
		}
		tSample = __xgeShapeExMatrixPoint(tInverse, tPoint);
	} else {
		float fW = fabsf(tBounds.fW) > XGE_SHAPE_EX_EPSILON ? tBounds.fW : 1.0f;
		float fH = fabsf(tBounds.fH) > XGE_SHAPE_EX_EPSILON ? tBounds.fH : 1.0f;

		tSample.fX = (tPoint.fX - tBounds.fX) / fW;
		tSample.fY = (tPoint.fY - tBounds.fY) / fH;
	}
	if ( !__xgeShapeExMatrixInverse(tGradientTransform, &tInverse) ) {
		if ( pOk != NULL ) {
			*pOk = 0;
		}
		return tSample;
	}
	tSample = __xgeShapeExMatrixPoint(tInverse, tSample);
	return tSample;
}

static uint32_t __xgeShapeExGradientStopColorRaw(const xge_shape_ex_color_stop_t* pStops, int iStopCount, uint32_t iFallbackColor, float fT)
{
	int i;

	if ( (pStops == NULL) || (iStopCount <= 0) ) {
		return iFallbackColor;
	}
	if ( fT < pStops[0].fOffset ) {
		return pStops[0].iColor;
	}
	for ( i = 0; i < iStopCount; i++ ) {
		if ( fT == pStops[i].fOffset ) {
			while ( ((i + 1) < iStopCount) && (pStops[i + 1].fOffset == pStops[i].fOffset) ) i++;
			return pStops[i].iColor;
		}
		if ( (i == 0) || (fT > pStops[i].fOffset) ) {
			continue;
		}
		{
			float fA = pStops[i - 1].fOffset;
			float fB = pStops[i].fOffset;
			float fLocal = (fabsf(fB - fA) <= XGE_SHAPE_EX_EPSILON) ? 0.0f : ((fT - fA) / (fB - fA));
			return __xgeShapeExColorLerp(pStops[i - 1].iColor, pStops[i].iColor, fLocal);
		}
	}
	return pStops[iStopCount - 1].iColor;
}

static int __xgeShapeExGradientRepeatMargin(const xge_shape_ex_color_stop_t* pStops, int iStopCount, float fLength, int bBegin)
{
	float fAvailable;
	int iMargin;
	int iThreshold;

	if ( (pStops == NULL) || (iStopCount < 2) || (fLength <= XGE_SHAPE_EX_EPSILON) ) {
		return 0;
	}
	iMargin = (int)(800.0f / fLength);
	if ( iMargin > 40 ) iMargin = 40;
	if ( iMargin <= 0 ) return 0;
	fAvailable = bBegin ? pStops[1].fOffset : (1.0f - pStops[iStopCount - 2].fOffset);
	if ( fAvailable < 0.0f ) fAvailable = 0.0f;
	if ( fAvailable > 1.0f ) fAvailable = 1.0f;
	iThreshold = (int)(fAvailable * 1024.0f * 0.1f);
	if ( iMargin > iThreshold ) iMargin = iThreshold;
	return iMargin;
}

static uint32_t __xgeShapeExGradientStopColor(const xge_shape_ex_color_stop_t* pStops, int iStopCount, uint32_t iFallbackColor, int iSpread, float fT, float fRepeatLength)
{
	int iBegin;
	int iEnd;

	fT = __xgeShapeExGradientSpreadT(iSpread, fT);
	if ( (iSpread == XGE_SHAPE_EX_GRADIENT_SPREAD_REPEAT) && (iStopCount > 1) ) {
		iBegin = __xgeShapeExGradientRepeatMargin(pStops, iStopCount, fRepeatLength, 1);
		iEnd = __xgeShapeExGradientRepeatMargin(pStops, iStopCount, fRepeatLength, 0);
		if ( (iBegin > 0) && (iEnd > 0) ) {
			float fBegin = (float)iBegin / 1024.0f;
			float fEnd = (float)iEnd / 1024.0f;

			if ( (fT < fBegin) || (fT >= (1.0f - fEnd)) ) {
				uint32_t iEndColor = __xgeShapeExGradientStopColorRaw(pStops, iStopCount, iFallbackColor, 1.0f - fEnd);
				uint32_t iBeginColor = __xgeShapeExGradientStopColorRaw(pStops, iStopCount, iFallbackColor, fBegin);
				float fIndex = fT < fBegin ? (fEnd * 1024.0f + fT * 1024.0f) : ((fT - (1.0f - fEnd)) * 1024.0f);
				float fBlend = (fIndex + 1.0f) / (float)(iBegin + iEnd + 1);

				return __xgeShapeExColorLerp(iEndColor, iBeginColor, fBlend);
			}
		}
	}
	return __xgeShapeExGradientStopColorRaw(pStops, iStopCount, iFallbackColor, fT);
}

static uint32_t __xgeShapeExLinearGradientColorEx(float fX1, float fY1, float fX2, float fY2, int iUnits, xge_shape_ex_matrix_t tGradientTransform, const xge_shape_ex_color_stop_t* pStops, int iStopCount, uint32_t iFallbackColor, int iSpread, xge_vec2_t tPoint, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix)
{
	xge_vec2_t tA;
	xge_vec2_t tB;
	xge_vec2_t tSample;
	float fDX;
	float fDY;
	float fLenSq;
	float fT;
	int bOk;

	if ( (pStops == NULL) || (iStopCount <= 0) ) {
		return iFallbackColor;
	}
	tSample = __xgeShapeExGradientSamplePoint(iUnits, tGradientTransform, tPoint, tBounds, tMatrix, &bOk);
	if ( !bOk ) {
		return iFallbackColor;
	}
	tA.fX = fX1;
	tA.fY = fY1;
	tB.fX = fX2;
	tB.fY = fY2;
	fDX = tB.fX - tA.fX;
	fDY = tB.fY - tA.fY;
	fLenSq = (fDX * fDX) + (fDY * fDY);
	if ( fLenSq <= XGE_SHAPE_EX_EPSILON ) {
		return pStops[iStopCount - 1].iColor;
	}
	fT = (((tSample.fX - tA.fX) * fDX) + ((tSample.fY - tA.fY) * fDY)) / fLenSq;
	return __xgeShapeExGradientStopColor(pStops, iStopCount, iFallbackColor, iSpread, fT, sqrtf(fLenSq));
}

static float __xgeShapeExRadialGradientT(xge_vec2_t tPoint, xge_vec2_t tCenter, xge_vec2_t tFocal, float fRadius)
{
	float fDX;
	float fDY;
	float fFX;
	float fFY;
	float fA;
	float fB;
	float fC;
	float fDisc;
	float fS1;
	float fS2;
	float fS;

	if ( fRadius <= XGE_SHAPE_EX_EPSILON ) {
		return 1.0f;
	}
	fDX = tPoint.fX - tFocal.fX;
	fDY = tPoint.fY - tFocal.fY;
	fA = (fDX * fDX) + (fDY * fDY);
	if ( fA <= XGE_SHAPE_EX_EPSILON ) {
		return 0.0f;
	}
	fFX = tFocal.fX - tCenter.fX;
	fFY = tFocal.fY - tCenter.fY;
	fB = 2.0f * ((fDX * fFX) + (fDY * fFY));
	fC = (fFX * fFX) + (fFY * fFY) - (fRadius * fRadius);
	fDisc = (fB * fB) - (4.0f * fA * fC);
	if ( fDisc <= XGE_SHAPE_EX_EPSILON ) {
		return sqrtf(fA) / fRadius;
	}
	fDisc = sqrtf(fDisc);
	fS1 = (-fB + fDisc) / (2.0f * fA);
	fS2 = (-fB - fDisc) / (2.0f * fA);
	fS = fS1 > fS2 ? fS1 : fS2;
	if ( fS <= XGE_SHAPE_EX_EPSILON ) {
		fS = fS1 > XGE_SHAPE_EX_EPSILON ? fS1 : fS2;
	}
	if ( fS <= XGE_SHAPE_EX_EPSILON ) {
		return sqrtf(fA) / fRadius;
	}
	return 1.0f / fS;
}

static int __xgeShapeExRadialGradientCorrect(
	xge_vec2_t tCenter,
	float fRadius,
	xge_vec2_t* pFocal,
	float* pFocalRadius
)
{
	xge_vec2_t tDiff;
	float fAbsX;
	float fAbsY;
	float fDist;
	float fScale;
	float fMaxFR;
	int bFocalClamped;

	if ( (pFocal == NULL) || (pFocalRadius == NULL) ) return 0;
	if ( *pFocalRadius < 0.0f ) {
		*pFocalRadius = 0.0f;
	}
	if ( *pFocalRadius >= fRadius ) {
		*pFocalRadius = fRadius - XGE_SHAPE_EX_EPSILON;
		if ( *pFocalRadius < 0.0f ) {
			*pFocalRadius = 0.0f;
		}
	}
	bFocalClamped = 0;
	tDiff.fX = tCenter.fX - pFocal->fX;
	tDiff.fY = tCenter.fY - pFocal->fY;
	fAbsX = fabsf(tDiff.fX);
	fAbsY = fabsf(tDiff.fY);
	fDist = fAbsX > fAbsY ? (fAbsX + 0.375f * fAbsY) : (fAbsY + 0.375f * fAbsX);
	if ( (fRadius - fDist) < 0.02f ) {
		if ( fDist < 0.02f ) {
			fDist = 0.02f;
			tDiff.fX = 0.02f;
			tDiff.fY = 0.0f;
		}
		fScale = fRadius * 0.98f / fDist;
		tDiff.fX *= fScale;
		tDiff.fY *= fScale;
		fDist *= fScale;
		pFocal->fX = tCenter.fX - tDiff.fX;
		pFocal->fY = tCenter.fY - tDiff.fY;
		bFocalClamped = 1;
	}
	fMaxFR = (fRadius - fDist) * 0.98f;
	if ( *pFocalRadius > fMaxFR ) {
		*pFocalRadius = fMaxFR > 0.0f ? fMaxFR : 0.0f;
	}
	return bFocalClamped;
}

static float __xgeShapeExRadialGradientTEx(xge_vec2_t tPoint, xge_vec2_t tCenter, xge_vec2_t tFocal, float fRadius, float fFocalRadius)
{
	xge_vec2_t tP;
	xge_vec2_t tD;
	float fDR;
	float fDen;
	float fA;
	float fPrecision;
	float fB;
	float fC;
	float fDisc;
	int bFocalClamped;

	if ( fRadius < 0.02f ) {
		return 1.0f;
	}
	bFocalClamped = __xgeShapeExRadialGradientCorrect(tCenter, fRadius, &tFocal, &fFocalRadius);
	tP.fX = tPoint.fX - tFocal.fX;
	tP.fY = tPoint.fY - tFocal.fY;
	tD.fX = tCenter.fX - tFocal.fX;
	tD.fY = tCenter.fY - tFocal.fY;
	fDR = fRadius - fFocalRadius;
	fC = (tP.fX * tP.fX) + (tP.fY * tP.fY) - (fFocalRadius * fFocalRadius);
	fA = (fDR * fDR) - (tD.fX * tD.fX) - (tD.fY * tD.fY);
	fPrecision = fmaxf(0.00001f, fmaxf(fDR * fDR, 1.0f) * 0.0001f);
	fDen = (fDR * fFocalRadius) + (tP.fX * tD.fX) + (tP.fY * tD.fY);
	if ( fA < fPrecision ) {
		if ( fabsf(fDen) <= XGE_SHAPE_EX_EPSILON ) {
			return 1.0f;
		}
		return 0.5f * fC / fDen;
	}
	fB = fDen / fA;
	fDisc = (fB * fB) + (fC / fA);
	if ( fDisc < 0.0f ) return 0.0f;
	fDisc = sqrtf(fDisc);
	if ( bFocalClamped && (fB < 0.0f) ) return 0.0f;
	return fDisc - fB;
}

static uint32_t __xgeShapeExRadialGradientColorEx(float fCX, float fCY, float fRadius, float fFX, float fFY, float fFocalRadius, int iUnits, xge_shape_ex_matrix_t tGradientTransform, const xge_shape_ex_color_stop_t* pStops, int iStopCount, uint32_t iFallbackColor, int iSpread, xge_vec2_t tPoint, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix)
{
	xge_vec2_t tCenter;
	xge_vec2_t tFocal;
	xge_vec2_t tSample;
	float fT;
	int bOk;

	if ( (pStops == NULL) || (iStopCount <= 0) ) {
		return iFallbackColor;
	}
	tSample = __xgeShapeExGradientSamplePoint(iUnits, tGradientTransform, tPoint, tBounds, tMatrix, &bOk);
	if ( !bOk ) {
		return iFallbackColor;
	}
	tCenter.fX = fCX;
	tCenter.fY = fCY;
	tFocal.fX = fFX;
	tFocal.fY = fFY;
	fT = __xgeShapeExRadialGradientTEx(tSample, tCenter, tFocal, fRadius, fFocalRadius);
	return __xgeShapeExGradientStopColor(pStops, iStopCount, iFallbackColor, iSpread, fT, fabsf(fRadius));
}

static uint32_t __xgeShapeExLinearGradientColor(const xge_shape_ex pShape, xge_vec2_t tPoint, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix)
{
	if ( (pShape == NULL) || (pShape->iFillType != XGE_SHAPE_EX_FILL_LINEAR_GRADIENT) ) {
		return pShape != NULL ? pShape->iFillColor : XGE_COLOR_RGBA(0, 0, 0, 0);
	}
	return __xgeShapeExLinearGradientColorEx(pShape->fFillX1, pShape->fFillY1, pShape->fFillX2, pShape->fFillY2,
		pShape->iFillGradientUnits, pShape->tFillGradientTransform, pShape->pFillStops, pShape->iFillStopCount,
		pShape->iFillColor, pShape->iFillGradientSpread, tPoint, tBounds, tMatrix);
}

static uint32_t __xgeShapeExRadialGradientColor(const xge_shape_ex pShape, xge_vec2_t tPoint, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix)
{
	if ( (pShape == NULL) || (pShape->iFillType != XGE_SHAPE_EX_FILL_RADIAL_GRADIENT) ) {
		return pShape != NULL ? pShape->iFillColor : XGE_COLOR_RGBA(0, 0, 0, 0);
	}
	return __xgeShapeExRadialGradientColorEx(pShape->fFillX1, pShape->fFillY1, pShape->fFillR, pShape->fFillFX, pShape->fFillFY,
		pShape->fFillFR,
		pShape->iFillGradientUnits, pShape->tFillGradientTransform, pShape->pFillStops, pShape->iFillStopCount,
		pShape->iFillColor, pShape->iFillGradientSpread, tPoint, tBounds, tMatrix);
}

static int __xgeShapeExFillVisible(xge_shape_ex pShape, uint32_t iSolidColor)
{
	int i;

	if ( pShape == NULL ) {
		return 0;
	}
	if ( pShape->iFillType == XGE_SHAPE_EX_FILL_SOLID ) {
		return XGE_COLOR_GET_A(iSolidColor) != 0u;
	}
	for ( i = 0; i < pShape->iFillStopCount; i++ ) {
		if ( XGE_COLOR_GET_A(pShape->pFillStops[i].iColor) != 0u ) {
			return 1;
		}
	}
	return 0;
}

static int __xgeShapeExStrokeVisible(xge_shape_ex pShape, uint32_t iSolidColor)
{
	int i;

	if ( pShape == NULL ) {
		return 0;
	}
	if ( pShape->iStrokeType == XGE_SHAPE_EX_FILL_SOLID ) {
		return XGE_COLOR_GET_A(iSolidColor) != 0u;
	}
	for ( i = 0; i < pShape->iStrokeStopCount; i++ ) {
		if ( XGE_COLOR_GET_A(pShape->pStrokeStops[i].iColor) != 0u ) {
			return 1;
		}
	}
	return 0;
}

static int __xgeShapeExMaskUsesLuma(int iMethod)
{
	return (iMethod == XGE_SHAPE_EX_MASK_LUMA) || (iMethod == XGE_SHAPE_EX_MASK_INV_LUMA);
}

static int __xgeShapeExMaskColorVisible(uint32_t iColor, int iMethod)
{
	if ( XGE_COLOR_GET_A(iColor) == 0u ) return 0;
	if ( !__xgeShapeExMaskUsesLuma(iMethod) ) return 1;
	return (XGE_COLOR_GET_R(iColor) * 54u + XGE_COLOR_GET_G(iColor) * 182u + XGE_COLOR_GET_B(iColor) * 19u) != 0u;
}

static int __xgeShapeExMaskStopsVisible(const xge_shape_ex_color_stop_t* pStops, int iStopCount, int iMethod)
{
	int i;

	for ( i = 0; (pStops != NULL) && (i < iStopCount); i++ ) {
		if ( __xgeShapeExMaskColorVisible(pStops[i].iColor, iMethod) ) return 1;
	}
	return 0;
}

static int __xgeShapeExMaskShapePaintVisible(xge_shape_ex pShape, int iMethod)
{
	if ( !__xgeShapeExValid(pShape) || !pShape->bVisible || (pShape->fOpacity <= 0.0f) ) return 0;
	if ( pShape->iFillType == XGE_SHAPE_EX_FILL_SOLID ) {
		if ( __xgeShapeExMaskColorVisible(pShape->iFillColor, iMethod) ) return 1;
	} else if ( __xgeShapeExMaskStopsVisible(pShape->pFillStops, pShape->iFillStopCount, iMethod) ) {
		return 1;
	}
	if ( pShape->fStrokeWidth <= 0.0f ) return 0;
	if ( pShape->iStrokeType == XGE_SHAPE_EX_FILL_SOLID ) {
		return __xgeShapeExMaskColorVisible(pShape->iStrokeColor, iMethod);
	}
	return __xgeShapeExMaskStopsVisible(pShape->pStrokeStops, pShape->iStrokeStopCount, iMethod);
}

static int __xgeShapeExMaskScenePaintVisible(xge_shape_ex_scene pScene, int iMethod)
{
	int i;

	if ( !__xgeShapeExSceneValid(pScene) || !pScene->bVisible || (pScene->fOpacity <= 0.0f) ) return 0;
	for ( i = 0; i < pScene->iChildCount; i++ ) {
		const xge_shape_ex_scene_child_t* pChild = &pScene->pChildren[i];

		if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE ) {
			if ( __xgeShapeExMaskShapePaintVisible(pChild->pShape, iMethod) ) return 1;
		} else if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SCENE ) {
			if ( __xgeShapeExMaskScenePaintVisible(pChild->pScene, iMethod) ) return 1;
		}
	}
	return 0;
}

static int __xgeShapeExReserveCommands(xge_shape_ex pShape, int iNeeded)
{
	uint8_t* pCommands;
	int iCapacity;

	if ( !__xgeShapeExValid(pShape) || (iNeeded < 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iNeeded <= pShape->iCommandCapacity ) {
		return XGE_OK;
	}
	iCapacity = pShape->iCommandCapacity > 0 ? pShape->iCommandCapacity : 16;
	while ( iCapacity < iNeeded ) {
		if ( iCapacity > (INT32_MAX / 2) ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		iCapacity *= 2;
	}
	pCommands = (uint8_t*)xrtRealloc(pShape->pCommands, (size_t)iCapacity * sizeof(*pCommands));
	if ( pCommands == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pShape->pCommands = pCommands;
	pShape->iCommandCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeShapeExReservePoints(xge_shape_ex pShape, int iNeeded)
{
	xge_vec2_t* pPoints;
	int iCapacity;

	if ( !__xgeShapeExValid(pShape) || (iNeeded < 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iNeeded <= pShape->iPointCapacity ) {
		return XGE_OK;
	}
	iCapacity = pShape->iPointCapacity > 0 ? pShape->iPointCapacity : 32;
	while ( iCapacity < iNeeded ) {
		if ( iCapacity > (INT32_MAX / 2) ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		iCapacity *= 2;
	}
	pPoints = (xge_vec2_t*)xrtRealloc(pShape->pPoints, (size_t)iCapacity * sizeof(*pPoints));
	if ( pPoints == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pShape->pPoints = pPoints;
	pShape->iPointCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeShapeExReserveStopArray(
	xge_shape_ex_color_stop_t** ppStops,
	int* pCapacity,
	int iNeeded
)
{
	xge_shape_ex_color_stop_t* pStops;
	int iCapacity;

	if ( (ppStops == NULL) || (pCapacity == NULL) || (iNeeded < 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iNeeded <= *pCapacity ) {
		return XGE_OK;
	}
	iCapacity = *pCapacity > 0 ? *pCapacity : 4;
	while ( iCapacity < iNeeded ) {
		if ( iCapacity > (INT32_MAX / 2) ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		iCapacity *= 2;
	}
	pStops = (xge_shape_ex_color_stop_t*)xrtRealloc(*ppStops, (size_t)iCapacity * sizeof(*pStops));
	if ( pStops == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	*ppStops = pStops;
	*pCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeShapeExReserveFillStops(xge_shape_ex pShape, int iNeeded)
{
	int iRet;

	if ( !__xgeShapeExValid(pShape) || (iNeeded < 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeShapeExReserveStopArray(&pShape->pFillStops, &pShape->iFillStopCapacity, iNeeded);
	if ( iRet != XGE_OK ) return iRet;
	return __xgeShapeExReserveStopArray(
		&pShape->pFillSourceStops, &pShape->iFillSourceStopCapacity, iNeeded
	);
}

static int __xgeShapeExReserveStrokeStops(xge_shape_ex pShape, int iNeeded)
{
	int iRet;

	if ( !__xgeShapeExValid(pShape) || (iNeeded < 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeShapeExReserveStopArray(&pShape->pStrokeStops, &pShape->iStrokeStopCapacity, iNeeded);
	if ( iRet != XGE_OK ) return iRet;
	return __xgeShapeExReserveStopArray(
		&pShape->pStrokeSourceStops, &pShape->iStrokeSourceStopCapacity, iNeeded
	);
}

static void __xgeShapeExClipShapesClearInternal(xge_shape_ex pShape)
{
	int i;

	if ( pShape == NULL ) {
		return;
	}
	for ( i = 0; i < pShape->iClipShapeCount; i++ ) {
		if ( __xgeShapeExValid(pShape->pClipShapes[i]) &&
		     (pShape->pClipShapes[i]->pParentScene == pShape->pParentScene) ) {
			pShape->pClipShapes[i]->pParentScene = NULL;
		}
		xgeShapeExDestroy(pShape->pClipShapes[i]);
	}
	pShape->iClipShapeCount = 0;
}

static int __xgeShapeExClipModeValid(int iMode)
{
	return (iMode == XGE_SHAPE_EX_CLIP_INTERSECT) || (iMode == XGE_SHAPE_EX_CLIP_SUBTRACT);
}

static int __xgeShapeExReserveClipShapes(xge_shape_ex pShape, int iNeeded)
{
	xge_shape_ex* pShapes;
	int* pModes;
	int iCapacity;

	if ( !__xgeShapeExValid(pShape) || (iNeeded < 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iNeeded <= pShape->iClipShapeCapacity ) {
		return XGE_OK;
	}
	iCapacity = pShape->iClipShapeCapacity > 0 ? pShape->iClipShapeCapacity : 4;
	while ( iCapacity < iNeeded ) {
		if ( iCapacity > (INT32_MAX / 2) ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		iCapacity *= 2;
	}
	pShapes = (xge_shape_ex*)xrtMalloc((size_t)iCapacity * sizeof(*pShapes));
	if ( pShapes == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pModes = (int*)xrtMalloc((size_t)iCapacity * sizeof(*pModes));
	if ( pModes == NULL ) {
		xrtFree(pShapes);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( pShape->iClipShapeCount > 0 ) {
		memcpy(pShapes, pShape->pClipShapes, (size_t)pShape->iClipShapeCount * sizeof(*pShapes));
		if ( pShape->pClipShapeModes != NULL ) {
			memcpy(pModes, pShape->pClipShapeModes, (size_t)pShape->iClipShapeCount * sizeof(*pModes));
		} else {
			int i;

			for ( i = 0; i < pShape->iClipShapeCount; i++ ) {
				pModes[i] = XGE_SHAPE_EX_CLIP_INTERSECT;
			}
		}
	}
	xrtFree(pShape->pClipShapes);
	xrtFree(pShape->pClipShapeModes);
	pShape->pClipShapes = pShapes;
	pShape->pClipShapeModes = pModes;
	pShape->iClipShapeCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeShapeExClipShapeReferences(xge_shape_ex pShape, xge_shape_ex pTarget, int iDepth, int* pReferences)
{
	int i;

	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExValid(pTarget) || (pReferences == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pReferences = 0;
	if ( pShape == pTarget ) {
		*pReferences = 1;
		return XGE_OK;
	}
	if ( iDepth >= XGE_SHAPE_EX_CLIP_BOUNDS_MAX_DEPTH ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < pShape->iClipShapeCount; i++ ) {
		int bReferences;
		int iRet;

		iRet = __xgeShapeExClipShapeReferences(pShape->pClipShapes[i], pTarget, iDepth + 1, &bReferences);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		if ( bReferences ) {
			*pReferences = 1;
			return XGE_OK;
		}
	}
	return XGE_OK;
}

static int __xgeShapeExClipShapeAddRefEx(xge_shape_ex pShape, xge_shape_ex pClipShape, int iMode)
{
	int iRet;
	int bWouldCycle;

	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExValid(pClipShape) || (pShape == pClipShape) || !__xgeShapeExClipModeValid(iMode) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pClipShape->pParentScene != NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	bWouldCycle = 0;
	iRet = __xgeShapeExClipShapeReferences(pClipShape, pShape, 0, &bWouldCycle);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( bWouldCycle ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( __xgeShapeExPaintReaches(
		XGE_SHAPE_EX_MASK_TARGET_SHAPE, pClipShape,
		XGE_SHAPE_EX_MASK_TARGET_SHAPE, pShape
	) ) return XGE_ERROR_INVALID_ARGUMENT;
	iRet = __xgeShapeExReserveClipShapes(pShape, pShape->iClipShapeCount + 1);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = xgeShapeExAddRef(pClipShape);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pShape->pClipShapes[pShape->iClipShapeCount] = pClipShape;
	pShape->pClipShapeModes[pShape->iClipShapeCount] = iMode;
	pShape->iClipShapeCount++;
	if ( pShape->pParentScene != NULL ) pClipShape->pParentScene = pShape->pParentScene;
	return XGE_OK;
}

static int __xgeShapeExClipShapeAddRef(xge_shape_ex pShape, xge_shape_ex pClipShape)
{
	return __xgeShapeExClipShapeAddRefEx(pShape, pClipShape, XGE_SHAPE_EX_CLIP_INTERSECT);
}

static void __xgeShapeExNormalizeStops(xge_shape_ex_color_stop_t* pStops, int iCount)
{
	int i;
	float fPrev;

	if ( (pStops == NULL) || (iCount <= 0) ) {
		return;
	}
	fPrev = 0.0f;
	for ( i = 0; i < iCount; i++ ) {
		if ( pStops[i].fOffset < fPrev ) {
			pStops[i].fOffset = fPrev;
		} else if ( pStops[i].fOffset > 1.0f ) {
			pStops[i].fOffset = 1.0f;
		}
		fPrev = pStops[i].fOffset;
	}
}

static int __xgeShapeExAppendCommand(xge_shape_ex pShape, uint8_t iCommand, const xge_vec2_t* pPoints, int iPointCount)
{
	int i;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || (iPointCount < 0) || ((pPoints == NULL) && (iPointCount > 0)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < iPointCount; i++ ) {
		if ( !__xgeShapeExPointFinite(pPoints[i]) ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
	}
	iRet = __xgeShapeExReserveCommands(pShape, pShape->iCommandCount + 1);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = __xgeShapeExReservePoints(pShape, pShape->iPointCount + iPointCount);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pShape->pCommands[pShape->iCommandCount++] = iCommand;
	if ( iPointCount > 0 ) {
		memcpy(pShape->pPoints + pShape->iPointCount, pPoints, (size_t)iPointCount * sizeof(*pPoints));
		pShape->iPointCount += iPointCount;
	}
	return XGE_OK;
}

static int __xgeShapeExAddMoveTo(xge_shape_ex pShape, float fX, float fY)
{
	xge_vec2_t tPoint;
	int iRet;

	tPoint.fX = fX;
	tPoint.fY = fY;
	iRet = __xgeShapeExAppendCommand(pShape, XGE_SHAPE_EX_CMD_MOVE_TO, &tPoint, 1);
	if ( iRet == XGE_OK ) {
		pShape->tCurrent = tPoint;
		pShape->tStart = tPoint;
		pShape->tLastCubicControl = tPoint;
		pShape->tLastQuadControl = tPoint;
		pShape->bHasCurrent = 1;
		pShape->bHasStart = 1;
	}
	return iRet;
}

static int __xgeShapeExAddLineTo(xge_shape_ex pShape, float fX, float fY)
{
	xge_vec2_t tPoint;
	xge_vec2_t tPrev;
	int bHadCurrent;
	int iRet;

	tPoint.fX = fX;
	tPoint.fY = fY;
	tPrev = pShape->tCurrent;
	bHadCurrent = pShape->bHasCurrent;
	iRet = __xgeShapeExAppendCommand(pShape, XGE_SHAPE_EX_CMD_LINE_TO, &tPoint, 1);
	if ( iRet == XGE_OK ) {
		pShape->tCurrent = tPoint;
		pShape->tLastCubicControl = tPoint;
		pShape->tLastQuadControl = tPoint;
		pShape->bHasCurrent = 1;
		if ( !bHadCurrent ) {
			pShape->tStart = tPoint;
			pShape->bHasStart = 1;
		} else if ( !pShape->bHasStart ) {
			pShape->tStart = tPrev;
			pShape->bHasStart = 1;
		}
	}
	return iRet;
}

static int __xgeShapeExAddCubicTo(xge_shape_ex pShape, float fC1X, float fC1Y, float fC2X, float fC2Y, float fX, float fY)
{
	xge_vec2_t arrPoints[3];
	xge_vec2_t tPrev;
	int bHadCurrent;
	int iRet;

	arrPoints[0].fX = fC1X;
	arrPoints[0].fY = fC1Y;
	arrPoints[1].fX = fC2X;
	arrPoints[1].fY = fC2Y;
	arrPoints[2].fX = fX;
	arrPoints[2].fY = fY;
	tPrev = pShape->tCurrent;
	bHadCurrent = pShape->bHasCurrent;
	iRet = __xgeShapeExAppendCommand(pShape, XGE_SHAPE_EX_CMD_CUBIC_TO, arrPoints, 3);
	if ( iRet == XGE_OK ) {
		pShape->tLastCubicControl = arrPoints[1];
		pShape->tLastQuadControl = arrPoints[2];
		pShape->tCurrent = arrPoints[2];
		pShape->bHasCurrent = 1;
		if ( bHadCurrent && !pShape->bHasStart ) {
			pShape->tStart = tPrev;
			pShape->bHasStart = 1;
		}
	}
	return iRet;
}

static int __xgeShapeExFlatReservePoints(xge_shape_ex_flat_path_t* pFlat, int iNeeded)
{
	xge_vec2_t* pPoints;
	int iCapacity;

	if ( (pFlat == NULL) || (iNeeded < 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iNeeded <= pFlat->iPointCapacity ) {
		return XGE_OK;
	}
	iCapacity = pFlat->iPointCapacity > 0 ? pFlat->iPointCapacity : 64;
	while ( iCapacity < iNeeded ) {
		if ( iCapacity > (INT32_MAX / 2) ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		iCapacity *= 2;
	}
	pPoints = (xge_vec2_t*)xrtRealloc(pFlat->pPoints, (size_t)iCapacity * sizeof(*pPoints));
	if ( pPoints == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pFlat->pPoints = pPoints;
	pFlat->iPointCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeShapeExFlatReserveContours(xge_shape_ex_flat_path_t* pFlat, int iNeeded)
{
	xge_shape_ex_flat_contour_t* pContours;
	int iCapacity;

	if ( (pFlat == NULL) || (iNeeded < 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iNeeded <= pFlat->iContourCapacity ) {
		return XGE_OK;
	}
	iCapacity = pFlat->iContourCapacity > 0 ? pFlat->iContourCapacity : 8;
	while ( iCapacity < iNeeded ) {
		if ( iCapacity > (INT32_MAX / 2) ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		iCapacity *= 2;
	}
	pContours = (xge_shape_ex_flat_contour_t*)xrtRealloc(pFlat->pContours, (size_t)iCapacity * sizeof(*pContours));
	if ( pContours == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pFlat->pContours = pContours;
	pFlat->iContourCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeShapeExFlatBeginContour(xge_shape_ex_flat_path_t* pFlat, xge_vec2_t tPoint)
{
	xge_shape_ex_flat_contour_t* pContour;
	int iRet;

	iRet = __xgeShapeExFlatReserveContours(pFlat, pFlat->iContourCount + 1);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = __xgeShapeExFlatReservePoints(pFlat, pFlat->iPointCount + 1);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pContour = &pFlat->pContours[pFlat->iContourCount++];
	pContour->iStart = pFlat->iPointCount;
	pContour->iCount = 1;
	pContour->bClosed = 0;
	pFlat->pPoints[pFlat->iPointCount++] = tPoint;
	return XGE_OK;
}

static int __xgeShapeExFlatAddPoint(xge_shape_ex_flat_path_t* pFlat, xge_vec2_t tPoint)
{
	xge_shape_ex_flat_contour_t* pContour;
	xge_vec2_t tPrev;
	int iRet;

	if ( (pFlat == NULL) || (pFlat->iContourCount <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pContour = &pFlat->pContours[pFlat->iContourCount - 1];
	if ( pContour->iCount > 0 ) {
		tPrev = pFlat->pPoints[pContour->iStart + pContour->iCount - 1];
		if ( (fabsf(tPrev.fX - tPoint.fX) <= XGE_SHAPE_EX_EPSILON) &&
		     (fabsf(tPrev.fY - tPoint.fY) <= XGE_SHAPE_EX_EPSILON) ) {
			if ( pContour->iCount != 1 ) {
				return XGE_OK;
			}
		}
	}
	iRet = __xgeShapeExFlatReservePoints(pFlat, pFlat->iPointCount + 1);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pFlat->pPoints[pFlat->iPointCount++] = tPoint;
	pContour->iCount++;
	return XGE_OK;
}

static void __xgeShapeExFlatCloseContour(xge_shape_ex_flat_path_t* pFlat)
{
	xge_shape_ex_flat_contour_t* pContour;

	if ( (pFlat == NULL) || (pFlat->iContourCount <= 0) ) {
		return;
	}
	pContour = &pFlat->pContours[pFlat->iContourCount - 1];
	pContour->bClosed = 1;
}

static void __xgeShapeExFlatFree(xge_shape_ex_flat_path_t* pFlat)
{
	if ( pFlat == NULL ) {
		return;
	}
	xrtFree(pFlat->pPoints);
	xrtFree(pFlat->pContours);
	memset(pFlat, 0, sizeof(*pFlat));
}

static xge_vec2_t __xgeShapeExMidPoint(xge_vec2_t a, xge_vec2_t b)
{
	return (xge_vec2_t){(a.fX + b.fX) * 0.5f, (a.fY + b.fY) * 0.5f};
}

static float __xgeShapeExPointLineDistance(xge_vec2_t tPoint, xge_vec2_t tA, xge_vec2_t tB)
{
	float fDX = tB.fX - tA.fX;
	float fDY = tB.fY - tA.fY;
	float fLength = hypotf(fDX, fDY);

	if ( fLength <= XGE_SHAPE_EX_EPSILON ) {
		return hypotf(tPoint.fX - tA.fX, tPoint.fY - tA.fY);
	}
	return fabsf(fDX * (tA.fY - tPoint.fY) - (tA.fX - tPoint.fX) * fDY) / fLength;
}

static int __xgeShapeExFlattenQuadAdaptive(
	xge_shape_ex_flat_path_t* pFlat,
	xge_vec2_t tP0,
	xge_vec2_t tP1,
	xge_vec2_t tP2,
	float fTolerance,
	int iDepth
)
{
	if ( (iDepth >= XGE_SHAPE_EX_FLATTEN_MAX_DEPTH) ||
	     (__xgeShapeExPointLineDistance(tP1, tP0, tP2) <= fTolerance * 2.0f) ) {
		return __xgeShapeExFlatAddPoint(pFlat, tP2);
	}
	{
		xge_vec2_t tP01 = __xgeShapeExMidPoint(tP0, tP1);
		xge_vec2_t tP12 = __xgeShapeExMidPoint(tP1, tP2);
		xge_vec2_t tP012 = __xgeShapeExMidPoint(tP01, tP12);
		int iRet = __xgeShapeExFlattenQuadAdaptive(
			pFlat, tP0, tP01, tP012, fTolerance, iDepth + 1
		);

		if ( iRet != XGE_OK ) return iRet;
		return __xgeShapeExFlattenQuadAdaptive(
			pFlat, tP012, tP12, tP2, fTolerance, iDepth + 1
		);
	}
}

static int __xgeShapeExFlattenCubicAdaptive(
	xge_shape_ex_flat_path_t* pFlat,
	xge_vec2_t tP0,
	xge_vec2_t tP1,
	xge_vec2_t tP2,
	xge_vec2_t tP3,
	float fTolerance,
	int iDepth
)
{
	float fFlatness1 = __xgeShapeExPointLineDistance(tP1, tP0, tP3);
	float fFlatness2 = __xgeShapeExPointLineDistance(tP2, tP0, tP3);

	if ( (iDepth >= XGE_SHAPE_EX_FLATTEN_MAX_DEPTH) ||
	     (fmaxf(fFlatness1, fFlatness2) <= fTolerance) ) {
		return __xgeShapeExFlatAddPoint(pFlat, tP3);
	}
	{
		xge_vec2_t tP01 = __xgeShapeExMidPoint(tP0, tP1);
		xge_vec2_t tP12 = __xgeShapeExMidPoint(tP1, tP2);
		xge_vec2_t tP23 = __xgeShapeExMidPoint(tP2, tP3);
		xge_vec2_t tP012 = __xgeShapeExMidPoint(tP01, tP12);
		xge_vec2_t tP123 = __xgeShapeExMidPoint(tP12, tP23);
		xge_vec2_t tP0123 = __xgeShapeExMidPoint(tP012, tP123);
		int iRet = __xgeShapeExFlattenCubicAdaptive(
			pFlat, tP0, tP01, tP012, tP0123, fTolerance, iDepth + 1
		);

		if ( iRet != XGE_OK ) return iRet;
		return __xgeShapeExFlattenCubicAdaptive(
			pFlat, tP0123, tP123, tP23, tP3, fTolerance, iDepth + 1
		);
	}
}

static int __xgeShapeExQuadStepCountUniform(xge_vec2_t tP0, xge_vec2_t tP1, xge_vec2_t tP2, float fTolerance)
{
	float fLen = hypotf(tP1.fX - tP0.fX, tP1.fY - tP0.fY) +
	             hypotf(tP2.fX - tP1.fX, tP2.fY - tP1.fY);
	int iSteps = (int)ceilf(fLen / (fTolerance * 18.0f));

	if ( iSteps < 3 ) iSteps = 3;
	if ( iSteps > 64 ) iSteps = 64;
	return iSteps;
}

static xge_vec2_t __xgeShapeExQuadPointUniform(xge_vec2_t tP0, xge_vec2_t tP1, xge_vec2_t tP2, float t)
{
	float u = 1.0f - t;

	return (xge_vec2_t){
		(u * u * tP0.fX) + (2.0f * u * t * tP1.fX) + (t * t * tP2.fX),
		(u * u * tP0.fY) + (2.0f * u * t * tP1.fY) + (t * t * tP2.fY)
	};
}

static int __xgeShapeExCubicStepCountUniform(xge_vec2_t tP0, xge_vec2_t tP1, xge_vec2_t tP2, xge_vec2_t tP3, float fTolerance)
{
	float fLen = hypotf(tP1.fX - tP0.fX, tP1.fY - tP0.fY) +
	             hypotf(tP2.fX - tP1.fX, tP2.fY - tP1.fY) +
	             hypotf(tP3.fX - tP2.fX, tP3.fY - tP2.fY);
	int iSteps = (int)ceilf(fLen / (fTolerance * 18.0f));

	if ( iSteps < 4 ) iSteps = 4;
	if ( iSteps > 96 ) iSteps = 96;
	return iSteps;
}

static xge_vec2_t __xgeShapeExCubicPointUniform(xge_vec2_t tP0, xge_vec2_t tP1, xge_vec2_t tP2, xge_vec2_t tP3, float t)
{
	float u = 1.0f - t;

	return (xge_vec2_t){
		(u * u * u * tP0.fX) + (3.0f * u * u * t * tP1.fX) + (3.0f * u * t * t * tP2.fX) + (t * t * t * tP3.fX),
		(u * u * u * tP0.fY) + (3.0f * u * u * t * tP1.fY) + (3.0f * u * t * t * tP2.fY) + (t * t * t * tP3.fY)
	};
}

static int __xgeShapeExFlattenInternal(xge_shape_ex pShape, xge_shape_ex_matrix_t tMatrix, float fTolerance, int bAdaptive, xge_shape_ex_flat_path_t* pFlat)
{
	xge_vec2_t tCurrent;
	xge_vec2_t tStart;
	int bHasCurrent;
	int bHasStart;
	int iPointRead;
	int i;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || (pFlat == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fTolerance <= 0.0f ) {
		fTolerance = XGE_SHAPE_EX_DEFAULT_TOLERANCE;
	}
	memset(pFlat, 0, sizeof(*pFlat));
	memset(&tCurrent, 0, sizeof(tCurrent));
	memset(&tStart, 0, sizeof(tStart));
	bHasCurrent = 0;
	bHasStart = 0;
	iPointRead = 0;
	for ( i = 0; i < pShape->iCommandCount; i++ ) {
		uint8_t iCommand = pShape->pCommands[i];

		if ( iCommand == XGE_SHAPE_EX_CMD_MOVE_TO ) {
			tCurrent = __xgeShapeExMatrixPoint(tMatrix, pShape->pPoints[iPointRead++]);
			iRet = __xgeShapeExFlatBeginContour(pFlat, tCurrent);
			if ( iRet != XGE_OK ) {
				__xgeShapeExFlatFree(pFlat);
				return iRet;
			}
			tStart = tCurrent;
			bHasCurrent = 1;
			bHasStart = 1;
		} else if ( iCommand == XGE_SHAPE_EX_CMD_LINE_TO ) {
			xge_vec2_t tPrev = tCurrent;

			tCurrent = __xgeShapeExMatrixPoint(tMatrix, pShape->pPoints[iPointRead++]);
			if ( !bHasCurrent ) {
				iRet = __xgeShapeExFlatBeginContour(pFlat, tCurrent);
				if ( iRet != XGE_OK ) {
					__xgeShapeExFlatFree(pFlat);
					return iRet;
				}
				tStart = tCurrent;
				bHasCurrent = 1;
				bHasStart = 1;
			} else {
				iRet = __xgeShapeExFlatAddPoint(pFlat, tCurrent);
				if ( iRet != XGE_OK ) {
					__xgeShapeExFlatFree(pFlat);
					return iRet;
				}
				if ( !bHasStart ) {
					tStart = tPrev;
					bHasStart = 1;
				}
			}
		} else if ( (iCommand == XGE_SHAPE_EX_CMD_QUAD_TO) && bHasCurrent ) {
			xge_vec2_t tP0 = tCurrent;
			xge_vec2_t tP1 = __xgeShapeExMatrixPoint(tMatrix, pShape->pPoints[iPointRead++]);
			xge_vec2_t tP2 = __xgeShapeExMatrixPoint(tMatrix, pShape->pPoints[iPointRead++]);

			if ( !bHasStart ) {
				iRet = __xgeShapeExFlatBeginContour(pFlat, tP0);
				if ( iRet != XGE_OK ) {
					__xgeShapeExFlatFree(pFlat);
					return iRet;
				}
				tStart = tP0;
				bHasStart = 1;
			}
			if ( bAdaptive ) {
				iRet = __xgeShapeExFlattenQuadAdaptive(pFlat, tP0, tP1, tP2, fTolerance, 0);
			} else {
				int iSteps = __xgeShapeExQuadStepCountUniform(tP0, tP1, tP2, fTolerance);
				int j;

				iRet = XGE_OK;
				for ( j = 1; (iRet == XGE_OK) && (j <= iSteps); j++ ) {
					iRet = __xgeShapeExFlatAddPoint(
						pFlat, __xgeShapeExQuadPointUniform(tP0, tP1, tP2, (float)j / (float)iSteps)
					);
				}
			}
			if ( iRet != XGE_OK ) {
				__xgeShapeExFlatFree(pFlat);
				return iRet;
			}
			tCurrent = tP2;
		} else if ( iCommand == XGE_SHAPE_EX_CMD_CUBIC_TO ) {
			xge_vec2_t tP0 = tCurrent;
			xge_vec2_t tP1 = __xgeShapeExMatrixPoint(tMatrix, pShape->pPoints[iPointRead++]);
			xge_vec2_t tP2 = __xgeShapeExMatrixPoint(tMatrix, pShape->pPoints[iPointRead++]);
			xge_vec2_t tP3 = __xgeShapeExMatrixPoint(tMatrix, pShape->pPoints[iPointRead++]);

			if ( !bHasCurrent ) {
				tCurrent = tP3;
				bHasCurrent = 1;
				continue;
			}
			if ( !bHasStart ) {
				iRet = __xgeShapeExFlatBeginContour(pFlat, tP0);
				if ( iRet != XGE_OK ) {
					__xgeShapeExFlatFree(pFlat);
					return iRet;
				}
				tStart = tP0;
				bHasStart = 1;
			}
			if ( bAdaptive ) {
				iRet = __xgeShapeExFlattenCubicAdaptive(pFlat, tP0, tP1, tP2, tP3, fTolerance, 0);
			} else {
				int iSteps = __xgeShapeExCubicStepCountUniform(tP0, tP1, tP2, tP3, fTolerance);
				int j;

				iRet = XGE_OK;
				for ( j = 1; (iRet == XGE_OK) && (j <= iSteps); j++ ) {
					iRet = __xgeShapeExFlatAddPoint(
						pFlat, __xgeShapeExCubicPointUniform(tP0, tP1, tP2, tP3, (float)j / (float)iSteps)
					);
				}
			}
			if ( iRet != XGE_OK ) {
				__xgeShapeExFlatFree(pFlat);
				return iRet;
			}
			tCurrent = tP3;
		} else if ( iCommand == XGE_SHAPE_EX_CMD_CLOSE ) {
			if ( !bHasCurrent || !bHasStart ) {
				continue;
			}
			iRet = __xgeShapeExFlatAddPoint(pFlat, tStart);
			if ( iRet != XGE_OK ) {
				__xgeShapeExFlatFree(pFlat);
				return iRet;
			}
			__xgeShapeExFlatCloseContour(pFlat);
			tCurrent = tStart;
		} else {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
	}
	return XGE_OK;
}

static int __xgeShapeExFlatten(xge_shape_ex pShape, xge_shape_ex_matrix_t tMatrix, float fTolerance, xge_shape_ex_flat_path_t* pFlat)
{
	return __xgeShapeExFlattenInternal(pShape, tMatrix, fTolerance, 1, pFlat);
}

static int __xgeShapeExFlattenStroke(xge_shape_ex pShape, xge_shape_ex_matrix_t tMatrix, float fTolerance, xge_shape_ex_flat_path_t* pFlat)
{
	return __xgeShapeExFlattenInternal(pShape, tMatrix, fTolerance, 0, pFlat);
}

static void __xgeShapeExCommandPathShapeView(
	xge_shape_ex pSource,
	const xge_shape_ex_stroke_path_t* pPath,
	xge_shape_ex_t* pView
)
{
	*pView = *pSource;
	pView->pCommands = pPath->pCommands;
	pView->iCommandCount = pPath->iCommandCount;
	pView->iCommandCapacity = pPath->iCommandCount;
	pView->pPoints = pPath->pPoints;
	pView->iPointCount = pPath->iPointCount;
	pView->iPointCapacity = pPath->iPointCount;
	pView->bTrimPath = 0;
}

static int __xgeShapeExFlattenTrimmedPath(
	xge_shape_ex pShape,
	xge_shape_ex_matrix_t tMatrix,
	float fTolerance,
	int bAdaptive,
	xge_shape_ex_flat_path_t* pFlat
)
{
	xge_shape_ex_stroke_path_t tTrimPath;
	xge_shape_ex_t tTrimShape;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || (pFlat == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(&tTrimPath, 0, sizeof(tTrimPath));
	memset(&tTrimShape, 0, sizeof(tTrimShape));
	iRet = __xgeShapeExTrimPathBuild(
		&tTrimPath, pShape->pCommands, pShape->iCommandCount,
		pShape->pPoints, pShape->iPointCount, pShape->fTrimBegin,
		pShape->fTrimEnd, pShape->bTrimSimultaneous
	);
	if ( iRet == XGE_OK ) {
		__xgeShapeExCommandPathShapeView(pShape, &tTrimPath, &tTrimShape);
		iRet = __xgeShapeExFlattenInternal(
			&tTrimShape, tMatrix, fTolerance, bAdaptive, pFlat
		);
	}
	__xgeShapeExStrokePathFree(&tTrimPath);
	return iRet;
}

static float __xgeShapeExContourArea(const xge_vec2_t* pPoints, int iCount)
{
	float fArea;
	int i;

	fArea = 0.0f;
	for ( i = 0; i < iCount; i++ ) {
		const xge_vec2_t* pA = &pPoints[i];
		const xge_vec2_t* pB = &pPoints[(i + 1) % iCount];
		fArea += (pA->fX * pB->fY) - (pB->fX * pA->fY);
	}
	return fArea * 0.5f;
}

static float __xgeShapeExCross(xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC)
{
	return ((tB.fX - tA.fX) * (tC.fY - tA.fY)) - ((tB.fY - tA.fY) * (tC.fX - tA.fX));
}

static int __xgeShapeExPointInTriangle(xge_vec2_t tP, xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC)
{
	float fC0 = __xgeShapeExCross(tA, tB, tP);
	float fC1 = __xgeShapeExCross(tB, tC, tP);
	float fC2 = __xgeShapeExCross(tC, tA, tP);

	return ((fC0 >= -XGE_SHAPE_EX_EPSILON) && (fC1 >= -XGE_SHAPE_EX_EPSILON) && (fC2 >= -XGE_SHAPE_EX_EPSILON)) ||
	       ((fC0 <= XGE_SHAPE_EX_EPSILON) && (fC1 <= XGE_SHAPE_EX_EPSILON) && (fC2 <= XGE_SHAPE_EX_EPSILON));
}

static int __xgeShapeExPointInTriangleStrict(xge_vec2_t tP, xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC)
{
	float fC0 = __xgeShapeExCross(tA, tB, tP);
	float fC1 = __xgeShapeExCross(tB, tC, tP);
	float fC2 = __xgeShapeExCross(tC, tA, tP);

	return ((fC0 > XGE_SHAPE_EX_EPSILON) && (fC1 > XGE_SHAPE_EX_EPSILON) && (fC2 > XGE_SHAPE_EX_EPSILON)) ||
	       ((fC0 < -XGE_SHAPE_EX_EPSILON) && (fC1 < -XGE_SHAPE_EX_EPSILON) && (fC2 < -XGE_SHAPE_EX_EPSILON));
}

static int __xgeShapeExPointInContour(xge_vec2_t tP, const xge_vec2_t* pPoints, int iCount)
{
	int bInside;
	int i;
	int j;

	if ( (pPoints == NULL) || (iCount < 3) ) {
		return 0;
	}
	bInside = 0;
	j = iCount - 1;
	for ( i = 0; i < iCount; i++ ) {
		float fYi = pPoints[i].fY;
		float fYj = pPoints[j].fY;

		if ( ((fYi > tP.fY) != (fYj > tP.fY)) ) {
			float fX = ((pPoints[j].fX - pPoints[i].fX) * (tP.fY - fYi) / (fYj - fYi)) + pPoints[i].fX;
			if ( tP.fX < fX ) {
				bInside = !bInside;
			}
		}
		j = i;
	}
	return bInside;
}

static float __xgeShapeExDistanceSq(xge_vec2_t tA, xge_vec2_t tB)
{
	float fDX = tA.fX - tB.fX;
	float fDY = tA.fY - tB.fY;
	return (fDX * fDX) + (fDY * fDY);
}

static int __xgeShapeExOrientation(xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC)
{
	float fCross = __xgeShapeExCross(tA, tB, tC);

	if ( fCross > XGE_SHAPE_EX_EPSILON ) return 1;
	if ( fCross < -XGE_SHAPE_EX_EPSILON ) return -1;
	return 0;
}

static int __xgeShapeExOnSegment(xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tP)
{
	return (tP.fX >= fminf(tA.fX, tB.fX) - XGE_SHAPE_EX_EPSILON) &&
	       (tP.fX <= fmaxf(tA.fX, tB.fX) + XGE_SHAPE_EX_EPSILON) &&
	       (tP.fY >= fminf(tA.fY, tB.fY) - XGE_SHAPE_EX_EPSILON) &&
	       (tP.fY <= fmaxf(tA.fY, tB.fY) + XGE_SHAPE_EX_EPSILON) &&
	       (fabsf(__xgeShapeExCross(tA, tB, tP)) <= XGE_SHAPE_EX_EPSILON);
}

static int __xgeShapeExRectContainsPoint(xge_rect_t tRect, xge_vec2_t tPoint)
{
	return (tPoint.fX >= tRect.fX - XGE_SHAPE_EX_EPSILON) &&
	       (tPoint.fX <= tRect.fX + tRect.fW + XGE_SHAPE_EX_EPSILON) &&
	       (tPoint.fY >= tRect.fY - XGE_SHAPE_EX_EPSILON) &&
	       (tPoint.fY <= tRect.fY + tRect.fH + XGE_SHAPE_EX_EPSILON);
}

static int __xgeShapeExSegmentsIntersect(xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC, xge_vec2_t tD);
static int __xgeShapeExRectIntersects(xge_rect_t a, xge_rect_t b);

static void __xgeShapeExRectPoints(xge_rect_t tRect, xge_vec2_t pPoints[4])
{
	pPoints[0] = (xge_vec2_t){tRect.fX, tRect.fY};
	pPoints[1] = (xge_vec2_t){tRect.fX + tRect.fW, tRect.fY};
	pPoints[2] = (xge_vec2_t){tRect.fX + tRect.fW, tRect.fY + tRect.fH};
	pPoints[3] = (xge_vec2_t){tRect.fX, tRect.fY + tRect.fH};
}

static int __xgeShapeExSegmentIntersectsRect(xge_vec2_t tA, xge_vec2_t tB, xge_rect_t tRect)
{
	xge_vec2_t pRect[4];
	int i;

	if ( __xgeShapeExRectContainsPoint(tRect, tA) || __xgeShapeExRectContainsPoint(tRect, tB) ) return 1;
	__xgeShapeExRectPoints(tRect, pRect);
	for ( i = 0; i < 4; i++ ) {
		if ( __xgeShapeExSegmentsIntersect(tA, tB, pRect[i], pRect[(i + 1) & 3]) ) return 1;
	}
	return 0;
}

static int __xgeShapeExConvexPolygonIntersectsRectArea(const xge_vec2_t* pPoints, int iCount, xge_rect_t tRect)
{
	xge_vec2_t pRect[4];
	int iAxis;

	if ( (pPoints == NULL) || (iCount < 3) || (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) return 0;
	__xgeShapeExRectPoints(tRect, pRect);
	for ( iAxis = -2; iAxis < iCount; iAxis++ ) {
		float fAxisX;
		float fAxisY;
		float fPolyMin;
		float fPolyMax;
		float fRectMin;
		float fRectMax;
		int i;

		if ( iAxis == -2 ) {
			fAxisX = 1.0f;
			fAxisY = 0.0f;
		} else if ( iAxis == -1 ) {
			fAxisX = 0.0f;
			fAxisY = 1.0f;
		} else {
			xge_vec2_t tA = pPoints[iAxis];
			xge_vec2_t tB = pPoints[(iAxis + 1) % iCount];

			fAxisX = -(tB.fY - tA.fY);
			fAxisY = tB.fX - tA.fX;
			if ( fabsf(fAxisX) + fabsf(fAxisY) <= XGE_SHAPE_EX_EPSILON ) continue;
		}
		fPolyMin = fPolyMax = pPoints[0].fX * fAxisX + pPoints[0].fY * fAxisY;
		for ( i = 1; i < iCount; i++ ) {
			float fProjection = pPoints[i].fX * fAxisX + pPoints[i].fY * fAxisY;

			fPolyMin = fminf(fPolyMin, fProjection);
			fPolyMax = fmaxf(fPolyMax, fProjection);
		}
		fRectMin = fRectMax = pRect[0].fX * fAxisX + pRect[0].fY * fAxisY;
		for ( i = 1; i < 4; i++ ) {
			float fProjection = pRect[i].fX * fAxisX + pRect[i].fY * fAxisY;

			fRectMin = fminf(fRectMin, fProjection);
			fRectMax = fmaxf(fRectMax, fProjection);
		}
		if ( (fPolyMax <= fRectMin + XGE_SHAPE_EX_EPSILON) ||
		     (fRectMax <= fPolyMin + XGE_SHAPE_EX_EPSILON) ) return 0;
	}
	return 1;
}

static int __xgeShapeExCircleIntersectsRect(xge_vec2_t tCenter, float fRadius, xge_rect_t tRect)
{
	float fNearestX;
	float fNearestY;
	float fDX;
	float fDY;

	if ( fRadius <= XGE_SHAPE_EX_EPSILON ) return __xgeShapeExRectContainsPoint(tRect, tCenter);
	fNearestX = fmaxf(tRect.fX, fminf(tCenter.fX, tRect.fX + tRect.fW));
	fNearestY = fmaxf(tRect.fY, fminf(tCenter.fY, tRect.fY + tRect.fH));
	fDX = tCenter.fX - fNearestX;
	fDY = tCenter.fY - fNearestY;
	return (fDX * fDX + fDY * fDY) <= (fRadius * fRadius + XGE_SHAPE_EX_EPSILON);
}

static int __xgeShapeExFlatContainsPoint(const xge_shape_ex_flat_path_t* pFlat, int iFillRule, xge_vec2_t tPoint)
{
	int iWinding;
	int iCrossings;
	int i;

	if ( pFlat == NULL ) {
		return 0;
	}
	iWinding = 0;
	iCrossings = 0;
	for ( i = 0; i < pFlat->iContourCount; i++ ) {
		const xge_shape_ex_flat_contour_t* pContour = &pFlat->pContours[i];
		const xge_vec2_t* pPoints;
		int j;

		if ( (pContour->iCount < 3) || (pContour->iStart < 0) || ((pContour->iStart + pContour->iCount) > pFlat->iPointCount) ) {
			continue;
		}
		pPoints = pFlat->pPoints + pContour->iStart;
		for ( j = 0; j < pContour->iCount; j++ ) {
			xge_vec2_t tA = pPoints[j];
			xge_vec2_t tB = pPoints[(j + 1) % pContour->iCount];
			float fCross;

			if ( __xgeShapeExOnSegment(tA, tB, tPoint) ) {
				return 1;
			}
			if ( (tA.fY > tPoint.fY) == (tB.fY > tPoint.fY) ) {
				continue;
			}
			fCross = __xgeShapeExCross(tA, tB, tPoint);
			if ( iFillRule == XGE_SHAPE_EX_FILL_EVEN_ODD ) {
				float fX = ((tB.fX - tA.fX) * (tPoint.fY - tA.fY) / (tB.fY - tA.fY)) + tA.fX;

				if ( tPoint.fX < fX ) {
					iCrossings++;
				}
			} else if ( (tA.fY <= tPoint.fY) && (tB.fY > tPoint.fY) ) {
				if ( fCross > XGE_SHAPE_EX_EPSILON ) {
					iWinding++;
				}
			} else if ( (tA.fY > tPoint.fY) && (tB.fY <= tPoint.fY) ) {
				if ( fCross < -XGE_SHAPE_EX_EPSILON ) {
					iWinding--;
				}
			}
		}
	}
	return (iFillRule == XGE_SHAPE_EX_FILL_EVEN_ODD) ? ((iCrossings & 1) != 0) : (iWinding != 0);
}

static int __xgeShapeExSegmentsIntersect(xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC, xge_vec2_t tD)
{
	int iO1 = __xgeShapeExOrientation(tA, tB, tC);
	int iO2 = __xgeShapeExOrientation(tA, tB, tD);
	int iO3 = __xgeShapeExOrientation(tC, tD, tA);
	int iO4 = __xgeShapeExOrientation(tC, tD, tB);

	if ( (iO1 != iO2) && (iO3 != iO4) ) {
		return 1;
	}
	if ( (iO1 == 0) && __xgeShapeExOnSegment(tA, tB, tC) ) return 1;
	if ( (iO2 == 0) && __xgeShapeExOnSegment(tA, tB, tD) ) return 1;
	if ( (iO3 == 0) && __xgeShapeExOnSegment(tC, tD, tA) ) return 1;
	if ( (iO4 == 0) && __xgeShapeExOnSegment(tC, tD, tB) ) return 1;
	return 0;
}

static int __xgeShapeExSamePoint(xge_vec2_t tA, xge_vec2_t tB);

static int __xgeShapeExFlatHasIntersectingEdges(const xge_shape_ex_flat_path_t* pFlat)
{
	int i;

	if ( pFlat == NULL ) {
		return 0;
	}
	for ( i = 0; i < pFlat->iContourCount; i++ ) {
		const xge_shape_ex_flat_contour_t* pAContour = &pFlat->pContours[i];
		const xge_vec2_t* pAPoints = pFlat->pPoints + pAContour->iStart;
		int iACount = pAContour->iCount;
		int iA;

		while ( (iACount > 2) && __xgeShapeExSamePoint(pAPoints[iACount - 1], pAPoints[0]) ) {
			iACount--;
		}
		if ( iACount < 3 ) {
			continue;
		}
		for ( iA = 0; iA < iACount; iA++ ) {
			xge_vec2_t tA0 = pAPoints[iA];
			xge_vec2_t tA1 = pAPoints[(iA + 1) % iACount];
			int j;

			if ( __xgeShapeExSamePoint(tA0, tA1) ) {
				continue;
			}
			for ( j = i; j < pFlat->iContourCount; j++ ) {
				const xge_shape_ex_flat_contour_t* pBContour = &pFlat->pContours[j];
				const xge_vec2_t* pBPoints = pFlat->pPoints + pBContour->iStart;
				int iBCount = pBContour->iCount;
				int iBStart = (j == i) ? (iA + 1) : 0;
				int iB;

				while ( (iBCount > 2) && __xgeShapeExSamePoint(pBPoints[iBCount - 1], pBPoints[0]) ) {
					iBCount--;
				}
				for ( iB = iBStart; iB < iBCount; iB++ ) {
					xge_vec2_t tB0;
					xge_vec2_t tB1;

					if ( (j == i) && ((iB == iA) || (iB == ((iA + 1) % iACount)) || (((iB + 1) % iACount) == iA)) ) {
						continue;
					}
					tB0 = pBPoints[iB];
					tB1 = pBPoints[(iB + 1) % iBCount];
					if ( __xgeShapeExSamePoint(tB0, tB1) ) {
						continue;
					}
					if ( __xgeShapeExSegmentsIntersect(tA0, tA1, tB0, tB1) ) {
						return 1;
					}
				}
			}
		}
	}
	return 0;
}

static int __xgeShapeExSamePoint(xge_vec2_t tA, xge_vec2_t tB)
{
	return (fabsf(tA.fX - tB.fX) <= XGE_SHAPE_EX_EPSILON) &&
	       (fabsf(tA.fY - tB.fY) <= XGE_SHAPE_EX_EPSILON);
}

static int __xgeShapeExTriangulateContour(const xge_vec2_t* pPoints, int iCount, xge_shape_vertex_t** ppVertices, int* pVertexCount, uint32_t** ppIndices, int* pIndexCount, uint32_t iColor)
{
	xge_shape_vertex_t* pVertices;
	uint32_t* pIndices;
	int* pOrder;
	int iWorkCount;
	int iWrite;
	int iGuard;
	float fArea;
	int i;

	if ( (pPoints == NULL) || (iCount < 3) || (ppVertices == NULL) || (pVertexCount == NULL) || (ppIndices == NULL) || (pIndexCount == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	while ( (iCount > 2) &&
	        (fabsf(pPoints[iCount - 1].fX - pPoints[0].fX) <= XGE_SHAPE_EX_EPSILON) &&
	        (fabsf(pPoints[iCount - 1].fY - pPoints[0].fY) <= XGE_SHAPE_EX_EPSILON) ) {
		iCount--;
	}
	if ( iCount < 3 ) {
		*ppVertices = NULL;
		*pVertexCount = 0;
		*ppIndices = NULL;
		*pIndexCount = 0;
		return XGE_OK;
	}
	pVertices = (xge_shape_vertex_t*)xrtMalloc((size_t)iCount * sizeof(*pVertices));
	pIndices = (uint32_t*)xrtMalloc((size_t)(iCount - 2) * 3u * sizeof(*pIndices));
	pOrder = (int*)xrtMalloc((size_t)iCount * sizeof(*pOrder));
	if ( (pVertices == NULL) || (pIndices == NULL) || (pOrder == NULL) ) {
		xrtFree(pVertices);
		xrtFree(pIndices);
		xrtFree(pOrder);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	for ( i = 0; i < iCount; i++ ) {
		pVertices[i].fX = pPoints[i].fX;
		pVertices[i].fY = pPoints[i].fY;
		pVertices[i].iColor = iColor;
		pOrder[i] = i;
	}
	fArea = __xgeShapeExContourArea(pPoints, iCount);
	iWorkCount = iCount;
	iWrite = 0;
	iGuard = 0;
	while ( iWorkCount > 2 && iGuard < iCount * iCount ) {
		int bCut = 0;

		for ( i = 0; i < iWorkCount; i++ ) {
			int iPrev = pOrder[(i + iWorkCount - 1) % iWorkCount];
			int iCurr = pOrder[i];
			int iNext = pOrder[(i + 1) % iWorkCount];
			float fCross = __xgeShapeExCross(pPoints[iPrev], pPoints[iCurr], pPoints[iNext]);
			int bConvex = (fArea >= 0.0f) ? (fCross > XGE_SHAPE_EX_EPSILON) : (fCross < -XGE_SHAPE_EX_EPSILON);
			int bContains = 0;
			int j;

			if ( !bConvex ) {
				continue;
			}
			for ( j = 0; j < iWorkCount; j++ ) {
				int iTest = pOrder[j];
				if ( (iTest == iPrev) || (iTest == iCurr) || (iTest == iNext) ) {
					continue;
				}
				if ( __xgeShapeExPointInTriangleStrict(pPoints[iTest], pPoints[iPrev], pPoints[iCurr], pPoints[iNext]) ) {
					bContains = 1;
					break;
				}
			}
			if ( bContains ) {
				continue;
			}
			if ( fArea >= 0.0f ) {
				pIndices[iWrite++] = (uint32_t)iPrev;
				pIndices[iWrite++] = (uint32_t)iCurr;
				pIndices[iWrite++] = (uint32_t)iNext;
			} else {
				pIndices[iWrite++] = (uint32_t)iNext;
				pIndices[iWrite++] = (uint32_t)iCurr;
				pIndices[iWrite++] = (uint32_t)iPrev;
			}
			memmove(&pOrder[i], &pOrder[i + 1], (size_t)(iWorkCount - i - 1) * sizeof(*pOrder));
			iWorkCount--;
			bCut = 1;
			break;
		}
		if ( !bCut ) {
			break;
		}
		iGuard++;
	}
	xrtFree(pOrder);
	if ( iWrite <= 0 ) {
		xrtFree(pVertices);
		xrtFree(pIndices);
		*ppVertices = NULL;
		*pVertexCount = 0;
		*ppIndices = NULL;
		*pIndexCount = 0;
		return XGE_OK;
	}
	*ppVertices = pVertices;
	*pVertexCount = iCount;
	*ppIndices = pIndices;
	*pIndexCount = iWrite;
	return XGE_OK;
}

static int __xgeShapeExSubdivideTriangles(xge_shape_vertex_t** ppVertices, int* pVertexCount, uint32_t** ppIndices, int* pIndexCount)
{
	xge_shape_vertex_t* pVertices;
	xge_shape_vertex_t* pNewVertices;
	uint32_t* pIndices;
	uint32_t* pNewIndices;
	int iVertexCount;
	int iIndexCount;
	int iTriangleCount;
	int iNewVertexCount;
	int iNewIndexCount;
	int i;

	if ( (ppVertices == NULL) || (pVertexCount == NULL) || (ppIndices == NULL) || (pIndexCount == NULL) ||
	     (*ppVertices == NULL) || (*ppIndices == NULL) || (*pVertexCount <= 0) || (*pIndexCount <= 0) || ((*pIndexCount % 3) != 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pVertices = *ppVertices;
	pIndices = *ppIndices;
	iVertexCount = *pVertexCount;
	iIndexCount = *pIndexCount;
	iTriangleCount = iIndexCount / 3;
	if ( (iTriangleCount <= 0) || (iTriangleCount > ((INT_MAX - iVertexCount) / 3)) || (iTriangleCount > (INT_MAX / 12)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iNewVertexCount = iVertexCount + (iTriangleCount * 3);
	iNewIndexCount = iTriangleCount * 12;
	pNewVertices = (xge_shape_vertex_t*)xrtMalloc((size_t)iNewVertexCount * sizeof(*pNewVertices));
	pNewIndices = (uint32_t*)xrtMalloc((size_t)iNewIndexCount * sizeof(*pNewIndices));
	if ( (pNewVertices == NULL) || (pNewIndices == NULL) ) {
		xrtFree(pNewVertices);
		xrtFree(pNewIndices);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	memcpy(pNewVertices, pVertices, (size_t)iVertexCount * sizeof(*pNewVertices));
	for ( i = 0; i < iTriangleCount; i++ ) {
		uint32_t iA = pIndices[i * 3 + 0];
		uint32_t iB = pIndices[i * 3 + 1];
		uint32_t iC = pIndices[i * 3 + 2];
		uint32_t iAB = (uint32_t)(iVertexCount + (i * 3));
		uint32_t iBC = iAB + 1u;
		uint32_t iCA = iAB + 2u;
		int iOut = i * 12;

		if ( (iA >= (uint32_t)iVertexCount) || (iB >= (uint32_t)iVertexCount) || (iC >= (uint32_t)iVertexCount) ) {
			xrtFree(pNewVertices);
			xrtFree(pNewIndices);
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		pNewVertices[iAB].fX = (pVertices[iA].fX + pVertices[iB].fX) * 0.5f;
		pNewVertices[iAB].fY = (pVertices[iA].fY + pVertices[iB].fY) * 0.5f;
		pNewVertices[iAB].iColor = __xgeShapeExColorLerp(pVertices[iA].iColor, pVertices[iB].iColor, 0.5f);
		pNewVertices[iBC].fX = (pVertices[iB].fX + pVertices[iC].fX) * 0.5f;
		pNewVertices[iBC].fY = (pVertices[iB].fY + pVertices[iC].fY) * 0.5f;
		pNewVertices[iBC].iColor = __xgeShapeExColorLerp(pVertices[iB].iColor, pVertices[iC].iColor, 0.5f);
		pNewVertices[iCA].fX = (pVertices[iC].fX + pVertices[iA].fX) * 0.5f;
		pNewVertices[iCA].fY = (pVertices[iC].fY + pVertices[iA].fY) * 0.5f;
		pNewVertices[iCA].iColor = __xgeShapeExColorLerp(pVertices[iC].iColor, pVertices[iA].iColor, 0.5f);
		pNewIndices[iOut + 0] = iA;
		pNewIndices[iOut + 1] = iAB;
		pNewIndices[iOut + 2] = iCA;
		pNewIndices[iOut + 3] = iAB;
		pNewIndices[iOut + 4] = iB;
		pNewIndices[iOut + 5] = iBC;
		pNewIndices[iOut + 6] = iCA;
		pNewIndices[iOut + 7] = iBC;
		pNewIndices[iOut + 8] = iC;
		pNewIndices[iOut + 9] = iAB;
		pNewIndices[iOut + 10] = iBC;
		pNewIndices[iOut + 11] = iCA;
	}
	xrtFree(pVertices);
	xrtFree(pIndices);
	*ppVertices = pNewVertices;
	*pVertexCount = iNewVertexCount;
	*ppIndices = pNewIndices;
	*pIndexCount = iNewIndexCount;
	return XGE_OK;
}

static xge_vec2_t __xgeShapeExRadialGradientCenterPoint(const xge_shape_ex pShape, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix)
{
	xge_vec2_t tCenter;

	if ( (pShape != NULL) && (pShape->iFillGradientUnits == XGE_SHAPE_EX_GRADIENT_USER_SPACE) ) {
		tCenter.fX = pShape->fFillX1;
		tCenter.fY = pShape->fFillY1;
		tCenter = __xgeShapeExMatrixPoint(pShape->tFillGradientTransform, tCenter);
		return __xgeShapeExMatrixPoint(tMatrix, tCenter);
	}
	tCenter.fX = tBounds.fX + ((pShape != NULL ? pShape->fFillX1 : 0.5f) * tBounds.fW);
	tCenter.fY = tBounds.fY + ((pShape != NULL ? pShape->fFillY1 : 0.5f) * tBounds.fH);
	if ( pShape != NULL ) {
		tCenter.fX = pShape->fFillX1;
		tCenter.fY = pShape->fFillY1;
		tCenter = __xgeShapeExMatrixPoint(pShape->tFillGradientTransform, tCenter);
		tCenter.fX = tBounds.fX + (tCenter.fX * tBounds.fW);
		tCenter.fY = tBounds.fY + (tCenter.fY * tBounds.fH);
	}
	return tCenter;
}

static int __xgeShapeExLinearGradientSubdivisionPasses(const xge_shape_ex_color_stop_t* pStops, int iStopCount, int iSpread)
{
	int i;

	if ( iSpread != XGE_SHAPE_EX_GRADIENT_SPREAD_PAD ) {
		return 6;
	}
	for ( i = 1; (pStops != NULL) && (i < iStopCount); i++ ) {
		if ( fabsf(pStops[i].fOffset - pStops[i - 1].fOffset) <= XGE_SHAPE_EX_EPSILON ) {
			return 7;
		}
	}
	if ( iStopCount > 4 ) {
		return 5;
	}
	if ( iStopCount > 2 ) {
		return 4;
	}
	return 0;
}

static int __xgeShapeExLinearGradientAdaptivePasses(int iPassCount, int iSpread, float fX1, float fY1, float fX2, float fY2, int iUnits, xge_shape_ex_matrix_t tGradientTransform, const xge_shape_vertex_t* pVertices, int iVertexCount, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix)
{
	float fDX;
	float fDY;
	float fLenSq;
	float fMinT;
	float fMaxT;
	int iRequiredSegments;
	int iSegments;
	int i;

	if ( (iSpread != XGE_SHAPE_EX_GRADIENT_SPREAD_REPEAT) || (pVertices == NULL) || (iVertexCount <= 0) ) {
		return iPassCount;
	}
	fDX = fX2 - fX1;
	fDY = fY2 - fY1;
	fLenSq = (fDX * fDX) + (fDY * fDY);
	if ( fLenSq <= XGE_SHAPE_EX_EPSILON ) return iPassCount;
	fMinT = FLT_MAX;
	fMaxT = -FLT_MAX;
	for ( i = 0; i < iVertexCount; i++ ) {
		xge_vec2_t tPoint;
		xge_vec2_t tSample;
		float fT;
		int bOk;

		tPoint.fX = pVertices[i].fX;
		tPoint.fY = pVertices[i].fY;
		tSample = __xgeShapeExGradientSamplePoint(iUnits, tGradientTransform, tPoint, tBounds, tMatrix, &bOk);
		if ( !bOk ) continue;
		fT = (((tSample.fX - fX1) * fDX) + ((tSample.fY - fY1) * fDY)) / fLenSq;
		if ( fT < fMinT ) fMinT = fT;
		if ( fT > fMaxT ) fMaxT = fT;
	}
	if ( (fMinT == FLT_MAX) || (fMaxT <= fMinT) ) return iPassCount;
	iRequiredSegments = (int)ceilf((fMaxT - fMinT) * 6.0f);
	iSegments = 1 << iPassCount;
	while ( (iPassCount < 7) && (iSegments < iRequiredSegments) ) {
		iPassCount++;
		iSegments <<= 1;
	}
	return iPassCount;
}

static int __xgeShapeExRadialGradientCorrectionPasses(float fCX, float fCY, float fRadius, float fFX, float fFY, float fFocalRadius)
{
	float fDX;
	float fDY;
	float fDist;
	float fMaxFR;

	if ( fRadius < 0.02f ) {
		return 0;
	}
	fDX = fCX - fFX;
	fDY = fCY - fFY;
	fDist = sqrtf((fDX * fDX) + (fDY * fDY));
	if ( (fRadius - fDist) < 0.02f ) {
		return 1;
	}
	fMaxFR = (fRadius - fDist) * 0.98f;
	return fFocalRadius > fMaxFR ? 1 : 0;
}

static int __xgeShapeExContourConvex(const xge_vec2_t* pPoints, int iCount)
{
	int iSign;
	int i;

	if ( (pPoints == NULL) || (iCount < 3) ) {
		return 0;
	}
	iSign = 0;
	for ( i = 0; i < iCount; i++ ) {
		xge_vec2_t tA = pPoints[i];
		xge_vec2_t tB = pPoints[(i + 1) % iCount];
		xge_vec2_t tC = pPoints[(i + 2) % iCount];
		float fCross = __xgeShapeExCross(tA, tB, tC);

		if ( fabsf(fCross) <= XGE_SHAPE_EX_EPSILON ) {
			continue;
		}
		if ( iSign == 0 ) {
			iSign = fCross > 0.0f ? 1 : -1;
		} else if ( ((fCross > 0.0f) ? 1 : -1) != iSign ) {
			return 0;
		}
	}
	return iSign != 0;
}

static int __xgeShapeExBuildCenterFan(const xge_vec2_t* pPoints, int iCount, xge_vec2_t tCenter, uint32_t iColor, xge_shape_vertex_t** ppVertices, int* pVertexCount, uint32_t** ppIndices, int* pIndexCount)
{
	xge_shape_vertex_t* pVertices;
	uint32_t* pIndices;
	float fArea;
	int i;

	if ( (pPoints == NULL) || (ppVertices == NULL) || (pVertexCount == NULL) || (ppIndices == NULL) || (pIndexCount == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	while ( (iCount > 2) && __xgeShapeExSamePoint(pPoints[iCount - 1], pPoints[0]) ) {
		iCount--;
	}
	if ( (iCount < 3) || !__xgeShapeExContourConvex(pPoints, iCount) || !__xgeShapeExPointInContour(tCenter, pPoints, iCount) ) {
		*ppVertices = NULL;
		*pVertexCount = 0;
		*ppIndices = NULL;
		*pIndexCount = 0;
		return XGE_OK;
	}
	pVertices = (xge_shape_vertex_t*)xrtMalloc((size_t)(iCount + 1) * sizeof(*pVertices));
	pIndices = (uint32_t*)xrtMalloc((size_t)iCount * 3u * sizeof(*pIndices));
	if ( (pVertices == NULL) || (pIndices == NULL) ) {
		xrtFree(pVertices);
		xrtFree(pIndices);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pVertices[0].fX = tCenter.fX;
	pVertices[0].fY = tCenter.fY;
	pVertices[0].iColor = iColor;
	for ( i = 0; i < iCount; i++ ) {
		pVertices[i + 1].fX = pPoints[i].fX;
		pVertices[i + 1].fY = pPoints[i].fY;
		pVertices[i + 1].iColor = iColor;
	}
	fArea = __xgeShapeExContourArea(pPoints, iCount);
	for ( i = 0; i < iCount; i++ ) {
		uint32_t iA = (uint32_t)(i + 1);
		uint32_t iB = (uint32_t)(((i + 1) % iCount) + 1);

		pIndices[i * 3 + 0] = 0;
		if ( fArea >= 0.0f ) {
			pIndices[i * 3 + 1] = iA;
			pIndices[i * 3 + 2] = iB;
		} else {
			pIndices[i * 3 + 1] = iB;
			pIndices[i * 3 + 2] = iA;
		}
	}
	*ppVertices = pVertices;
	*pVertexCount = iCount + 1;
	*ppIndices = pIndices;
	*pIndexCount = iCount * 3;
	return XGE_OK;
}

static xge_rect_t __xgeShapeExFlatBounds(const xge_shape_ex_flat_path_t* pFlat)
{
	xge_rect_t tBounds;
	int i;

	memset(&tBounds, 0, sizeof(tBounds));
	if ( (pFlat == NULL) || (pFlat->iPointCount <= 0) ) {
		return tBounds;
	}
	tBounds.fX = pFlat->pPoints[0].fX;
	tBounds.fY = pFlat->pPoints[0].fY;
	tBounds.fW = pFlat->pPoints[0].fX;
	tBounds.fH = pFlat->pPoints[0].fY;
	for ( i = 1; i < pFlat->iPointCount; i++ ) {
		if ( pFlat->pPoints[i].fX < tBounds.fX ) tBounds.fX = pFlat->pPoints[i].fX;
		if ( pFlat->pPoints[i].fY < tBounds.fY ) tBounds.fY = pFlat->pPoints[i].fY;
		if ( pFlat->pPoints[i].fX > tBounds.fW ) tBounds.fW = pFlat->pPoints[i].fX;
		if ( pFlat->pPoints[i].fY > tBounds.fH ) tBounds.fH = pFlat->pPoints[i].fY;
	}
	tBounds.fW -= tBounds.fX;
	tBounds.fH -= tBounds.fY;
	if ( tBounds.fW < 0.0f ) tBounds.fW = 0.0f;
	if ( tBounds.fH < 0.0f ) tBounds.fH = 0.0f;
	return tBounds;
}

static xge_rect_t __xgeShapeExFlatMappedBounds(
	const xge_shape_ex_flat_path_t* pFlat,
	xge_shape_ex_matrix_t tMatrix
)
{
	xge_rect_t tBounds;
	xge_vec2_t tPoint;
	float fRight;
	float fBottom;
	int i;

	memset(&tBounds, 0, sizeof(tBounds));
	if ( (pFlat == NULL) || (pFlat->iPointCount <= 0) ) {
		return tBounds;
	}
	tPoint = __xgeShapeExMatrixPoint(tMatrix, pFlat->pPoints[0]);
	tBounds.fX = tPoint.fX;
	tBounds.fY = tPoint.fY;
	fRight = tPoint.fX;
	fBottom = tPoint.fY;
	for ( i = 1; i < pFlat->iPointCount; i++ ) {
		tPoint = __xgeShapeExMatrixPoint(tMatrix, pFlat->pPoints[i]);
		if ( tPoint.fX < tBounds.fX ) tBounds.fX = tPoint.fX;
		if ( tPoint.fY < tBounds.fY ) tBounds.fY = tPoint.fY;
		if ( tPoint.fX > fRight ) fRight = tPoint.fX;
		if ( tPoint.fY > fBottom ) fBottom = tPoint.fY;
	}
	tBounds.fW = fRight - tBounds.fX;
	tBounds.fH = fBottom - tBounds.fY;
	return tBounds;
}

static float __xgeShapeExFlatLength(const xge_shape_ex_flat_path_t* pFlat)
{
	float fLength;
	int i;

	if ( pFlat == NULL ) {
		return 0.0f;
	}
	fLength = 0.0f;
	for ( i = 0; i < pFlat->iContourCount; i++ ) {
		const xge_shape_ex_flat_contour_t* pContour = &pFlat->pContours[i];
		const xge_vec2_t* pPoints;
		int j;

		if ( (pContour->iCount <= 1) || (pContour->iStart < 0) || ((pContour->iStart + pContour->iCount) > pFlat->iPointCount) ) {
			continue;
		}
		pPoints = pFlat->pPoints + pContour->iStart;
		for ( j = 1; j < pContour->iCount; j++ ) {
			fLength += hypotf(pPoints[j].fX - pPoints[j - 1].fX, pPoints[j].fY - pPoints[j - 1].fY);
		}
		if ( pContour->bClosed ) {
			fLength += hypotf(pPoints[0].fX - pPoints[pContour->iCount - 1].fX, pPoints[0].fY - pPoints[pContour->iCount - 1].fY);
		}
	}
	return fLength;
}

static void __xgeShapeExSetSampleResult(xge_vec2_t tPoint, xge_vec2_t tTangent, xge_vec2_t* pPoint, xge_vec2_t* pTangent)
{
	float fLen;

	if ( pPoint != NULL ) {
		*pPoint = tPoint;
	}
	if ( pTangent == NULL ) {
		return;
	}
	fLen = hypotf(tTangent.fX, tTangent.fY);
	if ( fLen <= XGE_SHAPE_EX_EPSILON ) {
		pTangent->fX = 1.0f;
		pTangent->fY = 0.0f;
		return;
	}
	pTangent->fX = tTangent.fX / fLen;
	pTangent->fY = tTangent.fY / fLen;
}

static int __xgeShapeExFlatPointAtLength(const xge_shape_ex_flat_path_t* pFlat, float fDistance, xge_vec2_t* pPoint, xge_vec2_t* pTangent)
{
	xge_vec2_t tFirst;
	xge_vec2_t tLast;
	xge_vec2_t tLastTangent;
	float fRemaining;
	int bHaveFirst;
	int bHaveLast;
	int i;

	if ( pFlat == NULL ) {
		return 0;
	}
	if ( fDistance < 0.0f ) {
		fDistance = 0.0f;
	}
	memset(&tFirst, 0, sizeof(tFirst));
	memset(&tLast, 0, sizeof(tLast));
	tLastTangent.fX = 1.0f;
	tLastTangent.fY = 0.0f;
	fRemaining = fDistance;
	bHaveFirst = 0;
	bHaveLast = 0;
	for ( i = 0; i < pFlat->iContourCount; i++ ) {
		const xge_shape_ex_flat_contour_t* pContour = &pFlat->pContours[i];
		const xge_vec2_t* pPoints;
		int j;

		if ( (pContour->iCount <= 0) || (pContour->iStart < 0) || ((pContour->iStart + pContour->iCount) > pFlat->iPointCount) ) {
			continue;
		}
		pPoints = pFlat->pPoints + pContour->iStart;
		if ( !bHaveFirst ) {
			tFirst = pPoints[0];
			tLast = tFirst;
			bHaveFirst = 1;
		}
		if ( pContour->iCount <= 1 ) {
			continue;
		}
		for ( j = 1; j < pContour->iCount; j++ ) {
			xge_vec2_t tA = pPoints[j - 1];
			xge_vec2_t tB = pPoints[j];
			xge_vec2_t tDelta;
			float fSegment;

			tDelta.fX = tB.fX - tA.fX;
			tDelta.fY = tB.fY - tA.fY;
			fSegment = hypotf(tDelta.fX, tDelta.fY);
			if ( fSegment <= XGE_SHAPE_EX_EPSILON ) {
				continue;
			}
			if ( fRemaining <= fSegment ) {
				float fT = fRemaining / fSegment;
				xge_vec2_t tPoint;

				tPoint.fX = tA.fX + tDelta.fX * fT;
				tPoint.fY = tA.fY + tDelta.fY * fT;
				__xgeShapeExSetSampleResult(tPoint, tDelta, pPoint, pTangent);
				return 1;
			}
			fRemaining -= fSegment;
			tLast = tB;
			tLastTangent = tDelta;
			bHaveLast = 1;
		}
		if ( pContour->bClosed ) {
			xge_vec2_t tA = pPoints[pContour->iCount - 1];
			xge_vec2_t tB = pPoints[0];
			xge_vec2_t tDelta;
			float fSegment;

			tDelta.fX = tB.fX - tA.fX;
			tDelta.fY = tB.fY - tA.fY;
			fSegment = hypotf(tDelta.fX, tDelta.fY);
			if ( fSegment > XGE_SHAPE_EX_EPSILON ) {
				if ( fRemaining <= fSegment ) {
					float fT = fRemaining / fSegment;
					xge_vec2_t tPoint;

					tPoint.fX = tA.fX + tDelta.fX * fT;
					tPoint.fY = tA.fY + tDelta.fY * fT;
					__xgeShapeExSetSampleResult(tPoint, tDelta, pPoint, pTangent);
					return 1;
				}
				fRemaining -= fSegment;
				tLast = tB;
				tLastTangent = tDelta;
				bHaveLast = 1;
			}
		}
	}
	if ( bHaveLast ) {
		__xgeShapeExSetSampleResult(tLast, tLastTangent, pPoint, pTangent);
		return 1;
	}
	if ( bHaveFirst ) {
		xge_vec2_t tTangent;

		tTangent.fX = 1.0f;
		tTangent.fY = 0.0f;
		__xgeShapeExSetSampleResult(tFirst, tTangent, pPoint, pTangent);
		return 1;
	}
	return 0;
}

static xge_vec2_t __xgeShapeExLerpPoint(xge_vec2_t tA, xge_vec2_t tB, float fT)
{
	xge_vec2_t tOut;

	if ( fT < 0.0f ) fT = 0.0f;
	if ( fT > 1.0f ) fT = 1.0f;
	tOut.fX = tA.fX + (tB.fX - tA.fX) * fT;
	tOut.fY = tA.fY + (tB.fY - tA.fY) * fT;
	return tOut;
}

static float __xgeShapeExFlatContourLength(const xge_shape_ex_flat_path_t* pFlat, const xge_shape_ex_flat_contour_t* pContour)
{
	const xge_vec2_t* pPoints;
	float fLength;
	int iSegmentCount;
	int i;

	if ( (pFlat == NULL) || (pContour == NULL) || (pContour->iCount <= 1) ||
	     (pContour->iStart < 0) || ((pContour->iStart + pContour->iCount) > pFlat->iPointCount) ) {
		return 0.0f;
	}
	pPoints = pFlat->pPoints + pContour->iStart;
	iSegmentCount = pContour->bClosed ? pContour->iCount : (pContour->iCount - 1);
	fLength = 0.0f;
	for ( i = 0; i < iSegmentCount; i++ ) {
		xge_vec2_t tA = pPoints[i];
		xge_vec2_t tB = pPoints[(i + 1) % pContour->iCount];

		fLength += hypotf(tB.fX - tA.fX, tB.fY - tA.fY);
	}
	return fLength;
}

static int __xgeShapeExFlatAddTrimmedContourRange(const xge_shape_ex_flat_path_t* pFlat, int iContour, float fStartDistance, float fEndDistance, xge_shape_ex_flat_path_t* pOut)
{
	const xge_shape_ex_flat_contour_t* pContour;
	const xge_vec2_t* pPoints;
	float fPos;
	int iSegmentCount;
	int bStarted;
	int i;

	if ( (pFlat == NULL) || (pOut == NULL) || (iContour < 0) || (iContour >= pFlat->iContourCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fEndDistance <= fStartDistance + XGE_SHAPE_EX_EPSILON ) {
		return XGE_OK;
	}
	pContour = &pFlat->pContours[iContour];
	if ( (pContour->iCount <= 1) || (pContour->iStart < 0) || ((pContour->iStart + pContour->iCount) > pFlat->iPointCount) ) {
		return XGE_OK;
	}
	pPoints = pFlat->pPoints + pContour->iStart;
	iSegmentCount = pContour->bClosed ? pContour->iCount : (pContour->iCount - 1);
	fPos = 0.0f;
	bStarted = 0;
	for ( i = 0; i < iSegmentCount; i++ ) {
		xge_vec2_t tA = pPoints[i];
		xge_vec2_t tB = pPoints[(i + 1) % pContour->iCount];
		float fSegment = hypotf(tB.fX - tA.fX, tB.fY - tA.fY);
		float fSegmentEnd;
		float fOverlapStart;
		float fOverlapEnd;
		xge_vec2_t tStart;
		xge_vec2_t tEnd;
		int iRet;

		if ( fSegment <= XGE_SHAPE_EX_EPSILON ) {
			continue;
		}
		fSegmentEnd = fPos + fSegment;
		if ( fSegmentEnd <= fStartDistance + XGE_SHAPE_EX_EPSILON ) {
			fPos = fSegmentEnd;
			continue;
		}
		if ( fPos >= fEndDistance - XGE_SHAPE_EX_EPSILON ) {
			break;
		}
		fOverlapStart = fmaxf(fStartDistance, fPos);
		fOverlapEnd = fminf(fEndDistance, fSegmentEnd);
		if ( fOverlapEnd <= fOverlapStart + XGE_SHAPE_EX_EPSILON ) {
			fPos = fSegmentEnd;
			continue;
		}
		tStart = __xgeShapeExLerpPoint(tA, tB, (fOverlapStart - fPos) / fSegment);
		tEnd = __xgeShapeExLerpPoint(tA, tB, (fOverlapEnd - fPos) / fSegment);
		if ( !bStarted ) {
			iRet = __xgeShapeExFlatBeginContour(pOut, tStart);
			if ( iRet != XGE_OK ) {
				return iRet;
			}
			bStarted = 1;
		}
		iRet = __xgeShapeExFlatAddPoint(pOut, tEnd);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		fPos = fSegmentEnd;
	}
	return XGE_OK;
}

static void __xgeShapeExAlphaMaskEncodeVertices(
	xge_shape_vertex_t* pVertices,
	int iVertexCount,
	const uint8_t* pCoverage
)
{
	int i;

	if ( (g_xgeShapeExAlphaMaskTargetDepth <= 0) || (pVertices == NULL) ||
	     (iVertexCount <= 0) ) return;
	for ( i = 0; i < iVertexCount; i++ ) {
		int iAlpha = XGE_COLOR_GET_A(pVertices[i].iColor);
		int iCoverage = pCoverage != NULL ? (int)pCoverage[i] : 255;

		pVertices[i].iColor = XGE_COLOR_RGBA(iCoverage, iCoverage, iCoverage, iAlpha);
	}
}

static uint8_t* __xgeShapeExCoverageFromVertexAlpha(
	const xge_shape_vertex_t* pVertices,
	int iVertexCount,
	int iBaseAlpha
)
{
	uint8_t* pCoverage;
	int i;

	if ( (g_xgeShapeExAlphaMaskTargetDepth <= 0) || (pVertices == NULL) ||
	     (iVertexCount <= 0) || (iBaseAlpha <= 0) ) return NULL;
	pCoverage = (uint8_t*)xrtMalloc((size_t)iVertexCount * sizeof(*pCoverage));
	if ( pCoverage == NULL ) return NULL;
	for ( i = 0; i < iVertexCount; i++ ) {
		int iAlpha = XGE_COLOR_GET_A(pVertices[i].iColor);
		int iCoverage = (iAlpha * 255 + iBaseAlpha / 2) / iBaseAlpha;

		if ( iCoverage > 255 ) iCoverage = 255;
		pCoverage[i] = (uint8_t)iCoverage;
	}
	return pCoverage;
}

static void __xgeShapeExApplyFillPaint(xge_shape_ex pShape, xge_shape_vertex_t* pVertices, int iVertexCount, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, uint32_t iFallbackColor, float fOpacity)
{
	int i;

	if ( (pShape == NULL) || (pVertices == NULL) || (iVertexCount <= 0) ) {
		return;
	}
	if ( (pShape->iFillType != XGE_SHAPE_EX_FILL_LINEAR_GRADIENT) && (pShape->iFillType != XGE_SHAPE_EX_FILL_RADIAL_GRADIENT) ) {
		for ( i = 0; i < iVertexCount; i++ ) {
			pVertices[i].iColor = iFallbackColor;
		}
		return;
	}
	for ( i = 0; i < iVertexCount; i++ ) {
		xge_vec2_t tPoint;

		tPoint.fX = pVertices[i].fX;
		tPoint.fY = pVertices[i].fY;
		if ( pShape->iFillType == XGE_SHAPE_EX_FILL_RADIAL_GRADIENT ) {
			pVertices[i].iColor = __xgeShapeExColorOpacity(__xgeShapeExRadialGradientColor(pShape, tPoint, tBounds, tMatrix), fOpacity);
		} else {
			pVertices[i].iColor = __xgeShapeExColorOpacity(__xgeShapeExLinearGradientColor(pShape, tPoint, tBounds, tMatrix), fOpacity);
		}
	}
}

static void __xgeShapeExApplyStrokePaint(xge_shape_ex pShape, xge_shape_vertex_t* pVertices, int iVertexCount, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, uint32_t iFallbackColor, float fOpacity)
{
	int i;

	if ( (pShape == NULL) || (pVertices == NULL) || (iVertexCount <= 0) ) {
		return;
	}
	if ( (pShape->iStrokeType != XGE_SHAPE_EX_FILL_LINEAR_GRADIENT) && (pShape->iStrokeType != XGE_SHAPE_EX_FILL_RADIAL_GRADIENT) ) {
		for ( i = 0; i < iVertexCount; i++ ) {
			pVertices[i].iColor = iFallbackColor;
		}
		return;
	}
	for ( i = 0; i < iVertexCount; i++ ) {
		xge_vec2_t tPoint;
		uint32_t iColor;

		tPoint.fX = pVertices[i].fX;
		tPoint.fY = pVertices[i].fY;
		if ( pShape->iStrokeType == XGE_SHAPE_EX_FILL_RADIAL_GRADIENT ) {
			iColor = __xgeShapeExRadialGradientColorEx(pShape->fStrokeX1, pShape->fStrokeY1, pShape->fStrokeR, pShape->fStrokeFX, pShape->fStrokeFY,
				pShape->fStrokeFR,
				pShape->iStrokeGradientUnits, pShape->tStrokeGradientTransform, pShape->pStrokeStops, pShape->iStrokeStopCount,
				pShape->iStrokeColor, pShape->iStrokeGradientSpread, tPoint, tBounds, tMatrix);
		} else {
			iColor = __xgeShapeExLinearGradientColorEx(pShape->fStrokeX1, pShape->fStrokeY1, pShape->fStrokeX2, pShape->fStrokeY2,
				pShape->iStrokeGradientUnits, pShape->tStrokeGradientTransform, pShape->pStrokeStops, pShape->iStrokeStopCount,
				pShape->iStrokeColor, pShape->iStrokeGradientSpread, tPoint, tBounds, tMatrix);
		}
		pVertices[i].iColor = __xgeShapeExColorOpacity(iColor, fOpacity);
	}
}

static xge_vec2_t* __xgeShapeExContourCopyOriented(const xge_shape_ex_flat_path_t* pFlat, int iContour, int bWantPositive, int* pCount)
{
	const xge_shape_ex_flat_contour_t* pContour;
	const xge_vec2_t* pSrc;
	xge_vec2_t* pOut;
	float fArea;
	int iCount;
	int i;

	if ( (pFlat == NULL) || (iContour < 0) || (iContour >= pFlat->iContourCount) || (pCount == NULL) ) {
		return NULL;
	}
	pContour = &pFlat->pContours[iContour];
	pSrc = pFlat->pPoints + pContour->iStart;
	iCount = pContour->iCount;
	while ( (iCount > 2) && __xgeShapeExSamePoint(pSrc[iCount - 1], pSrc[0]) ) {
		iCount--;
	}
	if ( iCount < 3 ) {
		return NULL;
	}
	pOut = (xge_vec2_t*)xrtMalloc((size_t)iCount * sizeof(*pOut));
	if ( pOut == NULL ) {
		return NULL;
	}
	fArea = __xgeShapeExContourArea(pSrc, iCount);
	if ( ((fArea >= 0.0f) && bWantPositive) || ((fArea < 0.0f) && !bWantPositive) ) {
		memcpy(pOut, pSrc, (size_t)iCount * sizeof(*pOut));
	} else {
		for ( i = 0; i < iCount; i++ ) {
			pOut[i] = pSrc[iCount - 1 - i];
		}
	}
	*pCount = iCount;
	return pOut;
}

static int __xgeShapeExBridgeVisible(const xge_vec2_t* pPoly, int iPolyCount, int iPolyVertex, const xge_vec2_t* pHole, int iHoleCount, int iHoleVertex)
{
	xge_vec2_t tA;
	xge_vec2_t tB;
	int i;

	if ( (pPoly == NULL) || (pHole == NULL) || (iPolyCount < 3) || (iHoleCount < 3) ) {
		return 0;
	}
	tA = pPoly[iPolyVertex];
	tB = pHole[iHoleVertex];
	for ( i = 0; i < iPolyCount; i++ ) {
		int iNext = (i + 1) % iPolyCount;

		if ( (i == iPolyVertex) || (iNext == iPolyVertex) ) {
			continue;
		}
		if ( __xgeShapeExSegmentsIntersect(tA, tB, pPoly[i], pPoly[iNext]) ) {
			return 0;
		}
	}
	for ( i = 0; i < iHoleCount; i++ ) {
		int iNext = (i + 1) % iHoleCount;

		if ( (i == iHoleVertex) || (iNext == iHoleVertex) ) {
			continue;
		}
		if ( __xgeShapeExSegmentsIntersect(tA, tB, pHole[i], pHole[iNext]) ) {
			return 0;
		}
	}
	return 1;
}

static int __xgeShapeExBridgeOneHole(xge_vec2_t** ppPoly, int* pPolyCount, const xge_shape_ex_flat_path_t* pFlat, int iHoleContour)
{
	xge_vec2_t* pPoly;
	xge_vec2_t* pHole;
	xge_vec2_t* pNew;
	float fBestDistance;
	float fBestRayX;
	xge_vec2_t tBridgePoint;
	int iPolyCount;
	int iHoleCount;
	int iHoleVertex;
	int iBridgeVertex;
	int iBridgeEdge;
	int iWrite;
	int i;

	if ( (ppPoly == NULL) || (*ppPoly == NULL) || (pPolyCount == NULL) || (pFlat == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pPoly = *ppPoly;
	iPolyCount = *pPolyCount;
	pHole = __xgeShapeExContourCopyOriented(pFlat, iHoleContour, 0, &iHoleCount);
	if ( pHole == NULL ) {
		return XGE_OK;
	}
	iHoleVertex = 0;
	for ( i = 1; i < iHoleCount; i++ ) {
		if ( (pHole[i].fX > pHole[iHoleVertex].fX) ||
		     ((fabsf(pHole[i].fX - pHole[iHoleVertex].fX) <= XGE_SHAPE_EX_EPSILON) && (pHole[i].fY < pHole[iHoleVertex].fY)) ) {
			iHoleVertex = i;
		}
	}
	iBridgeVertex = -1;
	iBridgeEdge = -1;
	fBestDistance = FLT_MAX;
	fBestRayX = FLT_MAX;
	tBridgePoint = pHole[iHoleVertex];
	for ( i = 0; i < iPolyCount; i++ ) {
		int iNext = (i + 1) % iPolyCount;
		xge_vec2_t tA = pPoly[i];
		xge_vec2_t tB = pPoly[iNext];
		float fY = pHole[iHoleVertex].fY;
		float fX;

		if ( fabsf(tA.fY - tB.fY) <= XGE_SHAPE_EX_EPSILON ) {
			continue;
		}
		if ( ((tA.fY > fY) == (tB.fY > fY)) ) {
			continue;
		}
		fX = tA.fX + ((fY - tA.fY) * (tB.fX - tA.fX) / (tB.fY - tA.fY));
		if ( fX <= pHole[iHoleVertex].fX + XGE_SHAPE_EX_EPSILON ) {
			continue;
		}
		if ( fX < fBestRayX ) {
			fBestRayX = fX;
			iBridgeEdge = i;
			tBridgePoint.fX = fX;
			tBridgePoint.fY = fY;
		}
	}
	if ( iBridgeEdge >= 0 ) {
		pNew = (xge_vec2_t*)xrtMalloc((size_t)(iPolyCount + iHoleCount + 3) * sizeof(*pNew));
		if ( pNew == NULL ) {
			xrtFree(pHole);
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		iWrite = 0;
		for ( i = 0; i <= iBridgeEdge; i++ ) {
			pNew[iWrite++] = pPoly[i];
		}
		pNew[iWrite++] = tBridgePoint;
		for ( i = 0; i < iHoleCount; i++ ) {
			pNew[iWrite++] = pHole[(iHoleVertex + i) % iHoleCount];
		}
		pNew[iWrite++] = pHole[iHoleVertex];
		pNew[iWrite++] = tBridgePoint;
		for ( i = iBridgeEdge + 1; i < iPolyCount; i++ ) {
			pNew[iWrite++] = pPoly[i];
		}
		xrtFree(pHole);
		xrtFree(pPoly);
		*ppPoly = pNew;
		*pPolyCount = iWrite;
		return XGE_OK;
	}
	for ( i = 0; i < iPolyCount; i++ ) {
		float fDistance;

		if ( pPoly[i].fX < (pHole[iHoleVertex].fX - XGE_SHAPE_EX_EPSILON) ) {
			continue;
		}
		if ( !__xgeShapeExBridgeVisible(pPoly, iPolyCount, i, pHole, iHoleCount, iHoleVertex) ) {
			continue;
		}
		fDistance = __xgeShapeExDistanceSq(pPoly[i], pHole[iHoleVertex]);
		if ( fDistance < fBestDistance ) {
			fBestDistance = fDistance;
			iBridgeVertex = i;
		}
	}
	if ( iBridgeVertex < 0 ) {
		for ( i = 0; i < iPolyCount; i++ ) {
			float fDistance;

			if ( !__xgeShapeExBridgeVisible(pPoly, iPolyCount, i, pHole, iHoleCount, iHoleVertex) ) {
				continue;
			}
			fDistance = __xgeShapeExDistanceSq(pPoly[i], pHole[iHoleVertex]);
			if ( fDistance < fBestDistance ) {
				fBestDistance = fDistance;
				iBridgeVertex = i;
			}
		}
	}
	if ( iBridgeVertex < 0 ) {
		xrtFree(pHole);
		return XGE_OK;
	}
	pNew = (xge_vec2_t*)xrtMalloc((size_t)(iPolyCount + iHoleCount + 2) * sizeof(*pNew));
	if ( pNew == NULL ) {
		xrtFree(pHole);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iWrite = 0;
	for ( i = 0; i <= iBridgeVertex; i++ ) {
		pNew[iWrite++] = pPoly[i];
	}
	for ( i = 0; i < iHoleCount; i++ ) {
		pNew[iWrite++] = pHole[(iHoleVertex + i) % iHoleCount];
	}
	pNew[iWrite++] = pHole[iHoleVertex];
	pNew[iWrite++] = pPoly[iBridgeVertex];
	for ( i = iBridgeVertex + 1; i < iPolyCount; i++ ) {
		pNew[iWrite++] = pPoly[i];
	}
	xrtFree(pHole);
	xrtFree(pPoly);
	*ppPoly = pNew;
	*pPolyCount = iWrite;
	return XGE_OK;
}

static int __xgeShapeExContourIsConvex(const xge_vec2_t* pPoints, int iCount)
{
	float fSign;
	int i;

	fSign = 0.0f;
	for ( i = 0; i < iCount; i++ ) {
		xge_vec2_t tA = pPoints[i];
		xge_vec2_t tB = pPoints[(i + 1) % iCount];
		xge_vec2_t tC = pPoints[(i + 2) % iCount];
		float fCross = (tB.fX - tA.fX) * (tC.fY - tB.fY) -
		               (tB.fY - tA.fY) * (tC.fX - tB.fX);

		if ( fabsf(fCross) <= XGE_SHAPE_EX_EPSILON ) continue;
		if ( fSign == 0.0f ) fSign = fCross;
		else if ( (fSign < 0.0f) != (fCross < 0.0f) ) return 0;
	}
	return fSign != 0.0f;
}

static int __xgeShapeExContourIsAxisAlignedRect(const xge_vec2_t* pPoints, int iCount)
{
	int iVertexCount;
	int i;

	if ( pPoints == NULL ) return 0;
	iVertexCount = iCount;
	if ( (iVertexCount == 5) && __xgeShapeExSamePoint(pPoints[0], pPoints[4]) ) {
		iVertexCount = 4;
	}
	if ( iVertexCount != 4 ) return 0;
	for ( i = 0; i < iVertexCount; i++ ) {
		xge_vec2_t tA = pPoints[i];
		xge_vec2_t tB = pPoints[(i + 1) % iVertexCount];
		float fDX = fabsf(tB.fX - tA.fX);
		float fDY = fabsf(tB.fY - tA.fY);

		if ( ((fDX <= XGE_SHAPE_EX_EPSILON) == (fDY <= XGE_SHAPE_EX_EPSILON)) ) return 0;
	}
	return 1;
}

static const xge_vec2_t* __xgeShapeExStableHalfOpenRect(
	const xge_vec2_t* pPoints,
	int iCount,
	xge_vec2_t* pStablePoints
)
{
	float fMaxX;
	float fMaxY;
	float fCenterX;
	float fCenterY;
	float fStableMaxX;
	float fStableMaxY;
	int bStableX;
	int bStableY;
	int i;

	if ( (pPoints == NULL) || (pStablePoints == NULL) || (iCount < 4) || (iCount > 5) ||
	     !__xgeShapeExContourIsAxisAlignedRect(pPoints, iCount) ) return pPoints;
	fMaxX = pPoints[0].fX;
	fMaxY = pPoints[0].fY;
	for ( i = 1; i < iCount; i++ ) {
		if ( pPoints[i].fX > fMaxX ) fMaxX = pPoints[i].fX;
		if ( pPoints[i].fY > fMaxY ) fMaxY = pPoints[i].fY;
	}
	fCenterX = floorf(fMaxX) + 0.5f;
	fCenterY = floorf(fMaxY) + 0.5f;
	bStableX = (fMaxX <= fCenterX) && ((fCenterX - fMaxX) < XGE_SHAPE_EX_HALF_OPEN_THRESHOLD);
	bStableY = (fMaxY <= fCenterY) && ((fCenterY - fMaxY) < XGE_SHAPE_EX_HALF_OPEN_THRESHOLD);
	if ( !bStableX && !bStableY ) return pPoints;
	fStableMaxX = fCenterX - XGE_SHAPE_EX_HALF_OPEN_BIAS;
	fStableMaxY = fCenterY - XGE_SHAPE_EX_HALF_OPEN_BIAS;
	for ( i = 0; i < iCount; i++ ) {
		pStablePoints[i] = pPoints[i];
		if ( bStableX && (fabsf(pPoints[i].fX - fMaxX) <= XGE_SHAPE_EX_EPSILON) ) {
			pStablePoints[i].fX = fStableMaxX;
		}
		if ( bStableY && (fabsf(pPoints[i].fY - fMaxY) <= XGE_SHAPE_EX_EPSILON) ) {
			pStablePoints[i].fY = fStableMaxY;
		}
	}
	return pStablePoints;
}

static int __xgeShapeExContourHasIntersectingEdges(const xge_vec2_t* pPoints, int iCount)
{
	int i;

	for ( i = 0; i < iCount; i++ ) {
		xge_vec2_t tA = pPoints[i];
		xge_vec2_t tB = pPoints[(i + 1) % iCount];
		int j;

		if ( __xgeShapeExSamePoint(tA, tB) ) return 1;
		for ( j = i + 1; j < iCount; j++ ) {
			xge_vec2_t tC;
			xge_vec2_t tD;

			if ( (j == ((i + 1) % iCount)) || (((j + 1) % iCount) == i) ) continue;
			tC = pPoints[j];
			tD = pPoints[(j + 1) % iCount];
			if ( __xgeShapeExSamePoint(tC, tD) || __xgeShapeExSegmentsIntersect(tA, tB, tC, tD) ) return 1;
		}
	}
	return 0;
}

static int __xgeShapeExOffsetSimpleContour(const xge_vec2_t* pPoints, int iCount, float fOffset, xge_vec2_t* pOut)
{
	int bPositive = __xgeShapeExContourArea(pPoints, iCount) >= 0.0f;
	int i;

	for ( i = 0; i < iCount; i++ ) {
		int iPrev = (i + iCount - 1) % iCount;
		int iNext = (i + 1) % iCount;
		float fPrevDX = pPoints[i].fX - pPoints[iPrev].fX;
		float fPrevDY = pPoints[i].fY - pPoints[iPrev].fY;
		float fNextDX = pPoints[iNext].fX - pPoints[i].fX;
		float fNextDY = pPoints[iNext].fY - pPoints[i].fY;
		float fPrevLength = sqrtf(fPrevDX * fPrevDX + fPrevDY * fPrevDY);
		float fNextLength = sqrtf(fNextDX * fNextDX + fNextDY * fNextDY);
		float fPrevNX;
		float fPrevNY;
		float fNextNX;
		float fNextNY;
		float fMX;
		float fMY;
		float fMLength;
		float fDenominator;
		float fScale;

		if ( (fPrevLength <= XGE_SHAPE_EX_EPSILON) || (fNextLength <= XGE_SHAPE_EX_EPSILON) ) {
			return 0;
		}
		if ( bPositive ) {
			fPrevNX = fPrevDY / fPrevLength;
			fPrevNY = -fPrevDX / fPrevLength;
			fNextNX = fNextDY / fNextLength;
			fNextNY = -fNextDX / fNextLength;
		} else {
			fPrevNX = -fPrevDY / fPrevLength;
			fPrevNY = fPrevDX / fPrevLength;
			fNextNX = -fNextDY / fNextLength;
			fNextNY = fNextDX / fNextLength;
		}
		fMX = fPrevNX + fNextNX;
		fMY = fPrevNY + fNextNY;
		fMLength = sqrtf(fMX * fMX + fMY * fMY);
		if ( fMLength <= XGE_SHAPE_EX_EPSILON ) {
			return 0;
		}
		fMX /= fMLength;
		fMY /= fMLength;
		fDenominator = fMX * fNextNX + fMY * fNextNY;
		if ( fabsf(fDenominator) <= XGE_SHAPE_EX_EPSILON ) {
			return 0;
		}
		fScale = fOffset / fDenominator;
		if ( fabsf(fScale) > fabsf(fOffset) * 4.0f ) return 0;
		pOut[i].fX = pPoints[i].fX + fMX * fScale;
		pOut[i].fY = pPoints[i].fY + fMY * fScale;
		if ( !__xgeShapeExFloatFinite(pOut[i].fX) || !__xgeShapeExFloatFinite(pOut[i].fY) ) return 0;
	}
	return 1;
}

static int __xgeShapeExBuildSimpleAAMesh(const xge_vec2_t* pPoints, int iCount, uint32_t iColor, float fAAHalfWidth, int bSkipAxisAlignedRect, xge_shape_vertex_t** ppVertices, int* pVertexCount, uint32_t** ppIndices, int* pIndexCount, int* pContourCount, int* pOuterVertexStart)
{
	xge_shape_vertex_t* pVertices;
	xge_shape_vertex_t* pFillVertices;
	uint32_t* pIndices;
	uint32_t* pFillIndices;
	xge_vec2_t* pInner;
	xge_vec2_t* pOuter;
	xge_vec2_t tCenter;
	float fOriginalArea;
	float fInnerArea;
	float fOuterArea;
	int iVertexCount;
	int iIndexCount;
	int iFillVertexCount;
	int iFillIndexCount;
	int iOuterStart;
	int bConvex;
	int iRet;
	int i;

	while ( (iCount > 2) && __xgeShapeExSamePoint(pPoints[iCount - 1], pPoints[0]) ) iCount--;
	if ( (iCount < 3) || (fAAHalfWidth <= XGE_SHAPE_EX_EPSILON) ||
	     (bSkipAxisAlignedRect && __xgeShapeExContourIsAxisAlignedRect(pPoints, iCount)) ||
	     __xgeShapeExContourHasIntersectingEdges(pPoints, iCount) ) return XGE_OK;
	if ( iCount > ((INT_MAX - 1) / 2) || iCount > (INT_MAX / 9) ) return XGE_ERROR_OUT_OF_MEMORY;
	bConvex = __xgeShapeExContourIsConvex(pPoints, iCount);
	pFillVertices = NULL;
	pFillIndices = NULL;
	iFillVertexCount = 0;
	iFillIndexCount = 0;
	pInner = (xge_vec2_t*)xrtMalloc((size_t)iCount * sizeof(*pInner));
	pOuter = (xge_vec2_t*)xrtMalloc((size_t)iCount * sizeof(*pOuter));
	if ( (pInner == NULL) || (pOuter == NULL) ) {
		xrtFree(pInner);
		xrtFree(pOuter);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( !__xgeShapeExOffsetSimpleContour(pPoints, iCount, -fAAHalfWidth, pInner) ||
	     !__xgeShapeExOffsetSimpleContour(pPoints, iCount, fAAHalfWidth, pOuter) ) {
		xrtFree(pInner);
		xrtFree(pOuter);
		return XGE_OK;
	}
	fOriginalArea = __xgeShapeExContourArea(pPoints, iCount);
	fInnerArea = __xgeShapeExContourArea(pInner, iCount);
	fOuterArea = __xgeShapeExContourArea(pOuter, iCount);
	if ( (fabsf(fOriginalArea) <= XGE_SHAPE_EX_EPSILON) ||
	     (fabsf(fInnerArea) <= XGE_SHAPE_EX_EPSILON) || (fabsf(fOuterArea) <= XGE_SHAPE_EX_EPSILON) ||
	     ((fOriginalArea < 0.0f) != (fInnerArea < 0.0f)) ||
	     ((fOriginalArea < 0.0f) != (fOuterArea < 0.0f)) ||
	     __xgeShapeExContourHasIntersectingEdges(pInner, iCount) ||
	     __xgeShapeExContourHasIntersectingEdges(pOuter, iCount) ) {
		xrtFree(pInner);
		xrtFree(pOuter);
		return XGE_OK;
	}
	if ( bConvex ) {
		iVertexCount = 1 + iCount * 2;
		iIndexCount = iCount * 9;
		iOuterStart = 1 + iCount;
	} else {
		iRet = __xgeShapeExTriangulateContour(
			pInner, iCount, &pFillVertices, &iFillVertexCount, &pFillIndices, &iFillIndexCount, iColor
		);
		if ( iRet != XGE_OK ) {
			xrtFree(pInner);
			xrtFree(pOuter);
			return iRet;
		}
		if ( (iFillVertexCount != iCount) || (iFillIndexCount != ((iCount - 2) * 3)) ) {
			xrtFree(pFillVertices);
			xrtFree(pFillIndices);
			xrtFree(pInner);
			xrtFree(pOuter);
			return XGE_OK;
		}
		iVertexCount = iCount * 2;
		iIndexCount = iFillIndexCount + iCount * 6;
		iOuterStart = iCount;
	}
	pVertices = (xge_shape_vertex_t*)xrtMalloc((size_t)iVertexCount * sizeof(*pVertices));
	pIndices = (uint32_t*)xrtMalloc((size_t)iIndexCount * sizeof(*pIndices));
	if ( (pVertices == NULL) || (pIndices == NULL) ) {
		xrtFree(pVertices);
		xrtFree(pIndices);
		xrtFree(pFillVertices);
		xrtFree(pFillIndices);
		xrtFree(pInner);
		xrtFree(pOuter);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( bConvex ) {
		tCenter = (xge_vec2_t){0.0f, 0.0f};
		for ( i = 0; i < iCount; i++ ) {
			tCenter.fX += pInner[i].fX;
			tCenter.fY += pInner[i].fY;
		}
		tCenter.fX /= (float)iCount;
		tCenter.fY /= (float)iCount;
		pVertices[0] = (xge_shape_vertex_t){tCenter.fX, tCenter.fY, iColor};
		for ( i = 0; i < iCount; i++ ) {
			int iNext = (i + 1) % iCount;
			int iFillBase = i * 3;
			int iFringeBase = iCount * 3 + i * 6;

			pVertices[1 + i] = (xge_shape_vertex_t){pInner[i].fX, pInner[i].fY, iColor};
			pIndices[iFillBase + 0] = 0u;
			pIndices[iFillBase + 1] = (uint32_t)(1 + i);
			pIndices[iFillBase + 2] = (uint32_t)(1 + iNext);
			pIndices[iFringeBase + 0] = (uint32_t)(1 + i);
			pIndices[iFringeBase + 1] = (uint32_t)(1 + iNext);
			pIndices[iFringeBase + 2] = (uint32_t)(iOuterStart + i);
			pIndices[iFringeBase + 3] = (uint32_t)(iOuterStart + i);
			pIndices[iFringeBase + 4] = (uint32_t)(1 + iNext);
			pIndices[iFringeBase + 5] = (uint32_t)(iOuterStart + iNext);
		}
	} else {
		memcpy(pVertices, pFillVertices, (size_t)iCount * sizeof(*pVertices));
		memcpy(pIndices, pFillIndices, (size_t)iFillIndexCount * sizeof(*pIndices));
		for ( i = 0; i < iCount; i++ ) {
			int iNext = (i + 1) % iCount;
			int iFringeBase = iFillIndexCount + i * 6;

			pIndices[iFringeBase + 0] = (uint32_t)i;
			pIndices[iFringeBase + 1] = (uint32_t)iNext;
			pIndices[iFringeBase + 2] = (uint32_t)(iOuterStart + i);
			pIndices[iFringeBase + 3] = (uint32_t)(iOuterStart + i);
			pIndices[iFringeBase + 4] = (uint32_t)iNext;
			pIndices[iFringeBase + 5] = (uint32_t)(iOuterStart + iNext);
		}
	}
	for ( i = 0; i < iCount; i++ ) {
		pVertices[iOuterStart + i] = (xge_shape_vertex_t){pOuter[i].fX, pOuter[i].fY, iColor & 0xFFFFFF00u};
	}
	xrtFree(pFillVertices);
	xrtFree(pFillIndices);
	xrtFree(pInner);
	xrtFree(pOuter);
	*ppVertices = pVertices;
	*pVertexCount = iVertexCount;
	*ppIndices = pIndices;
	*pIndexCount = iIndexCount;
	*pContourCount = iCount;
	*pOuterVertexStart = iOuterStart;
	return XGE_OK;
}

static int __xgeShapeExDrawFillMesh(xge_shape_ex pShape, const xge_vec2_t* pPoints, int iCount, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, float fOpacity, uint32_t iColor, int bScreenSpace, int bAntialias)
{
	xge_vec2_t arrProjectedRect[5];
	xge_vec2_t arrStableRect[5];
	const xge_vec2_t* pDrawPoints;
	xge_shape_vertex_t* pVertices;
	uint32_t* pIndices;
	uint8_t* pCoverage;
	int iVertexCount;
	int iIndexCount;
	int iRet;
	int iPass;
	int bUsedCenterFan;
	int iAAContourCount;
	int iAAOuterVertexStart;
	int bDrawScreenSpace;

	pVertices = NULL;
	pIndices = NULL;
	pCoverage = NULL;
	iVertexCount = 0;
	iIndexCount = 0;
	bUsedCenterFan = 0;
	iAAContourCount = 0;
	iAAOuterVertexStart = 0;
	pDrawPoints = pPoints;
	bDrawScreenSpace = bScreenSpace;
	if ( bAntialias && ((pShape == NULL) ||
	     ((!pShape->bAxisAlignedFillAntialias) && (pShape->iFillType == XGE_SHAPE_EX_FILL_SOLID))) &&
	     __xgeShapeExContourIsAxisAlignedRect(pPoints, iCount) ) {
		if ( bScreenSpace ) {
			pDrawPoints = __xgeShapeExStableHalfOpenRect(pPoints, iCount, arrStableRect);
		} else if ( iCount <= 5 ) {
			const xge_vec2_t* pStable;
			int i;

			for ( i = 0; i < iCount; i++ ) {
				arrProjectedRect[i] = xgeWorldToScreen(pPoints[i]);
			}
			pStable = __xgeShapeExStableHalfOpenRect(arrProjectedRect, iCount, arrStableRect);
			if ( pStable != arrProjectedRect ) {
				pDrawPoints = pStable;
				bDrawScreenSpace = 1;
			}
		}
	}
	if ( bAntialias ) {
		int bSkipAxisAlignedRect = (pShape == NULL) || !pShape->bAxisAlignedFillAntialias;

		iRet = __xgeShapeExBuildSimpleAAMesh(pDrawPoints, iCount, iColor, XGE_SHAPE_EX_AA_HALF_WIDTH, bSkipAxisAlignedRect, &pVertices, &iVertexCount, &pIndices, &iIndexCount, &iAAContourCount, &iAAOuterVertexStart);
		if ( iRet != XGE_OK ) return iRet;
	}
	if ( (pVertices == NULL) && (pShape != NULL) && (pShape->iFillType == XGE_SHAPE_EX_FILL_RADIAL_GRADIENT) ) {
		xge_vec2_t tCenter = __xgeShapeExRadialGradientCenterPoint(pShape, tBounds, tMatrix);

		iRet = __xgeShapeExBuildCenterFan(pDrawPoints, iCount, tCenter, iColor, &pVertices, &iVertexCount, &pIndices, &iIndexCount);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		bUsedCenterFan = pVertices != NULL;
	}
	if ( pVertices == NULL ) {
		iRet = __xgeShapeExTriangulateContour(pDrawPoints, iCount, &pVertices, &iVertexCount, &pIndices, &iIndexCount, iColor);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	if ( (iAAContourCount == 0) && (pShape != NULL) && (pShape->iFillType == XGE_SHAPE_EX_FILL_RADIAL_GRADIENT) && (iIndexCount > 0) ) {
		int iPassCount = bUsedCenterFan ? 5 : 4;

		if ( pShape->iFillGradientSpread != XGE_SHAPE_EX_GRADIENT_SPREAD_PAD ) {
			iPassCount = 6;
		}
		iPassCount += __xgeShapeExRadialGradientCorrectionPasses(
			pShape->fFillX1, pShape->fFillY1, pShape->fFillR,
			pShape->fFillFX, pShape->fFillFY, pShape->fFillFR
		);

		for ( iPass = 0; iPass < iPassCount; iPass++ ) {
			iRet = __xgeShapeExSubdivideTriangles(&pVertices, &iVertexCount, &pIndices, &iIndexCount);
			if ( iRet != XGE_OK ) {
				xrtFree(pVertices);
				xrtFree(pIndices);
				return iRet;
			}
		}
	}
	if ( (iAAContourCount == 0) && (pShape != NULL) && (pShape->iFillType == XGE_SHAPE_EX_FILL_LINEAR_GRADIENT) && (iIndexCount > 0) ) {
		int iPassCount = __xgeShapeExLinearGradientSubdivisionPasses(pShape->pFillStops, pShape->iFillStopCount, pShape->iFillGradientSpread);
		iPassCount = __xgeShapeExLinearGradientAdaptivePasses(iPassCount, pShape->iFillGradientSpread,
			pShape->fFillX1, pShape->fFillY1, pShape->fFillX2, pShape->fFillY2,
			pShape->iFillGradientUnits, pShape->tFillGradientTransform,
			pVertices, iVertexCount, tBounds, tMatrix);

		for ( iPass = 0; iPass < iPassCount; iPass++ ) {
			iRet = __xgeShapeExSubdivideTriangles(&pVertices, &iVertexCount, &pIndices, &iIndexCount);
			if ( iRet != XGE_OK ) {
				xrtFree(pVertices);
				xrtFree(pIndices);
				return iRet;
			}
		}
	}
	pCoverage = __xgeShapeExCoverageFromVertexAlpha(
		pVertices, iVertexCount, XGE_COLOR_GET_A(iColor)
	);
	if ( (g_xgeShapeExAlphaMaskTargetDepth > 0) && (pCoverage == NULL) &&
	     (XGE_COLOR_GET_A(iColor) > 0) ) {
		xrtFree(pVertices);
		xrtFree(pIndices);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	__xgeShapeExApplyFillPaint(pShape, pVertices, iVertexCount, tBounds, tMatrix, iColor, fOpacity);
	if ( pCoverage != NULL ) {
		__xgeShapeExAlphaMaskEncodeVertices(pVertices, iVertexCount, pCoverage);
	} else if ( iAAContourCount > 0 ) {
		for ( iPass = 0; iPass < iAAContourCount; iPass++ ) {
			pVertices[iAAOuterVertexStart + iPass].iColor &= 0xFFFFFF00u;
		}
	}
	if ( iIndexCount > 0 ) {
		iRet = bDrawScreenSpace ? xgeShapeMeshFillPx(pVertices, iVertexCount, pIndices, iIndexCount) : xgeShapeMeshFill(pVertices, iVertexCount, pIndices, iIndexCount);
	} else {
		iRet = XGE_OK;
	}
	xrtFree(pVertices);
	xrtFree(pIndices);
	xrtFree(pCoverage);
	return iRet;
}

typedef struct xge_shape_ex_scan_edge_t {
	xge_vec2_t tA;
	xge_vec2_t tB;
	float fXMid;
	int iWinding;
} xge_shape_ex_scan_edge_t;

typedef struct xge_shape_ex_scan_mesh_t {
	xge_shape_vertex_t* pVertices;
	uint32_t* pIndices;
	int iVertexCount;
	int iIndexCount;
	int iVertexCapacity;
	int iIndexCapacity;
} xge_shape_ex_scan_mesh_t;

static int __xgeShapeExDrawSupersampledScanMesh(
	xge_shape_vertex_t* pVertices,
	int iVertexCount,
	const uint32_t* pIndices,
	int iIndexCount,
	xge_rect_t tBounds
);

static int __xgeShapeExFloatCompare(const void* pA, const void* pB)
{
	float fA = *(const float*)pA;
	float fB = *(const float*)pB;

	return (fA > fB) - (fA < fB);
}

static int __xgeShapeExScanEdgeCompare(const void* pA, const void* pB)
{
	const xge_shape_ex_scan_edge_t* pEdgeA = (const xge_shape_ex_scan_edge_t*)pA;
	const xge_shape_ex_scan_edge_t* pEdgeB = (const xge_shape_ex_scan_edge_t*)pB;

	if ( pEdgeA->fXMid < pEdgeB->fXMid ) return -1;
	if ( pEdgeA->fXMid > pEdgeB->fXMid ) return 1;
	return pEdgeA->iWinding - pEdgeB->iWinding;
}

static float __xgeShapeExScanEdgeXAt(const xge_shape_ex_scan_edge_t* pEdge, float fY)
{
	float fDY = pEdge->tB.fY - pEdge->tA.fY;

	if ( fabsf(fDY) <= XGE_SHAPE_EX_EPSILON ) {
		return pEdge->tA.fX;
	}
	return pEdge->tA.fX + ((fY - pEdge->tA.fY) * (pEdge->tB.fX - pEdge->tA.fX) / fDY);
}

static int __xgeShapeExScanAppendLevel(float** ppLevels, int* pCount, int* pCapacity, float fY)
{
	float* pLevels;
	int iCapacity;

	if ( (*pCount + 1) > *pCapacity ) {
		iCapacity = (*pCapacity > 0) ? (*pCapacity * 2) : 32;
		while ( iCapacity < (*pCount + 1) ) {
			iCapacity *= 2;
		}
		pLevels = (float*)xrtRealloc(*ppLevels, (size_t)iCapacity * sizeof(*pLevels));
		if ( pLevels == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		*ppLevels = pLevels;
		*pCapacity = iCapacity;
	}
	(*ppLevels)[(*pCount)++] = fY;
	return XGE_OK;
}

static int __xgeShapeExScanProperIntersectionY(const xge_shape_ex_scan_edge_t* pA, const xge_shape_ex_scan_edge_t* pB, float* pY)
{
	float fRX = pA->tB.fX - pA->tA.fX;
	float fRY = pA->tB.fY - pA->tA.fY;
	float fSX = pB->tB.fX - pB->tA.fX;
	float fSY = pB->tB.fY - pB->tA.fY;
	float fQPX = pB->tA.fX - pA->tA.fX;
	float fQPY = pB->tA.fY - pA->tA.fY;
	float fDenominator = (fRX * fSY) - (fRY * fSX);
	float fT;
	float fU;

	if ( fabsf(fDenominator) <= XGE_SHAPE_EX_EPSILON ) {
		return 0;
	}
	fT = ((fQPX * fSY) - (fQPY * fSX)) / fDenominator;
	fU = ((fQPX * fRY) - (fQPY * fRX)) / fDenominator;
	if ( (fT <= XGE_SHAPE_EX_EPSILON) || (fT >= 1.0f - XGE_SHAPE_EX_EPSILON) ||
	     (fU <= XGE_SHAPE_EX_EPSILON) || (fU >= 1.0f - XGE_SHAPE_EX_EPSILON) ) {
		return 0;
	}
	*pY = pA->tA.fY + fT * fRY;
	return 1;
}

static int __xgeShapeExScanMeshReserve(xge_shape_ex_scan_mesh_t* pMesh, int iAddVertices, int iAddIndices)
{
	int iCapacity;

	if ( (pMesh->iVertexCount + iAddVertices) > pMesh->iVertexCapacity ) {
		xge_shape_vertex_t* pVertices;

		iCapacity = (pMesh->iVertexCapacity > 0) ? (pMesh->iVertexCapacity * 2) : 64;
		while ( iCapacity < (pMesh->iVertexCount + iAddVertices) ) iCapacity *= 2;
		pVertices = (xge_shape_vertex_t*)xrtRealloc(pMesh->pVertices, (size_t)iCapacity * sizeof(*pVertices));
		if ( pVertices == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
		pMesh->pVertices = pVertices;
		pMesh->iVertexCapacity = iCapacity;
	}
	if ( (pMesh->iIndexCount + iAddIndices) > pMesh->iIndexCapacity ) {
		uint32_t* pIndices;

		iCapacity = (pMesh->iIndexCapacity > 0) ? (pMesh->iIndexCapacity * 2) : 96;
		while ( iCapacity < (pMesh->iIndexCount + iAddIndices) ) iCapacity *= 2;
		pIndices = (uint32_t*)xrtRealloc(pMesh->pIndices, (size_t)iCapacity * sizeof(*pIndices));
		if ( pIndices == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
		pMesh->pIndices = pIndices;
		pMesh->iIndexCapacity = iCapacity;
	}
	return XGE_OK;
}

static int __xgeShapeExScanMeshAppendBand(xge_shape_ex_scan_mesh_t* pMesh, const xge_shape_ex_scan_edge_t* pLeft, const xge_shape_ex_scan_edge_t* pRight, float fY0, float fY1, uint32_t iColor)
{
	float fLeft0 = __xgeShapeExScanEdgeXAt(pLeft, fY0);
	float fLeft1 = __xgeShapeExScanEdgeXAt(pLeft, fY1);
	float fRight0 = __xgeShapeExScanEdgeXAt(pRight, fY0);
	float fRight1 = __xgeShapeExScanEdgeXAt(pRight, fY1);
	uint32_t iBase;
	int iRet;

	if ( (fRight0 <= fLeft0 + XGE_SHAPE_EX_EPSILON) && (fRight1 <= fLeft1 + XGE_SHAPE_EX_EPSILON) ) {
		return XGE_OK;
	}
	iRet = __xgeShapeExScanMeshReserve(pMesh, 4, 6);
	if ( iRet != XGE_OK ) return iRet;
	iBase = (uint32_t)pMesh->iVertexCount;
	pMesh->pVertices[pMesh->iVertexCount++] = (xge_shape_vertex_t){fLeft0, fY0, iColor};
	pMesh->pVertices[pMesh->iVertexCount++] = (xge_shape_vertex_t){fRight0, fY0, iColor};
	pMesh->pVertices[pMesh->iVertexCount++] = (xge_shape_vertex_t){fRight1, fY1, iColor};
	pMesh->pVertices[pMesh->iVertexCount++] = (xge_shape_vertex_t){fLeft1, fY1, iColor};
	pMesh->pIndices[pMesh->iIndexCount++] = iBase;
	pMesh->pIndices[pMesh->iIndexCount++] = iBase + 1u;
	pMesh->pIndices[pMesh->iIndexCount++] = iBase + 2u;
	pMesh->pIndices[pMesh->iIndexCount++] = iBase;
	pMesh->pIndices[pMesh->iIndexCount++] = iBase + 2u;
	pMesh->pIndices[pMesh->iIndexCount++] = iBase + 3u;
	return XGE_OK;
}

static int __xgeShapeExBuildScanlineFillMesh(const xge_shape_ex_flat_path_t* pFlat, int iFillRule, uint32_t iColor, xge_shape_ex_scan_mesh_t* pMesh)
{
	xge_shape_ex_scan_edge_t* pEdges;
	xge_shape_ex_scan_edge_t* pActive;
	float* pLevels;
	int iEdgeCount;
	int iEdgeCapacity;
	int iLevelCount;
	int iLevelCapacity;
	int i;
	int iRet;

	memset(pMesh, 0, sizeof(*pMesh));
	pEdges = NULL;
	pActive = NULL;
	pLevels = NULL;
	iEdgeCount = 0;
	iEdgeCapacity = 0;
	iLevelCount = 0;
	iLevelCapacity = 0;
	for ( i = 0; i < pFlat->iContourCount; i++ ) {
		const xge_shape_ex_flat_contour_t* pContour = &pFlat->pContours[i];
		const xge_vec2_t* pPoints = pFlat->pPoints + pContour->iStart;
		int iCount = pContour->iCount;
		int j;

		while ( (iCount > 2) && __xgeShapeExSamePoint(pPoints[iCount - 1], pPoints[0]) ) iCount--;
		if ( iCount < 3 ) continue;
		for ( j = 0; j < iCount; j++ ) {
			xge_vec2_t tA = pPoints[j];
			xge_vec2_t tB = pPoints[(j + 1) % iCount];

			iRet = __xgeShapeExScanAppendLevel(&pLevels, &iLevelCount, &iLevelCapacity, tA.fY);
			if ( iRet != XGE_OK ) goto xge_shape_ex_scanline_fail;
			if ( fabsf(tB.fY - tA.fY) <= XGE_SHAPE_EX_EPSILON ) continue;
			if ( (iEdgeCount + 1) > iEdgeCapacity ) {
				xge_shape_ex_scan_edge_t* pNewEdges;
				iEdgeCapacity = (iEdgeCapacity > 0) ? (iEdgeCapacity * 2) : 32;
				pNewEdges = (xge_shape_ex_scan_edge_t*)xrtRealloc(pEdges, (size_t)iEdgeCapacity * sizeof(*pNewEdges));
				if ( pNewEdges == NULL ) {
					iRet = XGE_ERROR_OUT_OF_MEMORY;
					goto xge_shape_ex_scanline_fail;
				}
				pEdges = pNewEdges;
			}
			pEdges[iEdgeCount].tA = tA;
			pEdges[iEdgeCount].tB = tB;
			pEdges[iEdgeCount].fXMid = 0.0f;
			pEdges[iEdgeCount].iWinding = (tA.fY < tB.fY) ? 1 : -1;
			iEdgeCount++;
		}
	}
	for ( i = 0; i < iEdgeCount; i++ ) {
		int j;

		for ( j = i + 1; j < iEdgeCount; j++ ) {
			float fY;

			if ( __xgeShapeExScanProperIntersectionY(&pEdges[i], &pEdges[j], &fY) ) {
				iRet = __xgeShapeExScanAppendLevel(&pLevels, &iLevelCount, &iLevelCapacity, fY);
				if ( iRet != XGE_OK ) goto xge_shape_ex_scanline_fail;
			}
		}
	}
	if ( (iEdgeCount <= 0) || (iLevelCount < 2) ) {
		iRet = XGE_OK;
		goto xge_shape_ex_scanline_done;
	}
	qsort(pLevels, (size_t)iLevelCount, sizeof(*pLevels), __xgeShapeExFloatCompare);
	{
		int iWrite = 0;

		for ( i = 0; i < iLevelCount; i++ ) {
			if ( (iWrite == 0) || (fabsf(pLevels[i] - pLevels[iWrite - 1]) > XGE_SHAPE_EX_EPSILON) ) {
				pLevels[iWrite++] = pLevels[i];
			}
		}
		iLevelCount = iWrite;
	}
	pActive = (xge_shape_ex_scan_edge_t*)xrtMalloc((size_t)iEdgeCount * sizeof(*pActive));
	if ( pActive == NULL ) {
		iRet = XGE_ERROR_OUT_OF_MEMORY;
		goto xge_shape_ex_scanline_fail;
	}
	for ( i = 0; i + 1 < iLevelCount; i++ ) {
		float fY0 = pLevels[i];
		float fY1 = pLevels[i + 1];
		float fYMid;
		int iActiveCount;
		int j;

		if ( fY1 <= fY0 + XGE_SHAPE_EX_EPSILON ) continue;
		fYMid = (fY0 + fY1) * 0.5f;
		iActiveCount = 0;
		for ( j = 0; j < iEdgeCount; j++ ) {
			float fMinY = fminf(pEdges[j].tA.fY, pEdges[j].tB.fY);
			float fMaxY = fmaxf(pEdges[j].tA.fY, pEdges[j].tB.fY);

			if ( (fYMid <= fMinY) || (fYMid >= fMaxY) ) continue;
			pActive[iActiveCount] = pEdges[j];
			pActive[iActiveCount].fXMid = __xgeShapeExScanEdgeXAt(&pActive[iActiveCount], fYMid);
			iActiveCount++;
		}
		if ( iActiveCount < 2 ) continue;
		qsort(pActive, (size_t)iActiveCount, sizeof(*pActive), __xgeShapeExScanEdgeCompare);
		{
			int iState = 0;
			int bInside = 0;
			xge_shape_ex_scan_edge_t tLeft;

			for ( j = 0; j < iActiveCount; ) {
				int k = j + 1;
				int iDelta = pActive[j].iWinding;
				int bWasInside = bInside;

				while ( (k < iActiveCount) && (fabsf(pActive[k].fXMid - pActive[j].fXMid) <= XGE_SHAPE_EX_EPSILON) ) {
					iDelta += pActive[k].iWinding;
					k++;
				}
				if ( iFillRule == XGE_SHAPE_EX_FILL_EVEN_ODD ) iState += (k - j);
				else iState += iDelta;
				bInside = (iFillRule == XGE_SHAPE_EX_FILL_EVEN_ODD) ? ((iState & 1) != 0) : (iState != 0);
				if ( !bWasInside && bInside ) {
					tLeft = pActive[j];
				} else if ( bWasInside && !bInside ) {
					iRet = __xgeShapeExScanMeshAppendBand(pMesh, &tLeft, &pActive[j], fY0, fY1, iColor);
					if ( iRet != XGE_OK ) goto xge_shape_ex_scanline_fail;
				}
				j = k;
			}
		}
	}
	iRet = XGE_OK;
	goto xge_shape_ex_scanline_done;

xge_shape_ex_scanline_fail:
	xrtFree(pMesh->pVertices);
	xrtFree(pMesh->pIndices);
	memset(pMesh, 0, sizeof(*pMesh));
xge_shape_ex_scanline_done:
	xrtFree(pEdges);
	xrtFree(pActive);
	xrtFree(pLevels);
	return iRet;
}

static int __xgeShapeExDrawScanlineFill(xge_shape_ex pShape, const xge_shape_ex_flat_path_t* pFlat, int iFillRule, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, float fOpacity, uint32_t iColor, int bScreenSpace, int bAntialias)
{
	xge_shape_ex_scan_mesh_t tMesh;
	uint8_t* pCoverage;
	int iRet;
	int iPass;

	pCoverage = NULL;
	iRet = __xgeShapeExBuildScanlineFillMesh(pFlat, iFillRule, iColor, &tMesh);
	if ( iRet != XGE_OK ) return iRet;
	if ( (pShape != NULL) && (pShape->iFillType == XGE_SHAPE_EX_FILL_RADIAL_GRADIENT) && (tMesh.iIndexCount > 0) ) {
		int iPassCount = (pShape->iFillGradientSpread == XGE_SHAPE_EX_GRADIENT_SPREAD_PAD) ? 4 : 6;

		iPassCount += __xgeShapeExRadialGradientCorrectionPasses(
			pShape->fFillX1, pShape->fFillY1, pShape->fFillR,
			pShape->fFillFX, pShape->fFillFY, pShape->fFillFR
		);
		for ( iPass = 0; iPass < iPassCount; iPass++ ) {
			iRet = __xgeShapeExSubdivideTriangles(&tMesh.pVertices, &tMesh.iVertexCount, &tMesh.pIndices, &tMesh.iIndexCount);
			if ( iRet != XGE_OK ) goto xge_shape_ex_scanline_draw_done;
		}
	} else if ( (pShape != NULL) && (pShape->iFillType == XGE_SHAPE_EX_FILL_LINEAR_GRADIENT) && (tMesh.iIndexCount > 0) ) {
		int iPassCount = __xgeShapeExLinearGradientSubdivisionPasses(pShape->pFillStops, pShape->iFillStopCount, pShape->iFillGradientSpread);
		iPassCount = __xgeShapeExLinearGradientAdaptivePasses(iPassCount, pShape->iFillGradientSpread,
			pShape->fFillX1, pShape->fFillY1, pShape->fFillX2, pShape->fFillY2,
			pShape->iFillGradientUnits, pShape->tFillGradientTransform,
			tMesh.pVertices, tMesh.iVertexCount, tBounds, tMatrix);

		for ( iPass = 0; iPass < iPassCount; iPass++ ) {
			iRet = __xgeShapeExSubdivideTriangles(&tMesh.pVertices, &tMesh.iVertexCount, &tMesh.pIndices, &tMesh.iIndexCount);
			if ( iRet != XGE_OK ) goto xge_shape_ex_scanline_draw_done;
		}
	}
	pCoverage = __xgeShapeExCoverageFromVertexAlpha(
		tMesh.pVertices, tMesh.iVertexCount, XGE_COLOR_GET_A(iColor)
	);
	if ( (g_xgeShapeExAlphaMaskTargetDepth > 0) && (pCoverage == NULL) &&
	     (XGE_COLOR_GET_A(iColor) > 0) ) {
		iRet = XGE_ERROR_OUT_OF_MEMORY;
		goto xge_shape_ex_scanline_draw_done;
	}
	__xgeShapeExApplyFillPaint(pShape, tMesh.pVertices, tMesh.iVertexCount, tBounds, tMatrix, iColor, fOpacity);
	__xgeShapeExAlphaMaskEncodeVertices(tMesh.pVertices, tMesh.iVertexCount, pCoverage);
	if ( bScreenSpace && bAntialias && (tMesh.iIndexCount > 0) ) {
		iRet = __xgeShapeExDrawSupersampledScanMesh(
			tMesh.pVertices, tMesh.iVertexCount, tMesh.pIndices, tMesh.iIndexCount, tBounds
		);
	} else if ( tMesh.iIndexCount > 0 ) {
		iRet = bScreenSpace ? xgeShapeMeshFillPx(tMesh.pVertices, tMesh.iVertexCount, tMesh.pIndices, tMesh.iIndexCount) :
		                       xgeShapeMeshFill(tMesh.pVertices, tMesh.iVertexCount, tMesh.pIndices, tMesh.iIndexCount);
	} else {
		iRet = XGE_OK;
	}
xge_shape_ex_scanline_draw_done:
	xrtFree(pCoverage);
	xrtFree(tMesh.pVertices);
	xrtFree(tMesh.pIndices);
	return iRet;
}

static int __xgeShapeExDrawCompoundFill(xge_shape_ex pShape, const xge_shape_ex_flat_path_t* pFlat, int iOuter, const int* pParent, const int* pDepth, const float* pAreas, int iFillRule, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, float fOpacity, uint32_t iColor, int bScreenSpace, int bAntialias)
{
	xge_vec2_t* pPoly;
	int iPolyCount;
	int i;
	int iRet;

	pPoly = __xgeShapeExContourCopyOriented(pFlat, iOuter, 1, &iPolyCount);
	if ( pPoly == NULL ) {
		return XGE_OK;
	}
	for ( i = 0; i < pFlat->iContourCount; i++ ) {
		int bHole;

		if ( (pParent[i] != iOuter) || (pDepth[i] != (pDepth[iOuter] + 1)) ) {
			continue;
		}
		bHole = (iFillRule == XGE_SHAPE_EX_FILL_EVEN_ODD) ? 1 : ((pAreas[i] * pAreas[iOuter]) < 0.0f);
		if ( !bHole ) {
			continue;
		}
		iRet = __xgeShapeExBridgeOneHole(&pPoly, &iPolyCount, pFlat, i);
		if ( iRet != XGE_OK ) {
			xrtFree(pPoly);
			return iRet;
		}
	}
	iRet = __xgeShapeExDrawFillMesh(pShape, pPoly, iPolyCount, tBounds, tMatrix, fOpacity, iColor, bScreenSpace, bAntialias);
	xrtFree(pPoly);
	return iRet;
}

static int __xgeShapeExDrawFillCoverageSpans(
	xge_shape_ex pShape,
	const xge_shape_ex_coverage_raster_t* pRaster,
	xge_rect_t tBounds,
	xge_shape_ex_matrix_t tMatrix,
	float fOpacity,
	uint32_t iColor
)
{
	xge_shape_vertex_t* pVertices;
	uint32_t* pIndices;
	int iVertexCount;
	int iIndexCount;
	int iRet;
	int i;

	if ( (pRaster == NULL) || (pRaster->iSpanCount <= 0) ) return XGE_OK;
	if ( (pRaster->iSpanCount > (INT_MAX / 6)) ||
	     ((uint64_t)pRaster->iSpanCount * 4u > UINT32_MAX) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iVertexCount = pRaster->iSpanCount * 4;
	iIndexCount = pRaster->iSpanCount * 6;
	pVertices = (xge_shape_vertex_t*)xrtMalloc((size_t)iVertexCount * sizeof(*pVertices));
	pIndices = (uint32_t*)xrtMalloc((size_t)iIndexCount * sizeof(*pIndices));
	if ( (pVertices == NULL) || (pIndices == NULL) ) {
		xrtFree(pVertices);
		xrtFree(pIndices);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	for ( i = 0; i < pRaster->iSpanCount; i++ ) {
		const xge_shape_ex_coverage_span_t* pSpan = &pRaster->pSpans[i];
		uint32_t iBase = (uint32_t)i * 4u;
		float fLeft = (float)pSpan->iX;
		float fTop = (float)pSpan->iY;
		float fRight = (float)(pSpan->iX + pSpan->iLength);
		float fBottom = (float)(pSpan->iY + 1);
		int iIndexBase = i * 6;

		pVertices[iBase + 0] = (xge_shape_vertex_t){fLeft, fTop, iColor};
		pVertices[iBase + 1] = (xge_shape_vertex_t){fRight, fTop, iColor};
		pVertices[iBase + 2] = (xge_shape_vertex_t){fRight, fBottom, iColor};
		pVertices[iBase + 3] = (xge_shape_vertex_t){fLeft, fBottom, iColor};
		pIndices[iIndexBase + 0] = iBase + 0;
		pIndices[iIndexBase + 1] = iBase + 1;
		pIndices[iIndexBase + 2] = iBase + 2;
		pIndices[iIndexBase + 3] = iBase + 0;
		pIndices[iIndexBase + 4] = iBase + 2;
		pIndices[iIndexBase + 5] = iBase + 3;
	}
	__xgeShapeExApplyFillPaint(pShape, pVertices, iVertexCount, tBounds, tMatrix, iColor, fOpacity);
	for ( i = 0; i < pRaster->iSpanCount; i++ ) {
		const xge_shape_ex_coverage_span_t* pSpan = &pRaster->pSpans[i];
		uint32_t iBase = (uint32_t)i * 4u;
		int j;

		for ( j = 0; j < 4; j++ ) {
			uint32_t iPaintColor = pVertices[iBase + (uint32_t)j].iColor;

			if ( g_xgeShapeExAlphaMaskTargetDepth > 0 ) {
				int iAlpha = XGE_COLOR_GET_A(iPaintColor);
				int iCoverage = (int)pSpan->iCoverage;

				pVertices[iBase + (uint32_t)j].iColor = XGE_COLOR_RGBA(
					iCoverage, iCoverage, iCoverage, iAlpha
				);
			} else {
				int iAlpha = ((int)(iPaintColor & 0xFFu) * (int)pSpan->iCoverage + 127) / 255;

				pVertices[iBase + (uint32_t)j].iColor =
					(iPaintColor & 0xFFFFFF00u) | (uint32_t)iAlpha;
			}
		}
	}
	iRet = xgeShapeMeshFillPx(pVertices, iVertexCount, pIndices, iIndexCount);
	xrtFree(pVertices);
	xrtFree(pIndices);
	return iRet;
}

static int __xgeShapeExDrawFillAreaCoverage(
	xge_shape_ex pShape,
	const xge_shape_ex_flat_path_t* pFlat,
	int iFillRule,
	xge_rect_t tBounds,
	xge_shape_ex_matrix_t tMatrix,
	float fOpacity,
	uint32_t iColor,
	int bUsePathCommands,
	int bBinaryCoverage
)
{
	xge_shape_ex_coverage_contour_t* pContours = NULL;
	xge_shape_ex_coverage_raster_t tRaster;
	xge_rect_t tClip;
	float fMinX;
	float fMinY;
	float fMaxX;
	float fMaxY;
	int iMinX;
	int iMinY;
	int iMaxX;
	int iMaxY;
	int iContourCount;
	int iRet;
	int i;

	if ( (pFlat == NULL) || (pFlat->iContourCount <= 0) ) return XGE_OK;
	iContourCount = 0;
	if ( !bUsePathCommands ) {
		if ( (size_t)pFlat->iContourCount > (SIZE_MAX / sizeof(*pContours)) ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		pContours = (xge_shape_ex_coverage_contour_t*)xrtMalloc(
			(size_t)pFlat->iContourCount * sizeof(*pContours)
		);
		if ( pContours == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
		for ( i = 0; i < pFlat->iContourCount; i++ ) {
			const xge_shape_ex_flat_contour_t* pContour = &pFlat->pContours[i];

			if ( pContour->iCount < 3 ) continue;
			pContours[iContourCount].pPoints = pFlat->pPoints + pContour->iStart;
			pContours[iContourCount].iPointCount = pContour->iCount;
			iContourCount++;
		}
		if ( iContourCount <= 0 ) {
			xrtFree(pContours);
			return XGE_OK;
		}
	}
	fMinX = tBounds.fX;
	fMinY = tBounds.fY;
	fMaxX = tBounds.fX + tBounds.fW;
	fMaxY = tBounds.fY + tBounds.fH;
	if ( !__xgeShapeExFloatFinite(fMinX) || !__xgeShapeExFloatFinite(fMinY) ||
	     !__xgeShapeExFloatFinite(fMaxX) || !__xgeShapeExFloatFinite(fMaxY) ) {
		xrtFree(pContours);
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fMinX < 0.0f ) fMinX = 0.0f;
	if ( fMinY < 0.0f ) fMinY = 0.0f;
	if ( fMaxX > (float)g_xge.iWidth ) fMaxX = (float)g_xge.iWidth;
	if ( fMaxY > (float)g_xge.iHeight ) fMaxY = (float)g_xge.iHeight;
	if ( g_xge.bClipEnabled ) {
		tClip = xgeClipGet();
		if ( fMinX < tClip.fX ) fMinX = tClip.fX;
		if ( fMinY < tClip.fY ) fMinY = tClip.fY;
		if ( fMaxX > (tClip.fX + tClip.fW) ) fMaxX = tClip.fX + tClip.fW;
		if ( fMaxY > (tClip.fY + tClip.fH) ) fMaxY = tClip.fY + tClip.fH;
	}
	if ( (fMaxX <= fMinX) || (fMaxY <= fMinY) ) {
		xrtFree(pContours);
		return XGE_OK;
	}
	iMinX = (int)floorf(fMinX);
	iMinY = (int)floorf(fMinY);
	iMaxX = (int)ceilf(fMaxX);
	iMaxY = (int)ceilf(fMaxY);
	memset(&tRaster, 0, sizeof(tRaster));
	if ( bUsePathCommands && __xgeShapeExValid(pShape) ) {
		iRet = __xgeShapeExCoverageRasterizePath(
			&tRaster, pShape->pCommands, pShape->iCommandCount,
			pShape->pPoints, pShape->iPointCount, tMatrix,
			iMinX, iMinY, iMaxX, iMaxY,
			iFillRule == XGE_SHAPE_EX_FILL_EVEN_ODD
		);
	} else {
		iRet = __xgeShapeExCoverageRasterizeContours(
			&tRaster, pContours, iContourCount, iMinX, iMinY, iMaxX, iMaxY,
			iFillRule == XGE_SHAPE_EX_FILL_EVEN_ODD
		);
	}
	if ( iRet == XGE_OK ) {
		if ( bBinaryCoverage ) {
			for ( i = 0; i < tRaster.iSpanCount; i++ ) {
				tRaster.pSpans[i].iCoverage = 255u;
			}
		}
		iRet = __xgeShapeExDrawFillCoverageSpans(
			pShape, &tRaster, tBounds, tMatrix, fOpacity, iColor
		);
	}
	__xgeShapeExCoverageRasterFree(&tRaster);
	xrtFree(pContours);
	return iRet;
}

static int __xgeShapeExDrawFill(xge_shape_ex pShape, const xge_shape_ex_flat_path_t* pFlat, int iFillRule, xge_shape_ex_matrix_t tMatrix, float fOpacity, uint32_t iColor, int bScreenSpace, int bAntialias, int bUsePathCoverage, int bBinaryCoverage)
{
	int i;
	int iRet;
	int* pParent;
	int* pDepth;
	float* pAreas;
	int bHasNested;
	int bHasIntersectingEdges;
	int bSingleAxisAlignedRect;
	xge_rect_t tBounds;

	if ( (pFlat == NULL) || !__xgeShapeExFillVisible(pShape, iColor) ) {
		return XGE_OK;
	}
	tBounds = __xgeShapeExFlatBounds(pFlat);
	bHasIntersectingEdges = __xgeShapeExFlatHasIntersectingEdges(pFlat);
	bSingleAxisAlignedRect = (pFlat->iContourCount == 1) &&
		(pFlat->pContours[0].iCount >= 3) &&
		__xgeShapeExContourIsAxisAlignedRect(
			pFlat->pPoints + pFlat->pContours[0].iStart,
			pFlat->pContours[0].iCount
		);
	if ( bScreenSpace && (bAntialias || bBinaryCoverage) &&
	     ((pShape == NULL) || (pShape->iFillType == XGE_SHAPE_EX_FILL_SOLID)) &&
	     ((bBinaryCoverage || !bSingleAxisAlignedRect ||
	       ((pShape != NULL) && pShape->bAxisAlignedFillAntialias)) ||
	      (pFlat->iContourCount > 1) || bHasIntersectingEdges) ) {
		return __xgeShapeExDrawFillAreaCoverage(
			pShape, pFlat, iFillRule, tBounds, tMatrix, fOpacity, iColor,
			bUsePathCoverage, bBinaryCoverage
		);
	}
	if ( bHasIntersectingEdges ) {
		return __xgeShapeExDrawScanlineFill(pShape, pFlat, iFillRule, tBounds, tMatrix, fOpacity, iColor, bScreenSpace, bAntialias);
	}
	pParent = (int*)xrtMalloc((size_t)pFlat->iContourCount * sizeof(*pParent));
	pDepth = (int*)xrtMalloc((size_t)pFlat->iContourCount * sizeof(*pDepth));
	pAreas = (float*)xrtMalloc((size_t)pFlat->iContourCount * sizeof(*pAreas));
	if ( (pParent == NULL) || (pDepth == NULL) || (pAreas == NULL) ) {
		xrtFree(pParent);
		xrtFree(pDepth);
		xrtFree(pAreas);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	bHasNested = 0;
	for ( i = 0; i < pFlat->iContourCount; i++ ) {
		const xge_shape_ex_flat_contour_t* pContour = &pFlat->pContours[i];
		const xge_vec2_t* pPoints = pFlat->pPoints + pContour->iStart;
		float fBestArea = FLT_MAX;
		int j;

		pParent[i] = -1;
		pDepth[i] = 0;
		pAreas[i] = 0.0f;
		if ( pContour->iCount < 3 ) {
			continue;
		}
		pAreas[i] = __xgeShapeExContourArea(pPoints, pContour->iCount);
		for ( j = 0; j < pFlat->iContourCount; j++ ) {
			const xge_shape_ex_flat_contour_t* pOther = &pFlat->pContours[j];
			const xge_vec2_t* pOtherPoints = pFlat->pPoints + pOther->iStart;
			float fOtherArea;

			if ( (i == j) || (pOther->iCount < 3) ) {
				continue;
			}
			fOtherArea = fabsf(__xgeShapeExContourArea(pOtherPoints, pOther->iCount));
			if ( fOtherArea <= fabsf(pAreas[i]) + XGE_SHAPE_EX_EPSILON ) {
				continue;
			}
			if ( !__xgeShapeExPointInContour(pPoints[0], pOtherPoints, pOther->iCount) ) {
				continue;
			}
			if ( fOtherArea < fBestArea ) {
				fBestArea = fOtherArea;
				pParent[i] = j;
			}
		}
	}
	for ( i = 0; i < pFlat->iContourCount; i++ ) {
		int iParent = pParent[i];
		while ( iParent >= 0 ) {
			pDepth[i]++;
			iParent = pParent[iParent];
		}
		if ( pDepth[i] > 0 ) {
			bHasNested = 1;
		}
	}
	if ( !bHasNested ) {
		for ( i = 0; i < pFlat->iContourCount; i++ ) {
			const xge_shape_ex_flat_contour_t* pContour = &pFlat->pContours[i];

			if ( pContour->iCount < 3 ) {
				continue;
			}
				iRet = __xgeShapeExDrawFillMesh(pShape, pFlat->pPoints + pContour->iStart, pContour->iCount, tBounds, tMatrix, fOpacity, iColor, bScreenSpace, bAntialias);
			if ( iRet != XGE_OK ) {
				xrtFree(pParent);
				xrtFree(pDepth);
				xrtFree(pAreas);
				return iRet;
			}
		}
		xrtFree(pParent);
		xrtFree(pDepth);
		xrtFree(pAreas);
		return XGE_OK;
	}
	for ( i = 0; i < pFlat->iContourCount; i++ ) {
		const xge_shape_ex_flat_contour_t* pContour = &pFlat->pContours[i];

		if ( (pContour->iCount < 3) || ((pDepth[i] % 2) != 0) ) {
			continue;
		}
		iRet = __xgeShapeExDrawCompoundFill(pShape, pFlat, i, pParent, pDepth, pAreas, iFillRule, tBounds, tMatrix, fOpacity, iColor, bScreenSpace, bAntialias);
		if ( iRet != XGE_OK ) {
			xrtFree(pParent);
			xrtFree(pDepth);
			xrtFree(pAreas);
			return iRet;
		}
	}
	xrtFree(pParent);
	xrtFree(pDepth);
	xrtFree(pAreas);
	return XGE_OK;
}

static void __xgeShapeExTransformVertices(xge_shape_vertex_t* pVertices, int iVertexCount, xge_shape_ex_matrix_t tMatrix)
{
	int i;

	if ( pVertices == NULL ) {
		return;
	}
	for ( i = 0; i < iVertexCount; i++ ) {
		xge_vec2_t tPoint;

		tPoint.fX = pVertices[i].fX;
		tPoint.fY = pVertices[i].fY;
		tPoint = __xgeShapeExMatrixPoint(tMatrix, tPoint);
		pVertices[i].fX = tPoint.fX;
		pVertices[i].fY = tPoint.fY;
	}
}

static float __xgeShapeExStrokeCoverageHalfWidth(const xge_shape_ex_matrix_t* pGeometryMatrix)
{
	float fAAHalfWidth = XGE_SHAPE_EX_AA_HALF_WIDTH;

	if ( pGeometryMatrix != NULL ) {
		float fGeometryScale = __xgeShapeExMatrixStrokeScale(*pGeometryMatrix);

		if ( fGeometryScale > XGE_SHAPE_EX_EPSILON ) {
			fAAHalfWidth /= fGeometryScale;
		}
	}
	return fAAHalfWidth;
}

static int __xgeShapeExStrokeCoverageNeedsScreenSpace(const xge_shape_ex_matrix_t* pGeometryMatrix)
{
	float fXX;
	float fYY;
	float fXY;
	float fScale;

	if ( pGeometryMatrix == NULL ) return 0;
	fXX = pGeometryMatrix->fA * pGeometryMatrix->fA + pGeometryMatrix->fB * pGeometryMatrix->fB;
	fYY = pGeometryMatrix->fC * pGeometryMatrix->fC + pGeometryMatrix->fD * pGeometryMatrix->fD;
	fXY = pGeometryMatrix->fA * pGeometryMatrix->fC + pGeometryMatrix->fB * pGeometryMatrix->fD;
	fScale = fmaxf(1.0f, fmaxf(fXX, fYY));
	return (fabsf(fXX - fYY) > fScale * 0.0001f) || (fabsf(fXY) > fScale * 0.0001f);
}

static void __xgeShapeExStrokeCoverageScreenPair(
	const xge_vec2_t* pBoundary,
	const xge_vec2_t* pCenters,
	int iCount,
	int iIndex,
	xge_vec2_t* pOpaque,
	xge_vec2_t* pTransparent
)
{
	int iPrev = (iIndex + iCount - 1) % iCount;
	int iNext = (iIndex + 1) % iCount;
	float fTX = pBoundary[iNext].fX - pBoundary[iPrev].fX;
	float fTY = pBoundary[iNext].fY - pBoundary[iPrev].fY;
	float fLength = hypotf(fTX, fTY);
	float fNX;
	float fNY;
	float fToCenterX;
	float fToCenterY;

	if ( fLength <= XGE_SHAPE_EX_EPSILON ) {
		fTX = pBoundary[iNext].fX - pBoundary[iIndex].fX;
		fTY = pBoundary[iNext].fY - pBoundary[iIndex].fY;
		fLength = hypotf(fTX, fTY);
	}
	if ( fLength <= XGE_SHAPE_EX_EPSILON ) {
		fTX = pBoundary[iIndex].fX - pBoundary[iPrev].fX;
		fTY = pBoundary[iIndex].fY - pBoundary[iPrev].fY;
		fLength = hypotf(fTX, fTY);
	}
	if ( fLength <= XGE_SHAPE_EX_EPSILON ) {
		*pOpaque = pBoundary[iIndex];
		*pTransparent = pBoundary[iIndex];
		return;
	}
	fNX = -fTY / fLength;
	fNY = fTX / fLength;
	fToCenterX = pCenters[iIndex].fX - pBoundary[iIndex].fX;
	fToCenterY = pCenters[iIndex].fY - pBoundary[iIndex].fY;
	if ( fNX * fToCenterX + fNY * fToCenterY < 0.0f ) {
		fNX = -fNX;
		fNY = -fNY;
	}
	pOpaque->fX = pBoundary[iIndex].fX + fNX * XGE_SHAPE_EX_AA_HALF_WIDTH;
	pOpaque->fY = pBoundary[iIndex].fY + fNY * XGE_SHAPE_EX_AA_HALF_WIDTH;
	pTransparent->fX = pBoundary[iIndex].fX - fNX * XGE_SHAPE_EX_AA_HALF_WIDTH;
	pTransparent->fY = pBoundary[iIndex].fY - fNY * XGE_SHAPE_EX_AA_HALF_WIDTH;
}

static int __xgeShapeExDrawStrokePaintMeshEx(xge_shape_ex pShape, xge_shape_vertex_t* pVertices, int iVertexCount, const uint32_t* pIndices, int iIndexCount, uint32_t iColor, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, const xge_shape_ex_matrix_t* pGeometryMatrix, float fOpacity, int bScreenSpace, int bPreserveCoverage)
{
	xge_shape_vertex_t* pDrawVertices;
	uint32_t* pDrawIndices;
	uint8_t* pCoverage;
	int iDrawVertexCount;
	int iDrawIndexCount;
	int bUseTempMesh;
	int iRet;

	if ( (pVertices == NULL) || (iVertexCount <= 0) || (pIndices == NULL) || (iIndexCount <= 0) ) {
		return XGE_OK;
	}
	pDrawVertices = pVertices;
	pDrawIndices = (uint32_t*)pIndices;
	iDrawVertexCount = iVertexCount;
	iDrawIndexCount = iIndexCount;
	bUseTempMesh = 0;
	pCoverage = NULL;
	if ( (pShape != NULL) && ((pShape->iStrokeType == XGE_SHAPE_EX_FILL_RADIAL_GRADIENT) || (pShape->iStrokeType == XGE_SHAPE_EX_FILL_LINEAR_GRADIENT)) && (iIndexCount >= 3) ) {
		int iPass;
		int iPassCount;

		if ( pShape->iStrokeType == XGE_SHAPE_EX_FILL_RADIAL_GRADIENT ) {
			iPassCount = (pShape->iStrokeGradientSpread == XGE_SHAPE_EX_GRADIENT_SPREAD_PAD) ? 5 : 6;
			iPassCount += __xgeShapeExRadialGradientCorrectionPasses(
				pShape->fStrokeX1, pShape->fStrokeY1, pShape->fStrokeR,
				pShape->fStrokeFX, pShape->fStrokeFY, pShape->fStrokeFR
			);
		} else {
			iPassCount = __xgeShapeExLinearGradientSubdivisionPasses(pShape->pStrokeStops, pShape->iStrokeStopCount, pShape->iStrokeGradientSpread);
			iPassCount = __xgeShapeExLinearGradientAdaptivePasses(iPassCount, pShape->iStrokeGradientSpread,
				pShape->fStrokeX1, pShape->fStrokeY1, pShape->fStrokeX2, pShape->fStrokeY2,
				pShape->iStrokeGradientUnits, pShape->tStrokeGradientTransform,
				pVertices, iVertexCount, tBounds, tMatrix);
		}
		if ( iPassCount <= 0 ) {
			goto xge_shape_ex_stroke_paint_no_subdivide;
		}
		pDrawVertices = (xge_shape_vertex_t*)xrtMalloc((size_t)iVertexCount * sizeof(*pDrawVertices));
		pDrawIndices = (uint32_t*)xrtMalloc((size_t)iIndexCount * sizeof(*pDrawIndices));
		if ( (pDrawVertices == NULL) || (pDrawIndices == NULL) ) {
			xrtFree(pDrawVertices);
			xrtFree(pDrawIndices);
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		memcpy(pDrawVertices, pVertices, (size_t)iVertexCount * sizeof(*pDrawVertices));
		memcpy(pDrawIndices, pIndices, (size_t)iIndexCount * sizeof(*pDrawIndices));
		bUseTempMesh = 1;
		for ( iPass = 0; iPass < iPassCount; iPass++ ) {
			iRet = __xgeShapeExSubdivideTriangles(&pDrawVertices, &iDrawVertexCount, &pDrawIndices, &iDrawIndexCount);
			if ( iRet != XGE_OK ) {
				xrtFree(pDrawVertices);
				xrtFree(pDrawIndices);
				return iRet;
			}
		}
	}
xge_shape_ex_stroke_paint_no_subdivide:
	if ( bPreserveCoverage || (g_xgeShapeExAlphaMaskTargetDepth > 0) ) {
		int iBaseAlpha = (int)(iColor & 0xFFu);
		int i;

		if ( iBaseAlpha > 0 ) {
			pCoverage = (uint8_t*)xrtMalloc((size_t)iDrawVertexCount * sizeof(*pCoverage));
			if ( pCoverage == NULL ) {
				if ( bUseTempMesh ) {
					xrtFree(pDrawVertices);
					xrtFree(pDrawIndices);
				}
				return XGE_ERROR_OUT_OF_MEMORY;
			}
			for ( i = 0; i < iDrawVertexCount; i++ ) {
				int iAlpha = (int)(pDrawVertices[i].iColor & 0xFFu);
				int iCoverage = (iAlpha * 255 + iBaseAlpha / 2) / iBaseAlpha;

				if ( iCoverage > 255 ) iCoverage = 255;
				pCoverage[i] = (uint8_t)iCoverage;
			}
		}
	}
	__xgeShapeExApplyStrokePaint(pShape, pDrawVertices, iDrawVertexCount, tBounds, tMatrix, iColor, fOpacity);
	if ( pCoverage != NULL ) {
		int i;

		for ( i = 0; i < iDrawVertexCount; i++ ) {
			if ( g_xgeShapeExAlphaMaskTargetDepth > 0 ) {
				int iAlpha = XGE_COLOR_GET_A(pDrawVertices[i].iColor);
				int iCoverage = (int)pCoverage[i];

				pDrawVertices[i].iColor = XGE_COLOR_RGBA(
					iCoverage, iCoverage, iCoverage, iAlpha
				);
			} else {
				int iAlpha = (int)(pDrawVertices[i].iColor & 0xFFu);
				int iCoveredAlpha = (iAlpha * (int)pCoverage[i] + 127) / 255;

				pDrawVertices[i].iColor =
					(pDrawVertices[i].iColor & 0xFFFFFF00u) | (uint32_t)iCoveredAlpha;
			}
		}
	}
	if ( pGeometryMatrix != NULL ) {
		__xgeShapeExTransformVertices(pDrawVertices, iDrawVertexCount, *pGeometryMatrix);
	}
	iRet = bScreenSpace ? xgeShapeMeshFillPx(pDrawVertices, iDrawVertexCount, pDrawIndices, iDrawIndexCount) : xgeShapeMeshFill(pDrawVertices, iDrawVertexCount, pDrawIndices, iDrawIndexCount);
	xrtFree(pCoverage);
	if ( bUseTempMesh ) {
		xrtFree(pDrawVertices);
		xrtFree(pDrawIndices);
	}
	return iRet;
}

static int __xgeShapeExDrawStrokePaintMesh(xge_shape_ex pShape, xge_shape_vertex_t* pVertices, int iVertexCount, const uint32_t* pIndices, int iIndexCount, uint32_t iColor, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, const xge_shape_ex_matrix_t* pGeometryMatrix, float fOpacity, int bScreenSpace)
{
	return __xgeShapeExDrawStrokePaintMeshEx(
		pShape, pVertices, iVertexCount, pIndices, iIndexCount, iColor, tBounds, tMatrix,
		pGeometryMatrix, fOpacity, bScreenSpace, 0
	);
}

static int __xgeShapeExBuildStrokeAAMesh(
	const xge_vec2_t* pPoints,
	int iPointCount,
	uint32_t iColor,
	float fAAHalfWidth,
	const xge_shape_ex_matrix_t* pGeometryMatrix,
	xge_shape_vertex_t** ppVertices,
	int* pVertexCount,
	uint32_t** ppIndices,
	int* pIndexCount,
	int* pContourCount,
	int* pOuterVertexStart
)
{
	xge_shape_ex_matrix_t tInverse;
	xge_vec2_t* pScreenPoints;
	int iRet;
	int i;

	if ( !__xgeShapeExStrokeCoverageNeedsScreenSpace(pGeometryMatrix) ||
	     !__xgeShapeExMatrixInverse(*pGeometryMatrix, &tInverse) ) {
		return __xgeShapeExBuildSimpleAAMesh(
			pPoints, iPointCount, iColor, fAAHalfWidth, 0, ppVertices, pVertexCount,
			ppIndices, pIndexCount, pContourCount, pOuterVertexStart
		);
	}
	pScreenPoints = (xge_vec2_t*)xrtMalloc((size_t)iPointCount * sizeof(*pScreenPoints));
	if ( pScreenPoints == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	for ( i = 0; i < iPointCount; i++ ) {
		pScreenPoints[i] = __xgeShapeExMatrixPoint(*pGeometryMatrix, pPoints[i]);
	}
	iRet = __xgeShapeExBuildSimpleAAMesh(
		pScreenPoints, iPointCount, iColor, XGE_SHAPE_EX_AA_HALF_WIDTH, 0,
		ppVertices, pVertexCount, ppIndices, pIndexCount, pContourCount, pOuterVertexStart
	);
	xrtFree(pScreenPoints);
	if ( (iRet != XGE_OK) || (*ppVertices == NULL) ) return iRet;
	__xgeShapeExTransformVertices(*ppVertices, *pVertexCount, tInverse);
	return XGE_OK;
}

static int __xgeShapeExDrawStrokeCoverageContour(xge_shape_ex pShape, const xge_vec2_t* pPoints, int iPointCount, float fAAHalfWidth, uint32_t iColor, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, const xge_shape_ex_matrix_t* pGeometryMatrix, float fOpacity, int bScreenSpace, int* pDrawn)
{
	xge_shape_vertex_t* pVertices;
	uint32_t* pIndices;
	int iVertexCount;
	int iIndexCount;
	int iContourCount;
	int iOuterVertexStart;
	int iRet;

	if ( pDrawn == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	*pDrawn = 0;
	pVertices = NULL;
	pIndices = NULL;
	iVertexCount = 0;
	iIndexCount = 0;
	iContourCount = 0;
	iOuterVertexStart = 0;
	iRet = __xgeShapeExBuildStrokeAAMesh(
		pPoints, iPointCount, iColor, fAAHalfWidth, pGeometryMatrix, &pVertices, &iVertexCount,
		&pIndices, &iIndexCount, &iContourCount, &iOuterVertexStart
	);
	if ( iRet != XGE_OK ) return iRet;
	if ( pVertices == NULL ) return XGE_OK;
	iRet = __xgeShapeExDrawStrokePaintMeshEx(
		pShape, pVertices, iVertexCount, pIndices, iIndexCount, iColor, tBounds, tMatrix,
		pGeometryMatrix, fOpacity, bScreenSpace, 1
	);
	xrtFree(pVertices);
	xrtFree(pIndices);
	if ( iRet == XGE_OK ) *pDrawn = 1;
	return iRet;
}

static int __xgeShapeExStrokeCircleSegments(float fRadius, const xge_shape_ex_matrix_t* pGeometryMatrix);
static int __xgeShapeExDrawStrokeArcFeather(xge_shape_ex pShape, xge_vec2_t tCenter, float fStart, float fDelta, float fRadius, int iSegments, uint32_t iColor, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, const xge_shape_ex_matrix_t* pGeometryMatrix, float fOpacity, int bScreenSpace);

static int __xgeShapeExDrawStrokeRoundCap(xge_shape_ex pShape, xge_vec2_t tCenter, float fDirX, float fDirY, float fHalfWidth, int bEndCap, uint32_t iColor, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, const xge_shape_ex_matrix_t* pGeometryMatrix, float fOpacity, int bScreenSpace)
{
	xge_shape_vertex_t tVertices[66];
	uint32_t iIndices[192];
	float fBase;
	float fStart;
	int iSegments;
	int iRet;
	int i;

	if ( (pShape == NULL) || (fHalfWidth <= XGE_SHAPE_EX_EPSILON) ) {
		return XGE_OK;
	}
	iSegments = (__xgeShapeExStrokeCircleSegments(fHalfWidth, pGeometryMatrix) + 1) / 2;
	fBase = atan2f(fDirY, fDirX);
	fStart = bEndCap ? (fBase - XGE_SHAPE_EX_PI * 0.5f) : (fBase + XGE_SHAPE_EX_PI * 0.5f);
	tVertices[0].fX = tCenter.fX;
	tVertices[0].fY = tCenter.fY;
	tVertices[0].iColor = iColor;
	for ( i = 0; i <= iSegments; i++ ) {
		float fA = fStart + ((float)i / (float)iSegments) * XGE_SHAPE_EX_PI;

		tVertices[i + 1].fX = tCenter.fX + cosf(fA) * fHalfWidth;
		tVertices[i + 1].fY = tCenter.fY + sinf(fA) * fHalfWidth;
		tVertices[i + 1].iColor = iColor;
	}
	for ( i = 0; i < iSegments; i++ ) {
		iIndices[i * 3 + 0] = 0;
		iIndices[i * 3 + 1] = (uint32_t)(i + 1);
		iIndices[i * 3 + 2] = (uint32_t)(i + 2);
	}
	iRet = __xgeShapeExDrawStrokePaintMesh(pShape, tVertices, iSegments + 2, iIndices, iSegments * 3, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
	if ( (iRet != XGE_OK) || pShape->bCoverageAntialiasDisabled ) return iRet;
	return __xgeShapeExDrawStrokeArcFeather(
		pShape, tCenter, fStart, XGE_SHAPE_EX_PI, fHalfWidth, iSegments, iColor, tBounds, tMatrix,
		pGeometryMatrix, fOpacity, bScreenSpace
	);
}

static int __xgeShapeExStrokeCircleSegments(float fRadius, const xge_shape_ex_matrix_t* pGeometryMatrix)
{
	float fScreenRadius = fabsf(fRadius);
	float fStep;
	int iSegments;

	if ( pGeometryMatrix != NULL ) {
		float fScaleX = hypotf(pGeometryMatrix->fA, pGeometryMatrix->fB);
		float fScaleY = hypotf(pGeometryMatrix->fC, pGeometryMatrix->fD);

		fScreenRadius *= fmaxf(fScaleX, fScaleY);
	}
	if ( fScreenRadius <= 0.25f ) return 24;
	fStep = 2.0f * acosf(fmaxf(-1.0f, fminf(1.0f, 1.0f - 0.25f / fScreenRadius)));
	if ( fStep <= XGE_SHAPE_EX_EPSILON ) return 128;
	iSegments = (int)ceilf(XGE_SHAPE_EX_TAU / fStep);
	if ( iSegments < 24 ) iSegments = 24;
	if ( iSegments > 128 ) iSegments = 128;
	return iSegments;
}

static int __xgeShapeExDrawStrokeArcFeather(xge_shape_ex pShape, xge_vec2_t tCenter, float fStart, float fDelta, float fRadius, int iSegments, uint32_t iColor, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, const xge_shape_ex_matrix_t* pGeometryMatrix, float fOpacity, int bScreenSpace)
{
	xge_shape_vertex_t tVertices[130];
	uint32_t iIndices[384];
	float fOuterRadius;
	int i;

	if ( (pShape == NULL) || (iSegments <= 0) || (iSegments > 64) ||
	     (fRadius <= XGE_SHAPE_EX_EPSILON) ) return XGE_OK;
	fOuterRadius = fRadius + __xgeShapeExStrokeCoverageHalfWidth(pGeometryMatrix);
	for ( i = 0; i <= iSegments; i++ ) {
		float fAngle = fStart + fDelta * ((float)i / (float)iSegments);
		float fCos = cosf(fAngle);
		float fSin = sinf(fAngle);

		tVertices[i * 2 + 0] = (xge_shape_vertex_t){
			tCenter.fX + fCos * fRadius,
			tCenter.fY + fSin * fRadius,
			iColor
		};
		tVertices[i * 2 + 1] = (xge_shape_vertex_t){
			tCenter.fX + fCos * fOuterRadius,
			tCenter.fY + fSin * fOuterRadius,
			iColor & 0xFFFFFF00u
		};
	}
	for ( i = 0; i < iSegments; i++ ) {
		int iBase = i * 6;
		uint32_t iInner = (uint32_t)(i * 2);
		uint32_t iNextInner = iInner + 2u;

		iIndices[iBase + 0] = iInner;
		iIndices[iBase + 1] = iNextInner;
		iIndices[iBase + 2] = iNextInner + 1u;
		iIndices[iBase + 3] = iInner;
		iIndices[iBase + 4] = iNextInner + 1u;
		iIndices[iBase + 5] = iInner + 1u;
	}
	return __xgeShapeExDrawStrokePaintMeshEx(
		pShape, tVertices, (iSegments + 1) * 2, iIndices, iSegments * 6, iColor, tBounds, tMatrix,
		pGeometryMatrix, fOpacity, bScreenSpace, 1
	);
}

static int __xgeShapeExDrawStrokeZeroLengthCap(xge_shape_ex pShape, xge_vec2_t tCenter, float fWidth, uint32_t iColor, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, const xge_shape_ex_matrix_t* pGeometryMatrix, float fOpacity, int bScreenSpace)
{
	float fHalfWidth;
	int i;

	if ( (pShape == NULL) || (fWidth <= XGE_SHAPE_EX_EPSILON) ) {
		return XGE_OK;
	}
	if ( pShape->iLineCap == XGE_SHAPE_EX_CAP_BUTT ) {
		return XGE_OK;
	}
	fHalfWidth = fWidth * 0.5f;
	if ( pShape->iLineCap == XGE_SHAPE_EX_CAP_ROUND ) {
		xge_shape_vertex_t tVertices[130];
		uint32_t iIndices[384];
		int iSegments = __xgeShapeExStrokeCircleSegments(fHalfWidth, pGeometryMatrix);
		xge_vec2_t arrContour[128];

		for ( i = 0; i < iSegments; i++ ) {
			float fA = ((float)i / (float)iSegments) * XGE_SHAPE_EX_TAU;

			arrContour[i].fX = tCenter.fX + cosf(fA) * fHalfWidth;
			arrContour[i].fY = tCenter.fY + sinf(fA) * fHalfWidth;
		}
		if ( !pShape->bCoverageAntialiasDisabled ) {
			int bDrawn;
			int iRet = __xgeShapeExDrawStrokeCoverageContour(
				pShape, arrContour, iSegments, __xgeShapeExStrokeCoverageHalfWidth(pGeometryMatrix),
				iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace, &bDrawn
			);

			if ( (iRet != XGE_OK) || bDrawn ) return iRet;
		}

		tVertices[0].fX = tCenter.fX;
		tVertices[0].fY = tCenter.fY;
		tVertices[0].iColor = iColor;
		for ( i = 0; i <= iSegments; i++ ) {
			float fA = ((float)i / (float)iSegments) * XGE_SHAPE_EX_TAU;
			tVertices[i + 1].fX = tCenter.fX + cosf(fA) * fHalfWidth;
			tVertices[i + 1].fY = tCenter.fY + sinf(fA) * fHalfWidth;
			tVertices[i + 1].iColor = iColor;
		}
		for ( i = 0; i < iSegments; i++ ) {
			iIndices[i * 3 + 0] = 0;
			iIndices[i * 3 + 1] = (uint32_t)(i + 1);
			iIndices[i * 3 + 2] = (uint32_t)(i + 2);
		}
		return __xgeShapeExDrawStrokePaintMesh(pShape, tVertices, iSegments + 2, iIndices, iSegments * 3, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
	}
	{
		xge_shape_vertex_t tVertices[4];
		uint32_t iIndices[6];
		xge_vec2_t arrContour[4];
		int bPixelAligned;

		tVertices[0].fX = tCenter.fX - fHalfWidth; tVertices[0].fY = tCenter.fY - fHalfWidth; tVertices[0].iColor = iColor;
		tVertices[1].fX = tCenter.fX + fHalfWidth; tVertices[1].fY = tCenter.fY - fHalfWidth; tVertices[1].iColor = iColor;
		tVertices[2].fX = tCenter.fX + fHalfWidth; tVertices[2].fY = tCenter.fY + fHalfWidth; tVertices[2].iColor = iColor;
		tVertices[3].fX = tCenter.fX - fHalfWidth; tVertices[3].fY = tCenter.fY + fHalfWidth; tVertices[3].iColor = iColor;
		for ( i = 0; i < 4; i++ ) {
			arrContour[i].fX = tVertices[i].fX;
			arrContour[i].fY = tVertices[i].fY;
		}
		bPixelAligned = 1;
		if ( (pGeometryMatrix != NULL) &&
		     ((fabsf(pGeometryMatrix->fB) > XGE_SHAPE_EX_EPSILON) || (fabsf(pGeometryMatrix->fC) > XGE_SHAPE_EX_EPSILON)) ) {
			bPixelAligned = 0;
		}
		for ( i = 0; bPixelAligned && (i < 4); i++ ) {
			xge_vec2_t tPoint = arrContour[i];

			if ( pGeometryMatrix != NULL ) tPoint = __xgeShapeExMatrixPoint(*pGeometryMatrix, tPoint);
			if ( (fabsf(tPoint.fX - roundf(tPoint.fX)) > 0.0005f) ||
			     (fabsf(tPoint.fY - roundf(tPoint.fY)) > 0.0005f) ) bPixelAligned = 0;
		}
		if ( !pShape->bCoverageAntialiasDisabled && !bPixelAligned ) {
			int bDrawn;
			int iRet = __xgeShapeExDrawStrokeCoverageContour(
				pShape, arrContour, 4, __xgeShapeExStrokeCoverageHalfWidth(pGeometryMatrix),
				iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace, &bDrawn
			);

			if ( (iRet != XGE_OK) || bDrawn ) return iRet;
		}
		iIndices[0] = 0; iIndices[1] = 1; iIndices[2] = 2;
		iIndices[3] = 0; iIndices[4] = 2; iIndices[5] = 3;
		return __xgeShapeExDrawStrokePaintMesh(pShape, tVertices, 4, iIndices, 6, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
	}
}

static int __xgeShapeExLineIntersection(xge_vec2_t tP, xge_vec2_t tR, xge_vec2_t tQ, xge_vec2_t tS, xge_vec2_t* pOut)
{
	float fDen;
	float fT;

	if ( pOut == NULL ) {
		return 0;
	}
	fDen = tR.fX * tS.fY - tR.fY * tS.fX;
	if ( fabsf(fDen) <= XGE_SHAPE_EX_EPSILON ) {
		return 0;
	}
	fT = ((tQ.fX - tP.fX) * tS.fY - (tQ.fY - tP.fY) * tS.fX) / fDen;
	pOut->fX = tP.fX + tR.fX * fT;
	pOut->fY = tP.fY + tR.fY * fT;
	return 1;
}

static int __xgeShapeExDrawStrokePolylineFeather(xge_shape_ex pShape, const xge_vec2_t* pPoints, int iPointCount, xge_vec2_t tInterior, uint32_t iColor, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, const xge_shape_ex_matrix_t* pGeometryMatrix, float fOpacity, int bScreenSpace)
{
	xge_shape_vertex_t tVertices[6];
	uint32_t iIndices[12];
	xge_vec2_t tOuter[3];
	xge_vec2_t tNormals[2];
	xge_vec2_t tDirections[2];
	float fAAHalfWidth;
	int i;

	if ( (pShape == NULL) || (pPoints == NULL) || (iPointCount < 2) || (iPointCount > 3) ) {
		return XGE_OK;
	}
	fAAHalfWidth = __xgeShapeExStrokeCoverageHalfWidth(pGeometryMatrix);
	for ( i = 0; i < iPointCount - 1; i++ ) {
		float fDX = pPoints[i + 1].fX - pPoints[i].fX;
		float fDY = pPoints[i + 1].fY - pPoints[i].fY;
		float fLength = hypotf(fDX, fDY);
		float fMidX;
		float fMidY;

		if ( fLength <= XGE_SHAPE_EX_EPSILON ) return XGE_OK;
		tDirections[i].fX = fDX / fLength;
		tDirections[i].fY = fDY / fLength;
		tNormals[i].fX = -tDirections[i].fY;
		tNormals[i].fY = tDirections[i].fX;
		fMidX = (pPoints[i].fX + pPoints[i + 1].fX) * 0.5f;
		fMidY = (pPoints[i].fY + pPoints[i + 1].fY) * 0.5f;
		if ( ((tInterior.fX - fMidX) * tNormals[i].fX +
		      (tInterior.fY - fMidY) * tNormals[i].fY) > 0.0f ) {
			tNormals[i].fX = -tNormals[i].fX;
			tNormals[i].fY = -tNormals[i].fY;
		}
	}
	tOuter[0].fX = pPoints[0].fX + tNormals[0].fX * fAAHalfWidth;
	tOuter[0].fY = pPoints[0].fY + tNormals[0].fY * fAAHalfWidth;
	tOuter[iPointCount - 1].fX = pPoints[iPointCount - 1].fX + tNormals[iPointCount - 2].fX * fAAHalfWidth;
	tOuter[iPointCount - 1].fY = pPoints[iPointCount - 1].fY + tNormals[iPointCount - 2].fY * fAAHalfWidth;
	if ( iPointCount == 3 ) {
		xge_vec2_t tOffset0 = {
			pPoints[1].fX + tNormals[0].fX * fAAHalfWidth,
			pPoints[1].fY + tNormals[0].fY * fAAHalfWidth
		};
		xge_vec2_t tOffset1 = {
			pPoints[1].fX + tNormals[1].fX * fAAHalfWidth,
			pPoints[1].fY + tNormals[1].fY * fAAHalfWidth
		};

		if ( !__xgeShapeExLineIntersection(tOffset0, tDirections[0], tOffset1, tDirections[1], &tOuter[1]) ) {
			float fNX = tNormals[0].fX + tNormals[1].fX;
			float fNY = tNormals[0].fY + tNormals[1].fY;
			float fLength = hypotf(fNX, fNY);

			if ( fLength <= XGE_SHAPE_EX_EPSILON ) {
				tOuter[1] = tOffset1;
			} else {
				tOuter[1].fX = pPoints[1].fX + (fNX / fLength) * fAAHalfWidth;
				tOuter[1].fY = pPoints[1].fY + (fNY / fLength) * fAAHalfWidth;
			}
		}
	}
	for ( i = 0; i < iPointCount; i++ ) {
		tVertices[i * 2 + 0] = (xge_shape_vertex_t){pPoints[i].fX, pPoints[i].fY, iColor};
		tVertices[i * 2 + 1] = (xge_shape_vertex_t){tOuter[i].fX, tOuter[i].fY, iColor & 0xFFFFFF00u};
	}
	for ( i = 0; i < iPointCount - 1; i++ ) {
		int iBase = i * 6;
		uint32_t iInner = (uint32_t)(i * 2);
		uint32_t iNextInner = iInner + 2u;

		iIndices[iBase + 0] = iInner;
		iIndices[iBase + 1] = iNextInner;
		iIndices[iBase + 2] = iNextInner + 1u;
		iIndices[iBase + 3] = iInner;
		iIndices[iBase + 4] = iNextInner + 1u;
		iIndices[iBase + 5] = iInner + 1u;
	}
	return __xgeShapeExDrawStrokePaintMeshEx(
		pShape, tVertices, iPointCount * 2, iIndices, (iPointCount - 1) * 6, iColor, tBounds, tMatrix,
		pGeometryMatrix, fOpacity, bScreenSpace, 1
	);
}

static int __xgeShapeExDrawStrokeJoin(xge_shape_ex pShape, xge_vec2_t tPrev, xge_vec2_t tCurr, xge_vec2_t tNext, float fWidth, uint32_t iColor, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, const xge_shape_ex_matrix_t* pGeometryMatrix, float fOpacity, int bScreenSpace)
{
	xge_shape_vertex_t tVertices[67];
	uint32_t iIndices[192];
	xge_vec2_t arrContour[67];
	xge_vec2_t tD1;
	xge_vec2_t tD2;
	xge_vec2_t tN1;
	xge_vec2_t tN2;
	xge_vec2_t tOuter1;
	xge_vec2_t tOuter2;
	float fL1;
	float fL2;
	float fHalfWidth;
	float fCross;
	int iVertexCount;

	if ( (pShape == NULL) || (fWidth <= XGE_SHAPE_EX_EPSILON) ) {
		return XGE_OK;
	}
	tD1.fX = tCurr.fX - tPrev.fX;
	tD1.fY = tCurr.fY - tPrev.fY;
	tD2.fX = tNext.fX - tCurr.fX;
	tD2.fY = tNext.fY - tCurr.fY;
	fL1 = hypotf(tD1.fX, tD1.fY);
	fL2 = hypotf(tD2.fX, tD2.fY);
	if ( (fL1 <= XGE_SHAPE_EX_EPSILON) || (fL2 <= XGE_SHAPE_EX_EPSILON) ) {
		return XGE_OK;
	}
	tD1.fX /= fL1; tD1.fY /= fL1;
	tD2.fX /= fL2; tD2.fY /= fL2;
	fCross = tD1.fX * tD2.fY - tD1.fY * tD2.fX;
	if ( fabsf(fCross) <= XGE_SHAPE_EX_EPSILON ) {
		float fDot = tD1.fX * tD2.fX + tD1.fY * tD2.fY;

		if ( (fDot < 0.0f) && (pShape->iLineJoin == XGE_SHAPE_EX_JOIN_ROUND) ) {
			return __xgeShapeExDrawStrokeRoundCap(
				pShape, tCurr, tD1.fX, tD1.fY, fWidth * 0.5f, 1, iColor, tBounds, tMatrix,
				pGeometryMatrix, fOpacity, bScreenSpace
			);
		}
		return XGE_OK;
	}
	fHalfWidth = fWidth * 0.5f;
	tN1.fX = -tD1.fY * fHalfWidth;
	tN1.fY = tD1.fX * fHalfWidth;
	tN2.fX = -tD2.fY * fHalfWidth;
	tN2.fY = tD2.fX * fHalfWidth;
	if ( fCross > 0.0f ) {
		tOuter1.fX = -tN1.fX;
		tOuter1.fY = -tN1.fY;
		tOuter2.fX = -tN2.fX;
		tOuter2.fY = -tN2.fY;
	} else {
		tOuter1 = tN1;
		tOuter2 = tN2;
	}
	if ( pShape->iLineJoin == XGE_SHAPE_EX_JOIN_ROUND ) {
		float fA0 = atan2f(tOuter1.fY, tOuter1.fX);
		float fA1 = atan2f(tOuter2.fY, tOuter2.fX);
		float fDelta = fA1 - fA0;
		int iCircleSegments;
		int iSegments;
		int iRet;
		int i;

		while ( fDelta <= -XGE_SHAPE_EX_PI ) {
			fDelta += XGE_SHAPE_EX_TAU;
		}
		while ( fDelta > XGE_SHAPE_EX_PI ) {
			fDelta -= XGE_SHAPE_EX_TAU;
		}
		iCircleSegments = __xgeShapeExStrokeCircleSegments(fHalfWidth, pGeometryMatrix);
		iSegments = (int)ceilf((float)iCircleSegments * fabsf(fDelta) / XGE_SHAPE_EX_TAU);
		if ( iSegments < 1 ) {
			iSegments = 1;
		}
		if ( iSegments > 64 ) {
			iSegments = 64;
		}
		tVertices[0].fX = tCurr.fX;
		tVertices[0].fY = tCurr.fY;
		tVertices[0].iColor = iColor;
		for ( i = 0; i <= iSegments; i++ ) {
			float fA = fA0 + fDelta * ((float)i / (float)iSegments);

			tVertices[i + 1].fX = tCurr.fX + cosf(fA) * fHalfWidth;
			tVertices[i + 1].fY = tCurr.fY + sinf(fA) * fHalfWidth;
			tVertices[i + 1].iColor = iColor;
		}
		for ( i = 0; i < iSegments; i++ ) {
			iIndices[i * 3 + 0] = 0;
			iIndices[i * 3 + 1] = (uint32_t)(i + 1);
			iIndices[i * 3 + 2] = (uint32_t)(i + 2);
		}
		iRet = __xgeShapeExDrawStrokePaintMesh(pShape, tVertices, iSegments + 2, iIndices, iSegments * 3, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
		if ( (iRet != XGE_OK) || pShape->bCoverageAntialiasDisabled ) return iRet;
		return __xgeShapeExDrawStrokeArcFeather(
			pShape, tCurr, fA0, fDelta, fHalfWidth, iSegments, iColor, tBounds, tMatrix,
			pGeometryMatrix, fOpacity, bScreenSpace
		);
	}
	iVertexCount = 0;
	if ( pShape->iLineJoin == XGE_SHAPE_EX_JOIN_MITER ) {
		xge_vec2_t tMiter;
		xge_vec2_t tP1;
		xge_vec2_t tP2;

		tP1.fX = tCurr.fX + tOuter1.fX; tP1.fY = tCurr.fY + tOuter1.fY;
		tP2.fX = tCurr.fX + tOuter2.fX; tP2.fY = tCurr.fY + tOuter2.fY;
		if ( __xgeShapeExLineIntersection(tP1, tD1, tP2, tD2, &tMiter) &&
		     (hypotf(tMiter.fX - tCurr.fX, tMiter.fY - tCurr.fY) <= fHalfWidth * pShape->fMiterLimit) ) {
			tVertices[iVertexCount].fX = tCurr.fX; tVertices[iVertexCount].fY = tCurr.fY; tVertices[iVertexCount++].iColor = iColor;
			tVertices[iVertexCount].fX = tCurr.fX + tOuter1.fX; tVertices[iVertexCount].fY = tCurr.fY + tOuter1.fY; tVertices[iVertexCount++].iColor = iColor;
			tVertices[iVertexCount].fX = tMiter.fX; tVertices[iVertexCount].fY = tMiter.fY; tVertices[iVertexCount++].iColor = iColor;
			tVertices[iVertexCount].fX = tCurr.fX + tOuter2.fX; tVertices[iVertexCount].fY = tCurr.fY + tOuter2.fY; tVertices[iVertexCount++].iColor = iColor;
			iIndices[0] = 0; iIndices[1] = 1; iIndices[2] = 2;
			iIndices[3] = 0; iIndices[4] = 2; iIndices[5] = 3;
			{
				int iRet = __xgeShapeExDrawStrokePaintMesh(pShape, tVertices, iVertexCount, iIndices, 6, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);

				if ( (iRet != XGE_OK) || pShape->bCoverageAntialiasDisabled ) return iRet;
				arrContour[0].fX = tCurr.fX + tOuter1.fX;
				arrContour[0].fY = tCurr.fY + tOuter1.fY;
				arrContour[1] = tMiter;
				arrContour[2].fX = tCurr.fX + tOuter2.fX;
				arrContour[2].fY = tCurr.fY + tOuter2.fY;
				return __xgeShapeExDrawStrokePolylineFeather(
					pShape, arrContour, 3, tCurr, iColor, tBounds, tMatrix, pGeometryMatrix,
					fOpacity, bScreenSpace
				);
			}
		}
	}
	tVertices[iVertexCount].fX = tCurr.fX; tVertices[iVertexCount].fY = tCurr.fY; tVertices[iVertexCount++].iColor = iColor;
	tVertices[iVertexCount].fX = tCurr.fX + tOuter1.fX; tVertices[iVertexCount].fY = tCurr.fY + tOuter1.fY; tVertices[iVertexCount++].iColor = iColor;
	tVertices[iVertexCount].fX = tCurr.fX + tOuter2.fX; tVertices[iVertexCount].fY = tCurr.fY + tOuter2.fY; tVertices[iVertexCount++].iColor = iColor;
	iIndices[0] = 0; iIndices[1] = 1; iIndices[2] = 2;
	{
		int iRet = __xgeShapeExDrawStrokePaintMesh(pShape, tVertices, iVertexCount, iIndices, 3, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);

		if ( (iRet != XGE_OK) || pShape->bCoverageAntialiasDisabled ) return iRet;
		arrContour[0].fX = tCurr.fX + tOuter1.fX;
		arrContour[0].fY = tCurr.fY + tOuter1.fY;
		arrContour[1].fX = tCurr.fX + tOuter2.fX;
		arrContour[1].fY = tCurr.fY + tOuter2.fY;
		return __xgeShapeExDrawStrokePolylineFeather(
			pShape, arrContour, 2, tCurr, iColor, tBounds, tMatrix, pGeometryMatrix,
			fOpacity, bScreenSpace
		);
	}
}

static int __xgeShapeExDrawStrokeSegment(xge_shape_ex pShape, xge_vec2_t tA, xge_vec2_t tB, float fWidth, uint32_t iColor, int bCapStart, int bCapEnd, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, const xge_shape_ex_matrix_t* pGeometryMatrix, float fOpacity, int bScreenSpace)
{
	if ( pShape != NULL ) {
		xge_shape_vertex_t tVertices[4];
		uint32_t iIndices[6];
		float fDX = tB.fX - tA.fX;
		float fDY = tB.fY - tA.fY;
		float fLen = hypotf(fDX, fDY);
		float fNX;
		float fNY;
		float fDirX;
		float fDirY;
		float fHalfWidth;
		int iRet;

		if ( fLen <= XGE_SHAPE_EX_EPSILON ) {
			if ( !bCapStart && !bCapEnd ) {
				return XGE_OK;
			}
			return __xgeShapeExDrawStrokeZeroLengthCap(pShape, tA, fWidth, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
		}
		fDirX = fDX / fLen;
		fDirY = fDY / fLen;
		fHalfWidth = fWidth * 0.5f;
		if ( pShape->iLineCap == XGE_SHAPE_EX_CAP_SQUARE ) {
			if ( bCapStart ) {
				tA.fX -= fDirX * fHalfWidth;
				tA.fY -= fDirY * fHalfWidth;
			}
			if ( bCapEnd ) {
				tB.fX += fDirX * fHalfWidth;
				tB.fY += fDirY * fHalfWidth;
			}
		}
		fNX = -fDirY * fHalfWidth;
		fNY = fDirX * fHalfWidth;
		tVertices[0].fX = tA.fX + fNX; tVertices[0].fY = tA.fY + fNY; tVertices[0].iColor = iColor;
		tVertices[1].fX = tB.fX + fNX; tVertices[1].fY = tB.fY + fNY; tVertices[1].iColor = iColor;
		tVertices[2].fX = tB.fX - fNX; tVertices[2].fY = tB.fY - fNY; tVertices[2].iColor = iColor;
		tVertices[3].fX = tA.fX - fNX; tVertices[3].fY = tA.fY - fNY; tVertices[3].iColor = iColor;
		iIndices[0] = 0; iIndices[1] = 1; iIndices[2] = 2;
		iIndices[3] = 0; iIndices[4] = 2; iIndices[5] = 3;
		iRet = __xgeShapeExDrawStrokePaintMesh(pShape, tVertices, 4, iIndices, 6, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		if ( pShape->iLineCap == XGE_SHAPE_EX_CAP_ROUND ) {
			if ( bCapStart ) {
				iRet = __xgeShapeExDrawStrokeRoundCap(pShape, tA, fDirX, fDirY, fHalfWidth, 0, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
				if ( iRet != XGE_OK ) {
					return iRet;
				}
			}
			if ( bCapEnd ) {
				iRet = __xgeShapeExDrawStrokeRoundCap(pShape, tB, fDirX, fDirY, fHalfWidth, 1, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
				if ( iRet != XGE_OK ) {
					return iRet;
				}
			}
		}
		return XGE_OK;
	}
	if ( bScreenSpace ) {
		xgeShapeLinePx(tA.fX, tA.fY, tB.fX, tB.fY, fWidth, iColor);
	} else {
		xgeShapeLine(tA.fX, tA.fY, tB.fX, tB.fY, fWidth, iColor);
	}
	return XGE_OK;
}

static int __xgeShapeExCompactStrokePoints(const xge_vec2_t* pSrc, int iCount, int bClosed, xge_vec2_t** ppPoints, int* pCount)
{
	xge_vec2_t* pPoints;
	int iWrite;
	int i;

	if ( (pSrc == NULL) || (ppPoints == NULL) || (pCount == NULL) || (iCount <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppPoints = NULL;
	*pCount = 0;
	pPoints = (xge_vec2_t*)xrtMalloc((size_t)iCount * sizeof(*pPoints));
	if ( pPoints == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iWrite = 0;
	for ( i = 0; i < iCount; i++ ) {
		if ( (iWrite > 0) && __xgeShapeExSamePoint(pPoints[iWrite - 1], pSrc[i]) ) {
			continue;
		}
		pPoints[iWrite++] = pSrc[i];
	}
	if ( bClosed ) {
		while ( (iWrite > 1) && __xgeShapeExSamePoint(pPoints[iWrite - 1], pPoints[0]) ) {
			iWrite--;
		}
	}
	if ( iWrite <= 0 ) {
		xrtFree(pPoints);
		return XGE_OK;
	}
	*ppPoints = pPoints;
	*pCount = iWrite;
	return XGE_OK;
}

static int __xgeShapeExNormalizeSegment(xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t* pDir)
{
	float fDX;
	float fDY;
	float fLen;

	if ( pDir == NULL ) {
		return 0;
	}
	fDX = tB.fX - tA.fX;
	fDY = tB.fY - tA.fY;
	fLen = hypotf(fDX, fDY);
	if ( fLen <= XGE_SHAPE_EX_EPSILON ) {
		pDir->fX = 0.0f;
		pDir->fY = 0.0f;
		return 0;
	}
	pDir->fX = fDX / fLen;
	pDir->fY = fDY / fLen;
	return 1;
}

static xge_vec2_t __xgeShapeExStrokeSidePoint(xge_vec2_t tPrev, xge_vec2_t tCurr, xge_vec2_t tNext, float fHalfWidth, float fMiterLimit, int iSide)
{
	xge_vec2_t tD1;
	xge_vec2_t tD2;
	xge_vec2_t tN1;
	xge_vec2_t tN2;
	xge_vec2_t tP1;
	xge_vec2_t tP2;
	xge_vec2_t tOut;
	float fMaxMiter;

	if ( !__xgeShapeExNormalizeSegment(tPrev, tCurr, &tD1) ||
	     !__xgeShapeExNormalizeSegment(tCurr, tNext, &tD2) ) {
		tOut = tCurr;
		return tOut;
	}
	tN1.fX = -tD1.fY * fHalfWidth * (float)iSide;
	tN1.fY =  tD1.fX * fHalfWidth * (float)iSide;
	tN2.fX = -tD2.fY * fHalfWidth * (float)iSide;
	tN2.fY =  tD2.fX * fHalfWidth * (float)iSide;
	tP1.fX = tCurr.fX + tN1.fX;
	tP1.fY = tCurr.fY + tN1.fY;
	tP2.fX = tCurr.fX + tN2.fX;
	tP2.fY = tCurr.fY + tN2.fY;
	fMaxMiter = fHalfWidth * fMiterLimit;
	if ( fMaxMiter < fHalfWidth ) {
		fMaxMiter = fHalfWidth;
	}
	if ( __xgeShapeExLineIntersection(tP1, tD1, tP2, tD2, &tOut) ) {
		float fDX = tOut.fX - tCurr.fX;
		float fDY = tOut.fY - tCurr.fY;
		float fLen = hypotf(fDX, fDY);

		if ( (fLen > XGE_SHAPE_EX_EPSILON) && (fLen > fMaxMiter) ) {
			tOut.fX = tCurr.fX + (fDX / fLen) * fMaxMiter;
			tOut.fY = tCurr.fY + (fDY / fLen) * fMaxMiter;
		}
		return tOut;
	}
	tOut.fX = tN1.fX + tN2.fX;
	tOut.fY = tN1.fY + tN2.fY;
	if ( hypotf(tOut.fX, tOut.fY) <= XGE_SHAPE_EX_EPSILON ) {
		tOut = tN2;
	} else {
		float fLen = hypotf(tOut.fX, tOut.fY);

		tOut.fX = (tOut.fX / fLen) * fHalfWidth;
		tOut.fY = (tOut.fY / fLen) * fHalfWidth;
	}
	tOut.fX += tCurr.fX;
	tOut.fY += tCurr.fY;
	return tOut;
}

static void __xgeShapeExStrokeEndpointPair(xge_vec2_t tPoint, xge_vec2_t tOther, float fHalfWidth, int iCap, int bEnd, xge_vec2_t* pLeft, xge_vec2_t* pRight)
{
	xge_vec2_t tDir;
	xge_vec2_t tBase;
	float fNX;
	float fNY;

	tBase = tPoint;
	if ( !__xgeShapeExNormalizeSegment(bEnd ? tOther : tPoint, bEnd ? tPoint : tOther, &tDir) ) {
		tDir.fX = 1.0f;
		tDir.fY = 0.0f;
	}
	if ( iCap == XGE_SHAPE_EX_CAP_SQUARE ) {
		if ( bEnd ) {
			tBase.fX += tDir.fX * fHalfWidth;
			tBase.fY += tDir.fY * fHalfWidth;
		} else {
			tBase.fX -= tDir.fX * fHalfWidth;
			tBase.fY -= tDir.fY * fHalfWidth;
		}
	}
	fNX = -tDir.fY * fHalfWidth;
	fNY =  tDir.fX * fHalfWidth;
	pLeft->fX = tBase.fX + fNX;
	pLeft->fY = tBase.fY + fNY;
	pRight->fX = tBase.fX - fNX;
	pRight->fY = tBase.fY - fNY;
}

static int __xgeShapeExStrokeOutlineAppend(xge_vec2_t** ppPoints, int* pPointCount, int* pPointCapacity, xge_vec2_t tPoint)
{
	xge_vec2_t* pPoints;
	int iCapacity;

	if ( (ppPoints == NULL) || (pPointCount == NULL) || (pPointCapacity == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (*pPointCount > 0) && __xgeShapeExSamePoint((*ppPoints)[*pPointCount - 1], tPoint) ) {
		return XGE_OK;
	}
	if ( *pPointCount >= *pPointCapacity ) {
		iCapacity = *pPointCapacity > 0 ? *pPointCapacity : 32;
		while ( iCapacity <= *pPointCount ) {
			if ( iCapacity > (INT_MAX / 2) ) return XGE_ERROR_OUT_OF_MEMORY;
			iCapacity *= 2;
		}
		pPoints = (xge_vec2_t*)xrtRealloc(*ppPoints, (size_t)iCapacity * sizeof(*pPoints));
		if ( pPoints == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
		*ppPoints = pPoints;
		*pPointCapacity = iCapacity;
	}
	(*ppPoints)[(*pPointCount)++] = tPoint;
	return XGE_OK;
}

static int __xgeShapeExStrokeOutlineAppendArc(xge_vec2_t** ppPoints, int* pPointCount, int* pPointCapacity, xge_vec2_t tCenter, float fRadius, float fStart, float fDelta, int iSegments)
{
	int i;

	if ( iSegments < 1 ) iSegments = 1;
	for ( i = 0; i <= iSegments; i++ ) {
		float fAngle = fStart + fDelta * ((float)i / (float)iSegments);
		xge_vec2_t tPoint = {
			tCenter.fX + cosf(fAngle) * fRadius,
			tCenter.fY + sinf(fAngle) * fRadius
		};
		int iRet = __xgeShapeExStrokeOutlineAppend(ppPoints, pPointCount, pPointCapacity, tPoint);

		if ( iRet != XGE_OK ) return iRet;
	}
	return XGE_OK;
}

static int __xgeShapeExStrokeOutlineAppendJoin(xge_shape_ex pShape, xge_vec2_t** ppOutline, int* pOutlineCount, int* pOutlineCapacity, xge_vec2_t tPrev, xge_vec2_t tCurr, xge_vec2_t tNext, float fHalfWidth, int iSide, int bReverse, const xge_shape_ex_matrix_t* pGeometryMatrix)
{
	xge_vec2_t tD1;
	xge_vec2_t tD2;
	xge_vec2_t tP1;
	xge_vec2_t tP2;
	xge_vec2_t tStart;
	xge_vec2_t tEnd;
	xge_vec2_t tMiter;
	float fCross;
	float fDot;
	int bOuter;
	int iRet;

	if ( !__xgeShapeExNormalizeSegment(tPrev, tCurr, &tD1) ||
	     !__xgeShapeExNormalizeSegment(tCurr, tNext, &tD2) ) return XGE_OK;
	tP1.fX = tCurr.fX - tD1.fY * fHalfWidth * (float)iSide;
	tP1.fY = tCurr.fY + tD1.fX * fHalfWidth * (float)iSide;
	tP2.fX = tCurr.fX - tD2.fY * fHalfWidth * (float)iSide;
	tP2.fY = tCurr.fY + tD2.fX * fHalfWidth * (float)iSide;
	tStart = bReverse ? tP2 : tP1;
	tEnd = bReverse ? tP1 : tP2;
	fCross = tD1.fX * tD2.fY - tD1.fY * tD2.fX;
	fDot = tD1.fX * tD2.fX + tD1.fY * tD2.fY;
	if ( fabsf(fCross) <= XGE_SHAPE_EX_EPSILON ) {
		if ( fDot >= 0.0f ) return __xgeShapeExStrokeOutlineAppend(ppOutline, pOutlineCount, pOutlineCapacity, tEnd);
		if ( pShape->iLineJoin != XGE_SHAPE_EX_JOIN_ROUND ) {
			iRet = __xgeShapeExStrokeOutlineAppend(ppOutline, pOutlineCount, pOutlineCapacity, tStart);
			if ( iRet != XGE_OK ) return iRet;
			return __xgeShapeExStrokeOutlineAppend(ppOutline, pOutlineCount, pOutlineCapacity, tEnd);
		}
	}
	bOuter = (fCross * (float)iSide) < 0.0f;
	if ( !bOuter ) {
		if ( __xgeShapeExLineIntersection(tP1, tD1, tP2, tD2, &tMiter) ) {
			return __xgeShapeExStrokeOutlineAppend(ppOutline, pOutlineCount, pOutlineCapacity, tMiter);
		}
		return __xgeShapeExStrokeOutlineAppend(ppOutline, pOutlineCount, pOutlineCapacity, tEnd);
	}
	if ( pShape->iLineJoin == XGE_SHAPE_EX_JOIN_ROUND ) {
		float fStart = atan2f(tStart.fY - tCurr.fY, tStart.fX - tCurr.fX);
		float fEnd = atan2f(tEnd.fY - tCurr.fY, tEnd.fX - tCurr.fX);
		float fDelta = fEnd - fStart;
		int iCircleSegments = __xgeShapeExStrokeCircleSegments(fHalfWidth, pGeometryMatrix);
		int iSegments;

		while ( fDelta <= -XGE_SHAPE_EX_PI ) fDelta += XGE_SHAPE_EX_TAU;
		while ( fDelta > XGE_SHAPE_EX_PI ) fDelta -= XGE_SHAPE_EX_TAU;
		iSegments = (int)ceilf((float)iCircleSegments * fabsf(fDelta) / XGE_SHAPE_EX_TAU);
		return __xgeShapeExStrokeOutlineAppendArc(
			ppOutline, pOutlineCount, pOutlineCapacity, tCurr, fHalfWidth, fStart, fDelta, iSegments
		);
	}
	if ( pShape->iLineJoin == XGE_SHAPE_EX_JOIN_MITER ) {
		float fLimit = fHalfWidth * fmaxf(1.0f, pShape->fMiterLimit);

		if ( __xgeShapeExLineIntersection(tP1, tD1, tP2, tD2, &tMiter) &&
		     (hypotf(tMiter.fX - tCurr.fX, tMiter.fY - tCurr.fY) <= fLimit) ) {
			return __xgeShapeExStrokeOutlineAppend(ppOutline, pOutlineCount, pOutlineCapacity, tMiter);
		}
	}
	iRet = __xgeShapeExStrokeOutlineAppend(ppOutline, pOutlineCount, pOutlineCapacity, tStart);
	if ( iRet != XGE_OK ) return iRet;
	return __xgeShapeExStrokeOutlineAppend(ppOutline, pOutlineCount, pOutlineCapacity, tEnd);
}

static int __xgeShapeExBuildOpenStrokeOutline(xge_shape_ex pShape, const xge_vec2_t* pPoints, int iPointCount, float fHalfWidth, const xge_shape_ex_matrix_t* pGeometryMatrix, xge_vec2_t** ppOutline, int* pOutlineCount)
{
	xge_vec2_t* pOutline;
	xge_vec2_t tLeftStart;
	xge_vec2_t tRightStart;
	xge_vec2_t tLeftEnd;
	xge_vec2_t tRightEnd;
	int iOutlineCount;
	int iOutlineCapacity;
	int iCapSegments;
	int iRet;
	int i;

	if ( (pShape == NULL) || (pPoints == NULL) || (iPointCount < 2) ||
	     (ppOutline == NULL) || (pOutlineCount == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	*ppOutline = NULL;
	*pOutlineCount = 0;
	pOutline = NULL;
	iOutlineCount = 0;
	iOutlineCapacity = 0;
	__xgeShapeExStrokeEndpointPair(pPoints[0], pPoints[1], fHalfWidth, pShape->iLineCap, 0, &tLeftStart, &tRightStart);
	__xgeShapeExStrokeEndpointPair(pPoints[iPointCount - 1], pPoints[iPointCount - 2], fHalfWidth, pShape->iLineCap, 1, &tLeftEnd, &tRightEnd);
	iRet = __xgeShapeExStrokeOutlineAppend(&pOutline, &iOutlineCount, &iOutlineCapacity, tLeftStart);
	for ( i = 1; (iRet == XGE_OK) && (i < iPointCount - 1); i++ ) {
		iRet = __xgeShapeExStrokeOutlineAppendJoin(
			pShape, &pOutline, &iOutlineCount, &iOutlineCapacity, pPoints[i - 1], pPoints[i], pPoints[i + 1],
			fHalfWidth, 1, 0, pGeometryMatrix
		);
	}
	if ( iRet == XGE_OK ) iRet = __xgeShapeExStrokeOutlineAppend(&pOutline, &iOutlineCount, &iOutlineCapacity, tLeftEnd);
	iCapSegments = (__xgeShapeExStrokeCircleSegments(fHalfWidth, pGeometryMatrix) + 1) / 2;
	if ( (iRet == XGE_OK) && (pShape->iLineCap == XGE_SHAPE_EX_CAP_ROUND) ) {
		xge_vec2_t tDir;

		if ( __xgeShapeExNormalizeSegment(pPoints[iPointCount - 2], pPoints[iPointCount - 1], &tDir) ) {
			iRet = __xgeShapeExStrokeOutlineAppendArc(
				&pOutline, &iOutlineCount, &iOutlineCapacity, pPoints[iPointCount - 1], fHalfWidth,
				atan2f(tDir.fY, tDir.fX) + XGE_SHAPE_EX_PI * 0.5f, -XGE_SHAPE_EX_PI, iCapSegments
			);
		}
	} else if ( iRet == XGE_OK ) {
		iRet = __xgeShapeExStrokeOutlineAppend(&pOutline, &iOutlineCount, &iOutlineCapacity, tRightEnd);
	}
	for ( i = iPointCount - 2; (iRet == XGE_OK) && (i > 0); i-- ) {
		iRet = __xgeShapeExStrokeOutlineAppendJoin(
			pShape, &pOutline, &iOutlineCount, &iOutlineCapacity, pPoints[i - 1], pPoints[i], pPoints[i + 1],
			fHalfWidth, -1, 1, pGeometryMatrix
		);
	}
	if ( iRet == XGE_OK ) iRet = __xgeShapeExStrokeOutlineAppend(&pOutline, &iOutlineCount, &iOutlineCapacity, tRightStart);
	if ( (iRet == XGE_OK) && (pShape->iLineCap == XGE_SHAPE_EX_CAP_ROUND) ) {
		xge_vec2_t tDir;

		if ( __xgeShapeExNormalizeSegment(pPoints[0], pPoints[1], &tDir) ) {
			iRet = __xgeShapeExStrokeOutlineAppendArc(
				&pOutline, &iOutlineCount, &iOutlineCapacity, pPoints[0], fHalfWidth,
				atan2f(tDir.fY, tDir.fX) - XGE_SHAPE_EX_PI * 0.5f, -XGE_SHAPE_EX_PI, iCapSegments
			);
		}
	}
	if ( iRet != XGE_OK ) {
		xrtFree(pOutline);
		return iRet;
	}
	if ( (iOutlineCount > 1) && __xgeShapeExSamePoint(pOutline[0], pOutline[iOutlineCount - 1]) ) iOutlineCount--;
	*ppOutline = pOutline;
	*pOutlineCount = iOutlineCount;
	return XGE_OK;
}

static int __xgeShapeExBuildClosedStrokeOutline(xge_shape_ex pShape, const xge_vec2_t* pPoints, int iPointCount, float fHalfWidth, const xge_shape_ex_matrix_t* pGeometryMatrix, xge_vec2_t** ppOutline, int* pOutlineCount)
{
	xge_vec2_t* pOutline;
	int iOutlineCount;
	int iOutlineCapacity;
	int iRightStart;
	int iRet;
	int i;

	if ( (pShape == NULL) || (pPoints == NULL) || (iPointCount < 2) ||
	     (ppOutline == NULL) || (pOutlineCount == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	*ppOutline = NULL;
	*pOutlineCount = 0;
	pOutline = NULL;
	iOutlineCount = 0;
	iOutlineCapacity = 0;
	iRet = XGE_OK;
	for ( i = 0; (iRet == XGE_OK) && (i < iPointCount); i++ ) {
		int iPrev = (i + iPointCount - 1) % iPointCount;
		int iNext = (i + 1) % iPointCount;

		iRet = __xgeShapeExStrokeOutlineAppendJoin(
			pShape, &pOutline, &iOutlineCount, &iOutlineCapacity,
			pPoints[iPrev], pPoints[i], pPoints[iNext], fHalfWidth, 1, 0, pGeometryMatrix
		);
	}
	if ( (iRet == XGE_OK) && (iOutlineCount > 0) ) {
		iRet = __xgeShapeExStrokeOutlineAppend(
			&pOutline, &iOutlineCount, &iOutlineCapacity, pOutline[0]
		);
	}
	iRightStart = iOutlineCount;
	for ( i = iPointCount - 1; (iRet == XGE_OK) && (i >= 0); i-- ) {
		int iPrev = (i + iPointCount - 1) % iPointCount;
		int iNext = (i + 1) % iPointCount;

		iRet = __xgeShapeExStrokeOutlineAppendJoin(
			pShape, &pOutline, &iOutlineCount, &iOutlineCapacity,
			pPoints[iPrev], pPoints[i], pPoints[iNext], fHalfWidth, -1, 1, pGeometryMatrix
		);
	}
	if ( (iRet == XGE_OK) && (iOutlineCount > iRightStart) ) {
		iRet = __xgeShapeExStrokeOutlineAppend(
			&pOutline, &iOutlineCount, &iOutlineCapacity, pOutline[iRightStart]
		);
	}
	if ( iRet != XGE_OK ) {
		xrtFree(pOutline);
		return iRet;
	}
	if ( (iRightStart <= 1) || (iOutlineCount <= (iRightStart + 1)) ) {
		xrtFree(pOutline);
		return XGE_OK;
	}
	*ppOutline = pOutline;
	*pOutlineCount = iOutlineCount;
	return XGE_OK;
}

static int __xgeShapeExDrawStrokeCoverageSpans(
	xge_shape_ex pShape,
	const xge_shape_ex_coverage_raster_t* pRaster,
	uint32_t iColor,
	xge_rect_t tBounds,
	xge_shape_ex_matrix_t tMatrix,
	float fOpacity
)
{
	xge_shape_vertex_t* pVertices;
	uint32_t* pIndices;
	int iVertexCount;
	int iIndexCount;
	int iRet;
	int i;

	if ( (pRaster == NULL) || (pRaster->iSpanCount <= 0) ) return XGE_OK;
	if ( (pRaster->iSpanCount > (INT_MAX / 6)) ||
	     ((uint64_t)pRaster->iSpanCount * 4u > UINT32_MAX) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iVertexCount = pRaster->iSpanCount * 4;
	iIndexCount = pRaster->iSpanCount * 6;
	pVertices = (xge_shape_vertex_t*)xrtMalloc((size_t)iVertexCount * sizeof(*pVertices));
	pIndices = (uint32_t*)xrtMalloc((size_t)iIndexCount * sizeof(*pIndices));
	if ( (pVertices == NULL) || (pIndices == NULL) ) {
		xrtFree(pVertices);
		xrtFree(pIndices);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	for ( i = 0; i < pRaster->iSpanCount; i++ ) {
		const xge_shape_ex_coverage_span_t* pSpan = &pRaster->pSpans[i];
		uint32_t iBase = (uint32_t)i * 4u;
		int iAlpha = ((int)(iColor & 0xFFu) * (int)pSpan->iCoverage + 127) / 255;
		uint32_t iSpanColor = (iColor & 0xFFFFFF00u) | (uint32_t)iAlpha;
		float fLeft = (float)pSpan->iX;
		float fTop = (float)pSpan->iY;
		float fRight = (float)(pSpan->iX + pSpan->iLength);
		float fBottom = (float)(pSpan->iY + 1);
		int iIndexBase = i * 6;

		pVertices[iBase + 0] = (xge_shape_vertex_t){fLeft, fTop, iSpanColor};
		pVertices[iBase + 1] = (xge_shape_vertex_t){fRight, fTop, iSpanColor};
		pVertices[iBase + 2] = (xge_shape_vertex_t){fRight, fBottom, iSpanColor};
		pVertices[iBase + 3] = (xge_shape_vertex_t){fLeft, fBottom, iSpanColor};
		pIndices[iIndexBase + 0] = iBase + 0;
		pIndices[iIndexBase + 1] = iBase + 1;
		pIndices[iIndexBase + 2] = iBase + 2;
		pIndices[iIndexBase + 3] = iBase + 0;
		pIndices[iIndexBase + 4] = iBase + 2;
		pIndices[iIndexBase + 5] = iBase + 3;
	}
	iRet = __xgeShapeExDrawStrokePaintMeshEx(
		pShape, pVertices, iVertexCount, pIndices, iIndexCount, iColor, tBounds, tMatrix,
		NULL, fOpacity, 1, 1
	);
	xrtFree(pVertices);
	xrtFree(pIndices);
	return iRet;
}

static int __xgeShapeExDrawStrokeRasterPathCoverage(
	xge_shape_ex pShape,
	const xge_shape_ex_stroke_path_t* pPath,
	uint32_t iColor,
	xge_rect_t tBounds,
	xge_shape_ex_matrix_t tMatrix,
	xge_shape_ex_matrix_t tRasterMatrix,
	float fOpacity
)
{
	xge_shape_ex_coverage_raster_t tRaster;
	xge_rect_t tClip;
	float fMinX = FLT_MAX;
	float fMinY = FLT_MAX;
	float fMaxX = -FLT_MAX;
	float fMaxY = -FLT_MAX;
	int iMinX;
	int iMinY;
	int iMaxX;
	int iMaxY;
	int iRet;
	int i;

	if ( pPath == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( (pPath->iCommandCount <= 0) || (pPath->iPointCount <= 0) ) return XGE_OK;
	memset(&tRaster, 0, sizeof(tRaster));
	for ( i = 0; i < pPath->iPointCount; i++ ) {
		xge_vec2_t tPoint = __xgeShapeExMatrixPoint(tRasterMatrix, pPath->pPoints[i]);

		if ( tPoint.fX < fMinX ) fMinX = tPoint.fX;
		if ( tPoint.fY < fMinY ) fMinY = tPoint.fY;
		if ( tPoint.fX > fMaxX ) fMaxX = tPoint.fX;
		if ( tPoint.fY > fMaxY ) fMaxY = tPoint.fY;
	}
	if ( !__xgeShapeExFloatFinite(fMinX) || !__xgeShapeExFloatFinite(fMinY) ||
	     !__xgeShapeExFloatFinite(fMaxX) || !__xgeShapeExFloatFinite(fMaxY) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fMinX < 0.0f ) fMinX = 0.0f;
	if ( fMinY < 0.0f ) fMinY = 0.0f;
	if ( fMaxX > (float)g_xge.iWidth ) fMaxX = (float)g_xge.iWidth;
	if ( fMaxY > (float)g_xge.iHeight ) fMaxY = (float)g_xge.iHeight;
	if ( g_xge.bClipEnabled ) {
		tClip = xgeClipGet();
		if ( fMinX < tClip.fX ) fMinX = tClip.fX;
		if ( fMinY < tClip.fY ) fMinY = tClip.fY;
		if ( fMaxX > (tClip.fX + tClip.fW) ) fMaxX = tClip.fX + tClip.fW;
		if ( fMaxY > (tClip.fY + tClip.fH) ) fMaxY = tClip.fY + tClip.fH;
	}
	if ( (fMaxX <= fMinX) || (fMaxY <= fMinY) ) {
		return XGE_OK;
	}
	iMinX = (int)floorf(fMinX);
	iMinY = (int)floorf(fMinY);
	iMaxX = (int)ceilf(fMaxX);
	iMaxY = (int)ceilf(fMaxY);
	iRet = __xgeShapeExCoverageRasterizePath(
		&tRaster, pPath->pCommands, pPath->iCommandCount, pPath->pPoints,
		pPath->iPointCount, tRasterMatrix, iMinX, iMinY, iMaxX, iMaxY, 0
	);
	if ( iRet == XGE_OK ) {
		iRet = __xgeShapeExDrawStrokeCoverageSpans(
			pShape, &tRaster, iColor, tBounds, tMatrix, fOpacity
		);
	}
	__xgeShapeExCoverageRasterFree(&tRaster);
	return iRet;
}

static int __xgeShapeExDrawStrokePathCoverage(
	xge_shape_ex pShape,
	xge_shape_ex pPathShape,
	float fWidth,
	uint32_t iColor,
	xge_rect_t tBounds,
	xge_shape_ex_matrix_t tMatrix,
	const xge_shape_ex_matrix_t* pGeometryMatrix,
	float fOpacity
)
{
	xge_shape_ex_stroke_path_t tPath;
	xge_shape_ex_matrix_t tIdentity;
	xge_shape_ex_matrix_t tPathMatrix;
	xge_shape_ex_matrix_t tRasterMatrix;
	int iRet;

	memset(&tPath, 0, sizeof(tPath));
	tIdentity = __xgeShapeExMatrixIdentity();
	tPathMatrix = pGeometryMatrix != NULL ? tIdentity : tMatrix;
	tRasterMatrix = pGeometryMatrix != NULL ? *pGeometryMatrix : tIdentity;
	if ( !__xgeShapeExValid(pPathShape) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( (pPathShape->iCommandCount <= 0) || (pPathShape->iPointCount <= 0) ) return XGE_OK;
	iRet = __xgeShapeExStrokePathBuild(
		&tPath, pPathShape->pCommands, pPathShape->iCommandCount, pPathShape->pPoints,
		pPathShape->iPointCount, tPathMatrix, fWidth * 0.5f, pShape->iLineCap,
		pShape->iLineJoin, pShape->fMiterLimit
	);
	if ( iRet == XGE_OK ) {
		iRet = __xgeShapeExDrawStrokeRasterPathCoverage(
			pShape, &tPath, iColor, tBounds, tMatrix, tRasterMatrix, fOpacity
		);
	}
	__xgeShapeExStrokePathFree(&tPath);
	return iRet;
}

static int __xgeShapeExDrawStrokeDashPathCoverage(
	xge_shape_ex pShape,
	xge_shape_ex pPathShape,
	float fWidth,
	uint32_t iColor,
	const float* pDashPattern,
	int iDashCount,
	float fDashOffset,
	xge_rect_t tBounds,
	xge_shape_ex_matrix_t tMatrix,
	const xge_shape_ex_matrix_t* pGeometryMatrix,
	float fOpacity
)
{
	xge_shape_ex_stroke_path_t tDashPath;
	xge_shape_ex_stroke_path_t tStrokePath;
	xge_shape_ex_matrix_t tIdentity;
	xge_shape_ex_matrix_t tPathMatrix;
	xge_shape_ex_matrix_t tRasterMatrix;
	int iRet;

	memset(&tDashPath, 0, sizeof(tDashPath));
	memset(&tStrokePath, 0, sizeof(tStrokePath));
	tIdentity = __xgeShapeExMatrixIdentity();
	tPathMatrix = pGeometryMatrix != NULL ? tIdentity : tMatrix;
	tRasterMatrix = pGeometryMatrix != NULL ? *pGeometryMatrix : tIdentity;
	if ( !__xgeShapeExValid(pPathShape) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( (pPathShape->iCommandCount <= 0) || (pPathShape->iPointCount <= 0) ) return XGE_OK;
	iRet = __xgeShapeExStrokeDashPathBuild(
		&tDashPath, pPathShape->pCommands, pPathShape->iCommandCount, pPathShape->pPoints,
		pPathShape->iPointCount, tPathMatrix, pDashPattern, iDashCount, fDashOffset,
		pShape->iLineCap
	);
	if ( (iRet == XGE_OK) && (tDashPath.iCommandCount > 0) &&
	     (tDashPath.iPointCount > 0) ) {
		iRet = __xgeShapeExStrokePathBuild(
			&tStrokePath, tDashPath.pCommands, tDashPath.iCommandCount,
			tDashPath.pPoints, tDashPath.iPointCount, tIdentity, fWidth * 0.5f,
			pShape->iLineCap, pShape->iLineJoin, pShape->fMiterLimit
		);
	}
	if ( iRet == XGE_OK ) {
		iRet = __xgeShapeExDrawStrokeRasterPathCoverage(
			pShape, &tStrokePath, iColor, tBounds, tMatrix, tRasterMatrix, fOpacity
		);
	}
	__xgeShapeExStrokePathFree(&tStrokePath);
	__xgeShapeExStrokePathFree(&tDashPath);
	return iRet;
}

static int __xgeShapeExDrawStrokeAreaCoverage(
	xge_shape_ex pShape,
	const xge_vec2_t* pOutline,
	int iOutlineCount,
	uint32_t iColor,
	xge_rect_t tBounds,
	xge_shape_ex_matrix_t tMatrix,
	const xge_shape_ex_matrix_t* pGeometryMatrix,
	float fOpacity
)
{
	xge_shape_ex_coverage_raster_t tRaster;
	xge_vec2_t* pScreenOutline;
	xge_rect_t tClip;
	float fMinX;
	float fMinY;
	float fMaxX;
	float fMaxY;
	int iMinX;
	int iMinY;
	int iMaxX;
	int iMaxY;
	int iRet;
	int i;

	memset(&tRaster, 0, sizeof(tRaster));
	if ( (iOutlineCount <= 0) ||
	     ((size_t)iOutlineCount > (SIZE_MAX / sizeof(*pScreenOutline))) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pScreenOutline = (xge_vec2_t*)xrtMalloc((size_t)iOutlineCount * sizeof(*pScreenOutline));
	if ( pScreenOutline == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	fMinX = FLT_MAX;
	fMinY = FLT_MAX;
	fMaxX = -FLT_MAX;
	fMaxY = -FLT_MAX;
	for ( i = 0; i < iOutlineCount; i++ ) {
		pScreenOutline[i] = pGeometryMatrix != NULL ?
			__xgeShapeExMatrixPoint(*pGeometryMatrix, pOutline[i]) : pOutline[i];
		if ( pScreenOutline[i].fX < fMinX ) fMinX = pScreenOutline[i].fX;
		if ( pScreenOutline[i].fY < fMinY ) fMinY = pScreenOutline[i].fY;
		if ( pScreenOutline[i].fX > fMaxX ) fMaxX = pScreenOutline[i].fX;
		if ( pScreenOutline[i].fY > fMaxY ) fMaxY = pScreenOutline[i].fY;
	}
	if ( !__xgeShapeExFloatFinite(fMinX) || !__xgeShapeExFloatFinite(fMinY) ||
	     !__xgeShapeExFloatFinite(fMaxX) || !__xgeShapeExFloatFinite(fMaxY) ) {
		xrtFree(pScreenOutline);
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fMinX < 0.0f ) fMinX = 0.0f;
	if ( fMinY < 0.0f ) fMinY = 0.0f;
	if ( fMaxX > (float)g_xge.iWidth ) fMaxX = (float)g_xge.iWidth;
	if ( fMaxY > (float)g_xge.iHeight ) fMaxY = (float)g_xge.iHeight;
	if ( g_xge.bClipEnabled ) {
		tClip = xgeClipGet();
		if ( fMinX < tClip.fX ) fMinX = tClip.fX;
		if ( fMinY < tClip.fY ) fMinY = tClip.fY;
		if ( fMaxX > (tClip.fX + tClip.fW) ) fMaxX = tClip.fX + tClip.fW;
		if ( fMaxY > (tClip.fY + tClip.fH) ) fMaxY = tClip.fY + tClip.fH;
	}
	if ( (fMaxX <= fMinX) || (fMaxY <= fMinY) ) {
		xrtFree(pScreenOutline);
		return XGE_OK;
	}
	iMinX = (int)floorf(fMinX);
	iMinY = (int)floorf(fMinY);
	iMaxX = (int)ceilf(fMaxX);
	iMaxY = (int)ceilf(fMaxY);
	iRet = __xgeShapeExCoverageRasterizeContour(
		&tRaster, pScreenOutline, iOutlineCount, iMinX, iMinY, iMaxX, iMaxY, 0
	);
	if ( iRet == XGE_OK ) {
		iRet = __xgeShapeExDrawStrokeCoverageSpans(
			pShape, &tRaster, iColor, tBounds, tMatrix, fOpacity
		);
	}
	__xgeShapeExCoverageRasterFree(&tRaster);
	xrtFree(pScreenOutline);
	return iRet;
}

static int __xgeShapeExStrokeContourWantsExplicitJoins(const xge_vec2_t* pSrc, int iCount, int bClosed);

static int __xgeShapeExDrawClosedStrokeCoverageMesh(xge_shape_ex pShape, const xge_vec2_t* pPoints, int iPointCount, float fWidth, float fMiterLimit, float fAAHalfWidth, uint32_t iColor, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, const xge_shape_ex_matrix_t* pGeometryMatrix, float fOpacity, int bScreenSpace)
{
	xge_shape_vertex_t* pVertices;
	uint32_t* pIndices;
	xge_vec2_t* pScreenCenters;
	xge_vec2_t* pScreenLeft;
	xge_vec2_t* pScreenRight;
	xge_shape_ex_matrix_t tInverse;
	float fHalfWidth;
	float fInnerHalfWidth;
	float fOuterHalfWidth;
	int bScreenCoverage;
	int iVertexCount;
	int iIndexCount;
	int iRet;
	int i;

	if ( (pShape == NULL) || (pPoints == NULL) || (iPointCount < 2) || (fWidth <= XGE_SHAPE_EX_EPSILON) ) {
		return XGE_OK;
	}
	if ( (iPointCount > (INT_MAX / 4)) || (iPointCount > (INT_MAX / 18)) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iVertexCount = iPointCount * 4;
	iIndexCount = iPointCount * 18;
	pVertices = (xge_shape_vertex_t*)xrtMalloc((size_t)iVertexCount * sizeof(*pVertices));
	pIndices = (uint32_t*)xrtMalloc((size_t)iIndexCount * sizeof(*pIndices));
	if ( (pVertices == NULL) || (pIndices == NULL) ) {
		xrtFree(pVertices);
		xrtFree(pIndices);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	fHalfWidth = fWidth * 0.5f;
	pScreenCenters = NULL;
	pScreenLeft = NULL;
	pScreenRight = NULL;
	bScreenCoverage = !(
		(pShape->iLineJoin == XGE_SHAPE_EX_JOIN_MITER) &&
		__xgeShapeExStrokeContourWantsExplicitJoins(pPoints, iPointCount, 1)
	) && __xgeShapeExStrokeCoverageNeedsScreenSpace(pGeometryMatrix) &&
		__xgeShapeExMatrixInverse(*pGeometryMatrix, &tInverse);
	if ( bScreenCoverage ) {
		pScreenCenters = (xge_vec2_t*)xrtMalloc((size_t)iPointCount * sizeof(*pScreenCenters));
		pScreenLeft = (xge_vec2_t*)xrtMalloc((size_t)iPointCount * sizeof(*pScreenLeft));
		pScreenRight = (xge_vec2_t*)xrtMalloc((size_t)iPointCount * sizeof(*pScreenRight));
		if ( (pScreenCenters == NULL) || (pScreenLeft == NULL) || (pScreenRight == NULL) ) {
			xrtFree(pScreenCenters);
			xrtFree(pScreenLeft);
			xrtFree(pScreenRight);
			xrtFree(pVertices);
			xrtFree(pIndices);
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		for ( i = 0; i < iPointCount; i++ ) {
			int iPrev = (i + iPointCount - 1) % iPointCount;
			int iNext = (i + 1) % iPointCount;
			xge_vec2_t tLeft = __xgeShapeExStrokeSidePoint(
				pPoints[iPrev], pPoints[i], pPoints[iNext], fHalfWidth, fMiterLimit, 1
			);
			xge_vec2_t tRight = __xgeShapeExStrokeSidePoint(
				pPoints[iPrev], pPoints[i], pPoints[iNext], fHalfWidth, fMiterLimit, -1
			);

			pScreenCenters[i] = __xgeShapeExMatrixPoint(*pGeometryMatrix, pPoints[i]);
			pScreenLeft[i] = __xgeShapeExMatrixPoint(*pGeometryMatrix, tLeft);
			pScreenRight[i] = __xgeShapeExMatrixPoint(*pGeometryMatrix, tRight);
		}
		for ( i = 0; i < iPointCount; i++ ) {
			xge_vec2_t tInnerLeft;
			xge_vec2_t tInnerRight;
			xge_vec2_t tOuterLeft;
			xge_vec2_t tOuterRight;

			__xgeShapeExStrokeCoverageScreenPair(
				pScreenLeft, pScreenCenters, iPointCount, i, &tInnerLeft, &tOuterLeft
			);
			__xgeShapeExStrokeCoverageScreenPair(
				pScreenRight, pScreenCenters, iPointCount, i, &tInnerRight, &tOuterRight
			);
			tInnerLeft = __xgeShapeExMatrixPoint(tInverse, tInnerLeft);
			tInnerRight = __xgeShapeExMatrixPoint(tInverse, tInnerRight);
			tOuterLeft = __xgeShapeExMatrixPoint(tInverse, tOuterLeft);
			tOuterRight = __xgeShapeExMatrixPoint(tInverse, tOuterRight);
			pVertices[i] = (xge_shape_vertex_t){tInnerLeft.fX, tInnerLeft.fY, iColor};
			pVertices[iPointCount + i] = (xge_shape_vertex_t){tInnerRight.fX, tInnerRight.fY, iColor};
			pVertices[iPointCount * 2 + i] = (xge_shape_vertex_t){tOuterLeft.fX, tOuterLeft.fY, iColor & 0xFFFFFF00u};
			pVertices[iPointCount * 3 + i] = (xge_shape_vertex_t){tOuterRight.fX, tOuterRight.fY, iColor & 0xFFFFFF00u};
		}
	} else {
		fInnerHalfWidth = fmaxf(0.0f, fHalfWidth - fAAHalfWidth);
		fOuterHalfWidth = fHalfWidth + fAAHalfWidth;
		for ( i = 0; i < iPointCount; i++ ) {
			int iPrev = (i + iPointCount - 1) % iPointCount;
			int iNext = (i + 1) % iPointCount;
			xge_vec2_t tInnerLeft = __xgeShapeExStrokeSidePoint(pPoints[iPrev], pPoints[i], pPoints[iNext], fInnerHalfWidth, fMiterLimit, 1);
			xge_vec2_t tInnerRight = __xgeShapeExStrokeSidePoint(pPoints[iPrev], pPoints[i], pPoints[iNext], fInnerHalfWidth, fMiterLimit, -1);
			xge_vec2_t tOuterLeft = __xgeShapeExStrokeSidePoint(pPoints[iPrev], pPoints[i], pPoints[iNext], fOuterHalfWidth, fMiterLimit, 1);
			xge_vec2_t tOuterRight = __xgeShapeExStrokeSidePoint(pPoints[iPrev], pPoints[i], pPoints[iNext], fOuterHalfWidth, fMiterLimit, -1);

			pVertices[i] = (xge_shape_vertex_t){tInnerLeft.fX, tInnerLeft.fY, iColor};
			pVertices[iPointCount + i] = (xge_shape_vertex_t){tInnerRight.fX, tInnerRight.fY, iColor};
			pVertices[iPointCount * 2 + i] = (xge_shape_vertex_t){tOuterLeft.fX, tOuterLeft.fY, iColor & 0xFFFFFF00u};
			pVertices[iPointCount * 3 + i] = (xge_shape_vertex_t){tOuterRight.fX, tOuterRight.fY, iColor & 0xFFFFFF00u};
		}
	}
	for ( i = 0; i < iPointCount; i++ ) {
		uint32_t iInnerLeft = (uint32_t)i;
		uint32_t iInnerRight = (uint32_t)(iPointCount + i);
		uint32_t iOuterLeft = (uint32_t)(iPointCount * 2 + i);
		uint32_t iOuterRight = (uint32_t)(iPointCount * 3 + i);
		uint32_t iNextInnerLeft = (uint32_t)((i + 1) % iPointCount);
		uint32_t iNextInnerRight = (uint32_t)(iPointCount + ((i + 1) % iPointCount));
		uint32_t iNextOuterLeft = (uint32_t)(iPointCount * 2 + ((i + 1) % iPointCount));
		uint32_t iNextOuterRight = (uint32_t)(iPointCount * 3 + ((i + 1) % iPointCount));
		int iBase = i * 18;

		pIndices[iBase + 0] = iOuterLeft;
		pIndices[iBase + 1] = iNextOuterLeft;
		pIndices[iBase + 2] = iNextInnerLeft;
		pIndices[iBase + 3] = iOuterLeft;
		pIndices[iBase + 4] = iNextInnerLeft;
		pIndices[iBase + 5] = iInnerLeft;
		pIndices[iBase + 6] = iInnerLeft;
		pIndices[iBase + 7] = iNextInnerLeft;
		pIndices[iBase + 8] = iNextInnerRight;
		pIndices[iBase + 9] = iInnerLeft;
		pIndices[iBase + 10] = iNextInnerRight;
		pIndices[iBase + 11] = iInnerRight;
		pIndices[iBase + 12] = iInnerRight;
		pIndices[iBase + 13] = iNextInnerRight;
		pIndices[iBase + 14] = iNextOuterRight;
		pIndices[iBase + 15] = iInnerRight;
		pIndices[iBase + 16] = iNextOuterRight;
		pIndices[iBase + 17] = iOuterRight;
	}
	iRet = __xgeShapeExDrawStrokePaintMeshEx(
		pShape, pVertices, iVertexCount, pIndices, iIndexCount, iColor, tBounds, tMatrix,
		pGeometryMatrix, fOpacity, bScreenSpace, 1
	);
	xrtFree(pVertices);
	xrtFree(pIndices);
	xrtFree(pScreenCenters);
	xrtFree(pScreenLeft);
	xrtFree(pScreenRight);
	return iRet;
}

static int __xgeShapeExStrokeBandPixelAligned(const xge_vec2_t* pPoints, int iPointCount, int bClosed, float fHalfWidth, float fMiterLimit, int iLineCap, const xge_shape_ex_matrix_t* pGeometryMatrix)
{
	int i;

	if ( (pPoints == NULL) || (iPointCount < 2) || (iLineCap == XGE_SHAPE_EX_CAP_ROUND) ) {
		return 0;
	}
	if ( bClosed ) {
		if ( !__xgeShapeExContourIsAxisAlignedRect(pPoints, iPointCount) ) return 0;
	} else {
		float fDX;
		float fDY;

		if ( iPointCount != 2 ) return 0;
		fDX = fabsf(pPoints[1].fX - pPoints[0].fX);
		fDY = fabsf(pPoints[1].fY - pPoints[0].fY);
		if ( (fDX <= XGE_SHAPE_EX_EPSILON) == (fDY <= XGE_SHAPE_EX_EPSILON) ) return 0;
	}
	if ( (pGeometryMatrix != NULL) &&
	     ((fabsf(pGeometryMatrix->fB) > XGE_SHAPE_EX_EPSILON) || (fabsf(pGeometryMatrix->fC) > XGE_SHAPE_EX_EPSILON)) ) {
		return 0;
	}
	for ( i = 0; i < iPointCount; i++ ) {
		xge_vec2_t tLeft;
		xge_vec2_t tRight;
		xge_vec2_t arrPoints[2];
		int j;

		if ( !bClosed && (i == 0) ) {
			__xgeShapeExStrokeEndpointPair(pPoints[0], pPoints[1], fHalfWidth, iLineCap, 0, &tLeft, &tRight);
		} else if ( !bClosed && (i == (iPointCount - 1)) ) {
			__xgeShapeExStrokeEndpointPair(pPoints[iPointCount - 1], pPoints[iPointCount - 2], fHalfWidth, iLineCap, 1, &tLeft, &tRight);
		} else {
			int iPrev = (i + iPointCount - 1) % iPointCount;
			int iNext = (i + 1) % iPointCount;

			tLeft = __xgeShapeExStrokeSidePoint(pPoints[iPrev], pPoints[i], pPoints[iNext], fHalfWidth, fMiterLimit, 1);
			tRight = __xgeShapeExStrokeSidePoint(pPoints[iPrev], pPoints[i], pPoints[iNext], fHalfWidth, fMiterLimit, -1);
		}
		arrPoints[0] = tLeft;
		arrPoints[1] = tRight;
		for ( j = 0; j < 2; j++ ) {
			xge_vec2_t tPoint = arrPoints[j];

			if ( pGeometryMatrix != NULL ) {
				tPoint = __xgeShapeExMatrixPoint(*pGeometryMatrix, tPoint);
			}
			if ( (fabsf(tPoint.fX - roundf(tPoint.fX)) > 0.0005f) ||
			     (fabsf(tPoint.fY - roundf(tPoint.fY)) > 0.0005f) ) {
				return 0;
			}
		}
	}
	return 1;
}

static int __xgeShapeExDrawStrokeHardContourMesh(xge_shape_ex pShape, const xge_vec2_t* pPoints, int iPointCount, int bClosed, float fWidth, float fMiterLimit, uint32_t iColor, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, const xge_shape_ex_matrix_t* pGeometryMatrix, float fOpacity, int bScreenSpace)
{
	xge_shape_vertex_t* pVertices;
	uint32_t* pIndices;
	float fHalfWidth;
	int iSegmentCount;
	int iIndexCount;
	int iRet;
	int i;

	iSegmentCount = bClosed ? iPointCount : (iPointCount - 1);
	if ( iSegmentCount <= 0 ) return XGE_OK;
	iIndexCount = iSegmentCount * 6;
	pVertices = (xge_shape_vertex_t*)xrtMalloc((size_t)iPointCount * 2u * sizeof(*pVertices));
	pIndices = (uint32_t*)xrtMalloc((size_t)iIndexCount * sizeof(*pIndices));
	if ( (pVertices == NULL) || (pIndices == NULL) ) {
		xrtFree(pVertices);
		xrtFree(pIndices);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	fHalfWidth = fWidth * 0.5f;
	for ( i = 0; i < iPointCount; i++ ) {
		xge_vec2_t tLeft;
		xge_vec2_t tRight;

		if ( !bClosed && (i == 0) ) {
			__xgeShapeExStrokeEndpointPair(pPoints[0], pPoints[1], fHalfWidth, pShape->iLineCap, 0, &tLeft, &tRight);
		} else if ( !bClosed && (i == (iPointCount - 1)) ) {
			__xgeShapeExStrokeEndpointPair(pPoints[iPointCount - 1], pPoints[iPointCount - 2], fHalfWidth, pShape->iLineCap, 1, &tLeft, &tRight);
		} else {
			int iPrev = (i + iPointCount - 1) % iPointCount;
			int iNext = (i + 1) % iPointCount;

			tLeft = __xgeShapeExStrokeSidePoint(pPoints[iPrev], pPoints[i], pPoints[iNext], fHalfWidth, fMiterLimit, 1);
			tRight = __xgeShapeExStrokeSidePoint(pPoints[iPrev], pPoints[i], pPoints[iNext], fHalfWidth, fMiterLimit, -1);
		}
		pVertices[i * 2 + 0] = (xge_shape_vertex_t){tLeft.fX, tLeft.fY, iColor};
		pVertices[i * 2 + 1] = (xge_shape_vertex_t){tRight.fX, tRight.fY, iColor};
	}
	for ( i = 0; i < iSegmentCount; i++ ) {
		uint32_t iA = (uint32_t)(i * 2);
		uint32_t iB = (uint32_t)(((i + 1) % iPointCount) * 2);
		int iBase = i * 6;

		pIndices[iBase + 0] = iA;
		pIndices[iBase + 1] = iB;
		pIndices[iBase + 2] = iB + 1;
		pIndices[iBase + 3] = iA;
		pIndices[iBase + 4] = iB + 1;
		pIndices[iBase + 5] = iA + 1;
	}
	iRet = __xgeShapeExDrawStrokePaintMesh(
		pShape, pVertices, iPointCount * 2, pIndices, iIndexCount, iColor, tBounds, tMatrix,
		pGeometryMatrix, fOpacity, bScreenSpace
	);
	if ( (iRet == XGE_OK) && !bClosed && (pShape->iLineCap == XGE_SHAPE_EX_CAP_ROUND) ) {
		xge_vec2_t tDir;

		if ( __xgeShapeExNormalizeSegment(pPoints[0], pPoints[1], &tDir) ) {
			iRet = __xgeShapeExDrawStrokeRoundCap(
				pShape, pPoints[0], tDir.fX, tDir.fY, fHalfWidth, 0, iColor, tBounds, tMatrix,
				pGeometryMatrix, fOpacity, bScreenSpace
			);
		}
		if ( (iRet == XGE_OK) && __xgeShapeExNormalizeSegment(pPoints[iPointCount - 2], pPoints[iPointCount - 1], &tDir) ) {
			iRet = __xgeShapeExDrawStrokeRoundCap(
				pShape, pPoints[iPointCount - 1], tDir.fX, tDir.fY, fHalfWidth, 1, iColor, tBounds, tMatrix,
				pGeometryMatrix, fOpacity, bScreenSpace
			);
		}
	}
	xrtFree(pVertices);
	xrtFree(pIndices);
	return iRet;
}

static int __xgeShapeExDrawStrokeContourMesh(xge_shape_ex pShape, const xge_vec2_t* pSrc, int iCount, int bClosed, float fWidth, uint32_t iColor, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, const xge_shape_ex_matrix_t* pGeometryMatrix, float fOpacity, int bScreenSpace)
{
	xge_shape_vertex_t* pVertices;
	uint32_t* pIndices;
	xge_vec2_t* pPoints;
	xge_vec2_t* pOutline;
	float fHalfWidth;
	float fAAHalfWidth;
	float fMiterLimit;
	int iPointCount;
	int iIndexCount;
	int iOutlineCount;
	int iRet;

	if ( (pShape == NULL) || (pSrc == NULL) || (iCount < 2) || (fWidth <= XGE_SHAPE_EX_EPSILON) ) {
		return XGE_OK;
	}
	pVertices = NULL;
	pIndices = NULL;
	pPoints = NULL;
	pOutline = NULL;
	iRet = __xgeShapeExCompactStrokePoints(pSrc, iCount, bClosed, &pPoints, &iPointCount);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( iPointCount < 2 ) {
		xge_vec2_t tPoint;

		if ( iPointCount == 1 ) {
			tPoint = pPoints[0];
			xrtFree(pPoints);
			return __xgeShapeExDrawStrokeZeroLengthCap(pShape, tPoint, fWidth, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
		}
		xrtFree(pPoints);
		return XGE_OK;
	}
	fHalfWidth = fWidth * 0.5f;
	fAAHalfWidth = __xgeShapeExStrokeCoverageHalfWidth(pGeometryMatrix);
	fMiterLimit = pShape->fMiterLimit;
	if ( pShape->iLineJoin != XGE_SHAPE_EX_JOIN_MITER ) {
		fMiterLimit = 1.0f;
	}
	if ( fMiterLimit < 1.0f ) {
		fMiterLimit = 1.0f;
	}
	if ( pShape->bCoverageAntialiasDisabled ) {
		iRet = __xgeShapeExDrawStrokeHardContourMesh(
			pShape, pPoints, iPointCount, bClosed, fWidth, fMiterLimit, iColor, tBounds, tMatrix,
			pGeometryMatrix, fOpacity, bScreenSpace
		);
		xrtFree(pPoints);
		return iRet;
	}
	if ( __xgeShapeExStrokeBandPixelAligned(
		pPoints, iPointCount, bClosed, fHalfWidth, fMiterLimit, pShape->iLineCap, pGeometryMatrix
	) ) {
		iRet = __xgeShapeExDrawStrokeHardContourMesh(
			pShape, pPoints, iPointCount, bClosed, fWidth, fMiterLimit, iColor, tBounds, tMatrix,
			pGeometryMatrix, fOpacity, bScreenSpace
		);
		xrtFree(pPoints);
		return iRet;
	}
	if ( bClosed ) {
		if ( bScreenSpace && (pShape->iStrokeType == XGE_SHAPE_EX_FILL_SOLID) ) {
			iOutlineCount = 0;
			iRet = __xgeShapeExBuildClosedStrokeOutline(
				pShape, pPoints, iPointCount, fHalfWidth, pGeometryMatrix, &pOutline, &iOutlineCount
			);
			if ( iRet != XGE_OK ) {
				xrtFree(pPoints);
				return iRet;
			}
			if ( (pOutline != NULL) && (iOutlineCount >= 6) ) {
				iRet = __xgeShapeExDrawStrokeAreaCoverage(
					pShape, pOutline, iOutlineCount, iColor, tBounds, tMatrix,
					pGeometryMatrix, fOpacity
				);
				xrtFree(pOutline);
				xrtFree(pPoints);
				return iRet;
			}
			xrtFree(pOutline);
		}
		iRet = __xgeShapeExDrawClosedStrokeCoverageMesh(
			pShape, pPoints, iPointCount, fWidth, fMiterLimit, fAAHalfWidth, iColor, tBounds, tMatrix,
			pGeometryMatrix, fOpacity, bScreenSpace
		);
		xrtFree(pPoints);
		return iRet;
	}
	iOutlineCount = 0;
	iRet = __xgeShapeExBuildOpenStrokeOutline(
		pShape, pPoints, iPointCount, fHalfWidth, pGeometryMatrix, &pOutline, &iOutlineCount
	);
	if ( iRet != XGE_OK ) {
		xrtFree(pPoints);
		return iRet;
	}
	if ( (pOutline != NULL) && (iOutlineCount >= 3) ) {
		const xge_shape_ex_matrix_t* pCoverageGeometryMatrix = pGeometryMatrix;
		int iAAContourCount = 0;
		int iAAOuterVertexStart = 0;
		int iVertexCount = 0;

		iIndexCount = 0;
		if ( bScreenSpace && (pShape->iStrokeType == XGE_SHAPE_EX_FILL_SOLID) ) {
			iRet = __xgeShapeExDrawStrokeAreaCoverage(
				pShape, pOutline, iOutlineCount, iColor, tBounds, tMatrix,
				pGeometryMatrix, fOpacity
			);
			xrtFree(pOutline);
			xrtFree(pPoints);
			return iRet;
		}
		if ( (pShape->iLineJoin == XGE_SHAPE_EX_JOIN_MITER) &&
		     __xgeShapeExStrokeContourWantsExplicitJoins(pPoints, iPointCount, 0) ) {
			pCoverageGeometryMatrix = NULL;
		}
		iRet = __xgeShapeExBuildStrokeAAMesh(
			pOutline, iOutlineCount, iColor, fAAHalfWidth, pCoverageGeometryMatrix, &pVertices, &iVertexCount, &pIndices, &iIndexCount,
			&iAAContourCount, &iAAOuterVertexStart
		);
		if ( iRet != XGE_OK ) {
			xrtFree(pOutline);
			xrtFree(pPoints);
			return iRet;
		}
		if ( pVertices != NULL ) {
			iRet = __xgeShapeExDrawStrokePaintMeshEx(
				pShape, pVertices, iVertexCount, pIndices, iIndexCount, iColor, tBounds, tMatrix,
				pGeometryMatrix, fOpacity, bScreenSpace, 1
			);
			xrtFree(pVertices);
			xrtFree(pIndices);
			xrtFree(pOutline);
			xrtFree(pPoints);
			return iRet;
		}
	}
	xrtFree(pOutline);
	iRet = __xgeShapeExDrawStrokeHardContourMesh(
		pShape, pPoints, iPointCount, 0, fWidth, fMiterLimit, iColor, tBounds, tMatrix,
		pGeometryMatrix, fOpacity, bScreenSpace
	);
	xrtFree(pPoints);
	return iRet;
}

static int __xgeShapeExDrawStrokeSegmentJoinContour(xge_shape_ex pShape, const xge_vec2_t* pSrc, int iCount, int bClosed, float fWidth, uint32_t iColor, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, const xge_shape_ex_matrix_t* pGeometryMatrix, float fOpacity, int bScreenSpace)
{
	xge_vec2_t* pPoints;
	int iPointCount;
	int iSegmentCount;
	int iRet;
	int i;

	if ( (pShape == NULL) || (pSrc == NULL) || (iCount < 2) || (fWidth <= XGE_SHAPE_EX_EPSILON) ) {
		return XGE_OK;
	}
	pPoints = NULL;
	iPointCount = 0;
	iRet = __xgeShapeExCompactStrokePoints(pSrc, iCount, bClosed, &pPoints, &iPointCount);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( iPointCount < 2 ) {
		xge_vec2_t tPoint;

		if ( iPointCount == 1 ) {
			tPoint = pPoints[0];
			xrtFree(pPoints);
			return __xgeShapeExDrawStrokeZeroLengthCap(pShape, tPoint, fWidth, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
		}
		xrtFree(pPoints);
		return XGE_OK;
	}
	iSegmentCount = bClosed ? iPointCount : (iPointCount - 1);
	if ( pShape->bStrokeUnionMaskDraw ) {
		struct xge_shape_ex_t tSegmentShape = *pShape;
		float fHalfWidth = fWidth * 0.5f;
		float fOverlap = __xgeShapeExStrokeCoverageHalfWidth(pGeometryMatrix);

		tSegmentShape.iLineCap = XGE_SHAPE_EX_CAP_BUTT;
		for ( i = 0; i < iSegmentCount; i++ ) {
			xge_vec2_t arrSegment[2];
			xge_vec2_t tDir;

			arrSegment[0] = pPoints[i];
			arrSegment[1] = pPoints[(i + 1) % iPointCount];
			if ( __xgeShapeExNormalizeSegment(arrSegment[0], arrSegment[1], &tDir) ) {
				if ( bClosed || (i > 0) || (pShape->iLineCap != XGE_SHAPE_EX_CAP_BUTT) ) {
					arrSegment[0].fX -= tDir.fX * fOverlap;
					arrSegment[0].fY -= tDir.fY * fOverlap;
				}
				if ( bClosed || (i < (iSegmentCount - 1)) || (pShape->iLineCap != XGE_SHAPE_EX_CAP_BUTT) ) {
					arrSegment[1].fX += tDir.fX * fOverlap;
					arrSegment[1].fY += tDir.fY * fOverlap;
				}
			}
			iRet = __xgeShapeExDrawStrokeContourMesh(
				&tSegmentShape, arrSegment, 2, 0, fWidth, iColor, tBounds, tMatrix,
				pGeometryMatrix, fOpacity, bScreenSpace
			);
			if ( iRet != XGE_OK ) {
				xrtFree(pPoints);
				return iRet;
			}
		}
		if ( !bClosed && (pShape->iLineCap != XGE_SHAPE_EX_CAP_BUTT) ) {
			if ( __xgeShapeExSamePoint(pPoints[0], pPoints[iPointCount - 1]) ) {
				iRet = __xgeShapeExDrawStrokeZeroLengthCap(
					pShape, pPoints[0], fWidth, iColor, tBounds, tMatrix, pGeometryMatrix,
					fOpacity, bScreenSpace
				);
			} else {
				xge_vec2_t tStartDir;
				xge_vec2_t tEndDir;

				if ( __xgeShapeExNormalizeSegment(pPoints[0], pPoints[1], &tStartDir) ) {
					if ( pShape->iLineCap == XGE_SHAPE_EX_CAP_ROUND ) {
						iRet = __xgeShapeExDrawStrokeRoundCap(
							pShape, pPoints[0], tStartDir.fX, tStartDir.fY, fHalfWidth, 0,
							iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace
						);
					} else {
						xge_vec2_t arrCap[2];

						arrCap[0].fX = pPoints[0].fX - tStartDir.fX * fHalfWidth;
						arrCap[0].fY = pPoints[0].fY - tStartDir.fY * fHalfWidth;
						arrCap[1] = pPoints[0];
						iRet = __xgeShapeExDrawStrokeContourMesh(
							&tSegmentShape, arrCap, 2, 0, fWidth, iColor, tBounds, tMatrix,
							pGeometryMatrix, fOpacity, bScreenSpace
						);
					}
				}
				if ( (iRet == XGE_OK) && __xgeShapeExNormalizeSegment(
					pPoints[iPointCount - 2], pPoints[iPointCount - 1], &tEndDir
				) ) {
					if ( pShape->iLineCap == XGE_SHAPE_EX_CAP_ROUND ) {
						iRet = __xgeShapeExDrawStrokeRoundCap(
							pShape, pPoints[iPointCount - 1], tEndDir.fX, tEndDir.fY, fHalfWidth, 1,
							iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace
						);
					} else {
						xge_vec2_t arrCap[2];

						arrCap[0] = pPoints[iPointCount - 1];
						arrCap[1].fX = arrCap[0].fX + tEndDir.fX * fHalfWidth;
						arrCap[1].fY = arrCap[0].fY + tEndDir.fY * fHalfWidth;
						iRet = __xgeShapeExDrawStrokeContourMesh(
							&tSegmentShape, arrCap, 2, 0, fWidth, iColor, tBounds, tMatrix,
							pGeometryMatrix, fOpacity, bScreenSpace
						);
					}
				}
			}
			if ( iRet != XGE_OK ) {
				xrtFree(pPoints);
				return iRet;
			}
		}
		if ( iSegmentCount > 1 ) {
			int iJoinStart = bClosed ? 0 : 1;
			int iJoinEnd = bClosed ? iPointCount : (iPointCount - 1);

			for ( i = iJoinStart; i < iJoinEnd; i++ ) {
				int iPrev = (i + iPointCount - 1) % iPointCount;
				int iNext = (i + 1) % iPointCount;

				iRet = __xgeShapeExDrawStrokeJoin(
					pShape, pPoints[iPrev], pPoints[i], pPoints[iNext], fWidth, iColor,
					tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace
				);
				if ( iRet != XGE_OK ) {
					xrtFree(pPoints);
					return iRet;
				}
			}
		}
		xrtFree(pPoints);
		return XGE_OK;
	}
	for ( i = 0; i < iSegmentCount; i++ ) {
		xge_vec2_t tA = pPoints[i];
		xge_vec2_t tB = pPoints[(i + 1) % iPointCount];
		int bCapStart = (!bClosed && (i == 0));
		int bCapEnd = (!bClosed && (i == (iSegmentCount - 1)));

		iRet = __xgeShapeExDrawStrokeSegment(pShape, tA, tB, fWidth, iColor, bCapStart, bCapEnd, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
		if ( iRet != XGE_OK ) {
			xrtFree(pPoints);
			return iRet;
		}
	}
	if ( iSegmentCount > 1 ) {
		int iJoinStart = bClosed ? 0 : 1;
		int iJoinEnd = bClosed ? iPointCount : (iPointCount - 1);

		for ( i = iJoinStart; i < iJoinEnd; i++ ) {
			int iPrev = (i + iPointCount - 1) % iPointCount;
			int iNext = (i + 1) % iPointCount;

			iRet = __xgeShapeExDrawStrokeJoin(pShape, pPoints[iPrev], pPoints[i], pPoints[iNext], fWidth, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
			if ( iRet != XGE_OK ) {
				xrtFree(pPoints);
				return iRet;
			}
		}
	}
	xrtFree(pPoints);
	return XGE_OK;
}

static float __xgeShapeExStrokeJoinAngle(xge_vec2_t tPrev, xge_vec2_t tCurr, xge_vec2_t tNext)
{
	xge_vec2_t tD1;
	xge_vec2_t tD2;
	float fDot;

	if ( !__xgeShapeExNormalizeSegment(tPrev, tCurr, &tD1) ||
	     !__xgeShapeExNormalizeSegment(tCurr, tNext, &tD2) ) {
		return 0.0f;
	}
	fDot = tD1.fX * tD2.fX + tD1.fY * tD2.fY;
	if ( fDot < -1.0f ) fDot = -1.0f;
	if ( fDot > 1.0f ) fDot = 1.0f;
	return acosf(fDot);
}

static int __xgeShapeExStrokeContourWantsExplicitJoins(const xge_vec2_t* pSrc, int iCount, int bClosed)
{
	xge_vec2_t* pPoints;
	int iPointCount;
	int iSharpCount;
	int iJoinStart;
	int iJoinEnd;
	int i;

	if ( (pSrc == NULL) || (iCount < 3) ) {
		return 0;
	}
	pPoints = NULL;
	iPointCount = 0;
	if ( __xgeShapeExCompactStrokePoints(pSrc, iCount, bClosed, &pPoints, &iPointCount) != XGE_OK ) {
		return 0;
	}
	if ( iPointCount < 3 ) {
		xrtFree(pPoints);
		return 0;
	}
	iSharpCount = 0;
	iJoinStart = bClosed ? 0 : 1;
	iJoinEnd = bClosed ? iPointCount : (iPointCount - 1);
	for ( i = iJoinStart; i < iJoinEnd; i++ ) {
		int iPrev = (i + iPointCount - 1) % iPointCount;
		int iNext = (i + 1) % iPointCount;
		float fAngle = __xgeShapeExStrokeJoinAngle(pPoints[iPrev], pPoints[i], pPoints[iNext]);

		if ( fAngle >= XGE_SHAPE_EX_EXPLICIT_JOIN_ANGLE ) {
			iSharpCount++;
		}
	}
	xrtFree(pPoints);
	return (iSharpCount > 0) && ((iSharpCount * 3) >= (iJoinEnd - iJoinStart));
}

static int __xgeShapeExStrokeContourMiterLimitExceeded(const xge_vec2_t* pSrc, int iCount, int bClosed, float fMiterLimit)
{
	xge_vec2_t* pPoints;
	int iPointCount;
	int iJoinStart;
	int iJoinEnd;
	int i;

	if ( (pSrc == NULL) || (iCount < 3) ) {
		return 0;
	}
	if ( fMiterLimit < 1.0f ) {
		fMiterLimit = 1.0f;
	}
	pPoints = NULL;
	iPointCount = 0;
	if ( __xgeShapeExCompactStrokePoints(pSrc, iCount, bClosed, &pPoints, &iPointCount) != XGE_OK ) {
		return 0;
	}
	if ( iPointCount < 3 ) {
		xrtFree(pPoints);
		return 0;
	}
	iJoinStart = bClosed ? 0 : 1;
	iJoinEnd = bClosed ? iPointCount : (iPointCount - 1);
	for ( i = iJoinStart; i < iJoinEnd; i++ ) {
		int iPrev = (i + iPointCount - 1) % iPointCount;
		int iNext = (i + 1) % iPointCount;
		float fAngle = __xgeShapeExStrokeJoinAngle(pPoints[iPrev], pPoints[i], pPoints[iNext]);

		if ( fAngle >= XGE_SHAPE_EX_EXPLICIT_JOIN_ANGLE ) {
			float fCos = cosf(fAngle * 0.5f);

			if ( (fCos > XGE_SHAPE_EX_EPSILON) && ((1.0f / fCos) > (fMiterLimit + XGE_SHAPE_EX_EPSILON)) ) {
				xrtFree(pPoints);
				return 1;
			}
		}
	}
	xrtFree(pPoints);
	return 0;
}

static int __xgeShapeExStrokeContourNeedsUnion(const xge_vec2_t* pSrc, int iCount, int bClosed)
{
	xge_vec2_t* pPoints;
	int iPointCount;
	int iJoinStart;
	int iJoinEnd;
	int i;

	if ( (pSrc == NULL) || (iCount < 3) ) return 0;
	pPoints = NULL;
	iPointCount = 0;
	if ( __xgeShapeExCompactStrokePoints(pSrc, iCount, bClosed, &pPoints, &iPointCount) != XGE_OK ) {
		return 0;
	}
	if ( iPointCount < 3 ) {
		xrtFree(pPoints);
		return 0;
	}
	iJoinStart = bClosed ? 0 : 1;
	iJoinEnd = bClosed ? iPointCount : (iPointCount - 1);
	for ( i = iJoinStart; i < iJoinEnd; i++ ) {
		int iPrev = (i + iPointCount - 1) % iPointCount;
		int iNext = (i + 1) % iPointCount;
		xge_vec2_t tD1;
		xge_vec2_t tD2;

		if ( __xgeShapeExNormalizeSegment(pPoints[iPrev], pPoints[i], &tD1) &&
		     __xgeShapeExNormalizeSegment(pPoints[i], pPoints[iNext], &tD2) &&
		     ((tD1.fX * tD2.fX + tD1.fY * tD2.fY) <= XGE_SHAPE_EX_STROKE_UNION_DOT) ) {
			xrtFree(pPoints);
			return 1;
		}
	}
	xrtFree(pPoints);
	return 0;
}

static int __xgeShapeExFlatStrokeNeedsUnion(const xge_shape_ex_flat_path_t* pFlat)
{
	int i;

	if ( pFlat == NULL ) return 0;
	for ( i = 0; i < pFlat->iContourCount; i++ ) {
		const xge_shape_ex_flat_contour_t* pContour = &pFlat->pContours[i];

		if ( (pContour->iCount >= 3) &&
		     __xgeShapeExStrokeContourNeedsUnion(
			pFlat->pPoints + pContour->iStart, pContour->iCount, pContour->bClosed
		     ) ) return 1;
	}
	return 0;
}

static int __xgeShapeExDashLogicalCount(int iDashCount)
{
	if ( iDashCount <= 0 ) {
		return 0;
	}
	if ( (iDashCount & 1) != 0 ) {
		if ( iDashCount > (INT_MAX / 2) ) {
			return 0;
		}
		return iDashCount * 2;
	}
	return iDashCount;
}

static float __xgeShapeExDashTotal(const float* pDashPattern, int iDashCount)
{
	float fDashTotal;
	int iLogicalCount;
	int i;

	if ( (pDashPattern == NULL) || (iDashCount <= 0) ) {
		return 0.0f;
	}
	iLogicalCount = __xgeShapeExDashLogicalCount(iDashCount);
	fDashTotal = 0.0f;
	for ( i = 0; i < iLogicalCount; i++ ) {
		float fValue = pDashPattern[i % iDashCount];

		if ( fValue > 0.0f ) {
			fDashTotal += fValue;
		}
	}
	return fDashTotal;
}

static float __xgeShapeExDashMinPositive(const float* pDashPattern, int iDashCount)
{
	float fMin;
	int i;

	fMin = 0.0f;
	for ( i = 0; i < iDashCount; i++ ) {
		if ( pDashPattern[i] > XGE_SHAPE_EX_EPSILON ) {
			if ( (fMin <= 0.0f) || (pDashPattern[i] < fMin) ) {
				fMin = pDashPattern[i];
			}
		}
	}
	return fMin;
}

static void __xgeShapeExDashStateInit(const float* pDashPattern, int iDashCount, float fDashOffset, int* pDashIndex, int* pDashOn, float* pDashRemaining)
{
	float fDashTotal;
	float fOffset;
	int iLogicalCount;

	*pDashIndex = 0;
	*pDashOn = 1;
	*pDashRemaining = (iDashCount > 0) ? pDashPattern[0] : 0.0f;
	iLogicalCount = __xgeShapeExDashLogicalCount(iDashCount);
	fDashTotal = __xgeShapeExDashTotal(pDashPattern, iDashCount);
	if ( (iLogicalCount <= 0) || (fDashTotal <= XGE_SHAPE_EX_EPSILON) ) {
		return;
	}
	fOffset = fmodf(fDashOffset, fDashTotal);
	if ( fOffset < 0.0f ) {
		fOffset += fDashTotal;
	}
	while ( fOffset > XGE_SHAPE_EX_EPSILON ) {
		float fStep = pDashPattern[*pDashIndex % iDashCount];

		if ( fStep <= XGE_SHAPE_EX_EPSILON ) {
			*pDashIndex = (*pDashIndex + 1) % iLogicalCount;
			*pDashOn = !*pDashOn;
			continue;
		}
		if ( fStep > fOffset ) {
			*pDashRemaining = fStep - fOffset;
			return;
		}
		fOffset -= fStep;
		*pDashIndex = (*pDashIndex + 1) % iLogicalCount;
		*pDashOn = !*pDashOn;
	}
	*pDashRemaining = pDashPattern[*pDashIndex % iDashCount];
}

static void __xgeShapeExDashStateNext(const float* pDashPattern, int iDashCount, int* pDashIndex, int* pDashOn, float* pDashRemaining)
{
	int iLogicalCount;
	int iGuard;

	iLogicalCount = __xgeShapeExDashLogicalCount(iDashCount);
	if ( (pDashPattern == NULL) || (iLogicalCount <= 0) ) {
		*pDashRemaining = 0.0f;
		return;
	}
	iGuard = 0;
	do {
		*pDashIndex = (*pDashIndex + 1) % iLogicalCount;
		*pDashOn = !*pDashOn;
		*pDashRemaining = pDashPattern[*pDashIndex % iDashCount];
		iGuard++;
	} while ( (*pDashRemaining <= XGE_SHAPE_EX_EPSILON) && (iGuard <= iLogicalCount) );
}

static int __xgeShapeExStrokePointInZeroCap(xge_shape_ex pShape, xge_vec2_t tCenter, float fHalfWidth, float fTolerance, xge_vec2_t tPoint)
{
	float fHitWidth;
	float fDX;
	float fDY;

	if ( (pShape == NULL) || (fHalfWidth <= XGE_SHAPE_EX_EPSILON) ) {
		return 0;
	}
	if ( pShape->iLineCap == XGE_SHAPE_EX_CAP_BUTT ) {
		return 0;
	}
	(void)fTolerance;
	fHitWidth = fHalfWidth;
	fDX = tPoint.fX - tCenter.fX;
	fDY = tPoint.fY - tCenter.fY;
	if ( pShape->iLineCap == XGE_SHAPE_EX_CAP_ROUND ) {
		return (fDX * fDX + fDY * fDY) <= (fHitWidth * fHitWidth);
	}
	return (fabsf(fDX) <= fHitWidth) && (fabsf(fDY) <= fHitWidth);
}

static int __xgeShapeExStrokeSegmentContainsPoint(xge_shape_ex pShape, xge_vec2_t tA, xge_vec2_t tB, int bCapStart, int bCapEnd, float fHalfWidth, float fTolerance, xge_vec2_t tPoint)
{
	float fDX;
	float fDY;
	float fLen;
	float fDirX;
	float fDirY;
	float fNX;
	float fNY;
	float fAlong;
	float fAcross;
	float fStart;
	float fEnd;
	float fHitWidth;

	if ( (pShape == NULL) || (fHalfWidth <= XGE_SHAPE_EX_EPSILON) ) {
		return 0;
	}
	fDX = tB.fX - tA.fX;
	fDY = tB.fY - tA.fY;
	fLen = hypotf(fDX, fDY);
	if ( fLen <= XGE_SHAPE_EX_EPSILON ) {
		return __xgeShapeExStrokePointInZeroCap(pShape, tA, fHalfWidth, fTolerance, tPoint);
	}
	fDirX = fDX / fLen;
	fDirY = fDY / fLen;
	fNX = -fDirY;
	fNY = fDirX;
	fStart = 0.0f;
	fEnd = fLen;
	if ( pShape->iLineCap == XGE_SHAPE_EX_CAP_SQUARE ) {
		if ( bCapStart ) {
			fStart -= fHalfWidth;
		}
		if ( bCapEnd ) {
			fEnd += fHalfWidth;
		}
	}
	fAlong = ((tPoint.fX - tA.fX) * fDirX) + ((tPoint.fY - tA.fY) * fDirY);
	fAcross = fabsf(((tPoint.fX - tA.fX) * fNX) + ((tPoint.fY - tA.fY) * fNY));
	(void)fTolerance;
	fHitWidth = fHalfWidth;
	if ( (fAlong >= fStart - fTolerance) && (fAlong <= fEnd + fTolerance) && (fAcross <= fHitWidth) ) {
		return 1;
	}
	if ( pShape->iLineCap == XGE_SHAPE_EX_CAP_ROUND ) {
		float fHitWidthSq = fHitWidth * fHitWidth;

		if ( bCapStart ) {
			float fCapDX = tPoint.fX - tA.fX;
			float fCapDY = tPoint.fY - tA.fY;

			if ( (fCapDX * fCapDX + fCapDY * fCapDY) <= fHitWidthSq ) {
				return 1;
			}
		}
		if ( bCapEnd ) {
			float fCapDX = tPoint.fX - tB.fX;
			float fCapDY = tPoint.fY - tB.fY;

			if ( (fCapDX * fCapDX + fCapDY * fCapDY) <= fHitWidthSq ) {
				return 1;
			}
		}
	}
	return 0;
}

static int __xgeShapeExStrokeJoinContainsPoint(xge_shape_ex pShape, xge_vec2_t tPrev, xge_vec2_t tCurr, xge_vec2_t tNext, float fHalfWidth, float fTolerance, xge_vec2_t tPoint)
{
	xge_vec2_t tD1;
	xge_vec2_t tD2;
	xge_vec2_t tN1;
	xge_vec2_t tN2;
	xge_vec2_t tOuter1;
	xge_vec2_t tOuter2;
	float fCross;
	float fHitHalfWidth;

	if ( (pShape == NULL) || (fHalfWidth <= XGE_SHAPE_EX_EPSILON) ) {
		return 0;
	}
	if ( !__xgeShapeExNormalizeSegment(tPrev, tCurr, &tD1) ||
	     !__xgeShapeExNormalizeSegment(tCurr, tNext, &tD2) ) {
		return 0;
	}
	fCross = tD1.fX * tD2.fY - tD1.fY * tD2.fX;
	if ( fabsf(fCross) <= XGE_SHAPE_EX_EPSILON ) {
		float fDot = tD1.fX * tD2.fX + tD1.fY * tD2.fY;

		if ( (fDot < 0.0f) && (pShape->iLineJoin == XGE_SHAPE_EX_JOIN_ROUND) ) {
			float fDX = tPoint.fX - tCurr.fX;
			float fDY = tPoint.fY - tCurr.fY;

			fHitHalfWidth = fHalfWidth + fmaxf(fTolerance, XGE_SHAPE_EX_EPSILON);
			return (fDX * fDX + fDY * fDY) <= (fHitHalfWidth * fHitHalfWidth);
		}
		return 0;
	}
	fHitHalfWidth = fHalfWidth + fmaxf(fTolerance, XGE_SHAPE_EX_EPSILON);
	tN1.fX = -tD1.fY * fHitHalfWidth;
	tN1.fY =  tD1.fX * fHitHalfWidth;
	tN2.fX = -tD2.fY * fHitHalfWidth;
	tN2.fY =  tD2.fX * fHitHalfWidth;
	if ( fCross > 0.0f ) {
		tOuter1.fX = -tN1.fX;
		tOuter1.fY = -tN1.fY;
		tOuter2.fX = -tN2.fX;
		tOuter2.fY = -tN2.fY;
	} else {
		tOuter1 = tN1;
		tOuter2 = tN2;
	}
	if ( pShape->iLineJoin == XGE_SHAPE_EX_JOIN_ROUND ) {
		float fDX = tPoint.fX - tCurr.fX;
		float fDY = tPoint.fY - tCurr.fY;

		return (fDX * fDX + fDY * fDY) <= (fHitHalfWidth * fHitHalfWidth);
	}
	if ( pShape->iLineJoin == XGE_SHAPE_EX_JOIN_MITER ) {
		xge_vec2_t tMiter;
		xge_vec2_t tP1;
		xge_vec2_t tP2;

		tP1.fX = tCurr.fX + tOuter1.fX;
		tP1.fY = tCurr.fY + tOuter1.fY;
		tP2.fX = tCurr.fX + tOuter2.fX;
		tP2.fY = tCurr.fY + tOuter2.fY;
		if ( __xgeShapeExLineIntersection(tP1, tD1, tP2, tD2, &tMiter) &&
		     (hypotf(tMiter.fX - tCurr.fX, tMiter.fY - tCurr.fY) <= fHitHalfWidth * pShape->fMiterLimit) ) {
			return __xgeShapeExPointInTriangle(tPoint, tCurr, tP1, tMiter) ||
			       __xgeShapeExPointInTriangle(tPoint, tCurr, tMiter, tP2);
		}
	}
	{
		xge_vec2_t tP1;
		xge_vec2_t tP2;

		tP1.fX = tCurr.fX + tOuter1.fX;
		tP1.fY = tCurr.fY + tOuter1.fY;
		tP2.fX = tCurr.fX + tOuter2.fX;
		tP2.fY = tCurr.fY + tOuter2.fY;
		return __xgeShapeExPointInTriangle(tPoint, tCurr, tP1, tP2);
	}
}

static int __xgeShapeExFlatStrokeSolidContainsPoint(xge_shape_ex pShape, const xge_shape_ex_flat_path_t* pFlat, float fStrokeWidth, float fTolerance, xge_vec2_t tPoint)
{
	float fHalfWidth;
	int i;

	if ( (pShape == NULL) || (pFlat == NULL) || (fStrokeWidth <= XGE_SHAPE_EX_EPSILON) ) {
		return 0;
	}
	fHalfWidth = fStrokeWidth * 0.5f;
	for ( i = 0; i < pFlat->iContourCount; i++ ) {
		const xge_shape_ex_flat_contour_t* pContour = &pFlat->pContours[i];
		const xge_vec2_t* pSrc;
		xge_vec2_t* pPoints;
		int iPointCount;
		int iSegmentCount;
		int j;

		if ( (pContour->iCount <= 0) || (pContour->iStart < 0) || ((pContour->iStart + pContour->iCount) > pFlat->iPointCount) ) {
			continue;
		}
		pSrc = pFlat->pPoints + pContour->iStart;
		pPoints = NULL;
		iPointCount = 0;
		if ( __xgeShapeExCompactStrokePoints(pSrc, pContour->iCount, pContour->bClosed, &pPoints, &iPointCount) != XGE_OK ) {
			return 0;
		}
		if ( iPointCount == 1 ) {
			int bHit = __xgeShapeExStrokePointInZeroCap(pShape, pPoints[0], fHalfWidth, fTolerance, tPoint);

			xrtFree(pPoints);
			if ( bHit ) {
				return 1;
			}
			continue;
		}
		if ( iPointCount < 2 ) {
			xrtFree(pPoints);
			continue;
		}
		iSegmentCount = pContour->bClosed ? iPointCount : (iPointCount - 1);
		for ( j = 0; j < iSegmentCount; j++ ) {
			xge_vec2_t tA = pPoints[j];
			xge_vec2_t tB = pPoints[(j + 1) % iPointCount];
			int bCapStart = (!pContour->bClosed && (j == 0));
			int bCapEnd = (!pContour->bClosed && (j == (iSegmentCount - 1)));

			if ( __xgeShapeExStrokeSegmentContainsPoint(pShape, tA, tB, bCapStart, bCapEnd, fHalfWidth, fTolerance, tPoint) ) {
				xrtFree(pPoints);
				return 1;
			}
		}
		if ( iSegmentCount > 1 ) {
			int iJoinStart = pContour->bClosed ? 0 : 1;
			int iJoinEnd = pContour->bClosed ? iPointCount : (iPointCount - 1);

			for ( j = iJoinStart; j < iJoinEnd; j++ ) {
				int iPrev = (j + iPointCount - 1) % iPointCount;
				int iNext = (j + 1) % iPointCount;

				if ( __xgeShapeExStrokeJoinContainsPoint(pShape, pPoints[iPrev], pPoints[j], pPoints[iNext], fHalfWidth, fTolerance, tPoint) ) {
					xrtFree(pPoints);
					return 1;
				}
			}
		}
		xrtFree(pPoints);
	}
	return 0;
}

static int __xgeShapeExFlatStrokeDashedContainsPoint(xge_shape_ex pShape, const xge_shape_ex_flat_path_t* pFlat, float fStrokeWidth, const float* pDashPattern, int iDashCount, float fDashOffset, float fTolerance, xge_vec2_t tPoint)
{
	float fDashTotal;
	float fDashMin;
	int i;

	if ( (pFlat == NULL) || (pDashPattern == NULL) || (iDashCount <= 0) ) {
		return __xgeShapeExFlatStrokeSolidContainsPoint(pShape, pFlat, fStrokeWidth, fTolerance, tPoint);
	}
	fDashTotal = __xgeShapeExDashTotal(pDashPattern, iDashCount);
	if ( fDashTotal <= XGE_SHAPE_EX_EPSILON ) {
		return __xgeShapeExFlatStrokeSolidContainsPoint(pShape, pFlat, fStrokeWidth, fTolerance, tPoint);
	}
	fDashMin = __xgeShapeExDashMinPositive(pDashPattern, iDashCount);
	if ( fDashMin <= XGE_SHAPE_EX_EPSILON ) {
		return __xgeShapeExFlatStrokeSolidContainsPoint(pShape, pFlat, fStrokeWidth, fTolerance, tPoint);
	}
	for ( i = 0; i < pFlat->iContourCount; i++ ) {
		const xge_shape_ex_flat_contour_t* pContour = &pFlat->pContours[i];
		float fContourLength;
		float fDashRemaining = 0.0f;
		float fPos;
		float fDashEpsilon;
		float fGuardEstimate;
		int iGuard;
		int iGuardLimit;
		int iDashIndex = 0;
		int bDashOn = 1;

		if ( (pContour->iCount < 2) || (pContour->iStart < 0) || ((pContour->iStart + pContour->iCount) > pFlat->iPointCount) ) {
			continue;
		}
		fContourLength = __xgeShapeExFlatContourLength(pFlat, pContour);
		__xgeShapeExDashStateInit(pDashPattern, iDashCount, fDashOffset, &iDashIndex, &bDashOn, &fDashRemaining);
		if ( fContourLength <= XGE_SHAPE_EX_EPSILON ) {
			if ( bDashOn && (fDashRemaining > XGE_SHAPE_EX_EPSILON) &&
			     __xgeShapeExStrokePointInZeroCap(pShape, pFlat->pPoints[pContour->iStart], fStrokeWidth * 0.5f, fTolerance, tPoint) ) {
				return 1;
			}
			continue;
		}
		fPos = 0.0f;
		fDashEpsilon = fmaxf(XGE_SHAPE_EX_EPSILON, fContourLength * 0.000001f);
		fGuardEstimate = (fContourLength / fDashMin) + (float)__xgeShapeExDashLogicalCount(iDashCount) + 16.0f;
		if ( fGuardEstimate > 2000000.0f ) {
			return 0;
		}
		iGuard = 0;
		iGuardLimit = (int)ceilf(fGuardEstimate);
		while ( fPos < fContourLength - fDashEpsilon ) {
			float fStep = fDashRemaining;
			float fNext;

			if ( ++iGuard > iGuardLimit ) {
				return 0;
			}
			if ( fStep <= fDashEpsilon ) {
				__xgeShapeExDashStateNext(pDashPattern, iDashCount, &iDashIndex, &bDashOn, &fDashRemaining);
				continue;
			}
			fNext = fPos + fStep;
			if ( fNext <= fPos + fDashEpsilon ) {
				fDashRemaining = 0.0f;
				__xgeShapeExDashStateNext(pDashPattern, iDashCount, &iDashIndex, &bDashOn, &fDashRemaining);
				continue;
			}
			if ( fNext > fContourLength ) {
				fNext = fContourLength;
			}
			if ( bDashOn && (fNext > fPos + fDashEpsilon) ) {
				xge_shape_ex_flat_path_t tDashFlat;
				int iRet;

				memset(&tDashFlat, 0, sizeof(tDashFlat));
				iRet = __xgeShapeExFlatAddTrimmedContourRange(pFlat, i, fPos, fNext, &tDashFlat);
				if ( iRet != XGE_OK ) {
					__xgeShapeExFlatFree(&tDashFlat);
					return 0;
				}
				if ( __xgeShapeExFlatStrokeSolidContainsPoint(pShape, &tDashFlat, fStrokeWidth, fTolerance, tPoint) ) {
					__xgeShapeExFlatFree(&tDashFlat);
					return 1;
				}
				__xgeShapeExFlatFree(&tDashFlat);
			}
			fDashRemaining -= (fNext - fPos);
			fPos = fNext;
			if ( fDashRemaining <= fDashEpsilon ) {
				__xgeShapeExDashStateNext(pDashPattern, iDashCount, &iDashIndex, &bDashOn, &fDashRemaining);
			}
		}
	}
	return 0;
}

static int __xgeShapeExFlatFillIntersectsRect(const xge_shape_ex_flat_path_t* pFlat, int iFillRule, xge_rect_t tRect)
{
	xge_vec2_t pRect[4];
	int i;

	if ( pFlat == NULL ) return 0;
	__xgeShapeExRectPoints(tRect, pRect);
	for ( i = 0; i < 4; i++ ) {
		if ( __xgeShapeExFlatContainsPoint(pFlat, iFillRule, pRect[i]) ) return 1;
	}
	for ( i = 0; i < pFlat->iContourCount; i++ ) {
		const xge_shape_ex_flat_contour_t* pContour = &pFlat->pContours[i];
		const xge_vec2_t* pPoints;
		int j;

		if ( (pContour->iCount < 3) || (pContour->iStart < 0) ||
		     ((pContour->iStart + pContour->iCount) > pFlat->iPointCount) ) continue;
		pPoints = pFlat->pPoints + pContour->iStart;
		for ( j = 0; j < pContour->iCount; j++ ) {
			if ( __xgeShapeExRectContainsPoint(tRect, pPoints[j]) ||
			     __xgeShapeExSegmentIntersectsRect(pPoints[j], pPoints[(j + 1) % pContour->iCount], tRect) ) return 1;
		}
	}
	return 0;
}

static int __xgeShapeExStrokePointIntersectsRect(xge_shape_ex pShape, xge_vec2_t tCenter, float fHalfWidth, float fTolerance, xge_rect_t tRect)
{
	float fHitWidth;

	if ( (pShape == NULL) || (fHalfWidth <= XGE_SHAPE_EX_EPSILON) ||
	     (pShape->iLineCap == XGE_SHAPE_EX_CAP_BUTT) ) return 0;
	fHitWidth = fHalfWidth + fmaxf(fTolerance, XGE_SHAPE_EX_EPSILON);
	if ( pShape->iLineCap == XGE_SHAPE_EX_CAP_ROUND ) {
		return __xgeShapeExCircleIntersectsRect(tCenter, fHitWidth, tRect);
	}
	return __xgeShapeExRectIntersects(
		(xge_rect_t){tCenter.fX - fHitWidth, tCenter.fY - fHitWidth, fHitWidth * 2.0f, fHitWidth * 2.0f},
		tRect
	);
}

static int __xgeShapeExStrokeSegmentIntersectsRect(
	xge_shape_ex pShape,
	xge_vec2_t tA,
	xge_vec2_t tB,
	int bCapStart,
	int bCapEnd,
	float fHalfWidth,
	float fTolerance,
	xge_rect_t tRect
)
{
	xge_vec2_t pBody[4];
	float fDX;
	float fDY;
	float fLength;
	float fDirX;
	float fDirY;
	float fNX;
	float fNY;
	float fStart;
	float fEnd;
	float fHitWidth;
	xge_vec2_t tStart;
	xge_vec2_t tEnd;

	if ( (pShape == NULL) || (fHalfWidth <= XGE_SHAPE_EX_EPSILON) ) return 0;
	fDX = tB.fX - tA.fX;
	fDY = tB.fY - tA.fY;
	fLength = hypotf(fDX, fDY);
	if ( fLength <= XGE_SHAPE_EX_EPSILON ) {
		return __xgeShapeExStrokePointIntersectsRect(pShape, tA, fHalfWidth, fTolerance, tRect);
	}
	fDirX = fDX / fLength;
	fDirY = fDY / fLength;
	fNX = -fDirY;
	fNY = fDirX;
	fStart = 0.0f;
	fEnd = fLength;
	fHitWidth = fHalfWidth + fmaxf(fTolerance, XGE_SHAPE_EX_EPSILON);
	if ( pShape->iLineCap == XGE_SHAPE_EX_CAP_SQUARE ) {
		if ( bCapStart ) fStart -= fHalfWidth;
		if ( bCapEnd ) fEnd += fHalfWidth;
	}
	tStart = (xge_vec2_t){tA.fX + fDirX * fStart, tA.fY + fDirY * fStart};
	tEnd = (xge_vec2_t){tA.fX + fDirX * fEnd, tA.fY + fDirY * fEnd};
	pBody[0] = (xge_vec2_t){tStart.fX + fNX * fHitWidth, tStart.fY + fNY * fHitWidth};
	pBody[1] = (xge_vec2_t){tEnd.fX + fNX * fHitWidth, tEnd.fY + fNY * fHitWidth};
	pBody[2] = (xge_vec2_t){tEnd.fX - fNX * fHitWidth, tEnd.fY - fNY * fHitWidth};
	pBody[3] = (xge_vec2_t){tStart.fX - fNX * fHitWidth, tStart.fY - fNY * fHitWidth};
	if ( __xgeShapeExConvexPolygonIntersectsRectArea(pBody, 4, tRect) ) return 1;
	if ( pShape->iLineCap == XGE_SHAPE_EX_CAP_ROUND ) {
		if ( bCapStart && __xgeShapeExCircleIntersectsRect(tA, fHitWidth, tRect) ) return 1;
		if ( bCapEnd && __xgeShapeExCircleIntersectsRect(tB, fHitWidth, tRect) ) return 1;
	}
	return 0;
}

static int __xgeShapeExStrokeJoinIntersectsRect(
	xge_shape_ex pShape,
	xge_vec2_t tPrev,
	xge_vec2_t tCurr,
	xge_vec2_t tNext,
	float fHalfWidth,
	float fTolerance,
	xge_rect_t tRect
)
{
	xge_vec2_t tD1;
	xge_vec2_t tD2;
	xge_vec2_t tN1;
	xge_vec2_t tN2;
	xge_vec2_t tOuter1;
	xge_vec2_t tOuter2;
	xge_vec2_t pJoin[4];
	float fCross;
	float fHitWidth;

	if ( (pShape == NULL) || (fHalfWidth <= XGE_SHAPE_EX_EPSILON) ||
	     !__xgeShapeExNormalizeSegment(tPrev, tCurr, &tD1) ||
	     !__xgeShapeExNormalizeSegment(tCurr, tNext, &tD2) ) return 0;
	(void)fTolerance;
	fHitWidth = fHalfWidth;
	fCross = tD1.fX * tD2.fY - tD1.fY * tD2.fX;
	if ( fabsf(fCross) <= XGE_SHAPE_EX_EPSILON ) {
		float fDot = tD1.fX * tD2.fX + tD1.fY * tD2.fY;
		return (fDot < 0.0f) && (pShape->iLineJoin == XGE_SHAPE_EX_JOIN_ROUND) &&
		       __xgeShapeExCircleIntersectsRect(tCurr, fHitWidth, tRect);
	}
	if ( pShape->iLineJoin == XGE_SHAPE_EX_JOIN_ROUND ) {
		return __xgeShapeExCircleIntersectsRect(tCurr, fHitWidth, tRect);
	}
	tN1 = (xge_vec2_t){-tD1.fY * fHitWidth, tD1.fX * fHitWidth};
	tN2 = (xge_vec2_t){-tD2.fY * fHitWidth, tD2.fX * fHitWidth};
	if ( fCross > 0.0f ) {
		tOuter1 = (xge_vec2_t){-tN1.fX, -tN1.fY};
		tOuter2 = (xge_vec2_t){-tN2.fX, -tN2.fY};
	} else {
		tOuter1 = tN1;
		tOuter2 = tN2;
	}
	pJoin[0] = tCurr;
	pJoin[1] = (xge_vec2_t){tCurr.fX + tOuter1.fX, tCurr.fY + tOuter1.fY};
	if ( pShape->iLineJoin == XGE_SHAPE_EX_JOIN_MITER ) {
		xge_vec2_t tMiter;

		if ( __xgeShapeExLineIntersection(pJoin[1], tD1,
			(xge_vec2_t){tCurr.fX + tOuter2.fX, tCurr.fY + tOuter2.fY}, tD2, &tMiter) &&
		     (hypotf(tMiter.fX - tCurr.fX, tMiter.fY - tCurr.fY) <= fHitWidth * pShape->fMiterLimit) ) {
			pJoin[2] = tMiter;
			pJoin[3] = (xge_vec2_t){tCurr.fX + tOuter2.fX, tCurr.fY + tOuter2.fY};
			return __xgeShapeExConvexPolygonIntersectsRectArea(pJoin, 4, tRect);
		}
	}
	pJoin[2] = (xge_vec2_t){tCurr.fX + tOuter2.fX, tCurr.fY + tOuter2.fY};
	return __xgeShapeExConvexPolygonIntersectsRectArea(pJoin, 3, tRect);
}

static int __xgeShapeExFlatStrokeSolidIntersectsRect(
	xge_shape_ex pShape,
	const xge_shape_ex_flat_path_t* pFlat,
	float fStrokeWidth,
	float fTolerance,
	xge_rect_t tRect
)
{
	float fHalfWidth;
	int i;

	if ( (pShape == NULL) || (pFlat == NULL) || (fStrokeWidth <= XGE_SHAPE_EX_EPSILON) ) return 0;
	fHalfWidth = fStrokeWidth * 0.5f;
	for ( i = 0; i < pFlat->iContourCount; i++ ) {
		const xge_shape_ex_flat_contour_t* pContour = &pFlat->pContours[i];
		const xge_vec2_t* pSource;
		xge_vec2_t* pPoints;
		int iPointCount;
		int iSegmentCount;
		int j;

		if ( (pContour->iCount <= 0) || (pContour->iStart < 0) ||
		     ((pContour->iStart + pContour->iCount) > pFlat->iPointCount) ) continue;
		pSource = pFlat->pPoints + pContour->iStart;
		pPoints = NULL;
		iPointCount = 0;
		if ( __xgeShapeExCompactStrokePoints(pSource, pContour->iCount, pContour->bClosed, &pPoints, &iPointCount) != XGE_OK ) return 0;
		if ( iPointCount == 1 ) {
			int bHit = __xgeShapeExStrokePointIntersectsRect(pShape, pPoints[0], fHalfWidth, fTolerance, tRect);
			xrtFree(pPoints);
			if ( bHit ) return 1;
			continue;
		}
		if ( iPointCount < 2 ) {
			xrtFree(pPoints);
			continue;
		}
		iSegmentCount = pContour->bClosed ? iPointCount : (iPointCount - 1);
		for ( j = 0; j < iSegmentCount; j++ ) {
			if ( __xgeShapeExStrokeSegmentIntersectsRect(
				pShape, pPoints[j], pPoints[(j + 1) % iPointCount],
				!pContour->bClosed && (j == 0),
				!pContour->bClosed && (j == iSegmentCount - 1),
				fHalfWidth, fTolerance, tRect
			) ) {
				xrtFree(pPoints);
				return 1;
			}
		}
		if ( iSegmentCount > 1 ) {
			int iJoinStart = pContour->bClosed ? 0 : 1;
			int iJoinEnd = pContour->bClosed ? iPointCount : (iPointCount - 1);

			for ( j = iJoinStart; j < iJoinEnd; j++ ) {
				if ( __xgeShapeExStrokeJoinIntersectsRect(
					pShape, pPoints[(j + iPointCount - 1) % iPointCount],
					pPoints[j], pPoints[(j + 1) % iPointCount],
					fHalfWidth, fTolerance, tRect
				) ) {
					xrtFree(pPoints);
					return 1;
				}
			}
		}
		xrtFree(pPoints);
	}
	return 0;
}

static int __xgeShapeExFlatStrokeDashedIntersectsRect(
	xge_shape_ex pShape,
	const xge_shape_ex_flat_path_t* pFlat,
	float fStrokeWidth,
	const float* pDashPattern,
	int iDashCount,
	float fDashOffset,
	float fTolerance,
	xge_rect_t tRect
)
{
	float fDashTotal;
	float fDashMin;
	int i;

	if ( (pFlat == NULL) || (pDashPattern == NULL) || (iDashCount <= 0) ) {
		return __xgeShapeExFlatStrokeSolidIntersectsRect(pShape, pFlat, fStrokeWidth, fTolerance, tRect);
	}
	fDashTotal = __xgeShapeExDashTotal(pDashPattern, iDashCount);
	fDashMin = __xgeShapeExDashMinPositive(pDashPattern, iDashCount);
	if ( (fDashTotal <= XGE_SHAPE_EX_EPSILON) || (fDashMin <= XGE_SHAPE_EX_EPSILON) ) {
		return __xgeShapeExFlatStrokeSolidIntersectsRect(pShape, pFlat, fStrokeWidth, fTolerance, tRect);
	}
	for ( i = 0; i < pFlat->iContourCount; i++ ) {
		const xge_shape_ex_flat_contour_t* pContour = &pFlat->pContours[i];
		float fContourLength;
		float fDashRemaining;
		float fPosition;
		float fDashEpsilon;
		float fGuardEstimate;
		int iDashIndex;
		int bDashOn;
		int iGuard;
		int iGuardLimit;

		if ( (pContour->iCount < 2) || (pContour->iStart < 0) ||
		     ((pContour->iStart + pContour->iCount) > pFlat->iPointCount) ) continue;
		fContourLength = __xgeShapeExFlatContourLength(pFlat, pContour);
		iDashIndex = 0;
		bDashOn = 1;
		fDashRemaining = 0.0f;
		__xgeShapeExDashStateInit(pDashPattern, iDashCount, fDashOffset, &iDashIndex, &bDashOn, &fDashRemaining);
		if ( fContourLength <= XGE_SHAPE_EX_EPSILON ) {
			if ( bDashOn && (fDashRemaining > XGE_SHAPE_EX_EPSILON) &&
			     __xgeShapeExStrokePointIntersectsRect(pShape, pFlat->pPoints[pContour->iStart], fStrokeWidth * 0.5f, fTolerance, tRect) ) return 1;
			continue;
		}
		fPosition = 0.0f;
		fDashEpsilon = fmaxf(XGE_SHAPE_EX_EPSILON, fContourLength * 0.000001f);
		fGuardEstimate = (fContourLength / fDashMin) + (float)__xgeShapeExDashLogicalCount(iDashCount) + 16.0f;
		if ( fGuardEstimate > 2000000.0f ) return 0;
		iGuard = 0;
		iGuardLimit = (int)ceilf(fGuardEstimate);
		while ( fPosition < fContourLength - fDashEpsilon ) {
			float fStep = fDashRemaining;
			float fNext;

			if ( ++iGuard > iGuardLimit ) return 0;
			if ( fStep <= fDashEpsilon ) {
				__xgeShapeExDashStateNext(pDashPattern, iDashCount, &iDashIndex, &bDashOn, &fDashRemaining);
				continue;
			}
			fNext = fminf(fPosition + fStep, fContourLength);
			if ( bDashOn && (fNext > fPosition + fDashEpsilon) ) {
				xge_shape_ex_flat_path_t tDashFlat;
				int iRet;

				memset(&tDashFlat, 0, sizeof(tDashFlat));
				iRet = __xgeShapeExFlatAddTrimmedContourRange(pFlat, i, fPosition, fNext, &tDashFlat);
				if ( iRet != XGE_OK ) {
					__xgeShapeExFlatFree(&tDashFlat);
					return 0;
				}
				if ( __xgeShapeExFlatStrokeSolidIntersectsRect(pShape, &tDashFlat, fStrokeWidth, fTolerance, tRect) ) {
					__xgeShapeExFlatFree(&tDashFlat);
					return 1;
				}
				__xgeShapeExFlatFree(&tDashFlat);
			}
			fDashRemaining -= (fNext - fPosition);
			fPosition = fNext;
			if ( fDashRemaining <= fDashEpsilon ) {
				__xgeShapeExDashStateNext(pDashPattern, iDashCount, &iDashIndex, &bDashOn, &fDashRemaining);
			}
		}
	}
	return 0;
}

static int __xgeShapeExDrawStrokeUnionComposite(
	xge_shape_ex pShape,
	const xge_shape_ex_flat_path_t* pFlat,
	float fWidth,
	uint32_t iColor,
	xge_rect_t tBounds,
	xge_shape_ex_matrix_t tMatrix,
	const xge_shape_ex_matrix_t* pGeometryMatrix,
	float fOpacity,
	int bScreenSpace
);

static int __xgeShapeExDrawStrokeUnionMaskGeometry(
	xge_shape_ex pShape,
	const xge_shape_ex_flat_path_t* pFlat,
	float fWidth,
	uint32_t iColor,
	xge_rect_t tBounds,
	xge_shape_ex_matrix_t tMatrix,
	const xge_shape_ex_matrix_t* pGeometryMatrix,
	float fOpacity,
	int bScreenSpace
)
{
	int iOldBlend;
	int iRet;
	int i;

	if ( (pShape == NULL) || (pFlat == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	iRet = __xgeShapeAutoBatchFlush();
	if ( iRet != XGE_OK ) return iRet;
	iOldBlend = xgeBlendGet();
	xgeBlendSet(XGE_BLEND_LIGHTEN);
	for ( i = 0; i < pFlat->iContourCount; i++ ) {
		const xge_shape_ex_flat_contour_t* pContour = &pFlat->pContours[i];

		if ( pContour->iCount < 2 ) continue;
		iRet = __xgeShapeExDrawStrokeSegmentJoinContour(
			pShape, pFlat->pPoints + pContour->iStart, pContour->iCount, pContour->bClosed,
			fWidth, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace
		);
		if ( iRet != XGE_OK ) break;
	}
	{
		int iFlushRet = __xgeShapeAutoBatchFlush();

		if ( iRet == XGE_OK ) iRet = iFlushRet;
	}
	xgeBlendSet(iOldBlend);
	return iRet;
}

static int __xgeShapeExDrawStrokeSolid(xge_shape_ex pShape, const xge_shape_ex_flat_path_t* pFlat, float fWidth, uint32_t iColor, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, const xge_shape_ex_matrix_t* pGeometryMatrix, float fOpacity, int bScreenSpace)
{
	int i;

	if ( (pFlat == NULL) || (fWidth <= 0.0f) ) {
		return XGE_OK;
	}
	if ( (pShape != NULL) && __xgeShapeExFlatStrokeNeedsUnion(pFlat) ) {
		if ( pShape->bStrokeUnionMaskDraw ) {
			struct xge_shape_ex_t tUnionShape = *pShape;

			tUnionShape.bCoverageAntialiasDisabled = 0;
			return __xgeShapeExDrawStrokeUnionMaskGeometry(
				&tUnionShape, pFlat, fWidth, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace
			);
		}
		return __xgeShapeExDrawStrokeUnionComposite(
			pShape, pFlat, fWidth, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace
		);
	}
	for ( i = 0; i < pFlat->iContourCount; i++ ) {
		const xge_shape_ex_flat_contour_t* pContour = &pFlat->pContours[i];
		int iSegmentCount;
		int j;

		if ( pContour->iCount < 2 ) {
			continue;
		}
		iSegmentCount = pContour->bClosed ? pContour->iCount : (pContour->iCount - 1);
		if ( pShape != NULL ) {
			int iRet;
			int bExplicitJoin;

			if ( !pContour->bClosed ) {
				bExplicitJoin = 0;
			} else if ( pShape->iLineJoin == XGE_SHAPE_EX_JOIN_MITER ) {
				bExplicitJoin = __xgeShapeExStrokeContourMiterLimitExceeded(pFlat->pPoints + pContour->iStart, pContour->iCount, pContour->bClosed, pShape->fMiterLimit);
			} else {
				bExplicitJoin = __xgeShapeExStrokeContourWantsExplicitJoins(pFlat->pPoints + pContour->iStart, pContour->iCount, pContour->bClosed);
			}
			if ( bExplicitJoin && !pContour->bClosed ) {
				iRet = __xgeShapeExDrawStrokeSegmentJoinContour(pShape, pFlat->pPoints + pContour->iStart, pContour->iCount, pContour->bClosed, fWidth, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
			} else {
				iRet = __xgeShapeExDrawStrokeContourMesh(pShape, pFlat->pPoints + pContour->iStart, pContour->iCount, pContour->bClosed, fWidth, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
			}
			if ( iRet != XGE_OK ) {
				return iRet;
			}
			continue;
		}
		for ( j = 0; j < iSegmentCount; j++ ) {
			xge_vec2_t tA = pFlat->pPoints[pContour->iStart + j];
			xge_vec2_t tB = pFlat->pPoints[pContour->iStart + ((j + 1) % pContour->iCount)];
			int bCapStart = (!pContour->bClosed && (j == 0));
			int bCapEnd = (!pContour->bClosed && (j == (iSegmentCount - 1)));
			int iRet;

			iRet = __xgeShapeExDrawStrokeSegment(pShape, tA, tB, fWidth, iColor, bCapStart, bCapEnd, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
			if ( iRet != XGE_OK ) {
				return iRet;
			}
		}
	}
	return XGE_OK;
}

static int __xgeShapeExDrawStrokeDashed(xge_shape_ex pShape, const xge_shape_ex_flat_path_t* pFlat, float fWidth, uint32_t iColor, const float* pDashPattern, int iDashCount, float fDashOffset, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, const xge_shape_ex_matrix_t* pGeometryMatrix, float fOpacity, int bScreenSpace)
{
	float fDashTotal;
	float fDashMin;
	int i;

	if ( (pFlat == NULL) || (pDashPattern == NULL) || (iDashCount <= 0) ) {
		return __xgeShapeExDrawStrokeSolid(pShape, pFlat, fWidth, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
	}
	fDashTotal = __xgeShapeExDashTotal(pDashPattern, iDashCount);
	if ( fDashTotal <= XGE_SHAPE_EX_EPSILON ) {
		return __xgeShapeExDrawStrokeSolid(pShape, pFlat, fWidth, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
	}
	fDashMin = __xgeShapeExDashMinPositive(pDashPattern, iDashCount);
	if ( fDashMin <= XGE_SHAPE_EX_EPSILON ) {
		return __xgeShapeExDrawStrokeSolid(pShape, pFlat, fWidth, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
	}
	for ( i = 0; i < pFlat->iContourCount; i++ ) {
		const xge_shape_ex_flat_contour_t* pContour = &pFlat->pContours[i];
		float fContourLength;
		float fDashRemaining = 0.0f;
		float fPos;
		float fDashEpsilon;
		float fGuardEstimate;
		int iGuard;
		int iGuardLimit;
		int iDashIndex = 0;
		int bDashOn = 1;

		if ( (pContour->iCount < 2) || (pContour->iStart < 0) || ((pContour->iStart + pContour->iCount) > pFlat->iPointCount) ) {
			continue;
		}
		fContourLength = __xgeShapeExFlatContourLength(pFlat, pContour);
		if ( fContourLength <= XGE_SHAPE_EX_EPSILON ) {
			__xgeShapeExDashStateInit(pDashPattern, iDashCount, fDashOffset, &iDashIndex, &bDashOn, &fDashRemaining);
			if ( bDashOn && (fDashRemaining > XGE_SHAPE_EX_EPSILON) ) {
				int iRet = __xgeShapeExDrawStrokeZeroLengthCap(pShape, pFlat->pPoints[pContour->iStart], fWidth, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
				if ( iRet != XGE_OK ) {
					return iRet;
				}
			}
			continue;
		}
		__xgeShapeExDashStateInit(pDashPattern, iDashCount, fDashOffset, &iDashIndex, &bDashOn, &fDashRemaining);
		fPos = 0.0f;
		fDashEpsilon = fmaxf(XGE_SHAPE_EX_EPSILON, fContourLength * 0.000001f);
		fGuardEstimate = (fContourLength / fDashMin) + (float)__xgeShapeExDashLogicalCount(iDashCount) + 16.0f;
		if ( fGuardEstimate > 2000000.0f ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		iGuard = 0;
		iGuardLimit = (int)ceilf(fGuardEstimate);
		while ( fPos < fContourLength - fDashEpsilon ) {
			float fStep = fDashRemaining;
			float fNext;

			if ( ++iGuard > iGuardLimit ) {
				return XGE_ERROR_INVALID_ARGUMENT;
			}
			if ( fStep <= fDashEpsilon ) {
				__xgeShapeExDashStateNext(pDashPattern, iDashCount, &iDashIndex, &bDashOn, &fDashRemaining);
				continue;
			}
			fNext = fPos + fStep;
			if ( fNext <= fPos + fDashEpsilon ) {
				fDashRemaining = 0.0f;
				__xgeShapeExDashStateNext(pDashPattern, iDashCount, &iDashIndex, &bDashOn, &fDashRemaining);
				continue;
			}
			if ( fNext > fContourLength ) {
				fNext = fContourLength;
			}
			if ( bDashOn && (fNext > fPos + fDashEpsilon) ) {
				xge_shape_ex_flat_path_t tDashFlat;
				int iRet;

				memset(&tDashFlat, 0, sizeof(tDashFlat));
				iRet = __xgeShapeExFlatAddTrimmedContourRange(pFlat, i, fPos, fNext, &tDashFlat);
				if ( iRet != XGE_OK ) {
					__xgeShapeExFlatFree(&tDashFlat);
					return iRet;
				}
				iRet = __xgeShapeExDrawStrokeSolid(pShape, &tDashFlat, fWidth, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
				__xgeShapeExFlatFree(&tDashFlat);
				if ( iRet != XGE_OK ) {
					return iRet;
				}
			}
			fDashRemaining -= (fNext - fPos);
			fPos = fNext;
			if ( fDashRemaining <= fDashEpsilon ) {
				__xgeShapeExDashStateNext(pDashPattern, iDashCount, &iDashIndex, &bDashOn, &fDashRemaining);
			}
		}
	}
	return XGE_OK;
}

static int __xgeShapeExDrawStroke(xge_shape_ex pShape, xge_shape_ex pPathShape, const xge_shape_ex_flat_path_t* pFlat, float fWidth, uint32_t iColor, const float* pDashPattern, int iDashCount, float fDashOffset, xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, const xge_shape_ex_matrix_t* pGeometryMatrix, float fOpacity, int bScreenSpace)
{
	if ( (pFlat == NULL) || (fWidth <= 0.0f) || !__xgeShapeExStrokeVisible(pShape, iColor) ) {
		return XGE_OK;
	}
	if ( __xgeShapeExDashTotal(pDashPattern, iDashCount) > XGE_SHAPE_EX_EPSILON ) {
		if ( (pPathShape != NULL) && bScreenSpace && !pShape->bCoverageAntialiasDisabled &&
		     (pShape->iStrokeType == XGE_SHAPE_EX_FILL_SOLID) ) {
			return __xgeShapeExDrawStrokeDashPathCoverage(
				pShape, pPathShape, fWidth, iColor, pDashPattern, iDashCount, fDashOffset,
				tBounds, tMatrix, pGeometryMatrix, fOpacity
			);
		}
		return __xgeShapeExDrawStrokeDashed(pShape, pFlat, fWidth, iColor, pDashPattern, iDashCount, fDashOffset, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
	}
	if ( (pPathShape != NULL) && bScreenSpace && !pShape->bCoverageAntialiasDisabled &&
	     (pShape->iStrokeType == XGE_SHAPE_EX_FILL_SOLID) ) {
		return __xgeShapeExDrawStrokePathCoverage(
			pShape, pPathShape, fWidth, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity
		);
	}
	return __xgeShapeExDrawStrokeSolid(pShape, pFlat, fWidth, iColor, tBounds, tMatrix, pGeometryMatrix, fOpacity, bScreenSpace);
}

static int __xgeShapeExStencilAvailable(void)
{
	return (glEnable != NULL) &&
		(glDisable != NULL) &&
		(glClear != NULL) &&
		(glClearStencil != NULL) &&
		(glStencilFunc != NULL) &&
		(glStencilOp != NULL) &&
		(glStencilMask != NULL) &&
		(glColorMask != NULL);
}

#define XGE_SHAPE_EX_STENCIL_BIT_COUNT 8
#define XGE_SHAPE_EX_STENCIL_CONTEXT_MAX 8
#ifndef GL_ALWAYS
#define GL_ALWAYS 0x0207
#endif
#ifndef GL_ZERO
#define GL_ZERO 0
#endif

/* Each active clip context retains one result bit; higher bits are recursion scratch space. */
static uint32_t g_xgeShapeExStencilActiveMask = 0;
static uint32_t g_xgeShapeExStencilContextMasks[XGE_SHAPE_EX_STENCIL_CONTEXT_MAX];
static int g_xgeShapeExStencilContextCount = 0;

static int __xgeShapeExDrawStencilViewportPass(void)
{
	xge_rect_t tViewport;

	tViewport = xgeViewportGet();
	if ( (tViewport.fW <= 0.0f) || (tViewport.fH <= 0.0f) ) {
		return XGE_OK;
	}
	xgeShapeRectFillPx(tViewport, XGE_COLOR_RGBA(255, 255, 255, 255));
	return __xgeShapeAutoBatchFlush();
}

static uint32_t __xgeShapeExStencilBit(int iBitIndex)
{
	if ( (iBitIndex < 0) || (iBitIndex >= XGE_SHAPE_EX_STENCIL_BIT_COUNT) ) {
		return 0;
	}
	return (uint32_t)1u << (uint32_t)iBitIndex;
}

static int __xgeShapeExStencilClearBit(uint32_t iBit)
{
	if ( iBit == 0 ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	glStencilMask(iBit);
	glStencilFunc(GL_ALWAYS, 0, iBit);
	glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
	return __xgeShapeExDrawStencilViewportPass();
}

static int __xgeShapeExStencilSetBitViewport(uint32_t iBit)
{
	if ( iBit == 0 ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	glStencilMask(iBit);
	glStencilFunc(GL_ALWAYS, (GLint)iBit, iBit);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	return __xgeShapeExDrawStencilViewportPass();
}

static int __xgeShapeExStencilOrBitViewport(uint32_t iDestinationBit, uint32_t iSourceBit)
{
	if ( (iDestinationBit == 0) || (iSourceBit == 0) || (iDestinationBit == iSourceBit) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	glStencilMask(iDestinationBit);
	glStencilFunc(GL_EQUAL, (GLint)(iSourceBit | iDestinationBit), iSourceBit);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	return __xgeShapeExDrawStencilViewportPass();
}

static int __xgeShapeExStencilSubtractBitViewport(uint32_t iDestinationBit, uint32_t iSourceBit)
{
	if ( (iDestinationBit == 0) || (iSourceBit == 0) || (iDestinationBit == iSourceBit) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	glStencilMask(iDestinationBit);
	glStencilFunc(GL_EQUAL, (GLint)(iDestinationBit | iSourceBit), iDestinationBit | iSourceBit);
	glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
	return __xgeShapeExDrawStencilViewportPass();
}

static int __xgeShapeExDrawStencilClipShapeGeometry(xge_shape_ex pClipShape, float fTolerance, int bScreenSpace, xge_shape_ex_matrix_t tParent)
{
	xge_shape_ex_flat_path_t tFlat;
	xge_shape_ex_stroke_path_t tTrimPath;
	xge_shape_ex_t tTrimShape;
	xge_shape_ex pDrawShape;
	xge_shape_ex_matrix_t tMatrix;
	struct xge_shape_ex_t tStencilShape;
	uint32_t iFillColor;
	uint32_t iStrokeColor;
	xge_rect_t tOldClip;
	xge_rect_t tStrokeBounds;
	float fStrokeScale;
	float fStrokeWidth;
	float fDashOffset;
	float* pDashPattern;
	int bOldClip;
	int bClipApplied;
	int iRet;

	memset(&tFlat, 0, sizeof(tFlat));
	memset(&tTrimPath, 0, sizeof(tTrimPath));
	memset(&tTrimShape, 0, sizeof(tTrimShape));
	memset(&tOldClip, 0, sizeof(tOldClip));
	bOldClip = 0;
	bClipApplied = 0;
	if ( !__xgeShapeExValid(pClipShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( !pClipShape->bVisible ) {
		return XGE_OK;
	}
	tMatrix = __xgeShapeExMatrixMul(tParent, pClipShape->tTransform);
	if ( pClipShape->bClipRect ) {
		xge_rect_t tClip = __xgeShapeExMatrixRectBounds(tMatrix, pClipShape->tClipRect);

		if ( (tClip.fW <= 0.0f) || (tClip.fH <= 0.0f) ) {
			return XGE_OK;
		}
		iRet = __xgeShapeAutoBatchFlush();
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		tOldClip = xgeClipGet();
		bOldClip = (tOldClip.fW > 0.0f) && (tOldClip.fH > 0.0f);
		if ( bOldClip ) {
			tClip = __xgeShapeExRectIntersect(tOldClip, tClip);
		}
		if ( (tClip.fW <= 0.0f) || (tClip.fH <= 0.0f) ) {
			return XGE_OK;
		}
		tClip = __xgeShapeExPixelCenterClipRect(tClip);
		xgeClipSet(tClip);
		bClipApplied = 1;
	}
	pDrawShape = pClipShape;
	if ( pClipShape->bTrimPath ) {
		iRet = __xgeShapeExTrimPathBuild(
			&tTrimPath, pClipShape->pCommands, pClipShape->iCommandCount,
			pClipShape->pPoints, pClipShape->iPointCount,
			pClipShape->fTrimBegin, pClipShape->fTrimEnd,
			pClipShape->bTrimSimultaneous
		);
		if ( iRet != XGE_OK ) {
			if ( bClipApplied ) {
				if ( bOldClip ) xgeClipSet(tOldClip);
				else xgeClipClear();
			}
			return iRet;
		}
		__xgeShapeExCommandPathShapeView(pClipShape, &tTrimPath, &tTrimShape);
		pDrawShape = &tTrimShape;
	}
	iRet = __xgeShapeExFlatten(pDrawShape, tMatrix, fTolerance, &tFlat);
	if ( iRet != XGE_OK ) {
		__xgeShapeExStrokePathFree(&tTrimPath);
		if ( bClipApplied ) {
			if ( bOldClip ) xgeClipSet(tOldClip);
			else xgeClipClear();
		}
		return iRet;
	}
	tStencilShape = *pClipShape;
	if ( pClipShape->bStencilPaint ) {
		iFillColor = pClipShape->iFillColor;
		iStrokeColor = pClipShape->iStrokeColor;
		tStrokeBounds = __xgeShapeExFlatBounds(&tFlat);
		fStrokeScale = pClipShape->bStrokeNonScaling ? 1.0f : __xgeShapeExMatrixStrokeScale(tMatrix);
		fStrokeWidth = pClipShape->fStrokeWidth * fStrokeScale;
		fDashOffset = pClipShape->fDashOffset * fStrokeScale;
		pDashPattern = pClipShape->pDashPattern;
		if ( (pClipShape->pDashPattern != NULL) && (pClipShape->iDashCount > 0) && (fabsf(fStrokeScale - 1.0f) > XGE_SHAPE_EX_EPSILON) ) {
			int i;

			pDashPattern = (float*)xrtMalloc((size_t)pClipShape->iDashCount * sizeof(*pDashPattern));
			if ( pDashPattern == NULL ) {
				__xgeShapeExStrokePathFree(&tTrimPath);
				__xgeShapeExFlatFree(&tFlat);
				if ( bClipApplied ) {
					if ( bOldClip ) xgeClipSet(tOldClip);
					else xgeClipClear();
				}
				return XGE_ERROR_OUT_OF_MEMORY;
			}
			for ( i = 0; i < pClipShape->iDashCount; i++ ) {
				pDashPattern[i] = pClipShape->pDashPattern[i] * fStrokeScale;
			}
		}
		if ( pClipShape->bStrokeFirst ) {
			iRet = __xgeShapeExDrawStroke(&tStencilShape, pDrawShape, &tFlat, fStrokeWidth, iStrokeColor, pDashPattern, pClipShape->iDashCount, fDashOffset, tStrokeBounds, tMatrix, NULL, 1.0f, bScreenSpace);
			if ( iRet == XGE_OK ) {
				iRet = __xgeShapeExDrawFill(&tStencilShape, &tFlat, pClipShape->iFillRule, tMatrix, 1.0f, iFillColor, bScreenSpace, 0, 0, 0);
			}
		} else {
			iRet = __xgeShapeExDrawFill(&tStencilShape, &tFlat, pClipShape->iFillRule, tMatrix, 1.0f, iFillColor, bScreenSpace, 0, 0, 0);
			if ( iRet == XGE_OK ) {
				iRet = __xgeShapeExDrawStroke(&tStencilShape, pDrawShape, &tFlat, fStrokeWidth, iStrokeColor, pDashPattern, pClipShape->iDashCount, fDashOffset, tStrokeBounds, tMatrix, NULL, 1.0f, bScreenSpace);
			}
		}
		if ( pDashPattern != pClipShape->pDashPattern ) {
			xrtFree(pDashPattern);
		}
	} else {
		tStencilShape.iFillType = XGE_SHAPE_EX_FILL_SOLID;
		tStencilShape.iFillColor = XGE_COLOR_RGBA(255, 255, 255, 255);
		tStencilShape.pFillStops = NULL;
		tStencilShape.iFillStopCount = 0;
		tStencilShape.iFillStopCapacity = 0;
		iRet = __xgeShapeExDrawFill(&tStencilShape, &tFlat, pClipShape->iFillRule, tMatrix, 1.0f, XGE_COLOR_RGBA(255, 255, 255, 255), bScreenSpace, 0, 0, 0);
	}
	__xgeShapeExStrokePathFree(&tTrimPath);
	__xgeShapeExFlatFree(&tFlat);
	if ( bClipApplied ) {
		int iFlushRet = __xgeShapeAutoBatchFlush();

		if ( iRet == XGE_OK ) {
			iRet = iFlushRet;
		}
		if ( bOldClip ) xgeClipSet(tOldClip);
		else xgeClipClear();
	}
	return iRet;
}

static int __xgeShapeExStencilWriteGeometryBit(xge_shape_ex pClipShape, float fTolerance, int bScreenSpace, xge_shape_ex_matrix_t tParent, uint32_t iDestinationBit, uint32_t iConditionBit)
{
	int iRet;

	if ( (iDestinationBit == 0) || (iDestinationBit == iConditionBit) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	glStencilMask(iDestinationBit);
	if ( iConditionBit != 0 ) {
		glStencilFunc(GL_EQUAL, (GLint)(iConditionBit | iDestinationBit), iConditionBit);
	} else {
		glStencilFunc(GL_ALWAYS, (GLint)iDestinationBit, iDestinationBit);
	}
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	iRet = __xgeShapeExDrawStencilClipShapeGeometry(pClipShape, fTolerance, bScreenSpace, tParent);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	return __xgeShapeAutoBatchFlush();
}

static int __xgeShapeExEvaluateStencilClipShape(xge_shape_ex pClipShape, float fTolerance, int bScreenSpace, xge_shape_ex_matrix_t tParent, int iBitIndex)
{
	xge_shape_ex_matrix_t tChildParent;
	uint32_t iDestinationBit;
	uint32_t iChildBit;
	int iIncludeCount;
	int iRet;
	int i;

	if ( !__xgeShapeExValid(pClipShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iDestinationBit = __xgeShapeExStencilBit(iBitIndex);
	iChildBit = __xgeShapeExStencilBit(iBitIndex + 1);
	if ( iDestinationBit == 0 ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	iRet = __xgeShapeExStencilClearBit(iDestinationBit);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iIncludeCount = 0;
	for ( i = 0; i < pClipShape->iClipShapeCount; i++ ) {
		int iMode = pClipShape->pClipShapeModes != NULL ? pClipShape->pClipShapeModes[i] : XGE_SHAPE_EX_CLIP_INTERSECT;

		if ( iMode != XGE_SHAPE_EX_CLIP_SUBTRACT ) {
			iIncludeCount++;
		}
	}
	tChildParent = __xgeShapeExMatrixMul(tParent, pClipShape->tTransform);
	if ( iIncludeCount <= 0 ) {
		iRet = __xgeShapeExStencilWriteGeometryBit(pClipShape, fTolerance, bScreenSpace, tParent, iDestinationBit, 0);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	} else {
		if ( iChildBit == 0 ) {
			return XGE_ERROR_UNSUPPORTED;
		}
		for ( i = 0; i < pClipShape->iClipShapeCount; i++ ) {
			int iMode = pClipShape->pClipShapeModes != NULL ? pClipShape->pClipShapeModes[i] : XGE_SHAPE_EX_CLIP_INTERSECT;

			if ( iMode == XGE_SHAPE_EX_CLIP_SUBTRACT ) {
				continue;
			}
			iRet = __xgeShapeExEvaluateStencilClipShape(pClipShape->pClipShapes[i], fTolerance, bScreenSpace, tChildParent, iBitIndex + 1);
			if ( iRet != XGE_OK ) {
				return iRet;
			}
			iRet = __xgeShapeExStencilWriteGeometryBit(pClipShape, fTolerance, bScreenSpace, tParent, iDestinationBit, iChildBit);
			if ( iRet != XGE_OK ) {
				return iRet;
			}
		}
	}
	for ( i = 0; i < pClipShape->iClipShapeCount; i++ ) {
		int iMode = pClipShape->pClipShapeModes != NULL ? pClipShape->pClipShapeModes[i] : XGE_SHAPE_EX_CLIP_INTERSECT;

		if ( iMode != XGE_SHAPE_EX_CLIP_SUBTRACT ) {
			continue;
		}
		if ( iChildBit == 0 ) {
			return XGE_ERROR_UNSUPPORTED;
		}
		iRet = __xgeShapeExEvaluateStencilClipShape(pClipShape->pClipShapes[i], fTolerance, bScreenSpace, tChildParent, iBitIndex + 1);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		iRet = __xgeShapeExStencilSubtractBitViewport(iDestinationBit, iChildBit);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	if ( iChildBit != 0 ) {
		iRet = __xgeShapeExStencilClearBit(iChildBit);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	return XGE_OK;
}

static int __xgeShapeExEndStencilClip(int bStencilApplied, int iRet)
{
	if ( bStencilApplied ) {
		int iFlushRet = __xgeShapeAutoBatchFlush();
		uint32_t iPreviousMask;

		if ( iRet == XGE_OK ) {
			iRet = iFlushRet;
		}
		if ( g_xgeShapeExStencilContextCount > 0 ) {
			g_xgeShapeExStencilContextCount--;
			iPreviousMask = g_xgeShapeExStencilContextMasks[g_xgeShapeExStencilContextCount];
		} else {
			iPreviousMask = 0;
		}
		g_xgeShapeExStencilActiveMask = iPreviousMask;
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		if ( iPreviousMask != 0 ) {
			glStencilMask(0x00u);
			glStencilFunc(GL_EQUAL, (GLint)iPreviousMask, iPreviousMask);
			glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		} else {
			glStencilMask(0xFFu);
			glDisable(GL_STENCIL_TEST);
		}
	}
	return iRet;
}

static int __xgeShapeExPrepareStencilContext(int* pBitIndex, uint32_t* pPreviousMask)
{
	int iRet;

	if ( (pBitIndex == NULL) || (pPreviousMask == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( g_xgeShapeExStencilContextCount >= XGE_SHAPE_EX_STENCIL_CONTEXT_MAX ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	iRet = __xgeShapeAutoBatchFlush();
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	*pBitIndex = g_xgeShapeExStencilContextCount;
	*pPreviousMask = g_xgeShapeExStencilActiveMask;
	glEnable(GL_STENCIL_TEST);
	if ( *pPreviousMask == 0 ) {
		glStencilMask(0xFFu);
		glClearStencil(0);
		glClear(GL_STENCIL_BUFFER_BIT);
	}
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	return XGE_OK;
}

static int __xgeShapeExAbortStencilContext(uint32_t iPreviousMask, int iRet)
{
	int iFlushRet = __xgeShapeAutoBatchFlush();

	if ( iRet == XGE_OK ) {
		iRet = iFlushRet;
	}
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	if ( iPreviousMask != 0 ) {
		glStencilMask(0x00u);
		glStencilFunc(GL_EQUAL, (GLint)iPreviousMask, iPreviousMask);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	} else {
		glStencilMask(0xFFu);
		glDisable(GL_STENCIL_TEST);
	}
	return iRet;
}

static int __xgeShapeExCommitStencilContext(int iBitIndex, uint32_t iPreviousMask)
{
	uint32_t iResultBit;
	int iRet;

	iResultBit = __xgeShapeExStencilBit(iBitIndex);
	if ( (iResultBit == 0) || (g_xgeShapeExStencilContextCount >= XGE_SHAPE_EX_STENCIL_CONTEXT_MAX) ) {
		return __xgeShapeExAbortStencilContext(iPreviousMask, XGE_ERROR_UNSUPPORTED);
	}
	iRet = __xgeShapeAutoBatchFlush();
	if ( iRet != XGE_OK ) {
		return __xgeShapeExAbortStencilContext(iPreviousMask, iRet);
	}
	g_xgeShapeExStencilContextMasks[g_xgeShapeExStencilContextCount] = iPreviousMask;
	g_xgeShapeExStencilContextCount++;
	g_xgeShapeExStencilActiveMask = iPreviousMask | iResultBit;
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glStencilMask(0x00u);
	glStencilFunc(GL_EQUAL, (GLint)g_xgeShapeExStencilActiveMask, g_xgeShapeExStencilActiveMask);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	return XGE_OK;
}

static int __xgeShapeExBeginStencilClipList(const xge_shape_ex* pClipShapes, const int* pClipShapeModes, int iClipShapeCount, float fTolerance, int bScreenSpace, xge_shape_ex_matrix_t tMatrix)
{
	uint32_t iPreviousMask;
	uint32_t iResultBit;
	uint32_t iChildBit;
	int iBitIndex;
	int iFirstInclude;
	int i;
	int iRet;
	int iIncludeCount;

	if ( (pClipShapes == NULL) || (iClipShapeCount <= 0) ) {
		return XGE_OK;
	}
	if ( !__xgeShapeExStencilAvailable() ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	iRet = __xgeShapeExPrepareStencilContext(&iBitIndex, &iPreviousMask);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iResultBit = __xgeShapeExStencilBit(iBitIndex);
	iChildBit = __xgeShapeExStencilBit(iBitIndex + 1);
	if ( iResultBit == 0 ) {
		return __xgeShapeExAbortStencilContext(iPreviousMask, XGE_ERROR_UNSUPPORTED);
	}
	iIncludeCount = 0;
	iFirstInclude = -1;
	for ( i = 0; i < iClipShapeCount; i++ ) {
		int iMode = pClipShapeModes != NULL ? pClipShapeModes[i] : XGE_SHAPE_EX_CLIP_INTERSECT;

		if ( iMode != XGE_SHAPE_EX_CLIP_SUBTRACT ) {
			if ( iFirstInclude < 0 ) iFirstInclude = i;
			iIncludeCount++;
		}
	}
	if ( iIncludeCount <= 0 ) {
		iRet = __xgeShapeExStencilSetBitViewport(iResultBit);
		if ( iRet != XGE_OK ) {
			return __xgeShapeExAbortStencilContext(iPreviousMask, iRet);
		}
	} else {
		iRet = __xgeShapeExEvaluateStencilClipShape(pClipShapes[iFirstInclude], fTolerance, bScreenSpace, tMatrix, iBitIndex);
		if ( iRet != XGE_OK ) {
			return __xgeShapeExAbortStencilContext(iPreviousMask, iRet);
		}
	}
	for ( i = 0; i < iClipShapeCount; i++ ) {
		int iMode = pClipShapeModes != NULL ? pClipShapeModes[i] : XGE_SHAPE_EX_CLIP_INTERSECT;

		if ( (iMode == XGE_SHAPE_EX_CLIP_SUBTRACT) || (i == iFirstInclude) ) {
			continue;
		}
		if ( iChildBit == 0 ) {
			return __xgeShapeExAbortStencilContext(iPreviousMask, XGE_ERROR_UNSUPPORTED);
		}
		iRet = __xgeShapeExEvaluateStencilClipShape(pClipShapes[i], fTolerance, bScreenSpace, tMatrix, iBitIndex + 1);
		if ( iRet != XGE_OK ) {
			return __xgeShapeExAbortStencilContext(iPreviousMask, iRet);
		}
		iRet = __xgeShapeExStencilOrBitViewport(iResultBit, iChildBit);
		if ( iRet != XGE_OK ) {
			return __xgeShapeExAbortStencilContext(iPreviousMask, iRet);
		}
	}
	for ( i = 0; i < iClipShapeCount; i++ ) {
		int iMode = pClipShapeModes != NULL ? pClipShapeModes[i] : XGE_SHAPE_EX_CLIP_INTERSECT;

		if ( iMode != XGE_SHAPE_EX_CLIP_SUBTRACT ) {
			continue;
		}
		if ( iChildBit == 0 ) {
			return __xgeShapeExAbortStencilContext(iPreviousMask, XGE_ERROR_UNSUPPORTED);
		}
		iRet = __xgeShapeExEvaluateStencilClipShape(pClipShapes[i], fTolerance, bScreenSpace, tMatrix, iBitIndex + 1);
		if ( iRet != XGE_OK ) {
			return __xgeShapeExAbortStencilContext(iPreviousMask, iRet);
		}
		iRet = __xgeShapeExStencilSubtractBitViewport(iResultBit, iChildBit);
		if ( iRet != XGE_OK ) {
			return __xgeShapeExAbortStencilContext(iPreviousMask, iRet);
		}
	}
	if ( iChildBit != 0 ) {
		iRet = __xgeShapeExStencilClearBit(iChildBit);
		if ( iRet != XGE_OK ) {
			return __xgeShapeExAbortStencilContext(iPreviousMask, iRet);
		}
	}
	return __xgeShapeExCommitStencilContext(iBitIndex, iPreviousMask);
}

static int __xgeShapeExBeginStencilClip(xge_shape_ex pShape, float fTolerance, int bScreenSpace, xge_shape_ex_matrix_t tMatrix)
{
	if ( pShape == NULL ) {
		return XGE_OK;
	}
	return __xgeShapeExBeginStencilClipList(pShape->pClipShapes, pShape->pClipShapeModes, pShape->iClipShapeCount, fTolerance, bScreenSpace, tMatrix);
}

static int __xgeShapeExStencilClipBeginPublic(xge_shape_ex pClipShape, float fTolerance, int bScreenSpace, const xge_shape_ex_matrix_t* pParentMatrix, int* pApplied)
{
	xge_shape_ex_matrix_t tParent;
	uint32_t iPreviousMask;
	int iBitIndex;
	int iRet;

	if ( pApplied == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pApplied = 0;
	if ( pClipShape == NULL ) {
		return XGE_OK;
	}
	if ( !__xgeShapeExValid(pClipShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( !__xgeShapeExStencilAvailable() ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	tParent = pParentMatrix != NULL ? *pParentMatrix : __xgeShapeExMatrixIdentity();
	iRet = __xgeShapeExPrepareStencilContext(&iBitIndex, &iPreviousMask);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = __xgeShapeExEvaluateStencilClipShape(pClipShape, fTolerance, bScreenSpace, tParent, iBitIndex);
	if ( iRet != XGE_OK ) {
		return __xgeShapeExAbortStencilContext(iPreviousMask, iRet);
	}
	iRet = __xgeShapeExCommitStencilContext(iBitIndex, iPreviousMask);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	*pApplied = 1;
	return XGE_OK;
}

static int __xgeShapeExFinishClip(int bClipApplied, int bOldClip, xge_rect_t tOldClip, int iRet)
{
	if ( bClipApplied ) {
		int iFlushRet = __xgeShapeAutoBatchFlush();

		if ( iRet == XGE_OK ) {
			iRet = iFlushRet;
		}
		if ( bOldClip ) xgeClipSet(tOldClip);
		else xgeClipClear();
	}
	return iRet;
}

static int __xgeShapeExFinishShapeClips(int bClipApplied, int bOldClip, xge_rect_t tOldClip, int bStencilApplied, int iRet)
{
	iRet = __xgeShapeExEndStencilClip(bStencilApplied, iRet);
	return __xgeShapeExFinishClip(bClipApplied, bOldClip, tOldClip, iRet);
}

static int __xgeShapeExBlendBegin(int bBlendSet, int iBlend, int* pOldBlend, int* pBlendApplied)
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
	if ( !__xgeShapeExBlendValid(iBlend) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
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

static int __xgeShapeExBlendEnd(int iOldBlend, int bBlendApplied, int iRet)
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

static int __xgeShapeExDrawDispatch(xge_shape_ex pShape, float fTolerance, int bScreenSpace, xge_shape_ex_matrix_t tParent, float fParentOpacity, int bSuppressOwnBlend);
static int __xgeShapeExSceneDrawDispatch(xge_shape_ex_scene pScene, float fTolerance, int bScreenSpace, xge_shape_ex_matrix_t tParent, float fParentOpacity, int bSuppressOwnBlend);

static int __xgeShapeExTrailingOpaqueStrokeCoversFill(
	xge_shape_ex pShape,
	float fStrokeWidth,
	uint32_t iStrokeColor
)
{
	return __xgeShapeExStrokeVisible(pShape, iStrokeColor) &&
		!pShape->bStrokeFirst && (fStrokeWidth >= 2.0f) &&
		(__xgeShapeExDashTotal(pShape->pDashPattern, pShape->iDashCount) <= XGE_SHAPE_EX_EPSILON) &&
		!pShape->bTrimPath && (pShape->iStrokeType == XGE_SHAPE_EX_FILL_SOLID) &&
		((pShape->iStrokeColor & 0xFFu) == 0xFFu);
}

static int __xgeShapeExDrawGradientComposite(
	xge_shape_ex pShape,
	int iPaint,
	float fTolerance,
	int bScreenSpace,
	xge_shape_ex_matrix_t tParent,
	float fOpacity,
	xge_rect_t tPaintBounds,
	xge_shape_ex_matrix_t tPaintMatrix,
	const xge_shape_ex_matrix_t* pGeometryMatrix
);

static int __xgeShapeExDrawInternal(xge_shape_ex pShape, float fTolerance, int bScreenSpace, xge_shape_ex_matrix_t tParent, float fParentOpacity, int bSuppressOwnBlend)
{
	xge_shape_ex_flat_path_t tFlat;
	xge_shape_ex_flat_path_t tStrokeFlat;
	xge_shape_ex_stroke_path_t tTrimPath;
	xge_shape_ex_t tTrimShape;
	const xge_shape_ex_flat_path_t* pDrawFlat;
	const xge_shape_ex_flat_path_t* pStrokeFlat;
	xge_shape_ex pDrawShape;
	xge_shape_ex pStrokeShape;
	xge_shape_ex_matrix_t tMatrix;
	xge_shape_ex_matrix_t tFillPaintMatrix;
	xge_shape_ex_matrix_t tStrokePaintMatrix;
	xge_shape_ex_matrix_t tIdentity;
	const xge_shape_ex_matrix_t* pFillGeometryMatrix;
	const xge_shape_ex_matrix_t* pStrokeGeometryMatrix;
	uint32_t iFillColor;
	uint32_t iStrokeColor;
	xge_rect_t tFillBounds;
	xge_rect_t tFillPaintBounds;
	xge_rect_t tStrokeBounds;
	float fOpacity;
	float fStrokeScale;
	float fStrokeWidth;
	float fDashOffset;
	float* pDashPattern;
	xge_rect_t tOldClip;
	int bOldClip;
	int bClipApplied;
	int bStencilApplied;
	int iOldBlend;
	int bBlendApplied;
	int bFillAntialias;
	int bFillBinaryCoverage;
	int i;
	int iRet;

	memset(&tFlat, 0, sizeof(tFlat));
	memset(&tStrokeFlat, 0, sizeof(tStrokeFlat));
	memset(&tTrimPath, 0, sizeof(tTrimPath));
	memset(&tTrimShape, 0, sizeof(tTrimShape));
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( !pShape->bVisible ) {
		return XGE_OK;
	}
	fOpacity = fParentOpacity * pShape->fOpacity;
	if ( fOpacity <= 0.0f ) {
		return XGE_OK;
	}
	tMatrix = __xgeShapeExMatrixMul(tParent, pShape->tTransform);
	tIdentity = __xgeShapeExMatrixIdentity();
	bClipApplied = 0;
	bStencilApplied = 0;
	iOldBlend = XGE_BLEND_ALPHA;
	bBlendApplied = 0;
	bOldClip = 0;
	memset(&tOldClip, 0, sizeof(tOldClip));
	if ( pShape->bClipRect ) {
		xge_rect_t tClip = __xgeShapeExMatrixRectBounds(tMatrix, pShape->tClipRect);

		iRet = __xgeShapeAutoBatchFlush();
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		tOldClip = xgeClipGet();
		bOldClip = (tOldClip.fW > 0.0f) && (tOldClip.fH > 0.0f);
		if ( bOldClip ) {
			tClip = __xgeShapeExRectIntersect(tOldClip, tClip);
		}
		tClip = __xgeShapeExPixelCenterClipRect(tClip);
		xgeClipSet(tClip);
		bClipApplied = 1;
	}
	if ( pShape->iClipShapeCount > 0 ) {
		iRet = __xgeShapeExBeginStencilClip(pShape, fTolerance, bScreenSpace, tMatrix);
		if ( iRet != XGE_OK ) {
			return __xgeShapeExFinishShapeClips(bClipApplied, bOldClip, tOldClip, 0, iRet);
		}
		bStencilApplied = 1;
	}
	pDrawShape = pShape;
	if ( pShape->bTrimPath ) {
		iRet = __xgeShapeExTrimPathBuild(
			&tTrimPath, pShape->pCommands, pShape->iCommandCount,
			pShape->pPoints, pShape->iPointCount, pShape->fTrimBegin,
			pShape->fTrimEnd, pShape->bTrimSimultaneous
		);
		if ( iRet != XGE_OK ) {
			return __xgeShapeExFinishShapeClips(
				bClipApplied, bOldClip, tOldClip, bStencilApplied, iRet
			);
		}
		__xgeShapeExCommandPathShapeView(pShape, &tTrimPath, &tTrimShape);
		pDrawShape = &tTrimShape;
	}
	iRet = __xgeShapeExFlatten(pDrawShape, tMatrix, fTolerance, &tFlat);
	if ( iRet != XGE_OK ) {
		__xgeShapeExStrokePathFree(&tTrimPath);
		return __xgeShapeExFinishShapeClips(bClipApplied, bOldClip, tOldClip, bStencilApplied, iRet);
	}
	pDrawFlat = &tFlat;
	tFillBounds = __xgeShapeExFlatBounds(pDrawFlat);
	tFillPaintBounds = tFillBounds;
	tFillPaintMatrix = tMatrix;
	pFillGeometryMatrix = NULL;
	if ( (pShape->iFillGradientUnits == XGE_SHAPE_EX_GRADIENT_OBJECT_BOUNDING_BOX) &&
	     ((pShape->iFillType == XGE_SHAPE_EX_FILL_LINEAR_GRADIENT) ||
	      (pShape->iFillType == XGE_SHAPE_EX_FILL_RADIAL_GRADIENT)) ) {
		xge_shape_ex_matrix_t tScreenToLocal;

		if ( __xgeShapeExMatrixInverse(tMatrix, &tScreenToLocal) ) {
			tFillPaintBounds = __xgeShapeExFlatMappedBounds(pDrawFlat, tScreenToLocal);
		}
		tFillPaintMatrix = tIdentity;
		pFillGeometryMatrix = &tMatrix;
	}
	iFillColor = __xgeShapeExColorOpacity(pShape->iFillColor, fOpacity);
	iStrokeColor = __xgeShapeExColorOpacity(pShape->iStrokeColor, fOpacity);
	fStrokeScale = pShape->bStrokeNonScaling ? 1.0f : __xgeShapeExMatrixStrokeScale(tMatrix);
	fStrokeWidth = pShape->fStrokeWidth * fStrokeScale;
	bFillAntialias = 1;
	bFillBinaryCoverage = 0;
	/* Match ThorVG: an opaque trailing stroke covers the fill edge, so its fill AA is unnecessary. */
	if ( pShape->bCoverageAntialiasDisabled ) {
		bFillAntialias = 0;
		bFillBinaryCoverage = 1;
	} else if ( __xgeShapeExTrailingOpaqueStrokeCoversFill(pShape, fStrokeWidth, iStrokeColor) ) {
		for ( i = 0; i < pDrawFlat->iContourCount; i++ ) {
			if ( !pDrawFlat->pContours[i].bClosed ) {
				bFillAntialias = 0;
				bFillBinaryCoverage = 1;
				break;
			}
		}
	}
	fDashOffset = pShape->fDashOffset * fStrokeScale;
	pDashPattern = pShape->pDashPattern;
	pStrokeShape = pDrawShape;
	pStrokeFlat = pDrawFlat;
	tStrokeBounds = __xgeShapeExFlatBounds(pStrokeFlat);
	tStrokePaintMatrix = tMatrix;
	pStrokeGeometryMatrix = NULL;
	if ( __xgeShapeExStrokeVisible(pShape, iStrokeColor) ) {
		xge_shape_ex_matrix_t tStrokeFlattenMatrix = tMatrix;
		float fStrokeTolerance = fTolerance > 0.0f ? fTolerance : XGE_SHAPE_EX_DEFAULT_TOLERANCE;

		if ( fStrokeTolerance < XGE_SHAPE_EX_STROKE_MIN_TOLERANCE ) {
			fStrokeTolerance = XGE_SHAPE_EX_STROKE_MIN_TOLERANCE;
		}
		if ( !pShape->bStrokeNonScaling ) {
			tStrokeFlattenMatrix = tIdentity;
			if ( fStrokeScale > XGE_SHAPE_EX_EPSILON ) {
				fStrokeTolerance /= fStrokeScale;
			}
		}
		iRet = __xgeShapeExFlattenStroke(pStrokeShape, tStrokeFlattenMatrix, fStrokeTolerance, &tStrokeFlat);
		if ( iRet != XGE_OK ) {
			if ( pDashPattern != pShape->pDashPattern ) {
				xrtFree(pDashPattern);
			}
			__xgeShapeExStrokePathFree(&tTrimPath);
			__xgeShapeExFlatFree(&tFlat);
			return __xgeShapeExFinishShapeClips(bClipApplied, bOldClip, tOldClip, bStencilApplied, iRet);
		}
		pStrokeFlat = &tStrokeFlat;
		tStrokeBounds = __xgeShapeExFlatBounds(pStrokeFlat);
		if ( !pShape->bStrokeNonScaling ) {
			tStrokePaintMatrix = tIdentity;
			pStrokeGeometryMatrix = &tMatrix;
			fStrokeWidth = pShape->fStrokeWidth;
			fDashOffset = pShape->fDashOffset;
			fStrokeScale = 1.0f;
		}
	}
	if ( (pShape->pDashPattern != NULL) && (pShape->iDashCount > 0) && (fabsf(fStrokeScale - 1.0f) > XGE_SHAPE_EX_EPSILON) ) {
		int i;

		pDashPattern = (float*)xrtMalloc((size_t)pShape->iDashCount * sizeof(*pDashPattern));
		if ( pDashPattern == NULL ) {
			__xgeShapeExFlatFree(&tStrokeFlat);
			__xgeShapeExStrokePathFree(&tTrimPath);
			__xgeShapeExFlatFree(&tFlat);
			return __xgeShapeExFinishShapeClips(bClipApplied, bOldClip, tOldClip, bStencilApplied, XGE_ERROR_OUT_OF_MEMORY);
		}
		for ( i = 0; i < pShape->iDashCount; i++ ) {
			pDashPattern[i] = pShape->pDashPattern[i] * fStrokeScale;
		}
	}
	if ( (pShape->iStrokeGradientUnits == XGE_SHAPE_EX_GRADIENT_OBJECT_BOUNDING_BOX) &&
	     ((pShape->iStrokeType == XGE_SHAPE_EX_FILL_LINEAR_GRADIENT) || (pShape->iStrokeType == XGE_SHAPE_EX_FILL_RADIAL_GRADIENT)) ) {
		float fStrokeHalf = fStrokeWidth * 0.5f;

		tStrokeBounds.fX -= fStrokeHalf;
		tStrokeBounds.fY -= fStrokeHalf;
		tStrokeBounds.fW += fStrokeWidth;
		tStrokeBounds.fH += fStrokeWidth;
	}
	iRet = __xgeShapeExBlendBegin(
		bSuppressOwnBlend ? 0 : pShape->bBlendSet,
		pShape->iBlend,
		&iOldBlend,
		&bBlendApplied
	);
	if ( iRet != XGE_OK ) {
		if ( pDashPattern != pShape->pDashPattern ) {
			xrtFree(pDashPattern);
		}
		__xgeShapeExFlatFree(&tFlat);
		__xgeShapeExFlatFree(&tStrokeFlat);
		__xgeShapeExStrokePathFree(&tTrimPath);
		return __xgeShapeExFinishShapeClips(bClipApplied, bOldClip, tOldClip, bStencilApplied, iRet);
	}
	if ( pShape->bStrokeFirst ) {
		if ( ((pShape->iStrokeType == XGE_SHAPE_EX_FILL_LINEAR_GRADIENT) ||
		      (pShape->iStrokeType == XGE_SHAPE_EX_FILL_RADIAL_GRADIENT)) &&
		     __xgeShapeExStrokeVisible(pShape, iStrokeColor) ) {
			iRet = __xgeShapeExDrawGradientComposite(
				pShape, XGE_SHAPE_EX_PAINT_STROKE, fTolerance, bScreenSpace, tParent, fOpacity,
				tStrokeBounds, tStrokePaintMatrix, pStrokeGeometryMatrix
			);
		} else {
			iRet = __xgeShapeExDrawStroke(pShape, pStrokeShape, pStrokeFlat, fStrokeWidth, iStrokeColor, pDashPattern, pShape->iDashCount, fDashOffset, tStrokeBounds, tStrokePaintMatrix, pStrokeGeometryMatrix, fOpacity, bScreenSpace);
		}
		if ( iRet == XGE_OK ) {
			if ( ((pShape->iFillType == XGE_SHAPE_EX_FILL_LINEAR_GRADIENT) ||
			      (pShape->iFillType == XGE_SHAPE_EX_FILL_RADIAL_GRADIENT)) &&
			     __xgeShapeExFillVisible(pShape, iFillColor) ) {
				iRet = __xgeShapeExDrawGradientComposite(
					pShape, XGE_SHAPE_EX_PAINT_FILL, fTolerance, bScreenSpace, tParent, fOpacity,
					tFillPaintBounds, tFillPaintMatrix, pFillGeometryMatrix
				);
			} else {
				iRet = __xgeShapeExDrawFill(pDrawShape, pDrawFlat, pShape->iFillRule, tMatrix, fOpacity, iFillColor, bScreenSpace, bFillAntialias, 1, bFillBinaryCoverage);
			}
		}
	} else {
		if ( ((pShape->iFillType == XGE_SHAPE_EX_FILL_LINEAR_GRADIENT) ||
		      (pShape->iFillType == XGE_SHAPE_EX_FILL_RADIAL_GRADIENT)) &&
		     __xgeShapeExFillVisible(pShape, iFillColor) ) {
			iRet = __xgeShapeExDrawGradientComposite(
				pShape, XGE_SHAPE_EX_PAINT_FILL, fTolerance, bScreenSpace, tParent, fOpacity,
				tFillPaintBounds, tFillPaintMatrix, pFillGeometryMatrix
			);
		} else {
			iRet = __xgeShapeExDrawFill(pDrawShape, pDrawFlat, pShape->iFillRule, tMatrix, fOpacity, iFillColor, bScreenSpace, bFillAntialias, 1, bFillBinaryCoverage);
		}
		if ( iRet == XGE_OK ) {
			if ( ((pShape->iStrokeType == XGE_SHAPE_EX_FILL_LINEAR_GRADIENT) ||
			      (pShape->iStrokeType == XGE_SHAPE_EX_FILL_RADIAL_GRADIENT)) &&
			     __xgeShapeExStrokeVisible(pShape, iStrokeColor) ) {
				iRet = __xgeShapeExDrawGradientComposite(
					pShape, XGE_SHAPE_EX_PAINT_STROKE, fTolerance, bScreenSpace, tParent, fOpacity,
					tStrokeBounds, tStrokePaintMatrix, pStrokeGeometryMatrix
				);
			} else {
				iRet = __xgeShapeExDrawStroke(pShape, pStrokeShape, pStrokeFlat, fStrokeWidth, iStrokeColor, pDashPattern, pShape->iDashCount, fDashOffset, tStrokeBounds, tStrokePaintMatrix, pStrokeGeometryMatrix, fOpacity, bScreenSpace);
			}
		}
	}
	iRet = __xgeShapeExBlendEnd(iOldBlend, bBlendApplied, iRet);
	if ( pDashPattern != pShape->pDashPattern ) {
		xrtFree(pDashPattern);
	}
	__xgeShapeExFlatFree(&tStrokeFlat);
	__xgeShapeExStrokePathFree(&tTrimPath);
	__xgeShapeExFlatFree(&tFlat);
	return __xgeShapeExFinishShapeClips(bClipApplied, bOldClip, tOldClip, bStencilApplied, iRet);
}

static int __xgeShapeExSceneDrawInternal(xge_shape_ex_scene pScene, float fTolerance, int bScreenSpace, xge_shape_ex_matrix_t tParent, float fParentOpacity, int bSuppressOwnBlend)
{
	xge_shape_ex_matrix_t tMatrix;
	float fOpacity;
	xge_rect_t tOldClip;
	int bOldClip;
	int bClipApplied;
	int bStencilApplied;
	int iOldBlend;
	int bBlendApplied;
	int i;
	int iRet;

	if ( !__xgeShapeExSceneValid(pScene) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( !pScene->bVisible ) {
		return XGE_OK;
	}
	fOpacity = fParentOpacity * pScene->fOpacity;
	if ( fOpacity <= 0.0f ) {
		return XGE_OK;
	}
	tMatrix = __xgeShapeExMatrixMul(tParent, pScene->tTransform);
	bClipApplied = 0;
	bStencilApplied = 0;
	bOldClip = 0;
	memset(&tOldClip, 0, sizeof(tOldClip));
	if ( pScene->bClipRect ) {
		xge_rect_t tClip = __xgeShapeExMatrixRectBounds(tMatrix, pScene->tClipRect);

		iRet = __xgeShapeAutoBatchFlush();
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		tOldClip = xgeClipGet();
		bOldClip = (tOldClip.fW > 0.0f) && (tOldClip.fH > 0.0f);
		if ( bOldClip ) {
			tClip = __xgeShapeExRectIntersect(tOldClip, tClip);
		}
		tClip = __xgeShapeExPixelCenterClipRect(tClip);
		xgeClipSet(tClip);
		bClipApplied = 1;
	}
	if ( pScene->iClipShapeCount > 0 ) {
		iRet = __xgeShapeExBeginStencilClipList(pScene->pClipShapes, pScene->pClipShapeModes, pScene->iClipShapeCount, fTolerance, bScreenSpace, tMatrix);
		if ( iRet != XGE_OK ) {
			return __xgeShapeExFinishShapeClips(bClipApplied, bOldClip, tOldClip, 0, iRet);
		}
		bStencilApplied = 1;
	}
	iRet = __xgeShapeExBlendBegin(
		bSuppressOwnBlend ? 0 : pScene->bBlendSet,
		pScene->iBlend,
		&iOldBlend,
		&bBlendApplied
	);
	if ( iRet != XGE_OK ) {
		return __xgeShapeExFinishShapeClips(bClipApplied, bOldClip, tOldClip, bStencilApplied, iRet);
	}
	for ( i = 0; i < pScene->iChildCount; i++ ) {
		const xge_shape_ex_scene_child_t* pChild = &pScene->pChildren[i];

		if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE ) {
			iRet = __xgeShapeExDrawDispatch(pChild->pShape, fTolerance, bScreenSpace, tMatrix, fOpacity, 0);
		} else if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SCENE ) {
			iRet = __xgeShapeExSceneDrawDispatch(pChild->pScene, fTolerance, bScreenSpace, tMatrix, fOpacity, 0);
		} else {
			iRet = XGE_ERROR_INVALID_ARGUMENT;
		}
		if ( iRet != XGE_OK ) {
			break;
		}
	}
	iRet = __xgeShapeExBlendEnd(iOldBlend, bBlendApplied, iRet);
	return __xgeShapeExFinishShapeClips(bClipApplied, bOldClip, tOldClip, bStencilApplied, iRet);
}

static int __xgeShapeExSvgIsSpace(char c)
{
	return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n') || (c == '\f');
}

static int __xgeShapeExSvgIsCommand(char c)
{
	switch ( c ) {
		case 'M': case 'm': case 'L': case 'l': case 'H': case 'h': case 'V': case 'v':
		case 'C': case 'c': case 'S': case 's': case 'Q': case 'q': case 'T': case 't':
		case 'A': case 'a': case 'Z': case 'z':
			return 1;
		default:
			return 0;
	}
}

static char __xgeShapeExSvgLower(char c)
{
	return (char)((c >= 'A' && c <= 'Z') ? (c + 32) : c);
}

static void __xgeShapeExSvgSkipSeparators(const char** ppText)
{
	const char* pText = *ppText;

	while ( (*pText == ',') || __xgeShapeExSvgIsSpace(*pText) ) {
		pText++;
	}
	*ppText = pText;
}

static int __xgeShapeExSvgHasNumber(const char* sText)
{
	__xgeShapeExSvgSkipSeparators(&sText);
	return (*sText != '\0') && !__xgeShapeExSvgIsCommand(*sText);
}

static int __xgeShapeExSvgIsDigit(char c)
{
	return (c >= '0') && (c <= '9');
}

static int __xgeShapeExSvgScanNumberEnd(const char* pText, const char** ppEnd)
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
	while ( __xgeShapeExSvgIsDigit(*p) ) {
		iDigitCount++;
		p++;
	}
	iFracDigitCount = 0;
	if ( *p == '.' ) {
		p++;
		while ( __xgeShapeExSvgIsDigit(*p) ) {
			iFracDigitCount++;
			p++;
		}
	}
	if ( (iDigitCount + iFracDigitCount) <= 0 ) {
		return 0;
	}
	if ( (*p == 'e') || (*p == 'E') ) {
		p++;
		if ( (*p == '+') || (*p == '-') ) {
			p++;
		}
		iExpDigitCount = 0;
		while ( __xgeShapeExSvgIsDigit(*p) ) {
			iExpDigitCount++;
			p++;
		}
		if ( iExpDigitCount <= 0 ) {
			return 0;
		}
	}
	*ppEnd = p;
	return 1;
}

static int __xgeShapeExSvgDoubleToFloat(double fValue, float* pValue)
{
	if ( pValue == NULL ) {
		return 0;
	}
	if ( fValue != fValue ) {
		return 0;
	}
	if ( (fValue < -(double)FLT_MAX) || (fValue > (double)FLT_MAX) ) {
		return 0;
	}
	*pValue = (float)fValue;
	return 1;
}

static int __xgeShapeExSvgReadNumber(const char** ppText, float* pValue)
{
	const char* pText;
	const char* pNumberEnd;
	char* pEnd;
	double fValue;

	if ( (ppText == NULL) || (*ppText == NULL) || (pValue == NULL) ) {
		return 0;
	}
	pText = *ppText;
	__xgeShapeExSvgSkipSeparators(&pText);
	if ( (*pText == '\0') || __xgeShapeExSvgIsCommand(*pText) ) {
		return 0;
	}
	if ( !__xgeShapeExSvgScanNumberEnd(pText, &pNumberEnd) ) {
		return 0;
	}
	fValue = strtod(pText, &pEnd);
	if ( (pEnd == pText) || (pEnd != pNumberEnd) ) {
		return 0;
	}
	if ( !__xgeShapeExSvgDoubleToFloat(fValue, pValue) ) {
		return 0;
	}
	*ppText = pNumberEnd;
	return 1;
}

static int __xgeShapeExSvgReadFlag(const char** ppText, int* pValue)
{
	const char* pText;

	if ( (ppText == NULL) || (*ppText == NULL) || (pValue == NULL) ) {
		return 0;
	}
	pText = *ppText;
	__xgeShapeExSvgSkipSeparators(&pText);
	if ( (*pText != '0') && (*pText != '1') ) {
		return 0;
	}
	*pValue = *pText - '0';
	pText++;
	__xgeShapeExSvgSkipSeparators(&pText);
	*ppText = pText;
	return 1;
}

static int __xgeShapeExSvgArcTo(xge_shape_ex pShape, xge_vec2_t tCurrent, float fRX, float fRY, float fAxisDegrees, int bLargeArc, int bSweep, float fX, float fY)
{
	float fPhi;
	float fCosPhi;
	float fSinPhi;
	float fDX2;
	float fDY2;
	float fX1p;
	float fY1p;
	float fLambda;
	float fSign;
	float fNumerator;
	float fDenominator;
	float fCoef;
	float fCXp;
	float fCYp;
	float fCX;
	float fCY;
	float fUX;
	float fUY;
	float fVX;
	float fVY;
	float fTheta;
	float fDelta;
	int iSegments;
	int i;
	int iRet;

	fRX = fabsf(fRX);
	fRY = fabsf(fRY);
	if ( hypotf(fX - tCurrent.fX, fY - tCurrent.fY) <= XGE_SHAPE_EX_EPSILON ) {
		return XGE_OK;
	}
	if ( (fRX <= XGE_SHAPE_EX_EPSILON) || (fRY <= XGE_SHAPE_EX_EPSILON) ) {
		return xgeShapeExLineTo(pShape, fX, fY);
	}
	fPhi = fAxisDegrees * (XGE_SHAPE_EX_PI / 180.0f);
	fCosPhi = cosf(fPhi);
	fSinPhi = sinf(fPhi);
	fDX2 = (tCurrent.fX - fX) * 0.5f;
	fDY2 = (tCurrent.fY - fY) * 0.5f;
	fX1p = (fCosPhi * fDX2) + (fSinPhi * fDY2);
	fY1p = (-fSinPhi * fDX2) + (fCosPhi * fDY2);
	fLambda = (fX1p * fX1p) / (fRX * fRX) + (fY1p * fY1p) / (fRY * fRY);
	if ( fLambda > 1.0f ) {
		float fScale = sqrtf(fLambda);
		fRX *= fScale;
		fRY *= fScale;
	}
	fSign = (bLargeArc == bSweep) ? -1.0f : 1.0f;
	fNumerator = (fRX * fRX * fRY * fRY) - (fRX * fRX * fY1p * fY1p) - (fRY * fRY * fX1p * fX1p);
	fDenominator = (fRX * fRX * fY1p * fY1p) + (fRY * fRY * fX1p * fX1p);
	fCoef = (fDenominator <= XGE_SHAPE_EX_EPSILON) ? 0.0f : fSign * sqrtf(fmaxf(0.0f, fNumerator / fDenominator));
	fCXp = fCoef * ((fRX * fY1p) / fRY);
	fCYp = fCoef * (-(fRY * fX1p) / fRX);
	fCX = (fCosPhi * fCXp) - (fSinPhi * fCYp) + ((tCurrent.fX + fX) * 0.5f);
	fCY = (fSinPhi * fCXp) + (fCosPhi * fCYp) + ((tCurrent.fY + fY) * 0.5f);
	fUX = (fX1p - fCXp) / fRX;
	fUY = (fY1p - fCYp) / fRY;
	fVX = (-fX1p - fCXp) / fRX;
	fVY = (-fY1p - fCYp) / fRY;
	fTheta = atan2f(fUY, fUX);
	fDelta = atan2f((fUX * fVY) - (fUY * fVX), (fUX * fVX) + (fUY * fVY));
	if ( !bSweep && fDelta > 0.0f ) {
		fDelta -= XGE_SHAPE_EX_TAU;
	} else if ( bSweep && fDelta < 0.0f ) {
		fDelta += XGE_SHAPE_EX_TAU;
	}
	/* ThorVG keeps every SVG arc segment strictly below 90 degrees. */
	iSegments = (int)(fabsf(fDelta) / (XGE_SHAPE_EX_PI * 0.5f) + 1.0f);
	if ( iSegments < 1 ) {
		iSegments = 1;
	}
	for ( i = 0; i < iSegments; i++ ) {
		float fT0 = fTheta + fDelta * ((float)i / (float)iSegments);
		float fT1 = fTheta + fDelta * ((float)(i + 1) / (float)iSegments);
		float fAlpha = (4.0f / 3.0f) * tanf((fT1 - fT0) * 0.25f);
		float fCos0 = cosf(fT0);
		float fSin0 = sinf(fT0);
		float fCos1 = cosf(fT1);
		float fSin1 = sinf(fT1);
		xge_vec2_t tC1;
		xge_vec2_t tC2;
		xge_vec2_t tP;

		tC1.fX = fCX + fRX * ((fCos0 - fAlpha * fSin0) * fCosPhi) - fRY * ((fSin0 + fAlpha * fCos0) * fSinPhi);
		tC1.fY = fCY + fRX * ((fCos0 - fAlpha * fSin0) * fSinPhi) + fRY * ((fSin0 + fAlpha * fCos0) * fCosPhi);
		tC2.fX = fCX + fRX * ((fCos1 + fAlpha * fSin1) * fCosPhi) - fRY * ((fSin1 - fAlpha * fCos1) * fSinPhi);
		tC2.fY = fCY + fRX * ((fCos1 + fAlpha * fSin1) * fSinPhi) + fRY * ((fSin1 - fAlpha * fCos1) * fCosPhi);
		if ( i == (iSegments - 1) ) {
			tP.fX = fX;
			tP.fY = fY;
		} else {
			tP.fX = fCX + fRX * fCos1 * fCosPhi - fRY * fSin1 * fSinPhi;
			tP.fY = fCY + fRX * fCos1 * fSinPhi + fRY * fSin1 * fCosPhi;
		}
		iRet = xgeShapeExCubicTo(pShape, tC1.fX, tC1.fY, tC2.fX, tC2.fY, tP.fX, tP.fY);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	return XGE_OK;
}

static xge_vec2_t __xgeShapeExEllipsePoint(float fCX, float fCY, float fRX, float fRY, float fRadians)
{
	xge_vec2_t tPoint;

	tPoint.fX = fCX + cosf(fRadians) * fRX;
	tPoint.fY = fCY + sinf(fRadians) * fRY;
	return tPoint;
}

static float __xgeShapeExClampSweep(float fSweep)
{
	if ( fSweep > XGE_SHAPE_EX_TAU ) {
		return XGE_SHAPE_EX_TAU;
	}
	if ( fSweep < -XGE_SHAPE_EX_TAU ) {
		return -XGE_SHAPE_EX_TAU;
	}
	return fSweep;
}

static int __xgeShapeExAppendEllipseArcSegments(xge_shape_ex pShape, float fCX, float fCY, float fRX, float fRY, float fStartRadians, float fSweepRadians, int bMoveToStart)
{
	xge_vec2_t tStart;
	int iSegments;
	int i;
	int iRet;

	if ( fabsf(fSweepRadians) <= XGE_SHAPE_EX_EPSILON ) {
		return XGE_OK;
	}
	tStart = __xgeShapeExEllipsePoint(fCX, fCY, fRX, fRY, fStartRadians);
	if ( bMoveToStart ) {
		iRet = xgeShapeExMoveTo(pShape, tStart.fX, tStart.fY);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	} else if ( !pShape->bHasCurrent ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iSegments = (int)ceilf(fabsf(fSweepRadians) / (XGE_SHAPE_EX_PI * 0.5f));
	if ( iSegments < 1 ) {
		iSegments = 1;
	}
	for ( i = 0; i < iSegments; i++ ) {
		float fT0 = fStartRadians + fSweepRadians * ((float)i / (float)iSegments);
		float fT1 = fStartRadians + fSweepRadians * ((float)(i + 1) / (float)iSegments);
		float fAlpha = (4.0f / 3.0f) * tanf((fT1 - fT0) * 0.25f);
		float fCos0 = cosf(fT0);
		float fSin0 = sinf(fT0);
		float fCos1 = cosf(fT1);
		float fSin1 = sinf(fT1);
		xge_vec2_t tC1;
		xge_vec2_t tC2;
		xge_vec2_t tP;

		tC1.fX = fCX + fRX * (fCos0 - fAlpha * fSin0);
		tC1.fY = fCY + fRY * (fSin0 + fAlpha * fCos0);
		tC2.fX = fCX + fRX * (fCos1 + fAlpha * fSin1);
		tC2.fY = fCY + fRY * (fSin1 - fAlpha * fCos1);
		tP = __xgeShapeExEllipsePoint(fCX, fCY, fRX, fRY, fT1);
		iRet = xgeShapeExCubicTo(pShape, tC1.fX, tC1.fY, tC2.fX, tC2.fY, tP.fX, tP.fY);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	return XGE_OK;
}

int xgeShapeExCreate(xge_shape_ex* ppShape)
{
	xge_shape_ex pShape;

	if ( ppShape == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pShape = (xge_shape_ex)xrtCalloc(1, sizeof(*pShape));
	if ( pShape == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pShape->iMagic = XGE_SHAPE_EX_MAGIC;
	pShape->iRefCount = 1;
	pShape->iFillColor = XGE_COLOR_RGBA(0, 0, 0, 0);
	pShape->iStrokeType = XGE_SHAPE_EX_FILL_SOLID;
	pShape->iStrokeColor = XGE_COLOR_RGBA(0, 0, 0, 0);
	pShape->fStrokeWidth = 0.0f;
	pShape->iFillRule = XGE_SHAPE_EX_FILL_NON_ZERO;
	pShape->iLineJoin = XGE_SHAPE_EX_JOIN_BEVEL;
	pShape->iLineCap = XGE_SHAPE_EX_CAP_SQUARE;
	pShape->fMiterLimit = 4.0f;
	pShape->fTrimBegin = 0.0f;
	pShape->fTrimEnd = 1.0f;
	pShape->bTrimSimultaneous = 0;
	pShape->fOpacity = 1.0f;
	pShape->bVisible = 1;
	pShape->iBlend = XGE_BLEND_ALPHA;
	pShape->bBlendSet = 0;
	pShape->iFillGradientSpread = XGE_SHAPE_EX_GRADIENT_SPREAD_PAD;
	pShape->tFillGradientTransform = __xgeShapeExMatrixIdentity();
	pShape->iStrokeGradientSpread = XGE_SHAPE_EX_GRADIENT_SPREAD_PAD;
	pShape->tStrokeGradientTransform = __xgeShapeExMatrixIdentity();
	pShape->tTransform = __xgeShapeExMatrixIdentity();
	__xgeShapeExTransformStateReset(&pShape->tTransformState);
	*ppShape = pShape;
	return XGE_OK;
}

int xgeShapeExAddRef(xge_shape_ex pShape)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pShape->iRefCount++;
	return XGE_OK;
}

int xgeShapeExRefCountGet(xge_shape_ex pShape, int* pRefCount)
{
	if ( !__xgeShapeExValid(pShape) || (pRefCount == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pRefCount = pShape->iRefCount;
	return XGE_OK;
}

uint32_t xgeShapeExIdFromName(const char* sName)
{
	uint32_t iHash;
	int iByte;

	if ( sName == NULL ) return 0;
	iHash = 5381u;
	while ( (iByte = (int)(signed char)*sName++) != 0 ) {
		iHash = iHash * 33u + (uint32_t)iByte;
	}
	return iHash;
}

int xgeShapeExId(xge_shape_ex pShape, uint32_t iId)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pShape->iId = iId;
	return XGE_OK;
}

int xgeShapeExIdGet(xge_shape_ex pShape, uint32_t* pId)
{
	if ( !__xgeShapeExValid(pShape) || (pId == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pId = pShape->iId;
	return XGE_OK;
}

int xgeShapeExParentGet(xge_shape_ex pShape, xge_shape_ex_scene* ppParentScene)
{
	if ( !__xgeShapeExValid(pShape) || (ppParentScene == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppParentScene = pShape->pParentScene;
	return XGE_OK;
}

static int __xgeShapeExCloneInternal(xge_shape_ex pShape, xge_shape_ex* ppClone, xge_shape_ex* pCloneStack, int iCloneStackCount)
{
	xge_shape_ex pClone;
	int iRet;
	int i;

	if ( !__xgeShapeExValid(pShape) || (ppClone == NULL) || (pCloneStack == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppClone = NULL;
	if ( iCloneStackCount >= XGE_SHAPE_EX_CLIP_BOUNDS_MAX_DEPTH ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < iCloneStackCount; i++ ) {
		if ( pCloneStack[i] == pShape ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
	}
	pCloneStack[iCloneStackCount++] = pShape;
	iRet = xgeShapeExCreate(&pClone);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = __xgeShapeExReserveCommands(pClone, pShape->iCommandCount);
	if ( iRet == XGE_OK ) {
		iRet = __xgeShapeExReservePoints(pClone, pShape->iPointCount);
	}
	if ( iRet != XGE_OK ) {
		xgeShapeExDestroy(pClone);
		return iRet;
	}
	if ( pShape->iCommandCount > 0 ) {
		memcpy(pClone->pCommands, pShape->pCommands, (size_t)pShape->iCommandCount * sizeof(*pShape->pCommands));
	}
	if ( pShape->iPointCount > 0 ) {
		memcpy(pClone->pPoints, pShape->pPoints, (size_t)pShape->iPointCount * sizeof(*pShape->pPoints));
	}
	pClone->iCommandCount = pShape->iCommandCount;
	pClone->iPointCount = pShape->iPointCount;
	pClone->tCurrent = pShape->tCurrent;
	pClone->tStart = pShape->tStart;
	pClone->tLastCubicControl = pShape->tLastCubicControl;
	pClone->tLastQuadControl = pShape->tLastQuadControl;
	pClone->bHasCurrent = pShape->bHasCurrent;
	pClone->bHasStart = pShape->bHasStart;
	pClone->iFillType = pShape->iFillType;
	pClone->iFillColor = pShape->iFillColor;
	pClone->iStrokeType = pShape->iStrokeType;
	pClone->iStrokeColor = pShape->iStrokeColor;
	pClone->bStrokeStateCreated = pShape->bStrokeStateCreated;
	pClone->fStrokeWidth = pShape->fStrokeWidth;
	pClone->fFillX1 = pShape->fFillX1;
	pClone->fFillY1 = pShape->fFillY1;
	pClone->fFillX2 = pShape->fFillX2;
	pClone->fFillY2 = pShape->fFillY2;
	pClone->fFillR = pShape->fFillR;
	pClone->fFillFX = pShape->fFillFX;
	pClone->fFillFY = pShape->fFillFY;
	pClone->fFillFR = pShape->fFillFR;
	pClone->iFillGradientUnits = pShape->iFillGradientUnits;
	pClone->iFillGradientSpread = pShape->iFillGradientSpread;
	pClone->tFillGradientTransform = pShape->tFillGradientTransform;
	pClone->fStrokeX1 = pShape->fStrokeX1;
	pClone->fStrokeY1 = pShape->fStrokeY1;
	pClone->fStrokeX2 = pShape->fStrokeX2;
	pClone->fStrokeY2 = pShape->fStrokeY2;
	pClone->fStrokeR = pShape->fStrokeR;
	pClone->fStrokeFX = pShape->fStrokeFX;
	pClone->fStrokeFY = pShape->fStrokeFY;
	pClone->fStrokeFR = pShape->fStrokeFR;
	pClone->iStrokeGradientUnits = pShape->iStrokeGradientUnits;
	pClone->iStrokeGradientSpread = pShape->iStrokeGradientSpread;
	pClone->tStrokeGradientTransform = pShape->tStrokeGradientTransform;
	pClone->iFillRule = pShape->iFillRule;
	pClone->iLineJoin = pShape->iLineJoin;
	pClone->iLineCap = pShape->iLineCap;
	pClone->fMiterLimit = pShape->fMiterLimit;
	pClone->bStrokeNonScaling = pShape->bStrokeNonScaling;
	pClone->bTrimPath = pShape->bTrimPath;
	pClone->fTrimBegin = pShape->fTrimBegin;
	pClone->fTrimEnd = pShape->fTrimEnd;
	pClone->bTrimSimultaneous = pShape->bTrimSimultaneous;
	pClone->bStrokeFirst = pShape->bStrokeFirst;
	pClone->bStencilPaint = pShape->bStencilPaint;
	pClone->fOpacity = pShape->fOpacity;
	pClone->bVisible = pShape->bVisible;
	pClone->iBlend = pShape->iBlend;
	pClone->bBlendSet = pShape->bBlendSet;
	pClone->bCoverageAntialiasDisabled = pShape->bCoverageAntialiasDisabled;
	pClone->bAxisAlignedFillAntialias = pShape->bAxisAlignedFillAntialias;
	pClone->bStrokeUnionMaskDraw = pShape->bStrokeUnionMaskDraw;
	pClone->bClipRect = pShape->bClipRect;
	pClone->tClipRect = pShape->tClipRect;
	pClone->tTransform = pShape->tTransform;
	pClone->tTransformState = pShape->tTransformState;
	if ( pShape->iClipShapeCount > 0 ) {
		iRet = __xgeShapeExReserveClipShapes(pClone, pShape->iClipShapeCount);
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pClone);
			return iRet;
		}
		for ( i = 0; i < pShape->iClipShapeCount; i++ ) {
			xge_shape_ex pClipClone;

			pClipClone = NULL;
			iRet = __xgeShapeExCloneInternal(pShape->pClipShapes[i], &pClipClone, pCloneStack, iCloneStackCount);
			if ( iRet == XGE_OK ) {
				int iMode = pShape->pClipShapeModes != NULL ? pShape->pClipShapeModes[i] : XGE_SHAPE_EX_CLIP_INTERSECT;

				iRet = __xgeShapeExClipShapeAddRefEx(pClone, pClipClone, iMode);
				xgeShapeExDestroy(pClipClone);
			}
			if ( iRet != XGE_OK ) {
				xgeShapeExDestroy(pClone);
				return iRet;
			}
		}
	}
	if ( pShape->iMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_SHAPE ) {
		xge_shape_ex pMaskClone = NULL;

		iRet = xgeShapeExClone(pShape->pMaskShape, &pMaskClone);
		if ( iRet == XGE_OK ) {
			iRet = xgeShapeExMaskShapeSet(pClone, pMaskClone, pShape->iMaskMethod);
		}
		xgeShapeExDestroy(pMaskClone);
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pClone);
			return iRet;
		}
	} else if ( pShape->iMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_SCENE ) {
		xge_shape_ex_scene pMaskClone = NULL;

		iRet = xgeShapeExSceneClone(pShape->pMaskScene, &pMaskClone);
		if ( iRet == XGE_OK ) {
			iRet = xgeShapeExMaskSceneSet(pClone, pMaskClone, pShape->iMaskMethod);
		}
		xgeShapeExSceneDestroy(pMaskClone);
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pClone);
			return iRet;
		}
	}
	if ( (pShape->pDashPattern != NULL) && (pShape->iDashCount > 0) ) {
		iRet = xgeShapeExStrokeDash(pClone, pShape->pDashPattern, pShape->iDashCount, pShape->fDashOffset);
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pClone);
			return iRet;
		}
	}
	if ( (pShape->pFillStops != NULL) && (pShape->iFillStopCount > 0) ) {
		iRet = __xgeShapeExReserveFillStops(pClone, pShape->iFillStopCount);
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pClone);
			return iRet;
		}
		memcpy(pClone->pFillStops, pShape->pFillStops, (size_t)pShape->iFillStopCount * sizeof(*pShape->pFillStops));
		memcpy(
			pClone->pFillSourceStops, pShape->pFillSourceStops,
			(size_t)pShape->iFillStopCount * sizeof(*pShape->pFillSourceStops)
		);
		pClone->iFillStopCount = pShape->iFillStopCount;
	}
	if ( (pShape->pStrokeStops != NULL) && (pShape->iStrokeStopCount > 0) ) {
		iRet = __xgeShapeExReserveStrokeStops(pClone, pShape->iStrokeStopCount);
		if ( iRet != XGE_OK ) {
			xgeShapeExDestroy(pClone);
			return iRet;
		}
		memcpy(pClone->pStrokeStops, pShape->pStrokeStops, (size_t)pShape->iStrokeStopCount * sizeof(*pShape->pStrokeStops));
		memcpy(
			pClone->pStrokeSourceStops, pShape->pStrokeSourceStops,
			(size_t)pShape->iStrokeStopCount * sizeof(*pShape->pStrokeSourceStops)
		);
		pClone->iStrokeStopCount = pShape->iStrokeStopCount;
	}
	*ppClone = pClone;
	return XGE_OK;
}

int xgeShapeExClone(xge_shape_ex pShape, xge_shape_ex* ppClone)
{
	xge_shape_ex arrCloneStack[XGE_SHAPE_EX_CLIP_BOUNDS_MAX_DEPTH];

	return __xgeShapeExCloneInternal(pShape, ppClone, arrCloneStack, 0);
}

void xgeShapeExDestroy(xge_shape_ex pShape)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return;
	}
	pShape->iRefCount--;
	if ( pShape->iRefCount > 0 ) {
		return;
	}
	__xgeShapeExClipShapesClearInternal(pShape);
	__xgeShapeExMaskClearInternal(pShape);
	pShape->iMagic = 0;
	xrtFree(pShape->pCommands);
	xrtFree(pShape->pPoints);
	xrtFree(pShape->pDashPattern);
	xrtFree(pShape->pFillStops);
	xrtFree(pShape->pFillSourceStops);
	xrtFree(pShape->pStrokeStops);
	xrtFree(pShape->pStrokeSourceStops);
	xrtFree(pShape->pClipShapes);
	xrtFree(pShape->pClipShapeModes);
	xrtFree(pShape);
}

int xgeShapeExReset(xge_shape_ex pShape)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pShape->iCommandCount = 0;
	pShape->iPointCount = 0;
	pShape->bHasCurrent = 0;
	pShape->bHasStart = 0;
	memset(&pShape->tCurrent, 0, sizeof(pShape->tCurrent));
	memset(&pShape->tStart, 0, sizeof(pShape->tStart));
	memset(&pShape->tLastCubicControl, 0, sizeof(pShape->tLastCubicControl));
	memset(&pShape->tLastQuadControl, 0, sizeof(pShape->tLastQuadControl));
	return XGE_OK;
}

int xgeShapeExMoveTo(xge_shape_ex pShape, float fX, float fY)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	return __xgeShapeExAddMoveTo(pShape, fX, fY);
}

int xgeShapeExLineTo(xge_shape_ex pShape, float fX, float fY)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	return __xgeShapeExAddLineTo(pShape, fX, fY);
}

int xgeShapeExQuadTo(xge_shape_ex pShape, float fCX, float fCY, float fX, float fY)
{
	xge_vec2_t arrPoints[2];
	xge_vec2_t tPrev;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || !pShape->bHasCurrent ||
	     !__xgeShapeExCoordsFinite(fCX, fCY) || !__xgeShapeExCoordsFinite(fX, fY) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tPrev = pShape->tCurrent;
	arrPoints[0].fX = fCX;
	arrPoints[0].fY = fCY;
	arrPoints[1].fX = fX;
	arrPoints[1].fY = fY;
	iRet = __xgeShapeExAppendCommand(pShape, XGE_SHAPE_EX_CMD_QUAD_TO, arrPoints, 2);
	if ( iRet == XGE_OK ) {
		pShape->tLastCubicControl = arrPoints[1];
		pShape->tLastQuadControl = arrPoints[0];
		pShape->tCurrent = arrPoints[1];
		if ( !pShape->bHasStart ) {
			pShape->tStart = tPrev;
			pShape->bHasStart = 1;
		}
	}
	return iRet;
}

int xgeShapeExCubicTo(xge_shape_ex pShape, float fC1X, float fC1Y, float fC2X, float fC2Y, float fX, float fY)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	return __xgeShapeExAddCubicTo(pShape, fC1X, fC1Y, fC2X, fC2Y, fX, fY);
}

int xgeShapeExArcTo(xge_shape_ex pShape, float fRX, float fRY, float fAxisDegrees, int bLargeArc, int bSweep, float fX, float fY)
{
	xge_shape_ex_path_snapshot_t tSnapshot = {0};
	int iRet;

	if ( !__xgeShapeExValid(pShape) || !pShape->bHasCurrent ||
	     !__xgeShapeExCoordsFinite(fRX, fRY) || !__xgeShapeExFloatFinite(fAxisDegrees) || !__xgeShapeExCoordsFinite(fX, fY) ||
	     ((bLargeArc != 0) && (bLargeArc != 1)) || ((bSweep != 0) && (bSweep != 1)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	__xgeShapeExPathSnapshot(pShape, &tSnapshot);
	iRet = __xgeShapeExSvgArcTo(pShape, pShape->tCurrent, fRX, fRY, fAxisDegrees, bLargeArc, bSweep, fX, fY);
	if ( iRet != XGE_OK ) {
		__xgeShapeExPathRestore(pShape, &tSnapshot);
	}
	return iRet;
}

int xgeShapeExClose(xge_shape_ex pShape)
{
	int iRet;

	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (pShape->iCommandCount > 0) &&
	     (pShape->pCommands[pShape->iCommandCount - 1] == XGE_SHAPE_EX_CMD_CLOSE) ) {
		return XGE_OK;
	}
	iRet = __xgeShapeExAppendCommand(pShape, XGE_SHAPE_EX_CMD_CLOSE, NULL, 0);
	if ( (iRet == XGE_OK) && pShape->bHasStart ) {
		pShape->tCurrent = pShape->tStart;
		pShape->tLastCubicControl = pShape->tCurrent;
		pShape->tLastQuadControl = pShape->tCurrent;
	}
	return iRet;
}

int xgeShapeExAppendTriangle(xge_shape_ex pShape, float fX1, float fY1, float fX2, float fY2, float fX3, float fY3, int bClockwise)
{
	xge_shape_ex_path_snapshot_t tSnapshot = {0};
	int iRet;

	if ( !__xgeShapeExValid(pShape) ||
	     !__xgeShapeExCoordsFinite(fX1, fY1) || !__xgeShapeExCoordsFinite(fX2, fY2) || !__xgeShapeExCoordsFinite(fX3, fY3) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	__xgeShapeExPathSnapshot(pShape, &tSnapshot);
	iRet = xgeShapeExMoveTo(pShape, fX1, fY1);
	if ( iRet != XGE_OK ) goto fail;
	if ( bClockwise ) {
		iRet = xgeShapeExLineTo(pShape, fX2, fY2);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExLineTo(pShape, fX3, fY3);
		if ( iRet != XGE_OK ) goto fail;
	} else {
		iRet = xgeShapeExLineTo(pShape, fX3, fY3);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExLineTo(pShape, fX2, fY2);
		if ( iRet != XGE_OK ) goto fail;
	}
	iRet = xgeShapeExClose(pShape);
	if ( iRet != XGE_OK ) goto fail;
	return XGE_OK;
fail:
	__xgeShapeExPathRestore(pShape, &tSnapshot);
	return iRet;
}

int xgeShapeExAppendLine(xge_shape_ex pShape, float fX1, float fY1, float fX2, float fY2)
{
	xge_shape_ex_path_snapshot_t tSnapshot = {0};
	int iRet;

	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExCoordsFinite(fX1, fY1) || !__xgeShapeExCoordsFinite(fX2, fY2) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	__xgeShapeExPathSnapshot(pShape, &tSnapshot);
	iRet = xgeShapeExMoveTo(pShape, fX1, fY1);
	if ( iRet != XGE_OK ) goto fail;
	iRet = xgeShapeExLineTo(pShape, fX2, fY2);
	if ( iRet != XGE_OK ) goto fail;
	return XGE_OK;
fail:
	__xgeShapeExPathRestore(pShape, &tSnapshot);
	return iRet;
}

static int __xgeShapeExAppendPointList(xge_shape_ex pShape, const xge_vec2_t* pPoints, int iPointCount, int bClose)
{
	xge_shape_ex_path_snapshot_t tSnapshot = {0};
	int i;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || (pPoints == NULL) || (iPointCount <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < iPointCount; i++ ) {
		if ( !__xgeShapeExPointFinite(pPoints[i]) ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
	}
	__xgeShapeExPathSnapshot(pShape, &tSnapshot);
	iRet = xgeShapeExMoveTo(pShape, pPoints[0].fX, pPoints[0].fY);
	if ( iRet != XGE_OK ) goto fail;
	for ( i = 1; i < iPointCount; i++ ) {
		iRet = xgeShapeExLineTo(pShape, pPoints[i].fX, pPoints[i].fY);
		if ( iRet != XGE_OK ) goto fail;
	}
	if ( bClose ) {
		iRet = xgeShapeExClose(pShape);
		if ( iRet != XGE_OK ) goto fail;
	}
	return XGE_OK;
fail:
	__xgeShapeExPathRestore(pShape, &tSnapshot);
	return iRet;
}

int xgeShapeExAppendPolyline(xge_shape_ex pShape, const xge_vec2_t* pPoints, int iPointCount)
{
	return __xgeShapeExAppendPointList(pShape, pPoints, iPointCount, 0);
}

int xgeShapeExAppendPolygon(xge_shape_ex pShape, const xge_vec2_t* pPoints, int iPointCount)
{
	return __xgeShapeExAppendPointList(pShape, pPoints, iPointCount, 1);
}

static int __xgeShapeExAppendRectWithOrigin(xge_shape_ex pShape, float fX, float fY, float fW, float fH, float fRX, float fRY, int bClockwise, int bSvgOrigin)
{
	xge_shape_ex_path_snapshot_t tSnapshot = {0};
	float fHalfW;
	float fHalfH;
	float fKX;
	float fKY;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExCoordsFinite(fX, fY) ||
	     !__xgeShapeExCoordsFinite(fW, fH) || !__xgeShapeExCoordsFinite(fRX, fRY) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	__xgeShapeExPathSnapshot(pShape, &tSnapshot);
	if ( (fabsf(fRX) <= XGE_SHAPE_EX_EPSILON) && (fabsf(fRY) <= XGE_SHAPE_EX_EPSILON) ) {
		iRet = xgeShapeExMoveTo(pShape, bSvgOrigin ? fX : fX + fW, fY);
		if ( iRet != XGE_OK ) goto fail;
		if ( bSvgOrigin && bClockwise ) {
			iRet = xgeShapeExLineTo(pShape, fX + fW, fY);
			if ( iRet != XGE_OK ) goto fail;
			iRet = xgeShapeExLineTo(pShape, fX + fW, fY + fH);
			if ( iRet != XGE_OK ) goto fail;
			iRet = xgeShapeExLineTo(pShape, fX, fY + fH);
			if ( iRet != XGE_OK ) goto fail;
		} else if ( bSvgOrigin ) {
			iRet = xgeShapeExLineTo(pShape, fX, fY + fH);
			if ( iRet != XGE_OK ) goto fail;
			iRet = xgeShapeExLineTo(pShape, fX + fW, fY + fH);
			if ( iRet != XGE_OK ) goto fail;
			iRet = xgeShapeExLineTo(pShape, fX + fW, fY);
			if ( iRet != XGE_OK ) goto fail;
		} else if ( bClockwise ) {
			iRet = xgeShapeExLineTo(pShape, fX + fW, fY + fH);
			if ( iRet != XGE_OK ) goto fail;
			iRet = xgeShapeExLineTo(pShape, fX, fY + fH);
			if ( iRet != XGE_OK ) goto fail;
			iRet = xgeShapeExLineTo(pShape, fX, fY);
			if ( iRet != XGE_OK ) goto fail;
		} else {
			iRet = xgeShapeExLineTo(pShape, fX, fY);
			if ( iRet != XGE_OK ) goto fail;
			iRet = xgeShapeExLineTo(pShape, fX, fY + fH);
			if ( iRet != XGE_OK ) goto fail;
			iRet = xgeShapeExLineTo(pShape, fX + fW, fY + fH);
			if ( iRet != XGE_OK ) goto fail;
		}
		iRet = xgeShapeExClose(pShape);
		if ( iRet != XGE_OK ) goto fail;
		return XGE_OK;
	}
	fHalfW = fW * 0.5f;
	fHalfH = fH * 0.5f;
	if ( fRX > fHalfW ) fRX = fHalfW;
	if ( fRY > fHalfH ) fRY = fHalfH;
	fKX = fRX * XGE_SHAPE_EX_KAPPA;
	fKY = fRY * XGE_SHAPE_EX_KAPPA;
	iRet = xgeShapeExMoveTo(pShape, bSvgOrigin ? fX + fRX : fX + fW, bSvgOrigin ? fY : fY + fRY);
	if ( iRet != XGE_OK ) goto fail;
	if ( bSvgOrigin && bClockwise ) {
		iRet = xgeShapeExLineTo(pShape, fX + fW - fRX, fY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fX + fW - fRX + fKX, fY, fX + fW, fY + fRY - fKY, fX + fW, fY + fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExLineTo(pShape, fX + fW, fY + fH - fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fX + fW, fY + fH - fRY + fKY, fX + fW - fRX + fKX, fY + fH, fX + fW - fRX, fY + fH);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExLineTo(pShape, fX + fRX, fY + fH);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fX + fRX - fKX, fY + fH, fX, fY + fH - fRY + fKY, fX, fY + fH - fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExLineTo(pShape, fX, fY + fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fX, fY + fRY - fKY, fX + fRX - fKX, fY, fX + fRX, fY);
		if ( iRet != XGE_OK ) goto fail;
	} else if ( bSvgOrigin ) {
		iRet = xgeShapeExCubicTo(pShape, fX + fRX - fKX, fY, fX, fY + fRY - fKY, fX, fY + fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExLineTo(pShape, fX, fY + fH - fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fX, fY + fH - fRY + fKY, fX + fRX - fKX, fY + fH, fX + fRX, fY + fH);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExLineTo(pShape, fX + fW - fRX, fY + fH);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fX + fW - fRX + fKX, fY + fH, fX + fW, fY + fH - fRY + fKY, fX + fW, fY + fH - fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExLineTo(pShape, fX + fW, fY + fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fX + fW, fY + fRY - fKY, fX + fW - fRX + fKX, fY, fX + fW - fRX, fY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExLineTo(pShape, fX + fRX, fY);
		if ( iRet != XGE_OK ) goto fail;
	} else if ( bClockwise ) {
		iRet = xgeShapeExLineTo(pShape, fX + fW, fY + fH - fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fX + fW, fY + fH - fRY + fKY, fX + fW - fRX + fKX, fY + fH, fX + fW - fRX, fY + fH);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExLineTo(pShape, fX + fRX, fY + fH);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fX + fRX - fKX, fY + fH, fX, fY + fH - fRY + fKY, fX, fY + fH - fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExLineTo(pShape, fX, fY + fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fX, fY + fRY - fKY, fX + fRX - fKX, fY, fX + fRX, fY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExLineTo(pShape, fX + fW - fRX, fY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fX + fW - fRX + fKX, fY, fX + fW, fY + fRY - fKY, fX + fW, fY + fRY);
		if ( iRet != XGE_OK ) goto fail;
	} else {
		iRet = xgeShapeExCubicTo(pShape, fX + fW, fY + fRY - fKY, fX + fW - fRX + fKX, fY, fX + fW - fRX, fY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExLineTo(pShape, fX + fRX, fY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fX + fRX - fKX, fY, fX, fY + fRY - fKY, fX, fY + fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExLineTo(pShape, fX, fY + fH - fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fX, fY + fH - fRY + fKY, fX + fRX - fKX, fY + fH, fX + fRX, fY + fH);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExLineTo(pShape, fX + fW - fRX, fY + fH);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fX + fW - fRX + fKX, fY + fH, fX + fW, fY + fH - fRY + fKY, fX + fW, fY + fH - fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExLineTo(pShape, fX + fW, fY + fRY);
		if ( iRet != XGE_OK ) goto fail;
	}
	iRet = xgeShapeExClose(pShape);
	if ( iRet != XGE_OK ) goto fail;
	return XGE_OK;
fail:
	__xgeShapeExPathRestore(pShape, &tSnapshot);
	return iRet;
}

int xgeShapeExAppendRect(xge_shape_ex pShape, float fX, float fY, float fW, float fH, float fRX, float fRY, int bClockwise)
{
	return __xgeShapeExAppendRectWithOrigin(pShape, fX, fY, fW, fH, fRX, fRY, bClockwise, 0);
}

static int __xgeShapeExAppendSvgRect(xge_shape_ex pShape, float fX, float fY, float fW, float fH, float fRX, float fRY)
{
	return __xgeShapeExAppendRectWithOrigin(pShape, fX, fY, fW, fH, fRX, fRY, 1, 1);
}

int xgeShapeExAppendCapsule(xge_shape_ex pShape, float fX, float fY, float fW, float fH, int bClockwise)
{
	float fRadius;

	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExCoordsFinite(fX, fY) ||
	     !__xgeShapeExCoordsFinite(fW, fH) || (fW < 0.0f) || (fH < 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	fRadius = (fW < fH ? fW : fH) * 0.5f;
	return xgeShapeExAppendRect(pShape, fX, fY, fW, fH, fRadius, fRadius, bClockwise);
}

static int __xgeShapeExAppendEllipseWithOrigin(xge_shape_ex pShape, float fCX, float fCY, float fRX, float fRY, int bClockwise, int bSvgOrigin)
{
	xge_shape_ex_path_snapshot_t tSnapshot = {0};
	float kx;
	float ky;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExCoordsFinite(fCX, fCY) || !__xgeShapeExCoordsFinite(fRX, fRY) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	__xgeShapeExPathSnapshot(pShape, &tSnapshot);
	kx = fRX * XGE_SHAPE_EX_KAPPA;
	ky = fRY * XGE_SHAPE_EX_KAPPA;
	if ( bSvgOrigin && bClockwise ) {
		iRet = xgeShapeExMoveTo(pShape, fCX + fRX, fCY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fCX + fRX, fCY + ky, fCX + kx, fCY + fRY, fCX, fCY + fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fCX - kx, fCY + fRY, fCX - fRX, fCY + ky, fCX - fRX, fCY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fCX - fRX, fCY - ky, fCX - kx, fCY - fRY, fCX, fCY - fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fCX + kx, fCY - fRY, fCX + fRX, fCY - ky, fCX + fRX, fCY);
		if ( iRet != XGE_OK ) goto fail;
	} else if ( bSvgOrigin ) {
		iRet = xgeShapeExMoveTo(pShape, fCX + fRX, fCY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fCX + fRX, fCY - ky, fCX + kx, fCY - fRY, fCX, fCY - fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fCX - kx, fCY - fRY, fCX - fRX, fCY - ky, fCX - fRX, fCY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fCX - fRX, fCY + ky, fCX - kx, fCY + fRY, fCX, fCY + fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fCX + kx, fCY + fRY, fCX + fRX, fCY + ky, fCX + fRX, fCY);
		if ( iRet != XGE_OK ) goto fail;
	} else if ( bClockwise ) {
		iRet = xgeShapeExMoveTo(pShape, fCX, fCY - fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fCX + kx, fCY - fRY, fCX + fRX, fCY - ky, fCX + fRX, fCY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fCX + fRX, fCY + ky, fCX + kx, fCY + fRY, fCX, fCY + fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fCX - kx, fCY + fRY, fCX - fRX, fCY + ky, fCX - fRX, fCY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fCX - fRX, fCY - ky, fCX - kx, fCY - fRY, fCX, fCY - fRY);
		if ( iRet != XGE_OK ) goto fail;
	} else {
		iRet = xgeShapeExMoveTo(pShape, fCX, fCY - fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fCX - kx, fCY - fRY, fCX - fRX, fCY - ky, fCX - fRX, fCY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fCX - fRX, fCY + ky, fCX - kx, fCY + fRY, fCX, fCY + fRY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fCX + kx, fCY + fRY, fCX + fRX, fCY + ky, fCX + fRX, fCY);
		if ( iRet != XGE_OK ) goto fail;
		iRet = xgeShapeExCubicTo(pShape, fCX + fRX, fCY - ky, fCX + kx, fCY - fRY, fCX, fCY - fRY);
		if ( iRet != XGE_OK ) goto fail;
	}
	iRet = xgeShapeExClose(pShape);
	if ( iRet != XGE_OK ) goto fail;
	return XGE_OK;
fail:
	__xgeShapeExPathRestore(pShape, &tSnapshot);
	return iRet;
}

int xgeShapeExAppendCircle(xge_shape_ex pShape, float fCX, float fCY, float fRX, float fRY, int bClockwise)
{
	return __xgeShapeExAppendEllipseWithOrigin(pShape, fCX, fCY, fRX, fRY, bClockwise, 0);
}

int xgeShapeExAppendEllipse(xge_shape_ex pShape, float fCX, float fCY, float fRX, float fRY, int bClockwise)
{
	return xgeShapeExAppendCircle(pShape, fCX, fCY, fRX, fRY, bClockwise);
}

static int __xgeShapeExAppendSvgEllipse(xge_shape_ex pShape, float fCX, float fCY, float fRX, float fRY)
{
	return __xgeShapeExAppendEllipseWithOrigin(pShape, fCX, fCY, fRX, fRY, 1, 1);
}

int xgeShapeExAppendArc(xge_shape_ex pShape, float fCX, float fCY, float fRX, float fRY, float fStartRadians, float fEndRadians)
{
	xge_shape_ex_path_snapshot_t tSnapshot = {0};
	float fSweep;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExCoordsFinite(fCX, fCY) ||
	     !__xgeShapeExCoordsFinite(fRX, fRY) || !__xgeShapeExCoordsFinite(fStartRadians, fEndRadians) ||
	     (fRX < 0.0f) || (fRY < 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	__xgeShapeExPathSnapshot(pShape, &tSnapshot);
	fSweep = __xgeShapeExClampSweep(fEndRadians - fStartRadians);
	if ( fabsf(fSweep) <= XGE_SHAPE_EX_EPSILON ) {
		return XGE_OK;
	}
	iRet = __xgeShapeExAppendEllipseArcSegments(pShape, fCX, fCY, fRX, fRY, fStartRadians, fSweep, 1);
	if ( iRet != XGE_OK ) {
		goto fail;
	}
	if ( fabsf(fSweep) >= (XGE_SHAPE_EX_TAU - XGE_SHAPE_EX_EPSILON) ) {
		iRet = xgeShapeExClose(pShape);
		if ( iRet != XGE_OK ) goto fail;
	}
	return XGE_OK;
fail:
	__xgeShapeExPathRestore(pShape, &tSnapshot);
	return iRet;
}

int xgeShapeExAppendPie(xge_shape_ex pShape, float fCX, float fCY, float fRX, float fRY, float fStartRadians, float fEndRadians)
{
	xge_shape_ex_path_snapshot_t tSnapshot = {0};
	xge_vec2_t tStart;
	float fSweep;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExCoordsFinite(fCX, fCY) ||
	     !__xgeShapeExCoordsFinite(fRX, fRY) || !__xgeShapeExCoordsFinite(fStartRadians, fEndRadians) ||
	     (fRX < 0.0f) || (fRY < 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	__xgeShapeExPathSnapshot(pShape, &tSnapshot);
	fSweep = __xgeShapeExClampSweep(fEndRadians - fStartRadians);
	if ( fabsf(fSweep) <= XGE_SHAPE_EX_EPSILON ) {
		return XGE_OK;
	}
	if ( fabsf(fSweep) >= (XGE_SHAPE_EX_TAU - XGE_SHAPE_EX_EPSILON) ) {
		iRet = xgeShapeExAppendCircle(pShape, fCX, fCY, fRX, fRY, fSweep >= 0.0f);
		if ( iRet != XGE_OK ) goto fail;
		return XGE_OK;
	}
	tStart = __xgeShapeExEllipsePoint(fCX, fCY, fRX, fRY, fStartRadians);
	iRet = xgeShapeExMoveTo(pShape, fCX, fCY);
	if ( iRet != XGE_OK ) goto fail;
	iRet = xgeShapeExLineTo(pShape, tStart.fX, tStart.fY);
	if ( iRet != XGE_OK ) goto fail;
	iRet = __xgeShapeExAppendEllipseArcSegments(pShape, fCX, fCY, fRX, fRY, fStartRadians, fSweep, 0);
	if ( iRet != XGE_OK ) goto fail;
	iRet = xgeShapeExClose(pShape);
	if ( iRet != XGE_OK ) goto fail;
	return XGE_OK;
fail:
	__xgeShapeExPathRestore(pShape, &tSnapshot);
	return iRet;
}

int xgeShapeExAppendChord(xge_shape_ex pShape, float fCX, float fCY, float fRX, float fRY, float fStartRadians, float fEndRadians)
{
	xge_shape_ex_path_snapshot_t tSnapshot = {0};
	float fSweep;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExCoordsFinite(fCX, fCY) ||
	     !__xgeShapeExCoordsFinite(fRX, fRY) || !__xgeShapeExCoordsFinite(fStartRadians, fEndRadians) ||
	     (fRX < 0.0f) || (fRY < 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	__xgeShapeExPathSnapshot(pShape, &tSnapshot);
	fSweep = __xgeShapeExClampSweep(fEndRadians - fStartRadians);
	if ( fabsf(fSweep) <= XGE_SHAPE_EX_EPSILON ) {
		return XGE_OK;
	}
	if ( fabsf(fSweep) >= (XGE_SHAPE_EX_TAU - XGE_SHAPE_EX_EPSILON) ) {
		iRet = xgeShapeExAppendCircle(pShape, fCX, fCY, fRX, fRY, fSweep >= 0.0f);
		if ( iRet != XGE_OK ) goto fail;
		return XGE_OK;
	}
	iRet = __xgeShapeExAppendEllipseArcSegments(pShape, fCX, fCY, fRX, fRY, fStartRadians, fSweep, 1);
	if ( iRet != XGE_OK ) goto fail;
	iRet = xgeShapeExClose(pShape);
	if ( iRet != XGE_OK ) goto fail;
	return XGE_OK;
fail:
	__xgeShapeExPathRestore(pShape, &tSnapshot);
	return iRet;
}

int xgeShapeExAppendPath(xge_shape_ex pShape, const uint8_t* pCommands, int iCommandCount, const xge_vec2_t* pPoints, int iPointCount)
{
	xge_vec2_t tCurrent;
	xge_vec2_t tStart;
	xge_vec2_t tLastCubicControl;
	xge_vec2_t tLastQuadControl;
	int bHasCurrent;
	int bHasStart;
	int iPointRead;
	int iPointNeeded;
	int bValidateHasCurrent;
	int bValidateHasStart;
	int i;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || (pCommands == NULL) || (iCommandCount <= 0) || (pPoints == NULL) || (iPointCount <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iPointNeeded = 0;
	for ( i = 0; i < iCommandCount; i++ ) {
		int iCount = __xgeShapeExCommandPointCount(pCommands[i]);

		if ( iCount < 0 ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		if ( iPointNeeded > (INT_MAX - iCount) ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		iPointNeeded += iCount;
	}
	if ( iPointNeeded != iPointCount ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < iPointCount; i++ ) {
		if ( !__xgeShapeExPointFinite(pPoints[i]) ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
	}
	bValidateHasCurrent = pShape->bHasCurrent;
	bValidateHasStart = pShape->bHasStart;
	for ( i = 0; i < iCommandCount; i++ ) {
		switch ( pCommands[i] ) {
			case XGE_SHAPE_EX_CMD_CLOSE:
				if ( bValidateHasStart ) {
					bValidateHasCurrent = 1;
				}
				break;
			case XGE_SHAPE_EX_CMD_MOVE_TO:
				bValidateHasCurrent = 1;
				bValidateHasStart = 1;
				break;
			case XGE_SHAPE_EX_CMD_LINE_TO:
				if ( !bValidateHasCurrent ) {
					bValidateHasCurrent = 1;
					bValidateHasStart = 1;
				} else if ( !bValidateHasStart ) {
					bValidateHasStart = 1;
				}
				break;
			case XGE_SHAPE_EX_CMD_QUAD_TO:
				if ( !bValidateHasCurrent ) {
					return XGE_ERROR_INVALID_ARGUMENT;
				}
				if ( !bValidateHasStart ) {
					bValidateHasStart = 1;
				}
				break;
			case XGE_SHAPE_EX_CMD_CUBIC_TO:
				if ( !bValidateHasCurrent ) {
					bValidateHasCurrent = 1;
					break;
				}
				if ( !bValidateHasStart ) {
					bValidateHasStart = 1;
				}
				break;
			default:
				return XGE_ERROR_INVALID_ARGUMENT;
		}
	}
	iRet = __xgeShapeExReserveCommands(pShape, pShape->iCommandCount + iCommandCount);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = __xgeShapeExReservePoints(pShape, pShape->iPointCount + iPointCount);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	tCurrent = pShape->tCurrent;
	tStart = pShape->tStart;
	tLastCubicControl = pShape->tLastCubicControl;
	tLastQuadControl = pShape->tLastQuadControl;
	bHasCurrent = pShape->bHasCurrent;
	bHasStart = pShape->bHasStart;
	iPointRead = 0;
	for ( i = 0; i < iCommandCount; i++ ) {
		switch ( pCommands[i] ) {
			case XGE_SHAPE_EX_CMD_CLOSE:
				if ( bHasStart ) {
					tCurrent = tStart;
					tLastCubicControl = tCurrent;
					tLastQuadControl = tCurrent;
					bHasCurrent = 1;
				}
				break;
			case XGE_SHAPE_EX_CMD_MOVE_TO:
				tCurrent = pPoints[iPointRead++];
				tStart = tCurrent;
				tLastCubicControl = tCurrent;
				tLastQuadControl = tCurrent;
				bHasCurrent = 1;
				bHasStart = 1;
				break;
			case XGE_SHAPE_EX_CMD_LINE_TO:
			{
				xge_vec2_t tPrev = tCurrent;

				tCurrent = pPoints[iPointRead++];
				tLastCubicControl = tCurrent;
				tLastQuadControl = tCurrent;
				if ( !bHasCurrent ) {
					tStart = tCurrent;
					bHasStart = 1;
				}
				bHasCurrent = 1;
				if ( !bHasStart ) {
					tStart = tPrev;
					bHasStart = 1;
				}
				break;
			}
			case XGE_SHAPE_EX_CMD_QUAD_TO:
				if ( !bHasCurrent ) {
					return XGE_ERROR_INVALID_ARGUMENT;
				}
			{
				xge_vec2_t tPrev = tCurrent;

				tLastQuadControl = pPoints[iPointRead];
				tCurrent = pPoints[iPointRead + 1];
				tLastCubicControl = tCurrent;
				iPointRead += 2;
				bHasCurrent = 1;
				if ( !bHasStart ) {
					tStart = tPrev;
					bHasStart = 1;
				}
				break;
			}
			case XGE_SHAPE_EX_CMD_CUBIC_TO:
			{
				xge_vec2_t tPrev = tCurrent;

				if ( !bHasCurrent ) {
					tLastCubicControl = pPoints[iPointRead + 1];
					tLastQuadControl = pPoints[iPointRead + 2];
					tCurrent = pPoints[iPointRead + 2];
					iPointRead += 3;
					bHasCurrent = 1;
					break;
				}
				tLastCubicControl = pPoints[iPointRead + 1];
				tLastQuadControl = pPoints[iPointRead + 2];
				tCurrent = pPoints[iPointRead + 2];
				iPointRead += 3;
				bHasCurrent = 1;
				if ( !bHasStart ) {
					tStart = tPrev;
					bHasStart = 1;
				}
				break;
			}
			default:
				return XGE_ERROR_INVALID_ARGUMENT;
		}
		pShape->pCommands[pShape->iCommandCount + i] = pCommands[i];
	}
	if ( iPointCount > 0 ) {
		memcpy(pShape->pPoints + pShape->iPointCount, pPoints, (size_t)iPointCount * sizeof(*pPoints));
	}
	pShape->iCommandCount += iCommandCount;
	pShape->iPointCount += iPointCount;
	pShape->tCurrent = tCurrent;
	pShape->tStart = tStart;
	pShape->tLastCubicControl = tLastCubicControl;
	pShape->tLastQuadControl = tLastQuadControl;
	pShape->bHasCurrent = bHasCurrent;
	pShape->bHasStart = bHasStart;
	return XGE_OK;
}

typedef int (*xge_shape_ex_svg_path_event_proc)(void* pUser, uint8_t iCommand, xge_vec2_t tStart, xge_vec2_t tControl1, xge_vec2_t tControl2, xge_vec2_t tEnd);

static int __xgeShapeExSvgPathEvent(xge_shape_ex_svg_path_event_proc pEvent, void* pUser, uint8_t iCommand, xge_vec2_t tStart, xge_vec2_t tControl1, xge_vec2_t tControl2, xge_vec2_t tEnd)
{
	if ( pEvent == NULL ) {
		return XGE_OK;
	}
	return pEvent(pUser, iCommand, tStart, tControl1, tControl2, tEnd);
}

static int __xgeShapeExSvgPathArcEvent(xge_shape_ex pShape, int iCommandBefore, int iPointBefore, xge_vec2_t tStart, xge_shape_ex_svg_path_event_proc pEvent, void* pUser)
{
	xge_vec2_t tZero = {0.0f, 0.0f};
	xge_vec2_t tFirstC1 = {0.0f, 0.0f};
	xge_vec2_t tLastC2 = {0.0f, 0.0f};
	xge_vec2_t tEnd = {0.0f, 0.0f};
	int iPointRead;
	int bHasCubic;
	int i;

	if ( pEvent == NULL ) {
		return XGE_OK;
	}
	if ( !__xgeShapeExValid(pShape) || (iCommandBefore < 0) || (iPointBefore < 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pShape->iCommandCount <= iCommandBefore ) {
		return XGE_OK;
	}
	iPointRead = iPointBefore;
	bHasCubic = 0;
	for ( i = iCommandBefore; i < pShape->iCommandCount; i++ ) {
		uint8_t iCommand = pShape->pCommands[i];
		int iPointCount = __xgeShapeExCommandPointCount(iCommand);

		if ( iPointCount < 0 ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		if ( (iPointRead + iPointCount) > pShape->iPointCount ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		if ( iCommand == XGE_SHAPE_EX_CMD_LINE_TO ) {
			return __xgeShapeExSvgPathEvent(pEvent, pUser, XGE_SHAPE_EX_CMD_LINE_TO, tStart, tZero, tZero, pShape->pPoints[iPointRead]);
		}
		if ( iCommand == XGE_SHAPE_EX_CMD_CUBIC_TO ) {
			if ( !bHasCubic ) {
				tFirstC1 = pShape->pPoints[iPointRead];
				bHasCubic = 1;
			}
			tLastC2 = pShape->pPoints[iPointRead + 1];
			tEnd = pShape->pPoints[iPointRead + 2];
		}
		iPointRead += iPointCount;
	}
	if ( bHasCubic ) {
		return __xgeShapeExSvgPathEvent(pEvent, pUser, XGE_SHAPE_EX_CMD_CUBIC_TO, tStart, tFirstC1, tLastC2, tEnd);
	}
	return XGE_OK;
}

static int __xgeShapeExAppendSvgPathNoRollback(xge_shape_ex pShape, const char* sPath, xge_shape_ex_svg_path_event_proc pEvent, void* pUser)
{
	const char* pText;
	char cCommand;
	char cPrevCommand;
	int bRelative;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || (sPath == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pText = sPath;
	cCommand = 0;
	cPrevCommand = 0;
	while ( 1 ) {
		__xgeShapeExSvgSkipSeparators(&pText);
		if ( *pText == '\0' ) {
			break;
		}
		if ( __xgeShapeExSvgIsCommand(*pText) ) {
			cCommand = *pText++;
		} else if ( cCommand == 0 ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		bRelative = (cCommand >= 'a') && (cCommand <= 'z');
		if ( (__xgeShapeExSvgLower(cCommand) != 'm') && !pShape->bHasCurrent ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		switch ( __xgeShapeExSvgLower(cCommand) ) {
			case 'm': {
				float fX;
				float fY;
				int bFirst = 1;
				while ( __xgeShapeExSvgHasNumber(pText) ) {
					if ( !__xgeShapeExSvgReadNumber(&pText, &fX) || !__xgeShapeExSvgReadNumber(&pText, &fY) ) return XGE_ERROR_INVALID_ARGUMENT;
					if ( bRelative && pShape->bHasCurrent ) {
						fX += pShape->tCurrent.fX;
						fY += pShape->tCurrent.fY;
					}
					if ( bFirst ) {
						iRet = xgeShapeExMoveTo(pShape, fX, fY);
						if ( iRet != XGE_OK ) return iRet;
						iRet = __xgeShapeExSvgPathEvent(pEvent, pUser, XGE_SHAPE_EX_CMD_MOVE_TO, pShape->tCurrent, (xge_vec2_t){0.0f, 0.0f}, (xge_vec2_t){0.0f, 0.0f}, pShape->tCurrent);
						if ( iRet != XGE_OK ) return iRet;
						bFirst = 0;
					} else {
						xge_vec2_t tStart = pShape->tCurrent;
						iRet = xgeShapeExLineTo(pShape, fX, fY);
						if ( iRet != XGE_OK ) return iRet;
						iRet = __xgeShapeExSvgPathEvent(pEvent, pUser, XGE_SHAPE_EX_CMD_LINE_TO, tStart, (xge_vec2_t){0.0f, 0.0f}, (xge_vec2_t){0.0f, 0.0f}, pShape->tCurrent);
						if ( iRet != XGE_OK ) return iRet;
					}
				}
				if ( bFirst ) {
					return XGE_ERROR_INVALID_ARGUMENT;
				}
				cPrevCommand = cCommand;
				cCommand = bRelative ? 'l' : 'L';
				break;
			}
			case 'l': {
				float fX;
				float fY;
				int bConsumed = 0;
				while ( __xgeShapeExSvgHasNumber(pText) ) {
					if ( !__xgeShapeExSvgReadNumber(&pText, &fX) || !__xgeShapeExSvgReadNumber(&pText, &fY) ) return XGE_ERROR_INVALID_ARGUMENT;
					if ( bRelative ) {
						fX += pShape->tCurrent.fX;
						fY += pShape->tCurrent.fY;
					}
					{
						xge_vec2_t tStart = pShape->tCurrent;
						iRet = xgeShapeExLineTo(pShape, fX, fY);
						if ( iRet != XGE_OK ) return iRet;
						iRet = __xgeShapeExSvgPathEvent(pEvent, pUser, XGE_SHAPE_EX_CMD_LINE_TO, tStart, (xge_vec2_t){0.0f, 0.0f}, (xge_vec2_t){0.0f, 0.0f}, pShape->tCurrent);
						if ( iRet != XGE_OK ) return iRet;
					}
					bConsumed = 1;
				}
				if ( !bConsumed ) return XGE_ERROR_INVALID_ARGUMENT;
				cPrevCommand = cCommand;
				break;
			}
			case 'h': {
				float fX;
				int bConsumed = 0;
				while ( __xgeShapeExSvgHasNumber(pText) ) {
					if ( !__xgeShapeExSvgReadNumber(&pText, &fX) ) return XGE_ERROR_INVALID_ARGUMENT;
					if ( bRelative ) fX += pShape->tCurrent.fX;
					{
						xge_vec2_t tStart = pShape->tCurrent;
						iRet = xgeShapeExLineTo(pShape, fX, pShape->tCurrent.fY);
						if ( iRet != XGE_OK ) return iRet;
						iRet = __xgeShapeExSvgPathEvent(pEvent, pUser, XGE_SHAPE_EX_CMD_LINE_TO, tStart, (xge_vec2_t){0.0f, 0.0f}, (xge_vec2_t){0.0f, 0.0f}, pShape->tCurrent);
						if ( iRet != XGE_OK ) return iRet;
					}
					bConsumed = 1;
				}
				if ( !bConsumed ) return XGE_ERROR_INVALID_ARGUMENT;
				cPrevCommand = cCommand;
				break;
			}
			case 'v': {
				float fY;
				int bConsumed = 0;
				while ( __xgeShapeExSvgHasNumber(pText) ) {
					if ( !__xgeShapeExSvgReadNumber(&pText, &fY) ) return XGE_ERROR_INVALID_ARGUMENT;
					if ( bRelative ) fY += pShape->tCurrent.fY;
					{
						xge_vec2_t tStart = pShape->tCurrent;
						iRet = xgeShapeExLineTo(pShape, pShape->tCurrent.fX, fY);
						if ( iRet != XGE_OK ) return iRet;
						iRet = __xgeShapeExSvgPathEvent(pEvent, pUser, XGE_SHAPE_EX_CMD_LINE_TO, tStart, (xge_vec2_t){0.0f, 0.0f}, (xge_vec2_t){0.0f, 0.0f}, pShape->tCurrent);
						if ( iRet != XGE_OK ) return iRet;
					}
					bConsumed = 1;
				}
				if ( !bConsumed ) return XGE_ERROR_INVALID_ARGUMENT;
				cPrevCommand = cCommand;
				break;
			}
			case 'c': {
				float fC1X, fC1Y, fC2X, fC2Y, fX, fY;
				int bConsumed = 0;
				while ( __xgeShapeExSvgHasNumber(pText) ) {
					if ( !__xgeShapeExSvgReadNumber(&pText, &fC1X) || !__xgeShapeExSvgReadNumber(&pText, &fC1Y) ||
					     !__xgeShapeExSvgReadNumber(&pText, &fC2X) || !__xgeShapeExSvgReadNumber(&pText, &fC2Y) ||
					     !__xgeShapeExSvgReadNumber(&pText, &fX) || !__xgeShapeExSvgReadNumber(&pText, &fY) ) return XGE_ERROR_INVALID_ARGUMENT;
					if ( bRelative ) {
						fC1X += pShape->tCurrent.fX; fC1Y += pShape->tCurrent.fY;
						fC2X += pShape->tCurrent.fX; fC2Y += pShape->tCurrent.fY;
						fX += pShape->tCurrent.fX; fY += pShape->tCurrent.fY;
					}
					{
						xge_vec2_t tStart = pShape->tCurrent;
						iRet = xgeShapeExCubicTo(pShape, fC1X, fC1Y, fC2X, fC2Y, fX, fY);
						if ( iRet != XGE_OK ) return iRet;
						iRet = __xgeShapeExSvgPathEvent(pEvent, pUser, XGE_SHAPE_EX_CMD_CUBIC_TO, tStart, (xge_vec2_t){fC1X, fC1Y}, (xge_vec2_t){fC2X, fC2Y}, pShape->tCurrent);
						if ( iRet != XGE_OK ) return iRet;
					}
					bConsumed = 1;
				}
				if ( !bConsumed ) return XGE_ERROR_INVALID_ARGUMENT;
				cPrevCommand = cCommand;
				break;
			}
			case 's': {
				float fC1X, fC1Y, fC2X, fC2Y, fX, fY;
				int bConsumed = 0;
				while ( __xgeShapeExSvgHasNumber(pText) ) {
					if ( !__xgeShapeExSvgReadNumber(&pText, &fC2X) || !__xgeShapeExSvgReadNumber(&pText, &fC2Y) ||
					     !__xgeShapeExSvgReadNumber(&pText, &fX) || !__xgeShapeExSvgReadNumber(&pText, &fY) ) return XGE_ERROR_INVALID_ARGUMENT;
					if ( (__xgeShapeExSvgLower(cPrevCommand) == 'c') || (__xgeShapeExSvgLower(cPrevCommand) == 's') ) {
						fC1X = (2.0f * pShape->tCurrent.fX) - pShape->tLastCubicControl.fX;
						fC1Y = (2.0f * pShape->tCurrent.fY) - pShape->tLastCubicControl.fY;
					} else {
						fC1X = pShape->tCurrent.fX;
						fC1Y = pShape->tCurrent.fY;
					}
					if ( bRelative ) {
						fC2X += pShape->tCurrent.fX; fC2Y += pShape->tCurrent.fY;
						fX += pShape->tCurrent.fX; fY += pShape->tCurrent.fY;
					}
					{
						xge_vec2_t tStart = pShape->tCurrent;
						iRet = xgeShapeExCubicTo(pShape, fC1X, fC1Y, fC2X, fC2Y, fX, fY);
						if ( iRet != XGE_OK ) return iRet;
						iRet = __xgeShapeExSvgPathEvent(pEvent, pUser, XGE_SHAPE_EX_CMD_CUBIC_TO, tStart, (xge_vec2_t){fC1X, fC1Y}, (xge_vec2_t){fC2X, fC2Y}, pShape->tCurrent);
						if ( iRet != XGE_OK ) return iRet;
					}
					cPrevCommand = cCommand;
					bConsumed = 1;
				}
				if ( !bConsumed ) return XGE_ERROR_INVALID_ARGUMENT;
				cPrevCommand = cCommand;
				break;
			}
			case 'q': {
				float fCX, fCY, fX, fY;
				int bConsumed = 0;
				while ( __xgeShapeExSvgHasNumber(pText) ) {
					if ( !__xgeShapeExSvgReadNumber(&pText, &fCX) || !__xgeShapeExSvgReadNumber(&pText, &fCY) ||
					     !__xgeShapeExSvgReadNumber(&pText, &fX) || !__xgeShapeExSvgReadNumber(&pText, &fY) ) return XGE_ERROR_INVALID_ARGUMENT;
					if ( bRelative ) {
						fCX += pShape->tCurrent.fX; fCY += pShape->tCurrent.fY;
						fX += pShape->tCurrent.fX; fY += pShape->tCurrent.fY;
					}
					{
						xge_vec2_t tStart = pShape->tCurrent;
						iRet = xgeShapeExQuadTo(pShape, fCX, fCY, fX, fY);
						if ( iRet != XGE_OK ) return iRet;
						iRet = __xgeShapeExSvgPathEvent(pEvent, pUser, XGE_SHAPE_EX_CMD_QUAD_TO, tStart, (xge_vec2_t){fCX, fCY}, (xge_vec2_t){0.0f, 0.0f}, pShape->tCurrent);
						if ( iRet != XGE_OK ) return iRet;
					}
					bConsumed = 1;
				}
				if ( !bConsumed ) return XGE_ERROR_INVALID_ARGUMENT;
				cPrevCommand = cCommand;
				break;
			}
			case 't': {
				float fCX, fCY, fX, fY;
				int bConsumed = 0;
				while ( __xgeShapeExSvgHasNumber(pText) ) {
					if ( !__xgeShapeExSvgReadNumber(&pText, &fX) || !__xgeShapeExSvgReadNumber(&pText, &fY) ) return XGE_ERROR_INVALID_ARGUMENT;
					if ( (__xgeShapeExSvgLower(cPrevCommand) == 'q') || (__xgeShapeExSvgLower(cPrevCommand) == 't') ) {
						fCX = (2.0f * pShape->tCurrent.fX) - pShape->tLastQuadControl.fX;
						fCY = (2.0f * pShape->tCurrent.fY) - pShape->tLastQuadControl.fY;
					} else {
						fCX = pShape->tCurrent.fX;
						fCY = pShape->tCurrent.fY;
					}
					if ( bRelative ) {
						fX += pShape->tCurrent.fX;
						fY += pShape->tCurrent.fY;
					}
					{
						xge_vec2_t tStart = pShape->tCurrent;
						iRet = xgeShapeExQuadTo(pShape, fCX, fCY, fX, fY);
						if ( iRet != XGE_OK ) return iRet;
						iRet = __xgeShapeExSvgPathEvent(pEvent, pUser, XGE_SHAPE_EX_CMD_QUAD_TO, tStart, (xge_vec2_t){fCX, fCY}, (xge_vec2_t){0.0f, 0.0f}, pShape->tCurrent);
						if ( iRet != XGE_OK ) return iRet;
					}
					cPrevCommand = cCommand;
					bConsumed = 1;
				}
				if ( !bConsumed ) return XGE_ERROR_INVALID_ARGUMENT;
				cPrevCommand = cCommand;
				break;
			}
			case 'a': {
				float fRX, fRY, fAxis, fX, fY;
				int bLargeArcFlag;
				int bSweepFlag;
				int bConsumed = 0;
				while ( __xgeShapeExSvgHasNumber(pText) ) {
					int iArcRet;
					if ( !__xgeShapeExSvgReadNumber(&pText, &fRX) || !__xgeShapeExSvgReadNumber(&pText, &fRY) ||
					     !__xgeShapeExSvgReadNumber(&pText, &fAxis) || !__xgeShapeExSvgReadFlag(&pText, &bLargeArcFlag) ||
					     !__xgeShapeExSvgReadFlag(&pText, &bSweepFlag) || !__xgeShapeExSvgReadNumber(&pText, &fX) ||
					     !__xgeShapeExSvgReadNumber(&pText, &fY) ) return XGE_ERROR_INVALID_ARGUMENT;
					if ( bRelative ) {
						fX += pShape->tCurrent.fX;
						fY += pShape->tCurrent.fY;
					}
					{
						xge_vec2_t tStart = pShape->tCurrent;
						int iCommandBefore = pShape->iCommandCount;
						int iPointBefore = pShape->iPointCount;
						iArcRet = xgeShapeExArcTo(pShape, fRX, fRY, fAxis, bLargeArcFlag, bSweepFlag, fX, fY);
						if ( iArcRet != XGE_OK ) {
							return iArcRet;
						}
						iRet = __xgeShapeExSvgPathArcEvent(pShape, iCommandBefore, iPointBefore, tStart, pEvent, pUser);
						if ( iRet != XGE_OK ) return iRet;
					}
					bConsumed = 1;
				}
				if ( !bConsumed ) return XGE_ERROR_INVALID_ARGUMENT;
				cPrevCommand = cCommand;
				break;
			}
			case 'z':
			{
				xge_vec2_t tStart = pShape->tCurrent;
				xge_vec2_t tEnd = pShape->tStart;
				iRet = xgeShapeExClose(pShape);
				if ( iRet != XGE_OK ) return iRet;
				iRet = __xgeShapeExSvgPathEvent(pEvent, pUser, XGE_SHAPE_EX_CMD_CLOSE, tStart, (xge_vec2_t){0.0f, 0.0f}, (xge_vec2_t){0.0f, 0.0f}, tEnd);
				if ( iRet != XGE_OK ) return iRet;
				cPrevCommand = cCommand;
				cCommand = 0;
				break;
			}
			default:
				return XGE_ERROR_INVALID_ARGUMENT;
		}
	}
	return XGE_OK;
}

int xgeShapeExAppendSvgPath(xge_shape_ex pShape, const char* sPath)
{
	xge_shape_ex_path_snapshot_t tSnapshot = {0};
	int iRet;

	if ( !__xgeShapeExValid(pShape) || (sPath == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	__xgeShapeExPathSnapshot(pShape, &tSnapshot);
	iRet = __xgeShapeExAppendSvgPathNoRollback(pShape, sPath, NULL, NULL);
	return __xgeShapeExSvgPathFinish(pShape, &tSnapshot, iRet);
}

int xgeShapeExGetPath(xge_shape_ex pShape, const uint8_t** ppCommands, int* pCommandCount, const xge_vec2_t** ppPoints, int* pPointCount)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( ppCommands != NULL ) {
		*ppCommands = pShape->pCommands;
	}
	if ( pCommandCount != NULL ) {
		*pCommandCount = pShape->iCommandCount;
	}
	if ( ppPoints != NULL ) {
		*ppPoints = pShape->pPoints;
	}
	if ( pPointCount != NULL ) {
		*pPointCount = pShape->iPointCount;
	}
	return XGE_OK;
}

static int __xgeShapeExSvgPathAppendFormat(char* sBuffer, int iBufferSize, int* pLength, const char* sFormat, ...)
{
	va_list args;
	int iWritten;

	if ( (pLength == NULL) || (sFormat == NULL) || (*pLength < 0) || (iBufferSize < 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	va_start(args, sFormat);
	if ( (sBuffer != NULL) && (*pLength < iBufferSize) ) {
		iWritten = vsnprintf(sBuffer + *pLength, (size_t)(iBufferSize - *pLength), sFormat, args);
	} else {
		iWritten = vsnprintf(NULL, 0, sFormat, args);
	}
	va_end(args);
	if ( iWritten < 0 ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iWritten > INT_MAX - *pLength ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pLength += iWritten;
	return XGE_OK;
}

int xgeShapeExGetSvgPathData(xge_shape_ex pShape, char* sBuffer, int iBufferSize, int* pRequiredSize)
{
	int iPointRead;
	int iLength;
	int i;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || (iBufferSize < 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (sBuffer != NULL) && (iBufferSize > 0) ) {
		sBuffer[0] = '\0';
	}
	iPointRead = 0;
	iLength = 0;
	for ( i = 0; i < pShape->iCommandCount; i++ ) {
		uint8_t iCommand = pShape->pCommands[i];

		if ( i > 0 ) {
			iRet = __xgeShapeExSvgPathAppendFormat(sBuffer, iBufferSize, &iLength, " ");
			if ( iRet != XGE_OK ) return iRet;
		}
		if ( iCommand == XGE_SHAPE_EX_CMD_MOVE_TO ) {
			if ( iPointRead + 1 > pShape->iPointCount ) return XGE_ERROR_INVALID_ARGUMENT;
			iRet = __xgeShapeExSvgPathAppendFormat(sBuffer, iBufferSize, &iLength, "M%.9g %.9g", pShape->pPoints[iPointRead].fX, pShape->pPoints[iPointRead].fY);
			iPointRead++;
		} else if ( iCommand == XGE_SHAPE_EX_CMD_LINE_TO ) {
			if ( iPointRead + 1 > pShape->iPointCount ) return XGE_ERROR_INVALID_ARGUMENT;
			iRet = __xgeShapeExSvgPathAppendFormat(sBuffer, iBufferSize, &iLength, "L%.9g %.9g", pShape->pPoints[iPointRead].fX, pShape->pPoints[iPointRead].fY);
			iPointRead++;
		} else if ( iCommand == XGE_SHAPE_EX_CMD_QUAD_TO ) {
			if ( iPointRead + 2 > pShape->iPointCount ) return XGE_ERROR_INVALID_ARGUMENT;
			iRet = __xgeShapeExSvgPathAppendFormat(sBuffer, iBufferSize, &iLength, "Q%.9g %.9g %.9g %.9g",
				pShape->pPoints[iPointRead].fX, pShape->pPoints[iPointRead].fY,
				pShape->pPoints[iPointRead + 1].fX, pShape->pPoints[iPointRead + 1].fY);
			iPointRead += 2;
		} else if ( iCommand == XGE_SHAPE_EX_CMD_CUBIC_TO ) {
			if ( iPointRead + 3 > pShape->iPointCount ) return XGE_ERROR_INVALID_ARGUMENT;
			iRet = __xgeShapeExSvgPathAppendFormat(sBuffer, iBufferSize, &iLength, "C%.9g %.9g %.9g %.9g %.9g %.9g",
				pShape->pPoints[iPointRead].fX, pShape->pPoints[iPointRead].fY,
				pShape->pPoints[iPointRead + 1].fX, pShape->pPoints[iPointRead + 1].fY,
				pShape->pPoints[iPointRead + 2].fX, pShape->pPoints[iPointRead + 2].fY);
			iPointRead += 3;
		} else if ( iCommand == XGE_SHAPE_EX_CMD_CLOSE ) {
			iRet = __xgeShapeExSvgPathAppendFormat(sBuffer, iBufferSize, &iLength, "Z");
		} else {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		if ( iRet != XGE_OK ) {
			return iRet;
		}
	}
	if ( iPointRead != pShape->iPointCount ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (sBuffer != NULL) && (iBufferSize > 0) ) {
		if ( iLength >= iBufferSize ) {
			sBuffer[iBufferSize - 1] = '\0';
		} else {
			sBuffer[iLength] = '\0';
		}
	}
	if ( pRequiredSize != NULL ) {
		*pRequiredSize = iLength + 1;
	}
	if ( (sBuffer != NULL) && (iBufferSize < iLength + 1) ) {
		return XGE_ERROR_BUFFER_TOO_SMALL;
	}
	return XGE_OK;
}

int xgeShapeExFillColor(xge_shape_ex pShape, uint32_t iColor)
{
	if ( !__xgeShapeExValid(pShape) ) return XGE_ERROR_INVALID_ARGUMENT;
	pShape->iFillType = XGE_SHAPE_EX_FILL_SOLID;
	pShape->iFillColor = iColor;
	return XGE_OK;
}

int xgeShapeExFillTypeGet(xge_shape_ex pShape, int* pType)
{
	if ( !__xgeShapeExValid(pShape) || (pType == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pType = pShape->iFillType;
	return XGE_OK;
}

int xgeShapeExFillColorGet(xge_shape_ex pShape, uint32_t* pColor)
{
	if ( !__xgeShapeExValid(pShape) || (pColor == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pColor = pShape->iFillColor;
	return XGE_OK;
}

int xgeShapeExFillLinearGradient(xge_shape_ex pShape, float fX1, float fY1, float fX2, float fY2, int iUnits, const xge_shape_ex_color_stop_t* pStops, int iStopCount)
{
	int i;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExCoordsFinite(fX1, fY1) || !__xgeShapeExCoordsFinite(fX2, fY2) ||
	     !__xgeShapeExStopsFinite(pStops, iStopCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iUnits != XGE_SHAPE_EX_GRADIENT_USER_SPACE) && (iUnits != XGE_SHAPE_EX_GRADIENT_OBJECT_BOUNDING_BOX) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeShapeExReserveFillStops(pShape, iStopCount);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	for ( i = 0; i < iStopCount; i++ ) {
		pShape->pFillSourceStops[i] = pStops[i];
		pShape->pFillStops[i] = pStops[i];
		if ( pShape->pFillStops[i].fOffset < 0.0f ) pShape->pFillStops[i].fOffset = 0.0f;
		if ( pShape->pFillStops[i].fOffset > 1.0f ) pShape->pFillStops[i].fOffset = 1.0f;
	}
	__xgeShapeExNormalizeStops(pShape->pFillStops, iStopCount);
	pShape->iFillStopCount = iStopCount;
	pShape->iFillType = XGE_SHAPE_EX_FILL_LINEAR_GRADIENT;
	pShape->fFillX1 = fX1;
	pShape->fFillY1 = fY1;
	pShape->fFillX2 = fX2;
	pShape->fFillY2 = fY2;
	pShape->iFillGradientUnits = iUnits;
	pShape->iFillGradientSpread = XGE_SHAPE_EX_GRADIENT_SPREAD_PAD;
	pShape->tFillGradientTransform = __xgeShapeExMatrixIdentity();
	return XGE_OK;
}

int xgeShapeExFillLinearGradientGet(xge_shape_ex pShape, float* pX1, float* pY1, float* pX2, float* pY2, int* pUnits, const xge_shape_ex_color_stop_t** ppStops, int* pStopCount)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pShape->iFillType != XGE_SHAPE_EX_FILL_LINEAR_GRADIENT ) {
		return XGE_ERROR_NOT_FOUND;
	}
	if ( pX1 != NULL ) *pX1 = pShape->fFillX1;
	if ( pY1 != NULL ) *pY1 = pShape->fFillY1;
	if ( pX2 != NULL ) *pX2 = pShape->fFillX2;
	if ( pY2 != NULL ) *pY2 = pShape->fFillY2;
	if ( pUnits != NULL ) *pUnits = pShape->iFillGradientUnits;
	if ( ppStops != NULL ) *ppStops = pShape->pFillSourceStops;
	if ( pStopCount != NULL ) *pStopCount = pShape->iFillStopCount;
	return XGE_OK;
}

int xgeShapeExFillRadialGradient(xge_shape_ex pShape, float fCX, float fCY, float fRadius, float fFX, float fFY, int iUnits, const xge_shape_ex_color_stop_t* pStops, int iStopCount)
{
	return xgeShapeExFillRadialGradientEx(pShape, fCX, fCY, fRadius, fFX, fFY, 0.0f, iUnits, pStops, iStopCount);
}

int xgeShapeExFillRadialGradientEx(xge_shape_ex pShape, float fCX, float fCY, float fRadius, float fFX, float fFY, float fFocalRadius, int iUnits, const xge_shape_ex_color_stop_t* pStops, int iStopCount)
{
	int i;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExCoordsFinite(fCX, fCY) ||
	     !__xgeShapeExCoordsFinite(fRadius, fFocalRadius) || !__xgeShapeExCoordsFinite(fFX, fFY) ||
	     !__xgeShapeExStopsFinite(pStops, iStopCount) || (fRadius < 0.0f) || (fFocalRadius < 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iUnits != XGE_SHAPE_EX_GRADIENT_USER_SPACE) && (iUnits != XGE_SHAPE_EX_GRADIENT_OBJECT_BOUNDING_BOX) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeShapeExReserveFillStops(pShape, iStopCount);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	for ( i = 0; i < iStopCount; i++ ) {
		pShape->pFillSourceStops[i] = pStops[i];
		pShape->pFillStops[i] = pStops[i];
		if ( pShape->pFillStops[i].fOffset < 0.0f ) pShape->pFillStops[i].fOffset = 0.0f;
		if ( pShape->pFillStops[i].fOffset > 1.0f ) pShape->pFillStops[i].fOffset = 1.0f;
	}
	__xgeShapeExNormalizeStops(pShape->pFillStops, iStopCount);
	pShape->iFillStopCount = iStopCount;
	pShape->iFillType = XGE_SHAPE_EX_FILL_RADIAL_GRADIENT;
	pShape->fFillX1 = fCX;
	pShape->fFillY1 = fCY;
	pShape->fFillX2 = fCX + fRadius;
	pShape->fFillY2 = fCY;
	pShape->fFillR = fRadius;
	pShape->fFillFX = fFX;
	pShape->fFillFY = fFY;
	pShape->fFillFR = fFocalRadius;
	pShape->iFillGradientUnits = iUnits;
	pShape->iFillGradientSpread = XGE_SHAPE_EX_GRADIENT_SPREAD_PAD;
	pShape->tFillGradientTransform = __xgeShapeExMatrixIdentity();
	return XGE_OK;
}

int xgeShapeExFillRadialGradientGet(xge_shape_ex pShape, float* pCX, float* pCY, float* pRadius, float* pFX, float* pFY, int* pUnits, const xge_shape_ex_color_stop_t** ppStops, int* pStopCount)
{
	return xgeShapeExFillRadialGradientGetEx(pShape, pCX, pCY, pRadius, pFX, pFY, NULL, pUnits, ppStops, pStopCount);
}

int xgeShapeExFillRadialGradientGetEx(xge_shape_ex pShape, float* pCX, float* pCY, float* pRadius, float* pFX, float* pFY, float* pFocalRadius, int* pUnits, const xge_shape_ex_color_stop_t** ppStops, int* pStopCount)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pShape->iFillType != XGE_SHAPE_EX_FILL_RADIAL_GRADIENT ) {
		return XGE_ERROR_NOT_FOUND;
	}
	if ( pCX != NULL ) *pCX = pShape->fFillX1;
	if ( pCY != NULL ) *pCY = pShape->fFillY1;
	if ( pRadius != NULL ) *pRadius = pShape->fFillR;
	if ( pFX != NULL ) *pFX = pShape->fFillFX;
	if ( pFY != NULL ) *pFY = pShape->fFillFY;
	if ( pFocalRadius != NULL ) *pFocalRadius = pShape->fFillFR;
	if ( pUnits != NULL ) *pUnits = pShape->iFillGradientUnits;
	if ( ppStops != NULL ) *ppStops = pShape->pFillSourceStops;
	if ( pStopCount != NULL ) *pStopCount = pShape->iFillStopCount;
	return XGE_OK;
}

int xgeShapeExFillGradientSpread(xge_shape_ex pShape, int iSpread)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iSpread != XGE_SHAPE_EX_GRADIENT_SPREAD_PAD) &&
	     (iSpread != XGE_SHAPE_EX_GRADIENT_SPREAD_REFLECT) &&
	     (iSpread != XGE_SHAPE_EX_GRADIENT_SPREAD_REPEAT) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pShape->iFillGradientSpread = iSpread;
	return XGE_OK;
}

int xgeShapeExFillGradientSpreadGet(xge_shape_ex pShape, int* pSpread)
{
	if ( !__xgeShapeExValid(pShape) || (pSpread == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pSpread = pShape->iFillGradientSpread;
	return XGE_OK;
}

int xgeShapeExFillGradientTransformSet(xge_shape_ex pShape, const xge_shape_ex_matrix_t* pMatrix)
{
	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExMatrixFinite(pMatrix) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pShape->tFillGradientTransform = *pMatrix;
	return XGE_OK;
}

int xgeShapeExFillGradientTransformIdentity(xge_shape_ex pShape)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pShape->tFillGradientTransform = __xgeShapeExMatrixIdentity();
	return XGE_OK;
}

int xgeShapeExFillGradientTransformGet(xge_shape_ex pShape, xge_shape_ex_matrix_t* pMatrix)
{
	if ( !__xgeShapeExValid(pShape) || (pMatrix == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pMatrix = pShape->tFillGradientTransform;
	return XGE_OK;
}

int xgeShapeExStrokeColor(xge_shape_ex pShape, uint32_t iColor)
{
	if ( !__xgeShapeExValid(pShape) ) return XGE_ERROR_INVALID_ARGUMENT;
	pShape->iStrokeType = XGE_SHAPE_EX_FILL_SOLID;
	pShape->iStrokeColor = iColor;
	__xgeShapeExEnsureStrokeState(pShape);
	return XGE_OK;
}

int xgeShapeExStrokeTypeGet(xge_shape_ex pShape, int* pType)
{
	if ( !__xgeShapeExValid(pShape) || (pType == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pType = pShape->iStrokeType;
	return XGE_OK;
}

int xgeShapeExStrokeColorGet(xge_shape_ex pShape, uint32_t* pColor)
{
	if ( !__xgeShapeExValid(pShape) || (pColor == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( !pShape->bStrokeStateCreated ) return XGE_ERROR_INVALID_STATE;
	*pColor = pShape->iStrokeColor;
	return XGE_OK;
}

int xgeShapeExStrokeLinearGradient(xge_shape_ex pShape, float fX1, float fY1, float fX2, float fY2, int iUnits, const xge_shape_ex_color_stop_t* pStops, int iStopCount)
{
	int i;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExCoordsFinite(fX1, fY1) || !__xgeShapeExCoordsFinite(fX2, fY2) ||
	     !__xgeShapeExStopsFinite(pStops, iStopCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iUnits != XGE_SHAPE_EX_GRADIENT_USER_SPACE) && (iUnits != XGE_SHAPE_EX_GRADIENT_OBJECT_BOUNDING_BOX) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeShapeExReserveStrokeStops(pShape, iStopCount);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	for ( i = 0; i < iStopCount; i++ ) {
		pShape->pStrokeSourceStops[i] = pStops[i];
		pShape->pStrokeStops[i] = pStops[i];
		if ( pShape->pStrokeStops[i].fOffset < 0.0f ) pShape->pStrokeStops[i].fOffset = 0.0f;
		if ( pShape->pStrokeStops[i].fOffset > 1.0f ) pShape->pStrokeStops[i].fOffset = 1.0f;
	}
	__xgeShapeExNormalizeStops(pShape->pStrokeStops, iStopCount);
	pShape->iStrokeStopCount = iStopCount;
	pShape->iStrokeType = XGE_SHAPE_EX_FILL_LINEAR_GRADIENT;
	pShape->iStrokeColor = XGE_COLOR_RGBA(
		XGE_COLOR_GET_R(pShape->iStrokeColor),
		XGE_COLOR_GET_G(pShape->iStrokeColor),
		XGE_COLOR_GET_B(pShape->iStrokeColor), 0
	);
	__xgeShapeExEnsureStrokeState(pShape);
	pShape->fStrokeX1 = fX1;
	pShape->fStrokeY1 = fY1;
	pShape->fStrokeX2 = fX2;
	pShape->fStrokeY2 = fY2;
	pShape->iStrokeGradientUnits = iUnits;
	pShape->iStrokeGradientSpread = XGE_SHAPE_EX_GRADIENT_SPREAD_PAD;
	pShape->tStrokeGradientTransform = __xgeShapeExMatrixIdentity();
	return XGE_OK;
}

int xgeShapeExStrokeLinearGradientGet(xge_shape_ex pShape, float* pX1, float* pY1, float* pX2, float* pY2, int* pUnits, const xge_shape_ex_color_stop_t** ppStops, int* pStopCount)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pShape->iStrokeType != XGE_SHAPE_EX_FILL_LINEAR_GRADIENT ) {
		return XGE_ERROR_NOT_FOUND;
	}
	if ( pX1 != NULL ) *pX1 = pShape->fStrokeX1;
	if ( pY1 != NULL ) *pY1 = pShape->fStrokeY1;
	if ( pX2 != NULL ) *pX2 = pShape->fStrokeX2;
	if ( pY2 != NULL ) *pY2 = pShape->fStrokeY2;
	if ( pUnits != NULL ) *pUnits = pShape->iStrokeGradientUnits;
	if ( ppStops != NULL ) *ppStops = pShape->pStrokeSourceStops;
	if ( pStopCount != NULL ) *pStopCount = pShape->iStrokeStopCount;
	return XGE_OK;
}

int xgeShapeExStrokeRadialGradient(xge_shape_ex pShape, float fCX, float fCY, float fRadius, float fFX, float fFY, int iUnits, const xge_shape_ex_color_stop_t* pStops, int iStopCount)
{
	return xgeShapeExStrokeRadialGradientEx(pShape, fCX, fCY, fRadius, fFX, fFY, 0.0f, iUnits, pStops, iStopCount);
}

int xgeShapeExStrokeRadialGradientEx(xge_shape_ex pShape, float fCX, float fCY, float fRadius, float fFX, float fFY, float fFocalRadius, int iUnits, const xge_shape_ex_color_stop_t* pStops, int iStopCount)
{
	int i;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExCoordsFinite(fCX, fCY) ||
	     !__xgeShapeExCoordsFinite(fRadius, fFocalRadius) || !__xgeShapeExCoordsFinite(fFX, fFY) ||
	     !__xgeShapeExStopsFinite(pStops, iStopCount) || (fRadius < 0.0f) || (fFocalRadius < 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iUnits != XGE_SHAPE_EX_GRADIENT_USER_SPACE) && (iUnits != XGE_SHAPE_EX_GRADIENT_OBJECT_BOUNDING_BOX) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeShapeExReserveStrokeStops(pShape, iStopCount);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	for ( i = 0; i < iStopCount; i++ ) {
		pShape->pStrokeSourceStops[i] = pStops[i];
		pShape->pStrokeStops[i] = pStops[i];
		if ( pShape->pStrokeStops[i].fOffset < 0.0f ) pShape->pStrokeStops[i].fOffset = 0.0f;
		if ( pShape->pStrokeStops[i].fOffset > 1.0f ) pShape->pStrokeStops[i].fOffset = 1.0f;
	}
	__xgeShapeExNormalizeStops(pShape->pStrokeStops, iStopCount);
	pShape->iStrokeStopCount = iStopCount;
	pShape->iStrokeType = XGE_SHAPE_EX_FILL_RADIAL_GRADIENT;
	pShape->iStrokeColor = XGE_COLOR_RGBA(
		XGE_COLOR_GET_R(pShape->iStrokeColor),
		XGE_COLOR_GET_G(pShape->iStrokeColor),
		XGE_COLOR_GET_B(pShape->iStrokeColor), 0
	);
	__xgeShapeExEnsureStrokeState(pShape);
	pShape->fStrokeX1 = fCX;
	pShape->fStrokeY1 = fCY;
	pShape->fStrokeX2 = fCX + fRadius;
	pShape->fStrokeY2 = fCY;
	pShape->fStrokeR = fRadius;
	pShape->fStrokeFX = fFX;
	pShape->fStrokeFY = fFY;
	pShape->fStrokeFR = fFocalRadius;
	pShape->iStrokeGradientUnits = iUnits;
	pShape->iStrokeGradientSpread = XGE_SHAPE_EX_GRADIENT_SPREAD_PAD;
	pShape->tStrokeGradientTransform = __xgeShapeExMatrixIdentity();
	return XGE_OK;
}

int xgeShapeExStrokeRadialGradientGet(xge_shape_ex pShape, float* pCX, float* pCY, float* pRadius, float* pFX, float* pFY, int* pUnits, const xge_shape_ex_color_stop_t** ppStops, int* pStopCount)
{
	return xgeShapeExStrokeRadialGradientGetEx(pShape, pCX, pCY, pRadius, pFX, pFY, NULL, pUnits, ppStops, pStopCount);
}

int xgeShapeExStrokeRadialGradientGetEx(xge_shape_ex pShape, float* pCX, float* pCY, float* pRadius, float* pFX, float* pFY, float* pFocalRadius, int* pUnits, const xge_shape_ex_color_stop_t** ppStops, int* pStopCount)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pShape->iStrokeType != XGE_SHAPE_EX_FILL_RADIAL_GRADIENT ) {
		return XGE_ERROR_NOT_FOUND;
	}
	if ( pCX != NULL ) *pCX = pShape->fStrokeX1;
	if ( pCY != NULL ) *pCY = pShape->fStrokeY1;
	if ( pRadius != NULL ) *pRadius = pShape->fStrokeR;
	if ( pFX != NULL ) *pFX = pShape->fStrokeFX;
	if ( pFY != NULL ) *pFY = pShape->fStrokeFY;
	if ( pFocalRadius != NULL ) *pFocalRadius = pShape->fStrokeFR;
	if ( pUnits != NULL ) *pUnits = pShape->iStrokeGradientUnits;
	if ( ppStops != NULL ) *ppStops = pShape->pStrokeSourceStops;
	if ( pStopCount != NULL ) *pStopCount = pShape->iStrokeStopCount;
	return XGE_OK;
}

int xgeShapeExStrokeGradientSpread(xge_shape_ex pShape, int iSpread)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iSpread != XGE_SHAPE_EX_GRADIENT_SPREAD_PAD) &&
	     (iSpread != XGE_SHAPE_EX_GRADIENT_SPREAD_REFLECT) &&
	     (iSpread != XGE_SHAPE_EX_GRADIENT_SPREAD_REPEAT) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pShape->iStrokeGradientSpread = iSpread;
	return XGE_OK;
}

int xgeShapeExStrokeGradientSpreadGet(xge_shape_ex pShape, int* pSpread)
{
	if ( !__xgeShapeExValid(pShape) || (pSpread == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pSpread = pShape->iStrokeGradientSpread;
	return XGE_OK;
}

int xgeShapeExStrokeGradientTransformSet(xge_shape_ex pShape, const xge_shape_ex_matrix_t* pMatrix)
{
	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExMatrixFinite(pMatrix) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pShape->tStrokeGradientTransform = *pMatrix;
	return XGE_OK;
}

int xgeShapeExStrokeGradientTransformIdentity(xge_shape_ex pShape)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pShape->tStrokeGradientTransform = __xgeShapeExMatrixIdentity();
	return XGE_OK;
}

int xgeShapeExStrokeGradientTransformGet(xge_shape_ex pShape, xge_shape_ex_matrix_t* pMatrix)
{
	if ( !__xgeShapeExValid(pShape) || (pMatrix == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pMatrix = pShape->tStrokeGradientTransform;
	return XGE_OK;
}

int xgeShapeExStrokeWidth(xge_shape_ex pShape, float fWidth)
{
	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExFloatFinite(fWidth) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( fWidth < 0.0f ) fWidth = 0.0f;
	pShape->fStrokeWidth = fWidth;
	__xgeShapeExEnsureStrokeState(pShape);
	return XGE_OK;
}

int xgeShapeExStrokeWidthGet(xge_shape_ex pShape, float* pWidth)
{
	if ( !__xgeShapeExValid(pShape) || (pWidth == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pWidth = pShape->fStrokeWidth;
	return XGE_OK;
}

int xgeShapeExStrokeCap(xge_shape_ex pShape, int iCap)
{
	if ( !__xgeShapeExValid(pShape) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( (iCap != XGE_SHAPE_EX_CAP_BUTT) && (iCap != XGE_SHAPE_EX_CAP_ROUND) && (iCap != XGE_SHAPE_EX_CAP_SQUARE) ) return XGE_ERROR_INVALID_ARGUMENT;
	pShape->iLineCap = iCap;
	__xgeShapeExEnsureStrokeState(pShape);
	return XGE_OK;
}

int xgeShapeExStrokeCapGet(xge_shape_ex pShape, int* pCap)
{
	if ( !__xgeShapeExValid(pShape) || (pCap == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pCap = pShape->iLineCap;
	return XGE_OK;
}

int xgeShapeExStrokeJoin(xge_shape_ex pShape, int iJoin)
{
	if ( !__xgeShapeExValid(pShape) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( (iJoin != XGE_SHAPE_EX_JOIN_MITER) && (iJoin != XGE_SHAPE_EX_JOIN_ROUND) && (iJoin != XGE_SHAPE_EX_JOIN_BEVEL) ) return XGE_ERROR_INVALID_ARGUMENT;
	pShape->iLineJoin = iJoin;
	__xgeShapeExEnsureStrokeState(pShape);
	return XGE_OK;
}

int xgeShapeExStrokeJoinGet(xge_shape_ex pShape, int* pJoin)
{
	if ( !__xgeShapeExValid(pShape) || (pJoin == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pJoin = pShape->iLineJoin;
	return XGE_OK;
}

int xgeShapeExStrokeMiterLimit(xge_shape_ex pShape, float fLimit)
{
	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExFloatFinite(fLimit) || (fLimit < 0.0f) ) return XGE_ERROR_INVALID_ARGUMENT;
	pShape->fMiterLimit = fLimit;
	__xgeShapeExEnsureStrokeState(pShape);
	return XGE_OK;
}

int xgeShapeExStrokeMiterLimitGet(xge_shape_ex pShape, float* pLimit)
{
	if ( !__xgeShapeExValid(pShape) || (pLimit == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pLimit = pShape->fMiterLimit;
	return XGE_OK;
}

int xgeShapeExStrokeNonScaling(xge_shape_ex pShape, int bNonScaling)
{
	if ( !__xgeShapeExValid(pShape) ) return XGE_ERROR_INVALID_ARGUMENT;
	pShape->bStrokeNonScaling = bNonScaling != 0;
	__xgeShapeExEnsureStrokeState(pShape);
	return XGE_OK;
}

int xgeShapeExStrokeNonScalingGet(xge_shape_ex pShape, int* pNonScaling)
{
	if ( !__xgeShapeExValid(pShape) || (pNonScaling == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pNonScaling = pShape->bStrokeNonScaling;
	return XGE_OK;
}

int xgeShapeExStrokeDash(xge_shape_ex pShape, const float* pDashPattern, int iDashCount, float fDashOffset)
{
	float* pCopy;
	int i;

	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExFloatFinite(fDashOffset) ||
	     (iDashCount < 0) || ((pDashPattern == NULL) && (iDashCount > 0)) || ((pDashPattern != NULL) && (iDashCount == 0)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( ((iDashCount & 1) != 0) && (iDashCount > (INT_MAX / 2)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < iDashCount; i++ ) {
		if ( !__xgeShapeExFloatFinite(pDashPattern[i]) ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
	}
	xrtFree(pShape->pDashPattern);
	pShape->pDashPattern = NULL;
	pShape->iDashCount = 0;
	pShape->fDashOffset = fDashOffset;
	__xgeShapeExEnsureStrokeState(pShape);
	if ( iDashCount == 0 ) {
		return XGE_OK;
	}
	pCopy = (float*)xrtMalloc((size_t)iDashCount * sizeof(*pCopy));
	if ( pCopy == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	for ( i = 0; i < iDashCount; i++ ) {
		pCopy[i] = pDashPattern[i] < 0.0f ? 0.0f : pDashPattern[i];
	}
	pShape->pDashPattern = pCopy;
	pShape->iDashCount = iDashCount;
	return XGE_OK;
}

int xgeShapeExStrokeDashGet(xge_shape_ex pShape, const float** ppDashPattern, int* pDashCount, float* pDashOffset)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( ppDashPattern != NULL ) {
		if ( pShape->bStrokeStateCreated ) *ppDashPattern = pShape->pDashPattern;
	}
	if ( pDashCount != NULL ) {
		*pDashCount = pShape->iDashCount;
	}
	if ( pDashOffset != NULL ) {
		if ( pShape->bStrokeStateCreated ) *pDashOffset = pShape->fDashOffset;
	}
	return XGE_OK;
}

int xgeShapeExTrimPath(xge_shape_ex pShape, float fBegin, float fEnd, int bSimultaneous)
{
	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExCoordsFinite(fBegin, fEnd) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( !pShape->bStrokeStateCreated && (fBegin == 0.0f) && (fEnd == 1.0f) ) {
		pShape->bTrimPath = 0;
		pShape->fTrimBegin = 0.0f;
		pShape->fTrimEnd = 1.0f;
		pShape->bTrimSimultaneous = 0;
		return XGE_OK;
	}
	__xgeShapeExEnsureStrokeState(pShape);
	pShape->bTrimPath = (fBegin != 0.0f) || (fEnd != 1.0f);
	pShape->fTrimBegin = fBegin;
	pShape->fTrimEnd = fEnd;
	pShape->bTrimSimultaneous = bSimultaneous != 0;
	return XGE_OK;
}

int xgeShapeExTrimPathGet(xge_shape_ex pShape, float* pBegin, float* pEnd, int* pSimultaneous, int* pEnabled)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pBegin != NULL ) {
		*pBegin = pShape->fTrimBegin;
	}
	if ( pEnd != NULL ) {
		*pEnd = pShape->fTrimEnd;
	}
	if ( pSimultaneous != NULL ) {
		*pSimultaneous = pShape->bTrimSimultaneous;
	}
	if ( pEnabled != NULL ) {
		*pEnabled = pShape->bTrimPath;
	}
	return XGE_OK;
}

int xgeShapeExTrimClear(xge_shape_ex pShape)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pShape->bTrimPath = 0;
	pShape->fTrimBegin = 0.0f;
	pShape->fTrimEnd = 1.0f;
	pShape->bTrimSimultaneous = pShape->bStrokeStateCreated ? 1 : 0;
	return XGE_OK;
}

int xgeShapeExFillRule(xge_shape_ex pShape, int iRule)
{
	if ( !__xgeShapeExValid(pShape) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( (iRule != XGE_SHAPE_EX_FILL_NON_ZERO) && (iRule != XGE_SHAPE_EX_FILL_EVEN_ODD) ) return XGE_ERROR_INVALID_ARGUMENT;
	pShape->iFillRule = iRule;
	return XGE_OK;
}

int xgeShapeExFillRuleGet(xge_shape_ex pShape, int* pRule)
{
	if ( !__xgeShapeExValid(pShape) || (pRule == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pRule = pShape->iFillRule;
	return XGE_OK;
}

int xgeShapeExPaintOrder(xge_shape_ex pShape, int bStrokeFirst)
{
	if ( !__xgeShapeExValid(pShape) ) return XGE_ERROR_INVALID_ARGUMENT;
	pShape->bStrokeFirst = bStrokeFirst != 0;
	__xgeShapeExEnsureStrokeState(pShape);
	return XGE_OK;
}

int xgeShapeExPaintOrderGet(xge_shape_ex pShape, int* pStrokeFirst)
{
	if ( !__xgeShapeExValid(pShape) || (pStrokeFirst == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pStrokeFirst = pShape->bStrokeFirst;
	return XGE_OK;
}

int xgeShapeExOpacity(xge_shape_ex pShape, float fOpacity)
{
	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExFloatFinite(fOpacity) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( fOpacity < 0.0f ) fOpacity = 0.0f;
	if ( fOpacity > 1.0f ) fOpacity = 1.0f;
	pShape->fOpacity = fOpacity;
	return XGE_OK;
}

int xgeShapeExOpacityGet(xge_shape_ex pShape, float* pOpacity)
{
	if ( !__xgeShapeExValid(pShape) || (pOpacity == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pOpacity = pShape->fOpacity;
	return XGE_OK;
}

int xgeShapeExVisible(xge_shape_ex pShape, int bVisible)
{
	if ( !__xgeShapeExValid(pShape) ) return XGE_ERROR_INVALID_ARGUMENT;
	pShape->bVisible = bVisible != 0;
	return XGE_OK;
}

int xgeShapeExVisibleGet(xge_shape_ex pShape, int* pVisible)
{
	if ( !__xgeShapeExValid(pShape) || (pVisible == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pVisible = pShape->bVisible;
	return XGE_OK;
}

int xgeShapeExBlend(xge_shape_ex pShape, int iBlend)
{
	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExBlendValid(iBlend) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pShape->iBlend = iBlend;
	pShape->bBlendSet = 1;
	return XGE_OK;
}

int xgeShapeExBlendClear(xge_shape_ex pShape)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pShape->iBlend = XGE_BLEND_ALPHA;
	pShape->bBlendSet = 0;
	return XGE_OK;
}

int xgeShapeExBlendGet(xge_shape_ex pShape, int* pBlend, int* pBlendSet)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pBlend != NULL ) {
		*pBlend = pShape->iBlend;
	}
	if ( pBlendSet != NULL ) {
		*pBlendSet = pShape->bBlendSet;
	}
	return XGE_OK;
}

int xgeShapeExMaskShapeSet(xge_shape_ex pShape, xge_shape_ex pTarget, int iMethod)
{
	int iRet;

	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExValid(pTarget) || !__xgeShapeExMaskMethodValid(iMethod) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pTarget->pParentScene != NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( __xgeShapeExPaintReaches(
		XGE_SHAPE_EX_MASK_TARGET_SHAPE, pTarget,
		XGE_SHAPE_EX_MASK_TARGET_SHAPE, pShape
	) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = xgeShapeExAddRef(pTarget);
	if ( iRet != XGE_OK ) return iRet;
	__xgeShapeExMaskClearInternal(pShape);
	pShape->iMaskMethod = iMethod;
	pShape->iMaskTargetType = XGE_SHAPE_EX_MASK_TARGET_SHAPE;
	pShape->pMaskShape = pTarget;
	if ( pShape->pParentScene != NULL ) pTarget->pParentScene = pShape->pParentScene;
	return XGE_OK;
}

int xgeShapeExMaskSceneSet(xge_shape_ex pShape, xge_shape_ex_scene pTarget, int iMethod)
{
	int iRet;

	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExSceneValid(pTarget) || !__xgeShapeExMaskMethodValid(iMethod) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pTarget->pParentScene != NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( __xgeShapeExPaintReaches(
		XGE_SHAPE_EX_MASK_TARGET_SCENE, pTarget,
		XGE_SHAPE_EX_MASK_TARGET_SHAPE, pShape
	) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = xgeShapeExSceneAddRef(pTarget);
	if ( iRet != XGE_OK ) return iRet;
	__xgeShapeExMaskClearInternal(pShape);
	pShape->iMaskMethod = iMethod;
	pShape->iMaskTargetType = XGE_SHAPE_EX_MASK_TARGET_SCENE;
	pShape->pMaskScene = pTarget;
	if ( pShape->pParentScene != NULL ) pTarget->pParentScene = pShape->pParentScene;
	return XGE_OK;
}

int xgeShapeExMaskClear(xge_shape_ex pShape)
{
	if ( !__xgeShapeExValid(pShape) ) return XGE_ERROR_INVALID_ARGUMENT;
	__xgeShapeExMaskClearInternal(pShape);
	return XGE_OK;
}

int xgeShapeExMaskGet(xge_shape_ex pShape, int* pMethod, int* pTargetType, xge_shape_ex* ppTargetShape, xge_shape_ex_scene* ppTargetScene)
{
	if ( !__xgeShapeExValid(pShape) ||
	     ((pMethod == NULL) && (pTargetType == NULL) && (ppTargetShape == NULL) && (ppTargetScene == NULL)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pMethod != NULL ) *pMethod = pShape->iMaskMethod;
	if ( pTargetType != NULL ) *pTargetType = pShape->iMaskTargetType;
	if ( ppTargetShape != NULL ) *ppTargetShape = pShape->pMaskShape;
	if ( ppTargetScene != NULL ) *ppTargetScene = pShape->pMaskScene;
	return XGE_OK;
}

static xge_rect_t __xgeShapeExExpandBoundsForStroke(
	xge_shape_ex pShape,
	const xge_shape_ex_flat_path_t* pFlat,
	xge_rect_t tBounds,
	xge_shape_ex_matrix_t tMatrix,
	int bLogicalBounds
);
static xge_rect_t __xgeShapeExMaskCombineBounds(xge_rect_t tSource, xge_rect_t tMask, int iMethod);
static int __xgeShapeExSceneGetBoundsInternalEx(
	xge_shape_ex_scene pScene,
	float fTolerance,
	xge_shape_ex_matrix_t tParent,
	xge_rect_t* pBounds,
	int bIncludeEffects,
	int iDepth
);
static int __xgeShapeExSceneGetRenderBounds(
	xge_shape_ex_scene pScene,
	float fTolerance,
	xge_rect_t* pBounds
);
static int __xgeShapeExApplyMaskBounds(
	int iTargetType,
	xge_shape_ex pMaskShape,
	xge_shape_ex_scene pMaskScene,
	int iMethod,
	float fTolerance,
	xge_shape_ex_matrix_t tParent,
	int bIncludeSceneEffects,
	int iDepth,
	xge_rect_t* pBounds
);

static int __xgeShapeExGetBoundsInternalEx(
	xge_shape_ex pShape,
	float fTolerance,
	xge_shape_ex_matrix_t tParent,
	xge_rect_t* pBounds,
	int bIncludeDecorations,
	int iDepth
)
{
	xge_shape_ex_flat_path_t tFlat;
	xge_shape_ex_matrix_t tMatrix;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || (pBounds == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iDepth > XGE_SHAPE_EX_CLIP_BOUNDS_MAX_DEPTH ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fTolerance <= 0.0f ) {
		fTolerance = XGE_SHAPE_EX_DEFAULT_TOLERANCE;
	}
	tMatrix = __xgeShapeExMatrixMul(tParent, pShape->tTransform);
	if ( pShape->bTrimPath ) {
		iRet = __xgeShapeExFlattenTrimmedPath(
			pShape, tMatrix, fTolerance, 1, &tFlat
		);
	} else {
		iRet = __xgeShapeExFlatten(pShape, tMatrix, fTolerance, &tFlat);
	}
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	*pBounds = __xgeShapeExFlatBounds(&tFlat);
	if ( (tFlat.iPointCount > 0) && (pShape->fStrokeWidth > 0.0f) &&
	     __xgeShapeExStrokeVisible(pShape, pShape->iStrokeColor) ) {
		xge_rect_t tStrokeBounds = __xgeShapeExExpandBoundsForStroke(
			pShape, &tFlat, __xgeShapeExFlatBounds(&tFlat), tMatrix,
			!bIncludeDecorations
		);

		*pBounds = __xgeShapeExRectUnion(*pBounds, tStrokeBounds);
	}
	if ( bIncludeDecorations && pShape->bClipRect ) {
		*pBounds = __xgeShapeExRectIntersect(*pBounds, __xgeShapeExMatrixRectBounds(tMatrix, pShape->tClipRect));
	}
	if ( bIncludeDecorations && (pShape->iClipShapeCount > 0) ) {
		xge_rect_t tClipUnion;
		int bHasClipBounds;
		int bHasIncludeClip;
		int i;

		memset(&tClipUnion, 0, sizeof(tClipUnion));
		bHasClipBounds = 0;
		bHasIncludeClip = 0;
		for ( i = 0; i < pShape->iClipShapeCount; i++ ) {
			xge_rect_t tClipBounds;
			int iMode = pShape->pClipShapeModes != NULL ? pShape->pClipShapeModes[i] : XGE_SHAPE_EX_CLIP_INTERSECT;

			if ( iMode == XGE_SHAPE_EX_CLIP_SUBTRACT ) {
				continue;
			}
			bHasIncludeClip = 1;

			iRet = __xgeShapeExGetBoundsInternalEx(
				pShape->pClipShapes[i], fTolerance, tMatrix, &tClipBounds, 1, iDepth + 1
			);
			if ( iRet != XGE_OK ) {
				__xgeShapeExFlatFree(&tFlat);
				return iRet;
			}
			if ( (tClipBounds.fW > 0.0f) && (tClipBounds.fH > 0.0f) ) {
				if ( !bHasClipBounds ) {
					tClipUnion = tClipBounds;
					bHasClipBounds = 1;
				} else {
					tClipUnion = __xgeShapeExRectUnion(tClipUnion, tClipBounds);
				}
			}
		}
		if ( bHasIncludeClip ) {
			if ( bHasClipBounds ) {
				*pBounds = __xgeShapeExRectIntersect(*pBounds, tClipUnion);
			} else {
				memset(pBounds, 0, sizeof(*pBounds));
			}
		}
	}
	if ( bIncludeDecorations && (pShape->iMaskTargetType != XGE_SHAPE_EX_MASK_TARGET_NONE) ) {
		iRet = __xgeShapeExApplyMaskBounds(
			pShape->iMaskTargetType, pShape->pMaskShape, pShape->pMaskScene,
			pShape->iMaskMethod, fTolerance, tParent, 1, iDepth + 1, pBounds
		);
		if ( iRet != XGE_OK ) {
			__xgeShapeExFlatFree(&tFlat);
			return iRet;
		}
	}
	__xgeShapeExFlatFree(&tFlat);
	return XGE_OK;
}

static int __xgeShapeExGetBoundsInternal(xge_shape_ex pShape, float fTolerance, xge_shape_ex_matrix_t tParent, xge_rect_t* pBounds)
{
	return __xgeShapeExGetBoundsInternalEx(pShape, fTolerance, tParent, pBounds, 1, 0);
}

static int __xgeShapeExGetLogicalBoundsInternal(
	xge_shape_ex pShape,
	float fTolerance,
	xge_shape_ex_matrix_t tParent,
	xge_rect_t* pBounds
)
{
	return __xgeShapeExGetBoundsInternalEx(pShape, fTolerance, tParent, pBounds, 0, 0);
}

static int __xgeShapeExApplyMaskBounds(
	int iTargetType,
	xge_shape_ex pMaskShape,
	xge_shape_ex_scene pMaskScene,
	int iMethod,
	float fTolerance,
	xge_shape_ex_matrix_t tParent,
	int bIncludeSceneEffects,
	int iDepth,
	xge_rect_t* pBounds
)
{
	xge_rect_t tMaskBounds;
	int iRet;

	if ( (pBounds == NULL) || !__xgeShapeExMaskMethodValid(iMethod) ||
	     (iDepth > XGE_SHAPE_EX_CLIP_BOUNDS_MAX_DEPTH) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(&tMaskBounds, 0, sizeof(tMaskBounds));
	if ( iTargetType == XGE_SHAPE_EX_MASK_TARGET_SHAPE ) {
		if ( __xgeShapeExMaskShapePaintVisible(pMaskShape, iMethod) ) {
			iRet = __xgeShapeExGetBoundsInternalEx(
				pMaskShape, fTolerance, tParent, &tMaskBounds, 1, iDepth
			);
		} else {
			iRet = XGE_OK;
		}
	} else if ( iTargetType == XGE_SHAPE_EX_MASK_TARGET_SCENE ) {
		if ( __xgeShapeExMaskScenePaintVisible(pMaskScene, iMethod) ) {
			iRet = __xgeShapeExSceneGetBoundsInternalEx(
				pMaskScene, fTolerance, tParent, &tMaskBounds,
				bIncludeSceneEffects, iDepth
			);
		} else {
			iRet = XGE_OK;
		}
	} else {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iRet != XGE_OK ) return iRet;
	*pBounds = __xgeShapeExMaskCombineBounds(*pBounds, tMaskBounds, iMethod);
	return XGE_OK;
}

static xge_rect_t __xgeShapeExBoundsIncludePoint(xge_rect_t tBounds, xge_vec2_t tPoint)
{
	float fLeft = tBounds.fX;
	float fTop = tBounds.fY;
	float fRight = tBounds.fX + tBounds.fW;
	float fBottom = tBounds.fY + tBounds.fH;

	if ( tPoint.fX < fLeft ) fLeft = tPoint.fX;
	if ( tPoint.fY < fTop ) fTop = tPoint.fY;
	if ( tPoint.fX > fRight ) fRight = tPoint.fX;
	if ( tPoint.fY > fBottom ) fBottom = tPoint.fY;
	tBounds.fX = fLeft;
	tBounds.fY = fTop;
	tBounds.fW = fRight - fLeft;
	tBounds.fH = fBottom - fTop;
	if ( tBounds.fW < 0.0f ) tBounds.fW = 0.0f;
	if ( tBounds.fH < 0.0f ) tBounds.fH = 0.0f;
	return tBounds;
}

static xge_rect_t __xgeShapeExBoundsIncludeCircle(xge_rect_t tBounds, xge_vec2_t tCenter, float fRadius)
{
	xge_vec2_t tPoint;

	if ( fRadius <= 0.0f ) {
		return __xgeShapeExBoundsIncludePoint(tBounds, tCenter);
	}
	tPoint.fX = tCenter.fX - fRadius;
	tPoint.fY = tCenter.fY - fRadius;
	tBounds = __xgeShapeExBoundsIncludePoint(tBounds, tPoint);
	tPoint.fX = tCenter.fX + fRadius;
	tPoint.fY = tCenter.fY + fRadius;
	return __xgeShapeExBoundsIncludePoint(tBounds, tPoint);
}

static int __xgeShapeExStrokeMiterJoinWithinLimit(xge_vec2_t tPrev, xge_vec2_t tCurr, xge_vec2_t tNext, float fMiterLimit)
{
	xge_vec2_t tD1;
	xge_vec2_t tD2;
	float fCross;
	float fDot;
	float fAngle;
	float fCos;

	if ( !__xgeShapeExNormalizeSegment(tPrev, tCurr, &tD1) ||
	     !__xgeShapeExNormalizeSegment(tCurr, tNext, &tD2) ) {
		return 0;
	}
	fCross = tD1.fX * tD2.fY - tD1.fY * tD2.fX;
	if ( fabsf(fCross) <= XGE_SHAPE_EX_EPSILON ) {
		return 0;
	}
	fDot = tD1.fX * tD2.fX + tD1.fY * tD2.fY;
	if ( fDot < -1.0f ) fDot = -1.0f;
	if ( fDot > 1.0f ) fDot = 1.0f;
	fAngle = acosf(fDot);
	fCos = cosf(fAngle * 0.5f);
	if ( fCos <= XGE_SHAPE_EX_EPSILON ) {
		return 0;
	}
	return (1.0f / fCos) <= (fMiterLimit + XGE_SHAPE_EX_EPSILON);
}

static int __xgeShapeExFlatStrokeHasDrawableContour(xge_shape_ex pShape, const xge_shape_ex_flat_path_t* pFlat)
{
	int i;

	if ( (pShape == NULL) || (pFlat == NULL) ) {
		return 0;
	}
	for ( i = 0; i < pFlat->iContourCount; i++ ) {
		const xge_shape_ex_flat_contour_t* pContour = &pFlat->pContours[i];
		const xge_vec2_t* pSrc;
		xge_vec2_t* pPoints;
		int iPointCount;

		if ( (pContour->iCount <= 0) || (pContour->iStart < 0) || ((pContour->iStart + pContour->iCount) > pFlat->iPointCount) ) {
			continue;
		}
		pSrc = pFlat->pPoints + pContour->iStart;
		pPoints = NULL;
		iPointCount = 0;
		if ( __xgeShapeExCompactStrokePoints(pSrc, pContour->iCount, pContour->bClosed, &pPoints, &iPointCount) != XGE_OK ) {
			return 1;
		}
		xrtFree(pPoints);
		if ( iPointCount >= 2 ) {
			return 1;
		}
		if ( (iPointCount == 1) && (pShape->iLineCap != XGE_SHAPE_EX_CAP_BUTT) ) {
			return 1;
		}
	}
	return 0;
}

static xge_rect_t __xgeShapeExExpandBoundsForStrokeOutlines(xge_shape_ex pShape, const xge_shape_ex_flat_path_t* pFlat, xge_rect_t tBounds, float fStrokeWidth)
{
	float fHalfWidth;
	float fMiterLimit;
	int i;

	if ( (pFlat == NULL) || (fStrokeWidth <= 0.0f) ) {
		return tBounds;
	}
	fHalfWidth = fStrokeWidth * 0.5f;
	fMiterLimit = pShape->fMiterLimit;
	if ( fMiterLimit < 1.0f ) {
		fMiterLimit = 1.0f;
	}
	for ( i = 0; i < pFlat->iContourCount; i++ ) {
		const xge_shape_ex_flat_contour_t* pContour = &pFlat->pContours[i];
		const xge_vec2_t* pSrc;
		xge_vec2_t* pPoints;
		int iPointCount;
		int iJoinStart;
		int iJoinEnd;
		int j;

		if ( (pContour->iCount <= 0) || (pContour->iStart < 0) || ((pContour->iStart + pContour->iCount) > pFlat->iPointCount) ) {
			continue;
		}
		pSrc = pFlat->pPoints + pContour->iStart;
		pPoints = NULL;
		iPointCount = 0;
		if ( __xgeShapeExCompactStrokePoints(pSrc, pContour->iCount, pContour->bClosed, &pPoints, &iPointCount) != XGE_OK ) {
			return tBounds;
		}
		if ( iPointCount == 1 ) {
			if ( pShape->iLineCap != XGE_SHAPE_EX_CAP_BUTT ) {
				tBounds = __xgeShapeExBoundsIncludeCircle(tBounds, pPoints[0], fHalfWidth);
			}
			xrtFree(pPoints);
			continue;
		}
		if ( !pContour->bClosed && (pShape->iLineCap == XGE_SHAPE_EX_CAP_SQUARE) ) {
			xge_vec2_t tLeft;
			xge_vec2_t tRight;

			__xgeShapeExStrokeEndpointPair(pPoints[0], pPoints[1], fHalfWidth, pShape->iLineCap, 0, &tLeft, &tRight);
			tBounds = __xgeShapeExBoundsIncludePoint(tBounds, tLeft);
			tBounds = __xgeShapeExBoundsIncludePoint(tBounds, tRight);
			__xgeShapeExStrokeEndpointPair(pPoints[iPointCount - 1], pPoints[iPointCount - 2], fHalfWidth, pShape->iLineCap, 1, &tLeft, &tRight);
			tBounds = __xgeShapeExBoundsIncludePoint(tBounds, tLeft);
			tBounds = __xgeShapeExBoundsIncludePoint(tBounds, tRight);
		}
		if ( iPointCount < 3 ) {
			xrtFree(pPoints);
			continue;
		}
		if ( pShape->iLineJoin != XGE_SHAPE_EX_JOIN_MITER ) {
			xrtFree(pPoints);
			continue;
		}
		iJoinStart = pContour->bClosed ? 0 : 1;
		iJoinEnd = pContour->bClosed ? iPointCount : (iPointCount - 1);
		for ( j = iJoinStart; j < iJoinEnd; j++ ) {
			int iPrev = (j + iPointCount - 1) % iPointCount;
			int iNext = (j + 1) % iPointCount;
			xge_vec2_t tLeft = __xgeShapeExStrokeSidePoint(pPoints[iPrev], pPoints[j], pPoints[iNext], fHalfWidth, fMiterLimit, 1);
			xge_vec2_t tRight = __xgeShapeExStrokeSidePoint(pPoints[iPrev], pPoints[j], pPoints[iNext], fHalfWidth, fMiterLimit, -1);

			if ( !__xgeShapeExStrokeMiterJoinWithinLimit(pPoints[iPrev], pPoints[j], pPoints[iNext], fMiterLimit) ) {
				continue;
			}
			tBounds = __xgeShapeExBoundsIncludePoint(tBounds, tLeft);
			tBounds = __xgeShapeExBoundsIncludePoint(tBounds, tRight);
		}
		xrtFree(pPoints);
	}
	return tBounds;
}

static xge_rect_t __xgeShapeExExpandBoundsForTransformedStrokeOutlines(
	xge_shape_ex pShape,
	const xge_shape_ex_flat_path_t* pFlat,
	xge_rect_t tBounds,
	float fHalfWidth,
	xge_shape_ex_matrix_t tMatrix
)
{
	xge_shape_ex_matrix_t tInverse;
	int i;

	if ( !__xgeShapeExMatrixInverse(tMatrix, &tInverse) ) {
		float fExtentScale = 1.0f;
		float fExpandX;
		float fExpandY;

		if ( pShape->iLineCap == XGE_SHAPE_EX_CAP_SQUARE ) {
			fExtentScale = 1.41421356237f;
		}
		if ( pShape->iLineJoin == XGE_SHAPE_EX_JOIN_MITER ) {
			float fMiterLimit = pShape->fMiterLimit;

			if ( fMiterLimit < 1.0f ) fMiterLimit = 1.0f;
			if ( fMiterLimit > fExtentScale ) fExtentScale = fMiterLimit;
		}
		fExpandX = fHalfWidth * hypotf(tMatrix.fA, tMatrix.fC) * fExtentScale;
		fExpandY = fHalfWidth * hypotf(tMatrix.fB, tMatrix.fD) * fExtentScale;
		tBounds.fX -= fExpandX;
		tBounds.fY -= fExpandY;
		tBounds.fW += fExpandX * 2.0f;
		tBounds.fH += fExpandY * 2.0f;
		return tBounds;
	}
	{
		float fExpandX = fHalfWidth * hypotf(tMatrix.fA, tMatrix.fC);
		float fExpandY = fHalfWidth * hypotf(tMatrix.fB, tMatrix.fD);

		tBounds.fX -= fExpandX;
		tBounds.fY -= fExpandY;
		tBounds.fW += fExpandX * 2.0f;
		tBounds.fH += fExpandY * 2.0f;
	}
	for ( i = 0; i < pFlat->iContourCount; i++ ) {
		const xge_shape_ex_flat_contour_t* pContour = &pFlat->pContours[i];
		const xge_vec2_t* pSrc;
		xge_vec2_t* pPoints;
		int iPointCount;
		int j;

		if ( (pContour->iCount <= 0) || (pContour->iStart < 0) ||
		     ((pContour->iStart + pContour->iCount) > pFlat->iPointCount) ) {
			continue;
		}
		pSrc = pFlat->pPoints + pContour->iStart;
		pPoints = NULL;
		iPointCount = 0;
		if ( __xgeShapeExCompactStrokePoints(pSrc, pContour->iCount, pContour->bClosed, &pPoints, &iPointCount) != XGE_OK ) {
			return tBounds;
		}
		for ( j = 0; j < iPointCount; j++ ) {
			pPoints[j] = __xgeShapeExMatrixPoint(tInverse, pPoints[j]);
		}
		if ( !pContour->bClosed && (iPointCount >= 2) && (pShape->iLineCap == XGE_SHAPE_EX_CAP_SQUARE) ) {
			xge_vec2_t tLeft;
			xge_vec2_t tRight;

			__xgeShapeExStrokeEndpointPair(pPoints[0], pPoints[1], fHalfWidth, pShape->iLineCap, 0, &tLeft, &tRight);
			tBounds = __xgeShapeExBoundsIncludePoint(tBounds, __xgeShapeExMatrixPoint(tMatrix, tLeft));
			tBounds = __xgeShapeExBoundsIncludePoint(tBounds, __xgeShapeExMatrixPoint(tMatrix, tRight));
			__xgeShapeExStrokeEndpointPair(pPoints[iPointCount - 1], pPoints[iPointCount - 2], fHalfWidth, pShape->iLineCap, 1, &tLeft, &tRight);
			tBounds = __xgeShapeExBoundsIncludePoint(tBounds, __xgeShapeExMatrixPoint(tMatrix, tLeft));
			tBounds = __xgeShapeExBoundsIncludePoint(tBounds, __xgeShapeExMatrixPoint(tMatrix, tRight));
		}
		if ( (iPointCount >= 3) && (pShape->iLineJoin == XGE_SHAPE_EX_JOIN_MITER) ) {
			float fMiterLimit = pShape->fMiterLimit;
			int iJoinStart;
			int iJoinEnd;

			if ( fMiterLimit < 1.0f ) fMiterLimit = 1.0f;
			iJoinStart = pContour->bClosed ? 0 : 1;
			iJoinEnd = pContour->bClosed ? iPointCount : (iPointCount - 1);
			for ( j = iJoinStart; j < iJoinEnd; j++ ) {
				int iPrev = (j + iPointCount - 1) % iPointCount;
				int iNext = (j + 1) % iPointCount;
				xge_vec2_t tLeft;
				xge_vec2_t tRight;

				if ( !__xgeShapeExStrokeMiterJoinWithinLimit(pPoints[iPrev], pPoints[j], pPoints[iNext], fMiterLimit) ) {
					continue;
				}
				tLeft = __xgeShapeExStrokeSidePoint(pPoints[iPrev], pPoints[j], pPoints[iNext], fHalfWidth, fMiterLimit, 1);
				tRight = __xgeShapeExStrokeSidePoint(pPoints[iPrev], pPoints[j], pPoints[iNext], fHalfWidth, fMiterLimit, -1);
				tBounds = __xgeShapeExBoundsIncludePoint(tBounds, __xgeShapeExMatrixPoint(tMatrix, tLeft));
				tBounds = __xgeShapeExBoundsIncludePoint(tBounds, __xgeShapeExMatrixPoint(tMatrix, tRight));
			}
		}
		xrtFree(pPoints);
	}
	return tBounds;
}

static xge_rect_t __xgeShapeExExpandBoundsForStroke(
	xge_shape_ex pShape,
	const xge_shape_ex_flat_path_t* pFlat,
	xge_rect_t tBounds,
	xge_shape_ex_matrix_t tMatrix,
	int bLogicalBounds
)
{
	float fStrokeScale;
	float fStrokeWidth;
	float fExpand;

	if ( !__xgeShapeExValid(pShape) || (pFlat == NULL) || (pFlat->iPointCount <= 0) ) {
		return tBounds;
	}
	if ( (pShape->fStrokeWidth <= 0.0f) || !__xgeShapeExStrokeVisible(pShape, pShape->iStrokeColor) ) {
		return tBounds;
	}
	fStrokeScale = pShape->bStrokeNonScaling ? 1.0f : __xgeShapeExMatrixStrokeScale(tMatrix);
	fStrokeWidth = pShape->fStrokeWidth * fStrokeScale;
	if ( fStrokeWidth <= 0.0f ) {
		return tBounds;
	}
	if ( !__xgeShapeExFlatStrokeHasDrawableContour(pShape, pFlat) ) {
		return tBounds;
	}
	if ( bLogicalBounds ) {
		float fExpandX;
		float fExpandY;

		if ( pShape->bStrokeNonScaling ) {
			fExpandX = fStrokeWidth * 0.5f;
			fExpandY = fExpandX;
		} else {
			fExpandX = pShape->fStrokeWidth * 0.5f * hypotf(tMatrix.fA, tMatrix.fC);
			fExpandY = pShape->fStrokeWidth * 0.5f * hypotf(tMatrix.fB, tMatrix.fD);
		}
		tBounds.fX -= fExpandX;
		tBounds.fY -= fExpandY;
		tBounds.fW += fExpandX * 2.0f;
		tBounds.fH += fExpandY * 2.0f;
		return tBounds;
	}
	if ( !pShape->bStrokeNonScaling ) {
		return __xgeShapeExExpandBoundsForTransformedStrokeOutlines(
			pShape, pFlat, tBounds, pShape->fStrokeWidth * 0.5f, tMatrix
		);
	}
	fExpand = fStrokeWidth * 0.5f;
	tBounds.fX -= fExpand;
	tBounds.fY -= fExpand;
	tBounds.fW += fExpand * 2.0f;
	tBounds.fH += fExpand * 2.0f;
	tBounds = __xgeShapeExExpandBoundsForStrokeOutlines(pShape, pFlat, tBounds, fStrokeWidth);
	return tBounds;
}

static int __xgeShapeExGetLocalBoundsEx(xge_shape_ex pShape, float fTolerance, xge_rect_t* pBounds, int iDepth)
{
	xge_shape_ex_flat_path_t tFlat;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || (pBounds == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iDepth > XGE_SHAPE_EX_CLIP_BOUNDS_MAX_DEPTH ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fTolerance <= 0.0f ) {
		fTolerance = XGE_SHAPE_EX_DEFAULT_TOLERANCE;
	}
	if ( pShape->bTrimPath ) {
		iRet = __xgeShapeExFlattenTrimmedPath(
			pShape, __xgeShapeExMatrixIdentity(), fTolerance, 1, &tFlat
		);
	} else {
		iRet = __xgeShapeExFlatten(pShape, __xgeShapeExMatrixIdentity(), fTolerance, &tFlat);
	}
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	*pBounds = __xgeShapeExFlatBounds(&tFlat);
	if ( (tFlat.iPointCount > 0) && (pShape->fStrokeWidth > 0.0f) &&
	     __xgeShapeExStrokeVisible(pShape, pShape->iStrokeColor) ) {
		xge_rect_t tStrokeBounds = __xgeShapeExExpandBoundsForStroke(
			pShape, &tFlat, __xgeShapeExFlatBounds(&tFlat),
			__xgeShapeExMatrixIdentity(), 1
		);

		*pBounds = __xgeShapeExRectUnion(*pBounds, tStrokeBounds);
	}
	__xgeShapeExFlatFree(&tFlat);
	return XGE_OK;
}

static int __xgeShapeExGetLocalBounds(xge_shape_ex pShape, float fTolerance, xge_rect_t* pBounds)
{
	return __xgeShapeExGetLocalBoundsEx(pShape, fTolerance, pBounds, 0);
}

static void __xgeShapeExRectToOBB(xge_rect_t tBounds, xge_shape_ex_matrix_t tMatrix, xge_vec2_t* pPoints4)
{
	xge_vec2_t tPoint;

	tPoint.fX = tBounds.fX;
	tPoint.fY = tBounds.fY;
	pPoints4[0] = __xgeShapeExMatrixPoint(tMatrix, tPoint);
	tPoint.fX = tBounds.fX + tBounds.fW;
	tPoint.fY = tBounds.fY;
	pPoints4[1] = __xgeShapeExMatrixPoint(tMatrix, tPoint);
	tPoint.fX = tBounds.fX + tBounds.fW;
	tPoint.fY = tBounds.fY + tBounds.fH;
	pPoints4[2] = __xgeShapeExMatrixPoint(tMatrix, tPoint);
	tPoint.fX = tBounds.fX;
	tPoint.fY = tBounds.fY + tBounds.fH;
	pPoints4[3] = __xgeShapeExMatrixPoint(tMatrix, tPoint);
}

static int __xgeShapeExGetOBBInternal(xge_shape_ex pShape, float fTolerance, xge_shape_ex_matrix_t tParent, xge_vec2_t* pPoints4)
{
	xge_shape_ex_matrix_t tMatrix;
	xge_rect_t tBounds;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || (pPoints4 == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeShapeExGetLocalBounds(pShape, fTolerance, &tBounds);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	tMatrix = __xgeShapeExMatrixMul(tParent, pShape->tTransform);
	__xgeShapeExRectToOBB(tBounds, tMatrix, pPoints4);
	return XGE_OK;
}

static int __xgeShapeExRectIntersects(xge_rect_t a, xge_rect_t b)
{
	if ( (a.fW <= 0.0f) || (a.fH <= 0.0f) || (b.fW <= 0.0f) || (b.fH <= 0.0f) ) {
		return 0;
	}
	return (a.fX < (b.fX + b.fW)) && ((a.fX + a.fW) > b.fX) && (a.fY < (b.fY + b.fH)) && ((a.fY + a.fH) > b.fY);
}

static int __xgeShapeExSimpleIntersectsRect(
	xge_shape_ex pShape,
	xge_rect_t tRect,
	float fTolerance,
	xge_shape_ex_matrix_t tParent,
	int* pIntersects
)
{
	xge_shape_ex_flat_path_t tFlat;
	xge_shape_ex_matrix_t tMatrix;
	int bFillVisible;
	int bStrokeVisible;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || (pIntersects == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	*pIntersects = 0;
	if ( pShape->fOpacity <= 0.0f ) return XGE_OK;
	bFillVisible = __xgeShapeExFillVisible(pShape, pShape->iFillColor);
	bStrokeVisible = (pShape->fStrokeWidth > 0.0f) && __xgeShapeExStrokeVisible(pShape, pShape->iStrokeColor);
	if ( !bFillVisible && !bStrokeVisible ) return XGE_OK;
	tMatrix = __xgeShapeExMatrixMul(tParent, pShape->tTransform);
	if ( pShape->bTrimPath ) {
		iRet = __xgeShapeExFlattenTrimmedPath(pShape, tMatrix, fTolerance, 1, &tFlat);
	} else {
		iRet = __xgeShapeExFlatten(pShape, tMatrix, fTolerance, &tFlat);
	}
	if ( iRet != XGE_OK ) return iRet;
	if ( bFillVisible && __xgeShapeExFlatFillIntersectsRect(&tFlat, pShape->iFillRule, tRect) ) {
		*pIntersects = 1;
	}
	if ( !*pIntersects && bStrokeVisible ) {
		float fStrokeScale = pShape->bStrokeNonScaling ? 1.0f : __xgeShapeExMatrixStrokeScale(tMatrix);
		float fStrokeWidth = pShape->fStrokeWidth * fStrokeScale;
		float fDashOffset = pShape->fDashOffset * fStrokeScale;
		const float* pDashPattern = pShape->pDashPattern;
		float* pScaledDashPattern = NULL;

		if ( (pShape->pDashPattern != NULL) && (pShape->iDashCount > 0) &&
		     (fabsf(fStrokeScale - 1.0f) > XGE_SHAPE_EX_EPSILON) ) {
			int i;

			pScaledDashPattern = (float*)xrtMalloc((size_t)pShape->iDashCount * sizeof(*pScaledDashPattern));
			if ( pScaledDashPattern == NULL ) {
				__xgeShapeExFlatFree(&tFlat);
				return XGE_ERROR_OUT_OF_MEMORY;
			}
			for ( i = 0; i < pShape->iDashCount; i++ ) pScaledDashPattern[i] = pShape->pDashPattern[i] * fStrokeScale;
			pDashPattern = pScaledDashPattern;
		}
		if ( fStrokeWidth > XGE_SHAPE_EX_EPSILON ) {
			if ( __xgeShapeExDashTotal(pDashPattern, pShape->iDashCount) > XGE_SHAPE_EX_EPSILON ) {
				*pIntersects = __xgeShapeExFlatStrokeDashedIntersectsRect(
					pShape, &tFlat, fStrokeWidth, pDashPattern, pShape->iDashCount,
					fDashOffset, fTolerance, tRect
				);
			} else {
				*pIntersects = __xgeShapeExFlatStrokeSolidIntersectsRect(pShape, &tFlat, fStrokeWidth, fTolerance, tRect);
			}
		}
		xrtFree(pScaledDashPattern);
	}
	__xgeShapeExFlatFree(&tFlat);
	return XGE_OK;
}

static int __xgeShapeExContainsPointInternal(xge_shape_ex pShape, xge_vec2_t tPoint, float fTolerance, xge_shape_ex_matrix_t tParent, int iDepth, int* pContains);

static int __xgeShapeExClipContainsPointInternal(xge_shape_ex pClipShape, xge_vec2_t tPoint, float fTolerance, xge_shape_ex_matrix_t tParent, int iDepth, int* pContains)
{
	xge_shape_ex_t tGeometryShape;

	if ( !__xgeShapeExValid(pClipShape) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( pClipShape->bStencilPaint ) {
		return __xgeShapeExContainsPointInternal(pClipShape, tPoint, fTolerance, tParent, iDepth, pContains);
	}
	tGeometryShape = *pClipShape;
	tGeometryShape.iFillType = XGE_SHAPE_EX_FILL_SOLID;
	tGeometryShape.iFillColor = XGE_COLOR_RGBA(255, 255, 255, 255);
	tGeometryShape.fStrokeWidth = 0.0f;
	return __xgeShapeExContainsPointInternal(&tGeometryShape, tPoint, fTolerance, tParent, iDepth, pContains);
}

static int __xgeShapeExMaskCombineContains(int bSource, int bMask, int iMethod)
{
	if ( (iMethod == XGE_SHAPE_EX_MASK_ALPHA) ||
	     (iMethod == XGE_SHAPE_EX_MASK_LUMA) ||
	     (iMethod == XGE_SHAPE_EX_MASK_INTERSECT) ||
	     (iMethod == XGE_SHAPE_EX_MASK_DARKEN) ) {
		return bSource && bMask;
	}
	if ( (iMethod == XGE_SHAPE_EX_MASK_INV_ALPHA) ||
	     (iMethod == XGE_SHAPE_EX_MASK_INV_LUMA) ||
	     (iMethod == XGE_SHAPE_EX_MASK_SUBTRACT) ) {
		return bSource && !bMask;
	}
	if ( iMethod == XGE_SHAPE_EX_MASK_DIFFERENCE ) return bSource != bMask;
	return bSource || bMask;
}

static int __xgeShapeExMaskTargetHitTest(
	int iTargetType,
	xge_shape_ex pMaskShape,
	xge_shape_ex_scene pMaskScene,
	int iMethod,
	xge_vec2_t tPoint,
	float fTolerance,
	xge_shape_ex_matrix_t tParent,
	int iDepth,
	int* pContains,
	xge_shape_ex* ppHitShape
)
{
	int iRet;

	if ( (pContains == NULL) || (iDepth > XGE_SHAPE_EX_CLIP_BOUNDS_MAX_DEPTH) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pContains = 0;
	if ( ppHitShape != NULL ) *ppHitShape = NULL;
	if ( iTargetType == XGE_SHAPE_EX_MASK_TARGET_SHAPE ) {
		iRet = __xgeShapeExContainsPointInternal(
			pMaskShape, tPoint, fTolerance, tParent, iDepth, pContains
		);
		if ( (iRet == XGE_OK) && *pContains && !__xgeShapeExMaskShapePaintVisible(pMaskShape, iMethod) ) *pContains = 0;
		if ( (iRet == XGE_OK) && *pContains && (ppHitShape != NULL) ) *ppHitShape = pMaskShape;
		return iRet;
	}
	if ( iTargetType == XGE_SHAPE_EX_MASK_TARGET_SCENE ) {
		xge_shape_ex pHitShape = NULL;

		iRet = xgeShapeExSceneHitTestEx(
			pMaskScene, tPoint.fX, tPoint.fY, fTolerance, &tParent, &pHitShape
		);
		if ( iRet != XGE_OK ) return iRet;
		*pContains = (pHitShape != NULL) && __xgeShapeExMaskShapePaintVisible(pHitShape, iMethod);
		if ( ppHitShape != NULL ) *ppHitShape = pHitShape;
		return XGE_OK;
	}
	return XGE_ERROR_INVALID_ARGUMENT;
}

static int __xgeShapeExContainsPointInternal(xge_shape_ex pShape, xge_vec2_t tPoint, float fTolerance, xge_shape_ex_matrix_t tParent, int iDepth, int* pContains)
{
	xge_shape_ex_flat_path_t tFlat;
	xge_shape_ex_matrix_t tMatrix;
	xge_shape_ex_matrix_t tInverse;
	int iRet;
	int bFillVisible;
	int bStrokeVisible;

	if ( !__xgeShapeExValid(pShape) || (pContains == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pContains = 0;
	if ( iDepth > XGE_SHAPE_EX_CLIP_BOUNDS_MAX_DEPTH ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fTolerance <= 0.0f ) {
		fTolerance = XGE_SHAPE_EX_DEFAULT_TOLERANCE;
	}
	if ( !pShape->bVisible || (pShape->fOpacity <= 0.0f) ) {
		return XGE_OK;
	}
	if ( pShape->iMaskTargetType != XGE_SHAPE_EX_MASK_TARGET_NONE ) {
		xge_shape_ex_t tSource = *pShape;
		int bSourceContains = 0;
		int bMaskContains = 0;

		tSource.iMaskMethod = XGE_SHAPE_EX_MASK_NONE;
		tSource.iMaskTargetType = XGE_SHAPE_EX_MASK_TARGET_NONE;
		tSource.pMaskShape = NULL;
		tSource.pMaskScene = NULL;
		iRet = __xgeShapeExContainsPointInternal(
			&tSource, tPoint, fTolerance, tParent, iDepth + 1, &bSourceContains
		);
		if ( iRet != XGE_OK ) return iRet;
		iRet = __xgeShapeExMaskTargetHitTest(
			pShape->iMaskTargetType, pShape->pMaskShape, pShape->pMaskScene,
			pShape->iMaskMethod,
			tPoint, fTolerance, tParent, iDepth + 1, &bMaskContains, NULL
		);
		if ( iRet != XGE_OK ) return iRet;
		*pContains = __xgeShapeExMaskCombineContains(bSourceContains, bMaskContains, pShape->iMaskMethod);
		return XGE_OK;
	}
	bFillVisible = __xgeShapeExFillVisible(pShape, pShape->iFillColor);
	bStrokeVisible = (pShape->fStrokeWidth > 0.0f) && __xgeShapeExStrokeVisible(pShape, pShape->iStrokeColor);
	if ( !bFillVisible && !bStrokeVisible ) {
		return XGE_OK;
	}
	tMatrix = __xgeShapeExMatrixMul(tParent, pShape->tTransform);
	if ( pShape->bClipRect ) {
		xge_vec2_t tLocalPoint;

		if ( !__xgeShapeExMatrixInverse(tMatrix, &tInverse) ) {
			return XGE_OK;
		}
		tLocalPoint = __xgeShapeExMatrixPoint(tInverse, tPoint);
		if ( !__xgeShapeExRectContainsPoint(pShape->tClipRect, tLocalPoint) ) {
			return XGE_OK;
		}
	}
	if ( pShape->iClipShapeCount > 0 ) {
		int bAnyClipContains;
		int bHasIncludeClip;
		int i;

		bAnyClipContains = 0;
		bHasIncludeClip = 0;
		for ( i = 0; i < pShape->iClipShapeCount; i++ ) {
			int bClipContains;
			int iMode = pShape->pClipShapeModes != NULL ? pShape->pClipShapeModes[i] : XGE_SHAPE_EX_CLIP_INTERSECT;

			iRet = __xgeShapeExClipContainsPointInternal(pShape->pClipShapes[i], tPoint, fTolerance, tMatrix, iDepth + 1, &bClipContains);
			if ( iRet != XGE_OK ) {
				return iRet;
			}
			if ( iMode == XGE_SHAPE_EX_CLIP_SUBTRACT ) {
				if ( bClipContains ) {
					return XGE_OK;
				}
				continue;
			}
			bHasIncludeClip = 1;
			if ( bClipContains ) {
				bAnyClipContains = 1;
			}
		}
		if ( bHasIncludeClip && !bAnyClipContains ) {
			return XGE_OK;
		}
	}
	if ( pShape->bTrimPath ) {
		iRet = __xgeShapeExFlattenTrimmedPath(
			pShape, tMatrix, fTolerance, 1, &tFlat
		);
	} else {
		iRet = __xgeShapeExFlatten(pShape, tMatrix, fTolerance, &tFlat);
	}
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	if ( bFillVisible ) {
		*pContains = __xgeShapeExFlatContainsPoint(&tFlat, pShape->iFillRule, tPoint);
	}
	if ( !*pContains && bStrokeVisible ) {
		float fStrokeScale = pShape->bStrokeNonScaling ? 1.0f : __xgeShapeExMatrixStrokeScale(tMatrix);
		float fStrokeWidth = pShape->fStrokeWidth * fStrokeScale;
		float fDashOffset = pShape->fDashOffset * fStrokeScale;
		const float* pDashPattern = pShape->pDashPattern;
		float* pScaledDashPattern = NULL;

		if ( (pShape->pDashPattern != NULL) && (pShape->iDashCount > 0) && (fabsf(fStrokeScale - 1.0f) > XGE_SHAPE_EX_EPSILON) ) {
			int i;

			pScaledDashPattern = (float*)xrtMalloc((size_t)pShape->iDashCount * sizeof(*pScaledDashPattern));
			if ( pScaledDashPattern == NULL ) {
				__xgeShapeExFlatFree(&tFlat);
				return XGE_ERROR_OUT_OF_MEMORY;
			}
			for ( i = 0; i < pShape->iDashCount; i++ ) {
				pScaledDashPattern[i] = pShape->pDashPattern[i] * fStrokeScale;
			}
			pDashPattern = pScaledDashPattern;
		}
		if ( fStrokeWidth > XGE_SHAPE_EX_EPSILON ) {
			if ( __xgeShapeExDashTotal(pDashPattern, pShape->iDashCount) > XGE_SHAPE_EX_EPSILON ) {
				*pContains = __xgeShapeExFlatStrokeDashedContainsPoint(pShape, &tFlat, fStrokeWidth, pDashPattern, pShape->iDashCount, fDashOffset, fTolerance, tPoint);
			} else {
				*pContains = __xgeShapeExFlatStrokeSolidContainsPoint(pShape, &tFlat, fStrokeWidth, fTolerance, tPoint);
			}
		}
		xrtFree(pScaledDashPattern);
	}
	__xgeShapeExFlatFree(&tFlat);
	return XGE_OK;
}

static int __xgeShapeExDecoratedIntersectsRect(
	xge_shape_ex pShape,
	xge_rect_t tRect,
	float fTolerance,
	xge_shape_ex_matrix_t tParent,
	int* pIntersects
)
{
	xge_shape_ex_t tProbe;
	xge_rect_t tBounds;
	xge_rect_t tScan;
	int iStartX;
	int iStartY;
	int iEndX;
	int iEndY;
	int y;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || (pIntersects == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	*pIntersects = 0;
	if ( pShape->fOpacity <= 0.0f ) return XGE_OK;
	iRet = __xgeShapeExGetBoundsInternal(pShape, fTolerance, tParent, &tBounds);
	if ( iRet != XGE_OK ) return iRet;
	tScan = __xgeShapeExRectIntersect(tBounds, tRect);
	if ( (tScan.fW <= 0.0f) || (tScan.fH <= 0.0f) ) return XGE_OK;
	tProbe = *pShape;
	tProbe.bVisible = 1;
	{
		float fStartX = floorf(tScan.fX);
		float fStartY = floorf(tScan.fY);
		float fEndX = ceilf(tScan.fX + tScan.fW);
		float fEndY = ceilf(tScan.fY + tScan.fH);

		if ( !__xgeShapeExCoordsFinite(fStartX, fStartY) ||
		     !__xgeShapeExCoordsFinite(fEndX, fEndY) ||
		     (fStartX < -2147483648.0f) || (fStartY < -2147483648.0f) ||
		     (fEndX >= 2147483648.0f) || (fEndY >= 2147483648.0f) ) {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		iStartX = (int)fStartX;
		iStartY = (int)fStartY;
		iEndX = (int)fEndX;
		iEndY = (int)fEndY;
	}
	for ( y = iStartY; y < iEndY; y++ ) {
		int x;

		for ( x = iStartX; x < iEndX; x++ ) {
			float fLeft = fmaxf(tScan.fX, (float)x);
			float fTop = fmaxf(tScan.fY, (float)y);
			float fRight = fminf(tScan.fX + tScan.fW, (float)x + 1.0f);
			float fBottom = fminf(tScan.fY + tScan.fH, (float)y + 1.0f);
			float pSampleX[3];
			float pSampleY[3];
			int sy;

			if ( (fRight <= fLeft) || (fBottom <= fTop) ) continue;
			pSampleX[0] = (fLeft + fRight) * 0.5f;
			pSampleX[1] = fLeft + (fRight - fLeft) * 0.25f;
			pSampleX[2] = fLeft + (fRight - fLeft) * 0.75f;
			pSampleY[0] = (fTop + fBottom) * 0.5f;
			pSampleY[1] = fTop + (fBottom - fTop) * 0.25f;
			pSampleY[2] = fTop + (fBottom - fTop) * 0.75f;
			for ( sy = 0; sy < 3; sy++ ) {
				int sx;

				for ( sx = 0; sx < 3; sx++ ) {
					int bContains = 0;
					xge_vec2_t tPoint = {pSampleX[sx], pSampleY[sy]};

					iRet = __xgeShapeExContainsPointInternal(&tProbe, tPoint, fTolerance, tParent, 0, &bContains);
					if ( iRet != XGE_OK ) return iRet;
					if ( bContains ) {
						*pIntersects = 1;
						return XGE_OK;
					}
				}
			}
		}
	}
	return XGE_OK;
}

static int __xgeShapeExIntersectsInternal(
	xge_shape_ex pShape,
	xge_rect_t tRect,
	float fTolerance,
	xge_shape_ex_matrix_t tParent,
	int* pIntersects
)
{
	if ( (pShape->iMaskTargetType != XGE_SHAPE_EX_MASK_TARGET_NONE) || pShape->bClipRect ||
	     (pShape->iClipShapeCount > 0) ) {
		return __xgeShapeExDecoratedIntersectsRect(pShape, tRect, fTolerance, tParent, pIntersects);
	}
	return __xgeShapeExSimpleIntersectsRect(pShape, tRect, fTolerance, tParent, pIntersects);
}

static int __xgeShapeExSceneHitTestInternal(xge_shape_ex_scene pScene, xge_vec2_t tPoint, float fTolerance, xge_shape_ex_matrix_t tParent, xge_shape_ex* ppShape)
{
	xge_shape_ex_matrix_t tMatrix;
	int i;
	int iRet;

	if ( !__xgeShapeExSceneValid(pScene) || (ppShape == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppShape = NULL;
	if ( !pScene->bVisible || (pScene->fOpacity <= 0.0f) ) {
		return XGE_OK;
	}
	if ( pScene->iMaskTargetType != XGE_SHAPE_EX_MASK_TARGET_NONE ) {
		xge_shape_ex_scene_t tSource = *pScene;
		xge_shape_ex pSourceHit = NULL;
		xge_shape_ex pMaskHit = NULL;
		int bSourceContains;
		int bMaskContains;

		tSource.iMaskMethod = XGE_SHAPE_EX_MASK_NONE;
		tSource.iMaskTargetType = XGE_SHAPE_EX_MASK_TARGET_NONE;
		tSource.pMaskShape = NULL;
		tSource.pMaskScene = NULL;
		iRet = __xgeShapeExSceneHitTestInternal(
			&tSource, tPoint, fTolerance, tParent, &pSourceHit
		);
		if ( iRet != XGE_OK ) return iRet;
		iRet = __xgeShapeExMaskTargetHitTest(
			pScene->iMaskTargetType, pScene->pMaskShape, pScene->pMaskScene,
			pScene->iMaskMethod,
			tPoint, fTolerance, tParent, 1, &bMaskContains, &pMaskHit
		);
		if ( iRet != XGE_OK ) return iRet;
		bSourceContains = pSourceHit != NULL;
		if ( __xgeShapeExMaskCombineContains(bSourceContains, bMaskContains, pScene->iMaskMethod) ) {
			*ppShape = pSourceHit != NULL ? pSourceHit : pMaskHit;
		}
		return XGE_OK;
	}
	if ( fTolerance <= 0.0f ) {
		fTolerance = XGE_SHAPE_EX_DEFAULT_TOLERANCE;
	}
	tMatrix = __xgeShapeExMatrixMul(tParent, pScene->tTransform);
	if ( pScene->bClipRect ) {
		xge_shape_ex_matrix_t tInverse;
		xge_vec2_t tLocalPoint;

		if ( !__xgeShapeExMatrixInverse(tMatrix, &tInverse) ) {
			return XGE_OK;
		}
		tLocalPoint = __xgeShapeExMatrixPoint(tInverse, tPoint);
		if ( !__xgeShapeExRectContainsPoint(pScene->tClipRect, tLocalPoint) ) {
			return XGE_OK;
		}
	}
	if ( pScene->iClipShapeCount > 0 ) {
		int bAnyClipContains = 0;
		int bHasIncludeClip = 0;

		for ( i = 0; i < pScene->iClipShapeCount; i++ ) {
			int bClipContains = 0;
			int iMode = pScene->pClipShapeModes != NULL ? pScene->pClipShapeModes[i] : XGE_SHAPE_EX_CLIP_INTERSECT;

			iRet = __xgeShapeExClipContainsPointInternal(pScene->pClipShapes[i], tPoint, fTolerance, tMatrix, 0, &bClipContains);
			if ( iRet != XGE_OK ) {
				return iRet;
			}
			if ( iMode == XGE_SHAPE_EX_CLIP_SUBTRACT ) {
				if ( bClipContains ) {
					return XGE_OK;
				}
				continue;
			}
			bHasIncludeClip = 1;
			if ( bClipContains ) {
				bAnyClipContains = 1;
			}
		}
		if ( bHasIncludeClip && !bAnyClipContains ) {
			return XGE_OK;
		}
	}
	for ( i = pScene->iChildCount - 1; i >= 0; i-- ) {
		const xge_shape_ex_scene_child_t* pChild = &pScene->pChildren[i];
		xge_shape_ex pShape;
		int bContains = 0;
		int bFillVisible;
		int bStrokeVisible;

		if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SCENE ) {
			xge_shape_ex pHit = NULL;

			iRet = __xgeShapeExSceneHitTestInternal(
				pChild->pScene, tPoint, fTolerance, tMatrix, &pHit
			);
			if ( iRet != XGE_OK ) return iRet;
			if ( pHit != NULL ) {
				*ppShape = pHit;
				return XGE_OK;
			}
			continue;
		}
		if ( pChild->iType != XGE_SHAPE_EX_SCENE_CHILD_SHAPE ) continue;
		pShape = pChild->pShape;

		if ( !__xgeShapeExValid(pShape) || !pShape->bVisible || (pShape->fOpacity <= 0.0f) ) {
			continue;
		}
		bFillVisible = __xgeShapeExFillVisible(pShape, pShape->iFillColor);
		bStrokeVisible = (pShape->fStrokeWidth > 0.0f) && __xgeShapeExStrokeVisible(pShape, pShape->iStrokeColor);
		if ( !bFillVisible && !bStrokeVisible ) {
			continue;
		}
		iRet = __xgeShapeExContainsPointInternal(pShape, tPoint, fTolerance, tMatrix, 0, &bContains);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		if ( bContains ) {
			*ppShape = pShape;
			return XGE_OK;
		}
	}
	return XGE_OK;
}

int xgeShapeExGetBounds(xge_shape_ex pShape, float fTolerance, xge_rect_t* pBounds)
{
	if ( !__xgeShapeExValid(pShape) || (pBounds == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pShape->iCommandCount <= 0 ) {
		memset(pBounds, 0, sizeof(*pBounds));
		return XGE_ERROR_INVALID_STATE;
	}
	return __xgeShapeExGetLogicalBoundsInternal(
		pShape, fTolerance, __xgeShapeExMatrixIdentity(), pBounds
	);
}

int xgeShapeExGetOBB(xge_shape_ex pShape, float fTolerance, xge_vec2_t* pPoints4)
{
	if ( !__xgeShapeExValid(pShape) || (pPoints4 == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pShape->iCommandCount <= 0 ) {
		memset(pPoints4, 0, sizeof(*pPoints4) * 4u);
		return XGE_ERROR_INVALID_STATE;
	}
	return __xgeShapeExGetOBBInternal(pShape, fTolerance, __xgeShapeExMatrixIdentity(), pPoints4);
}

int xgeShapeExBoundsIntersects(xge_shape_ex pShape, xge_rect_t tRect, float fTolerance, int* pIntersects)
{
	xge_rect_t tBounds;
	int iRet;

	if ( !__xgeShapeExValid(pShape) || (pIntersects == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pIntersects = 0;
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XGE_OK;
	}
	iRet = xgeShapeExGetBounds(pShape, fTolerance, &tBounds);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	*pIntersects = __xgeShapeExRectIntersects(tBounds, tRect);
	return XGE_OK;
}

int xgeShapeExIntersects(xge_shape_ex pShape, xge_rect_t tRect, float fTolerance, int* pIntersects)
{
	return xgeShapeExIntersectsEx(pShape, tRect, fTolerance, NULL, pIntersects);
}

int xgeShapeExIntersectsEx(
	xge_shape_ex pShape,
	xge_rect_t tRect,
	float fTolerance,
	const xge_shape_ex_matrix_t* pParentMatrix,
	int* pIntersects
)
{
	xge_shape_ex_matrix_t tParent;

	if ( !__xgeShapeExValid(pShape) || (pIntersects == NULL) ||
	     !__xgeShapeExCoordsFinite(tRect.fX, tRect.fY) ||
	     !__xgeShapeExCoordsFinite(tRect.fW, tRect.fH) ||
	     !__xgeShapeExCoordsFinite(tRect.fX + tRect.fW, tRect.fY + tRect.fH) ||
	     !__xgeShapeExFloatFinite(fTolerance) ||
	     ((pParentMatrix != NULL) && !__xgeShapeExMatrixFinite(pParentMatrix)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pIntersects = 0;
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) return XGE_OK;
	if ( fTolerance <= 0.0f ) fTolerance = XGE_SHAPE_EX_DEFAULT_TOLERANCE;
	tParent = (pParentMatrix != NULL) ? *pParentMatrix : __xgeShapeExMatrixIdentity();
	return __xgeShapeExIntersectsInternal(pShape, tRect, fTolerance, tParent, pIntersects);
}

int xgeShapeExContainsPoint(xge_shape_ex pShape, float fX, float fY, float fTolerance, int* pContains)
{
	return xgeShapeExContainsPointEx(pShape, fX, fY, fTolerance, NULL, pContains);
}

int xgeShapeExContainsPointEx(xge_shape_ex pShape, float fX, float fY, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, int* pContains)
{
	xge_shape_ex_matrix_t tParent;
	xge_vec2_t tPoint;

	if ( !__xgeShapeExValid(pShape) || (pContains == NULL) || !__xgeShapeExCoordsFinite(fX, fY) || !__xgeShapeExFloatFinite(fTolerance) ||
	     ((pParentMatrix != NULL) && !__xgeShapeExMatrixFinite(pParentMatrix)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tParent = (pParentMatrix != NULL) ? *pParentMatrix : __xgeShapeExMatrixIdentity();
	tPoint.fX = fX;
	tPoint.fY = fY;
	return __xgeShapeExContainsPointInternal(pShape, tPoint, fTolerance, tParent, 0, pContains);
}

int xgeShapeExGetLength(xge_shape_ex pShape, float fTolerance, float* pLength)
{
	xge_shape_ex_path_measure pMeasure;
	int iRet;

	pMeasure = NULL;
	iRet = xgeShapeExPathMeasureCreate(&pMeasure, pShape, NULL, fTolerance);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = xgeShapeExPathMeasureGetLength(pMeasure, pLength);
	xgeShapeExPathMeasureDestroy(pMeasure);
	return iRet;
}

int xgeShapeExGetPointAtLength(xge_shape_ex pShape, float fDistance, float fTolerance, xge_vec2_t* pPoint, xge_vec2_t* pTangent)
{
	xge_shape_ex_path_measure pMeasure;
	int iRet;

	pMeasure = NULL;
	iRet = xgeShapeExPathMeasureCreate(&pMeasure, pShape, NULL, fTolerance);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = xgeShapeExPathMeasureGetPointAtLength(pMeasure, fDistance, pPoint, pTangent);
	xgeShapeExPathMeasureDestroy(pMeasure);
	return iRet;
}

int xgeShapeExPathMeasureCreate(xge_shape_ex_path_measure* ppMeasure, xge_shape_ex pShape, const xge_shape_ex_matrix_t* pMatrix, float fTolerance)
{
	xge_shape_ex_path_measure pMeasure;
	xge_shape_ex_matrix_t tMatrix;
	int iRet;

	if ( (ppMeasure == NULL) || !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppMeasure = NULL;
	if ( fTolerance <= 0.0f ) {
		fTolerance = XGE_SHAPE_EX_DEFAULT_TOLERANCE;
	}
	tMatrix = pMatrix != NULL ? *pMatrix : __xgeShapeExMatrixIdentity();
	pMeasure = (xge_shape_ex_path_measure)xrtCalloc(1, sizeof(*pMeasure));
	if ( pMeasure == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iRet = __xgeShapeExFlatten(pShape, tMatrix, fTolerance, &pMeasure->tFlat);
	if ( iRet != XGE_OK ) {
		xrtFree(pMeasure);
		return iRet;
	}
	pMeasure->fLength = __xgeShapeExFlatLength(&pMeasure->tFlat);
	pMeasure->iMagic = XGE_SHAPE_EX_PATH_MEASURE_MAGIC;
	*ppMeasure = pMeasure;
	return XGE_OK;
}

void xgeShapeExPathMeasureDestroy(xge_shape_ex_path_measure pMeasure)
{
	if ( !__xgeShapeExPathMeasureValid(pMeasure) ) {
		return;
	}
	pMeasure->iMagic = 0;
	__xgeShapeExFlatFree(&pMeasure->tFlat);
	memset(pMeasure, 0, sizeof(*pMeasure));
	xrtFree(pMeasure);
}

int xgeShapeExPathMeasureGetLength(xge_shape_ex_path_measure pMeasure, float* pLength)
{
	if ( !__xgeShapeExPathMeasureValid(pMeasure) || (pLength == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pLength = pMeasure->fLength;
	return XGE_OK;
}

int xgeShapeExPathMeasureGetPointAtLength(xge_shape_ex_path_measure pMeasure, float fDistance, xge_vec2_t* pPoint, xge_vec2_t* pTangent)
{
	if ( !__xgeShapeExPathMeasureValid(pMeasure) || ((pPoint == NULL) && (pTangent == NULL)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	return __xgeShapeExFlatPointAtLength(&pMeasure->tFlat, fDistance, pPoint, pTangent) ? XGE_OK : XGE_ERROR_NOT_FOUND;
}

int xgeShapeExClipRectSet(xge_shape_ex pShape, xge_rect_t tRect)
{
	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExRectFinite(tRect) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( tRect.fW < 0.0f ) tRect.fW = 0.0f;
	if ( tRect.fH < 0.0f ) tRect.fH = 0.0f;
	pShape->tClipRect = tRect;
	pShape->bClipRect = 1;
	return XGE_OK;
}

int xgeShapeExClipRectGet(xge_shape_ex pShape, xge_rect_t* pRect, int* pEnabled)
{
	if ( !__xgeShapeExValid(pShape) || ((pRect == NULL) && (pEnabled == NULL)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pRect != NULL ) {
		if ( pShape->bClipRect ) {
			*pRect = pShape->tClipRect;
		} else {
			memset(pRect, 0, sizeof(*pRect));
		}
	}
	if ( pEnabled != NULL ) {
		*pEnabled = pShape->bClipRect;
	}
	return XGE_OK;
}

int xgeShapeExClipShapeAdd(xge_shape_ex pShape, xge_shape_ex pClipShape)
{
	return __xgeShapeExClipShapeAddRef(pShape, pClipShape);
}

int xgeShapeExClipShapeAddEx(xge_shape_ex pShape, xge_shape_ex pClipShape, int iMode)
{
	return __xgeShapeExClipShapeAddRefEx(pShape, pClipShape, iMode);
}

int xgeShapeExClipShapeGetCount(xge_shape_ex pShape, int* pCount)
{
	if ( !__xgeShapeExValid(pShape) || (pCount == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pCount = pShape->iClipShapeCount;
	return XGE_OK;
}

int xgeShapeExClipShapeGetAt(xge_shape_ex pShape, int iIndex, xge_shape_ex* ppClipShape)
{
	if ( !__xgeShapeExValid(pShape) || (ppClipShape == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iIndex < 0) || (iIndex >= pShape->iClipShapeCount) ) {
		*ppClipShape = NULL;
		return XGE_ERROR_NOT_FOUND;
	}
	*ppClipShape = pShape->pClipShapes[iIndex];
	return XGE_OK;
}

int xgeShapeExClipShapeGetAtEx(xge_shape_ex pShape, int iIndex, xge_shape_ex* ppClipShape, int* pMode)
{
	if ( !__xgeShapeExValid(pShape) || ((ppClipShape == NULL) && (pMode == NULL)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iIndex < 0) || (iIndex >= pShape->iClipShapeCount) ) {
		if ( ppClipShape != NULL ) {
			*ppClipShape = NULL;
		}
		if ( pMode != NULL ) {
			*pMode = XGE_SHAPE_EX_CLIP_INTERSECT;
		}
		return XGE_ERROR_NOT_FOUND;
	}
	if ( ppClipShape != NULL ) {
		*ppClipShape = pShape->pClipShapes[iIndex];
	}
	if ( pMode != NULL ) {
		*pMode = pShape->pClipShapeModes != NULL ? pShape->pClipShapeModes[iIndex] : XGE_SHAPE_EX_CLIP_INTERSECT;
	}
	return XGE_OK;
}

int xgeShapeExClipShapeClear(xge_shape_ex pShape)
{
	if ( !__xgeShapeExValid(pShape) ) return XGE_ERROR_INVALID_ARGUMENT;
	__xgeShapeExClipShapesClearInternal(pShape);
	return XGE_OK;
}

int xgeShapeExClipClear(xge_shape_ex pShape)
{
	if ( !__xgeShapeExValid(pShape) ) return XGE_ERROR_INVALID_ARGUMENT;
	memset(&pShape->tClipRect, 0, sizeof(pShape->tClipRect));
	pShape->bClipRect = 0;
	__xgeShapeExClipShapesClearInternal(pShape);
	return XGE_OK;
}

int xgeShapeExStencilClipBegin(xge_shape_ex pClipShape, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, int* pApplied)
{
	return __xgeShapeExStencilClipBeginPublic(pClipShape, fTolerance, 0, pParentMatrix, pApplied);
}

int xgeShapeExStencilClipBeginPx(xge_shape_ex pClipShape, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, int* pApplied)
{
	return __xgeShapeExStencilClipBeginPublic(pClipShape, fTolerance, 1, pParentMatrix, pApplied);
}

int xgeShapeExStencilClipEnd(int bApplied, int iRet)
{
	return __xgeShapeExEndStencilClip(bApplied != 0, iRet);
}

int xgeShapeExTransformSet(xge_shape_ex pShape, const xge_shape_ex_matrix_t* pMatrix)
{
	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExMatrixFinite(pMatrix) ) return XGE_ERROR_INVALID_ARGUMENT;
	pShape->tTransform = *pMatrix;
	pShape->tTransformState.bOverride = 1;
	return XGE_OK;
}

int xgeShapeExTransformIdentity(xge_shape_ex pShape)
{
	if ( !__xgeShapeExValid(pShape) ) return XGE_ERROR_INVALID_ARGUMENT;
	pShape->tTransform = __xgeShapeExMatrixIdentity();
	__xgeShapeExTransformStateReset(&pShape->tTransformState);
	return XGE_OK;
}

int xgeShapeExTransformGet(xge_shape_ex pShape, xge_shape_ex_matrix_t* pMatrix)
{
	if ( !__xgeShapeExValid(pShape) || (pMatrix == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pMatrix = pShape->tTransform;
	return XGE_OK;
}

int xgeShapeExTransformTranslate(xge_shape_ex pShape, float fTX, float fTY)
{
	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExCoordsFinite(fTX, fTY) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( pShape->tTransformState.bOverride ) return XGE_ERROR_INVALID_STATE;
	pShape->tTransformState.fTranslateX = fTX;
	pShape->tTransformState.fTranslateY = fTY;
	__xgeShapeExTransformStateRebuild(&pShape->tTransformState, &pShape->tTransform);
	return XGE_OK;
}

int xgeShapeExTransformScale(xge_shape_ex pShape, float fSX, float fSY)
{
	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExCoordsFinite(fSX, fSY) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( pShape->tTransformState.bOverride ) return XGE_ERROR_INVALID_STATE;
	pShape->tTransformState.fScaleX = fSX;
	pShape->tTransformState.fScaleY = fSY;
	__xgeShapeExTransformStateRebuild(&pShape->tTransformState, &pShape->tTransform);
	return XGE_OK;
}

int xgeShapeExTransformRotate(xge_shape_ex pShape, float fRadians)
{
	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExFloatFinite(fRadians) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( pShape->tTransformState.bOverride ) return XGE_ERROR_INVALID_STATE;
	pShape->tTransformState.fRotateRadians = fRadians;
	__xgeShapeExTransformStateRebuild(&pShape->tTransformState, &pShape->tTransform);
	return XGE_OK;
}

int xgeShapeExTransformSkew(xge_shape_ex pShape, float fXRadians, float fYRadians)
{
	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExCoordsFinite(fXRadians, fYRadians) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( pShape->tTransformState.bOverride ) return XGE_ERROR_INVALID_STATE;
	pShape->tTransformState.fSkewXRadians = fXRadians;
	pShape->tTransformState.fSkewYRadians = fYRadians;
	__xgeShapeExTransformStateRebuild(&pShape->tTransformState, &pShape->tTransform);
	return XGE_OK;
}

static int __xgeShapeExBlendNeedsComposite(int iBlend)
{
	switch ( iBlend ) {
		case XGE_BLEND_ADD:
		case XGE_BLEND_MULTIPLY:
		case XGE_BLEND_SCREEN:
		case XGE_BLEND_DARKEN:
		case XGE_BLEND_LIGHTEN:
		case XGE_BLEND_OVERLAY:
		case XGE_BLEND_COLOR_DODGE:
		case XGE_BLEND_COLOR_BURN:
		case XGE_BLEND_HARD_LIGHT:
		case XGE_BLEND_SOFT_LIGHT:
		case XGE_BLEND_DIFFERENCE:
		case XGE_BLEND_EXCLUSION:
		case XGE_BLEND_HUE:
		case XGE_BLEND_SATURATION:
		case XGE_BLEND_COLOR:
		case XGE_BLEND_LUMINOSITY:
			return 1;
		default:
			return 0;
	}
}

#define XGE_SHAPE_EX_BLEND_SOURCE_SCENE 0
#define XGE_SHAPE_EX_BLEND_SOURCE_SHAPE 1
#define XGE_SHAPE_EX_BLEND_SOURCE_GRADIENT 2
#define XGE_SHAPE_EX_BLEND_SOURCE_MASKED 3
#define XGE_SHAPE_EX_BLEND_SOURCE_MASKED_GROUP 4
#define XGE_SHAPE_EX_BLEND_SOURCE_INHERITED_OPACITY 5
#define XGE_SHAPE_EX_BLEND_PAINT_ALL 0
#define XGE_SHAPE_EX_BLEND_PAINT_FILL 1
#define XGE_SHAPE_EX_BLEND_PAINT_STROKE 2

static int __xgeShapeExBlendShapeSource(xge_shape_ex pShape, int iPaint)
{
	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_SHAPE_EX_BLEND_SOURCE_SHAPE;
	}
	if ( pShape->iMaskTargetType != XGE_SHAPE_EX_MASK_TARGET_NONE ) {
		return XGE_SHAPE_EX_BLEND_SOURCE_MASKED;
	}
	if ( (iPaint != XGE_SHAPE_EX_BLEND_PAINT_STROKE) &&
	     ((pShape->iFillType == XGE_SHAPE_EX_FILL_LINEAR_GRADIENT) ||
	      (pShape->iFillType == XGE_SHAPE_EX_FILL_RADIAL_GRADIENT)) ) {
		return XGE_SHAPE_EX_BLEND_SOURCE_GRADIENT;
	}
	if ( (iPaint != XGE_SHAPE_EX_BLEND_PAINT_FILL) &&
	     ((pShape->iStrokeType == XGE_SHAPE_EX_FILL_LINEAR_GRADIENT) ||
	      (pShape->iStrokeType == XGE_SHAPE_EX_FILL_RADIAL_GRADIENT)) ) {
		return XGE_SHAPE_EX_BLEND_SOURCE_GRADIENT;
	}
	return XGE_SHAPE_EX_BLEND_SOURCE_SHAPE;
}

static float __xgeShapeExBlendSourceOpacity(int iSource, float fParentOpacity, float fOpacity)
{
	float fCombined;

	if ( (iSource != XGE_SHAPE_EX_BLEND_SOURCE_SHAPE) &&
	     (iSource != XGE_SHAPE_EX_BLEND_SOURCE_MASKED) ) return 1.0f;
	fCombined = fParentOpacity * fOpacity;
	if ( fCombined < 0.0f ) return 0.0f;
	if ( fCombined > 1.0f ) return 1.0f;
	return fCombined;
}

static float __xgeShapeExBlendPaintOpacity(xge_shape_ex pShape, int iPaint)
{
	const xge_shape_ex_color_stop_t* pStops;
	uint32_t iColor;
	int iStopCount;
	uint32_t iAlpha;
	int i;

	if ( !__xgeShapeExValid(pShape) ) return 1.0f;
	if ( iPaint == XGE_SHAPE_EX_BLEND_PAINT_STROKE ) {
		if ( pShape->iStrokeType == XGE_SHAPE_EX_FILL_SOLID ) {
			return (float)XGE_COLOR_GET_A(pShape->iStrokeColor) / 255.0f;
		}
		pStops = pShape->pStrokeStops;
		iStopCount = pShape->iStrokeStopCount;
	} else {
		if ( pShape->iFillType == XGE_SHAPE_EX_FILL_SOLID ) {
			return (float)XGE_COLOR_GET_A(pShape->iFillColor) / 255.0f;
		}
		pStops = pShape->pFillStops;
		iStopCount = pShape->iFillStopCount;
	}
	if ( (pStops == NULL) || (iStopCount <= 0) ) return 1.0f;
	iColor = pStops[0].iColor;
	iAlpha = XGE_COLOR_GET_A(iColor);
	for ( i = 1; i < iStopCount; i++ ) {
		if ( XGE_COLOR_GET_A(pStops[i].iColor) != iAlpha ) return 1.0f;
	}
	return (float)iAlpha / 255.0f;
}

static int __xgeShapeExBlendRendererEnsure(void)
{
	char sHeader[128];
	char sVertex[1536];
	char sFragment[24576];
	int iRet;

	if ( g_xgeShapeExBlendRenderer.bInitialized ) {
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
		"uniform sampler2D uTexture2;\n"
		"uniform sampler2D uTexture3;\n"
		"uniform int uBlendMode;\n"
		"uniform int uBlendSource;\n"
		"uniform float uBlendOpacity;\n"
		"out vec4 FragColor;\n"
		"const vec3 ONE = vec3(1.0);\n"
		"const vec3 LUM_I = vec3(54.0, 182.0, 19.0);\n"
		"vec3 channel8(vec3 color) { return floor(clamp(color, vec3(0.0), ONE) * 255.0 + 0.5); }\n"
		"float lum8(vec3 color) { return floor(dot(channel8(color), LUM_I) / 256.0); }\n"
		"vec3 truncDiv(vec3 value, float divisor) { return sign(value) * floor(abs(value) / divisor); }\n"
		"vec3 setLum(vec3 color, float l) {\n"
		"  vec3 value = channel8(color);\n"
		"  value += vec3(l - lum8(color));\n"
		"  float n = min(value.r, min(value.g, value.b));\n"
		"  float x = max(value.r, max(value.g, value.b));\n"
		"  if (n < 0.0) { value = vec3(l) + truncDiv((value - vec3(l)) * l, l - n); x = max(value.r, max(value.g, value.b)); }\n"
		"  if (x > 255.0) value = vec3(l) + truncDiv((value - vec3(l)) * (255.0 - l), x - l);\n"
		"  return value / 255.0;\n"
		"}\n"
		"float sat(vec3 color) {\n"
		"  return max(color.r, max(color.g, color.b)) - min(color.r, min(color.g, color.b));\n"
		"}\n"
		"vec3 setSat(vec3 color, float s) {\n"
		"  float rMin = step(color.r, color.g) * step(color.r, color.b);\n"
		"  float gMin = (1.0 - rMin) * step(color.g, color.r) * step(color.g, color.b);\n"
		"  vec3 minMask = vec3(rMin, gMin, 1.0 - rMin - gMin);\n"
		"  float bMax = step(color.r, color.b) * step(color.g, color.b);\n"
		"  float gMax = (1.0 - bMax) * step(color.r, color.g) * step(color.b, color.g);\n"
		"  vec3 maxMask = vec3(1.0 - bMax - gMax, gMax, bMax);\n"
		"  vec3 midMask = vec3(1.0) - minMask - maxMask;\n"
		"  float cMin = dot(color, minMask);\n"
		"  float cMid = dot(color, midMask);\n"
		"  float cMax = dot(color, maxMask);\n"
		"  float delta = cMax - cMin;\n"
		"  float deltaMask = sign(delta);\n"
		"  float scale = deltaMask * s / max(delta, 0.000001);\n"
		"  return maxMask * (s * deltaMask) + midMask * ((cMid - cMin) * scale);\n"
		"}\n"
		"vec3 overlayChannel(vec3 a, vec3 b) {\n"
		"  vec3 low = min(ONE, 2.0 * a * b);\n"
		"  vec3 high = ONE - min(ONE, 2.0 * (ONE - a) * (ONE - b));\n"
		"  return mix(low, high, step(vec3(0.5), b));\n"
		"}\n"
		"vec3 multiply8(vec3 c, vec3 a) { return floor((c * a + vec3(255.0)) / 256.0); }\n"
		"vec3 alphaBlend8(vec3 c, float a) { return floor(c * (a + 1.0) / 256.0); }\n"
		"vec3 unpremultiply8(vec3 c, float a) {\n"
		"  if (a > 0.0 && a < 255.0) return min(vec3(255.0), floor(c * 255.0 / a));\n"
		"  return c;\n"
		"}\n"
		"vec3 premix8(vec3 blended, vec3 source, float a) {\n"
		"  if (a >= 255.0) return blended;\n"
		"  if (a <= 0.0) return source;\n"
		"  return alphaBlend8(blended, a) + alphaBlend8(source, 255.0 - a);\n"
		"}\n"
		"vec3 overlay8(vec3 source, vec3 dest) {\n"
		"  vec3 low = min(vec3(255.0), 2.0 * multiply8(source, dest));\n"
		"  vec3 high = vec3(255.0) - min(vec3(255.0), 2.0 * multiply8(vec3(255.0) - source, vec3(255.0) - dest));\n"
		"  return mix(low, high, step(vec3(128.0), dest));\n"
		"}\n"
		"float dodge8(float source, float dest) {\n"
		"  if (dest <= 0.0) return 0.0;\n"
		"  if (source >= 255.0) return 255.0;\n"
		"  return min(255.0, floor(dest * 255.0 / (255.0 - source)));\n"
		"}\n"
		"float burn8(float source, float dest) {\n"
		"  if (dest >= 255.0) return 255.0;\n"
		"  if (source <= 0.0) return 0.0;\n"
		"  return 255.0 - min(255.0, floor((255.0 - dest) * 255.0 / source));\n"
		"}\n"
		"float truncDivScalar(float value, float divisor) { return sign(value) * floor(abs(value) / divisor); }\n"
		"float softLight8(float source, float dest) {\n"
		"  if (source <= 127.0) return dest - floor((255.0 - 2.0 * source) * dest * (255.0 - dest) / 65025.0);\n"
		"  float d = dest <= 64.0 ? 4.0 * dest - floor(12.0 * dest * dest / 255.0) + floor(16.0 * dest * dest * dest / 65025.0) : floor(sqrt(dest / 255.0) * 255.0);\n"
		"  return dest + truncDivScalar((2.0 * source - 255.0) * (d - dest), 255.0);\n"
		"}\n"
		"float lumValue8(vec3 color) { return floor(dot(color, LUM_I) / 256.0); }\n"
		"vec3 saturate8(vec3 color, vec3 saturationSource) {\n"
		"  float target = max(saturationSource.r, max(saturationSource.g, saturationSource.b)) - min(saturationSource.r, min(saturationSource.g, saturationSource.b));\n"
		"  vec3 value = color; int lo = 0; int mid = 1; int hi = 2; int tempIndex;\n"
		"  if (value[lo] > value[mid]) { tempIndex = lo; lo = mid; mid = tempIndex; }\n"
		"  if (value[mid] > value[hi]) { tempIndex = mid; mid = hi; hi = tempIndex; }\n"
		"  if (value[lo] > value[mid]) { tempIndex = lo; lo = mid; mid = tempIndex; }\n"
		"  float minValue = value[lo]; float maxValue = value[hi];\n"
		"  if (maxValue > minValue) { value[mid] = floor((value[mid] - minValue) * target / (maxValue - minValue)); value[hi] = target; }\n"
		"  else { value[mid] = 0.0; value[hi] = 0.0; }\n"
		"  value[lo] = 0.0; return value;\n"
		"}\n"
		"vec3 luminance8(vec3 color, float currentLum, float targetLum) {\n"
		"  vec3 value = color + vec3(targetLum - currentLum);\n"
		"  float minValue = min(value.r, min(value.g, value.b));\n"
		"  float maxValue = max(value.r, max(value.g, value.b));\n"
		"  if (minValue < 0.0) { value = vec3(targetLum) + truncDiv((value - vec3(targetLum)) * targetLum, targetLum - minValue); maxValue = max(value.r, max(value.g, value.b)); }\n"
		"  if (maxValue > 255.0) value = vec3(targetLum) + truncDiv((value - vec3(targetLum)) * (255.0 - targetLum), maxValue - targetLum);\n"
		"  return value;\n"
		"}\n"
		"vec4 composite8(vec3 blended, vec3 destPremul, float destAlpha, float alpha) {\n"
		"  float a = floor(clamp(alpha, 0.0, 1.0) * 255.0 + 0.5);\n"
		"  vec3 rgb = alphaBlend8(blended, a) + alphaBlend8(destPremul, 255.0 - a);\n"
		"  float outAlpha = floor(255.0 * (a + 1.0) / 256.0) + floor(destAlpha * (256.0 - a) / 256.0);\n"
		"  return vec4(clamp(rgb, vec3(0.0), vec3(255.0)) / 255.0, clamp(outAlpha, 0.0, 255.0) / 255.0);\n"
		"}\n"
		"vec4 interpolate8(vec3 blended, vec3 destPremul, float destAlpha, float alpha) {\n"
		"  float a = floor(clamp(alpha, 0.0, 1.0) * 255.0 + 0.5);\n"
		"  vec3 rgb = floor(((blended - destPremul) * a + destPremul * 256.0) / 256.0);\n"
		"  float outAlpha = floor(((255.0 - destAlpha) * a + destAlpha * 256.0) / 256.0);\n"
		"  return vec4(clamp(rgb, vec3(0.0), vec3(255.0)) / 255.0, clamp(outAlpha, 0.0, 255.0) / 255.0);\n"
		"}\n"
		"vec4 compositeSceneAlpha8(vec3 sourcePremul, float sourceAlpha, vec3 destPremul, float destAlpha, float opacity) {\n"
		"  float o = floor(clamp(opacity, 0.0, 1.0) * 255.0 + 0.5);\n"
		"  float a = floor(sourceAlpha * (o + 1.0) / 256.0);\n"
		"  vec3 rgb = alphaBlend8(sourcePremul, o) + alphaBlend8(destPremul, 255.0 - a);\n"
		"  float outAlpha = a + floor(destAlpha * (256.0 - a) / 256.0);\n"
		"  return vec4(clamp(rgb, vec3(0.0), vec3(255.0)) / 255.0, clamp(outAlpha, 0.0, 255.0) / 255.0);\n"
		"}\n"
		"float compositeAlpha8(float destAlpha, float alpha) {\n"
		"  float a = floor(clamp(alpha, 0.0, 1.0) * 255.0 + 0.5);\n"
		"  return clamp(floor(255.0 * (a + 1.0) / 256.0) + floor(destAlpha * (256.0 - a) / 256.0), 0.0, 255.0) / 255.0;\n"
		"}\n"
		"void main() {\n"
		"  vec4 src = texture(uTexture, vUV);\n"
		"  vec4 dst = texture(uTexture2, vUV);\n"
		"  vec3 Dc = dst.rgb;\n"
		"  float Da = dst.a;\n"
		"  vec3 sourceColor = src.a > 0.0 ? src.rgb / src.a : vec3(0.0);\n"
		"  float sourceCoverage = src.a;\n"
		"  float geometryCoverage = src.a;\n"
		"  vec3 Sc;\n"
		"  if (uBlendSource == 1) {\n"
		"    float baseAlpha = floor(clamp(uBlendOpacity, 0.0, 1.0) * 255.0 + 0.5) / 255.0;\n"
		"    sourceCoverage = baseAlpha > 0.0 ? clamp(src.a / baseAlpha, 0.0, 1.0) : 0.0;\n"
		"    Sc = sourceCoverage > 0.0 ? src.rgb / sourceCoverage : vec3(0.0);\n"
		"  } else if (uBlendSource == 2) {\n"
		"    geometryCoverage = texture(uTexture3, vUV).a;\n"
		"    float gradientAlpha = geometryCoverage > 0.000001 ? clamp(src.a / geometryCoverage, 0.0, 1.0) : 0.0;\n"
		"    vec3 gradientPremul = geometryCoverage > 0.000001 ? clamp(src.rgb / geometryCoverage, vec3(0.0), ONE) : vec3(0.0);\n"
		"    Sc = gradientPremul + Dc * (1.0 - gradientAlpha);\n"
		"  } else {\n"
		"    Sc = uBlendSource >= 3 ? sourceColor * uBlendOpacity : sourceColor;\n"
		"  }\n"
		"  vec3 source8 = channel8(Sc);\n"
		"  vec3 destPremul8 = channel8(Dc);\n"
		"  float destAlpha8 = floor(clamp(Da, 0.0, 1.0) * 255.0 + 0.5);\n"
		"  if (uBlendSource == 0) {\n"
		"    float sourceAlpha8 = floor(clamp(src.a, 0.0, 1.0) * 255.0 + 0.5);\n"
		"    source8 = unpremultiply8(channel8(src.rgb), sourceAlpha8);\n"
		"  }\n"
		"  vec3 dest8 = unpremultiply8(destPremul8, destAlpha8);\n"
		"  vec3 result8 = source8;\n"
		"  vec3 Rc = Sc;\n"
		"  if (uBlendMode == 2) {\n"
		"    result8 = min(source8 + destPremul8, vec3(255.0));\n"
		"  } else if (uBlendMode == 3) {\n"
		"    result8 = premix8(multiply8(source8, dest8), source8, destAlpha8);\n"
		"  } else if (uBlendMode == 4) {\n"
		"    result8 = source8 + destPremul8 - multiply8(source8, destPremul8);\n"
		"  } else if (uBlendMode == 6) {\n"
		"    result8 = premix8(min(source8, dest8), source8, destAlpha8);\n"
		"  } else if (uBlendMode == 7) {\n"
		"    result8 = max(source8, destPremul8);\n"
		"  } else if (uBlendMode == 8) {\n"
		"    result8 = premix8(overlay8(source8, dest8), source8, destAlpha8);\n"
		"  } else if (uBlendMode == 9) {\n"
		"    vec3 blended = vec3(dodge8(source8.r, dest8.r), dodge8(source8.g, dest8.g), dodge8(source8.b, dest8.b));\n"
		"    result8 = premix8(blended, source8, destAlpha8);\n"
		"  } else if (uBlendMode == 10) {\n"
		"    vec3 blended = vec3(burn8(source8.r, dest8.r), burn8(source8.g, dest8.g), burn8(source8.b, dest8.b));\n"
		"    result8 = premix8(blended, source8, destAlpha8);\n"
		"  } else if (uBlendMode == 11) {\n"
		"    result8 = premix8(overlay8(dest8, source8), source8, destAlpha8);\n"
		"  } else if (uBlendMode == 12) {\n"
		"    vec3 blended = vec3(softLight8(source8.r, dest8.r), softLight8(source8.g, dest8.g), softLight8(source8.b, dest8.b));\n"
		"    result8 = premix8(blended, source8, destAlpha8);\n"
		"  } else if (uBlendMode == 13) {\n"
		"    result8 = abs(destPremul8 - source8);\n"
		"  } else if (uBlendMode == 14) {\n"
		"    result8 = clamp(source8 + destPremul8 - 2.0 * multiply8(source8, destPremul8), vec3(0.0), vec3(255.0));\n"
		"  } else if (uBlendMode == 15) {\n"
		"    vec3 blended = saturate8(source8, dest8); blended = luminance8(blended, lumValue8(blended), lumValue8(dest8));\n"
		"    result8 = premix8(blended, source8, destAlpha8);\n"
		"  } else if (uBlendMode == 16) {\n"
		"    vec3 blended = saturate8(dest8, source8); blended = luminance8(blended, lumValue8(blended), lumValue8(dest8));\n"
		"    result8 = premix8(blended, source8, destAlpha8);\n"
		"  } else if (uBlendMode == 17) {\n"
		"    vec3 blended = luminance8(source8, lumValue8(source8), lumValue8(dest8));\n"
		"    result8 = premix8(blended, source8, destAlpha8);\n"
		"  } else if (uBlendMode == 18) {\n"
		"    vec3 blended = luminance8(dest8, lumValue8(dest8), lumValue8(source8));\n"
		"    result8 = premix8(blended, source8, destAlpha8);\n"
		"  }\n"
		"  if (uBlendMode >= 3 && uBlendMode <= 18) Rc = clamp(result8 / 255.0, vec3(0.0), ONE);\n"
		"  if (src.a <= 0.0) FragColor = dst;\n"
		"  else if (uBlendMode == 1 && uBlendSource == 0) {\n"
		"    FragColor = compositeSceneAlpha8(channel8(src.rgb), floor(clamp(src.a, 0.0, 1.0) * 255.0 + 0.5), destPremul8, destAlpha8, uBlendOpacity);\n"
		"  }\n"
		"  else if (uBlendSource == 1) {\n"
		"    FragColor = composite8(result8, destPremul8, destAlpha8, sourceCoverage);\n"
		"    if (uBlendMode == 3 || uBlendMode == 6 || (uBlendMode >= 8 && uBlendMode <= 12) || (uBlendMode >= 15 && uBlendMode <= 18)) {\n"
		"      FragColor.a = compositeAlpha8(destAlpha8, src.a);\n"
		"    }\n"
		"  }\n"
		"  else if (uBlendSource == 2) {\n"
		"    float coverage8 = floor(clamp(geometryCoverage, 0.0, 1.0) * 255.0 + 0.5);\n"
		"    vec3 rgb8 = floor((result8 - destPremul8) * coverage8 / 256.0 + destPremul8);\n"
		"    float alpha8 = floor((255.0 - destAlpha8) * coverage8 / 256.0 + destAlpha8);\n"
		"    FragColor = vec4(clamp(rgb8, vec3(0.0), vec3(255.0)) / 255.0, clamp(alpha8, 0.0, 255.0) / 255.0);\n"
		"  }\n"
		"  else if (uBlendSource == 0 && uBlendMode >= 3) FragColor = interpolate8(result8, destPremul8, destAlpha8, src.a * uBlendOpacity);\n"
		"  else if (uBlendSource == 3) FragColor = composite8(result8, destPremul8, destAlpha8, src.a);\n"
		"  else if (uBlendSource == 4) FragColor = composite8(result8, destPremul8, destAlpha8, src.a * uBlendOpacity);\n"
		"  else FragColor = composite8(result8, destPremul8, destAlpha8, src.a * uBlendOpacity);\n"
		"}\n",
		sHeader
	);
	iRet = xgeShaderCreate(&g_xgeShapeExBlendRenderer.tShader, sVertex, sFragment);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	g_xgeShapeExBlendRenderer.bInitialized = 1;
	return XGE_OK;
}

static int __xgeShapeExMaskRendererEnsure(void)
{
	char sHeader[128];
	char sVertex[1536];
	char sFragment[6144];
	int iRet;

	if ( g_xgeShapeExMaskRenderer.bInitialized ) return XGE_OK;
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
		"uniform sampler2D uTexture2;\n"
		"uniform int uMaskMethod;\n"
		"uniform int uQuantizeMatting;\n"
		"uniform int uAlphaMaskA8;\n"
		"out vec4 FragColor;\n"
		"vec4 byteSample(vec4 color) {\n"
		"  return floor(color * 255.0 + 0.5);\n"
		"}\n"
		"vec4 alphaBlendBytes(vec4 src, float alpha) {\n"
		"  return floor(src * (alpha + 1.0) / 256.0) / 255.0;\n"
		"}\n"
		"void main() {\n"
		"  vec4 src = texture(uTexture, vUV);\n"
		"  vec4 mask = texture(uTexture2, vUV);\n"
		"  vec4 outColor = src;\n"
		"  if (uQuantizeMatting != 0 && uMaskMethod >= 1 && uMaskMethod <= 4) {\n"
		"    vec4 srcByte = byteSample(src);\n"
		"    vec4 maskByte = byteSample(mask);\n"
		"    float alpha = (uAlphaMaskA8 != 0) ? maskByte.r : maskByte.a;\n"
		"    if (uMaskMethod >= 3) alpha = floor(dot(maskByte.rgb, vec3(54.0, 182.0, 19.0)) / 256.0);\n"
		"    if (uMaskMethod == 2 || uMaskMethod == 4) alpha = 255.0 - alpha;\n"
		"    outColor = alphaBlendBytes(srcByte, alpha);\n"
		"  }\n"
		"  else if (uMaskMethod == 1) outColor = src * mask.a;\n"
		"  else if (uMaskMethod == 2) outColor = src * (1.0 - mask.a);\n"
		"  else if (uMaskMethod == 3) {\n"
		"    vec3 color = mask.a > 0.000001 ? mask.rgb / mask.a : vec3(0.0);\n"
		"    outColor = src * dot(color, vec3(0.2109375, 0.7109375, 0.07421875)) * mask.a;\n"
		"  }\n"
		"  else if (uMaskMethod == 4) outColor = src * (1.0 - dot(mask.rgb, vec3(0.2109375, 0.7109375, 0.07421875)));\n"
		"  else if (uMaskMethod == 5) outColor = min(src + mask * (1.0 - src.a), vec4(1.0));\n"
		"  else if (uMaskMethod == 6) {\n"
		"    float a = src.a - mask.a;\n"
		"    if (a < 0.0 || src.a == 0.0) outColor = vec4(0.0);\n"
		"    else outColor = vec4((src.rgb / src.a) * a, a);\n"
		"  } else if (uMaskMethod == 7) outColor = mask * src.a;\n"
		"  else if (uMaskMethod == 8) {\n"
		"    float da = src.a - mask.a;\n"
		"    if (da == 0.0) outColor = vec4(0.0);\n"
		"    else if (da > 0.0) outColor = src * da;\n"
		"    else outColor = mask * (-da);\n"
		"  } else if (uMaskMethod == 9) {\n"
		"    vec3 color = src.a > 0.0 ? src.rgb / src.a : vec3(0.0);\n"
		"    float a = max(src.a, mask.a);\n"
		"    outColor = vec4(color * a, a);\n"
		"  } else if (uMaskMethod == 10) {\n"
		"    vec3 color = src.a > 0.0 ? src.rgb / src.a : vec3(0.0);\n"
		"    float a = min(src.a, mask.a);\n"
		"    outColor = vec4(color * a, a);\n"
		"  }\n"
		"  FragColor = clamp(outColor, vec4(0.0), vec4(1.0));\n"
		"}\n",
		sHeader
	);
	iRet = xgeShaderCreate(&g_xgeShapeExMaskRenderer.tShader, sVertex, sFragment);
	if ( iRet != XGE_OK ) return iRet;
	g_xgeShapeExMaskRenderer.bInitialized = 1;
	return XGE_OK;
}

static int __xgeShapeExGradientRendererEnsure(void)
{
	char sHeader[128];
	char sVertex[1536];
	char sFragment[8192];
	uint8_t arrPixels[XGE_SHAPE_EX_GRADIENT_TABLE_SIZE * 4];
	xge_sampler_t tSampler;
	int iRet;

	if ( g_xgeShapeExGradientRenderer.bInitialized ) return XGE_OK;
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
		"uniform sampler2D uTexture2;\n"
		"uniform vec4 uBounds;\n"
		"uniform vec2 uMaskUVSize;\n"
		"uniform vec3 uSampleRow0;\n"
		"uniform vec3 uSampleRow1;\n"
		"uniform vec4 uLinear0;\n"
		"uniform vec3 uRadial0;\n"
		"uniform vec3 uRadial1;\n"
		"uniform vec2 uLinearFixed;\n"
		"uniform int uGradientType;\n"
		"uniform int uSpread;\n"
		"uniform int uLinearFixedEnabled;\n"
		"uniform int uFocalClamped;\n"
		"out vec4 FragColor;\n"
		"float radialT(vec2 p) {\n"
		"  float radius = uRadial0.z;\n"
		"  if (radius < 0.02) return 1.0;\n"
		"  vec2 center = uRadial0.xy;\n"
		"  vec2 focal = uRadial1.xy;\n"
		"  float fr = uRadial1.z;\n"
		"  vec2 point = p - focal;\n"
		"  vec2 direction = center - focal;\n"
		"  float dr = radius - fr;\n"
		"  float c = dot(point, point) - fr * fr;\n"
		"  float a = dr * dr - dot(direction, direction);\n"
		"  float threshold = max(0.00001, max(dr * dr, 1.0) * 0.0001);\n"
		"  float den = dr * fr + dot(point, direction);\n"
		"  if (a < threshold) {\n"
		"    if (abs(den) <= 0.00001) return 1.0;\n"
		"    return 0.5 * c / den;\n"
		"  }\n"
		"  float b = den / a;\n"
		"  float disc = b * b + c / a;\n"
		"  if (disc < 0.0) return 0.0;\n"
		"  if (uFocalClamped != 0 && b < 0.0) return 0.0;\n"
		"  return sqrt(disc) - b;\n"
		"}\n"
		"float linearT(vec2 p) {\n"
		"  vec2 delta = uLinear0.zw - uLinear0.xy;\n"
		"  float lengthSquared = dot(delta, delta);\n"
		"  if (lengthSquared <= 0.0000001) return 1.0;\n"
		"  return dot(p - uLinear0.xy, delta) / lengthSquared;\n"
		"}\n"
		"float gradientT(vec2 p) {\n"
		"  return uGradientType == 1 ? radialT(p) : linearT(p);\n"
		"}\n"
		"float truncZero(float value) { return value < 0.0 ? ceil(value) : floor(value); }\n"
		"float linearColorIndex(vec2 point, vec2 screen) {\n"
		"  float offset = floor(screen.x - uLinearFixed.x);\n"
		"  float base = linearT(point) - offset * uLinearFixed.y;\n"
		"  float fixedBase = truncZero(base * 1023.0 * 256.0);\n"
		"  float fixedStep = truncZero(uLinearFixed.y * 1023.0 * 256.0);\n"
		"  return floor((fixedBase + fixedStep * offset + 128.0) / 256.0);\n"
		"}\n"
		"float colorIndex(float index) {\n"
		"  if (uSpread == 2) {\n"
		"    index = mod(index, 1024.0);\n"
		"    if (index < 0.0) index += 1024.0;\n"
		"  } else if (uSpread == 1) {\n"
		"    index = mod(index, 2048.0);\n"
		"    if (index < 0.0) index += 2048.0;\n"
		"    if (index >= 1024.0) index = 2047.0 - index;\n"
		"  } else {\n"
		"    index = clamp(index, 0.0, 1023.0);\n"
		"  }\n"
		"  return index;\n"
		"}\n"
		"void main() {\n"
		"  float maskAlpha = texture(uTexture, vUV).a;\n"
		"  if (maskAlpha <= 0.0) { FragColor = vec4(0.0); return; }\n"
		"  vec2 quadUV = vec2(vUV.x / uMaskUVSize.x, (1.0 - vUV.y) / uMaskUVSize.y);\n"
		"  vec2 screen = uBounds.xy + quadUV * uBounds.zw;\n"
		"  vec3 position = vec3(screen, 1.0);\n"
		"  vec2 samplePoint = vec2(dot(uSampleRow0, position), dot(uSampleRow1, position));\n"
		"  float rawIndex = uGradientType == 1 ? floor(gradientT(samplePoint) * 1023.0 + 0.5) :\n"
		"    (uLinearFixedEnabled != 0 ? linearColorIndex(samplePoint, screen) : floor(linearT(samplePoint) * 1023.0 + 0.5));\n"
		"  float index = colorIndex(rawIndex);\n"
		"  vec4 color = texture(uTexture2, vec2((index + 0.5) / 1024.0, 0.5));\n"
		"  FragColor = color * maskAlpha;\n"
		"}\n",
		sHeader
	);
	iRet = xgeShaderCreate(&g_xgeShapeExGradientRenderer.tShader, sVertex, sFragment);
	if ( iRet != XGE_OK ) return iRet;
	memset(arrPixels, 0, sizeof(arrPixels));
	iRet = xgeTextureCreateRGBA(
		&g_xgeShapeExGradientRenderer.tColorTable,
		XGE_SHAPE_EX_GRADIENT_TABLE_SIZE, 1, arrPixels
	);
	if ( iRet != XGE_OK ) {
		xgeShaderFree(&g_xgeShapeExGradientRenderer.tShader);
		return iRet;
	}
	tSampler = xgeSamplerDefault();
	tSampler.iMinFilter = XGE_FILTER_NEAREST;
	tSampler.iMagFilter = XGE_FILTER_NEAREST;
	tSampler.iWrapS = XGE_WRAP_CLAMP;
	tSampler.iWrapT = XGE_WRAP_CLAMP;
	iRet = xgeTextureSetSampler(&g_xgeShapeExGradientRenderer.tColorTable, &tSampler);
	if ( iRet != XGE_OK ) {
		xgeTextureFree(&g_xgeShapeExGradientRenderer.tColorTable);
		xgeShaderFree(&g_xgeShapeExGradientRenderer.tShader);
		return iRet;
	}
	g_xgeShapeExGradientRenderer.bInitialized = 1;
	return XGE_OK;
}

static int __xgeShapeExGradientMaskTargetEnsure(int iWidth, int iHeight)
{
	xge_sampler_t tSampler;
	int iTargetWidth;
	int iTargetHeight;
	int iRet;

	if ( (iWidth <= 0) || (iHeight <= 0) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( !g_xgeShapeExGradientRenderer.bMaskTargetValid ) {
		iRet = xgeRenderTargetCreate(
			&g_xgeShapeExGradientRenderer.tMaskTarget, iWidth, iHeight
		);
		if ( iRet != XGE_OK ) {
			return XGE_ERROR_GPU_FAILED;
		}
		tSampler = xgeSamplerDefault();
		tSampler.iMinFilter = XGE_FILTER_NEAREST;
		tSampler.iMagFilter = XGE_FILTER_NEAREST;
		tSampler.iWrapS = XGE_WRAP_CLAMP;
		tSampler.iWrapT = XGE_WRAP_CLAMP;
		iRet = xgeTextureSetSampler(
			xgeRenderTargetTexture(&g_xgeShapeExGradientRenderer.tMaskTarget), &tSampler
		);
		if ( iRet != XGE_OK ) {
			xgeRenderTargetFree(&g_xgeShapeExGradientRenderer.tMaskTarget);
			return iRet;
		}
		g_xgeShapeExGradientRenderer.bMaskTargetValid = 1;
		return XGE_OK;
	}
	if ( (iWidth <= g_xgeShapeExGradientRenderer.tMaskTarget.iWidth) &&
	     (iHeight <= g_xgeShapeExGradientRenderer.tMaskTarget.iHeight) ) {
		return XGE_OK;
	}
	iTargetWidth = g_xgeShapeExGradientRenderer.tMaskTarget.iWidth;
	iTargetHeight = g_xgeShapeExGradientRenderer.tMaskTarget.iHeight;
	if ( iTargetWidth < iWidth ) iTargetWidth = iWidth;
	if ( iTargetHeight < iHeight ) iTargetHeight = iHeight;
	iRet = xgeRenderTargetResize(
		&g_xgeShapeExGradientRenderer.tMaskTarget, iTargetWidth, iTargetHeight
	);
	if ( iRet != XGE_OK ) {
		g_xgeShapeExGradientRenderer.bMaskTargetValid = 0;
		return XGE_ERROR_GPU_FAILED;
	}
	tSampler = xgeSamplerDefault();
	tSampler.iMinFilter = XGE_FILTER_NEAREST;
	tSampler.iMagFilter = XGE_FILTER_NEAREST;
	tSampler.iWrapS = XGE_WRAP_CLAMP;
	tSampler.iWrapT = XGE_WRAP_CLAMP;
	iRet = xgeTextureSetSampler(
		xgeRenderTargetTexture(&g_xgeShapeExGradientRenderer.tMaskTarget), &tSampler
	);
	if ( iRet != XGE_OK ) {
		g_xgeShapeExGradientRenderer.bMaskTargetValid = 0;
		return iRet;
	}
	return XGE_OK;
}

static int __xgeShapeExGradientColorTableUpdate(
	const xge_shape_ex_color_stop_t* pStops,
	int iStopCount,
	uint32_t iFallbackColor,
	int iSpread,
	float fRepeatLength,
	float fOpacity
)
{
	uint8_t arrPixels[XGE_SHAPE_EX_GRADIENT_TABLE_SIZE * 4];
	uint8_t arrCurrent[4];
	uint8_t arrNext[4];
	uint64_t iHash;
	float fIncrement;
	float fPosition;
	int iOpacity;
	int iAABegin;
	int iAAEnd;
	int iWrite;
	int i;
	int j;

	if ( (pStops == NULL) || (iStopCount <= 0) ) return XGE_ERROR_INVALID_ARGUMENT;
	(void)iFallbackColor;
	if ( fOpacity < 0.0f ) fOpacity = 0.0f;
	if ( fOpacity > 1.0f ) fOpacity = 1.0f;
	iOpacity = (int)(fOpacity * 255.0f + 0.5f);
	arrCurrent[0] = (uint8_t)XGE_COLOR_GET_R(pStops[0].iColor);
	arrCurrent[1] = (uint8_t)XGE_COLOR_GET_G(pStops[0].iColor);
	arrCurrent[2] = (uint8_t)XGE_COLOR_GET_B(pStops[0].iColor);
	arrCurrent[3] = (uint8_t)((((int)XGE_COLOR_GET_A(pStops[0].iColor) * iOpacity) + 255) >> 8);
	iWrite = 0;
	for ( i = 0; i < 3; i++ ) {
		arrPixels[iWrite * 4 + i] = (uint8_t)(((int)arrCurrent[i] * ((int)arrCurrent[3] + 1)) >> 8);
	}
	arrPixels[iWrite * 4 + 3] = arrCurrent[3];
	iWrite++;
	fIncrement = 1.0f / (float)XGE_SHAPE_EX_GRADIENT_TABLE_SIZE;
	fPosition = 1.5f * fIncrement;
	while ( (fPosition <= pStops[0].fOffset) && (iWrite < XGE_SHAPE_EX_GRADIENT_TABLE_SIZE) ) {
		memcpy(arrPixels + iWrite * 4, arrPixels + (iWrite - 1) * 4, 4);
		iWrite++;
		fPosition += fIncrement;
	}
	iAABegin = 0;
	if ( (iSpread == XGE_SHAPE_EX_GRADIENT_SPREAD_REPEAT) && (fRepeatLength > XGE_SHAPE_EX_EPSILON) ) {
		iAABegin = (int)(800.0f / fRepeatLength);
	}
	iAAEnd = 0;
	for ( j = 0; j < (iStopCount - 1); j++ ) {
		float fDivision;
		float fDelta;

		if ( (iSpread == XGE_SHAPE_EX_GRADIENT_SPREAD_REPEAT) &&
		     (j == (iStopCount - 2)) && (iAAEnd == 0) ) {
			int iThreshold;

			iAAEnd = iAABegin;
			iThreshold = (int)((float)(XGE_SHAPE_EX_GRADIENT_TABLE_SIZE - iWrite) * 0.1f);
			if ( iAAEnd > iThreshold ) iAAEnd = iThreshold;
			if ( iAAEnd > 40 ) iAAEnd = 40;
		}
		arrNext[0] = (uint8_t)XGE_COLOR_GET_R(pStops[j + 1].iColor);
		arrNext[1] = (uint8_t)XGE_COLOR_GET_G(pStops[j + 1].iColor);
		arrNext[2] = (uint8_t)XGE_COLOR_GET_B(pStops[j + 1].iColor);
		arrNext[3] = (uint8_t)((((int)XGE_COLOR_GET_A(pStops[j + 1].iColor) * iOpacity) + 255) >> 8);
		fDivision = pStops[j + 1].fOffset - pStops[j].fOffset;
		fDelta = fabsf(fDivision) > XGE_SHAPE_EX_EPSILON ? (1.0f / fDivision) : 0.0f;
		while ( (fPosition < pStops[j + 1].fOffset) && (iWrite < XGE_SHAPE_EX_GRADIENT_TABLE_SIZE) ) {
			float fT = (fPosition - pStops[j].fOffset) * fDelta;
			int iDistance = (int)(255.0f * fT);
			int iCurrentWeight = 255 - iDistance;
			uint8_t arrColor[4];

			if ( iCurrentWeight < 0 ) iCurrentWeight = 0;
			if ( iCurrentWeight > 255 ) iCurrentWeight = 255;
			for ( i = 0; i < 4; i++ ) {
				arrColor[i] = (uint8_t)(
					((int)arrCurrent[i] * iCurrentWeight +
					 (int)arrNext[i] * (256 - iCurrentWeight)) >> 8
				);
			}
			for ( i = 0; i < 3; i++ ) {
				arrPixels[iWrite * 4 + i] = (uint8_t)(
					((int)arrColor[i] * ((int)arrColor[3] + 1)) >> 8
				);
			}
			arrPixels[iWrite * 4 + 3] = arrColor[3];
			iWrite++;
			fPosition += fIncrement;
		}
		memcpy(arrCurrent, arrNext, sizeof(arrCurrent));
		if ( (iSpread == XGE_SHAPE_EX_GRADIENT_SPREAD_REPEAT) && (j == 0) ) {
			int iThreshold = (int)((float)(iWrite - 1) * 0.1f);

			if ( iAABegin > iThreshold ) iAABegin = iThreshold;
			if ( iAABegin > 40 ) iAABegin = 40;
		}
	}
	while ( iWrite < XGE_SHAPE_EX_GRADIENT_TABLE_SIZE ) {
		for ( i = 0; i < 3; i++ ) {
			arrPixels[iWrite * 4 + i] = (uint8_t)(
				((int)arrCurrent[i] * ((int)arrCurrent[3] + 1)) >> 8
			);
		}
		arrPixels[iWrite * 4 + 3] = arrCurrent[3];
		iWrite++;
	}
	if ( iSpread != XGE_SHAPE_EX_GRADIENT_SPREAD_REPEAT ) {
		int iLast = XGE_SHAPE_EX_GRADIENT_TABLE_SIZE - 1;

		for ( i = 0; i < 3; i++ ) {
			arrPixels[iLast * 4 + i] = (uint8_t)(
				((int)arrCurrent[i] * ((int)arrCurrent[3] + 1)) >> 8
			);
		}
		arrPixels[iLast * 4 + 3] = arrCurrent[3];
	}
	if ( (iSpread == XGE_SHAPE_EX_GRADIENT_SPREAD_REPEAT) && (iAABegin > 0) && (iAAEnd > 0) ) {
		int iTable = XGE_SHAPE_EX_GRADIENT_TABLE_SIZE - iAAEnd;
		uint8_t arrEnd[4];
		uint8_t arrBegin[4];
		float fT = 1.0f / (float)(iAABegin + iAAEnd + 1);
		float fStep = fT;

		memcpy(arrEnd, arrPixels + iTable * 4, 4);
		memcpy(arrBegin, arrPixels + iAABegin * 4, 4);
		for ( i = 0; i < 3; i++ ) {
			if ( (arrEnd[3] != 0) && (arrEnd[3] != 255) ) arrEnd[i] = (uint8_t)((int)arrEnd[i] * 255 / (int)arrEnd[3]);
			if ( (arrBegin[3] != 0) && (arrBegin[3] != 255) ) arrBegin[i] = (uint8_t)((int)arrBegin[i] * 255 / (int)arrBegin[3]);
		}
		while ( iTable != iAABegin ) {
			int iDistance = 255 - (int)(255.0f * fT);
			uint8_t arrColor[4];

			for ( i = 0; i < 4; i++ ) {
				arrColor[i] = (uint8_t)(
					((int)arrEnd[i] * iDistance +
					 (int)arrBegin[i] * (256 - iDistance)) >> 8
				);
			}
			for ( i = 0; i < 3; i++ ) {
				arrPixels[iTable * 4 + i] = (uint8_t)(
					((int)arrColor[i] * ((int)arrColor[3] + 1)) >> 8
				);
			}
			arrPixels[iTable * 4 + 3] = arrColor[3];
			iTable++;
			if ( iTable == XGE_SHAPE_EX_GRADIENT_TABLE_SIZE ) iTable = 0;
			fT += fStep;
		}
	}
	iHash = UINT64_C(14695981039346656037);
	for ( i = 0; i < (int)sizeof(arrPixels); i++ ) {
		iHash ^= (uint64_t)arrPixels[i];
		iHash *= UINT64_C(1099511628211);
	}
	if ( g_xgeShapeExGradientRenderer.bColorTableValid &&
	     (g_xgeShapeExGradientRenderer.iColorTableHash == iHash) ) {
		return XGE_OK;
	}
	if ( xgeTextureUpdateRGBA(
		&g_xgeShapeExGradientRenderer.tColorTable,
		0, 0, XGE_SHAPE_EX_GRADIENT_TABLE_SIZE, 1,
		arrPixels, XGE_SHAPE_EX_GRADIENT_TABLE_SIZE * 4
	) != XGE_OK ) {
		return XGE_ERROR_GPU_FAILED;
	}
	g_xgeShapeExGradientRenderer.iColorTableHash = iHash;
	g_xgeShapeExGradientRenderer.bColorTableValid = 1;
	return XGE_OK;
}

typedef struct xge_shape_ex_stencil_state_t {
	uint32_t iActiveMask;
	uint32_t arrContextMasks[XGE_SHAPE_EX_STENCIL_CONTEXT_MAX];
	int iContextCount;
} xge_shape_ex_stencil_state_t;

static void __xgeShapeExStencilSuspend(xge_shape_ex_stencil_state_t* pState)
{
	if ( pState == NULL ) return;
	pState->iActiveMask = g_xgeShapeExStencilActiveMask;
	pState->iContextCount = g_xgeShapeExStencilContextCount;
	memcpy(pState->arrContextMasks, g_xgeShapeExStencilContextMasks, sizeof(pState->arrContextMasks));
	g_xgeShapeExStencilActiveMask = 0;
	g_xgeShapeExStencilContextCount = 0;
	memset(g_xgeShapeExStencilContextMasks, 0, sizeof(g_xgeShapeExStencilContextMasks));
	if ( glDisable != NULL ) glDisable(GL_STENCIL_TEST);
}

static void __xgeShapeExStencilResume(const xge_shape_ex_stencil_state_t* pState)
{
	if ( pState == NULL ) return;
	g_xgeShapeExStencilActiveMask = pState->iActiveMask;
	g_xgeShapeExStencilContextCount = pState->iContextCount;
	memcpy(g_xgeShapeExStencilContextMasks, pState->arrContextMasks, sizeof(g_xgeShapeExStencilContextMasks));
	if ( g_xgeShapeExStencilActiveMask != 0 ) {
		if ( glEnable != NULL ) glEnable(GL_STENCIL_TEST);
		if ( glStencilMask != NULL ) glStencilMask(0u);
		if ( glStencilFunc != NULL ) {
			glStencilFunc(GL_EQUAL, (GLint)g_xgeShapeExStencilActiveMask, g_xgeShapeExStencilActiveMask);
		}
		if ( glStencilOp != NULL ) glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	} else if ( glDisable != NULL ) {
		glDisable(GL_STENCIL_TEST);
	}
}

static int __xgeShapeExDrawToTarget(
	xge_render_target pTarget,
	xge_shape_ex_blend_draw_proc pDraw,
	void* pUser,
	xge_shape_ex_matrix_t tLocalParent
)
{
	xge_pass_t tPass;
	int iRet;

	if ( (pTarget == NULL) || (pDraw == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	memset(&tPass, 0, sizeof(tPass));
	xgePassInit(&tPass, pTarget, XGE_PASS_CLEAR_COLOR, XGE_COLOR_RGBA(0, 0, 0, 0));
	iRet = xgePassBegin(&tPass);
	if ( iRet == XGE_OK ) {
		iRet = pDraw(pUser, tLocalParent);
		if ( iRet == XGE_OK ) iRet = __xgeShapeAutoBatchFlush();
	}
	if ( tPass.bActive ) {
		int iEndRet = xgePassEnd(&tPass);
		if ( iRet == XGE_OK ) iRet = iEndRet;
	}
	return iRet;
}

static int __xgeShapeExCoverageTargetEnsure(int iWidth, int iHeight)
{
	int iTargetWidth;
	int iTargetHeight;

	if ( (iWidth <= 0) || (iHeight <= 0) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( !g_xgeShapeExCoverageRenderer.bTargetValid ) {
		if ( xgeRenderTargetCreate(
			&g_xgeShapeExCoverageRenderer.tTarget, iWidth, iHeight
		) != XGE_OK ) return XGE_ERROR_GPU_FAILED;
		g_xgeShapeExCoverageRenderer.bTargetValid = 1;
		return XGE_OK;
	}
	if ( (iWidth <= g_xgeShapeExCoverageRenderer.tTarget.iWidth) &&
	     (iHeight <= g_xgeShapeExCoverageRenderer.tTarget.iHeight) ) return XGE_OK;
	iTargetWidth = g_xgeShapeExCoverageRenderer.tTarget.iWidth;
	iTargetHeight = g_xgeShapeExCoverageRenderer.tTarget.iHeight;
	if ( iTargetWidth < iWidth ) iTargetWidth = iWidth;
	if ( iTargetHeight < iHeight ) iTargetHeight = iHeight;
	if ( xgeRenderTargetResize(
		&g_xgeShapeExCoverageRenderer.tTarget, iTargetWidth, iTargetHeight
	) != XGE_OK ) {
		g_xgeShapeExCoverageRenderer.bTargetValid = 0;
		return XGE_ERROR_GPU_FAILED;
	}
	return XGE_OK;
}

static int __xgeShapeExCoverageVerticesEnsure(int iVertexCount)
{
	xge_shape_vertex_t* pVertices;
	int iCapacity;

	if ( iVertexCount <= 0 ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( iVertexCount <= g_xgeShapeExCoverageRenderer.iScaledVertexCapacity ) return XGE_OK;
	iCapacity = g_xgeShapeExCoverageRenderer.iScaledVertexCapacity > 0 ?
		g_xgeShapeExCoverageRenderer.iScaledVertexCapacity : 256;
	while ( iCapacity < iVertexCount ) {
		if ( iCapacity > (INT_MAX / 2) ) {
			iCapacity = iVertexCount;
			break;
		}
		iCapacity *= 2;
	}
	if ( (size_t)iCapacity > (SIZE_MAX / sizeof(*pVertices)) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pVertices = (xge_shape_vertex_t*)xrtRealloc(
		g_xgeShapeExCoverageRenderer.pScaledVertices,
		(size_t)iCapacity * sizeof(*pVertices)
	);
	if ( pVertices == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	g_xgeShapeExCoverageRenderer.pScaledVertices = pVertices;
	g_xgeShapeExCoverageRenderer.iScaledVertexCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeShapeExDrawSupersampledScanMesh(
	xge_shape_vertex_t* pVertices,
	int iVertexCount,
	const uint32_t* pIndices,
	int iIndexCount,
	xge_rect_t tBounds
)
{
	const float fScale = 2.0f;
	xge_shape_vertex_t* pScaledVertices;
	xge_render_target pTarget;
	xge_shape_ex_stencil_state_t tStencilState;
	xge_draw_t tDraw;
	xge_pass_t tPass;
	xge_rect_t tFrame;
	xge_rect_t tOldClip;
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;
	int iWidth;
	int iHeight;
	int iTargetWidth;
	int iTargetHeight;
	int iOutputBlend;
	int bOldClip;
	int i;
	int iRet;

	if ( (pVertices == NULL) || (iVertexCount <= 0) || (pIndices == NULL) ||
	     (iIndexCount <= 0) ) return XGE_ERROR_INVALID_ARGUMENT;
	tFrame = (xge_rect_t){0.0f, 0.0f, (float)g_xge.iWidth, (float)g_xge.iHeight};
	bOldClip = g_xge.bClipEnabled;
	tOldClip = xgeClipGet();
	tBounds.fX -= 1.0f;
	tBounds.fY -= 1.0f;
	tBounds.fW += 2.0f;
	tBounds.fH += 2.0f;
	tBounds = __xgeShapeExRectIntersect(tBounds, tFrame);
	if ( bOldClip ) tBounds = __xgeShapeExRectIntersect(tBounds, tOldClip);
	if ( (tBounds.fW <= 0.0f) || (tBounds.fH <= 0.0f) ) return XGE_OK;
	fLeft = floorf(tBounds.fX);
	fTop = floorf(tBounds.fY);
	fRight = ceilf(tBounds.fX + tBounds.fW);
	fBottom = ceilf(tBounds.fY + tBounds.fH);
	iWidth = (int)(fRight - fLeft);
	iHeight = (int)(fBottom - fTop);
	if ( (iWidth <= 0) || (iHeight <= 0) ||
	     (iWidth > (INT_MAX / 2)) || (iHeight > (INT_MAX / 2)) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iTargetWidth = iWidth * 2;
	iTargetHeight = iHeight * 2;
	if ( glGetIntegerv != NULL ) {
		GLint iMaxTextureSize = 0;

		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &iMaxTextureSize);
		if ( (iMaxTextureSize > 0) &&
		     ((iTargetWidth > iMaxTextureSize) || (iTargetHeight > iMaxTextureSize)) ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
	}
	iRet = __xgeShapeExCoverageVerticesEnsure(iVertexCount);
	if ( iRet != XGE_OK ) return iRet;
	pScaledVertices = g_xgeShapeExCoverageRenderer.pScaledVertices;
	for ( i = 0; i < iVertexCount; i++ ) {
		pScaledVertices[i] = pVertices[i];
		pScaledVertices[i].fX = (pScaledVertices[i].fX - fLeft) * fScale;
		pScaledVertices[i].fY = (pScaledVertices[i].fY - fTop) * fScale;
	}
	memset(&tPass, 0, sizeof(tPass));
	iOutputBlend = xgeBlendGet();
	iRet = __xgeShapeAutoBatchFlush();
	if ( iRet != XGE_OK ) goto xge_shape_ex_scan_supersample_done;
	xgeClipClear();
	__xgeShapeExStencilSuspend(&tStencilState);
	xgeBlendSet(XGE_BLEND_ALPHA);
	iRet = __xgeShapeExCoverageTargetEnsure(iTargetWidth, iTargetHeight);
	pTarget = &g_xgeShapeExCoverageRenderer.tTarget;
	if ( iRet == XGE_OK ) {
		xgePassInit(&tPass, pTarget, XGE_PASS_CLEAR_COLOR, XGE_COLOR_RGBA(0, 0, 0, 0));
		iRet = xgePassBegin(&tPass);
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeShapeMeshFillPx(pScaledVertices, iVertexCount, pIndices, iIndexCount);
		if ( iRet == XGE_OK ) iRet = __xgeShapeAutoBatchFlush();
	}
	if ( tPass.bActive ) {
		int iEndRet = xgePassEnd(&tPass);

		if ( iRet == XGE_OK ) iRet = iEndRet;
	}
	xgeBlendSet(iOutputBlend);
	__xgeShapeExStencilResume(&tStencilState);
	if ( bOldClip ) xgeClipSet(tOldClip);
	else xgeClipClear();
	if ( iRet == XGE_OK ) {
		memset(&tDraw, 0, sizeof(tDraw));
		tDraw.pTexture = xgeRenderTargetTexture(pTarget);
		tDraw.tSrc = (xge_rect_t){
			0.0f, (float)(pTarget->iHeight - iTargetHeight),
			(float)iTargetWidth, (float)iTargetHeight
		};
		tDraw.tDst = (xge_rect_t){fLeft, fTop, (float)iWidth, (float)iHeight};
		tDraw.iColor = XGE_COLOR_RGBA(255, 255, 255, 255);
		tDraw.iFlags = XGE_DRAW_SCREEN_SPACE | XGE_DRAW_FLIP_Y;
		xgeDrawEx(&tDraw);
		iRet = xgeFlush();
	}

xge_shape_ex_scan_supersample_done:
	return iRet;
}

typedef struct xge_shape_ex_stroke_union_context_t {
	xge_shape_ex pShape;
	const xge_shape_ex_flat_path_t* pFlat;
	float fWidth;
	xge_rect_t tBounds;
	xge_shape_ex_matrix_t tMatrix;
	xge_shape_ex_matrix_t tGeometryMatrix;
	xge_camera_t tCamera;
	xge_rect_t tViewport;
	int bGeometryMatrix;
	int bViewport;
	int bScreenSpace;
} xge_shape_ex_stroke_union_context_t;

static int __xgeShapeExStrokeUnionRendererEnsure(void)
{
	char sHeader[128];
	char sVertex[1024];
	char sFragment[1024];
	int iRet;

	if ( g_xgeShapeExStrokeUnionRenderer.bInitialized ) return XGE_OK;
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
		"uniform vec4 uColor;\n"
		"out vec4 FragColor;\n"
		"void main() {\n"
		"  vec4 color = uColor;\n"
		"  color.rgb *= color.a;\n"
		"  FragColor = color * texture(uTexture, vUV).a;\n"
		"}\n",
		sHeader
	);
	iRet = xgeShaderCreate(&g_xgeShapeExStrokeUnionRenderer.tShader, sVertex, sFragment);
	if ( iRet != XGE_OK ) return iRet;
	g_xgeShapeExStrokeUnionRenderer.bInitialized = 1;
	return XGE_OK;
}

static int __xgeShapeExStrokeUnionTargetEnsure(int iWidth, int iHeight)
{
	if ( (iWidth <= 0) || (iHeight <= 0) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( g_xgeShapeExStrokeUnionRenderer.bMaskTargetValid &&
	     (g_xgeShapeExStrokeUnionRenderer.tMaskTarget.iWidth == iWidth) &&
	     (g_xgeShapeExStrokeUnionRenderer.tMaskTarget.iHeight == iHeight) ) {
		return XGE_OK;
	}
	if ( !g_xgeShapeExStrokeUnionRenderer.bMaskTargetValid ) {
		if ( xgeRenderTargetCreate(
			&g_xgeShapeExStrokeUnionRenderer.tMaskTarget, iWidth, iHeight
		) != XGE_OK ) return XGE_ERROR_GPU_FAILED;
		g_xgeShapeExStrokeUnionRenderer.bMaskTargetValid = 1;
		return XGE_OK;
	}
	if ( xgeRenderTargetResize(
		&g_xgeShapeExStrokeUnionRenderer.tMaskTarget, iWidth, iHeight
	) != XGE_OK ) {
		g_xgeShapeExStrokeUnionRenderer.bMaskTargetValid = 0;
		return XGE_ERROR_GPU_FAILED;
	}
	return XGE_OK;
}

static int __xgeShapeExStrokeUnionMaskDraw(void* pUser, xge_shape_ex_matrix_t tLocalParent)
{
	xge_shape_ex_stroke_union_context_t* pContext = (xge_shape_ex_stroke_union_context_t*)pUser;
	struct xge_shape_ex_t tMaskShape;
	const xge_shape_ex_matrix_t* pGeometryMatrix;

	(void)tLocalParent;
	if ( (pContext == NULL) || !__xgeShapeExValid(pContext->pShape) || (pContext->pFlat == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	xgeCameraSet(&pContext->tCamera);
	if ( pContext->bViewport ) xgeViewportSet(pContext->tViewport);
	tMaskShape = *pContext->pShape;
	tMaskShape.iStrokeType = XGE_SHAPE_EX_FILL_SOLID;
	tMaskShape.iStrokeColor = XGE_COLOR_RGBA(255, 255, 255, 255);
	tMaskShape.pStrokeStops = NULL;
	tMaskShape.iStrokeStopCount = 0;
	tMaskShape.iStrokeStopCapacity = 0;
	tMaskShape.bStrokeUnionMaskDraw = 1;
	tMaskShape.bCoverageAntialiasDisabled = 0;
	tMaskShape.fOpacity = 1.0f;
	pGeometryMatrix = pContext->bGeometryMatrix ? &pContext->tGeometryMatrix : NULL;
	return __xgeShapeExDrawStrokeUnionMaskGeometry(
		&tMaskShape, pContext->pFlat, pContext->fWidth, XGE_COLOR_RGBA(255, 255, 255, 255),
		pContext->tBounds, pContext->tMatrix, pGeometryMatrix, 1.0f, pContext->bScreenSpace
	);
}

static int __xgeShapeExDrawStrokeUnionComposite(
	xge_shape_ex pShape,
	const xge_shape_ex_flat_path_t* pFlat,
	float fWidth,
	uint32_t iColor,
	xge_rect_t tBounds,
	xge_shape_ex_matrix_t tMatrix,
	const xge_shape_ex_matrix_t* pGeometryMatrix,
	float fOpacity,
	int bScreenSpace
)
{
	xge_shape_ex_stroke_union_context_t tContext;
	xge_shape_ex_stencil_state_t tStencilState;
	xge_render_target pMask;
	xge_material_t tMaterial;
	xge_draw_t tDraw;
	xge_rect_t tViewport;
	xge_rect_t tFrame;
	int iOutputBlend;
	int iWidth;
	int iHeight;
	int bViewport;
	int iRet;

	(void)fOpacity;
	if ( !__xgeShapeExValid(pShape) || (pFlat == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	bViewport = g_xge.bViewportEnabled;
	tViewport = xgeViewportGet();
	iWidth = g_xge.iWidth;
	iHeight = g_xge.iHeight;
	if ( (iWidth <= 0) || (iHeight <= 0) ) return XGE_OK;
	tFrame = (xge_rect_t){0.0f, 0.0f, (float)iWidth, (float)iHeight};
	iRet = __xgeShapeAutoBatchFlush();
	if ( iRet != XGE_OK ) return iRet;
	iRet = __xgeShapeExStrokeUnionRendererEnsure();
	if ( iRet != XGE_OK ) return iRet;
	iRet = __xgeShapeExStrokeUnionTargetEnsure(iWidth, iHeight);
	if ( iRet != XGE_OK ) return iRet;
	pMask = &g_xgeShapeExStrokeUnionRenderer.tMaskTarget;
	memset(&tContext, 0, sizeof(tContext));
	tContext.pShape = pShape;
	tContext.pFlat = pFlat;
	tContext.fWidth = fWidth;
	tContext.tBounds = tBounds;
	tContext.tMatrix = tMatrix;
	tContext.tCamera = xgeCameraGet();
	tContext.tViewport = tViewport;
	tContext.bViewport = bViewport;
	if ( pGeometryMatrix != NULL ) {
		tContext.tGeometryMatrix = *pGeometryMatrix;
		tContext.bGeometryMatrix = 1;
	}
	tContext.bScreenSpace = bScreenSpace;
	iOutputBlend = xgeBlendGet();
	__xgeShapeExStencilSuspend(&tStencilState);
	iRet = __xgeShapeExDrawToTarget(
		pMask, __xgeShapeExStrokeUnionMaskDraw, &tContext, __xgeShapeExMatrixIdentity()
	);
	__xgeShapeExStencilResume(&tStencilState);
	if ( iRet != XGE_OK ) return iRet;
	xgeMaterialInit(&tMaterial);
	xgeMaterialSetShader(&tMaterial, &g_xgeShapeExStrokeUnionRenderer.tShader);
	xgeMaterialSetTexture(&tMaterial, xgeRenderTargetTexture(pMask));
	xgeMaterialSetColor(&tMaterial, iColor);
	xgeMaterialSetBlend(&tMaterial, iOutputBlend);
	memset(&tDraw, 0, sizeof(tDraw));
	tDraw.pTexture = xgeRenderTargetTexture(pMask);
	tDraw.tSrc = (xge_rect_t){0.0f, 0.0f, (float)iWidth, (float)iHeight};
	tDraw.tDst = tFrame;
	tDraw.iColor = XGE_COLOR_RGBA(255, 255, 255, 255);
	tDraw.iFlags = XGE_DRAW_SCREEN_SPACE | XGE_DRAW_FLIP_Y;
	if ( bViewport ) xgeViewportClear();
	xgeMaterialDraw(&tMaterial, &tDraw);
	xgeMaterialFree(&tMaterial);
	iRet = xgeFlush();
	if ( bViewport ) xgeViewportSet(tViewport);
	return iRet;
}

static int __xgeShapeExCompositeMask(
	int iMethod,
	int iOutputBlend,
	xge_rect_t tBounds,
	xge_shape_ex_blend_draw_proc pSourceDraw,
	void* pSourceUser,
	xge_shape_ex_blend_draw_proc pMaskDraw,
	void* pMaskUser,
	xge_shape_ex_matrix_t tScreenParent,
	int bQuantizeMatting
)
{
	xge_render_target_t tSource;
	xge_render_target_t tMask;
	xge_render_target pSource;
	xge_render_target pMask;
	xge_shape_ex_mask_target_set_t* pCachedTargets;
	xge_shape_ex_stencil_state_t tStencilState;
	xge_material_t tMaterial;
	xge_draw_t tDraw;
	xge_rect_t tOldClip;
	xge_rect_t tFrame;
	xge_shape_ex_matrix_t tOffset;
	xge_shape_ex_matrix_t tLocalParent;
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;
	int bOldClip;
	int iWidth;
	int iHeight;
	int bAlphaMaskA8;
	int bCachedTargets;
	int bStateSuspended;
	int iRet;

	if ( !__xgeShapeExMaskMethodValid(iMethod) || !__xgeShapeExBlendValid(iOutputBlend) ||
	     (pSourceDraw == NULL) || (pMaskDraw == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tFrame = (xge_rect_t){0.0f, 0.0f, (float)g_xge.iWidth, (float)g_xge.iHeight};
	tBounds = __xgeShapeExRectIntersect(tBounds, tFrame);
	bOldClip = g_xge.bClipEnabled;
	tOldClip = xgeClipGet();
	if ( bOldClip ) tBounds = __xgeShapeExRectIntersect(tBounds, tOldClip);
	if ( (tBounds.fW <= 0.0f) || (tBounds.fH <= 0.0f) ) return XGE_OK;
	fLeft = floorf(tBounds.fX);
	fTop = floorf(tBounds.fY);
	fRight = ceilf(tBounds.fX + tBounds.fW);
	fBottom = ceilf(tBounds.fY + tBounds.fH);
	if ( fLeft < 0.0f ) fLeft = 0.0f;
	if ( fTop < 0.0f ) fTop = 0.0f;
	if ( fRight > (float)g_xge.iWidth ) fRight = (float)g_xge.iWidth;
	if ( fBottom > (float)g_xge.iHeight ) fBottom = (float)g_xge.iHeight;
	iWidth = (int)(fRight - fLeft);
	iHeight = (int)(fBottom - fTop);
	bAlphaMaskA8 = (bQuantizeMatting != 0) &&
		((iMethod == XGE_SHAPE_EX_MASK_ALPHA) ||
		 (iMethod == XGE_SHAPE_EX_MASK_INV_ALPHA));
	if ( (iWidth <= 0) || (iHeight <= 0) ) return XGE_OK;
	if ( glGetIntegerv != NULL ) {
		GLint iMaxTextureSize = 0;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &iMaxTextureSize);
		if ( (iMaxTextureSize > 0) && ((iWidth > iMaxTextureSize) || (iHeight > iMaxTextureSize)) ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
	}
	memset(&tSource, 0, sizeof(tSource));
	memset(&tMask, 0, sizeof(tMask));
	pSource = &tSource;
	pMask = &tMask;
	pCachedTargets = NULL;
	bCachedTargets =
		g_xgeShapeExMaskCompositeTargetDepth < XGE_SHAPE_EX_MASK_TARGET_CACHE_MAX;
	if ( bCachedTargets ) {
		pCachedTargets = &g_xgeShapeExMaskRenderer.arrTargets[
			g_xgeShapeExMaskCompositeTargetDepth
		];
		pSource = &pCachedTargets->tSource;
		pMask = &pCachedTargets->tMask;
	}
	g_xgeShapeExMaskCompositeTargetDepth++;
	bStateSuspended = 0;
	iRet = __xgeShapeAutoBatchFlush();
	if ( iRet == XGE_OK ) {
		xgeClipClear();
		__xgeShapeExStencilSuspend(&tStencilState);
		bStateSuspended = 1;
	} else {
		memset(&tStencilState, 0, sizeof(tStencilState));
	}
	if ( (iRet == XGE_OK) && bCachedTargets ) {
		iRet = __xgeShapeExMaskTargetSetEnsure(pCachedTargets, iWidth, iHeight);
	} else if ( iRet == XGE_OK ) {
		iRet = xgeRenderTargetCreate(pSource, iWidth, iHeight);
		if ( iRet == XGE_OK ) iRet = xgeRenderTargetCreate(pMask, iWidth, iHeight);
	}
	tOffset = __xgeShapeExMatrixIdentity();
	tOffset.fE = -fLeft;
	tOffset.fF = -fTop;
	tLocalParent = __xgeShapeExMatrixMul(tOffset, tScreenParent);
	if ( iRet == XGE_OK ) {
		g_xgeShapeExMaskCompositeSourceDepth++;
		iRet = __xgeShapeExDrawToTarget(pSource, pSourceDraw, pSourceUser, tLocalParent);
		g_xgeShapeExMaskCompositeSourceDepth--;
	}
	if ( iRet == XGE_OK ) {
		g_xgeShapeExStencilActiveMask = 0;
		g_xgeShapeExStencilContextCount = 0;
		memset(g_xgeShapeExStencilContextMasks, 0, sizeof(g_xgeShapeExStencilContextMasks));
		if ( glDisable != NULL ) glDisable(GL_STENCIL_TEST);
		if ( bAlphaMaskA8 ) g_xgeShapeExAlphaMaskTargetDepth++;
		iRet = __xgeShapeExDrawToTarget(pMask, pMaskDraw, pMaskUser, tLocalParent);
		if ( bAlphaMaskA8 ) g_xgeShapeExAlphaMaskTargetDepth--;
	}
	if ( bStateSuspended ) {
		__xgeShapeExStencilResume(&tStencilState);
	}
	if ( bOldClip ) xgeClipSet(tOldClip);
	else xgeClipClear();
	if ( iRet == XGE_OK ) iRet = __xgeShapeExMaskRendererEnsure();
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform1i(&g_xgeShapeExMaskRenderer.tShader, "uMaskMethod", iMethod);
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform1i(
			&g_xgeShapeExMaskRenderer.tShader, "uQuantizeMatting",
			(bQuantizeMatting != 0) && (g_xgeShapeExBlendCompositeSourceDepth <= 0)
		);
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform1i(
			&g_xgeShapeExMaskRenderer.tShader, "uAlphaMaskA8", bAlphaMaskA8
		);
	}
	if ( iRet == XGE_OK ) {
		xgeMaterialInit(&tMaterial);
		xgeMaterialSetShader(&tMaterial, &g_xgeShapeExMaskRenderer.tShader);
		xgeMaterialSetTexture(&tMaterial, xgeRenderTargetTexture(pSource));
		xgeMaterialSetTexture2(&tMaterial, xgeRenderTargetTexture(pMask));
		xgeMaterialSetBlend(&tMaterial, iOutputBlend);
		memset(&tDraw, 0, sizeof(tDraw));
		tDraw.pTexture = xgeRenderTargetTexture(pSource);
		tDraw.tSrc = (xge_rect_t){0.0f, 0.0f, (float)iWidth, (float)iHeight};
		tDraw.tDst = (xge_rect_t){fLeft, fTop, (float)iWidth, (float)iHeight};
		tDraw.iColor = XGE_COLOR_RGBA(255, 255, 255, 255);
		tDraw.iFlags = XGE_DRAW_SCREEN_SPACE | XGE_DRAW_FLIP_Y;
		xgeMaterialDraw(&tMaterial, &tDraw);
		xgeMaterialFree(&tMaterial);
		iRet = xgeFlush();
	}
	if ( !bCachedTargets ) {
		xgeRenderTargetFree(&tMask);
		xgeRenderTargetFree(&tSource);
	}
	g_xgeShapeExMaskCompositeTargetDepth--;
	return iRet;
}

static xge_shape_ex_matrix_t __xgeShapeExScreenParent(xge_shape_ex_matrix_t tParent, int bScreenSpace)
{
	xge_shape_ex_matrix_t tScreen;
	xge_vec2_t tP0;
	xge_vec2_t tPX;
	xge_vec2_t tPY;

	if ( bScreenSpace ) {
		return tParent;
	}
	tP0 = xgeWorldToScreen(__xgeShapeExMatrixPoint(tParent, (xge_vec2_t){0.0f, 0.0f}));
	tPX = xgeWorldToScreen(__xgeShapeExMatrixPoint(tParent, (xge_vec2_t){1.0f, 0.0f}));
	tPY = xgeWorldToScreen(__xgeShapeExMatrixPoint(tParent, (xge_vec2_t){0.0f, 1.0f}));
	tScreen.fA = tPX.fX - tP0.fX;
	tScreen.fB = tPX.fY - tP0.fY;
	tScreen.fC = tPY.fX - tP0.fX;
	tScreen.fD = tPY.fY - tP0.fY;
	tScreen.fE = tP0.fX;
	tScreen.fF = tP0.fY;
	return tScreen;
}

typedef struct xge_shape_ex_gradient_mask_context_t {
	xge_shape_ex pShape;
	float fTolerance;
	int iPaint;
	int bBinaryCoverage;
} xge_shape_ex_gradient_mask_context_t;

static void __xgeShapeExGradientPaintOnly(
	xge_shape_ex_t* pPaint,
	xge_shape_ex pShape,
	int iPaint,
	int bSolidMask
)
{
	*pPaint = *pShape;
	pPaint->bBlendSet = 0;
	pPaint->iBlend = XGE_BLEND_ALPHA;
	pPaint->iMaskMethod = XGE_SHAPE_EX_MASK_NONE;
	pPaint->iMaskTargetType = XGE_SHAPE_EX_MASK_TARGET_NONE;
	pPaint->pMaskShape = NULL;
	pPaint->pMaskScene = NULL;
	pPaint->bClipRect = 0;
	pPaint->pClipShapes = NULL;
	pPaint->pClipShapeModes = NULL;
	pPaint->iClipShapeCount = 0;
	pPaint->iClipShapeCapacity = 0;
	if ( iPaint == XGE_SHAPE_EX_PAINT_FILL ) {
		pPaint->fStrokeWidth = 0.0f;
		pPaint->iStrokeType = XGE_SHAPE_EX_FILL_SOLID;
		pPaint->iStrokeColor = XGE_COLOR_RGBA(0, 0, 0, 0);
		pPaint->pStrokeStops = NULL;
		pPaint->iStrokeStopCount = 0;
		pPaint->iStrokeStopCapacity = 0;
		if ( bSolidMask ) {
			pPaint->iFillType = XGE_SHAPE_EX_FILL_SOLID;
			pPaint->iFillColor = XGE_COLOR_RGBA(255, 255, 255, 255);
			pPaint->pFillStops = NULL;
			pPaint->iFillStopCount = 0;
			pPaint->iFillStopCapacity = 0;
		}
	} else {
		pPaint->iFillType = XGE_SHAPE_EX_FILL_SOLID;
		pPaint->iFillColor = XGE_COLOR_RGBA(0, 0, 0, 0);
		pPaint->pFillStops = NULL;
		pPaint->iFillStopCount = 0;
		pPaint->iFillStopCapacity = 0;
		if ( bSolidMask ) {
			pPaint->iStrokeType = XGE_SHAPE_EX_FILL_SOLID;
			pPaint->iStrokeColor = XGE_COLOR_RGBA(255, 255, 255, 255);
			pPaint->pStrokeStops = NULL;
			pPaint->iStrokeStopCount = 0;
			pPaint->iStrokeStopCapacity = 0;
		}
	}
}

static int __xgeShapeExGradientMaskDraw(void* pUser, xge_shape_ex_matrix_t tLocalParent)
{
	xge_shape_ex_gradient_mask_context_t* pContext = (xge_shape_ex_gradient_mask_context_t*)pUser;
	xge_shape_ex_t tPaint;

	if ( (pContext == NULL) || !__xgeShapeExValid(pContext->pShape) ||
	     ((pContext->iPaint != XGE_SHAPE_EX_PAINT_FILL) &&
	      (pContext->iPaint != XGE_SHAPE_EX_PAINT_STROKE)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	__xgeShapeExGradientPaintOnly(&tPaint, pContext->pShape, pContext->iPaint, 1);
	tPaint.fOpacity = 1.0f;
	tPaint.bCoverageAntialiasDisabled = pContext->bBinaryCoverage;
	if ( pContext->iPaint == XGE_SHAPE_EX_PAINT_STROKE ) {
		tPaint.bStrokeUnionMaskDraw = 1;
	}
	return __xgeShapeExDrawDispatch(
		&tPaint, pContext->fTolerance, 1, tLocalParent, 1.0f, 1
	);
}

static int __xgeShapeExGradientSampleMatrix(
	int iUnits,
	xge_shape_ex_matrix_t tGradientTransform,
	xge_rect_t tPaintBounds,
	xge_shape_ex_matrix_t tPaintMatrix,
	const xge_shape_ex_matrix_t* pGeometryMatrix,
	int bScreenSpace,
	xge_shape_ex_matrix_t* pSampleMatrix
)
{
	xge_shape_ex_matrix_t tBase;
	xge_shape_ex_matrix_t tPaintToScreen;
	xge_shape_ex_matrix_t tScreenToPaint;
	xge_shape_ex_matrix_t tUnits;
	xge_shape_ex_matrix_t tGradientInverse;
	float fW;
	float fH;

	if ( pSampleMatrix == NULL ) return 0;
	tBase = __xgeShapeExScreenParent(__xgeShapeExMatrixIdentity(), bScreenSpace);
	tPaintToScreen = pGeometryMatrix != NULL ?
		__xgeShapeExMatrixMul(tBase, *pGeometryMatrix) : tBase;
	if ( !__xgeShapeExMatrixInverse(tPaintToScreen, &tScreenToPaint) ) return 0;
	if ( iUnits == XGE_SHAPE_EX_GRADIENT_USER_SPACE ) {
		if ( !__xgeShapeExMatrixInverse(tPaintMatrix, &tUnits) ) return 0;
	} else {
		fW = fabsf(tPaintBounds.fW) > XGE_SHAPE_EX_EPSILON ? tPaintBounds.fW : 1.0f;
		fH = fabsf(tPaintBounds.fH) > XGE_SHAPE_EX_EPSILON ? tPaintBounds.fH : 1.0f;
		tUnits = __xgeShapeExMatrixIdentity();
		tUnits.fA = 1.0f / fW;
		tUnits.fD = 1.0f / fH;
		tUnits.fE = -tPaintBounds.fX / fW;
		tUnits.fF = -tPaintBounds.fY / fH;
	}
	if ( !__xgeShapeExMatrixInverse(tGradientTransform, &tGradientInverse) ) return 0;
	*pSampleMatrix = __xgeShapeExMatrixMul(
		tGradientInverse,
		__xgeShapeExMatrixMul(tUnits, tScreenToPaint)
	);
	return 1;
}

static int __xgeShapeExDrawGradientSolidFallback(
	xge_shape_ex pShape,
	int iPaint,
	float fTolerance,
	int bScreenSpace,
	xge_shape_ex_matrix_t tParent,
	float fOpacity,
	uint32_t iColor
)
{
	xge_shape_ex_t tPaint;

	__xgeShapeExGradientPaintOnly(&tPaint, pShape, iPaint, 1);
	if ( iPaint == XGE_SHAPE_EX_PAINT_FILL ) tPaint.iFillColor = iColor;
	else tPaint.iStrokeColor = iColor;
	tPaint.fOpacity = 1.0f;
	return __xgeShapeExDrawDispatch(
		&tPaint, fTolerance, bScreenSpace, tParent, fOpacity, 1
	);
}

static int __xgeShapeExDrawGradientComposite(
	xge_shape_ex pShape,
	int iPaint,
	float fTolerance,
	int bScreenSpace,
	xge_shape_ex_matrix_t tParent,
	float fOpacity,
	xge_rect_t tPaintBounds,
	xge_shape_ex_matrix_t tPaintMatrix,
	const xge_shape_ex_matrix_t* pGeometryMatrix
)
{
	const xge_shape_ex_color_stop_t* pStops;
	xge_shape_ex_gradient_mask_context_t tContext;
	xge_shape_ex_stencil_state_t tStencilState;
	xge_shape_ex_matrix_t tGradientTransform;
	xge_shape_ex_matrix_t tSampleMatrix;
	xge_shape_ex_matrix_t tScreenParent;
	xge_shape_ex_matrix_t tOffset;
	xge_shape_ex_matrix_t tLocalParent;
	xge_shape_ex_t tBoundsPaint;
	xge_render_target pMask;
	xge_material_t tMaterial;
	xge_draw_t tDraw;
	xge_rect_t tBounds;
	xge_rect_t tFrame;
	xge_rect_t tOldClip;
	xge_vec2_t tCenter;
	xge_vec2_t tFocal;
	xge_vec2_t tLinearStart;
	xge_vec2_t tLinearEnd;
	uint32_t iFallbackColor;
	float fRadius;
	float fFocalRadius;
	float fRepeatLength;
	float fLinearBaseX;
	float fLinearTStep;
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;
	int iUnits;
	int iSpread;
	int iStopCount;
	int iOutputBlend;
	int iWidth;
	int iHeight;
	int iGradientType;
	int bLinearFixed;
	int bFocalClamped;
	int bOldClip;
	int iRet;

	if ( !__xgeShapeExValid(pShape) ||
	     ((iPaint != XGE_SHAPE_EX_PAINT_FILL) && (iPaint != XGE_SHAPE_EX_PAINT_STROKE)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iPaint == XGE_SHAPE_EX_PAINT_FILL ) {
		iGradientType = pShape->iFillType == XGE_SHAPE_EX_FILL_RADIAL_GRADIENT ? 1 : 0;
		pStops = pShape->pFillStops;
		iStopCount = pShape->iFillStopCount;
		iFallbackColor = pShape->iFillColor;
		tCenter = (xge_vec2_t){pShape->fFillX1, pShape->fFillY1};
		tFocal = (xge_vec2_t){pShape->fFillFX, pShape->fFillFY};
		tLinearStart = (xge_vec2_t){pShape->fFillX1, pShape->fFillY1};
		tLinearEnd = (xge_vec2_t){pShape->fFillX2, pShape->fFillY2};
		fRadius = pShape->fFillR;
		fFocalRadius = pShape->fFillFR;
		iUnits = pShape->iFillGradientUnits;
		iSpread = pShape->iFillGradientSpread;
		tGradientTransform = pShape->tFillGradientTransform;
	} else {
		iGradientType = pShape->iStrokeType == XGE_SHAPE_EX_FILL_RADIAL_GRADIENT ? 1 : 0;
		pStops = pShape->pStrokeStops;
		iStopCount = pShape->iStrokeStopCount;
		iFallbackColor = pShape->iStrokeColor;
		tCenter = (xge_vec2_t){pShape->fStrokeX1, pShape->fStrokeY1};
		tFocal = (xge_vec2_t){pShape->fStrokeFX, pShape->fStrokeFY};
		tLinearStart = (xge_vec2_t){pShape->fStrokeX1, pShape->fStrokeY1};
		tLinearEnd = (xge_vec2_t){pShape->fStrokeX2, pShape->fStrokeY2};
		fRadius = pShape->fStrokeR;
		fFocalRadius = pShape->fStrokeFR;
		iUnits = pShape->iStrokeGradientUnits;
		iSpread = pShape->iStrokeGradientSpread;
		tGradientTransform = pShape->tStrokeGradientTransform;
	}
	if ( (pStops == NULL) || (iStopCount <= 0) ) {
		return __xgeShapeExDrawGradientSolidFallback(
			pShape, iPaint, fTolerance, bScreenSpace, tParent, fOpacity, iFallbackColor
		);
	}
	if ( !__xgeShapeExGradientSampleMatrix(
		iUnits, tGradientTransform, tPaintBounds, tPaintMatrix,
		pGeometryMatrix, bScreenSpace, &tSampleMatrix
	) ) {
		return __xgeShapeExDrawGradientSolidFallback(
			pShape, iPaint, fTolerance, bScreenSpace, tParent, fOpacity,
			pStops[iStopCount - 1].iColor
		);
	}
	bFocalClamped = 0;
	bLinearFixed = (iGradientType == 0) &&
		(iPaint == XGE_SHAPE_EX_PAINT_FILL) &&
		(iSpread == XGE_SHAPE_EX_GRADIENT_SPREAD_REPEAT);
	if ( (iGradientType == 1) && (fRadius >= 0.02f) ) {
		bFocalClamped = __xgeShapeExRadialGradientCorrect(
			tCenter, fRadius, &tFocal, &fFocalRadius
		);
	}
	tScreenParent = __xgeShapeExScreenParent(tParent, bScreenSpace);
	__xgeShapeExGradientPaintOnly(&tBoundsPaint, pShape, iPaint, 0);
	iRet = __xgeShapeExGetBoundsInternal(&tBoundsPaint, fTolerance, tScreenParent, &tBounds);
	if ( iRet != XGE_OK ) return iRet;
	if ( (iPaint == XGE_SHAPE_EX_PAINT_STROKE) && !pShape->bStrokeNonScaling ) {
		xge_shape_ex_matrix_t tGeometryToScreen;
		xge_rect_t tLocalBounds;
		xge_rect_t tGeometryBounds;

		iRet = __xgeShapeExGetLocalBoundsEx(&tBoundsPaint, fTolerance, &tLocalBounds, 0);
		if ( iRet != XGE_OK ) return iRet;
		tGeometryToScreen = __xgeShapeExMatrixMul(tScreenParent, pShape->tTransform);
		tGeometryBounds = __xgeShapeExMatrixRectBounds(tGeometryToScreen, tLocalBounds);
		tBounds = __xgeShapeExRectUnion(tBounds, tGeometryBounds);
	}
	tFrame = (xge_rect_t){0.0f, 0.0f, (float)g_xge.iWidth, (float)g_xge.iHeight};
	tBounds = __xgeShapeExRectIntersect(tBounds, tFrame);
	bOldClip = g_xge.bClipEnabled;
	tOldClip = xgeClipGet();
	if ( bOldClip ) tBounds = __xgeShapeExRectIntersect(tBounds, tOldClip);
	if ( (tBounds.fW <= 0.0f) || (tBounds.fH <= 0.0f) ) return XGE_OK;
	fLinearBaseX = floorf(tBounds.fX);
	fLinearTStep = 0.0f;
	if ( iGradientType == 0 ) {
		float fDX = tLinearEnd.fX - tLinearStart.fX;
		float fDY = tLinearEnd.fY - tLinearStart.fY;
		float fLenSq = fDX * fDX + fDY * fDY;

		if ( fLenSq > XGE_SHAPE_EX_EPSILON ) {
			fLinearTStep = (tSampleMatrix.fA * fDX + tSampleMatrix.fB * fDY) / fLenSq;
		}
	}
	fLeft = floorf(tBounds.fX) - 1.0f;
	fTop = floorf(tBounds.fY) - 1.0f;
	fRight = ceilf(tBounds.fX + tBounds.fW) + 1.0f;
	fBottom = ceilf(tBounds.fY + tBounds.fH) + 1.0f;
	if ( fLeft < 0.0f ) fLeft = 0.0f;
	if ( fTop < 0.0f ) fTop = 0.0f;
	if ( fRight > (float)g_xge.iWidth ) fRight = (float)g_xge.iWidth;
	if ( fBottom > (float)g_xge.iHeight ) fBottom = (float)g_xge.iHeight;
	iWidth = (int)(fRight - fLeft);
	iHeight = (int)(fBottom - fTop);
	if ( (iWidth <= 0) || (iHeight <= 0) ) return XGE_OK;
	if ( glGetIntegerv != NULL ) {
		GLint iMaxTextureSize = 0;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &iMaxTextureSize);
		if ( (iMaxTextureSize > 0) && ((iWidth > iMaxTextureSize) || (iHeight > iMaxTextureSize)) ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
	}
	iRet = __xgeShapeExGradientRendererEnsure();
	if ( iRet != XGE_OK ) return iRet;
	if ( iGradientType == 1 ) {
		fRepeatLength = fabsf(fRadius);
	} else {
		float fDX = tLinearEnd.fX - tLinearStart.fX;
		float fDY = tLinearEnd.fY - tLinearStart.fY;

		fRepeatLength = sqrtf(fDX * fDX + fDY * fDY);
	}
	iRet = __xgeShapeExGradientColorTableUpdate(
		pStops, iStopCount, iFallbackColor, iSpread, fRepeatLength, fOpacity
	);
	if ( iRet != XGE_OK ) return iRet;
	iRet = __xgeShapeAutoBatchFlush();
	if ( iRet == XGE_OK ) iRet = __xgeShapeExGradientMaskTargetEnsure(iWidth, iHeight);
	if ( iRet != XGE_OK ) return iRet;
	pMask = &g_xgeShapeExGradientRenderer.tMaskTarget;
	iOutputBlend = xgeBlendGet();
	xgeClipClear();
	__xgeShapeExStencilSuspend(&tStencilState);
	xgeBlendSet(XGE_BLEND_ALPHA);
	tOffset = __xgeShapeExMatrixIdentity();
	tOffset.fE = -fLeft;
	tOffset.fF = -fTop;
	tLocalParent = __xgeShapeExMatrixMul(tOffset, tScreenParent);
	tContext.pShape = pShape;
	tContext.fTolerance = fTolerance;
	tContext.iPaint = iPaint;
	tContext.bBinaryCoverage = pShape->bCoverageAntialiasDisabled;
	iRet = __xgeShapeExDrawToTarget(
		pMask, __xgeShapeExGradientMaskDraw, &tContext, tLocalParent
	);
	xgeBlendSet(iOutputBlend);
	__xgeShapeExStencilResume(&tStencilState);
	if ( bOldClip ) xgeClipSet(tOldClip);
	else xgeClipClear();
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform4f(
			&g_xgeShapeExGradientRenderer.tShader, "uBounds",
			fLeft, fTop, (float)iWidth, (float)iHeight
		);
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform2f(
			&g_xgeShapeExGradientRenderer.tShader, "uMaskUVSize",
			(float)iWidth / (float)pMask->iWidth,
			(float)iHeight / (float)pMask->iHeight
		);
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform4f(
			&g_xgeShapeExGradientRenderer.tShader, "uLinear0",
			tLinearStart.fX, tLinearStart.fY, tLinearEnd.fX, tLinearEnd.fY
		);
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform2f(
			&g_xgeShapeExGradientRenderer.tShader, "uLinearFixed",
			fLinearBaseX, fLinearTStep
		);
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform3f(
			&g_xgeShapeExGradientRenderer.tShader, "uSampleRow0",
			tSampleMatrix.fA, tSampleMatrix.fC, tSampleMatrix.fE
		);
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform3f(
			&g_xgeShapeExGradientRenderer.tShader, "uSampleRow1",
			tSampleMatrix.fB, tSampleMatrix.fD, tSampleMatrix.fF
		);
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform3f(
			&g_xgeShapeExGradientRenderer.tShader, "uRadial0",
			tCenter.fX, tCenter.fY, fRadius
		);
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform3f(
			&g_xgeShapeExGradientRenderer.tShader, "uRadial1",
			tFocal.fX, tFocal.fY, fFocalRadius
		);
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform1i(
			&g_xgeShapeExGradientRenderer.tShader, "uGradientType", iGradientType
		);
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform1i(
			&g_xgeShapeExGradientRenderer.tShader, "uSpread", iSpread
		);
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform1i(
			&g_xgeShapeExGradientRenderer.tShader, "uLinearFixedEnabled", bLinearFixed
		);
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform1i(
			&g_xgeShapeExGradientRenderer.tShader, "uFocalClamped", bFocalClamped
		);
	}
	if ( iRet == XGE_OK ) {
		xgeMaterialInit(&tMaterial);
		xgeMaterialSetShader(&tMaterial, &g_xgeShapeExGradientRenderer.tShader);
		xgeMaterialSetTexture(&tMaterial, xgeRenderTargetTexture(pMask));
		xgeMaterialSetTexture2(&tMaterial, &g_xgeShapeExGradientRenderer.tColorTable);
		xgeMaterialSetBlend(&tMaterial, iOutputBlend);
		memset(&tDraw, 0, sizeof(tDraw));
		tDraw.pTexture = xgeRenderTargetTexture(pMask);
		tDraw.tSrc = (xge_rect_t){
			0.0f, (float)(pMask->iHeight - iHeight), (float)iWidth, (float)iHeight
		};
		tDraw.tDst = (xge_rect_t){fLeft, fTop, (float)iWidth, (float)iHeight};
		tDraw.iColor = XGE_COLOR_RGBA(255, 255, 255, 255);
		tDraw.iFlags = XGE_DRAW_SCREEN_SPACE | XGE_DRAW_FLIP_Y;
		xgeMaterialDraw(&tMaterial, &tDraw);
		xgeMaterialFree(&tMaterial);
		iRet = xgeFlush();
	}
	return iRet;
}

static int __xgeShapeExCompositeBlend(
	int iBlend,
	int iSource,
	float fOpacity,
	xge_rect_t tBounds,
	xge_shape_ex_blend_draw_proc pDraw,
	void* pUser,
	xge_shape_ex_blend_draw_proc pCoverageDraw,
	void* pCoverageUser,
	xge_shape_ex_matrix_t tScreenParent
)
{
	xge_render_target_t tSource;
	xge_render_target_t tDestination;
	xge_render_target_t tCoverage;
	xge_render_target pSource;
	xge_render_target pDestination;
	xge_render_target pCoverage;
	xge_shape_ex_blend_target_set_t* pCachedTargets;
	xge_shape_ex_stencil_state_t tStencilState;
	xge_shape_ex_matrix_t tOffset;
	xge_shape_ex_matrix_t tLocalParent;
	xge_pass_t tPass;
	xge_material_t tMaterial;
	xge_draw_t tDraw;
	xge_rect_t tOldClip;
	xge_rect_t tFrame;
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;
	int bOldClip;
	int iWidth;
	int iHeight;
	int bCachedTargets;
	int iRet;

	if ( ((iBlend != XGE_BLEND_ALPHA) && !__xgeShapeExBlendNeedsComposite(iBlend)) ||
	     ((iSource != XGE_SHAPE_EX_BLEND_SOURCE_SCENE) &&
	      (iSource != XGE_SHAPE_EX_BLEND_SOURCE_SHAPE) &&
	      (iSource != XGE_SHAPE_EX_BLEND_SOURCE_GRADIENT) &&
	      (iSource != XGE_SHAPE_EX_BLEND_SOURCE_MASKED) &&
	      (iSource != XGE_SHAPE_EX_BLEND_SOURCE_MASKED_GROUP) &&
	      (iSource != XGE_SHAPE_EX_BLEND_SOURCE_INHERITED_OPACITY)) ||
	     !__xgeShapeExFloatFinite(fOpacity) ||
	     (pDraw == NULL) ||
	     ((iSource == XGE_SHAPE_EX_BLEND_SOURCE_GRADIENT) && (pCoverageDraw == NULL)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fOpacity <= 0.0f ) return XGE_OK;
	if ( fOpacity > 1.0f ) fOpacity = 1.0f;
	tFrame = (xge_rect_t){0.0f, 0.0f, (float)g_xge.iWidth, (float)g_xge.iHeight};
	tBounds = __xgeShapeExRectIntersect(tBounds, tFrame);
	bOldClip = g_xge.bClipEnabled;
	tOldClip = xgeClipGet();
	if ( bOldClip ) {
		tBounds = __xgeShapeExRectIntersect(tBounds, tOldClip);
	}
	if ( (tBounds.fW <= 0.0f) || (tBounds.fH <= 0.0f) ) {
		return XGE_OK;
	}
	fLeft = floorf(tBounds.fX);
	fTop = floorf(tBounds.fY);
	fRight = ceilf(tBounds.fX + tBounds.fW);
	fBottom = ceilf(tBounds.fY + tBounds.fH);
	if ( fLeft < 0.0f ) fLeft = 0.0f;
	if ( fTop < 0.0f ) fTop = 0.0f;
	if ( fRight > (float)g_xge.iWidth ) fRight = (float)g_xge.iWidth;
	if ( fBottom > (float)g_xge.iHeight ) fBottom = (float)g_xge.iHeight;
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
	memset(&tSource, 0, sizeof(tSource));
	memset(&tDestination, 0, sizeof(tDestination));
	memset(&tCoverage, 0, sizeof(tCoverage));
	memset(&tPass, 0, sizeof(tPass));
	pSource = &tSource;
	pDestination = &tDestination;
	pCoverage = &tCoverage;
	pCachedTargets = NULL;
	bCachedTargets =
		g_xgeShapeExBlendCompositeSourceDepth < XGE_SHAPE_EX_BLEND_TARGET_CACHE_MAX;
	if ( bCachedTargets ) {
		pCachedTargets = &g_xgeShapeExBlendRenderer.arrTargets[
			g_xgeShapeExBlendCompositeSourceDepth
		];
		pSource = &pCachedTargets->tSource;
		pDestination = &pCachedTargets->tDestination;
		pCoverage = &pCachedTargets->tCoverage;
	}
	iRet = __xgeShapeAutoBatchFlush();
	if ( (iRet == XGE_OK) && bCachedTargets ) {
		iRet = __xgeShapeExBlendTargetSetEnsure(
			pCachedTargets, iWidth, iHeight,
			iSource == XGE_SHAPE_EX_BLEND_SOURCE_GRADIENT
		);
	} else if ( iRet == XGE_OK ) {
		iRet = xgeRenderTargetCreate(pSource, iWidth, iHeight);
		if ( iRet == XGE_OK ) {
			iRet = xgeRenderTargetCreate(pDestination, iWidth, iHeight);
		}
		if ( (iRet == XGE_OK) && (iSource == XGE_SHAPE_EX_BLEND_SOURCE_GRADIENT) ) {
			iRet = xgeRenderTargetCreate(pCoverage, iWidth, iHeight);
		}
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeRenderTargetCaptureCurrent(pDestination, (int)fLeft, (int)fTop);
	}
	if ( iRet == XGE_OK ) {
		xgeClipClear();
		__xgeShapeExStencilSuspend(&tStencilState);
		xgePassInit(&tPass, pSource, XGE_PASS_CLEAR_COLOR, XGE_COLOR_RGBA(0, 0, 0, 0));
		iRet = xgePassBegin(&tPass);
	} else {
		memset(&tStencilState, 0, sizeof(tStencilState));
	}
	if ( iRet == XGE_OK ) {
		tOffset = __xgeShapeExMatrixIdentity();
		tOffset.fE = -fLeft;
		tOffset.fF = -fTop;
		tLocalParent = __xgeShapeExMatrixMul(tOffset, tScreenParent);
		g_xgeShapeExBlendCompositeSourceDepth++;
		iRet = pDraw(pUser, tLocalParent);
		g_xgeShapeExBlendCompositeSourceDepth--;
		if ( iRet == XGE_OK ) {
			iRet = __xgeShapeAutoBatchFlush();
		}
	}
	if ( tPass.bActive ) {
		int iEndRet = xgePassEnd(&tPass);
		if ( iRet == XGE_OK ) iRet = iEndRet;
	}
	if ( (iRet == XGE_OK) && (iSource == XGE_SHAPE_EX_BLEND_SOURCE_GRADIENT) ) {
		iRet = __xgeShapeExDrawToTarget(
			pCoverage, pCoverageDraw, pCoverageUser, tLocalParent
		);
	}
	if ( tStencilState.iContextCount > 0 || tStencilState.iActiveMask != 0 ) {
		__xgeShapeExStencilResume(&tStencilState);
	} else if ( glDisable != NULL ) {
		glDisable(GL_STENCIL_TEST);
	}
	if ( bOldClip ) xgeClipSet(tOldClip);
	else xgeClipClear();
	if ( iRet == XGE_OK ) {
		iRet = __xgeShapeExBlendRendererEnsure();
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform1i(&g_xgeShapeExBlendRenderer.tShader, "uBlendMode", iBlend);
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform1i(&g_xgeShapeExBlendRenderer.tShader, "uBlendSource", iSource);
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform1f(&g_xgeShapeExBlendRenderer.tShader, "uBlendOpacity", fOpacity);
	}
	if ( iRet == XGE_OK ) {
		xgeMaterialInit(&tMaterial);
		xgeMaterialSetShader(&tMaterial, &g_xgeShapeExBlendRenderer.tShader);
		xgeMaterialSetTexture(&tMaterial, xgeRenderTargetTexture(pSource));
		xgeMaterialSetTexture2(&tMaterial, xgeRenderTargetTexture(pDestination));
		if ( iSource == XGE_SHAPE_EX_BLEND_SOURCE_GRADIENT ) {
			xgeMaterialSetTexture3(&tMaterial, xgeRenderTargetTexture(pCoverage));
		}
		xgeMaterialSetBlend(&tMaterial, XGE_BLEND_NONE);
		memset(&tDraw, 0, sizeof(tDraw));
		tDraw.pTexture = xgeRenderTargetTexture(pSource);
		tDraw.tSrc = (xge_rect_t){0.0f, 0.0f, (float)iWidth, (float)iHeight};
		tDraw.tDst = (xge_rect_t){fLeft, fTop, (float)iWidth, (float)iHeight};
		tDraw.iColor = XGE_COLOR_RGBA(255, 255, 255, 255);
		tDraw.iFlags = XGE_DRAW_SCREEN_SPACE | XGE_DRAW_FLIP_Y;
		xgeMaterialDraw(&tMaterial, &tDraw);
		xgeMaterialFree(&tMaterial);
		iRet = xgeFlush();
	}
	if ( !bCachedTargets ) {
		xgeRenderTargetFree(&tCoverage);
		xgeRenderTargetFree(&tDestination);
		xgeRenderTargetFree(&tSource);
	}
	return iRet;
}

typedef struct xge_shape_ex_mask_paint_context_t {
	int iTargetType;
	xge_shape_ex pShape;
	xge_shape_ex_scene pScene;
	float fTolerance;
	float fParentOpacity;
	int bSource;
} xge_shape_ex_mask_paint_context_t;

static int __xgeShapeExMaskPaintDraw(void* pUser, xge_shape_ex_matrix_t tLocalParent)
{
	xge_shape_ex_mask_paint_context_t* pContext = (xge_shape_ex_mask_paint_context_t*)pUser;

	if ( pContext == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( pContext->iTargetType == XGE_SHAPE_EX_MASK_TARGET_SHAPE ) {
		if ( pContext->bSource ) {
			xge_shape_ex_t tSource = *pContext->pShape;

			tSource.iMaskMethod = XGE_SHAPE_EX_MASK_NONE;
			tSource.iMaskTargetType = XGE_SHAPE_EX_MASK_TARGET_NONE;
			tSource.pMaskShape = NULL;
			tSource.pMaskScene = NULL;
			return __xgeShapeExDrawDispatch(
				&tSource, pContext->fTolerance, 1, tLocalParent, pContext->fParentOpacity, 1
			);
		}
		return __xgeShapeExDrawDispatch(
			pContext->pShape, pContext->fTolerance, 1, tLocalParent, pContext->fParentOpacity, 0
		);
	}
	if ( pContext->iTargetType == XGE_SHAPE_EX_MASK_TARGET_SCENE ) {
		if ( pContext->bSource ) {
			xge_shape_ex_scene_t tSource = *pContext->pScene;

			tSource.iMaskMethod = XGE_SHAPE_EX_MASK_NONE;
			tSource.iMaskTargetType = XGE_SHAPE_EX_MASK_TARGET_NONE;
			tSource.pMaskShape = NULL;
			tSource.pMaskScene = NULL;
			return __xgeShapeExSceneDrawDispatch(
				&tSource, pContext->fTolerance, 1, tLocalParent, pContext->fParentOpacity, 1
			);
		}
		return __xgeShapeExSceneDrawDispatch(
			pContext->pScene, pContext->fTolerance, 1, tLocalParent, pContext->fParentOpacity, 0
		);
	}
	return XGE_ERROR_INVALID_ARGUMENT;
}

static int __xgeShapeExMaskPaintBounds(
	const xge_shape_ex_mask_paint_context_t* pContext,
	xge_shape_ex_matrix_t tParent,
	xge_rect_t* pBounds
)
{
	int iRet;

	if ( (pContext == NULL) || (pBounds == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( pContext->iTargetType == XGE_SHAPE_EX_MASK_TARGET_SHAPE ) {
		if ( pContext->bSource ) {
			xge_shape_ex_t tSource = *pContext->pShape;

			tSource.iMaskMethod = XGE_SHAPE_EX_MASK_NONE;
			tSource.iMaskTargetType = XGE_SHAPE_EX_MASK_TARGET_NONE;
			tSource.pMaskShape = NULL;
			tSource.pMaskScene = NULL;
			return __xgeShapeExGetBoundsInternal(&tSource, pContext->fTolerance, tParent, pBounds);
		}
		return __xgeShapeExGetBoundsInternal(pContext->pShape, pContext->fTolerance, tParent, pBounds);
	}
	if ( pContext->iTargetType == XGE_SHAPE_EX_MASK_TARGET_SCENE ) {
		if ( pContext->bSource ) {
			xge_shape_ex_scene_t tSource = *pContext->pScene;

			tSource.iMaskMethod = XGE_SHAPE_EX_MASK_NONE;
			tSource.iMaskTargetType = XGE_SHAPE_EX_MASK_TARGET_NONE;
			tSource.pMaskShape = NULL;
			tSource.pMaskScene = NULL;
			iRet = __xgeShapeExSceneGetRenderBounds(&tSource, pContext->fTolerance, pBounds);
		} else {
			iRet = __xgeShapeExSceneGetRenderBounds(pContext->pScene, pContext->fTolerance, pBounds);
		}
		if ( iRet == XGE_OK ) *pBounds = __xgeShapeExMatrixRectBounds(tParent, *pBounds);
		return iRet;
	}
	return XGE_ERROR_INVALID_ARGUMENT;
}

static xge_rect_t __xgeShapeExMaskCombineBounds(xge_rect_t tSource, xge_rect_t tMask, int iMethod)
{
	int bSource = (tSource.fW > 0.0f) && (tSource.fH > 0.0f);
	int bMask = (tMask.fW > 0.0f) && (tMask.fH > 0.0f);

	if ( (iMethod == XGE_SHAPE_EX_MASK_ADD) ||
	     (iMethod == XGE_SHAPE_EX_MASK_DIFFERENCE) ||
	     (iMethod == XGE_SHAPE_EX_MASK_LIGHTEN) ) {
		if ( !bSource ) return tMask;
		if ( !bMask ) return tSource;
		return __xgeShapeExRectUnion(tSource, tMask);
	}
	if ( (iMethod == XGE_SHAPE_EX_MASK_ALPHA) ||
	     (iMethod == XGE_SHAPE_EX_MASK_LUMA) ||
	     (iMethod == XGE_SHAPE_EX_MASK_INTERSECT) ||
	     (iMethod == XGE_SHAPE_EX_MASK_DARKEN) ) {
		if ( !bSource || !bMask ) return (xge_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
		return __xgeShapeExRectIntersect(tSource, tMask);
	}
	return tSource;
}

typedef struct xge_shape_ex_external_draw_context_t {
	xge_shape_ex_draw_proc pDraw;
	void* pUser;
} xge_shape_ex_external_draw_context_t;

static int __xgeShapeExExternalDraw(void* pUser, xge_shape_ex_matrix_t tLocalParent)
{
	xge_shape_ex_external_draw_context_t* pContext = (xge_shape_ex_external_draw_context_t*)pUser;

	if ( (pContext == NULL) || (pContext->pDraw == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	return pContext->pDraw(pContext->pUser, &tLocalParent);
}

int xgeShapeExMaskCompositeScene(
	xge_shape_ex_scene pMaskScene,
	int iMethod,
	int iOutputBlend,
	xge_rect_t tSourceBounds,
	xge_shape_ex_draw_proc pSourceDraw,
	void* pSourceUser,
	float fTolerance,
	const xge_shape_ex_matrix_t* pParentMatrix,
	int bScreenSpace
)
{
	xge_shape_ex_external_draw_context_t tSourceContext;
	xge_shape_ex_mask_paint_context_t tMaskContext;
	xge_shape_ex_matrix_t tParent;
	xge_shape_ex_matrix_t tScreenParent;
	xge_rect_t tMaskBounds;
	xge_rect_t tBounds;
	int iRet;

	if ( !__xgeShapeExSceneValid(pMaskScene) || !__xgeShapeExMaskMethodValid(iMethod) ||
	     !__xgeShapeExBlendValid(iOutputBlend) || (pSourceDraw == NULL) ||
	     !__xgeShapeExRectFinite(tSourceBounds) || !__xgeShapeExFloatFinite(fTolerance) ||
	     ((pParentMatrix != NULL) && !__xgeShapeExMatrixFinite(pParentMatrix)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tParent = pParentMatrix != NULL ? *pParentMatrix : __xgeShapeExMatrixIdentity();
	tScreenParent = __xgeShapeExScreenParent(tParent, bScreenSpace != 0);
	tBounds = __xgeShapeExMatrixRectBounds(tScreenParent, tSourceBounds);
	memset(&tMaskContext, 0, sizeof(tMaskContext));
	tMaskContext.iTargetType = XGE_SHAPE_EX_MASK_TARGET_SCENE;
	tMaskContext.pScene = pMaskScene;
	tMaskContext.fTolerance = fTolerance;
	tMaskContext.fParentOpacity = 1.0f;
	iRet = __xgeShapeExMaskPaintBounds(&tMaskContext, tScreenParent, &tMaskBounds);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	tBounds = __xgeShapeExMaskCombineBounds(tBounds, tMaskBounds, iMethod);
	tSourceContext.pDraw = pSourceDraw;
	tSourceContext.pUser = pSourceUser;
	return __xgeShapeExCompositeMask(
		iMethod, iOutputBlend, tBounds,
		__xgeShapeExExternalDraw, &tSourceContext,
		__xgeShapeExMaskPaintDraw, &tMaskContext,
		tScreenParent, 1
	);
}

static int __xgeShapeExMaskOutputBlend(int bBlendSet, int iBlend, int bSuppressOwnBlend)
{
	if ( bSuppressOwnBlend || !bBlendSet || __xgeShapeExBlendNeedsComposite(iBlend) ) {
		return XGE_BLEND_ALPHA;
	}
	return iBlend;
}

typedef struct xge_shape_ex_clip_coverage_context_t {
	int iTargetType;
	xge_shape_ex pShape;
	xge_shape_ex_scene pScene;
	const xge_shape_ex* pClipShapes;
	int iClipShapeCount;
	float fTolerance;
	float fParentOpacity;
	xge_shape_ex_matrix_t tClipParentTransform;
} xge_shape_ex_clip_coverage_context_t;

static int __xgeShapeExClipCoverageSupported(const int* pModes, int iCount)
{
	int i;

	if ( iCount <= 0 ) return 0;
	for ( i = 0; i < iCount; i++ ) {
		int iMode = pModes != NULL ? pModes[i] : XGE_SHAPE_EX_CLIP_INTERSECT;

		if ( iMode == XGE_SHAPE_EX_CLIP_SUBTRACT ) return 0;
	}
	return 1;
}

static int __xgeShapeExClipCoverageSourceDraw(void* pUser, xge_shape_ex_matrix_t tLocalParent)
{
	xge_shape_ex_clip_coverage_context_t* pContext = (xge_shape_ex_clip_coverage_context_t*)pUser;

	if ( pContext == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( pContext->iTargetType == XGE_SHAPE_EX_MASK_TARGET_SHAPE ) {
		xge_shape_ex_t tSource;

		if ( !__xgeShapeExValid(pContext->pShape) ) return XGE_ERROR_INVALID_ARGUMENT;
		tSource = *pContext->pShape;
		tSource.pClipShapes = NULL;
		tSource.pClipShapeModes = NULL;
		tSource.iClipShapeCount = 0;
		tSource.iClipShapeCapacity = 0;
		return __xgeShapeExDrawDispatch(
			&tSource, pContext->fTolerance, 1, tLocalParent, pContext->fParentOpacity, 1
		);
	}
	if ( pContext->iTargetType == XGE_SHAPE_EX_MASK_TARGET_SCENE ) {
		xge_shape_ex_scene_t tSource;

		if ( !__xgeShapeExSceneValid(pContext->pScene) ) return XGE_ERROR_INVALID_ARGUMENT;
		tSource = *pContext->pScene;
		tSource.pClipShapes = NULL;
		tSource.pClipShapeModes = NULL;
		tSource.iClipShapeCount = 0;
		tSource.iClipShapeCapacity = 0;
		return __xgeShapeExSceneDrawDispatch(
			&tSource, pContext->fTolerance, 1, tLocalParent, pContext->fParentOpacity, 1
		);
	}
	return XGE_ERROR_INVALID_ARGUMENT;
}

static int __xgeShapeExClipCoverageMaskDraw(void* pUser, xge_shape_ex_matrix_t tLocalParent)
{
	xge_shape_ex_clip_coverage_context_t* pContext = (xge_shape_ex_clip_coverage_context_t*)pUser;
	xge_shape_ex_matrix_t tClipParent;
	int i;

	if ( (pContext == NULL) || (pContext->pClipShapes == NULL) ||
	     (pContext->iClipShapeCount <= 0) ) return XGE_ERROR_INVALID_ARGUMENT;
	tClipParent = __xgeShapeExMatrixMul(tLocalParent, pContext->tClipParentTransform);
	for ( i = 0; i < pContext->iClipShapeCount; i++ ) {
		xge_shape_ex_t tMask;
		xge_shape_ex pClipShape = pContext->pClipShapes[i];
		int iRet;

		if ( !__xgeShapeExValid(pClipShape) ) return XGE_ERROR_INVALID_ARGUMENT;
		tMask = *pClipShape;
		tMask.iFillType = XGE_SHAPE_EX_FILL_SOLID;
		tMask.iFillColor = XGE_COLOR_RGBA(255, 255, 255, 255);
		tMask.pFillStops = NULL;
		tMask.iFillStopCount = 0;
		tMask.iFillStopCapacity = 0;
		tMask.iStrokeType = XGE_SHAPE_EX_FILL_SOLID;
		tMask.iStrokeColor = XGE_COLOR_RGBA(0, 0, 0, 0);
		tMask.fStrokeWidth = 0.0f;
		tMask.pStrokeStops = NULL;
		tMask.iStrokeStopCount = 0;
		tMask.iStrokeStopCapacity = 0;
		tMask.fOpacity = 1.0f;
		tMask.bStrokeFirst = 0;
		tMask.bStencilPaint = 0;
		tMask.bBlendSet = 0;
		tMask.iBlend = XGE_BLEND_ALPHA;
		tMask.iMaskMethod = XGE_SHAPE_EX_MASK_NONE;
		tMask.iMaskTargetType = XGE_SHAPE_EX_MASK_TARGET_NONE;
		tMask.pMaskShape = NULL;
		tMask.pMaskScene = NULL;
		tMask.bCoverageAntialiasDisabled = 0;
		tMask.bAxisAlignedFillAntialias = pContext->iClipShapeCount > 1;
		iRet = __xgeShapeExDrawDispatch(
			&tMask, pContext->fTolerance, 1, tClipParent, 1.0f, 1
		);
		if ( iRet != XGE_OK ) return iRet;
	}
	return XGE_OK;
}

static int __xgeShapeExClipCoverageShape(
	xge_shape_ex pShape,
	float fTolerance,
	int bScreenSpace,
	xge_shape_ex_matrix_t tParent,
	float fParentOpacity,
	int bSuppressOwnBlend
)
{
	xge_shape_ex_clip_coverage_context_t tContext;
	xge_shape_ex_matrix_t tScreenParent;
	xge_rect_t tBounds;
	int iOutputBlend;
	int iRet;

	tScreenParent = __xgeShapeExScreenParent(tParent, bScreenSpace);
	iRet = __xgeShapeExGetBoundsInternal(pShape, fTolerance, tScreenParent, &tBounds);
	if ( iRet != XGE_OK ) return iRet;
	memset(&tContext, 0, sizeof(tContext));
	tContext.iTargetType = XGE_SHAPE_EX_MASK_TARGET_SHAPE;
	tContext.pShape = pShape;
	tContext.pClipShapes = pShape->pClipShapes;
	tContext.iClipShapeCount = pShape->iClipShapeCount;
	tContext.fTolerance = fTolerance;
	tContext.fParentOpacity = fParentOpacity;
	tContext.tClipParentTransform = pShape->tTransform;
	iOutputBlend = __xgeShapeExMaskOutputBlend(
		pShape->bBlendSet, pShape->iBlend, bSuppressOwnBlend
	);
	return __xgeShapeExCompositeMask(
		XGE_SHAPE_EX_MASK_ALPHA, iOutputBlend, tBounds,
		__xgeShapeExClipCoverageSourceDraw, &tContext,
		__xgeShapeExClipCoverageMaskDraw, &tContext,
		tScreenParent, 0
	);
}

static int __xgeShapeExClipCoverageScene(
	xge_shape_ex_scene pScene,
	float fTolerance,
	int bScreenSpace,
	xge_shape_ex_matrix_t tParent,
	float fParentOpacity,
	int bSuppressOwnBlend
)
{
	xge_shape_ex_clip_coverage_context_t tContext;
	xge_shape_ex_matrix_t tScreenParent;
	xge_rect_t tBounds;
	int iOutputBlend;
	int iRet;

	tScreenParent = __xgeShapeExScreenParent(tParent, bScreenSpace);
	iRet = __xgeShapeExSceneGetRenderBounds(pScene, fTolerance, &tBounds);
	if ( iRet != XGE_OK ) return iRet;
	tBounds = __xgeShapeExMatrixRectBounds(tScreenParent, tBounds);
	memset(&tContext, 0, sizeof(tContext));
	tContext.iTargetType = XGE_SHAPE_EX_MASK_TARGET_SCENE;
	tContext.pScene = pScene;
	tContext.pClipShapes = pScene->pClipShapes;
	tContext.iClipShapeCount = pScene->iClipShapeCount;
	tContext.fTolerance = fTolerance;
	tContext.fParentOpacity = fParentOpacity;
	tContext.tClipParentTransform = pScene->tTransform;
	iOutputBlend = __xgeShapeExMaskOutputBlend(
		pScene->bBlendSet, pScene->iBlend, bSuppressOwnBlend
	);
	return __xgeShapeExCompositeMask(
		XGE_SHAPE_EX_MASK_ALPHA, iOutputBlend, tBounds,
		__xgeShapeExClipCoverageSourceDraw, &tContext,
		__xgeShapeExClipCoverageMaskDraw, &tContext,
		tScreenParent, 0
	);
}

typedef struct xge_shape_ex_blend_shape_context_t {
	xge_shape_ex pShape;
	float fTolerance;
	float fParentOpacity;
	int iPaint;
	int bBinaryFillCoverage;
} xge_shape_ex_blend_shape_context_t;

static int __xgeShapeExBlendShapeDraw(void* pUser, xge_shape_ex_matrix_t tLocalParent)
{
	xge_shape_ex_blend_shape_context_t* pContext = (xge_shape_ex_blend_shape_context_t*)pUser;
	xge_shape_ex_t tPaint;

	if ( pContext->iPaint == XGE_SHAPE_EX_BLEND_PAINT_ALL ) {
		return __xgeShapeExDrawDispatch(
			pContext->pShape, pContext->fTolerance, 1, tLocalParent, pContext->fParentOpacity, 1
		);
	}
	tPaint = *pContext->pShape;
	tPaint.bCoverageAntialiasDisabled = 0;
	if ( pContext->iPaint == XGE_SHAPE_EX_BLEND_PAINT_FILL ) {
		tPaint.bCoverageAntialiasDisabled = pContext->bBinaryFillCoverage;
		tPaint.fStrokeWidth = 0.0f;
		tPaint.iStrokeColor = XGE_COLOR_RGBA(0, 0, 0, 0);
	} else {
		tPaint.iFillType = XGE_SHAPE_EX_FILL_SOLID;
		tPaint.iFillColor = XGE_COLOR_RGBA(0, 0, 0, 0);
		tPaint.pFillStops = NULL;
		tPaint.iFillStopCount = 0;
		tPaint.iFillStopCapacity = 0;
	}
	return __xgeShapeExDrawDispatch(
		&tPaint, pContext->fTolerance, 1, tLocalParent, pContext->fParentOpacity, 1
	);
}

static int __xgeShapeExDrawDispatch(xge_shape_ex pShape, float fTolerance, int bScreenSpace, xge_shape_ex_matrix_t tParent, float fParentOpacity, int bSuppressOwnBlend)
{
	xge_shape_ex_blend_shape_context_t tContext;
	xge_shape_ex_gradient_mask_context_t tCoverageContext;
	xge_shape_ex_matrix_t tScreenParent;
	xge_rect_t tBounds;
	int iRet;

	if ( !__xgeShapeExValid(pShape) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( !bScreenSpace ) {
		tParent = __xgeShapeExScreenParent(tParent, 0);
		bScreenSpace = 1;
	}
	if ( !bSuppressOwnBlend && pShape->bBlendSet && __xgeShapeExBlendNeedsComposite(pShape->iBlend) ) {
		int bFillVisible;
		int bStrokeVisible;
		int iFirstPaint;
		int iSecondPaint;

		tScreenParent = __xgeShapeExScreenParent(tParent, bScreenSpace);
		iRet = __xgeShapeExGetBoundsInternal(pShape, fTolerance, tScreenParent, &tBounds);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		tContext.pShape = pShape;
		tContext.fTolerance = fTolerance;
		tContext.fParentOpacity = fParentOpacity;
		{
			xge_shape_ex_matrix_t tMatrix = __xgeShapeExMatrixMul(tScreenParent, pShape->tTransform);
			float fStrokeScale = pShape->bStrokeNonScaling ? 1.0f : __xgeShapeExMatrixStrokeScale(tMatrix);
			uint32_t iStrokeColor = __xgeShapeExColorOpacity(
				pShape->iStrokeColor, fParentOpacity * pShape->fOpacity
			);

			tContext.bBinaryFillCoverage = __xgeShapeExTrailingOpaqueStrokeCoversFill(
				pShape, pShape->fStrokeWidth * fStrokeScale, iStrokeColor
			);
		}
		tCoverageContext.pShape = pShape;
		tCoverageContext.fTolerance = fTolerance;
		tCoverageContext.bBinaryCoverage = 0;
		bFillVisible = __xgeShapeExFillVisible(pShape, pShape->iFillColor);
		bStrokeVisible = (pShape->fStrokeWidth > 0.0f) && __xgeShapeExStrokeVisible(pShape, pShape->iStrokeColor);
		if ( bFillVisible && bStrokeVisible ) {
			int iBlendSource;

			iFirstPaint = pShape->bStrokeFirst ? XGE_SHAPE_EX_BLEND_PAINT_STROKE : XGE_SHAPE_EX_BLEND_PAINT_FILL;
			iSecondPaint = pShape->bStrokeFirst ? XGE_SHAPE_EX_BLEND_PAINT_FILL : XGE_SHAPE_EX_BLEND_PAINT_STROKE;
			tContext.iPaint = iFirstPaint;
			tCoverageContext.iPaint = iFirstPaint;
			tCoverageContext.bBinaryCoverage =
				(iFirstPaint == XGE_SHAPE_EX_BLEND_PAINT_FILL) &&
				tContext.bBinaryFillCoverage;
			iBlendSource = __xgeShapeExBlendShapeSource(pShape, iFirstPaint);
			iRet = __xgeShapeExCompositeBlend(
				pShape->iBlend, iBlendSource,
				__xgeShapeExBlendSourceOpacity(
					iBlendSource, fParentOpacity,
					pShape->fOpacity * __xgeShapeExBlendPaintOpacity(pShape, iFirstPaint)
				), tBounds,
				__xgeShapeExBlendShapeDraw, &tContext,
				__xgeShapeExGradientMaskDraw, &tCoverageContext, tScreenParent
			);
			if ( iRet != XGE_OK ) return iRet;
			tContext.iPaint = iSecondPaint;
			tCoverageContext.iPaint = iSecondPaint;
			tCoverageContext.bBinaryCoverage =
				(iSecondPaint == XGE_SHAPE_EX_BLEND_PAINT_FILL) &&
				tContext.bBinaryFillCoverage;
			iBlendSource = __xgeShapeExBlendShapeSource(pShape, iSecondPaint);
			return __xgeShapeExCompositeBlend(
				pShape->iBlend, iBlendSource,
				__xgeShapeExBlendSourceOpacity(
					iBlendSource, fParentOpacity,
					pShape->fOpacity * __xgeShapeExBlendPaintOpacity(pShape, iSecondPaint)
				), tBounds,
				__xgeShapeExBlendShapeDraw, &tContext,
				__xgeShapeExGradientMaskDraw, &tCoverageContext, tScreenParent
			);
		}
		tContext.iPaint = bStrokeVisible ? XGE_SHAPE_EX_BLEND_PAINT_STROKE : XGE_SHAPE_EX_BLEND_PAINT_FILL;
		tCoverageContext.iPaint = tContext.iPaint;
		tCoverageContext.bBinaryCoverage =
			(tContext.iPaint == XGE_SHAPE_EX_BLEND_PAINT_FILL) &&
			tContext.bBinaryFillCoverage;
		{
			int iBlendSource = __xgeShapeExBlendShapeSource(pShape, tContext.iPaint);

			return __xgeShapeExCompositeBlend(
				pShape->iBlend, iBlendSource,
				__xgeShapeExBlendSourceOpacity(
					iBlendSource, fParentOpacity,
					pShape->fOpacity * __xgeShapeExBlendPaintOpacity(pShape, tContext.iPaint)
				), tBounds,
				__xgeShapeExBlendShapeDraw, &tContext,
				__xgeShapeExGradientMaskDraw, &tCoverageContext, tScreenParent
			);
		}
	}
	if ( __xgeShapeExClipCoverageSupported(pShape->pClipShapeModes, pShape->iClipShapeCount) ) {
		return __xgeShapeExClipCoverageShape(
			pShape, fTolerance, bScreenSpace, tParent, fParentOpacity, bSuppressOwnBlend
		);
	}
	if ( pShape->iMaskTargetType != XGE_SHAPE_EX_MASK_TARGET_NONE ) {
		xge_shape_ex_mask_paint_context_t tSourceContext;
		xge_shape_ex_mask_paint_context_t tMaskContext;
		xge_rect_t tSourceBounds;
		xge_rect_t tMaskBounds;
		int iOutputBlend;

		memset(&tSourceContext, 0, sizeof(tSourceContext));
		memset(&tMaskContext, 0, sizeof(tMaskContext));
		tScreenParent = __xgeShapeExScreenParent(tParent, bScreenSpace);
		tSourceContext.iTargetType = XGE_SHAPE_EX_MASK_TARGET_SHAPE;
		tSourceContext.pShape = pShape;
		tSourceContext.fTolerance = fTolerance;
		tSourceContext.fParentOpacity = fParentOpacity;
		tSourceContext.bSource = 1;
		tMaskContext.iTargetType = pShape->iMaskTargetType;
		tMaskContext.pShape = pShape->pMaskShape;
		tMaskContext.pScene = pShape->pMaskScene;
		tMaskContext.fTolerance = fTolerance;
		tMaskContext.fParentOpacity = 1.0f;
		tMaskContext.bSource = 0;
		iRet = __xgeShapeExMaskPaintBounds(&tSourceContext, tScreenParent, &tSourceBounds);
		if ( iRet != XGE_OK ) return iRet;
		iRet = __xgeShapeExMaskPaintBounds(&tMaskContext, tScreenParent, &tMaskBounds);
		if ( iRet != XGE_OK ) return iRet;
		tBounds = __xgeShapeExMaskCombineBounds(tSourceBounds, tMaskBounds, pShape->iMaskMethod);
		iOutputBlend = __xgeShapeExMaskOutputBlend(pShape->bBlendSet, pShape->iBlend, bSuppressOwnBlend);
		return __xgeShapeExCompositeMask(
			pShape->iMaskMethod, iOutputBlend, tBounds,
			__xgeShapeExMaskPaintDraw, &tSourceContext,
			__xgeShapeExMaskPaintDraw, &tMaskContext,
			tScreenParent, !bSuppressOwnBlend
		);
	}
	return __xgeShapeExDrawInternal(
		pShape, fTolerance, bScreenSpace, tParent, fParentOpacity, bSuppressOwnBlend
	);
}

int xgeShapeExDraw(xge_shape_ex pShape, float fTolerance)
{
	return xgeShapeExDrawEx(pShape, fTolerance, NULL, 1.0f);
}

int xgeShapeExDrawPx(xge_shape_ex pShape, float fTolerance)
{
	return xgeShapeExDrawPxEx(pShape, fTolerance, NULL, 1.0f);
}

int xgeShapeExDrawEx(xge_shape_ex pShape, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, float fParentOpacity)
{
	xge_shape_ex_matrix_t tParent;

	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExFloatFinite(fTolerance) ||
	     !__xgeShapeExFloatFinite(fParentOpacity) ||
	     ((pParentMatrix != NULL) && !__xgeShapeExMatrixFinite(pParentMatrix)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tParent = pParentMatrix != NULL ? *pParentMatrix : __xgeShapeExMatrixIdentity();
	return __xgeShapeExDrawDispatch(pShape, fTolerance, 0, tParent, fParentOpacity, 0);
}

int xgeShapeExDrawPxEx(xge_shape_ex pShape, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, float fParentOpacity)
{
	xge_shape_ex_matrix_t tParent;

	if ( !__xgeShapeExValid(pShape) || !__xgeShapeExFloatFinite(fTolerance) ||
	     !__xgeShapeExFloatFinite(fParentOpacity) ||
	     ((pParentMatrix != NULL) && !__xgeShapeExMatrixFinite(pParentMatrix)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tParent = pParentMatrix != NULL ? *pParentMatrix : __xgeShapeExMatrixIdentity();
	return __xgeShapeExDrawDispatch(pShape, fTolerance, 1, tParent, fParentOpacity, 0);
}

static void __xgeShapeExSceneClipShapesClearInternal(xge_shape_ex_scene pScene)
{
	int i;

	if ( !__xgeShapeExSceneValid(pScene) ) {
		return;
	}
	for ( i = 0; i < pScene->iClipShapeCount; i++ ) {
		if ( __xgeShapeExValid(pScene->pClipShapes[i]) &&
		     (pScene->pClipShapes[i]->pParentScene == pScene->pParentScene) ) {
			pScene->pClipShapes[i]->pParentScene = NULL;
		}
		xgeShapeExDestroy(pScene->pClipShapes[i]);
	}
	pScene->iClipShapeCount = 0;
}

static int __xgeShapeExSceneReserveClipShapes(xge_shape_ex_scene pScene, int iNeeded)
{
	xge_shape_ex* pShapes;
	int* pModes;
	int iCapacity;

	if ( !__xgeShapeExSceneValid(pScene) || (iNeeded < 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iNeeded <= pScene->iClipShapeCapacity ) {
		return XGE_OK;
	}
	iCapacity = pScene->iClipShapeCapacity > 0 ? pScene->iClipShapeCapacity : 4;
	while ( iCapacity < iNeeded ) {
		if ( iCapacity > (INT_MAX / 2) ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		iCapacity *= 2;
	}
	pShapes = (xge_shape_ex*)xrtMalloc((size_t)iCapacity * sizeof(*pShapes));
	if ( pShapes == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pModes = (int*)xrtMalloc((size_t)iCapacity * sizeof(*pModes));
	if ( pModes == NULL ) {
		xrtFree(pShapes);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( pScene->iClipShapeCount > 0 ) {
		memcpy(pShapes, pScene->pClipShapes, (size_t)pScene->iClipShapeCount * sizeof(*pShapes));
		if ( pScene->pClipShapeModes != NULL ) {
			memcpy(pModes, pScene->pClipShapeModes, (size_t)pScene->iClipShapeCount * sizeof(*pModes));
		} else {
			int i;

			for ( i = 0; i < pScene->iClipShapeCount; i++ ) {
				pModes[i] = XGE_SHAPE_EX_CLIP_INTERSECT;
			}
		}
	}
	xrtFree(pScene->pClipShapes);
	xrtFree(pScene->pClipShapeModes);
	pScene->pClipShapes = pShapes;
	pScene->pClipShapeModes = pModes;
	pScene->iClipShapeCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeShapeExSceneClipShapeAddRefEx(xge_shape_ex_scene pScene, xge_shape_ex pClipShape, int iMode)
{
	int iRet;

	if ( !__xgeShapeExSceneValid(pScene) || !__xgeShapeExValid(pClipShape) || !__xgeShapeExClipModeValid(iMode) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pClipShape->pParentScene != NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( __xgeShapeExPaintReaches(
		XGE_SHAPE_EX_MASK_TARGET_SHAPE, pClipShape,
		XGE_SHAPE_EX_MASK_TARGET_SCENE, pScene
	) ) return XGE_ERROR_INVALID_ARGUMENT;
	iRet = __xgeShapeExSceneReserveClipShapes(pScene, pScene->iClipShapeCount + 1);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = xgeShapeExAddRef(pClipShape);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pScene->pClipShapes[pScene->iClipShapeCount] = pClipShape;
	pScene->pClipShapeModes[pScene->iClipShapeCount] = iMode;
	pScene->iClipShapeCount++;
	if ( pScene->pParentScene != NULL ) pClipShape->pParentScene = pScene->pParentScene;
	return XGE_OK;
}

static int __xgeShapeExSceneClipShapeAddRef(xge_shape_ex_scene pScene, xge_shape_ex pClipShape)
{
	return __xgeShapeExSceneClipShapeAddRefEx(pScene, pClipShape, XGE_SHAPE_EX_CLIP_INTERSECT);
}

static int __xgeShapeExSceneReserveEffects(xge_shape_ex_scene pScene, int iNeeded)
{
	xge_shape_ex_scene_effect_t* pEffects;
	int iCapacity;

	if ( !__xgeShapeExSceneValid(pScene) || (iNeeded < 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iNeeded <= pScene->iEffectCapacity ) return XGE_OK;
	iCapacity = pScene->iEffectCapacity > 0 ? pScene->iEffectCapacity * 2 : 4;
	while ( iCapacity < iNeeded ) {
		if ( iCapacity > (INT_MAX / 2) ) {
			iCapacity = iNeeded;
			break;
		}
		iCapacity *= 2;
	}
	pEffects = (xge_shape_ex_scene_effect_t*)xrtRealloc(
		pScene->pEffects, (size_t)iCapacity * sizeof(*pEffects)
	);
	if ( pEffects == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	pScene->pEffects = pEffects;
	pScene->iEffectCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeShapeExSceneEffectAppend(xge_shape_ex_scene pScene, const xge_shape_ex_scene_effect_t* pEffect)
{
	int iRet;

	if ( !__xgeShapeExSceneValid(pScene) || (pEffect == NULL) || !__xgeShapeExEffectTypeValid(pEffect->iType) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeShapeExSceneReserveEffects(pScene, pScene->iEffectCount + 1);
	if ( iRet != XGE_OK ) return iRet;
	pScene->pEffects[pScene->iEffectCount++] = *pEffect;
	return XGE_OK;
}

int xgeShapeExSceneCreate(xge_shape_ex_scene* ppScene)
{
	xge_shape_ex_scene pScene;

	if ( ppScene == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pScene = (xge_shape_ex_scene)xrtCalloc(1, sizeof(*pScene));
	if ( pScene == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pScene->iMagic = XGE_SHAPE_EX_SCENE_MAGIC;
	pScene->iRefCount = 1;
	pScene->fOpacity = 1.0f;
	pScene->bVisible = 1;
	pScene->iBlend = XGE_BLEND_ALPHA;
	pScene->bBlendSet = 0;
	pScene->tTransform = __xgeShapeExMatrixIdentity();
	__xgeShapeExTransformStateReset(&pScene->tTransformState);
	*ppScene = pScene;
	return XGE_OK;
}

int xgeShapeExSceneAddRef(xge_shape_ex_scene pScene)
{
	if ( !__xgeShapeExSceneValid(pScene) ) return XGE_ERROR_INVALID_ARGUMENT;
	pScene->iRefCount++;
	return XGE_OK;
}

int xgeShapeExSceneRefCountGet(xge_shape_ex_scene pScene, int* pRefCount)
{
	if ( !__xgeShapeExSceneValid(pScene) || (pRefCount == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pRefCount = pScene->iRefCount;
	return XGE_OK;
}

int xgeShapeExSceneId(xge_shape_ex_scene pScene, uint32_t iId)
{
	if ( !__xgeShapeExSceneValid(pScene) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pScene->iId = iId;
	return XGE_OK;
}

int xgeShapeExSceneIdGet(xge_shape_ex_scene pScene, uint32_t* pId)
{
	if ( !__xgeShapeExSceneValid(pScene) || (pId == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pId = pScene->iId;
	return XGE_OK;
}

int xgeShapeExSceneParentGet(xge_shape_ex_scene pScene, xge_shape_ex_scene* ppParentScene)
{
	if ( !__xgeShapeExSceneValid(pScene) || (ppParentScene == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppParentScene = pScene->pParentScene;
	return XGE_OK;
}

int xgeShapeExSceneClone(xge_shape_ex_scene pScene, xge_shape_ex_scene* ppClone)
{
	xge_shape_ex_scene pClone;
	int i;
	int iRet;

	if ( !__xgeShapeExSceneValid(pScene) || (ppClone == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppClone = NULL;
	iRet = xgeShapeExSceneCreate(&pClone);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pClone->fOpacity = pScene->fOpacity;
	pClone->bVisible = pScene->bVisible;
	pClone->iBlend = pScene->iBlend;
	pClone->bBlendSet = pScene->bBlendSet;
	pClone->bClipRect = pScene->bClipRect;
	pClone->tClipRect = pScene->tClipRect;
	pClone->tTransform = pScene->tTransform;
	pClone->tTransformState = pScene->tTransformState;
	if ( pScene->iEffectCount > 0 ) {
		iRet = __xgeShapeExSceneReserveEffects(pClone, pScene->iEffectCount);
		if ( iRet != XGE_OK ) {
			xgeShapeExSceneDestroy(pClone);
			return iRet;
		}
		memcpy(
			pClone->pEffects, pScene->pEffects,
			(size_t)pScene->iEffectCount * sizeof(*pClone->pEffects)
		);
		pClone->iEffectCount = pScene->iEffectCount;
	}
	for ( i = 0; i < pScene->iClipShapeCount; i++ ) {
		xge_shape_ex pClipClone;

		pClipClone = NULL;
		iRet = xgeShapeExClone(pScene->pClipShapes[i], &pClipClone);
		if ( iRet == XGE_OK ) {
			int iMode = pScene->pClipShapeModes != NULL ? pScene->pClipShapeModes[i] : XGE_SHAPE_EX_CLIP_INTERSECT;

			iRet = __xgeShapeExSceneClipShapeAddRefEx(pClone, pClipClone, iMode);
			xgeShapeExDestroy(pClipClone);
		}
		if ( iRet != XGE_OK ) {
			xgeShapeExSceneDestroy(pClone);
			return iRet;
		}
	}
	for ( i = 0; i < pScene->iChildCount; i++ ) {
		const xge_shape_ex_scene_child_t* pChild = &pScene->pChildren[i];

		if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE ) {
			xge_shape_ex pShapeClone = NULL;

			iRet = xgeShapeExClone(pChild->pShape, &pShapeClone);
			if ( iRet == XGE_OK ) iRet = xgeShapeExSceneAdd(pClone, pShapeClone);
			xgeShapeExDestroy(pShapeClone);
		} else if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SCENE ) {
			xge_shape_ex_scene pSceneClone = NULL;

			iRet = xgeShapeExSceneClone(pChild->pScene, &pSceneClone);
			if ( iRet == XGE_OK ) iRet = xgeShapeExSceneAddScene(pClone, pSceneClone);
			xgeShapeExSceneDestroy(pSceneClone);
		} else {
			iRet = XGE_ERROR_INVALID_ARGUMENT;
		}
		if ( iRet != XGE_OK ) {
			xgeShapeExSceneDestroy(pClone);
			return iRet;
		}
	}
	if ( pScene->iMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_SHAPE ) {
		xge_shape_ex pMaskClone = NULL;

		iRet = xgeShapeExClone(pScene->pMaskShape, &pMaskClone);
		if ( iRet == XGE_OK ) {
			iRet = xgeShapeExSceneMaskShapeSet(pClone, pMaskClone, pScene->iMaskMethod);
		}
		xgeShapeExDestroy(pMaskClone);
		if ( iRet != XGE_OK ) {
			xgeShapeExSceneDestroy(pClone);
			return iRet;
		}
	} else if ( pScene->iMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_SCENE ) {
		xge_shape_ex_scene pMaskClone = NULL;

		iRet = xgeShapeExSceneClone(pScene->pMaskScene, &pMaskClone);
		if ( iRet == XGE_OK ) {
			iRet = xgeShapeExSceneMaskSceneSet(pClone, pMaskClone, pScene->iMaskMethod);
		}
		xgeShapeExSceneDestroy(pMaskClone);
		if ( iRet != XGE_OK ) {
			xgeShapeExSceneDestroy(pClone);
			return iRet;
		}
	}
	*ppClone = pClone;
	return XGE_OK;
}

void xgeShapeExSceneDestroy(xge_shape_ex_scene pScene)
{
	int i;

	if ( !__xgeShapeExSceneValid(pScene) ) {
		return;
	}
	pScene->iRefCount--;
	if ( pScene->iRefCount > 0 ) {
		return;
	}
	for ( i = 0; i < pScene->iChildCount; i++ ) {
		__xgeShapeExSceneChildRelease(pScene, &pScene->pChildren[i]);
	}
	__xgeShapeExSceneClipShapesClearInternal(pScene);
	__xgeShapeExSceneMaskClearInternal(pScene);
	pScene->iMagic = 0;
	xrtFree(pScene->pChildren);
	xrtFree(pScene->pClipShapes);
	xrtFree(pScene->pClipShapeModes);
	xrtFree(pScene->pEffects);
	xrtFree(pScene);
}

int xgeShapeExSceneClear(xge_shape_ex_scene pScene)
{
	int i;

	if ( !__xgeShapeExSceneValid(pScene) ) return XGE_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pScene->iChildCount; i++ ) {
		__xgeShapeExSceneChildRelease(pScene, &pScene->pChildren[i]);
	}
	pScene->iChildCount = 0;
	return XGE_OK;
}

static int __xgeShapeExSceneReserveChildren(xge_shape_ex_scene pScene, int iNeeded)
{
	xge_shape_ex_scene_child_t* pChildren;
	int iCapacity;

	if ( !__xgeShapeExSceneValid(pScene) || (iNeeded < 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( iNeeded <= pScene->iChildCapacity ) {
		return XGE_OK;
	}
	iCapacity = pScene->iChildCapacity > 0 ? pScene->iChildCapacity * 2 : 8;
	while ( iCapacity < iNeeded ) {
		if ( iCapacity > (INT_MAX / 2) ) {
			iCapacity = iNeeded;
			break;
		}
		iCapacity *= 2;
	}
	pChildren = (xge_shape_ex_scene_child_t*)xrtRealloc(
		pScene->pChildren, (size_t)iCapacity * sizeof(*pChildren)
	);
	if ( pChildren == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( iCapacity > pScene->iChildCapacity ) {
		memset(
			pChildren + pScene->iChildCapacity, 0,
			(size_t)(iCapacity - pScene->iChildCapacity) * sizeof(*pChildren)
		);
	}
	pScene->pChildren = pChildren;
	pScene->iChildCapacity = iCapacity;
	return XGE_OK;
}

static int __xgeShapeExSceneInsertChildAt(
	xge_shape_ex_scene pScene,
	const xge_shape_ex_scene_child_t* pChild,
	int iIndex
)
{
	int iNodeType;
	const void* pNode;
	int iRet;

	if ( !__xgeShapeExSceneValid(pScene) || !__xgeShapeExSceneChildValid(pChild) ||
	     (iIndex < 0) || (iIndex > pScene->iChildCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( ((pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE) &&
	      (pChild->pShape->pParentScene != NULL)) ||
	     ((pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SCENE) &&
	      (pChild->pScene->pParentScene != NULL)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( ((pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE) &&
	      !__xgeShapeExShapeDecorationsDetached(pChild->pShape)) ||
	     ((pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SCENE) &&
	      !__xgeShapeExSceneDecorationsDetached(pChild->pScene)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iNodeType = pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE ?
		XGE_SHAPE_EX_MASK_TARGET_SHAPE : XGE_SHAPE_EX_MASK_TARGET_SCENE;
	pNode = pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE ?
		(const void*)pChild->pShape : (const void*)pChild->pScene;
	if ( __xgeShapeExPaintReaches(
		iNodeType, pNode, XGE_SHAPE_EX_MASK_TARGET_SCENE, pScene
	) ) return XGE_ERROR_INVALID_ARGUMENT;
	iRet = __xgeShapeExSceneReserveChildren(pScene, pScene->iChildCount + 1);
	if ( iRet != XGE_OK ) return iRet;
	if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE ) {
		iRet = xgeShapeExAddRef(pChild->pShape);
	} else {
		iRet = xgeShapeExSceneAddRef(pChild->pScene);
	}
	if ( iRet != XGE_OK ) return iRet;
	if ( iIndex < pScene->iChildCount ) {
		memmove(
			pScene->pChildren + iIndex + 1, pScene->pChildren + iIndex,
			(size_t)(pScene->iChildCount - iIndex) * sizeof(*pScene->pChildren)
		);
	}
	pScene->pChildren[iIndex] = *pChild;
	pScene->iChildCount++;
	if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE ) {
		pChild->pShape->pParentScene = pScene;
		__xgeShapeExShapeDecorationsParentReplace(pChild->pShape, NULL, pScene);
	} else {
		pChild->pScene->pParentScene = pScene;
		__xgeShapeExSceneDecorationsParentReplace(pChild->pScene, NULL, pScene);
	}
	return XGE_OK;
}

static int __xgeShapeExSceneRemoveChildAt(xge_shape_ex_scene pScene, int iIndex)
{
	if ( !__xgeShapeExSceneValid(pScene) || (iIndex < 0) || (iIndex >= pScene->iChildCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	__xgeShapeExSceneChildRelease(pScene, &pScene->pChildren[iIndex]);
	if ( iIndex + 1 < pScene->iChildCount ) {
		memmove(
			pScene->pChildren + iIndex, pScene->pChildren + iIndex + 1,
			(size_t)(pScene->iChildCount - iIndex - 1) * sizeof(*pScene->pChildren)
		);
	}
	pScene->iChildCount--;
	memset(&pScene->pChildren[pScene->iChildCount], 0, sizeof(*pScene->pChildren));
	return XGE_OK;
}

int xgeShapeExSceneInsertShapeAt(xge_shape_ex_scene pScene, xge_shape_ex pShape, int iIndex)
{
	xge_shape_ex_scene_child_t tChild;

	memset(&tChild, 0, sizeof(tChild));
	tChild.iType = XGE_SHAPE_EX_SCENE_CHILD_SHAPE;
	tChild.pShape = pShape;
	return __xgeShapeExSceneInsertChildAt(pScene, &tChild, iIndex);
}

int xgeShapeExSceneInsertSceneAt(xge_shape_ex_scene pScene, xge_shape_ex_scene pChildScene, int iIndex)
{
	xge_shape_ex_scene_child_t tChild;

	memset(&tChild, 0, sizeof(tChild));
	tChild.iType = XGE_SHAPE_EX_SCENE_CHILD_SCENE;
	tChild.pScene = pChildScene;
	return __xgeShapeExSceneInsertChildAt(pScene, &tChild, iIndex);
}

int xgeShapeExSceneAdd(xge_shape_ex_scene pScene, xge_shape_ex pShape)
{
	if ( !__xgeShapeExSceneValid(pScene) ) return XGE_ERROR_INVALID_ARGUMENT;
	return xgeShapeExSceneInsertShapeAt(pScene, pShape, pScene->iChildCount);
}

int xgeShapeExSceneAddScene(xge_shape_ex_scene pScene, xge_shape_ex_scene pChildScene)
{
	if ( !__xgeShapeExSceneValid(pScene) ) return XGE_ERROR_INVALID_ARGUMENT;
	return xgeShapeExSceneInsertSceneAt(pScene, pChildScene, pScene->iChildCount);
}

int xgeShapeExSceneInsert(xge_shape_ex_scene pScene, xge_shape_ex pShape, xge_shape_ex pBefore)
{
	int i;

	if ( !__xgeShapeExSceneValid(pScene) || !__xgeShapeExValid(pShape) ||
	     !__xgeShapeExValid(pBefore) || (pShape == pBefore) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < pScene->iChildCount; i++ ) {
		const xge_shape_ex_scene_child_t* pChild = &pScene->pChildren[i];

		if ( (pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE) &&
		     (pChild->pShape == pBefore) ) {
			return xgeShapeExSceneInsertShapeAt(pScene, pShape, i);
		}
	}
	return XGE_ERROR_NOT_FOUND;
}

int xgeShapeExSceneRemove(xge_shape_ex_scene pScene, xge_shape_ex pShape)
{
	int i;

	if ( !__xgeShapeExSceneValid(pScene) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( pShape == NULL ) return xgeShapeExSceneClear(pScene);
	if ( !__xgeShapeExValid(pShape) ) return XGE_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pScene->iChildCount; i++ ) {
		const xge_shape_ex_scene_child_t* pChild = &pScene->pChildren[i];

		if ( (pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE) &&
		     (pChild->pShape == pShape) ) {
			return __xgeShapeExSceneRemoveChildAt(pScene, i);
		}
	}
	return XGE_ERROR_NOT_FOUND;
}

int xgeShapeExSceneRemoveScene(xge_shape_ex_scene pScene, xge_shape_ex_scene pChildScene)
{
	int i;

	if ( !__xgeShapeExSceneValid(pScene) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( pChildScene == NULL ) return xgeShapeExSceneClear(pScene);
	if ( !__xgeShapeExSceneValid(pChildScene) ) return XGE_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pScene->iChildCount; i++ ) {
		const xge_shape_ex_scene_child_t* pChild = &pScene->pChildren[i];

		if ( (pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SCENE) &&
		     (pChild->pScene == pChildScene) ) {
			return __xgeShapeExSceneRemoveChildAt(pScene, i);
		}
	}
	return XGE_ERROR_NOT_FOUND;
}

int xgeShapeExSceneGetCount(xge_shape_ex_scene pScene, int* pCount)
{
	if ( !__xgeShapeExSceneValid(pScene) || (pCount == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pCount = pScene->iChildCount;
	return XGE_OK;
}

int xgeShapeExSceneGetAt(xge_shape_ex_scene pScene, int iIndex, xge_shape_ex* ppShape)
{
	if ( !__xgeShapeExSceneValid(pScene) || (ppShape == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppShape = NULL;
	if ( (iIndex < 0) || (iIndex >= pScene->iChildCount) ||
	     (pScene->pChildren[iIndex].iType != XGE_SHAPE_EX_SCENE_CHILD_SHAPE) ) {
		return XGE_ERROR_NOT_FOUND;
	}
	*ppShape = pScene->pChildren[iIndex].pShape;
	return XGE_OK;
}

int xgeShapeExSceneChildGetAt(xge_shape_ex_scene pScene, int iIndex, xge_shape_ex_scene_child_t* pChild)
{
	if ( !__xgeShapeExSceneValid(pScene) || (pChild == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(pChild, 0, sizeof(*pChild));
	if ( (iIndex < 0) || (iIndex >= pScene->iChildCount) ) return XGE_ERROR_NOT_FOUND;
	*pChild = pScene->pChildren[iIndex];
	return XGE_OK;
}

static int __xgeShapeExSceneTraverse(
	xge_shape_ex_scene pScene,
	xge_shape_ex_scene_visit_proc onPaint,
	void* pUser
)
{
	enum { XGE_SHAPE_EX_TRAVERSE_STOP = 1 };
	xge_shape_ex_scene_child_t tPaint;
	int i;
	int iRet;

	memset(&tPaint, 0, sizeof(tPaint));
	tPaint.iType = XGE_SHAPE_EX_SCENE_CHILD_SCENE;
	tPaint.pScene = pScene;
	if ( !onPaint(&tPaint, pUser) ) return XGE_SHAPE_EX_TRAVERSE_STOP;
	for ( i = 0; i < pScene->iChildCount; i++ ) {
		const xge_shape_ex_scene_child_t* pChild = &pScene->pChildren[i];

		if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SCENE ) {
			iRet = __xgeShapeExSceneTraverse(pChild->pScene, onPaint, pUser);
			if ( iRet != XGE_OK ) return iRet;
		} else if ( !onPaint(pChild, pUser) ) {
			return XGE_SHAPE_EX_TRAVERSE_STOP;
		}
	}
	return XGE_OK;
}

int xgeShapeExSceneTraverse(xge_shape_ex_scene pScene, xge_shape_ex_scene_visit_proc onPaint, void* pUser)
{
	enum { XGE_SHAPE_EX_TRAVERSE_STOP = 1 };
	int iRet;

	if ( !__xgeShapeExSceneValid(pScene) || (onPaint == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = xgeShapeExSceneAddRef(pScene);
	if ( iRet != XGE_OK ) return iRet;
	iRet = __xgeShapeExSceneTraverse(pScene, onPaint, pUser);
	xgeShapeExSceneDestroy(pScene);
	return iRet == XGE_SHAPE_EX_TRAVERSE_STOP ? XGE_OK : iRet;
}

int xgeShapeExSceneTransformSet(xge_shape_ex_scene pScene, const xge_shape_ex_matrix_t* pMatrix)
{
	if ( !__xgeShapeExSceneValid(pScene) || !__xgeShapeExMatrixFinite(pMatrix) ) return XGE_ERROR_INVALID_ARGUMENT;
	pScene->tTransform = *pMatrix;
	pScene->tTransformState.bOverride = 1;
	return XGE_OK;
}

int xgeShapeExSceneTransformIdentity(xge_shape_ex_scene pScene)
{
	if ( !__xgeShapeExSceneValid(pScene) ) return XGE_ERROR_INVALID_ARGUMENT;
	pScene->tTransform = __xgeShapeExMatrixIdentity();
	__xgeShapeExTransformStateReset(&pScene->tTransformState);
	return XGE_OK;
}

int xgeShapeExSceneTransformGet(xge_shape_ex_scene pScene, xge_shape_ex_matrix_t* pMatrix)
{
	if ( !__xgeShapeExSceneValid(pScene) || (pMatrix == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pMatrix = pScene->tTransform;
	return XGE_OK;
}

int xgeShapeExSceneTransformTranslate(xge_shape_ex_scene pScene, float fTX, float fTY)
{
	if ( !__xgeShapeExSceneValid(pScene) || !__xgeShapeExCoordsFinite(fTX, fTY) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( pScene->tTransformState.bOverride ) return XGE_ERROR_INVALID_STATE;
	pScene->tTransformState.fTranslateX = fTX;
	pScene->tTransformState.fTranslateY = fTY;
	__xgeShapeExTransformStateRebuild(&pScene->tTransformState, &pScene->tTransform);
	return XGE_OK;
}

int xgeShapeExSceneTransformScale(xge_shape_ex_scene pScene, float fSX, float fSY)
{
	if ( !__xgeShapeExSceneValid(pScene) || !__xgeShapeExCoordsFinite(fSX, fSY) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( pScene->tTransformState.bOverride ) return XGE_ERROR_INVALID_STATE;
	pScene->tTransformState.fScaleX = fSX;
	pScene->tTransformState.fScaleY = fSY;
	__xgeShapeExTransformStateRebuild(&pScene->tTransformState, &pScene->tTransform);
	return XGE_OK;
}

int xgeShapeExSceneTransformRotate(xge_shape_ex_scene pScene, float fRadians)
{
	if ( !__xgeShapeExSceneValid(pScene) || !__xgeShapeExFloatFinite(fRadians) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( pScene->tTransformState.bOverride ) return XGE_ERROR_INVALID_STATE;
	pScene->tTransformState.fRotateRadians = fRadians;
	__xgeShapeExTransformStateRebuild(&pScene->tTransformState, &pScene->tTransform);
	return XGE_OK;
}

int xgeShapeExSceneTransformSkew(xge_shape_ex_scene pScene, float fXRadians, float fYRadians)
{
	if ( !__xgeShapeExSceneValid(pScene) || !__xgeShapeExCoordsFinite(fXRadians, fYRadians) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( pScene->tTransformState.bOverride ) return XGE_ERROR_INVALID_STATE;
	pScene->tTransformState.fSkewXRadians = fXRadians;
	pScene->tTransformState.fSkewYRadians = fYRadians;
	__xgeShapeExTransformStateRebuild(&pScene->tTransformState, &pScene->tTransform);
	return XGE_OK;
}

int xgeShapeExSceneOpacity(xge_shape_ex_scene pScene, float fOpacity)
{
	if ( !__xgeShapeExSceneValid(pScene) || !__xgeShapeExFloatFinite(fOpacity) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( fOpacity < 0.0f ) fOpacity = 0.0f;
	if ( fOpacity > 1.0f ) fOpacity = 1.0f;
	pScene->fOpacity = fOpacity;
	return XGE_OK;
}

int xgeShapeExSceneOpacityGet(xge_shape_ex_scene pScene, float* pOpacity)
{
	if ( !__xgeShapeExSceneValid(pScene) || (pOpacity == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pOpacity = pScene->fOpacity;
	return XGE_OK;
}

int xgeShapeExSceneVisible(xge_shape_ex_scene pScene, int bVisible)
{
	if ( !__xgeShapeExSceneValid(pScene) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pScene->bVisible = bVisible != 0;
	return XGE_OK;
}

int xgeShapeExSceneVisibleGet(xge_shape_ex_scene pScene, int* pVisible)
{
	if ( !__xgeShapeExSceneValid(pScene) || (pVisible == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pVisible = pScene->bVisible;
	return XGE_OK;
}

int xgeShapeExSceneBlend(xge_shape_ex_scene pScene, int iBlend)
{
	if ( !__xgeShapeExSceneValid(pScene) || !__xgeShapeExBlendValid(iBlend) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pScene->iBlend = iBlend;
	pScene->bBlendSet = 1;
	return XGE_OK;
}

int xgeShapeExSceneBlendClear(xge_shape_ex_scene pScene)
{
	if ( !__xgeShapeExSceneValid(pScene) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pScene->iBlend = XGE_BLEND_ALPHA;
	pScene->bBlendSet = 0;
	return XGE_OK;
}

int xgeShapeExSceneBlendGet(xge_shape_ex_scene pScene, int* pBlend, int* pBlendSet)
{
	if ( !__xgeShapeExSceneValid(pScene) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pBlend != NULL ) {
		*pBlend = pScene->iBlend;
	}
	if ( pBlendSet != NULL ) {
		*pBlendSet = pScene->bBlendSet;
	}
	return XGE_OK;
}

int xgeShapeExSceneMaskShapeSet(xge_shape_ex_scene pScene, xge_shape_ex pTarget, int iMethod)
{
	int iRet;

	if ( !__xgeShapeExSceneValid(pScene) || !__xgeShapeExValid(pTarget) || !__xgeShapeExMaskMethodValid(iMethod) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pTarget->pParentScene != NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( __xgeShapeExPaintReaches(
		XGE_SHAPE_EX_MASK_TARGET_SHAPE, pTarget,
		XGE_SHAPE_EX_MASK_TARGET_SCENE, pScene
	) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = xgeShapeExAddRef(pTarget);
	if ( iRet != XGE_OK ) return iRet;
	__xgeShapeExSceneMaskClearInternal(pScene);
	pScene->iMaskMethod = iMethod;
	pScene->iMaskTargetType = XGE_SHAPE_EX_MASK_TARGET_SHAPE;
	pScene->pMaskShape = pTarget;
	if ( pScene->pParentScene != NULL ) pTarget->pParentScene = pScene->pParentScene;
	return XGE_OK;
}

int xgeShapeExSceneMaskSceneSet(xge_shape_ex_scene pScene, xge_shape_ex_scene pTarget, int iMethod)
{
	int iRet;

	if ( !__xgeShapeExSceneValid(pScene) || !__xgeShapeExSceneValid(pTarget) || !__xgeShapeExMaskMethodValid(iMethod) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pTarget->pParentScene != NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( __xgeShapeExPaintReaches(
		XGE_SHAPE_EX_MASK_TARGET_SCENE, pTarget,
		XGE_SHAPE_EX_MASK_TARGET_SCENE, pScene
	) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = xgeShapeExSceneAddRef(pTarget);
	if ( iRet != XGE_OK ) return iRet;
	__xgeShapeExSceneMaskClearInternal(pScene);
	pScene->iMaskMethod = iMethod;
	pScene->iMaskTargetType = XGE_SHAPE_EX_MASK_TARGET_SCENE;
	pScene->pMaskScene = pTarget;
	if ( pScene->pParentScene != NULL ) pTarget->pParentScene = pScene->pParentScene;
	return XGE_OK;
}

int xgeShapeExSceneMaskClear(xge_shape_ex_scene pScene)
{
	if ( !__xgeShapeExSceneValid(pScene) ) return XGE_ERROR_INVALID_ARGUMENT;
	__xgeShapeExSceneMaskClearInternal(pScene);
	return XGE_OK;
}

int xgeShapeExSceneMaskGet(xge_shape_ex_scene pScene, int* pMethod, int* pTargetType, xge_shape_ex* ppTargetShape, xge_shape_ex_scene* ppTargetScene)
{
	if ( !__xgeShapeExSceneValid(pScene) ||
	     ((pMethod == NULL) && (pTargetType == NULL) && (ppTargetShape == NULL) && (ppTargetScene == NULL)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pMethod != NULL ) *pMethod = pScene->iMaskMethod;
	if ( pTargetType != NULL ) *pTargetType = pScene->iMaskTargetType;
	if ( ppTargetShape != NULL ) *ppTargetShape = pScene->pMaskShape;
	if ( ppTargetScene != NULL ) *ppTargetScene = pScene->pMaskScene;
	return XGE_OK;
}

int xgeShapeExSceneClipRectSet(xge_shape_ex_scene pScene, xge_rect_t tRect)
{
	if ( !__xgeShapeExSceneValid(pScene) || !__xgeShapeExRectFinite(tRect) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( tRect.fW < 0.0f ) tRect.fW = 0.0f;
	if ( tRect.fH < 0.0f ) tRect.fH = 0.0f;
	pScene->tClipRect = tRect;
	pScene->bClipRect = 1;
	return XGE_OK;
}

int xgeShapeExSceneClipRectGet(xge_shape_ex_scene pScene, xge_rect_t* pRect, int* pEnabled)
{
	if ( !__xgeShapeExSceneValid(pScene) || ((pRect == NULL) && (pEnabled == NULL)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pRect != NULL ) {
		if ( pScene->bClipRect ) {
			*pRect = pScene->tClipRect;
		} else {
			memset(pRect, 0, sizeof(*pRect));
		}
	}
	if ( pEnabled != NULL ) {
		*pEnabled = pScene->bClipRect;
	}
	return XGE_OK;
}

int xgeShapeExSceneClipShapeAdd(xge_shape_ex_scene pScene, xge_shape_ex pClipShape)
{
	return __xgeShapeExSceneClipShapeAddRef(pScene, pClipShape);
}

int xgeShapeExSceneClipShapeAddEx(xge_shape_ex_scene pScene, xge_shape_ex pClipShape, int iMode)
{
	return __xgeShapeExSceneClipShapeAddRefEx(pScene, pClipShape, iMode);
}

int xgeShapeExSceneClipShapeGetCount(xge_shape_ex_scene pScene, int* pCount)
{
	if ( !__xgeShapeExSceneValid(pScene) || (pCount == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pCount = pScene->iClipShapeCount;
	return XGE_OK;
}

int xgeShapeExSceneClipShapeGetAt(xge_shape_ex_scene pScene, int iIndex, xge_shape_ex* ppClipShape)
{
	if ( !__xgeShapeExSceneValid(pScene) || (ppClipShape == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iIndex < 0) || (iIndex >= pScene->iClipShapeCount) ) {
		*ppClipShape = NULL;
		return XGE_ERROR_NOT_FOUND;
	}
	*ppClipShape = pScene->pClipShapes[iIndex];
	return XGE_OK;
}

int xgeShapeExSceneClipShapeGetAtEx(xge_shape_ex_scene pScene, int iIndex, xge_shape_ex* ppClipShape, int* pMode)
{
	if ( !__xgeShapeExSceneValid(pScene) || ((ppClipShape == NULL) && (pMode == NULL)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( (iIndex < 0) || (iIndex >= pScene->iClipShapeCount) ) {
		if ( ppClipShape != NULL ) {
			*ppClipShape = NULL;
		}
		if ( pMode != NULL ) {
			*pMode = XGE_SHAPE_EX_CLIP_INTERSECT;
		}
		return XGE_ERROR_NOT_FOUND;
	}
	if ( ppClipShape != NULL ) {
		*ppClipShape = pScene->pClipShapes[iIndex];
	}
	if ( pMode != NULL ) {
		*pMode = pScene->pClipShapeModes != NULL ? pScene->pClipShapeModes[iIndex] : XGE_SHAPE_EX_CLIP_INTERSECT;
	}
	return XGE_OK;
}

int xgeShapeExSceneClipShapeClear(xge_shape_ex_scene pScene)
{
	if ( !__xgeShapeExSceneValid(pScene) ) return XGE_ERROR_INVALID_ARGUMENT;
	__xgeShapeExSceneClipShapesClearInternal(pScene);
	return XGE_OK;
}

int xgeShapeExSceneClipClear(xge_shape_ex_scene pScene)
{
	if ( !__xgeShapeExSceneValid(pScene) ) return XGE_ERROR_INVALID_ARGUMENT;
	memset(&pScene->tClipRect, 0, sizeof(pScene->tClipRect));
	pScene->bClipRect = 0;
	__xgeShapeExSceneClipShapesClearInternal(pScene);
	return XGE_OK;
}

int xgeShapeExSceneEffectClear(xge_shape_ex_scene pScene)
{
	if ( !__xgeShapeExSceneValid(pScene) ) return XGE_ERROR_INVALID_ARGUMENT;
	pScene->iEffectCount = 0;
	return XGE_OK;
}

int xgeShapeExSceneEffectGaussianBlur(xge_shape_ex_scene pScene, float fSigma, int iDirection, int iBorder, int iQuality)
{
	xge_shape_ex_scene_effect_t tEffect;

	if ( !__xgeShapeExSceneValid(pScene) || !__xgeShapeExFloatFinite(fSigma) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(&tEffect, 0, sizeof(tEffect));
	tEffect.iType = XGE_SHAPE_EX_EFFECT_GAUSSIAN_BLUR;
	tEffect.fSigma = fSigma < 0.0f ? 0.0f : fSigma;
	tEffect.iDirection = iDirection < XGE_SHAPE_EX_BLUR_BOTH ? XGE_SHAPE_EX_BLUR_BOTH :
		(iDirection > XGE_SHAPE_EX_BLUR_VERTICAL ? XGE_SHAPE_EX_BLUR_VERTICAL : iDirection);
	tEffect.iBorder = iBorder < XGE_SHAPE_EX_BORDER_DUPLICATE ? XGE_SHAPE_EX_BORDER_DUPLICATE :
		(iBorder > XGE_SHAPE_EX_BORDER_WRAP ? XGE_SHAPE_EX_BORDER_WRAP : iBorder);
	tEffect.iQuality = iQuality < 0 ? 0 : (iQuality > 100 ? 100 : iQuality);
	return __xgeShapeExSceneEffectAppend(pScene, &tEffect);
}

int xgeShapeExSceneEffectDropShadow(xge_shape_ex_scene pScene, uint32_t iColor, float fAngleDegrees, float fDistance, float fSigma, int iQuality)
{
	xge_shape_ex_scene_effect_t tEffect;

	if ( !__xgeShapeExSceneValid(pScene) || !__xgeShapeExFloatFinite(fAngleDegrees) ||
	     !__xgeShapeExFloatFinite(fDistance) || !__xgeShapeExFloatFinite(fSigma) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(&tEffect, 0, sizeof(tEffect));
	tEffect.iType = XGE_SHAPE_EX_EFFECT_DROP_SHADOW;
	tEffect.iColor = iColor;
	tEffect.fAngleDegrees = fAngleDegrees;
	tEffect.fDistance = fDistance;
	tEffect.fSigma = fSigma < 0.0f ? 0.0f : fSigma;
	tEffect.iQuality = iQuality < 0 ? 0 : (iQuality > 100 ? 100 : iQuality);
	return __xgeShapeExSceneEffectAppend(pScene, &tEffect);
}

int xgeShapeExSceneEffectFill(xge_shape_ex_scene pScene, uint32_t iColor)
{
	xge_shape_ex_scene_effect_t tEffect;

	if ( !__xgeShapeExSceneValid(pScene) ) return XGE_ERROR_INVALID_ARGUMENT;
	memset(&tEffect, 0, sizeof(tEffect));
	tEffect.iType = XGE_SHAPE_EX_EFFECT_FILL;
	tEffect.iColor = iColor;
	return __xgeShapeExSceneEffectAppend(pScene, &tEffect);
}

int xgeShapeExSceneEffectTint(xge_shape_ex_scene pScene, uint32_t iBlackColor, uint32_t iWhiteColor, float fIntensity)
{
	xge_shape_ex_scene_effect_t tEffect;

	if ( !__xgeShapeExSceneValid(pScene) || !__xgeShapeExFloatFinite(fIntensity) ||
	     (fIntensity < 0.0f) || (fIntensity > 100.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(&tEffect, 0, sizeof(tEffect));
	tEffect.iType = XGE_SHAPE_EX_EFFECT_TINT;
	tEffect.iColor = iBlackColor;
	tEffect.iColor2 = iWhiteColor;
	tEffect.fIntensity = fIntensity;
	return __xgeShapeExSceneEffectAppend(pScene, &tEffect);
}

int xgeShapeExSceneEffectTritone(xge_shape_ex_scene pScene, uint32_t iShadowColor, uint32_t iMidtoneColor, uint32_t iHighlightColor, int iBlend)
{
	xge_shape_ex_scene_effect_t tEffect;

	if ( !__xgeShapeExSceneValid(pScene) || (iBlend < 0) || (iBlend > 255) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(&tEffect, 0, sizeof(tEffect));
	tEffect.iType = XGE_SHAPE_EX_EFFECT_TRITONE;
	tEffect.iColor = iShadowColor;
	tEffect.iColor2 = iMidtoneColor;
	tEffect.iColor3 = iHighlightColor;
	tEffect.iBlend = iBlend;
	return __xgeShapeExSceneEffectAppend(pScene, &tEffect);
}

int xgeShapeExSceneEffectGetCount(xge_shape_ex_scene pScene, int* pCount)
{
	if ( !__xgeShapeExSceneValid(pScene) || (pCount == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	*pCount = pScene->iEffectCount;
	return XGE_OK;
}

int xgeShapeExSceneEffectGetAt(xge_shape_ex_scene pScene, int iIndex, xge_shape_ex_scene_effect_t* pEffect)
{
	if ( !__xgeShapeExSceneValid(pScene) || (pEffect == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( (iIndex < 0) || (iIndex >= pScene->iEffectCount) ) {
		memset(pEffect, 0, sizeof(*pEffect));
		return XGE_ERROR_NOT_FOUND;
	}
	*pEffect = pScene->pEffects[iIndex];
	return XGE_OK;
}

static float __xgeShapeExEffectScale(xge_shape_ex_matrix_t tMatrix)
{
	float fScale = sqrtf(tMatrix.fA * tMatrix.fA + tMatrix.fB * tMatrix.fB);
	return fScale > XGE_SHAPE_EX_EPSILON ? fScale : 1.0f;
}

typedef struct xge_shape_ex_effect_blur_config_t {
	int iLevel;
	int arrKernel[3];
	int iExtent;
} xge_shape_ex_effect_blur_config_t;

static void __xgeShapeExEffectBlurConfig(
	float fSigma,
	int iQuality,
	xge_shape_ex_matrix_t tMatrix,
	xge_shape_ex_effect_blur_config_t* pConfig
)
{
	double fScaledSigma;
	double fVariance;
	double fLowerWidth;
	double fMi;
	int iLowerWidth;
	int iUpperWidth;
	int iLowerCount;
	int iSaturatedKernel;
	int i;

	if ( pConfig == NULL ) return;
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->iLevel = (int)(3.0f * ((float)(iQuality - 1) * 0.01f)) + 1;
	if ( pConfig->iLevel < 1 ) pConfig->iLevel = 1;
	if ( pConfig->iLevel > 3 ) pConfig->iLevel = 3;
	fScaledSigma = (double)fSigma * (double)__xgeShapeExEffectScale(tMatrix);
	fVariance = fScaledSigma * fScaledSigma;
	if ( fVariance <= XGE_SHAPE_EX_EPSILON ) return;
	fLowerWidth = sqrt((12.0 * fVariance / 3.0) + 1.0);
	if ( !isfinite(fLowerWidth) || (fLowerWidth >= (double)(INT_MAX / 2)) ) {
		iSaturatedKernel = (INT_MAX / 2) / pConfig->iLevel;
		for ( i = 0; i < pConfig->iLevel; i++ ) {
			pConfig->arrKernel[i] = iSaturatedKernel;
			pConfig->iExtent += iSaturatedKernel;
		}
		return;
	}
	iLowerWidth = (int)fLowerWidth;
	if ( (iLowerWidth & 1) == 0 ) iLowerWidth--;
	if ( iLowerWidth < 1 ) iLowerWidth = 1;
	iUpperWidth = iLowerWidth + 2;
	fMi = (12.0 * fVariance - 3.0 * (double)iLowerWidth * (double)iLowerWidth -
	       12.0 * (double)iLowerWidth - 9.0) /
	      (-4.0 * (double)iLowerWidth - 4.0);
	iLowerCount = (int)(fMi + 0.5f);
	for ( i = 0; i < pConfig->iLevel; i++ ) {
		pConfig->arrKernel[i] = (((i < iLowerCount) ? iLowerWidth : iUpperWidth) - 1) / 2;
		pConfig->iExtent += pConfig->arrKernel[i];
	}
}

static xge_vec2_t __xgeShapeExEffectShadowOffset(const xge_shape_ex_scene_effect_t* pEffect, xge_shape_ex_matrix_t tMatrix)
{
	float fScale = __xgeShapeExEffectScale(tMatrix);
	float fRotation = fabsf(atan2f(tMatrix.fB, tMatrix.fA));
	float fRadians = (90.0f - pEffect->fAngleDegrees) * (XGE_SHAPE_EX_PI / 180.0f) - fRotation;
	xge_vec2_t tOffset;

	tOffset.fX = truncf(pEffect->fDistance * fScale * cosf(fRadians));
	tOffset.fY = truncf(-pEffect->fDistance * fScale * sinf(fRadians));
	return tOffset;
}

static void __xgeShapeExSceneEffectBoundsApply(xge_shape_ex_scene pScene, xge_shape_ex_matrix_t tMatrix, xge_rect_t* pBounds)
{
	int i;

	if ( !__xgeShapeExSceneValid(pScene) || (pBounds == NULL) ||
	     (pBounds->fW <= 0.0f) || (pBounds->fH <= 0.0f) ) {
		return;
	}
	for ( i = 0; i < pScene->iEffectCount; i++ ) {
		const xge_shape_ex_scene_effect_t* pEffect = &pScene->pEffects[i];
		xge_shape_ex_effect_blur_config_t tBlur;

		if ( pEffect->iType == XGE_SHAPE_EX_EFFECT_GAUSSIAN_BLUR ) {
			__xgeShapeExEffectBlurConfig(pEffect->fSigma, pEffect->iQuality, tMatrix, &tBlur);
			if ( (tBlur.iExtent > 0) && (pEffect->iDirection != XGE_SHAPE_EX_BLUR_VERTICAL) ) {
				pBounds->fX -= (float)tBlur.iExtent;
				pBounds->fW += (float)tBlur.iExtent * 2.0f;
			}
			if ( (tBlur.iExtent > 0) && (pEffect->iDirection != XGE_SHAPE_EX_BLUR_HORIZONTAL) ) {
				pBounds->fY -= (float)tBlur.iExtent;
				pBounds->fH += (float)tBlur.iExtent * 2.0f;
			}
		} else if ( (pEffect->iType == XGE_SHAPE_EX_EFFECT_DROP_SHADOW) &&
		            (XGE_COLOR_GET_A(pEffect->iColor) > 0u) ) {
			xge_rect_t tShadow = *pBounds;
			xge_vec2_t tOffset = __xgeShapeExEffectShadowOffset(pEffect, tMatrix);

			__xgeShapeExEffectBlurConfig(pEffect->fSigma, pEffect->iQuality, tMatrix, &tBlur);
			tShadow.fX += tOffset.fX - (float)tBlur.iExtent;
			tShadow.fY += tOffset.fY - (float)tBlur.iExtent;
			tShadow.fW += (float)tBlur.iExtent * 2.0f;
			tShadow.fH += (float)tBlur.iExtent * 2.0f;
			*pBounds = __xgeShapeExRectUnion(*pBounds, tShadow);
		}
	}
}

static int __xgeShapeExSceneGetBoundsInternalEx(
	xge_shape_ex_scene pScene,
	float fTolerance,
	xge_shape_ex_matrix_t tParent,
	xge_rect_t* pBounds,
	int bRenderBounds,
	int iDepth
)
{
	xge_shape_ex_matrix_t tMatrix;
	xge_rect_t tOut;
	int bHasBounds;
	int i;
	int iRet;

	if ( !__xgeShapeExSceneValid(pScene) || (pBounds == NULL) ||
	     (iDepth > XGE_SHAPE_EX_CLIP_BOUNDS_MAX_DEPTH) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( fTolerance <= 0.0f ) {
		fTolerance = XGE_SHAPE_EX_DEFAULT_TOLERANCE;
	}
	memset(&tOut, 0, sizeof(tOut));
	bHasBounds = 0;
	tMatrix = __xgeShapeExMatrixMul(tParent, pScene->tTransform);
	for ( i = 0; i < pScene->iChildCount; i++ ) {
		const xge_shape_ex_scene_child_t* pChild = &pScene->pChildren[i];
		xge_rect_t tChildBounds;

		if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE ) {
			iRet = __xgeShapeExGetBoundsInternalEx(
				pChild->pShape, fTolerance, tMatrix, &tChildBounds,
				bRenderBounds, iDepth + 1
			);
		} else if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SCENE ) {
			iRet = __xgeShapeExSceneGetBoundsInternalEx(
				pChild->pScene, fTolerance, tMatrix, &tChildBounds,
				bRenderBounds, iDepth + 1
			);
		} else {
			iRet = XGE_ERROR_INVALID_ARGUMENT;
		}
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		if ( !bHasBounds ) {
			tOut = tChildBounds;
			bHasBounds = 1;
		} else {
			tOut = __xgeShapeExRectUnion(tOut, tChildBounds);
		}
	}
	if ( bRenderBounds && pScene->bClipRect ) {
		tOut = __xgeShapeExRectIntersect(tOut, __xgeShapeExMatrixRectBounds(tMatrix, pScene->tClipRect));
	}
	if ( bRenderBounds && (pScene->iClipShapeCount > 0) ) {
		xge_rect_t tClipUnion;
		int bHasClipBounds;
		int bHasIncludeClip;

		memset(&tClipUnion, 0, sizeof(tClipUnion));
		bHasClipBounds = 0;
		bHasIncludeClip = 0;
		for ( i = 0; i < pScene->iClipShapeCount; i++ ) {
			xge_rect_t tClipBounds;
			int iRet;
			int iMode = pScene->pClipShapeModes != NULL ? pScene->pClipShapeModes[i] : XGE_SHAPE_EX_CLIP_INTERSECT;

			if ( iMode == XGE_SHAPE_EX_CLIP_SUBTRACT ) {
				continue;
			}
			bHasIncludeClip = 1;

			iRet = __xgeShapeExGetBoundsInternalEx(
				pScene->pClipShapes[i], fTolerance, tMatrix, &tClipBounds, 1, iDepth + 1
			);
			if ( iRet != XGE_OK ) {
				return iRet;
			}
			if ( (tClipBounds.fW > 0.0f) && (tClipBounds.fH > 0.0f) ) {
				if ( !bHasClipBounds ) {
					tClipUnion = tClipBounds;
					bHasClipBounds = 1;
				} else {
					tClipUnion = __xgeShapeExRectUnion(tClipUnion, tClipBounds);
				}
			}
		}
		if ( bHasIncludeClip ) {
			if ( bHasClipBounds ) {
				tOut = __xgeShapeExRectIntersect(tOut, tClipUnion);
			} else {
				memset(&tOut, 0, sizeof(tOut));
			}
		}
	}
	if ( bRenderBounds ) {
		__xgeShapeExSceneEffectBoundsApply(pScene, tMatrix, &tOut);
	}
	if ( bRenderBounds && (pScene->iMaskTargetType != XGE_SHAPE_EX_MASK_TARGET_NONE) ) {
		iRet = __xgeShapeExApplyMaskBounds(
			pScene->iMaskTargetType, pScene->pMaskShape, pScene->pMaskScene,
			pScene->iMaskMethod, fTolerance, tParent, bRenderBounds,
			iDepth + 1, &tOut
		);
		if ( iRet != XGE_OK ) return iRet;
	}
	*pBounds = tOut;
	return XGE_OK;
}

int xgeShapeExSceneGetBounds(xge_shape_ex_scene pScene, float fTolerance, xge_rect_t* pBounds)
{
	if ( !__xgeShapeExSceneValid(pScene) || (pBounds == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pScene->iChildCount <= 0 ) {
		memset(pBounds, 0, sizeof(*pBounds));
		return XGE_ERROR_INVALID_STATE;
	}
	return __xgeShapeExSceneGetBoundsInternalEx(
		pScene, fTolerance, __xgeShapeExMatrixIdentity(), pBounds, 0, 0
	);
}

static int __xgeShapeExSceneGetRenderBounds(
	xge_shape_ex_scene pScene,
	float fTolerance,
	xge_rect_t* pBounds
)
{
	return __xgeShapeExSceneGetBoundsInternalEx(
		pScene, fTolerance, __xgeShapeExMatrixIdentity(), pBounds, 1, 0
	);
}

int xgeShapeExSceneGetOBB(xge_shape_ex_scene pScene, float fTolerance, xge_vec2_t* pPoints4)
{
	xge_rect_t tOut;
	int bHasBounds;
	int i;

	if ( !__xgeShapeExSceneValid(pScene) || (pPoints4 == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( pScene->iChildCount <= 0 ) {
		memset(pPoints4, 0, sizeof(*pPoints4) * 4u);
		return XGE_ERROR_INVALID_STATE;
	}
	if ( fTolerance <= 0.0f ) {
		fTolerance = XGE_SHAPE_EX_DEFAULT_TOLERANCE;
	}
	memset(&tOut, 0, sizeof(tOut));
	bHasBounds = 0;
	for ( i = 0; i < pScene->iChildCount; i++ ) {
		xge_shape_ex_scene_child_t* pChild = &pScene->pChildren[i];
		xge_rect_t tChildBounds;
		int iRet;

		if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE ) {
			iRet = __xgeShapeExGetLogicalBoundsInternal(
				pChild->pShape, fTolerance,
				__xgeShapeExMatrixIdentity(), &tChildBounds
			);
		} else if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SCENE ) {
			iRet = __xgeShapeExSceneGetBoundsInternalEx(
				pChild->pScene, fTolerance,
				__xgeShapeExMatrixIdentity(), &tChildBounds, 0, 0
			);
		} else {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		if ( !bHasBounds ) {
			tOut = tChildBounds;
			bHasBounds = 1;
		} else {
			tOut = __xgeShapeExRectUnion(tOut, tChildBounds);
		}
	}
	__xgeShapeExRectToOBB(tOut, pScene->tTransform, pPoints4);
	return XGE_OK;
}

static int __xgeShapeExSceneIntersectsInternal(
	xge_shape_ex_scene pScene,
	xge_rect_t tRect,
	float fTolerance,
	xge_shape_ex_matrix_t tParent,
	int* pIntersects
)
{
	xge_shape_ex_matrix_t tMatrix;
	int i;
	int iRet;

	if ( !__xgeShapeExSceneValid(pScene) || (pIntersects == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	*pIntersects = 0;
	if ( pScene->fOpacity <= 0.0f ) return XGE_OK;
	tMatrix = __xgeShapeExMatrixMul(tParent, pScene->tTransform);
	if ( (pScene->iMaskTargetType == XGE_SHAPE_EX_MASK_TARGET_NONE) && !pScene->bClipRect &&
	     (pScene->iClipShapeCount <= 0) ) {
		for ( i = 0; i < pScene->iChildCount; i++ ) {
			const xge_shape_ex_scene_child_t* pChild = &pScene->pChildren[i];

			if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SHAPE ) {
				iRet = __xgeShapeExIntersectsInternal(pChild->pShape, tRect, fTolerance, tMatrix, pIntersects);
			} else if ( pChild->iType == XGE_SHAPE_EX_SCENE_CHILD_SCENE ) {
				iRet = __xgeShapeExSceneIntersectsInternal(pChild->pScene, tRect, fTolerance, tMatrix, pIntersects);
			} else {
				continue;
			}
			if ( iRet != XGE_OK ) return iRet;
			if ( *pIntersects ) return XGE_OK;
		}
		return XGE_OK;
	}
	{
		xge_shape_ex_scene_t tProbe = *pScene;
		xge_rect_t tBounds;
		xge_rect_t tScan;
		int iStartX;
		int iStartY;
		int iEndX;
		int iEndY;
		int y;

		tProbe.bVisible = 1;
		iRet = xgeShapeExSceneGetBounds(pScene, fTolerance, &tBounds);
		if ( iRet != XGE_OK ) return iRet;
		tBounds = __xgeShapeExMatrixRectBounds(tParent, tBounds);
		tScan = __xgeShapeExRectIntersect(tBounds, tRect);
		if ( (tScan.fW <= 0.0f) || (tScan.fH <= 0.0f) ) return XGE_OK;
		{
			float fStartX = floorf(tScan.fX);
			float fStartY = floorf(tScan.fY);
			float fEndX = ceilf(tScan.fX + tScan.fW);
			float fEndY = ceilf(tScan.fY + tScan.fH);

			if ( !__xgeShapeExCoordsFinite(fStartX, fStartY) ||
			     !__xgeShapeExCoordsFinite(fEndX, fEndY) ||
			     (fStartX < -2147483648.0f) || (fStartY < -2147483648.0f) ||
			     (fEndX >= 2147483648.0f) || (fEndY >= 2147483648.0f) ) {
				return XGE_ERROR_INVALID_ARGUMENT;
			}
			iStartX = (int)fStartX;
			iStartY = (int)fStartY;
			iEndX = (int)fEndX;
			iEndY = (int)fEndY;
		}
		for ( y = iStartY; y < iEndY; y++ ) {
			int x;

			for ( x = iStartX; x < iEndX; x++ ) {
				float fLeft = fmaxf(tScan.fX, (float)x);
				float fTop = fmaxf(tScan.fY, (float)y);
				float fRight = fminf(tScan.fX + tScan.fW, (float)x + 1.0f);
				float fBottom = fminf(tScan.fY + tScan.fH, (float)y + 1.0f);
				float pSampleX[3];
				float pSampleY[3];
				int sy;

				if ( (fRight <= fLeft) || (fBottom <= fTop) ) continue;
				pSampleX[0] = (fLeft + fRight) * 0.5f;
				pSampleX[1] = fLeft + (fRight - fLeft) * 0.25f;
				pSampleX[2] = fLeft + (fRight - fLeft) * 0.75f;
				pSampleY[0] = (fTop + fBottom) * 0.5f;
				pSampleY[1] = fTop + (fBottom - fTop) * 0.25f;
				pSampleY[2] = fTop + (fBottom - fTop) * 0.75f;
				for ( sy = 0; sy < 3; sy++ ) {
					int sx;

					for ( sx = 0; sx < 3; sx++ ) {
						xge_shape_ex pHit = NULL;
						xge_vec2_t tPoint = {pSampleX[sx], pSampleY[sy]};

						iRet = __xgeShapeExSceneHitTestInternal(&tProbe, tPoint, fTolerance, tParent, &pHit);
						if ( iRet != XGE_OK ) return iRet;
						if ( pHit != NULL ) {
							*pIntersects = 1;
							return XGE_OK;
						}
					}
				}
			}
		}
	}
	return XGE_OK;
}

int xgeShapeExSceneBoundsIntersects(xge_shape_ex_scene pScene, xge_rect_t tRect, float fTolerance, int* pIntersects)
{
	xge_rect_t tBounds;
	int iRet;

	if ( !__xgeShapeExSceneValid(pScene) || (pIntersects == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pIntersects = 0;
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XGE_OK;
	}
	iRet = xgeShapeExSceneGetBounds(pScene, fTolerance, &tBounds);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	*pIntersects = __xgeShapeExRectIntersects(tBounds, tRect);
	return XGE_OK;
}

int xgeShapeExSceneIntersects(xge_shape_ex_scene pScene, xge_rect_t tRect, float fTolerance, int* pIntersects)
{
	return xgeShapeExSceneIntersectsEx(pScene, tRect, fTolerance, NULL, pIntersects);
}

int xgeShapeExSceneIntersectsEx(
	xge_shape_ex_scene pScene,
	xge_rect_t tRect,
	float fTolerance,
	const xge_shape_ex_matrix_t* pParentMatrix,
	int* pIntersects
)
{
	xge_shape_ex_matrix_t tParent;

	if ( !__xgeShapeExSceneValid(pScene) || (pIntersects == NULL) ||
	     !__xgeShapeExCoordsFinite(tRect.fX, tRect.fY) ||
	     !__xgeShapeExCoordsFinite(tRect.fW, tRect.fH) ||
	     !__xgeShapeExCoordsFinite(tRect.fX + tRect.fW, tRect.fY + tRect.fH) ||
	     !__xgeShapeExFloatFinite(fTolerance) ||
	     ((pParentMatrix != NULL) && !__xgeShapeExMatrixFinite(pParentMatrix)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*pIntersects = 0;
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) return XGE_OK;
	if ( fTolerance <= 0.0f ) fTolerance = XGE_SHAPE_EX_DEFAULT_TOLERANCE;
	tParent = (pParentMatrix != NULL) ? *pParentMatrix : __xgeShapeExMatrixIdentity();
	return __xgeShapeExSceneIntersectsInternal(pScene, tRect, fTolerance, tParent, pIntersects);
}

int xgeShapeExSceneContainsPoint(xge_shape_ex_scene pScene, float fX, float fY, float fTolerance, int* pContains)
{
	return xgeShapeExSceneContainsPointEx(pScene, fX, fY, fTolerance, NULL, pContains);
}

int xgeShapeExSceneContainsPointEx(xge_shape_ex_scene pScene, float fX, float fY, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, int* pContains)
{
	xge_shape_ex pHitShape;
	int iRet;

	if ( pContains == NULL ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pHitShape = NULL;
	iRet = xgeShapeExSceneHitTestEx(pScene, fX, fY, fTolerance, pParentMatrix, &pHitShape);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	*pContains = pHitShape != NULL;
	return XGE_OK;
}

int xgeShapeExSceneHitTest(xge_shape_ex_scene pScene, float fX, float fY, float fTolerance, xge_shape_ex* ppShape)
{
	return xgeShapeExSceneHitTestEx(pScene, fX, fY, fTolerance, NULL, ppShape);
}

int xgeShapeExSceneHitTestEx(xge_shape_ex_scene pScene, float fX, float fY, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, xge_shape_ex* ppShape)
{
	xge_shape_ex_matrix_t tParent;
	xge_vec2_t tPoint;

	if ( !__xgeShapeExSceneValid(pScene) || (ppShape == NULL) || !__xgeShapeExCoordsFinite(fX, fY) || !__xgeShapeExFloatFinite(fTolerance) ||
	     ((pParentMatrix != NULL) && !__xgeShapeExMatrixFinite(pParentMatrix)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tParent = (pParentMatrix != NULL) ? *pParentMatrix : __xgeShapeExMatrixIdentity();
	tPoint.fX = fX;
	tPoint.fY = fY;
	return __xgeShapeExSceneHitTestInternal(pScene, tPoint, fTolerance, tParent, ppShape);
}

static int __xgeShapeExEffectRendererEnsure(void)
{
	char sHeader[128];
	char sVertex[1536];
	char sBlurFragment[4096];
	char sColorFragment[4096];
	char sShadowFragment[3072];
	int iRet;

	if ( g_xgeShapeExEffectRenderer.bInitialized ) return XGE_OK;
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
		sBlurFragment, sizeof(sBlurFragment),
		"%s"
		"in vec2 vUV;\n"
		"uniform sampler2D uTexture;\n"
		"uniform vec2 uDirection;\n"
		"uniform float uRadius;\n"
		"uniform float uWrap;\n"
		"uniform float uFloorAlpha;\n"
		"out vec4 FragColor;\n"
		"void main() {\n"
		"  int radius = int(uRadius + 0.5);\n"
		"  if (radius <= 0) { FragColor = texture(uTexture, vUV); return; }\n"
		"  vec4 sum = vec4(0.0);\n"
		"  float weightSum = 0.0;\n"
		"  for (int i = -radius; i <= radius; ++i) {\n"
		"    vec2 coord = vUV + uDirection * float(i);\n"
		"    if (uWrap > 0.5) {\n"
		"      coord = mod(coord, vec2(1.0));\n"
		"      if (coord.x < 0.0) coord.x += 1.0;\n"
		"      if (coord.y < 0.0) coord.y += 1.0;\n"
		"    } else {\n"
		"      coord = clamp(coord, vec2(0.0), vec2(1.0));\n"
		"    }\n"
		"    sum += texture(uTexture, coord);\n"
		"    weightSum += 1.0;\n"
		"  }\n"
		"  vec4 result = weightSum > 0.0 ? sum / weightSum : texture(uTexture, vUV);\n"
		"  if (uFloorAlpha > 0.5) result.a = floor(clamp(result.a, 0.0, 1.0) * 255.0) / 255.0;\n"
		"  FragColor = result;\n"
		"}\n",
		sHeader
	);
	snprintf(
		sColorFragment, sizeof(sColorFragment),
		"%s"
		"in vec2 vUV;\n"
		"uniform sampler2D uTexture;\n"
		"uniform int uEffectType;\n"
		"uniform vec4 uColor0;\n"
		"uniform vec4 uColor1;\n"
		"uniform vec4 uColor2;\n"
		"uniform float uAmount;\n"
		"out vec4 FragColor;\n"
		"vec4 effectToU8(vec4 color) {\n"
		"  return floor(clamp(color, 0.0, 1.0) * 255.0 + 0.5);\n"
		"}\n"
		"vec4 effectAlphaBlend8(vec4 color, float alpha) {\n"
		"  return floor(color * (alpha + 1.0) / 256.0);\n"
		"}\n"
		"vec4 effectInterpolate8(vec4 source, vec4 destination, float alpha) {\n"
		"  return floor((source * alpha + destination * (256.0 - alpha)) / 256.0);\n"
		"}\n"
		"vec4 effectTritone8(vec4 shadow, vec4 midtone, vec4 highlight, float luma) {\n"
		"  float alpha;\n"
		"  if (luma < 128.0) {\n"
		"    alpha = min(luma * 2.0, 255.0);\n"
		"    return effectAlphaBlend8(shadow, 255.0 - alpha) + effectAlphaBlend8(midtone, alpha);\n"
		"  }\n"
		"  alpha = 2.0 * max(0.0, luma - 128.0);\n"
		"  return effectAlphaBlend8(midtone, 255.0 - alpha) + effectAlphaBlend8(highlight, alpha);\n"
		"}\n"
		"void main() {\n"
		"  vec4 orig = texture(uTexture, vUV);\n"
		"  if (uEffectType == 3) {\n"
		"    FragColor = vec4(uColor0.rgb * uColor0.a, uColor0.a) * orig.a;\n"
		"    return;\n"
		"  }\n"
		"  vec4 orig8 = effectToU8(orig);\n"
		"  float luma8 = floor(dot(orig8.rgb, vec3(54.0, 182.0, 19.0)) / 256.0);\n"
		"  if (uEffectType == 4) {\n"
		"    vec4 black8 = vec4(effectToU8(uColor0).rgb, 255.0);\n"
		"    vec4 white8 = vec4(effectToU8(uColor1).rgb, 255.0);\n"
		"    float intensity8 = floor(clamp(uAmount, 0.0, 1.0) * 255.0 + 0.5);\n"
		"    vec4 mapped8 = effectInterpolate8(white8, black8, luma8);\n"
		"    if (intensity8 < 255.0) mapped8 = effectInterpolate8(mapped8, orig8, intensity8);\n"
		"    FragColor = effectAlphaBlend8(mapped8, orig8.a) / 255.0;\n"
		"    return;\n"
		"  }\n"
		"  vec4 shadow8 = vec4(effectToU8(uColor0).rgb, 255.0);\n"
		"  vec4 midtone8 = vec4(effectToU8(uColor1).rgb, 255.0);\n"
		"  vec4 highlight8 = vec4(effectToU8(uColor2).rgb, 255.0);\n"
		"  float blend8 = floor(clamp(uAmount, 0.0, 1.0) * 255.0 + 0.5);\n"
		"  vec4 mapped8 = effectTritone8(shadow8, midtone8, highlight8, luma8);\n"
		"  if (blend8 > 0.0) mapped8 = effectInterpolate8(orig8, mapped8, blend8);\n"
		"  FragColor = effectAlphaBlend8(mapped8, orig8.a) / 255.0;\n"
		"}\n",
		sHeader
	);
	snprintf(
		sShadowFragment, sizeof(sShadowFragment),
		"%s"
		"in vec2 vUV;\n"
		"uniform sampler2D uTexture;\n"
		"uniform sampler2D uTexture2;\n"
		"uniform vec2 uTextureSize;\n"
		"uniform vec2 uOffset;\n"
		"uniform vec4 uShadowColor;\n"
		"out vec4 FragColor;\n"
		"vec4 shadowToU8(vec4 color) {\n"
		"  return floor(clamp(color, 0.0, 1.0) * 255.0 + 0.5);\n"
		"}\n"
		"vec4 shadowAlphaBlend8(vec4 color, float alpha) {\n"
		"  return floor(color * (alpha + 1.0) / 256.0);\n"
		"}\n"
		"void main() {\n"
		"  vec4 orig8 = shadowToU8(texture(uTexture, vUV));\n"
		"  vec2 sampleOffset = vec2(-uOffset.x / uTextureSize.x, uOffset.y / uTextureSize.y);\n"
		"  vec2 shadowUV = vUV + sampleOffset;\n"
		"  float blurAlpha8 = 0.0;\n"
		"  if (shadowUV.x >= 0.0 && shadowUV.x < 1.0 && shadowUV.y >= 0.0 && shadowUV.y < 1.0) {\n"
		"    blurAlpha8 = shadowToU8(texture(uTexture2, shadowUV)).a;\n"
		"  }\n"
		"  vec4 color8 = shadowToU8(uShadowColor);\n"
		"  vec4 shadow8 = shadowAlphaBlend8(vec4(color8.rgb, 255.0), blurAlpha8);\n"
		"  shadow8 = shadowAlphaBlend8(shadow8, color8.a);\n"
		"  vec4 result8 = orig8 + shadowAlphaBlend8(shadow8, 255.0 - orig8.a);\n"
		"  FragColor = min(result8, vec4(255.0)) / 255.0;\n"
		"}\n",
		sHeader
	);
	iRet = xgeShaderCreate(&g_xgeShapeExEffectRenderer.tBlurShader, sVertex, sBlurFragment);
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderCreate(&g_xgeShapeExEffectRenderer.tColorShader, sVertex, sColorFragment);
	}
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderCreate(&g_xgeShapeExEffectRenderer.tShadowShader, sVertex, sShadowFragment);
	}
	if ( iRet != XGE_OK ) {
		if ( g_xgeShapeExEffectRenderer.tShadowShader.iRefCount > 0 ) xgeShaderFree(&g_xgeShapeExEffectRenderer.tShadowShader);
		if ( g_xgeShapeExEffectRenderer.tColorShader.iRefCount > 0 ) xgeShaderFree(&g_xgeShapeExEffectRenderer.tColorShader);
		if ( g_xgeShapeExEffectRenderer.tBlurShader.iRefCount > 0 ) xgeShaderFree(&g_xgeShapeExEffectRenderer.tBlurShader);
		memset(&g_xgeShapeExEffectRenderer, 0, sizeof(g_xgeShapeExEffectRenderer));
		return iRet;
	}
	g_xgeShapeExEffectRenderer.bInitialized = 1;
	return XGE_OK;
}

static int __xgeShapeExEffectBlurPass(
	xge_render_target pDst,
	xge_render_target pSrc,
	int iRadius,
	int bHorizontal,
	int bWrap,
	int bFloorAlpha
)
{
	xge_pass_t tPass;
	xge_material_t tMaterial;
	xge_draw_t tDraw;
	int iRet;

	if ( (pDst == NULL) || (pSrc == NULL) || (pDst == pSrc) || (iRadius < 0) ||
	     (pDst->iWidth != pSrc->iWidth) || (pDst->iHeight != pSrc->iHeight) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeShapeExEffectRendererEnsure();
	if ( iRet != XGE_OK ) return iRet;
	memset(&tPass, 0, sizeof(tPass));
	xgePassInit(&tPass, pDst, XGE_PASS_CLEAR_COLOR, XGE_COLOR_RGBA(0, 0, 0, 0));
	iRet = xgePassBegin(&tPass);
	if ( iRet == XGE_OK ) {
		iRet = xgeShaderUniform2f(
			&g_xgeShapeExEffectRenderer.tBlurShader, "uDirection",
			bHorizontal ? (1.0f / (float)pSrc->iWidth) : 0.0f,
			bHorizontal ? 0.0f : (1.0f / (float)pSrc->iHeight)
		);
	}
	if ( iRet == XGE_OK ) iRet = xgeShaderUniform1f(&g_xgeShapeExEffectRenderer.tBlurShader, "uRadius", (float)iRadius);
	if ( iRet == XGE_OK ) iRet = xgeShaderUniform1f(&g_xgeShapeExEffectRenderer.tBlurShader, "uWrap", bWrap ? 1.0f : 0.0f);
	if ( iRet == XGE_OK ) iRet = xgeShaderUniform1f(&g_xgeShapeExEffectRenderer.tBlurShader, "uFloorAlpha", bFloorAlpha ? 1.0f : 0.0f);
	if ( iRet == XGE_OK ) {
		xgeMaterialInit(&tMaterial);
		xgeMaterialSetShader(&tMaterial, &g_xgeShapeExEffectRenderer.tBlurShader);
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
		if ( iRet == XGE_OK ) iRet = iEndRet;
	}
	return iRet;
}

static int __xgeShapeExEffectColorUniform(xge_shader pShader, const char* sName, uint32_t iColor, int bUseAlpha)
{
	return xgeShaderUniform4f(
		pShader, sName,
		(float)XGE_COLOR_GET_R(iColor) / 255.0f,
		(float)XGE_COLOR_GET_G(iColor) / 255.0f,
		(float)XGE_COLOR_GET_B(iColor) / 255.0f,
		bUseAlpha ? ((float)XGE_COLOR_GET_A(iColor) / 255.0f) : 1.0f
	);
}

static int __xgeShapeExEffectColorPass(xge_render_target pDst, xge_render_target pSrc, const xge_shape_ex_scene_effect_t* pEffect)
{
	xge_pass_t tPass;
	xge_material_t tMaterial;
	xge_draw_t tDraw;
	float fAmount;
	int iRet;

	if ( (pDst == NULL) || (pSrc == NULL) || (pDst == pSrc) || (pEffect == NULL) ||
	     (pDst->iWidth != pSrc->iWidth) || (pDst->iHeight != pSrc->iHeight) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeShapeExEffectRendererEnsure();
	if ( iRet != XGE_OK ) return iRet;
	memset(&tPass, 0, sizeof(tPass));
	xgePassInit(&tPass, pDst, XGE_PASS_CLEAR_COLOR, XGE_COLOR_RGBA(0, 0, 0, 0));
	iRet = xgePassBegin(&tPass);
	if ( iRet == XGE_OK ) iRet = xgeShaderUniform1i(&g_xgeShapeExEffectRenderer.tColorShader, "uEffectType", pEffect->iType);
	if ( iRet == XGE_OK ) iRet = __xgeShapeExEffectColorUniform(&g_xgeShapeExEffectRenderer.tColorShader, "uColor0", pEffect->iColor, pEffect->iType == XGE_SHAPE_EX_EFFECT_FILL);
	if ( iRet == XGE_OK ) iRet = __xgeShapeExEffectColorUniform(&g_xgeShapeExEffectRenderer.tColorShader, "uColor1", pEffect->iColor2, 0);
	if ( iRet == XGE_OK ) iRet = __xgeShapeExEffectColorUniform(&g_xgeShapeExEffectRenderer.tColorShader, "uColor2", pEffect->iColor3, 0);
	if ( pEffect->iType == XGE_SHAPE_EX_EFFECT_TINT ) {
		fAmount = (float)(uint8_t)(pEffect->fIntensity * 2.55f) / 255.0f;
	}
	else if ( pEffect->iType == XGE_SHAPE_EX_EFFECT_TRITONE ) fAmount = (float)pEffect->iBlend / 255.0f;
	else fAmount = 0.0f;
	if ( iRet == XGE_OK ) iRet = xgeShaderUniform1f(&g_xgeShapeExEffectRenderer.tColorShader, "uAmount", fAmount);
	if ( iRet == XGE_OK ) {
		xgeMaterialInit(&tMaterial);
		xgeMaterialSetShader(&tMaterial, &g_xgeShapeExEffectRenderer.tColorShader);
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
		if ( iRet == XGE_OK ) iRet = iEndRet;
	}
	return iRet;
}

static int __xgeShapeExEffectShadowPass(xge_render_target pDst, xge_render_target pSource, xge_render_target pBlur, uint32_t iColor, xge_vec2_t tOffset)
{
	xge_pass_t tPass;
	xge_material_t tMaterial;
	xge_draw_t tDraw;
	int iRet;

	if ( (pDst == NULL) || (pSource == NULL) || (pBlur == NULL) || (pDst == pSource) || (pDst == pBlur) ||
	     (pDst->iWidth != pSource->iWidth) || (pDst->iHeight != pSource->iHeight) ||
	     (pDst->iWidth != pBlur->iWidth) || (pDst->iHeight != pBlur->iHeight) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xgeShapeExEffectRendererEnsure();
	if ( iRet != XGE_OK ) return iRet;
	memset(&tPass, 0, sizeof(tPass));
	xgePassInit(&tPass, pDst, XGE_PASS_CLEAR_COLOR, XGE_COLOR_RGBA(0, 0, 0, 0));
	iRet = xgePassBegin(&tPass);
	if ( iRet == XGE_OK ) iRet = xgeShaderUniform2f(&g_xgeShapeExEffectRenderer.tShadowShader, "uTextureSize", (float)pSource->iWidth, (float)pSource->iHeight);
	if ( iRet == XGE_OK ) iRet = xgeShaderUniform2f(&g_xgeShapeExEffectRenderer.tShadowShader, "uOffset", tOffset.fX, tOffset.fY);
	if ( iRet == XGE_OK ) iRet = __xgeShapeExEffectColorUniform(&g_xgeShapeExEffectRenderer.tShadowShader, "uShadowColor", iColor, 1);
	if ( iRet == XGE_OK ) {
		xgeMaterialInit(&tMaterial);
		xgeMaterialSetShader(&tMaterial, &g_xgeShapeExEffectRenderer.tShadowShader);
		xgeMaterialSetTexture(&tMaterial, xgeRenderTargetTexture(pSource));
		xgeMaterialSetTexture2(&tMaterial, xgeRenderTargetTexture(pBlur));
		xgeMaterialSetBlend(&tMaterial, XGE_BLEND_NONE);
		memset(&tDraw, 0, sizeof(tDraw));
		tDraw.pTexture = xgeRenderTargetTexture(pSource);
		tDraw.tSrc = (xge_rect_t){0.0f, 0.0f, (float)pSource->iWidth, (float)pSource->iHeight};
		tDraw.tDst = (xge_rect_t){0.0f, 0.0f, (float)pDst->iWidth, (float)pDst->iHeight};
		tDraw.iColor = XGE_COLOR_RGBA(255, 255, 255, 255);
		tDraw.iFlags = XGE_DRAW_SCREEN_SPACE | XGE_DRAW_FLIP_Y;
		xgeMaterialDraw(&tMaterial, &tDraw);
		xgeMaterialFree(&tMaterial);
	}
	if ( tPass.bActive ) {
		int iEndRet = xgePassEnd(&tPass);
		if ( iRet == XGE_OK ) iRet = iEndRet;
	}
	return iRet;
}

typedef struct xge_shape_ex_effect_scene_context_t {
	xge_shape_ex_scene pScene;
	float fTolerance;
	float fParentOpacity;
} xge_shape_ex_effect_scene_context_t;

typedef struct xge_shape_ex_effect_output_context_t {
	xge_render_target pTarget;
	int iWidth;
	int iHeight;
} xge_shape_ex_effect_output_context_t;

static int __xgeShapeExEffectSceneSourceDraw(void* pUser, xge_shape_ex_matrix_t tLocalParent)
{
	xge_shape_ex_effect_scene_context_t* pContext = (xge_shape_ex_effect_scene_context_t*)pUser;
	return __xgeShapeExSceneDrawInternal(
		pContext->pScene, pContext->fTolerance, 1, tLocalParent, pContext->fParentOpacity, 1
	);
}

static int __xgeShapeExEffectOutputDraw(void* pUser, xge_shape_ex_matrix_t tLocalParent)
{
	xge_shape_ex_effect_output_context_t* pContext = (xge_shape_ex_effect_output_context_t*)pUser;
	xge_draw_t tDraw;
	int iOldBlend;
	int iRet;

	(void)tLocalParent;
	if ( (pContext == NULL) || (pContext->pTarget == NULL) ||
	     (pContext->iWidth <= 0) || (pContext->iHeight <= 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	memset(&tDraw, 0, sizeof(tDraw));
	tDraw.pTexture = xgeRenderTargetTexture(pContext->pTarget);
	tDraw.tSrc = (xge_rect_t){0.0f, 0.0f, (float)pContext->iWidth, (float)pContext->iHeight};
	tDraw.tDst = tDraw.tSrc;
	tDraw.iColor = XGE_COLOR_RGBA(255, 255, 255, 255);
	tDraw.iFlags = XGE_DRAW_SCREEN_SPACE | XGE_DRAW_FLIP_Y;
	iOldBlend = xgeBlendGet();
	xgeBlendSet(XGE_BLEND_ALPHA);
	xgeDrawEx(&tDraw);
	iRet = xgeFlush();
	xgeBlendSet(iOldBlend);
	return iRet;
}

static int __xgeShapeExSceneDrawEffects(xge_shape_ex_scene pScene, float fTolerance, xge_shape_ex_matrix_t tScreenParent, float fParentOpacity, int bSuppressOwnBlend)
{
	xge_shape_ex_scene_t tBaseScene;
	xge_shape_ex_effect_scene_context_t tContext;
	xge_shape_ex_stencil_state_t tStencilState;
	xge_shape_ex_matrix_t tEffectMatrix;
	xge_shape_ex_matrix_t tOffsetMatrix;
	xge_shape_ex_matrix_t tLocalParent;
	xge_render_target_t tTargetA;
	xge_render_target_t tTargetB;
	xge_render_target_t tTargetC;
	xge_shape_ex_effect_target_set_t* pCachedTargets;
	xge_render_target pTargetA;
	xge_render_target pTargetB;
	xge_render_target pTargetC;
	xge_render_target pCurrent;
	xge_render_target pScratch;
	xge_render_target pAux;
	xge_draw_t tDraw;
	xge_rect_t tBounds;
	xge_rect_t tOldClip;
	xge_rect_t tFrame;
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;
	int bOldClip;
	int iOldBlend;
	int iWidth;
	int iHeight;
	int bCachedTargets;
	int bStateSuspended;
	int i;
	int iRet;

	if ( !__xgeShapeExSceneValid(pScene) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( !pScene->bVisible || (fParentOpacity * pScene->fOpacity <= 0.0f) ) return XGE_OK;
	tBaseScene = *pScene;
	tBaseScene.iEffectCount = 0;
	tBaseScene.iMaskMethod = XGE_SHAPE_EX_MASK_NONE;
	tBaseScene.iMaskTargetType = XGE_SHAPE_EX_MASK_TARGET_NONE;
	tBaseScene.pMaskShape = NULL;
	tBaseScene.pMaskScene = NULL;
	tBaseScene.fOpacity = 1.0f;
	iRet = __xgeShapeExSceneGetRenderBounds(&tBaseScene, fTolerance, &tBounds);
	if ( iRet != XGE_OK ) return iRet;
	tBounds = __xgeShapeExMatrixRectBounds(tScreenParent, tBounds);
	tEffectMatrix = __xgeShapeExMatrixMul(tScreenParent, pScene->tTransform);
	__xgeShapeExSceneEffectBoundsApply(pScene, tEffectMatrix, &tBounds);
	tFrame = (xge_rect_t){0.0f, 0.0f, (float)g_xge.iWidth, (float)g_xge.iHeight};
	tBounds = __xgeShapeExRectIntersect(tBounds, tFrame);
	bOldClip = g_xge.bClipEnabled;
	tOldClip = xgeClipGet();
	if ( bOldClip ) tBounds = __xgeShapeExRectIntersect(tBounds, tOldClip);
	if ( (tBounds.fW <= 0.0f) || (tBounds.fH <= 0.0f) ) return XGE_OK;
	fLeft = floorf(tBounds.fX);
	fTop = floorf(tBounds.fY);
	fRight = ceilf(tBounds.fX + tBounds.fW);
	fBottom = ceilf(tBounds.fY + tBounds.fH);
	iWidth = (int)(fRight - fLeft);
	iHeight = (int)(fBottom - fTop);
	if ( (iWidth <= 0) || (iHeight <= 0) ) return XGE_OK;
	if ( glGetIntegerv != NULL ) {
		GLint iMaxTextureSize = 0;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &iMaxTextureSize);
		if ( (iMaxTextureSize > 0) && ((iWidth > iMaxTextureSize) || (iHeight > iMaxTextureSize)) ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
	}
	memset(&tTargetA, 0, sizeof(tTargetA));
	memset(&tTargetB, 0, sizeof(tTargetB));
	memset(&tTargetC, 0, sizeof(tTargetC));
	pTargetA = &tTargetA;
	pTargetB = &tTargetB;
	pTargetC = &tTargetC;
	pCachedTargets = NULL;
	bCachedTargets = 0;
	memset(&tStencilState, 0, sizeof(tStencilState));
	iOldBlend = xgeBlendGet();
	bStateSuspended = 0;
	iRet = __xgeShapeAutoBatchFlush();
	if ( iRet == XGE_OK ) {
		xgeClipClear();
		__xgeShapeExStencilSuspend(&tStencilState);
		bStateSuspended = 1;
		xgeBlendSet(XGE_BLEND_ALPHA);
		pCachedTargets = __xgeShapeExEffectTargetCacheAcquire(iWidth, iHeight);
		bCachedTargets = pCachedTargets != NULL;
		if ( bCachedTargets ) {
			pTargetA = &pCachedTargets->tTargetA;
			pTargetB = &pCachedTargets->tTargetB;
			pTargetC = &pCachedTargets->tTargetC;
		}
	}
	if ( (iRet == XGE_OK) && !bCachedTargets ) {
		iRet = xgeRenderTargetCreate(pTargetA, iWidth, iHeight);
		if ( iRet == XGE_OK ) iRet = xgeRenderTargetCreate(pTargetB, iWidth, iHeight);
		if ( iRet == XGE_OK ) iRet = xgeRenderTargetCreate(pTargetC, iWidth, iHeight);
	}
	tOffsetMatrix = __xgeShapeExMatrixIdentity();
	tOffsetMatrix.fE = -fLeft;
	tOffsetMatrix.fF = -fTop;
	tLocalParent = __xgeShapeExMatrixMul(tOffsetMatrix, tScreenParent);
	tContext.pScene = &tBaseScene;
	tContext.fTolerance = fTolerance;
	tContext.fParentOpacity = 1.0f;
	if ( iRet == XGE_OK ) {
		iRet = __xgeShapeExDrawToTarget(pTargetA, __xgeShapeExEffectSceneSourceDraw, &tContext, tLocalParent);
	}
	pCurrent = pTargetA;
	pScratch = pTargetB;
	pAux = pTargetC;
	for ( i = 0; (i < pScene->iEffectCount) && (iRet == XGE_OK); i++ ) {
		const xge_shape_ex_scene_effect_t* pEffect = &pScene->pEffects[i];
		xge_shape_ex_effect_blur_config_t tBlur;
		int j;

		__xgeShapeExEffectBlurConfig(pEffect->fSigma, pEffect->iQuality, tEffectMatrix, &tBlur);

		if ( pEffect->iType == XGE_SHAPE_EX_EFFECT_GAUSSIAN_BLUR ) {
			if ( tBlur.iExtent <= 0 ) continue;
			if ( pEffect->iDirection != XGE_SHAPE_EX_BLUR_VERTICAL ) {
				for ( j = 0; (j < tBlur.iLevel) && (iRet == XGE_OK); j++ ) {
					xge_render_target pSwap;
					iRet = __xgeShapeExEffectBlurPass(
						pScratch, pCurrent, tBlur.arrKernel[j], 1,
						pEffect->iBorder == XGE_SHAPE_EX_BORDER_WRAP, 0
					);
					pSwap = pCurrent;
					pCurrent = pScratch;
					pScratch = pSwap;
				}
			}
			if ( (iRet == XGE_OK) && (pEffect->iDirection != XGE_SHAPE_EX_BLUR_HORIZONTAL) ) {
				for ( j = 0; (j < tBlur.iLevel) && (iRet == XGE_OK); j++ ) {
					xge_render_target pSwap;
					iRet = __xgeShapeExEffectBlurPass(
						pScratch, pCurrent, tBlur.arrKernel[j], 0,
						pEffect->iBorder == XGE_SHAPE_EX_BORDER_WRAP, 0
					);
					pSwap = pCurrent;
					pCurrent = pScratch;
					pScratch = pSwap;
				}
			}
		} else if ( pEffect->iType == XGE_SHAPE_EX_EFFECT_DROP_SHADOW ) {
			xge_render_target pOriginal;
			xge_render_target pBlur;
			xge_render_target pResult;
			xge_render_target pDestination;
			xge_vec2_t tShadowOffset;

			if ( XGE_COLOR_GET_A(pEffect->iColor) == 0u ) continue;
			pOriginal = pCurrent;
			tShadowOffset = __xgeShapeExEffectShadowOffset(pEffect, tEffectMatrix);
			if ( tBlur.iExtent > 0 ) {
				pBlur = pOriginal;
				pDestination = pScratch;
				for ( j = 0; (j < tBlur.iLevel) && (iRet == XGE_OK); j++ ) {
					iRet = __xgeShapeExEffectBlurPass(pDestination, pBlur, tBlur.arrKernel[j], 1, 0, 1);
					pBlur = pDestination;
					pDestination = (pDestination == pScratch) ? pAux : pScratch;
				}
				for ( j = 0; (j < tBlur.iLevel) && (iRet == XGE_OK); j++ ) {
					iRet = __xgeShapeExEffectBlurPass(pDestination, pBlur, tBlur.arrKernel[j], 0, 0, 1);
					pBlur = pDestination;
					pDestination = (pDestination == pScratch) ? pAux : pScratch;
				}
				pResult = pDestination;
			} else {
				pBlur = pOriginal;
				pResult = pScratch;
			}
			if ( iRet == XGE_OK ) {
				iRet = __xgeShapeExEffectShadowPass(pResult, pOriginal, pBlur, pEffect->iColor, tShadowOffset);
			}
			if ( iRet == XGE_OK ) {
				pCurrent = pResult;
				pScratch = pOriginal;
				if ( tBlur.iExtent > 0 ) pAux = pBlur;
			}
		} else if ( (pEffect->iType == XGE_SHAPE_EX_EFFECT_FILL) ||
		            ((pEffect->iType == XGE_SHAPE_EX_EFFECT_TINT) && (pEffect->fIntensity > 0.0f)) ||
		            ((pEffect->iType == XGE_SHAPE_EX_EFFECT_TRITONE) && (pEffect->iBlend < 255)) ) {
			xge_render_target pSwap;
			iRet = __xgeShapeExEffectColorPass(pScratch, pCurrent, pEffect);
			pSwap = pCurrent;
			pCurrent = pScratch;
			pScratch = pSwap;
		}
	}
	xgeBlendSet(iOldBlend);
	if ( bStateSuspended ) {
		__xgeShapeExStencilResume(&tStencilState);
	}
	if ( bOldClip ) xgeClipSet(tOldClip);
	else xgeClipClear();
	if ( iRet == XGE_OK ) {
		int iOutputBlend = __xgeShapeExMaskOutputBlend(pScene->bBlendSet, pScene->iBlend, bSuppressOwnBlend);
		float fOpacity = fParentOpacity * pScene->fOpacity;

		if ( fOpacity < 0.0f ) fOpacity = 0.0f;
		if ( fOpacity > 1.0f ) fOpacity = 1.0f;
		if ( fOpacity < (1.0f - XGE_SHAPE_EX_EPSILON) ) {
			xge_shape_ex_effect_output_context_t tOutputContext;

			tOutputContext.pTarget = pCurrent;
			tOutputContext.iWidth = iWidth;
			tOutputContext.iHeight = iHeight;
			iRet = __xgeShapeExCompositeBlend(
				XGE_BLEND_ALPHA, XGE_SHAPE_EX_BLEND_SOURCE_SCENE, fOpacity, tBounds,
				__xgeShapeExEffectOutputDraw, &tOutputContext, NULL, NULL,
				__xgeShapeExMatrixIdentity()
			);
		} else {
			memset(&tDraw, 0, sizeof(tDraw));
			tDraw.pTexture = xgeRenderTargetTexture(pCurrent);
			tDraw.tSrc = (xge_rect_t){0.0f, 0.0f, (float)iWidth, (float)iHeight};
			tDraw.tDst = (xge_rect_t){fLeft, fTop, (float)iWidth, (float)iHeight};
			tDraw.iColor = XGE_COLOR_RGBA(255, 255, 255, 255);
			tDraw.iFlags = XGE_DRAW_SCREEN_SPACE | XGE_DRAW_FLIP_Y;
			xgeBlendSet(iOutputBlend);
			xgeDrawEx(&tDraw);
			iRet = xgeFlush();
			xgeBlendSet(iOldBlend);
		}
	}
	if ( !bCachedTargets ) {
		xgeRenderTargetFree(&tTargetC);
		xgeRenderTargetFree(&tTargetB);
		xgeRenderTargetFree(&tTargetA);
	} else {
		__xgeShapeExEffectTargetCacheRelease(pCachedTargets);
	}
	return iRet;
}

typedef struct xge_shape_ex_blend_scene_context_t {
	xge_shape_ex_scene pScene;
	float fTolerance;
	float fParentOpacity;
} xge_shape_ex_blend_scene_context_t;

typedef struct xge_shape_ex_opacity_scene_context_t {
	xge_shape_ex_scene pScene;
	float fTolerance;
} xge_shape_ex_opacity_scene_context_t;

static int __xgeShapeExSceneNeedsOpacityComposite(xge_shape_ex_scene pScene, float fParentOpacity)
{
	float fOpacity;

	if ( !__xgeShapeExSceneValid(pScene) || (pScene->iChildCount <= 0) ) return 0;
	fOpacity = fParentOpacity * pScene->fOpacity;
	if ( fOpacity >= (1.0f - XGE_SHAPE_EX_EPSILON) ) return 0;
	if ( pScene->iChildCount > 1 ) return 1;
	return pScene->pChildren[0].iType == XGE_SHAPE_EX_SCENE_CHILD_SCENE;
}

static int __xgeShapeExOpacitySceneDraw(void* pUser, xge_shape_ex_matrix_t tLocalParent)
{
	xge_shape_ex_opacity_scene_context_t* pContext = (xge_shape_ex_opacity_scene_context_t*)pUser;
	xge_shape_ex_scene_t tSource;

	if ( (pContext == NULL) || !__xgeShapeExSceneValid(pContext->pScene) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tSource = *pContext->pScene;
	tSource.fOpacity = 1.0f;
	tSource.bBlendSet = 0;
	return __xgeShapeExSceneDrawInternal(
		&tSource, pContext->fTolerance, 1, tLocalParent, 1.0f, 1
	);
}

static int __xgeShapeExSceneDrawOpacityComposite(
	xge_shape_ex_scene pScene,
	float fTolerance,
	xge_shape_ex_matrix_t tScreenParent,
	float fParentOpacity
)
{
	xge_shape_ex_opacity_scene_context_t tContext;
	xge_rect_t tBounds;
	float fOpacity;
	int iRet;

	iRet = __xgeShapeExSceneGetRenderBounds(pScene, fTolerance, &tBounds);
	if ( iRet != XGE_OK ) return iRet;
	tBounds = __xgeShapeExMatrixRectBounds(tScreenParent, tBounds);
	tContext.pScene = pScene;
	tContext.fTolerance = fTolerance;
	fOpacity = fParentOpacity * pScene->fOpacity;
	return __xgeShapeExCompositeBlend(
		XGE_BLEND_ALPHA, XGE_SHAPE_EX_BLEND_SOURCE_SCENE, fOpacity, tBounds,
		__xgeShapeExOpacitySceneDraw, &tContext, NULL, NULL, tScreenParent
	);
}

static int __xgeShapeExBlendSceneDraw(void* pUser, xge_shape_ex_matrix_t tLocalParent)
{
	xge_shape_ex_blend_scene_context_t* pContext = (xge_shape_ex_blend_scene_context_t*)pUser;
	return __xgeShapeExSceneDrawDispatch(
		pContext->pScene, pContext->fTolerance, 1, tLocalParent, pContext->fParentOpacity, 1
	);
}

static int __xgeShapeExSceneDrawDispatch(xge_shape_ex_scene pScene, float fTolerance, int bScreenSpace, xge_shape_ex_matrix_t tParent, float fParentOpacity, int bSuppressOwnBlend)
{
	xge_shape_ex_blend_scene_context_t tContext;
	xge_shape_ex_matrix_t tScreenParent;
	xge_rect_t tBounds;
	int iRet;

	if ( !__xgeShapeExSceneValid(pScene) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( !bScreenSpace ) {
		tParent = __xgeShapeExScreenParent(tParent, 0);
		bScreenSpace = 1;
	}
	if ( !bSuppressOwnBlend && pScene->bBlendSet && __xgeShapeExBlendNeedsComposite(pScene->iBlend) ) {
		int iBlendSource;

		tScreenParent = __xgeShapeExScreenParent(tParent, bScreenSpace);
		iRet = __xgeShapeExSceneGetRenderBounds(pScene, fTolerance, &tBounds);
		if ( iRet != XGE_OK ) {
			return iRet;
		}
		tBounds = __xgeShapeExMatrixRectBounds(tScreenParent, tBounds);
		tContext.pScene = pScene;
		tContext.fTolerance = fTolerance;
		tContext.fParentOpacity = fParentOpacity;
		if ( pScene->iMaskTargetType != XGE_SHAPE_EX_MASK_TARGET_NONE ) {
			iBlendSource = XGE_SHAPE_EX_BLEND_SOURCE_MASKED;
		} else if ( (g_xgeShapeExMaskCompositeSourceDepth > 0) ||
		            (fParentOpacity < (1.0f - XGE_SHAPE_EX_EPSILON)) ) {
			iBlendSource = XGE_SHAPE_EX_BLEND_SOURCE_INHERITED_OPACITY;
		} else {
			iBlendSource = XGE_SHAPE_EX_BLEND_SOURCE_SCENE;
		}
		return __xgeShapeExCompositeBlend(
			pScene->iBlend, iBlendSource,
			__xgeShapeExBlendSourceOpacity(iBlendSource, fParentOpacity, pScene->fOpacity), tBounds,
			__xgeShapeExBlendSceneDraw, &tContext, NULL, NULL, tScreenParent
		);
	}
	if ( __xgeShapeExClipCoverageSupported(pScene->pClipShapeModes, pScene->iClipShapeCount) ) {
		return __xgeShapeExClipCoverageScene(
			pScene, fTolerance, bScreenSpace, tParent, fParentOpacity, bSuppressOwnBlend
		);
	}
	if ( pScene->iMaskTargetType != XGE_SHAPE_EX_MASK_TARGET_NONE ) {
		xge_shape_ex_mask_paint_context_t tSourceContext;
		xge_shape_ex_mask_paint_context_t tMaskContext;
		xge_rect_t tSourceBounds;
		xge_rect_t tMaskBounds;
		int iOutputBlend;

		memset(&tSourceContext, 0, sizeof(tSourceContext));
		memset(&tMaskContext, 0, sizeof(tMaskContext));
		tScreenParent = __xgeShapeExScreenParent(tParent, bScreenSpace);
		tSourceContext.iTargetType = XGE_SHAPE_EX_MASK_TARGET_SCENE;
		tSourceContext.pScene = pScene;
		tSourceContext.fTolerance = fTolerance;
		tSourceContext.fParentOpacity = fParentOpacity;
		tSourceContext.bSource = 1;
		tMaskContext.iTargetType = pScene->iMaskTargetType;
		tMaskContext.pShape = pScene->pMaskShape;
		tMaskContext.pScene = pScene->pMaskScene;
		tMaskContext.fTolerance = fTolerance;
		tMaskContext.fParentOpacity = 1.0f;
		tMaskContext.bSource = 0;
		iRet = __xgeShapeExMaskPaintBounds(&tSourceContext, tScreenParent, &tSourceBounds);
		if ( iRet != XGE_OK ) return iRet;
		iRet = __xgeShapeExMaskPaintBounds(&tMaskContext, tScreenParent, &tMaskBounds);
		if ( iRet != XGE_OK ) return iRet;
		tBounds = __xgeShapeExMaskCombineBounds(tSourceBounds, tMaskBounds, pScene->iMaskMethod);
		iOutputBlend = __xgeShapeExMaskOutputBlend(pScene->bBlendSet, pScene->iBlend, bSuppressOwnBlend);
		return __xgeShapeExCompositeMask(
			pScene->iMaskMethod, iOutputBlend, tBounds,
			__xgeShapeExMaskPaintDraw, &tSourceContext,
			__xgeShapeExMaskPaintDraw, &tMaskContext,
			tScreenParent, !bSuppressOwnBlend
		);
	}
	if ( pScene->iEffectCount > 0 ) {
		tScreenParent = __xgeShapeExScreenParent(tParent, bScreenSpace);
		return __xgeShapeExSceneDrawEffects(
			pScene, fTolerance, tScreenParent, fParentOpacity, bSuppressOwnBlend
		);
	}
	if ( __xgeShapeExSceneNeedsOpacityComposite(pScene, fParentOpacity) ) {
		tScreenParent = __xgeShapeExScreenParent(tParent, bScreenSpace);
		return __xgeShapeExSceneDrawOpacityComposite(
			pScene, fTolerance, tScreenParent, fParentOpacity
		);
	}
	return __xgeShapeExSceneDrawInternal(
		pScene, fTolerance, bScreenSpace, tParent, fParentOpacity, bSuppressOwnBlend
	);
}

int xgeShapeExSceneDraw(xge_shape_ex_scene pScene, float fTolerance)
{
	return xgeShapeExSceneDrawEx(pScene, fTolerance, NULL, 1.0f);
}

int xgeShapeExSceneDrawPx(xge_shape_ex_scene pScene, float fTolerance)
{
	return xgeShapeExSceneDrawPxEx(pScene, fTolerance, NULL, 1.0f);
}

int xgeShapeExSceneDrawEx(xge_shape_ex_scene pScene, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, float fParentOpacity)
{
	xge_shape_ex_matrix_t tParent;

	if ( !__xgeShapeExSceneValid(pScene) || !__xgeShapeExFloatFinite(fTolerance) ||
	     !__xgeShapeExFloatFinite(fParentOpacity) ||
	     ((pParentMatrix != NULL) && !__xgeShapeExMatrixFinite(pParentMatrix)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tParent = pParentMatrix != NULL ? *pParentMatrix : __xgeShapeExMatrixIdentity();
	return __xgeShapeExSceneDrawDispatch(pScene, fTolerance, 0, tParent, fParentOpacity, 0);
}

int xgeShapeExSceneDrawPxEx(xge_shape_ex_scene pScene, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, float fParentOpacity)
{
	xge_shape_ex_matrix_t tParent;

	if ( !__xgeShapeExSceneValid(pScene) || !__xgeShapeExFloatFinite(fTolerance) ||
	     !__xgeShapeExFloatFinite(fParentOpacity) ||
	     ((pParentMatrix != NULL) && !__xgeShapeExMatrixFinite(pParentMatrix)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	tParent = pParentMatrix != NULL ? *pParentMatrix : __xgeShapeExMatrixIdentity();
	return __xgeShapeExSceneDrawDispatch(pScene, fTolerance, 1, tParent, fParentOpacity, 0);
}
