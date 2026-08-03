#ifndef XGE_H
#define XGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "xrt.h"

#define XGE_VERSION_MAJOR	2
#define XGE_VERSION_MINOR	0
#define XGE_VERSION_PATCH	0

#if !defined(XGE_DEBUG) && !defined(XGE_RELEASE)
	#if defined(NDEBUG)
		#define XGE_RELEASE	1
	#else
		#define XGE_DEBUG	1
	#endif
#endif

#if !defined(XGE_DEBUGMODE)
	#define XGE_DEBUGMODE	0
#endif

#if XGE_DEBUGMODE
	#define XGE_HAS_DEBUGMODE	1
#else
	#define XGE_HAS_DEBUGMODE	0
#endif

#if defined(_WIN32) || defined(_WIN64)
	#if defined(XGE_DLL)
		#if defined(XGE_BUILD_DLL)
			#define XGE_API __declspec(dllexport)
		#else
			#define XGE_API __declspec(dllimport)
		#endif
	#else
		#define XGE_API
	#endif
#else
	#define XGE_API
#endif

#define XGE_INIT_WINDOW		0x0000
#define XGE_INIT_FULLSCREEN	0x0001
#define XGE_INIT_NOFRAME	0x0002
#define XGE_INIT_RESIZABLE	0x0004
#define XGE_INIT_HIGHDPI	0x0008
#define XGE_INIT_VSYNC		0x0010
#define XGE_INIT_OFFSCREEN	0x0020
#define XGE_INIT_ON_DEMAND	0x0040

#define XGE_RUN_GAME_LOOP	0
#define XGE_RUN_MANUAL		1

#define XGE_PLATFORM_BACKEND_NONE	0
#define XGE_PLATFORM_BACKEND_SOKOL	1
#define XGE_PLATFORM_BACKEND_MINIPROGRAM	2
#define XGE_PLATFORM_BACKEND_EGL	3
#define XGE_PLATFORM_BACKEND_CUSTOM	100

#define XGE_GPU_BACKEND_NONE		0
#define XGE_GPU_BACKEND_OPENGL33	1
#define XGE_GPU_BACKEND_GLES30		2
#define XGE_GPU_BACKEND_WEBGL2		3
#define XGE_GPU_BACKEND_CUSTOM		100

#define XGE_UPDATE_VARIABLE	0
#define XGE_UPDATE_FIXED	1

#define XGE_LOG_TRACE	0
#define XGE_LOG_DEBUG	1
#define XGE_LOG_INFO	2
#define XGE_LOG_WARN	3
#define XGE_LOG_ERROR	4
#define XGE_LOG_FATAL	5
#define XGE_LOG_OFF	6

#define XGE_KEY_COUNT		512
#define XGE_KEY_SPACE		32
#define XGE_KEY_ESCAPE		256
#define XGE_KEY_ENTER		257
#define XGE_KEY_TAB			258
#define XGE_KEY_BACKSPACE	259
#define XGE_KEY_DELETE		261
#define XGE_KEY_RIGHT		262
#define XGE_KEY_LEFT		263
#define XGE_KEY_DOWN		264
#define XGE_KEY_UP			265
#define XGE_KEY_PAGE_UP		266
#define XGE_KEY_PAGE_DOWN	267
#define XGE_KEY_HOME		268
#define XGE_KEY_END			269
#define XGE_KEY_F10			299
#define XGE_KEY_MENU		348
#define XGE_TEXT_MAX		32
#define XGE_TOUCH_MAX		8
#define XGE_GAMEPAD_MAX		4
#define XGE_GAMEPAD_BUTTON_COUNT	32
#define XGE_GAMEPAD_AXIS_COUNT	8
#define XGE_MOUSE_LEFT		0x01
#define XGE_MOUSE_RIGHT		0x02
#define XGE_MOUSE_MIDDLE	0x04
#define XGE_DIRTY_RECT_MAX	64
#define XGE_SCENE_STACK_MAX	16

#define XGE_PIXEL_RGBA8		1
#define XGE_PIXEL_A8		2
#define XGE_PIXEL_YUV420P	3

#define XGE_IMAGE_PREMULTIPLIED		0x0001
#define XGE_IMAGE_STRAIGHT_ALPHA	0x0002

#define XGE_TEXTURE_FALLBACK		0x0001
#define XGE_TEXTURE_UPLOAD_QUEUED	0x0002
#define XGE_FONT_FALLBACK			0x0001
#define XGE_FONT_SIZE_EM			0x0002
#define XGE_SOUND_FALLBACK			0x80000000u

#define XGE_RENDER_TARGET_WINDOW	0x0001
#define XGE_RENDER_TARGET_TEXTURE	0x0002
#define XGE_PASS_CLEAR_COLOR		0x0001

#define XGE_BUFFER_VERTEX		1
#define XGE_BUFFER_INDEX		2
#define XGE_BUFFER_UNIFORM		3

#define XGE_BUFFER_STATIC		1
#define XGE_BUFFER_DYNAMIC		2

#define XGE_FILTER_NEAREST		1
#define XGE_FILTER_LINEAR		2

#define XGE_WRAP_CLAMP			1
#define XGE_WRAP_REPEAT		2

#define XGE_BLEND_NONE			0
#define XGE_BLEND_ALPHA			1
#define XGE_BLEND_ADD			2
#define XGE_BLEND_MULTIPLY		3
#define XGE_BLEND_SCREEN		4
#define XGE_BLEND_CUSTOM		5
#define XGE_BLEND_DARKEN		6
#define XGE_BLEND_LIGHTEN		7
#define XGE_BLEND_OVERLAY		8
#define XGE_BLEND_COLOR_DODGE	9
#define XGE_BLEND_COLOR_BURN	10
#define XGE_BLEND_HARD_LIGHT	11
#define XGE_BLEND_SOFT_LIGHT	12
#define XGE_BLEND_DIFFERENCE	13
#define XGE_BLEND_EXCLUSION		14
#define XGE_BLEND_HUE			15
#define XGE_BLEND_SATURATION	16
#define XGE_BLEND_COLOR			17
#define XGE_BLEND_LUMINOSITY	18

#define XGE_MATERIAL_DEFAULT_BLEND	-1
#define XGE_SHADER_DEFINE_MAX		8
#define XGE_SHADER_DEFINE_NAME_MAX	32

#define XGE_COORD_Y_DOWN	0
#define XGE_COORD_CENTER	1

#define XGE_CAMERA_ORTHO		0
#define XGE_CAMERA_PERSPECTIVE	1

#define XGE_DRAW_FLIP_X		0x0001
#define XGE_DRAW_FLIP_Y		0x0002
#define XGE_DRAW_SCREEN_SPACE	0x0004
#define XGE_NINE_PATCH_STRETCH	0
#define XGE_NINE_PATCH_TILE		1

#define XGE_MESH_DYNAMIC	0x0001

#define XGE_SHAPE_EX_CMD_CLOSE		0
#define XGE_SHAPE_EX_CMD_MOVE_TO	1
#define XGE_SHAPE_EX_CMD_LINE_TO	2
#define XGE_SHAPE_EX_CMD_CUBIC_TO	3
#define XGE_SHAPE_EX_CMD_QUAD_TO	4
#define XGE_SHAPE_EX_FILL_NON_ZERO	0
#define XGE_SHAPE_EX_FILL_EVEN_ODD	1
#define XGE_SHAPE_EX_JOIN_MITER		0
#define XGE_SHAPE_EX_JOIN_ROUND		1
#define XGE_SHAPE_EX_JOIN_BEVEL		2
#define XGE_SHAPE_EX_CAP_BUTT		0
#define XGE_SHAPE_EX_CAP_ROUND		1
#define XGE_SHAPE_EX_CAP_SQUARE		2
#define XGE_SHAPE_EX_GRADIENT_USER_SPACE			0
#define XGE_SHAPE_EX_GRADIENT_OBJECT_BOUNDING_BOX	1
#define XGE_SHAPE_EX_GRADIENT_SPREAD_PAD		0
#define XGE_SHAPE_EX_GRADIENT_SPREAD_REFLECT	1
#define XGE_SHAPE_EX_GRADIENT_SPREAD_REPEAT		2
#define XGE_SHAPE_EX_PAINT_SOLID				0
#define XGE_SHAPE_EX_PAINT_LINEAR_GRADIENT		1
#define XGE_SHAPE_EX_PAINT_RADIAL_GRADIENT		2
#define XGE_SHAPE_EX_CLIP_INTERSECT			0
#define XGE_SHAPE_EX_CLIP_SUBTRACT			1
#define XGE_SHAPE_EX_MASK_NONE				0
#define XGE_SHAPE_EX_MASK_ALPHA				1
#define XGE_SHAPE_EX_MASK_INV_ALPHA			2
#define XGE_SHAPE_EX_MASK_LUMA				3
#define XGE_SHAPE_EX_MASK_INV_LUMA			4
#define XGE_SHAPE_EX_MASK_ADD				5
#define XGE_SHAPE_EX_MASK_SUBTRACT			6
#define XGE_SHAPE_EX_MASK_INTERSECT			7
#define XGE_SHAPE_EX_MASK_DIFFERENCE		8
#define XGE_SHAPE_EX_MASK_LIGHTEN			9
#define XGE_SHAPE_EX_MASK_DARKEN			10
#define XGE_SHAPE_EX_MASK_TARGET_NONE		0
#define XGE_SHAPE_EX_MASK_TARGET_SHAPE		1
#define XGE_SHAPE_EX_MASK_TARGET_SCENE		2
#define XGE_SHAPE_EX_SCENE_CHILD_SHAPE		1
#define XGE_SHAPE_EX_SCENE_CHILD_SCENE		2
#define XGE_SHAPE_EX_EFFECT_GAUSSIAN_BLUR	1
#define XGE_SHAPE_EX_EFFECT_DROP_SHADOW		2
#define XGE_SHAPE_EX_EFFECT_FILL			3
#define XGE_SHAPE_EX_EFFECT_TINT			4
#define XGE_SHAPE_EX_EFFECT_TRITONE			5
#define XGE_SHAPE_EX_BLUR_BOTH			0
#define XGE_SHAPE_EX_BLUR_HORIZONTAL		1
#define XGE_SHAPE_EX_BLUR_VERTICAL		2
#define XGE_SHAPE_EX_BORDER_DUPLICATE		0
#define XGE_SHAPE_EX_BORDER_WRAP			1

#define XGE_SHAPE_ROUND_RECT_AUTO	0
#define XGE_SHAPE_ROUND_RECT_SDF	1
#define XGE_SHAPE_ROUND_RECT_MESH	2

#define XGE_SHAPE_RENDER_AA_MESH	0
#define XGE_SHAPE_RENDER_SDF		1

#define XGE_SVG_ASPECT_ALIGN_MIN	0
#define XGE_SVG_ASPECT_ALIGN_MID	1
#define XGE_SVG_ASPECT_ALIGN_MAX	2
#define XGE_SVG_ASPECT_NONE		0
#define XGE_SVG_ASPECT_MEET		1
#define XGE_SVG_ASPECT_SLICE		2
#define XGE_SVG_PAINT_SHAPE		1
#define XGE_SVG_PAINT_TEXT		2
#define XGE_SVG_PAINT_SVG_IMAGE		3
#define XGE_SVG_PAINT_RASTER_IMAGE	4
#define XGE_SVG_PAINT_SCENE		5
#define XGE_SVG_PAINT_FILTER		6
#define XGE_SVG_PAINT_GROUP		7
#define XGE_SVG_PAINT_PICTURE		8

#define XGE_TEXT_ALIGN_LEFT		0x0000
#define XGE_TEXT_ALIGN_CENTER	0x0001
#define XGE_TEXT_ALIGN_RIGHT	0x0002
#define XGE_TEXT_ALIGN_TOP		0x0000
#define XGE_TEXT_ALIGN_MIDDLE	0x0010
#define XGE_TEXT_ALIGN_BOTTOM	0x0020
#define XGE_TEXT_CLIP			0x0100
#define XGE_TEXT_UNDERLINE		0x0200
#define XGE_TEXT_SCREEN_SPACE	0x0400

#define XGE_FONT_WEIGHT_THIN		100
#define XGE_FONT_WEIGHT_EXTRA_LIGHT	200
#define XGE_FONT_WEIGHT_LIGHT		300
#define XGE_FONT_WEIGHT_NORMAL		400
#define XGE_FONT_WEIGHT_MEDIUM		500
#define XGE_FONT_WEIGHT_SEMI_BOLD	600
#define XGE_FONT_WEIGHT_BOLD		700
#define XGE_FONT_WEIGHT_EXTRA_BOLD	800
#define XGE_FONT_WEIGHT_BLACK		900

#define XGE_FONT_SLANT_NORMAL	0
#define XGE_FONT_SLANT_ITALIC	1
#define XGE_FONT_SLANT_OBLIQUE	2

#define XGE_FONT_STRETCH_NORMAL	100

#define XGE_TEXT_SHAPE_KERNING		0x0001
#define XGE_TEXT_SHAPE_DEFAULT		XGE_TEXT_SHAPE_KERNING

#define XGE_GLYPH_POSITION_LINE_BREAK	0x0001

#define XGE_TEXT_DECORATION_UNDERLINE	1
#define XGE_TEXT_DECORATION_OVERLINE	2
#define XGE_TEXT_DECORATION_STRIKE	3
#define XGE_TEXT_DECORATION_SQUIGGLE	4
#define XGE_TEXT_DECORATION_DOTTED	5
#define XGE_TEXT_DECORATION_DASHED	6
#define XGE_TEXT_DECORATION_USE_FONT_METRICS	0x0001
#define XGE_TEXT_DECORATION_SCREEN_SPACE		0x0002
#define XGE_TEXT_DECORATION_RANGE			0x0004

#define XGE_XRF_MAGIC			0x32465258u
#define XGE_XRF_VERSION_LEGACY	1
#define XGE_XRF_VERSION			2
#define XGE_XRF_PAGE_A8			1
#define XGE_XRF_PAGE_RGBA8		2
#define XGE_XRF_FLAG_KERNING	0x0001
#define XGE_XRF_FLAG_SOURCE_INFO	0x0002

#define XGE_AUDIO_SOUND		1
#define XGE_AUDIO_MUSIC		2
#define XGE_AUDIO_STREAM	3

#define XGE_ASYNC_PENDING		0
#define XGE_ASYNC_LOADING		1
#define XGE_ASYNC_READY		2
#define XGE_ASYNC_FAILED		3
#define XGE_ASYNC_CANCELLED	4

#define XGE_ASYNC_IMAGE		1
#define XGE_ASYNC_TEXTURE		2
#define XGE_ASYNC_FONT			3
#define XGE_ASYNC_SOUND		4

#define XGE_EVENT_NONE			0
#define XGE_EVENT_KEY_DOWN		1
#define XGE_EVENT_KEY_UP		2
#define XGE_EVENT_TEXT			3
#define XGE_EVENT_MOUSE_DOWN	4
#define XGE_EVENT_MOUSE_UP		5
#define XGE_EVENT_MOUSE_MOVE	6
#define XGE_EVENT_MOUSE_WHEEL	7
#define XGE_EVENT_TOUCH_BEGIN	8
#define XGE_EVENT_TOUCH_MOVE	9
#define XGE_EVENT_TOUCH_END		10
#define XGE_EVENT_TOUCH_CANCEL	11
#define XGE_EVENT_RESIZE		12
#define XGE_EVENT_QUIT			13
#define XGE_EVENT_IME_START		14
#define XGE_EVENT_IME_UPDATE	15
#define XGE_EVENT_IME_END		16
#define XGE_EVENT_IME_COMMIT	17
#define XGE_EVENT_GAMEPAD_CONNECTED	19
#define XGE_EVENT_GAMEPAD_DISCONNECTED	20



#define XGE_KEY_MOD_SHIFT	0x0001
#define XGE_KEY_MOD_CTRL	0x0002
#define XGE_KEY_MOD_ALT		0x0004
#define XGE_KEY_MOD_SUPER	0x0008

#define XGE_GAMEPAD_A				0x00000001u
#define XGE_GAMEPAD_B				0x00000002u
#define XGE_GAMEPAD_X				0x00000004u
#define XGE_GAMEPAD_Y				0x00000008u
#define XGE_GAMEPAD_LEFT_SHOULDER	0x00000010u
#define XGE_GAMEPAD_RIGHT_SHOULDER	0x00000020u
#define XGE_GAMEPAD_BACK			0x00000040u
#define XGE_GAMEPAD_START			0x00000080u
#define XGE_GAMEPAD_LEFT_STICK		0x00000100u
#define XGE_GAMEPAD_RIGHT_STICK		0x00000200u
#define XGE_GAMEPAD_DPAD_UP		0x00000400u
#define XGE_GAMEPAD_DPAD_DOWN		0x00000800u
#define XGE_GAMEPAD_DPAD_LEFT		0x00001000u
#define XGE_GAMEPAD_DPAD_RIGHT		0x00002000u
#define XGE_GAMEPAD_GUIDE			0x00004000u


























#define XGE_TOUCH_NONE			0
#define XGE_TOUCH_BEGIN			1
#define XGE_TOUCH_MOVE			2
#define XGE_TOUCH_STATIONARY	3
#define XGE_TOUCH_END			4
#define XGE_TOUCH_CANCEL		5

#define XGE_COLOR_RGBA(r, g, b, a)	((((uint32_t)(r) & 0xFFu) << 24) | (((uint32_t)(g) & 0xFFu) << 16) | (((uint32_t)(b) & 0xFFu) << 8) | ((uint32_t)(a) & 0xFFu))
#define XGE_COLOR_GET_R(c)			(((uint32_t)(c) >> 24) & 0xFFu)
#define XGE_COLOR_GET_G(c)			(((uint32_t)(c) >> 16) & 0xFFu)
#define XGE_COLOR_GET_B(c)			(((uint32_t)(c) >> 8) & 0xFFu)
#define XGE_COLOR_GET_A(c)			((uint32_t)(c) & 0xFFu)

typedef enum xge_result_t {
	XGE_OK = 0,
	XGE_ERROR = -1,
	XGE_ERROR_INVALID_ARGUMENT = -2,
	XGE_ERROR_NOT_INITIALIZED = -3,
	XGE_ERROR_ALREADY_INITIALIZED = -4,
	XGE_ERROR_OUT_OF_MEMORY = -5,
	XGE_ERROR_FILE_NOT_FOUND = -6,
	XGE_ERROR_UNSUPPORTED = -7,
	XGE_ERROR_BACKEND_FAILED = -8,
	XGE_ERROR_GPU_FAILED = -9,
	XGE_ERROR_RESOURCE_FAILED = -10,
	XGE_ERROR_AUDIO_FAILED = -11,
	XGE_ERROR_THREAD_FAILED = -12,
	XGE_ERROR_BUFFER_TOO_SMALL = -13,
	XGE_ERROR_NOT_FOUND = -14,
	XGE_ERROR_INVALID_STATE = -15
} xge_result_t;

typedef struct xge_desc_t {
	int iWidth;
	int iHeight;
	const char* sTitle;
	int iFlags;
	int iRunMode;
	int iTargetFPS;
	void* pNativeWindow;
	void* pUser;
} xge_desc_t;

typedef struct xge_rect_t {
	float fX;
	float fY;
	float fW;
	float fH;
} xge_rect_t;

typedef struct xge_vec2_t {
	float fX;
	float fY;
} xge_vec2_t;

typedef struct xge_vec3_t {
	float fX;
	float fY;
	float fZ;
} xge_vec3_t;

typedef struct xge_mat3_t {
	float m[9];
} xge_mat3_t;

typedef struct xge_mat4_t {
	float m[16];
} xge_mat4_t;

typedef struct xge_color_t {
	float fR;
	float fG;
	float fB;
	float fA;
} xge_color_t;

typedef struct xge_frame_stats_t {
	int iFrameCount;
	int iDrawCallCount;
	int iBatchCount;
	int iDirtyRectCount;
	float fFrameTimeMs;
	float fFrameTimeAvgMs;
	float fFrameTimeMaxMs;
} xge_frame_stats_t;

#if XGE_HAS_DEBUGMODE
typedef struct xge_debug_stats_t {
	xge_frame_stats_t tFrame;
	int iTextureCount;
	uint64_t iTextureMemoryBytes;
	int iFontCount;
	int iAudioCount;
	int iLastGLError;
} xge_debug_stats_t;
#endif

typedef struct xge_platform_backend_t {
	int iType;
	const char* sName;
	int (*init)(void* pUser);
	void (*unit)(void* pUser);
	int (*poll)(void* pUser);
	void (*present)(void* pUser);
	void* pUser;
} xge_platform_backend_t;

typedef struct xge_graphics_backend_t {
	int iType;
	const char* sName;
	int (*init)(void* pUser);
	void (*unit)(void* pUser);
	int (*begin)(void* pUser);
	int (*end)(void* pUser);
	void* pUser;
} xge_graphics_backend_t;

typedef struct xge_gpu_caps_t {
	int iBackend;
	int iMajorVersion;
	int iMinorVersion;
	int iMaxTextureSize;
	char sVendor[64];
	char sRenderer[128];
	char sVersion[64];
	char sShadingLanguage[64];
} xge_gpu_caps_t;

typedef struct xge_graphics_mapping_t {
	int iBackend;
	int bOpenGLCore;
	int bGLES;
	int bWebGL;
	int bVAO;
	int bFramebufferObject;
	int bDepthTexture;
	int bClientSideVertexArray;
	int iRGBA8InternalFormat;
	int iRGBAFormat;
	int iUnsignedByteType;
	char sShaderHeader[128];
} xge_graphics_mapping_t;

typedef struct xge_platform_caps_t {
	int iPlatformBackend;
	int iGraphicsBackend;
	char sPlatformName[64];
	char sGraphicsName[64];
	char sSokolTargetName[64];
	int bWindow;
	int bOffscreen;
	int bRenderTarget;
	int bPBuffer;
	int bSurfaceless;
	int bBoardLinux;
	int bMiniProgram;
	int bWASM;
	int bTouch;
	int bMouse;
	int bKeyboard;
	int bTextInput;
	int bGamepad;
	int bAudio;
	int bResourceProvider;
	int bThreadSafeSubmit;
	int bHighDPI;
	int bSokol;
	int bSokolWindows;
	int bSokolLinuxX11;
	int bSokolLinuxWayland;
	int bSokolMacOS;
	int bSokolAndroid;
	int bSokolIOS;
	int bSokolWeb;
	int bSokolGLCore;
	int bSokolGLES3;
	int bSokolD3D11;
	int bSokolMetal;
	int bSokolDummy;
} xge_platform_caps_t;

typedef struct xge_platform_runtime_t {
	int bRunning;
	int iWindowWidth;
	int iWindowHeight;
	int iFramebufferWidth;
	int iFramebufferHeight;
	float fDpiScale;
	int iKeyEventCount;
	int iTextEventCount;
	int iMouseEventCount;
	int iTouchEventCount;
	int iGamepadEventCount;
	int iResizeEventCount;
	int iQuitEventCount;
} xge_platform_runtime_t;

typedef struct xge_render_thread_caps_t {
	int bSupported;
	int bEnabled;
	int bWorkerDrain;
	int bGLContextOwned;
	int bAsyncFlush;
	int bCanUseWithCurrentContext;
} xge_render_thread_caps_t;

typedef struct xge_camera_t {
	xge_vec2_t tPosition;
	xge_vec2_t tScale;
	float fRotation;
	xge_rect_t tViewport;
	int iCoordinateMode;
	int iProjectionMode;
	float fFovY;
	float fNearZ;
	float fFarZ;
	float fPerspectiveDistance;
} xge_camera_t;

typedef struct xge_texture_t xge_texture_t;
typedef xge_texture_t* xge_texture;
typedef struct xge_render_target_t xge_render_target_t;
typedef xge_render_target_t* xge_render_target;
typedef struct xge_buffer_t xge_buffer_t;
typedef xge_buffer_t* xge_buffer;
typedef struct xge_pass_t xge_pass_t;
typedef xge_pass_t* xge_pass;
typedef struct xge_sprite_batch_t xge_sprite_batch_t;
typedef xge_sprite_batch_t* xge_sprite_batch;
typedef struct xge_shape_batch_t xge_shape_batch_t;
typedef xge_shape_batch_t* xge_shape_batch;
typedef struct xge_shape_ex_t xge_shape_ex_t;
typedef xge_shape_ex_t* xge_shape_ex;
typedef struct xge_shape_ex_path_measure_t xge_shape_ex_path_measure_t;
typedef xge_shape_ex_path_measure_t* xge_shape_ex_path_measure;
typedef struct xge_shape_ex_scene_t xge_shape_ex_scene_t;
typedef xge_shape_ex_scene_t* xge_shape_ex_scene;
typedef struct xge_svg_t xge_svg_t;
typedef xge_svg_t* xge_svg;
/* Borrowed SVG paint handles remain valid until the owning SVG is cleared, reloaded, or destroyed. */
typedef struct xge_svg_draw_item_t* xge_svg_paint;
typedef int (*xge_svg_paint_visit_proc)(xge_svg_paint pPaint, void* pUser);
typedef struct xge_shader_t xge_shader_t;
typedef xge_shader_t* xge_shader;
typedef struct xge_material_t xge_material_t;
typedef xge_material_t* xge_material;
typedef struct xge_shader_variant_t xge_shader_variant_t;
typedef xge_shader_variant_t* xge_shader_variant;
typedef struct xge_shader_variant_set_t xge_shader_variant_set_t;
typedef xge_shader_variant_set_t* xge_shader_variant_set;
typedef struct xge_mesh_t xge_mesh_t;
typedef xge_mesh_t* xge_mesh;
typedef struct xge_nine_patch_t xge_nine_patch_t;
typedef xge_nine_patch_t* xge_nine_patch;

typedef struct xge_draw_t {
	xge_texture pTexture;
	xge_rect_t tSrc;
	xge_rect_t tDst;
	xge_vec2_t tOrigin;
	float fRotation;
	uint32_t iColor;
	uint32_t iFlags;
} xge_draw_t;

struct xge_nine_patch_t {
	xge_texture pTexture;
	xge_rect_t tSrc;
	float fX1;
	float fY1;
	float fX2;
	float fY2;
	uint32_t iColor;
	int iMode;
	int bEasyMode;
};

typedef struct xge_vertex_t {
	float fX;
	float fY;
	float fZ;
	float fW;
	float fU;
	float fV;
	uint32_t iColor;
} xge_vertex_t;

typedef struct xge_shape_vertex_t {
	float fX;
	float fY;
	uint32_t iColor;
} xge_shape_vertex_t;

typedef struct xge_shape_round_rect_t {
	float fTopLeft;
	float fTopRight;
	float fBottomRight;
	float fBottomLeft;
} xge_shape_round_rect_t;

typedef struct xge_shape_ex_matrix_t {
	float fA;
	float fB;
	float fC;
	float fD;
	float fE;
	float fF;
} xge_shape_ex_matrix_t;

typedef struct xge_shape_ex_command_t {
	uint8_t iCommand;
} xge_shape_ex_command_t;

typedef struct xge_shape_ex_color_stop_t {
	float fOffset;
	uint32_t iColor;
} xge_shape_ex_color_stop_t;

typedef struct xge_shape_ex_scene_effect_t {
	int iType;
	uint32_t iColor;
	uint32_t iColor2;
	uint32_t iColor3;
	float fSigma;
	float fAngleDegrees;
	float fDistance;
	float fIntensity;
	int iDirection;
	int iBorder;
	int iQuality;
	int iBlend;
} xge_shape_ex_scene_effect_t;

typedef struct xge_shape_ex_scene_child_t {
	int iType;
	xge_shape_ex pShape;
	xge_shape_ex_scene pScene;
} xge_shape_ex_scene_child_t;

typedef int (*xge_shape_ex_scene_visit_proc)(const xge_shape_ex_scene_child_t* pPaint, void* pUser);

typedef int (*xge_shape_ex_draw_proc)(void* pUser, const xge_shape_ex_matrix_t* pParentMatrix);

typedef struct xge_sampler_t {
	int iMinFilter;
	int iMagFilter;
	int iWrapS;
	int iWrapT;
} xge_sampler_t;

struct xge_sprite_batch_t {
	xge_texture pTexture;
	int iCapacity;
	int iCount;
	uint32_t iFlags;
	void* pVertices;
};

typedef struct xge_pipeline_state_t {
	int iBlend;
	int bDepthTest;
	uint32_t iFlags;
} xge_pipeline_state_t;

struct xge_shader_t {
	int iRefCount;
	uint32_t iProgram;
	int iLocResolution;
	int iLocTexture;
	int iLocTexture2;
	int iLocTexture3;
	int iLocColor;
	void* pBackend;
};

typedef struct xge_shader_define_t {
	char sName[XGE_SHADER_DEFINE_NAME_MAX];
	int iValue;
} xge_shader_define_t;

struct xge_shader_variant_t {
	uint32_t iKey;
	xge_shader_t tShader;
	int iDefineCount;
	xge_shader_define_t arrDefines[XGE_SHADER_DEFINE_MAX];
	struct xge_shader_variant_t* pNext;
};

struct xge_shader_variant_set_t {
	char* sVertexSource;
	char* sFragmentSource;
	xge_shader_variant pVariants;
	int iCount;
};

struct xge_buffer_t {
	int iType;
	int iUsage;
	int iSize;
	uint32_t iBackendId;
	void* pData;
};

struct xge_material_t {
	xge_shader pShader;
	xge_texture pTexture;
	xge_texture pTexture2;
	xge_texture pTexture3;
	uint32_t iColor;
	xge_pipeline_state_t tPipeline;
};

struct xge_mesh_t {
	int iVertexCount;
	int iIndexCount;
	uint32_t iFlags;
	uint32_t iVertexBufferId;
	uint32_t iIndexBufferId;
	void* pVertices;
	void* pIndices;
};

struct xge_shape_batch_t {
	uint32_t iColor;
	uint32_t iFlags;
	int iTriangleCount;
	int iTriangleCapacity;
	void* pVertices;
};

struct xge_texture_t {
	int iWidth;
	int iHeight;
	int iFormat;
	int iRefCount;
	uint32_t iFlags;
	uint32_t iBackendId;
	xge_sampler_t tSampler;
	void* pBackend;
};

struct xge_render_target_t {
	int iWidth;
	int iHeight;
	uint32_t iFlags;
	uint32_t iFramebufferId;
	uint32_t iStencilRenderbufferId;
	xge_texture_t tTexture;
};

struct xge_pass_t {
	xge_render_target pTarget;
	uint32_t iClearFlags;
	uint32_t iClearColor;
	int iPrevWidth;
	int iPrevHeight;
	uint32_t iPrevFramebufferId;
	xge_camera_t tPrevCamera;
	int bPrevViewportEnabled;
	xge_rect_t tPrevViewportRect;
	int bActive;
};

typedef struct xge_image_t {
	int iWidth;
	int iHeight;
	int iFormat;
	int iStride;
	void* pPixels;
	uint32_t iFlags;
} xge_image_t, *xge_image;

typedef struct xge_resource_t {
	void* pData;
	int iSize;
	int iProvider;
	void (*free)(void* pData, void* pUser);
	void* pUser;
} xge_resource_t;

typedef struct xge_resource_provider_t {
	const char* sScheme;
	int (*load)(const char* sURI, void** ppData, int* pSize, void* pUser);
	void (*free)(void* pData, void* pUser);
	void* pUser;
} xge_resource_provider_t;

typedef void* (*xge_xpack_read_proc)(void* pPack, const char* sPath, uint64_t* pSize, void* pUser);
typedef void (*xge_xpack_free_proc)(void* pData, void* pUser);

typedef struct xge_xpack_provider_t {
	void* pPack;
	const char* sRoot;
	xge_xpack_read_proc read;
	xge_xpack_free_proc free;
	void* pUser;
} xge_xpack_provider_t;

typedef struct xge_miniprogram_desc_t {
	void* pCanvas;
	void* pWebGLContext;
	int iWidth;
	int iHeight;
	float fDevicePixelRatio;
	void* pUser;
} xge_miniprogram_desc_t;

typedef struct xge_miniprogram_touch_t {
	uint64_t iId;
	float fX;
	float fY;
	float fForce;
} xge_miniprogram_touch_t;

typedef struct xge_miniprogram_bridge_t {
	int (*request_frame)(void* pUser);
	int (*load_resource)(const char* sURI, void** ppData, int* pSize, void* pUser);
	void (*free_resource)(void* pData, void* pUser);
	int (*audio_command)(int iCommand, int iHandle, const void* pData, int iSize, void* pUser);
	void* pUser;
} xge_miniprogram_bridge_t;

typedef struct xge_egl_desc_t {
	void* pNativeDisplay;
	void* pNativeWindow;
	int iWidth;
	int iHeight;
	int bPBuffer;
	int bSurfaceless;
	int bBoardLinux;
	void* pUser;
} xge_egl_desc_t;

typedef struct xge_egl_caps_t {
	int bCompiled;
	int bOffscreen;
	int bPBuffer;
	int bSurfaceless;
	int bNativeWindow;
	int bBoardLinux;
	int iLastError;
	char sBackendName[64];
	char sLastStage[64];
} xge_egl_caps_t;

typedef struct xge_egl_context_t {
	int bInitialized;
	int bPBuffer;
	int bSurfaceless;
	int bBoardLinux;
	int iWidth;
	int iHeight;
	void* pDisplay;
	void* pConfig;
	void* pSurface;
	void* pContext;
	void* pUser;
	int iLastError;
	char sLastStage[64];
} xge_egl_context_t;

typedef struct xge_offscreen_t {
	int iWidth;
	int iHeight;
	xge_egl_context_t tEGL;
	xge_render_target_t tTarget;
	int bActive;
	int bEGLContext;
	int bFallbackRenderTarget;
} xge_offscreen_t, *xge_offscreen;

typedef struct xge_async_request_t xge_async_request_t;
typedef xge_async_request_t* xge_async_request;
typedef void (*xge_async_proc)(xge_async_request pRequest, void* pUser);

typedef struct xge_sound_t {
	int iRefCount;
	int iType;
	uint32_t iFlags;
	void* pBackend;
} xge_sound_t, *xge_sound;

typedef struct xge_music_t {
	int iRefCount;
	int iType;
	uint32_t iFlags;
	void* pBackend;
} xge_music_t, *xge_music;

typedef struct xge_stream_t {
	int iRefCount;
	int iType;
	uint32_t iFlags;
	void* pBackend;
} xge_stream_t, *xge_stream;

typedef struct xge_audio_group_t {
	float fVolume;
	void* pBackend;
} xge_audio_group_t, *xge_audio_group;

typedef struct xge_audio_listener_t {
	xge_vec3_t tPosition;
	xge_vec3_t tForward;
	xge_vec3_t tUp;
} xge_audio_listener_t;

struct xge_async_request_t {
	int iType;
	int iStatus;
	int iResult;
	float fProgress;
	void* pTarget;
	char* sURI;
	xge_async_proc onComplete;
	void* pUser;
	void* pThread;
	uint32_t iFlags;
	float fSize;
	int bCancel;
	int bThreaded;
	int bCallbackPending;
};

typedef struct xge_font_t xge_font_t;
typedef xge_font_t* xge_font;
typedef struct xge_font_face_t* xge_font_face;
typedef struct xge_font_family_t* xge_font_family;

typedef struct xge_font_face_desc_t {
	uint32_t iSize;
	int iFaceIndex;
	uint32_t iFlags;
} xge_font_face_desc_t;

typedef struct xge_font_instance_desc_t {
	uint32_t iSize;
	float fPixelSize;
	uint32_t iFlags;
} xge_font_instance_desc_t;

typedef struct xge_font_face_info_t {
	uint32_t iSize;
	int iFaceIndex;
	int iWeight;
	int iStretch;
	int iSlant;
	uint64_t iSourceHash;
	uint32_t iFlags;
} xge_font_face_info_t;

typedef struct xge_font_metrics_t {
	uint32_t iSize;
	float fPixelSize;
	float fAscent;
	float fDescent;
	float fLineGap;
	float fLineHeight;
	float fUnderlinePosition;
	float fUnderlineThickness;
	float fStrikePosition;
	float fStrikeThickness;
} xge_font_metrics_t;

typedef struct xge_font_cache_stats_t {
	uint32_t iSize;
	uint32_t iGlyphCount;
	uint32_t iCodepointCount;
	uint32_t iBucketCount;
	uint32_t iAtlasPageCount;
	uint64_t iAtlasCpuBytes;
	uint64_t iAtlasGpuBytes;
} xge_font_cache_stats_t;

typedef struct xge_glyph_metrics_t {
	uint32_t iCodepoint;
	int iGlyph;
	float fAdvanceX;
	float fLeftSideBearing;
	float fX0;
	float fY0;
	float fX1;
	float fY1;
} xge_glyph_metrics_t;

typedef struct xge_glyph_bitmap_t {
	uint32_t iCodepoint;
	int iWidth;
	int iHeight;
	int iStride;
	int iOffsetX;
	int iOffsetY;
	int iFormat;
	void* pPixels;
} xge_glyph_bitmap_t;

typedef struct xge_glyph_t {
	uint32_t iCodepoint;
	int iGlyph;
	int iPage;
	int iX;
	int iY;
	int iWidth;
	int iHeight;
	float fOffsetX;
	float fOffsetY;
	float fAdvanceX;
} xge_glyph_t;

typedef struct xge_glyph_atlas_t {
	int iPageWidth;
	int iPageHeight;
	int iPageCount;
	int iFormat;
	void* pPages;
} xge_glyph_atlas_t;

struct xge_font_t {
	int iRefCount;
	float fSize;
	float fScale;
	float fAscent;
	float fDescent;
	float fLineGap;
	float fLineHeight;
	uint32_t iFlags;
	void* pData;
	int iDataSize;
	void* pBackend;
	xge_font pFallback;
	xge_glyph_atlas_t tAtlas;
	void* pGlyphs;
};

typedef struct xge_xrf_header_t {
	uint32_t iMagic;
	uint16_t iVersion;
	uint16_t iHeaderSize;
	uint32_t iFlags;
	uint32_t iGlyphCount;
	uint32_t iPageCount;
	uint32_t iRangeCount;
	uint32_t iKerningCount;
	float fAscent;
	float fDescent;
	float fLineGap;
	float fLineHeight;
	uint32_t iGlyphOffset;
	uint32_t iPageOffset;
	uint32_t iRangeOffset;
	uint32_t iKerningOffset;
	uint32_t iPixelOffset;
} xge_xrf_header_t;

typedef struct xge_xrf_range_t {
	uint32_t iFirstCodepoint;
	uint32_t iCount;
} xge_xrf_range_t;

typedef struct xge_xrf_glyph_t {
	uint32_t iCodepoint;
	uint16_t iPage;
	uint16_t iGlyph;
	uint16_t iX;
	uint16_t iY;
	uint16_t iWidth;
	uint16_t iHeight;
	float fOffsetX;
	float fOffsetY;
	float fAdvanceX;
} xge_xrf_glyph_t;

typedef struct xge_xrf_page_t {
	uint16_t iWidth;
	uint16_t iHeight;
	uint16_t iFormat;
	uint16_t iReserved;
	uint32_t iPixelOffset;
	uint32_t iPixelSize;
} xge_xrf_page_t;

typedef struct xge_xrf_kerning_t {
	uint32_t iLeftCodepoint;
	uint32_t iRightCodepoint;
	float fAdvanceX;
} xge_xrf_kerning_t;

typedef struct xge_xrf_header_v2_t {
	xge_xrf_header_t tBase;
	uint64_t iSourceHash;
	float fPixelSize;
	int32_t iFaceIndex;
	int32_t iWeight;
	int32_t iStretch;
	int32_t iSlant;
	float fUnderlinePosition;
	float fUnderlineThickness;
	float fStrikePosition;
	float fStrikeThickness;
} xge_xrf_header_v2_t;

typedef struct xge_glyph_position_t {
	uint32_t iCodepoint;
	uint32_t iCluster;
	int iGlyph;
	uint32_t iFlags;
	xge_font pFont;
	float fAdvanceX;
	float fOffsetX;
	float fOffsetY;
} xge_glyph_position_t;

typedef struct xge_glyph_run_t {
	uint32_t iSize;
	uint32_t iFlags;
	int iTextSize;
	int iGlyphCount;
	xge_glyph_position_t* pGlyphs;
	float fWidth;
	float fHeight;
	float fAscent;
	float fDescent;
	float fLineHeight;
	void* pBackend;
} xge_glyph_run_t;

typedef struct xge_text_shape_desc_t {
	uint32_t iSize;
	xge_font pFont;
	const char* sText;
	int iTextSize;
	uint32_t iFlags;
} xge_text_shape_desc_t;

typedef struct xge_text_decoration_t {
	uint32_t iSize;
	int iType;
	uint32_t iColor;
	uint32_t iFlags;
	int iStart;
	int iEnd;
	float fThickness;
	float fOffset;
	float fAmplitude;
	float fWavelength;
	float fPhase;
} xge_text_decoration_t;


typedef struct xge_event_t {
	int iType;
	int iParam1;
	int iParam2;
	float fX;
	float fY;
	float fDX;
	float fDY;
	uint64_t iPointerId;
	uint32_t iCodepoint;
	double fTime;
	void* pData;
} xge_event_t;

typedef struct xge_scene_t xge_scene_t;
typedef xge_scene_t* xge_scene;
typedef int (*xge_scene_lifecycle_proc)(xge_scene pScene);
typedef int (*xge_scene_event_proc)(xge_scene pScene, const xge_event_t* pEvent);
typedef int (*xge_scene_update_proc)(xge_scene pScene, float fDelta);
typedef int (*xge_scene_draw_proc)(xge_scene pScene);

struct xge_scene_t {
	void* pUser;
	xge_scene_lifecycle_proc onEnter;
	xge_scene_lifecycle_proc onLeave;
	xge_scene_lifecycle_proc onPause;
	xge_scene_lifecycle_proc onResume;
	xge_scene_event_proc onEvent;
	xge_scene_update_proc onUpdate;
	xge_scene_draw_proc onDraw;
	xge_scene_lifecycle_proc onFree;
	int bEntered;
	int bPaused;
};

typedef struct xge_ime_event_t {
	uint32_t iSize;
	int iType;
	const char* sText;
	int iTextSize;
	int iCursor;
	int iSelectStart;
	int iSelectEnd;
} xge_ime_event_t;

typedef struct xge_touch_point_t {
	uint64_t iId;
	float fX;
	float fY;
	float fDX;
	float fDY;
	int iPhase;
	int bChanged;
	int bDown;
} xge_touch_point_t;

typedef struct xge_touch_event_t {
	int iPhase;
	int iCount;
	xge_touch_point_t arrPoints[XGE_TOUCH_MAX];
} xge_touch_event_t;

typedef struct xge_gamepad_state_t {
	int bConnected;
	uint32_t iButtons;
	uint32_t iButtonsPressed;
	uint32_t iButtonsReleased;
	float arrAxes[XGE_GAMEPAD_AXIS_COUNT];
} xge_gamepad_state_t;

typedef int (*xge_scene_proc)(void* pUser);

XGE_API int xgeInit(const xge_desc_t* pDesc);
XGE_API void xgeUnit(void);
XGE_API void xgeMemoryFree(void* pData);
XGE_API int xgeRun(xge_scene_proc procFrame, void* pUser);
XGE_API void xgeQuit(void);
XGE_API void xgeRenderRequest(void);
XGE_API int xgeFrame(void);
XGE_API int xgeRender(void);
XGE_API void xgeFrameStatsReset(void);
XGE_API xge_frame_stats_t xgeFrameStatsGet(void);
#if XGE_HAS_DEBUGMODE
/* Debug APIs are only declared for xgedbg builds.
 * Build with -DXGE_DEBUGMODE=1 and link xgedbg instead of xge. */
XGE_API int xgeDebugGetStats(xge_debug_stats_t* pStats);
XGE_API int xgeDebugDumpCaps(char* sBuffer, int iSize);
#endif
XGE_API int xgeLogSetLevel(int iLevel);
XGE_API int xgeLogGetLevel(void);
XGE_API int xgeLogWrite(int iLevel, const char* sTag, const char* sMessage);
XGE_API int xgeLogFlush(void);
XGE_API xge_platform_backend_t xgePlatformBackendDefault(void);
XGE_API int xgePlatformBackendSet(const xge_platform_backend_t* pBackend);
XGE_API xge_platform_backend_t xgePlatformBackendGet(void);
XGE_API xge_graphics_backend_t xgeGraphicsBackendDefault(void);
XGE_API int xgeGraphicsBackendSet(const xge_graphics_backend_t* pBackend);
XGE_API xge_graphics_backend_t xgeGraphicsBackendGet(void);
XGE_API int xgeGpuCapsGet(xge_gpu_caps_t* pCaps);
XGE_API int xgePlatformCapsGet(xge_platform_caps_t* pCaps);
XGE_API int xgePlatformRuntimeGet(xge_platform_runtime_t* pRuntime);
XGE_API int xgeGraphicsShaderHeaderGet(int iBackend, char* sBuffer, int iSize);
XGE_API int xgeGraphicsLibraryNameGet(int iBackend, int iIndex, char* sBuffer, int iSize);
XGE_API int xgeGraphicsMappingGet(int iBackend, xge_graphics_mapping_t* pMapping);
XGE_API int xgeSceneSet(xge_scene pScene);
XGE_API int xgeScenePush(xge_scene pScene);
XGE_API int xgeScenePop(void);
XGE_API int xgeSceneReplace(xge_scene pScene);
XGE_API xge_scene xgeSceneCurrent(void);
XGE_API int xgeSceneCount(void);
XGE_API int xgeSceneDispatchEvent(const xge_event_t* pEvent);
XGE_API int xgeSceneUpdateStrategySet(int iMode, float fFixedStep, int iMaxUpdates);
XGE_API void xgeSceneUpdateStrategyGet(int* pMode, float* pFixedStep, int* pMaxUpdates);

XGE_API int xgeGetWidth(void);
XGE_API int xgeGetHeight(void);
XGE_API float xgeGetDelta(void);
XGE_API int xgeGetFPS(void);
XGE_API double xgeTimer(void);
XGE_API int64_t xgeTimeNow(void);
XGE_API void xgeSleep(uint32_t iMilliseconds);
XGE_API void xgeSetTitle(const char* sTitle);

XGE_API int xgeBegin(void);
XGE_API int xgeEnd(void);
XGE_API int xgeFlush(void);
XGE_API int xgeRenderThreadCapsGet(xge_render_thread_caps_t* pCaps);
XGE_API int xgeRenderThreadEGLSet(const xge_egl_desc_t* pDesc);
XGE_API int xgeRenderThreadSet(int bEnabled);
XGE_API int xgeRenderThreadGet(void);
XGE_API void xgeClear(uint32_t iColor);
XGE_API void xgePresent(void);
XGE_API void xgeInvalidateRect(xge_rect_t tRect);
#if XGE_HAS_DEBUGMODE
/* Dirty rect inspection is a debug feature; xge keeps only the runtime
 * invalidation mechanism. Build with -DXGE_DEBUGMODE=1 and link xgedbg. */
XGE_API int xgedbgDirtyRectCount(void);
XGE_API int xgedbgDirtyRectGet(int iIndex, xge_rect_t* pRect);
XGE_API void xgedbgDirtyRectClear(void);
#endif
XGE_API uint32_t xgeColorRGBA(int iR, int iG, int iB, int iA);
XGE_API xge_color_t xgeColorUnpack(uint32_t iColor);
XGE_API void xgeBlendSet(int iBlend);
XGE_API int xgeBlendGet(void);
XGE_API void xgeDepthTestSet(int bEnabled);
XGE_API int xgeDepthTestGet(void);
XGE_API xge_camera_t xgeCameraDefault(float fWidth, float fHeight);
XGE_API xge_camera_t xgeCameraPerspective(float fWidth, float fHeight, float fFovY, float fNearZ, float fFarZ);
XGE_API void xgeCameraSet(const xge_camera_t* pCamera);
XGE_API xge_camera_t xgeCameraGet(void);
XGE_API xge_vec2_t xgeWorldToScreen(xge_vec2_t tPoint);
XGE_API xge_vec2_t xgeScreenToWorld(xge_vec2_t tPoint);
XGE_API int xgeResourceProviderAdd(const xge_resource_provider_t* pProvider);
XGE_API void xgeResourceProviderClear(void);
XGE_API int xgeResourceXPackProviderAdd(const xge_xpack_provider_t* pProvider);
XGE_API int xgeResourceLoad(const char* sURI, xge_resource_t* pResource);
XGE_API int xgeResourceLoadMemory(const void* pData, int iSize, xge_resource_t* pResource);
XGE_API void xgeResourceFree(xge_resource_t* pResource);
XGE_API int xgeMiniProgramInit(const xge_miniprogram_desc_t* pDesc);
XGE_API int xgeMiniProgramInitSimple(int iWidth, int iHeight, float fDevicePixelRatio);
XGE_API void xgeMiniProgramUnit(void);
XGE_API int xgeMiniProgramSetBridge(const xge_miniprogram_bridge_t* pBridge);
XGE_API int xgeMiniProgramFrame(double fTimeSeconds);
XGE_API int xgeMiniProgramResize(int iWidth, int iHeight, float fDevicePixelRatio);
XGE_API int xgeMiniProgramTouch(int iPhase, const xge_miniprogram_touch_t* pTouches, int iCount);
XGE_API int xgeMiniProgramTouchOne(int iPhase, int iId, float fX, float fY, float fForce);
XGE_API int xgeMiniProgramText(uint32_t iCodepoint);
XGE_API int xgeMiniProgramRequestFrame(void);
XGE_API int xgeMiniProgramAudioCommand(int iCommand, int iHandle, const void* pData, int iSize);
XGE_API int xgeEGLCapsGet(xge_egl_caps_t* pCaps);
XGE_API int xgeEGLInit(xge_egl_context_t* pContext, const xge_egl_desc_t* pDesc);
XGE_API void xgeEGLUnit(xge_egl_context_t* pContext);
XGE_API int xgeEGLMakeCurrent(xge_egl_context_t* pContext);
XGE_API int xgeOffscreenInit(xge_offscreen pOffscreen, int iWidth, int iHeight);
XGE_API void xgeOffscreenUnit(xge_offscreen pOffscreen);
XGE_API xge_render_target xgeOffscreenRenderTarget(xge_offscreen pOffscreen);
XGE_API int xgeOffscreenReadPixels(xge_offscreen pOffscreen, void* pPixels, int iStride);
XGE_API void xgeAsyncRequestInit(xge_async_request pRequest);
XGE_API void xgeAsyncRequestFree(xge_async_request pRequest);
XGE_API int xgeAsyncRequestCancel(xge_async_request pRequest);
XGE_API int xgeAsyncThreadingSet(int bEnabled);
XGE_API int xgeAsyncThreadingGet(void);
XGE_API int xgeAsyncPoll(xge_async_request pRequest);
XGE_API int xgeAsyncImageLoad(xge_async_request pRequest, xge_image pImage, const char* sPath, uint32_t iFlags, xge_async_proc onComplete, void* pUser);
XGE_API int xgeAsyncTextureLoad(xge_async_request pRequest, xge_texture pTexture, const char* sPath, uint32_t iFlags, xge_async_proc onComplete, void* pUser);
XGE_API int xgeAsyncFontLoad(xge_async_request pRequest, xge_font pFont, const char* sPath, float fSize, xge_async_proc onComplete, void* pUser);
XGE_API int xgeAsyncSoundLoad(xge_async_request pRequest, xge_sound pSound, const char* sPath, xge_async_proc onComplete, void* pUser);
XGE_API int xgeAudioInit(void);
XGE_API void xgeAudioUnit(void);
XGE_API int xgeAudioIsReady(void);
XGE_API void xgeAudioSetVolume(float fVolume);
XGE_API float xgeAudioGetVolume(void);
XGE_API void xgeAudioListenerSet(const xge_audio_listener_t* pListener);
XGE_API xge_audio_listener_t xgeAudioListenerGet(void);
XGE_API int xgeAudioGroupInit(xge_audio_group pGroup);
XGE_API void xgeAudioGroupFree(xge_audio_group pGroup);
XGE_API void xgeAudioGroupSetVolume(xge_audio_group pGroup, float fVolume);
XGE_API float xgeAudioGroupGetVolume(xge_audio_group pGroup);
XGE_API void xgeAudioGroupFade(xge_audio_group pGroup, float fFrom, float fTo, int iMilliseconds);
XGE_API int xgeSoundLoad(xge_sound pSound, const char* sPath);
XGE_API int xgeSoundLoadGroup(xge_sound pSound, const char* sPath, xge_audio_group pGroup);
XGE_API int xgeSoundFallbackSet(const char* sPath);
XGE_API int xgeSoundFallbackGet(xge_sound pSound);
XGE_API void xgeSoundFallbackClear(void);
XGE_API int xgeSoundPlay(xge_sound pSound);
XGE_API int xgeSoundStop(xge_sound pSound);
XGE_API int xgeSoundPause(xge_sound pSound);
XGE_API int xgeSoundResume(xge_sound pSound);
XGE_API int xgeSoundAddRef(xge_sound pSound);
XGE_API void xgeSoundFree(xge_sound pSound);
XGE_API void xgeSoundSetLoop(xge_sound pSound, int bLoop);
XGE_API void xgeSoundSetVolume(xge_sound pSound, float fVolume);
XGE_API void xgeSoundSetPosition(xge_sound pSound, float fX, float fY, float fZ);
XGE_API void xgeSoundFade(xge_sound pSound, float fFrom, float fTo, int iMilliseconds);
XGE_API int xgeSoundIsPlaying(xge_sound pSound);
XGE_API int xgeMusicLoad(xge_music pMusic, const char* sPath);
XGE_API int xgeMusicLoadGroup(xge_music pMusic, const char* sPath, xge_audio_group pGroup);
XGE_API int xgeMusicPlay(xge_music pMusic);
XGE_API int xgeMusicStop(xge_music pMusic);
XGE_API int xgeMusicPause(xge_music pMusic);
XGE_API int xgeMusicResume(xge_music pMusic);
XGE_API void xgeMusicFree(xge_music pMusic);
XGE_API void xgeMusicSetLoop(xge_music pMusic, int bLoop);
XGE_API void xgeMusicSetVolume(xge_music pMusic, float fVolume);
XGE_API void xgeMusicFade(xge_music pMusic, float fFrom, float fTo, int iMilliseconds);
XGE_API int xgeMusicIsPlaying(xge_music pMusic);
XGE_API int xgeStreamOpen(xge_stream pStream, const char* sPath);
XGE_API int xgeStreamOpenGroup(xge_stream pStream, const char* sPath, xge_audio_group pGroup);
XGE_API int xgeStreamPlay(xge_stream pStream);
XGE_API int xgeStreamStop(xge_stream pStream);
XGE_API int xgeStreamPause(xge_stream pStream);
XGE_API int xgeStreamResume(xge_stream pStream);
XGE_API void xgeStreamClose(xge_stream pStream);
XGE_API void xgeStreamSetLoop(xge_stream pStream, int bLoop);
XGE_API void xgeStreamSetVolume(xge_stream pStream, float fVolume);
XGE_API void xgeStreamSetPosition(xge_stream pStream, float fX, float fY, float fZ);
XGE_API void xgeStreamFade(xge_stream pStream, float fFrom, float fTo, int iMilliseconds);
XGE_API int xgeStreamIsPlaying(xge_stream pStream);
XGE_API int xgeTextUTF8Next(const char** psText, uint32_t* pCodepoint);
XGE_API int xgeFontFaceLoad(xge_font_face* ppFace, const char* sPath, const xge_font_face_desc_t* pDesc);
XGE_API int xgeFontFaceLoadMemory(xge_font_face* ppFace, const void* pData, int iSize, const xge_font_face_desc_t* pDesc);
XGE_API int xgeFontFaceAddRef(xge_font_face pFace);
XGE_API void xgeFontFaceFree(xge_font_face pFace);
XGE_API int xgeFontFaceGetInfo(xge_font_face pFace, xge_font_face_info_t* pInfo);
XGE_API int xgeFontFamilyCreate(xge_font_family* ppFamily);
XGE_API void xgeFontFamilyFree(xge_font_family pFamily);
XGE_API int xgeFontFamilyAddFace(xge_font_family pFamily, xge_font_face pFace);
XGE_API int xgeFontFamilyResolve(xge_font_family pFamily, int iWeight, int iSlant, xge_font_face* ppFace);
XGE_API int xgeFontFamilyResolveEx(xge_font_family pFamily, int iWeight, int iStretch, int iSlant, xge_font_face* ppFace);
XGE_API int xgeFontCreate(xge_font pFont, xge_font_face pFace, const xge_font_instance_desc_t* pDesc);
XGE_API int xgeFontLoad(xge_font pFont, const char* sPath, float fSize);
XGE_API int xgeFontLoadMemory(xge_font pFont, const void* pData, int iSize, float fSize);
XGE_API int xgeFontLoadXRF(xge_font pFont, const char* sPath);
XGE_API int xgeFontLoadXRFMemory(xge_font pFont, const void* pData, int iSize);
XGE_API int xgeFontBuildXRFMemory(xge_font pFont, uint32_t iFirstCodepoint, uint32_t iCount, void** ppData, int* pSize);
XGE_API int xgeFontSaveXRF(xge_font pFont, const char* sPath, uint32_t iFirstCodepoint, uint32_t iCount);
XGE_API int xgeFontLoadCached(xge_font pFont, const char* sTTFPath, const char* sXRFPath, float fSize, uint32_t iFirstCodepoint, uint32_t iCount);
XGE_API int xgeFontAddRef(xge_font pFont);
XGE_API void xgeFontFree(xge_font pFont);
XGE_API void xgeFontSetFallback(xge_font pFont, xge_font pFallback);
XGE_API int xgeFontFallbackSet(const char* sPath, float fSize);
XGE_API int xgeFontFallbackSetMemory(const void* pData, int iSize, float fSize);
XGE_API int xgeFontFallbackGet(xge_font pFont, float fSize);
XGE_API int xgeFontFallbackGetEx(xge_font pFont, float fSize, uint32_t iFlags);
XGE_API void xgeFontFallbackClear(void);
XGE_API int xgeFontGetMetrics(xge_font pFont, xge_font_metrics_t* pMetrics);
XGE_API int xgeFontCacheGetStats(xge_font pFont, xge_font_cache_stats_t* pStats);
XGE_API void xgeFontCacheClear(xge_font pFont);
XGE_API int xgeFontGlyphGet(xge_font pFont, uint32_t iCodepoint, xge_glyph_metrics_t* pMetrics);
XGE_API int xgeFontGlyphGetByIndex(xge_font pFont, int iGlyph, xge_glyph_metrics_t* pMetrics);
XGE_API int xgeFontGlyphRasterize(xge_font pFont, uint32_t iCodepoint, xge_glyph_bitmap_t* pBitmap);
XGE_API int xgeFontGlyphRasterizeByIndex(xge_font pFont, int iGlyph, xge_glyph_bitmap_t* pBitmap);
XGE_API int xgeFontGlyphAtlasGet(xge_font pFont, uint32_t iCodepoint, xge_glyph_t* pGlyph);
XGE_API int xgeFontGlyphAtlasGetByIndex(xge_font pFont, int iGlyph, xge_glyph_t* pGlyph);
XGE_API void xgeGlyphBitmapFree(xge_glyph_bitmap_t* pBitmap);
XGE_API int xgeTextShape(const xge_text_shape_desc_t* pDesc, xge_glyph_run_t* pRun);
XGE_API void xgeGlyphRunFree(xge_glyph_run_t* pRun);
XGE_API xge_vec2_t xgeGlyphRunMeasure(const xge_glyph_run_t* pRun);
XGE_API int xgeGlyphRunHitTest(const xge_glyph_run_t* pRun, float fX, float fY, uint32_t* pCluster, int* pTrailing);
XGE_API void xgeGlyphRunDraw(const xge_glyph_run_t* pRun, float fX, float fY, uint32_t iColor, uint32_t iFlags);
XGE_API void xgeGlyphRunDrawDecorated(const xge_glyph_run_t* pRun, float fX, float fY, uint32_t iColor, uint32_t iFlags, const xge_text_decoration_t* pDecorations, int iDecorationCount);
XGE_API int xgeFontGlyphOutlineAppendShapeEx(xge_font pFont, int iGlyph, xge_shape_ex pShape, float fPenX, float fBaselineY);
XGE_API int xgeGlyphRunAppendShapeEx(const xge_glyph_run_t* pRun, xge_shape_ex pShape, float fX, float fY);
XGE_API xge_vec2_t xgeTextMeasure(xge_font pFont, const char* sText);
XGE_API void xgeTextDraw(xge_font pFont, const char* sText, float fX, float fY, uint32_t iColor);
XGE_API void xgeTextDrawRect(xge_font pFont, const char* sText, xge_rect_t tRect, uint32_t iColor, uint32_t iFlags);
XGE_API int xgeImageLoad(xge_image pImage, const char* sPath);
XGE_API int xgeImageLoadEx(xge_image pImage, const char* sPath, uint32_t iFlags);
XGE_API int xgeImageLoadMemory(xge_image pImage, const void* pData, int iSize);
XGE_API int xgeImageLoadMemoryEx(xge_image pImage, const void* pData, int iSize, uint32_t iFlags);
XGE_API void* xgeImageGetPixels(xge_image pImage);
XGE_API void xgeImagePremultiply(xge_image pImage);
XGE_API int xgeImageSavePNG(const char* sPath, int iWidth, int iHeight, const void* pPixels, int iStride);
XGE_API void xgeImageFree(xge_image pImage);
XGE_API int xgeTextureCreateRGBA(xge_texture pTexture, int iWidth, int iHeight, const void* pPixels);
XGE_API int xgeTextureCreateYUV420P(xge_texture pTexture, int iWidth, int iHeight);
XGE_API int xgeTextureCreateFromImage(xge_texture pTexture, const xge_image_t* pImage);
XGE_API int xgeTextureLoad(xge_texture pTexture, const char* sPath);
XGE_API int xgeTextureLoadEx(xge_texture pTexture, const char* sPath, uint32_t iFlags);
XGE_API int xgeTextureLoadMemory(xge_texture pTexture, const void* pData, int iSize);
XGE_API int xgeTextureLoadMemoryEx(xge_texture pTexture, const void* pData, int iSize, uint32_t iFlags);
XGE_API int xgeTextureAddRef(xge_texture pTexture);
XGE_API int xgeTextureUpdateRGBA(xge_texture pTexture, int iX, int iY, int iWidth, int iHeight, const void* pPixels, int iStride);
XGE_API int xgeTextureUpdateYUV420P(xge_texture pTexture, const void* pY, int iStrideY, const void* pU, int iStrideU, const void* pV, int iStrideV);
XGE_API xge_sampler_t xgeSamplerDefault(void);
XGE_API int xgeTextureSetSampler(xge_texture pTexture, const xge_sampler_t* pSampler);
XGE_API xge_sampler_t xgeTextureGetSampler(xge_texture pTexture);
XGE_API int xgeTextureFallbackSetRGBA(int iWidth, int iHeight, const void* pPixels);
XGE_API int xgeTextureFallbackGet(xge_texture pTexture);
XGE_API void xgeTextureFallbackClear(void);
XGE_API int xgeTextureUploadQueue(xge_texture pTexture);
XGE_API int xgeTextureUploadFlush(void);
XGE_API int xgeTextureReadPixels(xge_texture pTexture, void* pPixels, int iStride);
XGE_API void xgeTextureFree(xge_texture pTexture);
XGE_API int xgeRenderTargetWindow(xge_render_target pTarget);
XGE_API int xgeRenderTargetCreate(xge_render_target pTarget, int iWidth, int iHeight);
XGE_API int xgeRenderTargetResize(xge_render_target pTarget, int iWidth, int iHeight);
XGE_API int xgeRenderTargetCaptureCurrent(xge_render_target pTarget, int iSrcX, int iSrcY);
XGE_API int xgeRenderTargetReadPixels(xge_render_target pTarget, void* pPixels, int iStride);
XGE_API xge_texture xgeRenderTargetTexture(xge_render_target pTarget);
XGE_API void xgeRenderTargetFree(xge_render_target pTarget);
XGE_API int xgeBufferCreate(xge_buffer pBuffer, int iType, int iUsage, const void* pData, int iSize);
XGE_API int xgeBufferUpdate(xge_buffer pBuffer, int iOffset, const void* pData, int iSize);
XGE_API int xgeBufferUpload(xge_buffer pBuffer);
XGE_API void xgeBufferFree(xge_buffer pBuffer);
XGE_API void xgePassInit(xge_pass pPass, xge_render_target pTarget, uint32_t iClearFlags, uint32_t iClearColor);
XGE_API int xgePassBegin(xge_pass pPass);
XGE_API int xgePassEnd(xge_pass pPass);
XGE_API int xgeShaderCreate(xge_shader pShader, const char* sVertexSource, const char* sFragmentSource);
XGE_API int xgeShaderAddRef(xge_shader pShader);
XGE_API void xgeShaderFree(xge_shader pShader);
XGE_API int xgeShaderUniform1i(xge_shader pShader, const char* sName, int iX);
XGE_API int xgeShaderUniform1f(xge_shader pShader, const char* sName, float fX);
XGE_API int xgeShaderUniform2f(xge_shader pShader, const char* sName, float fX, float fY);
XGE_API int xgeShaderUniform3f(xge_shader pShader, const char* sName, float fX, float fY, float fZ);
XGE_API int xgeShaderUniform4f(xge_shader pShader, const char* sName, float fX, float fY, float fZ, float fW);
XGE_API int xgeShaderVariantSetInit(xge_shader_variant_set pSet, const char* sVertexSource, const char* sFragmentSource);
XGE_API void xgeShaderVariantSetFree(xge_shader_variant_set pSet);
XGE_API int xgeShaderVariantGet(xge_shader_variant_set pSet, uint32_t iKey, const xge_shader_define_t* pDefines, int iDefineCount, xge_shader* ppShader);
XGE_API void xgeMaterialInit(xge_material pMaterial);
XGE_API void xgeMaterialFree(xge_material pMaterial);
XGE_API void xgeMaterialSetShader(xge_material pMaterial, xge_shader pShader);
XGE_API void xgeMaterialSetTexture(xge_material pMaterial, xge_texture pTexture);
XGE_API void xgeMaterialSetTexture2(xge_material pMaterial, xge_texture pTexture);
XGE_API void xgeMaterialSetTexture3(xge_material pMaterial, xge_texture pTexture);
XGE_API void xgeMaterialSetColor(xge_material pMaterial, uint32_t iColor);
XGE_API void xgeMaterialSetBlend(xge_material pMaterial, int iBlend);
XGE_API void xgeMaterialDraw(xge_material pMaterial, const xge_draw_t* pDraw);
XGE_API int xgeMeshCreate(xge_mesh pMesh, const xge_vertex_t* pVertices, int iVertexCount, const uint16_t* pIndices, int iIndexCount, uint32_t iFlags);
XGE_API int xgeMeshUpdate(xge_mesh pMesh, const xge_vertex_t* pVertices, int iVertexCount, const uint16_t* pIndices, int iIndexCount);
XGE_API void xgeMeshFree(xge_mesh pMesh);
XGE_API void xgeMeshDraw(xge_mesh pMesh, xge_texture pTexture, uint32_t iFlags);
XGE_API void xgeDraw(xge_texture pTexture, float fX, float fY);
XGE_API void xgeDrawEx(const xge_draw_t* pDraw);
XGE_API void xgeDrawQuad3D(xge_texture pTexture, const xge_vertex_t* pVertices, uint32_t iFlags);
XGE_API void xgeDrawPx(xge_texture pTexture, int iX, int iY);
XGE_API void xgeNinePatchInitSimple(xge_nine_patch pPatch, xge_texture pTexture, xge_rect_t tSrc);
XGE_API void xgeNinePatchInit(xge_nine_patch pPatch, xge_texture pTexture, xge_rect_t tSrc, float fX1, float fY1, float fX2, float fY2);
XGE_API void xgeNinePatchSetMode(xge_nine_patch pPatch, int iMode);
XGE_API void xgeNinePatchSetColor(xge_nine_patch pPatch, uint32_t iColor);
XGE_API void xgeNinePatchDraw(const xge_nine_patch_t* pPatch, xge_rect_t tDst, uint32_t iFlags);
XGE_API int xgeSpriteBatchInit(xge_sprite_batch pBatch, xge_texture pTexture, int iCapacity, uint32_t iFlags);
XGE_API void xgeSpriteBatchFree(xge_sprite_batch pBatch);
XGE_API void xgeSpriteBatchClear(xge_sprite_batch pBatch);
XGE_API int xgeSpriteBatchAdd(xge_sprite_batch pBatch, const xge_draw_t* pDraw);
XGE_API int xgeSpriteBatchFlush(xge_sprite_batch pBatch);
XGE_API void xgeShapePoint(float fX, float fY, float fSize, uint32_t iColor);
XGE_API void xgeShapePointPx(float fX, float fY, float fSize, uint32_t iColor);
XGE_API void xgeShapeLine(float fX0, float fY0, float fX1, float fY1, float fWidth, uint32_t iColor);
XGE_API void xgeShapeLinePx(float fX0, float fY0, float fX1, float fY1, float fWidth, uint32_t iColor);
XGE_API void xgeShapeRectFill(xge_rect_t tRect, uint32_t iColor);
XGE_API void xgeShapeRectFillPx(xge_rect_t tRect, uint32_t iColor);
XGE_API void xgeShapeRectStroke(xge_rect_t tRect, float fWidth, uint32_t iColor);
XGE_API void xgeShapeRectStrokePx(xge_rect_t tRect, float fWidth, uint32_t iColor);
XGE_API void xgeShapeCircleFill(float fX, float fY, float fRadius, uint32_t iColor);
XGE_API void xgeShapeCircleFillPx(float fX, float fY, float fRadius, uint32_t iColor);
XGE_API void xgeShapeCircleStroke(float fX, float fY, float fRadius, float fWidth, uint32_t iColor);
XGE_API void xgeShapeCircleStrokePx(float fX, float fY, float fRadius, float fWidth, uint32_t iColor);
XGE_API void xgeShapeArc(float fX, float fY, float fRadius, float fStartRadians, float fEndRadians, float fWidth, uint32_t iColor);
XGE_API void xgeShapeArcPx(float fX, float fY, float fRadius, float fStartRadians, float fEndRadians, float fWidth, uint32_t iColor);
XGE_API void xgeShapeTriangleFill(xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC, uint32_t iColor);
XGE_API void xgeShapeTriangleFillPx(xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC, uint32_t iColor);
XGE_API void xgeShapeTriangleStroke(xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC, float fWidth, uint32_t iColor);
XGE_API void xgeShapeTriangleStrokePx(xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC, float fWidth, uint32_t iColor);
XGE_API void xgeShapeEllipseFill(float fX, float fY, float fRadiusX, float fRadiusY, uint32_t iColor);
XGE_API void xgeShapeEllipseFillPx(float fX, float fY, float fRadiusX, float fRadiusY, uint32_t iColor);
XGE_API void xgeShapeEllipseStroke(float fX, float fY, float fRadiusX, float fRadiusY, float fWidth, uint32_t iColor);
XGE_API void xgeShapeEllipseStrokePx(float fX, float fY, float fRadiusX, float fRadiusY, float fWidth, uint32_t iColor);
XGE_API void xgeShapePieFill(float fX, float fY, float fRadiusX, float fRadiusY, float fStartRadians, float fEndRadians, uint32_t iColor);
XGE_API void xgeShapePieFillPx(float fX, float fY, float fRadiusX, float fRadiusY, float fStartRadians, float fEndRadians, uint32_t iColor);
XGE_API void xgeShapeChordFill(float fX, float fY, float fRadiusX, float fRadiusY, float fStartRadians, float fEndRadians, uint32_t iColor);
XGE_API void xgeShapeChordFillPx(float fX, float fY, float fRadiusX, float fRadiusY, float fStartRadians, float fEndRadians, uint32_t iColor);
XGE_API void xgeShapeCapsuleFill(xge_rect_t tRect, uint32_t iColor);
XGE_API void xgeShapeCapsuleFillPx(xge_rect_t tRect, uint32_t iColor);
XGE_API void xgeShapeCapsuleStroke(xge_rect_t tRect, float fWidth, uint32_t iColor);
XGE_API void xgeShapeCapsuleStrokePx(xge_rect_t tRect, float fWidth, uint32_t iColor);
XGE_API void xgeShapeRenderModeSet(int iMode);
XGE_API int xgeShapeRenderModeGet(void);
XGE_API void xgeShapeRoundRectModeSet(int iMode);
XGE_API int xgeShapeRoundRectModeGet(void);
XGE_API void xgeShapeRoundRectFill(xge_rect_t tRect, float fRadius, uint32_t iColor);
XGE_API void xgeShapeRoundRectFillPx(xge_rect_t tRect, float fRadius, uint32_t iColor);
XGE_API void xgeShapeRoundRectFillEx(xge_rect_t tRect, xge_shape_round_rect_t tRadii, int iMode, uint32_t iColor);
XGE_API void xgeShapeRoundRectFillExPx(xge_rect_t tRect, xge_shape_round_rect_t tRadii, int iMode, uint32_t iColor);
XGE_API void xgeShapeRoundRectStroke(xge_rect_t tRect, float fRadius, float fWidth, uint32_t iColor);
XGE_API void xgeShapeRoundRectStrokePx(xge_rect_t tRect, float fRadius, float fWidth, uint32_t iColor);
XGE_API void xgeShapeRoundRectStrokeEx(xge_rect_t tRect, xge_shape_round_rect_t tRadii, int iMode, float fWidth, uint32_t iColor);
XGE_API void xgeShapeRoundRectStrokeExPx(xge_rect_t tRect, xge_shape_round_rect_t tRadii, int iMode, float fWidth, uint32_t iColor);
XGE_API void xgeShapeRoundRectDraw(xge_rect_t tRect, float fRadius, uint32_t iFillColor, float fStrokeWidth, uint32_t iStrokeColor);
XGE_API void xgeShapeRoundRectDrawPx(xge_rect_t tRect, float fRadius, uint32_t iFillColor, float fStrokeWidth, uint32_t iStrokeColor);
XGE_API void xgeShapeRoundRectDrawEx(xge_rect_t tRect, xge_shape_round_rect_t tRadii, int iMode, uint32_t iFillColor, float fStrokeWidth, uint32_t iStrokeColor);
XGE_API void xgeShapeRoundRectDrawExPx(xge_rect_t tRect, xge_shape_round_rect_t tRadii, int iMode, uint32_t iFillColor, float fStrokeWidth, uint32_t iStrokeColor);
XGE_API void xgeShapePolygonFill(const xge_vec2_t* pPoints, int iCount, uint32_t iColor);
XGE_API void xgeShapePolygonFillPx(const xge_vec2_t* pPoints, int iCount, uint32_t iColor);
XGE_API int xgeShapeMeshFill(const xge_shape_vertex_t* pVertices, int iVertexCount, const uint32_t* pIndices, int iIndexCount);
XGE_API int xgeShapeMeshFillPx(const xge_shape_vertex_t* pVertices, int iVertexCount, const uint32_t* pIndices, int iIndexCount);
XGE_API int xgeShapeExMatrixIdentity(xge_shape_ex_matrix_t* pMatrix);
XGE_API int xgeShapeExMatrixMultiply(xge_shape_ex_matrix_t* pOut, const xge_shape_ex_matrix_t* pParent, const xge_shape_ex_matrix_t* pLocal);
XGE_API int xgeShapeExMatrixTranslate(xge_shape_ex_matrix_t* pMatrix, float fTX, float fTY);
XGE_API int xgeShapeExMatrixScale(xge_shape_ex_matrix_t* pMatrix, float fSX, float fSY);
XGE_API int xgeShapeExMatrixRotate(xge_shape_ex_matrix_t* pMatrix, float fRadians);
XGE_API int xgeShapeExMatrixSkew(xge_shape_ex_matrix_t* pMatrix, float fXRadians, float fYRadians);
XGE_API int xgeShapeExMatrixInvert(xge_shape_ex_matrix_t* pOut, const xge_shape_ex_matrix_t* pMatrix);
XGE_API int xgeShapeExMatrixPoint(xge_vec2_t* pOut, const xge_shape_ex_matrix_t* pMatrix, xge_vec2_t tPoint);
XGE_API int xgeShapeExMatrixVector(xge_vec2_t* pOut, const xge_shape_ex_matrix_t* pMatrix, xge_vec2_t tVector);
XGE_API int xgeShapeExMatrixRectBounds(xge_rect_t* pOut, const xge_shape_ex_matrix_t* pMatrix, xge_rect_t tRect);
XGE_API int xgeShapeExMatrixStrokeScale(float* pScale, const xge_shape_ex_matrix_t* pMatrix);
XGE_API int xgeShapeExCreate(xge_shape_ex* ppShape);
XGE_API int xgeShapeExAddRef(xge_shape_ex pShape);
XGE_API int xgeShapeExRefCountGet(xge_shape_ex pShape, int* pRefCount);
XGE_API uint32_t xgeShapeExIdFromName(const char* sName);
XGE_API int xgeShapeExId(xge_shape_ex pShape, uint32_t iId);
XGE_API int xgeShapeExIdGet(xge_shape_ex pShape, uint32_t* pId);
XGE_API int xgeShapeExParentGet(xge_shape_ex pShape, xge_shape_ex_scene* ppParentScene);
XGE_API int xgeShapeExClone(xge_shape_ex pShape, xge_shape_ex* ppClone);
XGE_API void xgeShapeExDestroy(xge_shape_ex pShape);
XGE_API int xgeShapeExReset(xge_shape_ex pShape);
XGE_API int xgeShapeExMoveTo(xge_shape_ex pShape, float fX, float fY);
XGE_API int xgeShapeExLineTo(xge_shape_ex pShape, float fX, float fY);
XGE_API int xgeShapeExQuadTo(xge_shape_ex pShape, float fCX, float fCY, float fX, float fY);
XGE_API int xgeShapeExCubicTo(xge_shape_ex pShape, float fC1X, float fC1Y, float fC2X, float fC2Y, float fX, float fY);
XGE_API int xgeShapeExArcTo(xge_shape_ex pShape, float fRX, float fRY, float fAxisDegrees, int bLargeArc, int bSweep, float fX, float fY);
XGE_API int xgeShapeExClose(xge_shape_ex pShape);
XGE_API int xgeShapeExAppendTriangle(xge_shape_ex pShape, float fX1, float fY1, float fX2, float fY2, float fX3, float fY3, int bClockwise);
XGE_API int xgeShapeExAppendLine(xge_shape_ex pShape, float fX1, float fY1, float fX2, float fY2);
XGE_API int xgeShapeExAppendPolyline(xge_shape_ex pShape, const xge_vec2_t* pPoints, int iPointCount);
XGE_API int xgeShapeExAppendPolygon(xge_shape_ex pShape, const xge_vec2_t* pPoints, int iPointCount);
XGE_API int xgeShapeExAppendRect(xge_shape_ex pShape, float fX, float fY, float fW, float fH, float fRX, float fRY, int bClockwise);
XGE_API int xgeShapeExAppendCapsule(xge_shape_ex pShape, float fX, float fY, float fW, float fH, int bClockwise);
XGE_API int xgeShapeExAppendCircle(xge_shape_ex pShape, float fCX, float fCY, float fRX, float fRY, int bClockwise);
XGE_API int xgeShapeExAppendEllipse(xge_shape_ex pShape, float fCX, float fCY, float fRX, float fRY, int bClockwise);
XGE_API int xgeShapeExAppendArc(xge_shape_ex pShape, float fCX, float fCY, float fRX, float fRY, float fStartRadians, float fEndRadians);
XGE_API int xgeShapeExAppendPie(xge_shape_ex pShape, float fCX, float fCY, float fRX, float fRY, float fStartRadians, float fEndRadians);
XGE_API int xgeShapeExAppendChord(xge_shape_ex pShape, float fCX, float fCY, float fRX, float fRY, float fStartRadians, float fEndRadians);
XGE_API int xgeShapeExAppendPath(xge_shape_ex pShape, const uint8_t* pCommands, int iCommandCount, const xge_vec2_t* pPoints, int iPointCount);
XGE_API int xgeShapeExAppendSvgPath(xge_shape_ex pShape, const char* sPath);
XGE_API int xgeShapeExGetPath(xge_shape_ex pShape, const uint8_t** ppCommands, int* pCommandCount, const xge_vec2_t** ppPoints, int* pPointCount);
XGE_API int xgeShapeExGetSvgPathData(xge_shape_ex pShape, char* sBuffer, int iBufferSize, int* pRequiredSize);
XGE_API int xgeShapeExFillColor(xge_shape_ex pShape, uint32_t iColor);
XGE_API int xgeShapeExFillTypeGet(xge_shape_ex pShape, int* pType);
XGE_API int xgeShapeExFillColorGet(xge_shape_ex pShape, uint32_t* pColor);
XGE_API int xgeShapeExFillLinearGradient(xge_shape_ex pShape, float fX1, float fY1, float fX2, float fY2, int iUnits, const xge_shape_ex_color_stop_t* pStops, int iStopCount);
XGE_API int xgeShapeExFillLinearGradientGet(xge_shape_ex pShape, float* pX1, float* pY1, float* pX2, float* pY2, int* pUnits, const xge_shape_ex_color_stop_t** ppStops, int* pStopCount);
XGE_API int xgeShapeExFillRadialGradient(xge_shape_ex pShape, float fCX, float fCY, float fRadius, float fFX, float fFY, int iUnits, const xge_shape_ex_color_stop_t* pStops, int iStopCount);
XGE_API int xgeShapeExFillRadialGradientEx(xge_shape_ex pShape, float fCX, float fCY, float fRadius, float fFX, float fFY, float fFocalRadius, int iUnits, const xge_shape_ex_color_stop_t* pStops, int iStopCount);
XGE_API int xgeShapeExFillRadialGradientGet(xge_shape_ex pShape, float* pCX, float* pCY, float* pRadius, float* pFX, float* pFY, int* pUnits, const xge_shape_ex_color_stop_t** ppStops, int* pStopCount);
XGE_API int xgeShapeExFillRadialGradientGetEx(xge_shape_ex pShape, float* pCX, float* pCY, float* pRadius, float* pFX, float* pFY, float* pFocalRadius, int* pUnits, const xge_shape_ex_color_stop_t** ppStops, int* pStopCount);
XGE_API int xgeShapeExFillGradientSpread(xge_shape_ex pShape, int iSpread);
XGE_API int xgeShapeExFillGradientSpreadGet(xge_shape_ex pShape, int* pSpread);
XGE_API int xgeShapeExFillGradientTransformSet(xge_shape_ex pShape, const xge_shape_ex_matrix_t* pMatrix);
XGE_API int xgeShapeExFillGradientTransformIdentity(xge_shape_ex pShape);
XGE_API int xgeShapeExFillGradientTransformGet(xge_shape_ex pShape, xge_shape_ex_matrix_t* pMatrix);
XGE_API int xgeShapeExStrokeColor(xge_shape_ex pShape, uint32_t iColor);
XGE_API int xgeShapeExStrokeTypeGet(xge_shape_ex pShape, int* pType);
XGE_API int xgeShapeExStrokeColorGet(xge_shape_ex pShape, uint32_t* pColor);
XGE_API int xgeShapeExStrokeLinearGradient(xge_shape_ex pShape, float fX1, float fY1, float fX2, float fY2, int iUnits, const xge_shape_ex_color_stop_t* pStops, int iStopCount);
XGE_API int xgeShapeExStrokeLinearGradientGet(xge_shape_ex pShape, float* pX1, float* pY1, float* pX2, float* pY2, int* pUnits, const xge_shape_ex_color_stop_t** ppStops, int* pStopCount);
XGE_API int xgeShapeExStrokeRadialGradient(xge_shape_ex pShape, float fCX, float fCY, float fRadius, float fFX, float fFY, int iUnits, const xge_shape_ex_color_stop_t* pStops, int iStopCount);
XGE_API int xgeShapeExStrokeRadialGradientEx(xge_shape_ex pShape, float fCX, float fCY, float fRadius, float fFX, float fFY, float fFocalRadius, int iUnits, const xge_shape_ex_color_stop_t* pStops, int iStopCount);
XGE_API int xgeShapeExStrokeRadialGradientGet(xge_shape_ex pShape, float* pCX, float* pCY, float* pRadius, float* pFX, float* pFY, int* pUnits, const xge_shape_ex_color_stop_t** ppStops, int* pStopCount);
XGE_API int xgeShapeExStrokeRadialGradientGetEx(xge_shape_ex pShape, float* pCX, float* pCY, float* pRadius, float* pFX, float* pFY, float* pFocalRadius, int* pUnits, const xge_shape_ex_color_stop_t** ppStops, int* pStopCount);
XGE_API int xgeShapeExStrokeGradientSpread(xge_shape_ex pShape, int iSpread);
XGE_API int xgeShapeExStrokeGradientSpreadGet(xge_shape_ex pShape, int* pSpread);
XGE_API int xgeShapeExStrokeGradientTransformSet(xge_shape_ex pShape, const xge_shape_ex_matrix_t* pMatrix);
XGE_API int xgeShapeExStrokeGradientTransformIdentity(xge_shape_ex pShape);
XGE_API int xgeShapeExStrokeGradientTransformGet(xge_shape_ex pShape, xge_shape_ex_matrix_t* pMatrix);
XGE_API int xgeShapeExStrokeWidth(xge_shape_ex pShape, float fWidth);
XGE_API int xgeShapeExStrokeWidthGet(xge_shape_ex pShape, float* pWidth);
XGE_API int xgeShapeExStrokeCap(xge_shape_ex pShape, int iCap);
XGE_API int xgeShapeExStrokeCapGet(xge_shape_ex pShape, int* pCap);
XGE_API int xgeShapeExStrokeJoin(xge_shape_ex pShape, int iJoin);
XGE_API int xgeShapeExStrokeJoinGet(xge_shape_ex pShape, int* pJoin);
XGE_API int xgeShapeExStrokeMiterLimit(xge_shape_ex pShape, float fLimit);
XGE_API int xgeShapeExStrokeMiterLimitGet(xge_shape_ex pShape, float* pLimit);
XGE_API int xgeShapeExStrokeNonScaling(xge_shape_ex pShape, int bNonScaling);
XGE_API int xgeShapeExStrokeNonScalingGet(xge_shape_ex pShape, int* pNonScaling);
XGE_API int xgeShapeExStrokeDash(xge_shape_ex pShape, const float* pDashPattern, int iDashCount, float fDashOffset);
XGE_API int xgeShapeExStrokeDashGet(xge_shape_ex pShape, const float** ppDashPattern, int* pDashCount, float* pDashOffset);
XGE_API int xgeShapeExTrimPath(xge_shape_ex pShape, float fBegin, float fEnd, int bSimultaneous);
XGE_API int xgeShapeExTrimPathGet(xge_shape_ex pShape, float* pBegin, float* pEnd, int* pSimultaneous, int* pEnabled);
XGE_API int xgeShapeExTrimClear(xge_shape_ex pShape);
XGE_API int xgeShapeExFillRule(xge_shape_ex pShape, int iRule);
XGE_API int xgeShapeExFillRuleGet(xge_shape_ex pShape, int* pRule);
XGE_API int xgeShapeExPaintOrder(xge_shape_ex pShape, int bStrokeFirst);
XGE_API int xgeShapeExPaintOrderGet(xge_shape_ex pShape, int* pStrokeFirst);
XGE_API int xgeShapeExOpacity(xge_shape_ex pShape, float fOpacity);
XGE_API int xgeShapeExOpacityGet(xge_shape_ex pShape, float* pOpacity);
XGE_API int xgeShapeExVisible(xge_shape_ex pShape, int bVisible);
XGE_API int xgeShapeExVisibleGet(xge_shape_ex pShape, int* pVisible);
XGE_API int xgeShapeExBlend(xge_shape_ex pShape, int iBlend);
XGE_API int xgeShapeExBlendClear(xge_shape_ex pShape);
XGE_API int xgeShapeExBlendGet(xge_shape_ex pShape, int* pBlend, int* pBlendSet);
XGE_API int xgeShapeExMaskShapeSet(xge_shape_ex pShape, xge_shape_ex pTarget, int iMethod);
XGE_API int xgeShapeExMaskSceneSet(xge_shape_ex pShape, xge_shape_ex_scene pTarget, int iMethod);
XGE_API int xgeShapeExMaskClear(xge_shape_ex pShape);
XGE_API int xgeShapeExMaskGet(xge_shape_ex pShape, int* pMethod, int* pTargetType, xge_shape_ex* ppTargetShape, xge_shape_ex_scene* ppTargetScene);
XGE_API int xgeShapeExMaskCompositeScene(xge_shape_ex_scene pMaskScene, int iMethod, int iOutputBlend, xge_rect_t tSourceBounds, xge_shape_ex_draw_proc pSourceDraw, void* pSourceUser, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, int bScreenSpace);
XGE_API int xgeShapeExGetBounds(xge_shape_ex pShape, float fTolerance, xge_rect_t* pBounds);
XGE_API int xgeShapeExGetOBB(xge_shape_ex pShape, float fTolerance, xge_vec2_t* pPoints4);
XGE_API int xgeShapeExBoundsIntersects(xge_shape_ex pShape, xge_rect_t tRect, float fTolerance, int* pIntersects);
XGE_API int xgeShapeExIntersects(xge_shape_ex pShape, xge_rect_t tRect, float fTolerance, int* pIntersects);
XGE_API int xgeShapeExIntersectsEx(xge_shape_ex pShape, xge_rect_t tRect, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, int* pIntersects);
XGE_API int xgeShapeExContainsPoint(xge_shape_ex pShape, float fX, float fY, float fTolerance, int* pContains);
XGE_API int xgeShapeExContainsPointEx(xge_shape_ex pShape, float fX, float fY, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, int* pContains);
XGE_API int xgeShapeExGetLength(xge_shape_ex pShape, float fTolerance, float* pLength);
XGE_API int xgeShapeExGetPointAtLength(xge_shape_ex pShape, float fDistance, float fTolerance, xge_vec2_t* pPoint, xge_vec2_t* pTangent);
XGE_API int xgeShapeExPathMeasureCreate(xge_shape_ex_path_measure* ppMeasure, xge_shape_ex pShape, const xge_shape_ex_matrix_t* pMatrix, float fTolerance);
XGE_API void xgeShapeExPathMeasureDestroy(xge_shape_ex_path_measure pMeasure);
XGE_API int xgeShapeExPathMeasureGetLength(xge_shape_ex_path_measure pMeasure, float* pLength);
XGE_API int xgeShapeExPathMeasureGetPointAtLength(xge_shape_ex_path_measure pMeasure, float fDistance, xge_vec2_t* pPoint, xge_vec2_t* pTangent);
XGE_API int xgeShapeExClipRectSet(xge_shape_ex pShape, xge_rect_t tRect);
XGE_API int xgeShapeExClipRectGet(xge_shape_ex pShape, xge_rect_t* pRect, int* pEnabled);
XGE_API int xgeShapeExClipShapeAdd(xge_shape_ex pShape, xge_shape_ex pClipShape);
XGE_API int xgeShapeExClipShapeAddEx(xge_shape_ex pShape, xge_shape_ex pClipShape, int iMode);
XGE_API int xgeShapeExClipShapeGetCount(xge_shape_ex pShape, int* pCount);
XGE_API int xgeShapeExClipShapeGetAt(xge_shape_ex pShape, int iIndex, xge_shape_ex* ppClipShape);
XGE_API int xgeShapeExClipShapeGetAtEx(xge_shape_ex pShape, int iIndex, xge_shape_ex* ppClipShape, int* pMode);
XGE_API int xgeShapeExClipShapeClear(xge_shape_ex pShape);
XGE_API int xgeShapeExClipClear(xge_shape_ex pShape);
XGE_API int xgeShapeExStencilClipBegin(xge_shape_ex pClipShape, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, int* pApplied);
XGE_API int xgeShapeExStencilClipBeginPx(xge_shape_ex pClipShape, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, int* pApplied);
XGE_API int xgeShapeExStencilClipEnd(int bApplied, int iRet);
XGE_API int xgeShapeExTransformSet(xge_shape_ex pShape, const xge_shape_ex_matrix_t* pMatrix);
XGE_API int xgeShapeExTransformIdentity(xge_shape_ex pShape);
XGE_API int xgeShapeExTransformGet(xge_shape_ex pShape, xge_shape_ex_matrix_t* pMatrix);
XGE_API int xgeShapeExTransformTranslate(xge_shape_ex pShape, float fTX, float fTY);
XGE_API int xgeShapeExTransformScale(xge_shape_ex pShape, float fSX, float fSY);
XGE_API int xgeShapeExTransformRotate(xge_shape_ex pShape, float fRadians);
XGE_API int xgeShapeExTransformSkew(xge_shape_ex pShape, float fXRadians, float fYRadians);
XGE_API int xgeShapeExDraw(xge_shape_ex pShape, float fTolerance);
XGE_API int xgeShapeExDrawPx(xge_shape_ex pShape, float fTolerance);
XGE_API int xgeShapeExDrawEx(xge_shape_ex pShape, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, float fParentOpacity);
XGE_API int xgeShapeExDrawPxEx(xge_shape_ex pShape, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, float fParentOpacity);
XGE_API int xgeShapeExSceneCreate(xge_shape_ex_scene* ppScene);
XGE_API int xgeShapeExSceneAddRef(xge_shape_ex_scene pScene);
XGE_API int xgeShapeExSceneRefCountGet(xge_shape_ex_scene pScene, int* pRefCount);
XGE_API int xgeShapeExSceneId(xge_shape_ex_scene pScene, uint32_t iId);
XGE_API int xgeShapeExSceneIdGet(xge_shape_ex_scene pScene, uint32_t* pId);
XGE_API int xgeShapeExSceneParentGet(xge_shape_ex_scene pScene, xge_shape_ex_scene* ppParentScene);
XGE_API int xgeShapeExSceneClone(xge_shape_ex_scene pScene, xge_shape_ex_scene* ppClone);
XGE_API void xgeShapeExSceneDestroy(xge_shape_ex_scene pScene);
XGE_API int xgeShapeExSceneClear(xge_shape_ex_scene pScene);
XGE_API int xgeShapeExSceneAdd(xge_shape_ex_scene pScene, xge_shape_ex pShape);
XGE_API int xgeShapeExSceneAddScene(xge_shape_ex_scene pScene, xge_shape_ex_scene pChildScene);
XGE_API int xgeShapeExSceneInsert(xge_shape_ex_scene pScene, xge_shape_ex pShape, xge_shape_ex pBefore);
XGE_API int xgeShapeExSceneInsertShapeAt(xge_shape_ex_scene pScene, xge_shape_ex pShape, int iIndex);
XGE_API int xgeShapeExSceneInsertSceneAt(xge_shape_ex_scene pScene, xge_shape_ex_scene pChildScene, int iIndex);
XGE_API int xgeShapeExSceneRemove(xge_shape_ex_scene pScene, xge_shape_ex pShape);
XGE_API int xgeShapeExSceneRemoveScene(xge_shape_ex_scene pScene, xge_shape_ex_scene pChildScene);
XGE_API int xgeShapeExSceneGetCount(xge_shape_ex_scene pScene, int* pCount);
XGE_API int xgeShapeExSceneGetAt(xge_shape_ex_scene pScene, int iIndex, xge_shape_ex* ppShape);
XGE_API int xgeShapeExSceneChildGetAt(xge_shape_ex_scene pScene, int iIndex, xge_shape_ex_scene_child_t* pChild);
XGE_API int xgeShapeExSceneTraverse(xge_shape_ex_scene pScene, xge_shape_ex_scene_visit_proc onPaint, void* pUser);
XGE_API int xgeShapeExSceneTransformSet(xge_shape_ex_scene pScene, const xge_shape_ex_matrix_t* pMatrix);
XGE_API int xgeShapeExSceneTransformIdentity(xge_shape_ex_scene pScene);
XGE_API int xgeShapeExSceneTransformGet(xge_shape_ex_scene pScene, xge_shape_ex_matrix_t* pMatrix);
XGE_API int xgeShapeExSceneTransformTranslate(xge_shape_ex_scene pScene, float fTX, float fTY);
XGE_API int xgeShapeExSceneTransformScale(xge_shape_ex_scene pScene, float fSX, float fSY);
XGE_API int xgeShapeExSceneTransformRotate(xge_shape_ex_scene pScene, float fRadians);
XGE_API int xgeShapeExSceneTransformSkew(xge_shape_ex_scene pScene, float fXRadians, float fYRadians);
XGE_API int xgeShapeExSceneOpacity(xge_shape_ex_scene pScene, float fOpacity);
XGE_API int xgeShapeExSceneOpacityGet(xge_shape_ex_scene pScene, float* pOpacity);
XGE_API int xgeShapeExSceneVisible(xge_shape_ex_scene pScene, int bVisible);
XGE_API int xgeShapeExSceneVisibleGet(xge_shape_ex_scene pScene, int* pVisible);
XGE_API int xgeShapeExSceneBlend(xge_shape_ex_scene pScene, int iBlend);
XGE_API int xgeShapeExSceneBlendClear(xge_shape_ex_scene pScene);
XGE_API int xgeShapeExSceneBlendGet(xge_shape_ex_scene pScene, int* pBlend, int* pBlendSet);
XGE_API int xgeShapeExSceneMaskShapeSet(xge_shape_ex_scene pScene, xge_shape_ex pTarget, int iMethod);
XGE_API int xgeShapeExSceneMaskSceneSet(xge_shape_ex_scene pScene, xge_shape_ex_scene pTarget, int iMethod);
XGE_API int xgeShapeExSceneMaskClear(xge_shape_ex_scene pScene);
XGE_API int xgeShapeExSceneMaskGet(xge_shape_ex_scene pScene, int* pMethod, int* pTargetType, xge_shape_ex* ppTargetShape, xge_shape_ex_scene* ppTargetScene);
XGE_API int xgeShapeExSceneClipRectSet(xge_shape_ex_scene pScene, xge_rect_t tRect);
XGE_API int xgeShapeExSceneClipRectGet(xge_shape_ex_scene pScene, xge_rect_t* pRect, int* pEnabled);
XGE_API int xgeShapeExSceneClipShapeAdd(xge_shape_ex_scene pScene, xge_shape_ex pClipShape);
XGE_API int xgeShapeExSceneClipShapeAddEx(xge_shape_ex_scene pScene, xge_shape_ex pClipShape, int iMode);
XGE_API int xgeShapeExSceneClipShapeGetCount(xge_shape_ex_scene pScene, int* pCount);
XGE_API int xgeShapeExSceneClipShapeGetAt(xge_shape_ex_scene pScene, int iIndex, xge_shape_ex* ppClipShape);
XGE_API int xgeShapeExSceneClipShapeGetAtEx(xge_shape_ex_scene pScene, int iIndex, xge_shape_ex* ppClipShape, int* pMode);
XGE_API int xgeShapeExSceneClipShapeClear(xge_shape_ex_scene pScene);
XGE_API int xgeShapeExSceneClipClear(xge_shape_ex_scene pScene);
XGE_API int xgeShapeExSceneEffectClear(xge_shape_ex_scene pScene);
XGE_API int xgeShapeExSceneEffectGaussianBlur(xge_shape_ex_scene pScene, float fSigma, int iDirection, int iBorder, int iQuality);
XGE_API int xgeShapeExSceneEffectDropShadow(xge_shape_ex_scene pScene, uint32_t iColor, float fAngleDegrees, float fDistance, float fSigma, int iQuality);
XGE_API int xgeShapeExSceneEffectFill(xge_shape_ex_scene pScene, uint32_t iColor);
XGE_API int xgeShapeExSceneEffectTint(xge_shape_ex_scene pScene, uint32_t iBlackColor, uint32_t iWhiteColor, float fIntensity);
XGE_API int xgeShapeExSceneEffectTritone(xge_shape_ex_scene pScene, uint32_t iShadowColor, uint32_t iMidtoneColor, uint32_t iHighlightColor, int iBlend);
XGE_API int xgeShapeExSceneEffectGetCount(xge_shape_ex_scene pScene, int* pCount);
XGE_API int xgeShapeExSceneEffectGetAt(xge_shape_ex_scene pScene, int iIndex, xge_shape_ex_scene_effect_t* pEffect);
XGE_API int xgeShapeExSceneGetBounds(xge_shape_ex_scene pScene, float fTolerance, xge_rect_t* pBounds);
XGE_API int xgeShapeExSceneGetOBB(xge_shape_ex_scene pScene, float fTolerance, xge_vec2_t* pPoints4);
XGE_API int xgeShapeExSceneBoundsIntersects(xge_shape_ex_scene pScene, xge_rect_t tRect, float fTolerance, int* pIntersects);
XGE_API int xgeShapeExSceneIntersects(xge_shape_ex_scene pScene, xge_rect_t tRect, float fTolerance, int* pIntersects);
XGE_API int xgeShapeExSceneIntersectsEx(xge_shape_ex_scene pScene, xge_rect_t tRect, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, int* pIntersects);
XGE_API int xgeShapeExSceneContainsPoint(xge_shape_ex_scene pScene, float fX, float fY, float fTolerance, int* pContains);
XGE_API int xgeShapeExSceneContainsPointEx(xge_shape_ex_scene pScene, float fX, float fY, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, int* pContains);
XGE_API int xgeShapeExSceneHitTest(xge_shape_ex_scene pScene, float fX, float fY, float fTolerance, xge_shape_ex* ppShape);
XGE_API int xgeShapeExSceneHitTestEx(xge_shape_ex_scene pScene, float fX, float fY, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, xge_shape_ex* ppShape);
XGE_API int xgeShapeExSceneDraw(xge_shape_ex_scene pScene, float fTolerance);
XGE_API int xgeShapeExSceneDrawPx(xge_shape_ex_scene pScene, float fTolerance);
XGE_API int xgeShapeExSceneDrawEx(xge_shape_ex_scene pScene, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, float fParentOpacity);
XGE_API int xgeShapeExSceneDrawPxEx(xge_shape_ex_scene pScene, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, float fParentOpacity);
XGE_API int xgeSvgCreate(xge_svg* ppSvg);
XGE_API void xgeSvgDestroy(xge_svg pSvg);
XGE_API int xgeSvgClear(xge_svg pSvg);
XGE_API int xgeSvgClone(xge_svg pSvg, xge_svg* ppClone);
XGE_API int xgeSvgLoad(xge_svg pSvg, const char* sURI);
XGE_API int xgeSvgLoadCached(const char* sURI, xge_svg* ppSvg);
XGE_API int xgeSvgLoadMemory(xge_svg pSvg, const void* pData, int iSize);
XGE_API int xgeSvgAddRef(xge_svg pSvg);
XGE_API int xgeSvgRefCountGet(xge_svg pSvg, int* pRefCount);
XGE_API int xgeSvgCacheInvalidate(const char* sURI);
XGE_API void xgeSvgCacheClear(void);
XGE_API int xgeSvgSetSize(xge_svg pSvg, float fWidth, float fHeight);
XGE_API int xgeSvgGetSize(xge_svg pSvg, float* pWidth, float* pHeight);
XGE_API int xgeSvgSetOrigin(xge_svg pSvg, float fX, float fY);
XGE_API int xgeSvgGetOrigin(xge_svg pSvg, float* pX, float* pY);
XGE_API int xgeSvgGetViewBox(xge_svg pSvg, xge_rect_t* pViewBox);
XGE_API int xgeSvgPaintGetById(xge_svg pSvg, uint32_t iId, xge_svg_paint* ppPaint);
XGE_API int xgeSvgPaintGetByName(xge_svg pSvg, const char* sName, xge_svg_paint* ppPaint);
XGE_API int xgeSvgPaintGetPicture(xge_svg pSvg, xge_svg_paint* ppPaint);
XGE_API int xgeSvgPaintIdGet(xge_svg_paint pPaint, uint32_t* pId);
XGE_API int xgeSvgPaintNameGet(xge_svg_paint pPaint, const char** ppName);
XGE_API int xgeSvgPaintTypeGet(xge_svg_paint pPaint, int* pType);
XGE_API int xgeSvgPaintTransformSet(xge_svg_paint pPaint, const xge_shape_ex_matrix_t* pMatrix);
XGE_API int xgeSvgPaintTransformIdentity(xge_svg_paint pPaint);
XGE_API int xgeSvgPaintTransformGet(xge_svg_paint pPaint, xge_shape_ex_matrix_t* pMatrix);
XGE_API int xgeSvgPaintTransformTranslate(xge_svg_paint pPaint, float fTX, float fTY);
XGE_API int xgeSvgPaintTransformScale(xge_svg_paint pPaint, float fSX, float fSY);
XGE_API int xgeSvgPaintTransformRotate(xge_svg_paint pPaint, float fRadians);
XGE_API int xgeSvgPaintOpacitySet(xge_svg_paint pPaint, float fOpacity);
XGE_API int xgeSvgPaintOpacityGet(xge_svg_paint pPaint, float* pOpacity);
XGE_API int xgeSvgPaintVisibleSet(xge_svg_paint pPaint, int bVisible);
XGE_API int xgeSvgPaintVisibleGet(xge_svg_paint pPaint, int* pVisible);
XGE_API int xgeSvgPaintBlendSet(xge_svg_paint pPaint, int iBlend);
XGE_API int xgeSvgPaintBlendGet(xge_svg_paint pPaint, int* pBlend);
XGE_API int xgeSvgPaintMaskShapeSet(xge_svg_paint pPaint, xge_shape_ex pTarget, int iMethod);
XGE_API int xgeSvgPaintMaskSceneSet(xge_svg_paint pPaint, xge_shape_ex_scene pTarget, int iMethod);
XGE_API int xgeSvgPaintMaskClear(xge_svg_paint pPaint);
XGE_API int xgeSvgPaintMaskGet(xge_svg_paint pPaint, int* pMethod, int* pTargetType, xge_shape_ex* ppTargetShape, xge_shape_ex_scene* ppTargetScene);
XGE_API int xgeSvgPaintClipShapeSet(xge_svg_paint pPaint, xge_shape_ex pClipShape);
XGE_API int xgeSvgPaintClipClear(xge_svg_paint pPaint);
XGE_API int xgeSvgPaintClipRectGet(xge_svg_paint pPaint, xge_rect_t* pRect, int* pEnabled);
XGE_API int xgeSvgPaintClipShapeGetCount(xge_svg_paint pPaint, int* pCount);
XGE_API int xgeSvgPaintClipShapeGetAt(xge_svg_paint pPaint, int iIndex, xge_shape_ex* ppClipShape);
XGE_API int xgeSvgPaintClipShapeGetAtEx(xge_svg_paint pPaint, int iIndex, xge_shape_ex* ppClipShape, int* pMode);
XGE_API int xgeSvgPaintShapeGet(xge_svg_paint pPaint, xge_shape_ex* ppShape);
XGE_API int xgeSvgPaintOwnerGet(xge_svg_paint pPaint, xge_svg* ppSvg);
XGE_API int xgeSvgPaintParentGet(xge_svg_paint pPaint, xge_svg_paint* ppParent);
XGE_API int xgeSvgPaintChildGetCount(xge_svg_paint pPaint, int* pCount);
XGE_API int xgeSvgPaintChildGetAt(xge_svg_paint pPaint, int iIndex, xge_svg_paint* ppChild);
XGE_API int xgeSvgPaintTraverse(xge_svg_paint pPaint, xge_svg_paint_visit_proc onPaint, void* pUser);
XGE_API int xgeSvgPaintGetBounds(xge_svg_paint pPaint, float fTolerance, xge_rect_t* pBounds);
XGE_API int xgeSvgPaintGetOBB(xge_svg_paint pPaint, float fTolerance, xge_vec2_t* pPoints4);
XGE_API int xgeSvgPaintIntersects(xge_svg_paint pPaint, xge_rect_t tRect, float fTolerance, int* pIntersects);
XGE_API int xgeSvgSetPreserveAspectRatio(xge_svg pSvg, const char* sValue);
XGE_API int xgeSvgGetPreserveAspectRatio(xge_svg pSvg, int* pAlignX, int* pAlignY, int* pMeetOrSlice);
XGE_API int xgeSvgGetDrawViewport(xge_svg pSvg, xge_rect_t tDst, xge_rect_t* pViewport);
XGE_API int xgeSvgGetBounds(xge_svg pSvg, float fTolerance, xge_rect_t* pBounds);
XGE_API int xgeSvgGetDrawBounds(xge_svg pSvg, xge_rect_t tDst, float fTolerance, xge_rect_t* pBounds);
XGE_API int xgeSvgContainsPoint(xge_svg pSvg, float fX, float fY, float fTolerance, int* pContains);
XGE_API int xgeSvgDrawContainsPoint(xge_svg pSvg, xge_rect_t tDst, float fX, float fY, float fTolerance, int* pContains);
XGE_API int xgeSvgDraw(xge_svg pSvg, xge_rect_t tDst, float fTolerance);
XGE_API int xgeSvgDrawPx(xge_svg pSvg, xge_rect_t tDst, float fTolerance);
XGE_API int xgeSvgRasterize(const char* sURI, int iWidth, int iHeight, void* pPixels, int iStride);
XGE_API int xgeSvgRasterizeMemory(const void* pData, int iSize, int iWidth, int iHeight, void* pPixels, int iStride);
XGE_API int xgeSvgTextureLoad(xge_texture pTexture, const char* sURI, int iWidth, int iHeight);
XGE_API int xgeSvgTextureLoadMemory(xge_texture pTexture, const void* pData, int iSize, int iWidth, int iHeight);
XGE_API int xgeShapeBatchInit(xge_shape_batch pBatch, uint32_t iColor, int iTriangleCapacity, uint32_t iFlags);
XGE_API void xgeShapeBatchFree(xge_shape_batch pBatch);
XGE_API void xgeShapeBatchClear(xge_shape_batch pBatch);
XGE_API int xgeShapeBatchTriangleFill(xge_shape_batch pBatch, xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC);
XGE_API int xgeShapeBatchRectFill(xge_shape_batch pBatch, xge_rect_t tRect);
XGE_API int xgeShapeBatchFlush(xge_shape_batch pBatch);
XGE_API void xgeViewportSet(xge_rect_t tRect);
XGE_API xge_rect_t xgeViewportGet(void);
XGE_API void xgeViewportClear(void);
XGE_API void xgeClipSet(xge_rect_t tRect);
XGE_API xge_rect_t xgeClipGet(void);
XGE_API void xgeClipClear(void);
XGE_API void xgeClipboardSetText(const char* sText);
XGE_API const char* xgeClipboardGetText(void);

XGE_API int xgeKeyDown(int iKey);
XGE_API int xgeKeyPressed(int iKey);
XGE_API int xgeKeyRepeated(int iKey);
XGE_API int xgeKeyReleased(int iKey);
XGE_API void xgeInputConsumeKey(int iKey);
XGE_API int xgeInputKeyConsumed(int iKey);
XGE_API void xgeMouseGet(float* pX, float* pY);
XGE_API void xgeMouseGetDelta(float* pDX, float* pDY);
XGE_API void xgeMouseGetWheel(float* pX, float* pY);
XGE_API int xgeMouseDown(int iButton);
XGE_API uint32_t xgeTextGet(void);
XGE_API int xgeImeGetEnabled(void);
XGE_API int xgeImeSetEnabled(int bEnabled);
/* sText remains valid until the next xgeImeEventGet call or xgeUnit. */
XGE_API int xgeImeEventGet(xge_ime_event_t* pEvent);
XGE_API int xgeTouchGetCount(void);
XGE_API int xgeTouchGet(int iIndex, xge_touch_point_t* pPoint);
XGE_API int xgeTouchFind(uint64_t iId, xge_touch_point_t* pPoint);
XGE_API int xgeGamepadConnected(int iGamepad);
XGE_API int xgeGamepadGetState(int iGamepad, xge_gamepad_state_t* pState);
XGE_API int xgeGamepadButtonDown(int iGamepad, uint32_t iButton);
XGE_API int xgeGamepadButtonPressed(int iGamepad, uint32_t iButton);
XGE_API int xgeGamepadButtonReleased(int iGamepad, uint32_t iButton);
XGE_API float xgeGamepadAxis(int iGamepad, int iAxis);
XGE_API int xgeGamepadSetConnected(int iGamepad, int bConnected);
XGE_API int xgeGamepadSetState(int iGamepad, const xge_gamepad_state_t* pState);


#ifdef __cplusplus
}
#endif

#endif /* XGE_H */
