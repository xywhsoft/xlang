#ifndef XGE_H
#define XGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "xge_config.h"
#include "extlibs/xrt/xrt_config.h"
#include "extlibs/xrt/xrt.h"

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define XGE_VERSION_MAJOR	3
#define XGE_VERSION_MINOR	0
#define XGE_VERSION_PATCH	0
#define XGE_FOUNDATION_API_REVISION 1u
/* Generated SDKs supply a public-header/configuration fingerprint. */
#ifndef XGE_BUILD_ABI_FINGERPRINT
#define XGE_BUILD_ABI_FINGERPRINT UINT64_C(0)
#endif
typedef struct xge_build_info_t {
    uint32_t size, api_major, api_minor, api_patch, foundation_revision, features;
    uint64_t abi_fingerprint;
} xge_build_info_t;

#if defined(XGE_BUILD_API_MAJOR) && XGE_BUILD_API_MAJOR != XGE_VERSION_MAJOR
#error "XGE headers and prebuilt SDK have different API versions; rebuild or select the matching SDK"
#endif

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
#define XGE_KEY_INSERT		260
#define XGE_KEY_DELETE		261
#define XGE_KEY_RIGHT		262
#define XGE_KEY_LEFT		263
#define XGE_KEY_DOWN		264
#define XGE_KEY_UP			265
#define XGE_KEY_PAGE_UP		266
#define XGE_KEY_PAGE_DOWN	267
#define XGE_KEY_HOME		268
#define XGE_KEY_END			269
#define XGE_KEY_F1			290
#define XGE_KEY_F2			291
#define XGE_KEY_F3			292
#define XGE_KEY_F4			293
#define XGE_KEY_F5			294
#define XGE_KEY_F6			295
#define XGE_KEY_F7			296
#define XGE_KEY_F8			297
#define XGE_KEY_F9			298
#define XGE_KEY_F10			299
#define XGE_KEY_F11			300
#define XGE_KEY_F12			301
#define XGE_KEY_F13			302
#define XGE_KEY_F14			303
#define XGE_KEY_F15			304
#define XGE_KEY_F16			305
#define XGE_KEY_F17			306
#define XGE_KEY_F18			307
#define XGE_KEY_F19			308
#define XGE_KEY_F20			309
#define XGE_KEY_F21			310
#define XGE_KEY_F22			311
#define XGE_KEY_F23			312
#define XGE_KEY_F24			313
#define XGE_KEY_F25			314
#define XGE_KEY_MENU		348
#define XGE_KEY_LEFT_SHIFT	340
#define XGE_KEY_LEFT_CONTROL	341
#define XGE_KEY_LEFT_ALT	342
#define XGE_KEY_LEFT_SUPER	343
#define XGE_KEY_RIGHT_SHIFT	344
#define XGE_KEY_RIGHT_CONTROL	345
#define XGE_KEY_RIGHT_ALT	346
#define XGE_KEY_RIGHT_SUPER	347
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
/* Engine-owned storage metadata; public image coordinates remain top-left. */
#define XGE_TEXTURE_ORIGIN_BOTTOM_LEFT 0x0004
#define XGE_TEXTURE_RENDER_TARGET      0x0008
#define XGE_TEXTURE_COORDINATE_CONTRACT 2
/* Texture creation policy; zero/default selects cheap, exact channel packing.
 * These bits may also be combined with XGE_IMAGE_* in texture load flags. */
#define XGE_TEXTURE_COMPRESS_LOSSLESS 0x00000000u
#define XGE_TEXTURE_COMPRESS_NONE     0x10000000u
#define XGE_TEXTURE_COMPRESS_LOSSY    0x20000000u
#define XGE_TEXTURE_COMPRESS_MASK     0x30000000u

#define XGE_TEXTURE_STORAGE_RGBA8    1
#define XGE_TEXTURE_STORAGE_R8       2
#define XGE_TEXTURE_STORAGE_RG8      3
#define XGE_TEXTURE_STORAGE_RGB565   4
#define XGE_TEXTURE_STORAGE_RGB5_A1  5
#define XGE_TEXTURE_STORAGE_RGBA4    6
#define XGE_TEXTURE_STORAGE_YUV420P  7

/* Optional diagnostics; GPU bytes exclude driver padding and attachments.
 * Initialize iSize to sizeof(xge_texture_storage_info_t) before querying. */
typedef struct xge_texture_storage_info_t {
	uint32_t iSize;
	uint32_t iCompression;
	int iStorage;
	int bResident;
	uint64_t iGpuBytes;
	uint64_t iCpuBytes;
} xge_texture_storage_info_t;
#define XGE_FONT_FALLBACK			0x0001
#define XGE_FONT_SIZE_EM			0x0002
#define XGE_SOUND_FALLBACK			0x80000000u
#define XGE_SOUND_CLIP                  0x40000000u

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

/* Custom GLSL receives primary-native UVs. These helpers map those UVs to
 * independently oriented secondary samplers. Place after GLSL precision lines.
 * Uniforms are supplied automatically by xgeMaterialDraw / batch flush. */
#define XGE_GLSL_MATERIAL_UV \
	"uniform vec4 uTextureUVTransform;\n" \
	"uniform vec4 uTexture2UVTransform;\n" \
	"uniform vec4 uTexture3UVTransform;\n" \
	"vec2 xgeMaterialUV1(vec2 p){return p*uTextureUVTransform.xy+uTextureUVTransform.zw;}\n" \
	"vec2 xgeMaterialUV2(vec2 p){return p*uTexture2UVTransform.xy+uTexture2UVTransform.zw;}\n" \
	"vec2 xgeMaterialUV3(vec2 p){return p*uTexture3UVTransform.xy+uTexture3UVTransform.zw;}\n"
#define XGE_SHADER_DEFINE_MAX		8
#define XGE_SHADER_DEFINE_NAME_MAX	32

#define XGE_COORD_Y_DOWN	0
#define XGE_COORD_CENTER	1

#define XGE_CAMERA_ORTHO		0
#define XGE_CAMERA_PERSPECTIVE	1

#define XGE_DRAW_FLIP_X		0x0001
#define XGE_DRAW_FLIP_Y		0x0002
#define XGE_DRAW_SCREEN_SPACE	0x0004
/* Glyph-run painting only: keep fractional X origin; baseline still snaps. */
#define XGE_DRAW_TEXT_SUBPIXEL_X	0x0008
/* Explicit low-level/legacy escape hatch: source rectangles and UVs are native. */
#define XGE_DRAW_RAW_UV           0x0010
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
#define XGE_TEXT_SHAPE_EMOJI		0x0002
/* Shape one resolved directional item. Logical clusters remain ascending;
 * visual positions and cluster-leading caret advances are right-to-left. */
#define XGE_TEXT_SHAPE_RTL		0x0004
#define XGE_TEXT_SHAPE_DEFAULT		(XGE_TEXT_SHAPE_KERNING | XGE_TEXT_SHAPE_EMOJI)

#define XGE_GLYPH_POSITION_LINE_BREAK	0x0001

#define XGE_TEXT_ITEM_GLYPH		0
#define XGE_TEXT_ITEM_EMOJI		1

#define XGE_EMOJI_PRESENTATION_AUTO		0
#define XGE_EMOJI_PRESENTATION_COLOR		1
#define XGE_EMOJI_PRESENTATION_TEXT		2
#define XGE_EMOJI_PRESENTATION_DISABLED	3

#define XGE_EMOJI_LINE_STABLE	0
#define XGE_EMOJI_LINE_EXPAND	1

#define XGE_EMOJI_ENTRY_TEXT_DEFAULT	0x0001

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
#define XGE_EVENT_IME_CANDIDATE_START	21
#define XGE_EVENT_IME_CANDIDATE_UPDATE	22
#define XGE_EVENT_IME_CANDIDATE_END	23
#define XGE_EVENT_MOUSE_LEAVE		24
#define XGE_EVENT_WINDOW_FOCUS		25
#define XGE_EVENT_WINDOW_BLUR		26

#define XGE_CURSOR_ARROW			0
#define XGE_CURSOR_IBEAM			1
#define XGE_CURSOR_HAND			2
#define XGE_CURSOR_RESIZE_EW		3
#define XGE_CURSOR_RESIZE_NS		4
#define XGE_CURSOR_RESIZE_NESW	5
#define XGE_CURSOR_RESIZE_NWSE	6
#define XGE_CURSOR_MOVE			7
#define XGE_CURSOR_NOT_ALLOWED		8

#define XGE_INPUT_EVENT_FLAG_REPEAT		0x00000001u
#define XGE_INPUT_EVENT_FLAG_NATIVE_IME	0x00000002u
#define XGE_INPUT_EVENT_FLAG_SYNTHETIC	0x00000004u

#define XGE_DRAG_EFFECT_NONE	0x00000000u
#define XGE_DRAG_EFFECT_COPY	0x00000001u
#define XGE_DRAG_EFFECT_MOVE	0x00000002u
#define XGE_DRAG_EFFECT_LINK	0x00000004u

#define XGE_DRAG_EVENT_NONE		0
#define XGE_DRAG_EVENT_ENTER		1
#define XGE_DRAG_EVENT_OVER		2
#define XGE_DRAG_EVENT_LEAVE		3
#define XGE_DRAG_EVENT_DROP		4
#define XGE_DRAG_EVENT_COMPLETE	5
#define XGE_DRAG_EVENT_CANCEL		6

#define XGE_DATA_FORMAT_TEXT_UTF8	"text/plain;charset=utf-8"
#define XGE_DATA_FORMAT_URI_LIST	"text/uri-list"
#define XGE_DATA_FORMAT_FILE_LIST	"application/x-xge-file-list;encoding=utf-8"
#define XGE_DATA_FORMAT_HTML		"text/html"

/* Windows IME presentation levels. Other platforms may expose only NATIVE. */
#define XGE_IME_MODE_NATIVE		0
#define XGE_IME_MODE_COMPOSITION	1
#define XGE_IME_MODE_FULL		2

/* Native IME display attribute for the current marked-text range. */
#define XGE_IME_ATTRIBUTE_NONE			0
#define XGE_IME_ATTRIBUTE_INPUT			1
#define XGE_IME_ATTRIBUTE_TARGET_CONVERTED	2
#define XGE_IME_ATTRIBUTE_CONVERTED		3
#define XGE_IME_ATTRIBUTE_TARGET_NOT_CONVERTED	4
#define XGE_IME_ATTRIBUTE_INPUT_ERROR		5
#define XGE_IME_ATTRIBUTE_FIXED_CONVERTED	6
#define XGE_IME_ATTRIBUTE_OTHER			7



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
/* 成功。 */
	XGE_OK = 0,
/* 通用失败。 */
	XGE_ERROR = -1,
/* 参数非法（空指针/越界/零尺寸）。 */
	XGE_ERROR_INVALID_ARGUMENT = -2,
/* 引擎或对象尚未初始化。 */
	XGE_ERROR_NOT_INITIALIZED = -3,
/* 重复初始化。 */
	XGE_ERROR_ALREADY_INITIALIZED = -4,
/* 内存分配失败。 */
	XGE_ERROR_OUT_OF_MEMORY = -5,
/* 文件不存在。 */
	XGE_ERROR_FILE_NOT_FOUND = -6,
/* 能力或平台不支持。 */
	XGE_ERROR_UNSUPPORTED = -7,
/* 平台后端操作失败。 */
	XGE_ERROR_BACKEND_FAILED = -8,
/* GPU/驱动操作失败。 */
	XGE_ERROR_GPU_FAILED = -9,
/* 资源加载或解析失败。 */
	XGE_ERROR_RESOURCE_FAILED = -10,
/* 音频操作失败。 */
	XGE_ERROR_AUDIO_FAILED = -11,
/* 线程操作失败。 */
	XGE_ERROR_THREAD_FAILED = -12,
/* 输出缓冲不足（API 同时输出所需大小）。 */
	XGE_ERROR_BUFFER_TOO_SMALL = -13,
/* 目标条目不存在。 */
	XGE_ERROR_NOT_FOUND = -14,
/* 当前状态不允许该操作。 */
	XGE_ERROR_INVALID_STATE = -15
} xge_result_t;

typedef struct xge_error_info_t {
	uint32_t iSize;
	int iCode;
	int bRecoverable;
	const char* sSubsystem;
	const char* sOperation;
	const char* sMessage;
} xge_error_info_t;

typedef void (*xge_error_proc)(const xge_error_info_t* pError, void* pUser);

typedef struct xge_desc_t {
/* 窗口客户区初始宽度（像素）。 */
	int iWidth;
/* 窗口客户区初始高度（像素）。 */
	int iHeight;
/* 窗口标题（UTF-8）。 */
	const char* sTitle;
/* XGE_INIT_* 组合；有效位 HIGHDPI/VSYNC/ON_DEMAND；FULLSCREEN/NOFRAME/RESIZABLE/OFFSCREEN 当前为占位未实现。 */
	int iFlags;
/* 运行模式：XGE_RUN_GAME_LOOP（默认）或 XGE_RUN_MANUAL 手动驱动。 */
	int iRunMode;
/* 目标帧率上限；0 表示不限制。 */
	int iTargetFPS;
/* 外部原生窗口句柄；NULL 时由引擎自建窗口。 */
	void* pNativeWindow;
/* 用户指针，透传到帧回调。 */
	void* pUser;
} xge_desc_t;

typedef struct xge_rect_t {
	float fX;
	float fY;
	float fW;
	float fH;
} xge_rect_t;

/* Integer framebuffer-pixel geometry. Rectangles use half-open bounds:
 * [iX, iX + iW) x [iY, iY + iH). */
typedef struct xge_point_i_t {
	int iX;
	int iY;
} xge_point_i_t;

typedef struct xge_size_i_t {
	int iW;
	int iH;
} xge_size_i_t;

typedef struct xge_rect_i_t {
	int iX;
	int iY;
	int iW;
	int iH;
} xge_rect_i_t;

typedef struct xge_edges_i_t {
	int iLeft;
	int iTop;
	int iRight;
	int iBottom;
} xge_edges_i_t;

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
typedef struct xge_emoji_pack_t xge_emoji_pack_t;
typedef xge_emoji_pack_t* xge_emoji_pack;
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
	int iLocTextureUVTransform[3];
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

/* NativeUV = LogicalUV * scale + offset. Logical (0,0) is the image top-left. */
typedef struct xge_texture_uv_transform_t {
	float fScaleU, fScaleV, fOffsetU, fOffsetV;
} xge_texture_uv_transform_t;

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
	int bPrevClipEnabled;
	xge_rect_t tPrevClipRect;
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
/* 宿主画布对象（平台透传）。 */
	void* pCanvas;
/* 宿主 WebGL2 上下文。 */
	void* pWebGLContext;
/* 画布逻辑宽度。 */
	int iWidth;
/* 画布逻辑高度。 */
	int iHeight;
/* 设备像素比（逻辑坐标到物理像素）。 */
	float fDevicePixelRatio;
/* 用户指针透传。 */
	void* pUser;
} xge_miniprogram_desc_t;

typedef struct xge_miniprogram_touch_t {
	uint64_t iId;
	/* Window coordinates; XGE converts them to framebuffer pixels using DPR. */
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
/* 原生显示句柄（EGL NativeDisplayType）。 */
	void* pNativeDisplay;
/* 原生窗口句柄；离屏模式可为 NULL。 */
	void* pNativeWindow;
/* 目标表面宽度（像素）。 */
	int iWidth;
/* 目标表面高度（像素）。 */
	int iHeight;
/* 使用 PBuffer 离屏表面。 */
	int bPBuffer;
/* 使用 Surfaceless 离屏（无表面）。 */
	int bSurfaceless;
/* 嵌入式 Linux 板级模式。 */
	int bBoardLinux;
/* 用户指针透传。 */
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

typedef struct xge_audio_clip xge_audio_clip;
typedef struct xge_audio_clip_info_t {
    uint32_t channels, sample_rate;
    uint64_t frames, bytes;
} xge_audio_clip_info_t;
typedef struct xge_audio_clip_decode_t {
    const void *data;
    size_t bytes;
    uint64_t max_decoded_bytes; /* 0: 64 MiB; maximum 512 MiB. */
    int (*cancelled)(void *user); /* Optional; polled between decoder reads. */
    void *user;
} xge_audio_clip_decode_t;
typedef struct xge_sound_clip_desc_t {
    xge_audio_clip *clip;
    uint64_t first_frame, frame_count; /* 0 count: remainder of clip. */
    int spatial; /* 0 disables engine spatialization. */
    xge_audio_group group; /* Borrowed; must outlive the sound. */
} xge_sound_clip_desc_t;

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
	/* Private worker result. It is consumed and freed by xgeAsyncPoll(). */
	void* pPayload;
};

typedef struct xge_font_t xge_font_t;
typedef xge_font_t* xge_font;
typedef struct xge_font_face_t* xge_font_face;
typedef struct xge_font_family_t* xge_font_family;

typedef struct xge_font_face_desc_t {
/* 结构体大小（sizeof，ABI 校验用）；置零初始化后填写。 */
	uint32_t iSize;
/* TTC 集合内的字体索引（单文件为 0）。 */
	int iFaceIndex;
/* 可选加载标志位。 */
	uint32_t iFlags;
} xge_font_face_desc_t;

typedef struct xge_font_instance_desc_t {
/* 结构体大小（sizeof，ABI 校验用）。 */
	uint32_t iSize;
/* 像素字号。 */
	float fPixelSize;
/* 缩放/渲染标志位。 */
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
	uint32_t iClusterEnd;
	uint32_t iItemKind;
	uint32_t iEmojiId;
	float fEmojiWidth;
	float fEmojiHeight;
	float fVisualX; /* Physical pen X within its line, before glyph offsets. */
} xge_glyph_position_t;

/* Interior Unicode-grapheme stops, measured from their cluster's leading edge.
 * OpenType GDEF positions are preferred; otherwise stops divide its advance
 * equally. UTF-8 offsets are in strictly increasing logical source order. */
typedef struct xge_glyph_caret_t {
	uint32_t iTextOffset;
	float fAdvance;
} xge_glyph_caret_t;

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
	int iCaretCount;
	xge_glyph_caret_t* pCarets; /* Owned by this run; GlyphRunFree releases it. */
} xge_glyph_run_t;

typedef struct xge_text_paint_span_t {
	uint32_t iSize;
	int iStart;
	int iEnd;
	uint32_t iColor;
} xge_text_paint_span_t;

typedef struct xge_text_shape_desc_t {
/* 结构体大小（sizeof，ABI 校验用）。 */
	uint32_t iSize;
/* 用于整形的字体实例。 */
	xge_font pFont;
/* UTF-8 文本起点。 */
	const char* sText;
/* 文本字节长度；-1 为 NUL 结尾，0 表示空片段。 */
	int iTextSize;
/* XGE_TEXT_SHAPE_KERNING/EMOJI/RTL 组合。 */
	uint32_t iFlags;
/* emoji 包；NULL 用全局默认。 */
	xge_emoji_pack pEmojiPack;
/* emoji 展示策略（文本/彩色）。 */
	int iEmojiPresentation;
/* emoji 与文本基线对齐策略。 */
	int iEmojiLinePolicy;
/* emoji 相对字号缩放。 */
	float fEmojiScale;
/* 有效的完整段落 UTF-8 上下文；NULL 使用 sText。本次调用借用，不保存在输出中。 */
	const char* sContext;
/* 上下文字节数；-1 为 NUL 结尾。提供上下文时须包含当前片段的原字节。 */
	int iContextSize;
/* 当前片段在上下文内的字节偏移；返回的 cluster/caret 仍以片段起点为零。 */
	int iContextOffset;
/* 已解析的 ISO 15924 脚本 tag（四个 ASCII 字母，大端）；0 从完整上下文解析。 */
	uint32_t iScript;
/* NUL 结尾的 BCP 47 tag，最多 255 字节；校验语法而非注册表。NULL 使用 und。 */
	const char* sLanguage;
} xge_text_shape_desc_t;

typedef struct xge_emoji_metrics_t {
	uint32_t iSize;
	float fAdvanceEm;
	float fWidthEm;
	float fHeightEm;
	float fBaselineRatio;
	uint32_t iFlags;
} xge_emoji_metrics_t;

typedef struct xge_emoji_match_t {
	uint32_t iSize;
	uint32_t iEmojiId;
	int iTextSize;
	xge_emoji_metrics_t tMetrics;
} xge_emoji_match_t;

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

typedef struct xge_data_object_t xge_data_object_t;
typedef xge_data_object_t* xge_data_object;

typedef int (*xge_data_provider_proc)(const char* sFormat, void* pOutput,
	size_t iCapacity, size_t* pOutputSize, void* pUser);
typedef void (*xge_data_provider_free_proc)(void* pUser);

typedef struct xge_drag_drop_caps_t {
	uint32_t iSize;
	int bNativeDragIn;
	int bNativeDragOut;
	int bCustomFormats;
	int bInternalOnly;
} xge_drag_drop_caps_t;

typedef struct xge_drag_event_t {
	uint32_t iSize;
	int iType;
	uint64_t iSequence;
	double fTime;
	float fX;
	float fY;
	uint32_t iModifiers;
	uint32_t iAllowedEffects;
	uint32_t iSuggestedEffect;
	uint32_t iEffect;
	xge_data_object pData;
} xge_drag_event_t;

typedef uint32_t (*xge_drag_event_proc)(const xge_drag_event_t* pEvent,
	void* pUser);

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
	int bReplacementRange;
	int iReplacementStart;
	int iReplacementEnd;
	int iAttribute;
} xge_ime_event_t;

/*
 * Ordered input event delivered on the platform UI thread.
 * Events keep their native arrival order and are consumed in the same frame.
 * Pointer fX/fY and unlocked pointer fDX/fDY use framebuffer pixels, matching
 * xgeGetWidth()/xgeGetHeight(). Wheel fDX/fDY remain device scroll units.
 * iModifiers is the event-time XGE_KEY_MOD_* snapshot for key, text, mouse,
 * and native touch events. Sources without modifier support set it to zero.
 * Synthetic pointer events posted by xgeInputEventPost use the same contract.
 * sText remains valid until the next xgeInputEventGet call or xgeUnit.
 */
typedef struct xge_input_event_t {
	uint32_t iSize;
	int iType;
	uint32_t iFlags;
	uint64_t iSequence;
	double fTime;
	int iKey;
	int iNativeKey;
	int iScanCode;
	uint32_t iModifiers;
	uint64_t iPointerId;
	int iButton;
	uint32_t iButtons;
	float fX;
	float fY;
	float fDX;
	float fDY;
	uint32_t iCodepoint;
	const char* sText;
	int iTextSize;
	int iCursor;
	int iSelectStart;
	int iSelectEnd;
	int bReplacementRange;
	int iReplacementStart;
	int iReplacementEnd;
	int iAttribute;
} xge_input_event_t;

/*
 * Snapshot supplied by the focused text control to the native IME backend.
 * Text and selection offsets use UTF-8 bytes. The backend copies the data
 * during onSnapshot; the pointers do not need to remain valid afterwards.
 */
typedef struct xge_ime_text_snapshot_t {
	uint32_t iSize;
	const char* sText;
	int iTextSize;
	int iSelectionStart;
	int iSelectionEnd;
	int iDocumentOffset;
	int iDocumentSize;
	uint32_t iRevision;
} xge_ime_text_snapshot_t;

typedef int (*xge_ime_text_snapshot_proc)(void* pUser, xge_ime_text_snapshot_t* pSnapshot);

typedef struct xge_ime_text_client_t {
	uint32_t iSize;
	xge_ime_text_snapshot_proc onSnapshot;
	void* pUser;
} xge_ime_text_client_t;

typedef struct xge_ime_candidate_info_t {
	uint32_t iSize;
	int bVisible;
	int bCanSelect;
	int iCount;
	int iSelection;
	int iPageStart;
	int iPageSize;
} xge_ime_candidate_info_t;

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

/* 填充引擎全局描述并完成预初始化；此时不创建窗口（窗口在 xgeRun 创建）。 */
XGE_API int xgeInit(const xge_desc_t* pDesc);
/* Compiled feature bits; compare with XGE_FEATURES in the consumer. */
XGE_API uint32_t xgeGetBuildFeatures(void);
/* Set size=sizeof(*out). A zero expected fingerprint checks features only. */
XGE_API int xgeGetBuildInfo(xge_build_info_t *out);
XGE_API int xgeCheckBuildCompatibility(uint64_t fingerprint, uint32_t features);
/* 关闭引擎并释放全局资源（主循环结束后调用）。 */
XGE_API void xgeUnit(void);
/* 释放引擎 API 输出的堆块（如 xgeFontBuildXRFMemory 的产物）。 */
XGE_API void xgeMemoryFree(void* pData);
/* 创建窗口并进入主循环；每帧回调 onProc，直到退出。 */
XGE_API int xgeRun(xge_scene_proc procFrame, void* pUser);
/* 立即退出主循环。 */
XGE_API void xgeQuit(void);
/* Native close and xgeRequestQuit call this on the UI thread. Return zero to
 * veto/defer (for example, while an asynchronous save confirmation is open).
 * xgeQuit remains unconditional and bypasses this callback. */
typedef int (*xge_quit_request_proc)(void* pUser);
/* 注册退出请求回调：返回 0 阻止退出，返回非 0 允许退出。xgeQuit 不经过此回调。 */
XGE_API void xgeSetQuitRequestCallback(xge_quit_request_proc proc, void* pUser);
/* 请求退出主循环（触发退出请求回调）。 */
XGE_API int xgeRequestQuit(void);
/* 请求渲染下一帧（on-demand 模式下唤醒帧循环）。 */
XGE_API void xgeRenderRequest(void);
/* Requests an on-demand frame after at most fDelaySeconds. Main-thread only. */
XGE_API void xgeRenderRequestAfter(float fDelaySeconds);
/* 手动模式：驱动一帧（处理事件+更新+绘制+呈现）。 */
XGE_API int xgeFrame(void);
/* 手动模式轮询运行状态：返回 1 运行中 / 0 已退出；未初始化或非手动模式返回错误。 */
XGE_API int xgeRender(void);
/* 清零帧统计。 */
XGE_API void xgeFrameStatsReset(void);
/* 输出帧统计（帧时间/绘制次数/批次数）。 */
XGE_API xge_frame_stats_t xgeFrameStatsGet(void);
#if XGE_HAS_DEBUGMODE
/* Debug APIs are only declared for xgedbg builds.
 * Build with -DXGE_DEBUGMODE=1 and link xgedbg instead of xge. */
XGE_API int xgeDebugGetStats(xge_debug_stats_t* pStats);
/* Writes a NUL-terminated capability report. Returns XGE_OK, or
 * XGE_ERROR_BUFFER_TOO_SMALL after writing a truncated NUL-terminated value. */
XGE_API int xgeDebugDumpCaps(char* sBuffer, int iSize);
#endif
/* 设置日志级别（TRACE..OFF）。 */
XGE_API int xgeLogSetLevel(int iLevel);
/* 读取日志级别。 */
XGE_API int xgeLogGetLevel(void);
/* 写一条日志（输出到 stderr）。 */
XGE_API int xgeLogWrite(int iLevel, const char* sTag, const char* sMessage);
/* 冲刷日志缓冲。 */
XGE_API int xgeLogFlush(void);
/* 注册错误回调（带重入保护）。 */
XGE_API int xgeSetErrorCallback(xge_error_proc onError, void* pUser);
/* 主动上报一条错误（走错误回调链）。 */
XGE_API int xgeReportError(const xge_error_info_t* pError);
/* 取默认平台后端描述（Sokol）。 */
XGE_API xge_platform_backend_t xgePlatformBackendDefault(void);
/* 注入平台后端描述。 */
XGE_API int xgePlatformBackendSet(const xge_platform_backend_t* pBackend);
/* 取当前平台后端描述。 */
XGE_API xge_platform_backend_t xgePlatformBackendGet(void);
/* 取默认图形后端（GLCore/GLES3/WebGL2 按平台）。 */
XGE_API xge_graphics_backend_t xgeGraphicsBackendDefault(void);
/* 注入图形后端描述。 */
XGE_API int xgeGraphicsBackendSet(const xge_graphics_backend_t* pBackend);
/* 取当前图形后端描述。 */
XGE_API xge_graphics_backend_t xgeGraphicsBackendGet(void);
/* 输出 GPU 能力（后端/版本/上限）。 */
XGE_API int xgeGpuCapsGet(xge_gpu_caps_t* pCaps);
/* 输出平台能力（窗口/剪贴板/IME/手柄等位）。 */
XGE_API int xgePlatformCapsGet(xge_platform_caps_t* pCaps);
/* 输出平台运行时信息（后端/目标/驱动版本）。 */
XGE_API int xgePlatformRuntimeGet(xge_platform_runtime_t* pRuntime);
/* Borrowed backend-native handle for the primary XGE window. The concrete
 * type is platform-specific; callers own all casts and native operations. */
XGE_API void* xgePlatformNativeHandle(void);
/* 输出拖放能力位（拖出/多格式等，多能力仅 Windows）。 */
XGE_API int xgeDragDropCapsGet(xge_drag_drop_caps_t* pCaps);
/* 创建多格式数据对象（拖放/剪贴板载荷）。 */
XGE_API int xgeDataObjectCreate(xge_data_object* ppData);
/* 数据对象引用计数加一。 */
XGE_API int xgeDataObjectAddRef(xge_data_object pData);
/* 数据对象引用计数减一，归零时释放。 */
XGE_API void xgeDataObjectRelease(xge_data_object pData);
/* 以立即值写入指定格式数据（内部复制）。 */
XGE_API int xgeDataObjectSet(xge_data_object pData, const char* sFormat,
	const void* pValue, size_t iSize);
/* 以惰性回调提供指定格式数据（用到才读取）。 */
XGE_API int xgeDataObjectSetProvider(xge_data_object pData,
	const char* sFormat, xge_data_provider_proc onRead,
	xge_data_provider_free_proc onFree, void* pUser);
/* 读取格式数量。 */
XGE_API int xgeDataObjectFormatCount(xge_data_object pData);
/* 按索引取格式名。 */
XGE_API const char* xgeDataObjectFormatAt(xge_data_object pData, int iIndex);
/* 是否包含指定格式。 */
XGE_API int xgeDataObjectHas(xge_data_object pData, const char* sFormat);
/* 读指定格式数据；iCapacity 不足时仅输出所需大小。 */
XGE_API int xgeDataObjectGet(xge_data_object pData, const char* sFormat,
	void* pOutput, size_t iCapacity, size_t* pOutputSize);
/* 注册拖放事件回调（进入/悬停/放下/离开）。 */
XGE_API int xgeDragEventCallbackSet(xge_drag_event_proc onEvent, void* pUser);
/* Dispatches a platform drag event synchronously. Custom window backends use
 * this entry point so effect negotiation completes before returning to the OS. */
XGE_API uint32_t xgeDragEventDispatch(const xge_drag_event_t* pEvent);
/* 发起拖出会话（完整 OLE 拖出仅 Windows；其他平台仅内部会话）。 */
XGE_API int xgeDragBegin(xge_data_object pData, uint32_t iAllowedEffects,
	uint32_t iSuggestedEffect);
/* 取消当前拖放会话。 */
XGE_API int xgeDragCancel(void);
/* 是否有拖放会话进行中。 */
XGE_API int xgeDragIsActive(void);
/* Writes a NUL-terminated string. Returns XGE_OK, or XGE_ERROR_BUFFER_TOO_SMALL
 * after writing a truncated NUL-terminated value. */
XGE_API int xgeGraphicsShaderHeaderGet(int iBackend, char* sBuffer, int iSize);
/* 输出指定后端的 GL 库候选名（按优先序）。 */
XGE_API int xgeGraphicsLibraryNameGet(int iBackend, int iIndex, char* sBuffer, int iSize);
/* 输出后端的能力映射表（纹理格式/功能位）。 */
XGE_API int xgeGraphicsMappingGet(int iBackend, xge_graphics_mapping_t* pMapping);
/* 直接切换当前场景（不压栈）。 */
XGE_API int xgeSceneSet(xge_scene pScene);
/* 压栈并切换场景（可 xgeScenePop 返回）。 */
XGE_API int xgeScenePush(xge_scene pScene);
/* 弹出栈顶场景并恢复上一场景。 */
XGE_API int xgeScenePop(void);
/* 以新场景替换栈顶。 */
XGE_API int xgeSceneReplace(xge_scene pScene);
/* 取当前场景；无场景返回 NULL。 */
XGE_API xge_scene xgeSceneCurrent(void);
/* 读取场景栈深度。 */
XGE_API int xgeSceneCount(void);
/* 向当前场景分发输入事件。 */
XGE_API int xgeSceneDispatchEvent(const xge_event_t* pEvent);
/* 设置更新策略（可变步长或定步长 fFixedSeconds，iMaxUpdates 防螺旋）。 */
XGE_API int xgeSceneUpdateStrategySet(int iMode, float fFixedStep, int iMaxUpdates);
/* 读取当前更新策略参数。 */
XGE_API void xgeSceneUpdateStrategyGet(int* pMode, float* pFixedStep, int* pMaxUpdates);

/* 读取窗口客户区宽度（像素）。 */
XGE_API int xgeGetWidth(void);
/* 读取窗口客户区高度（像素）。 */
XGE_API int xgeGetHeight(void);
/* 设置系统光标形状。 */
XGE_API int xgeSetCursor(int iCursor);
/* 读取当前光标形状。 */
XGE_API int xgeGetCursor(void);
/* 读取上一帧到当前帧的时间（秒）。 */
XGE_API float xgeGetDelta(void);
/* 读取当前帧率。 */
XGE_API int xgeGetFPS(void);
/* 高精度计时器当前值（秒）。 */
XGE_API double xgeTimer(void);
/* 读取墙钟时间（秒）。 */
XGE_API int64_t xgeTimeNow(void);
/* 阻塞休眠指定秒数。 */
XGE_API void xgeSleep(uint32_t iMilliseconds);
/* 设置窗口标题。 */
XGE_API void xgeSetTitle(const char* sTitle);

/* 手动帧开始：进入帧上下文（配合 xgeEnd 使用）。 */
XGE_API int xgeBegin(void);
/* 手动帧结束：冲刷绘制并呈现。 */
XGE_API int xgeEnd(void);
/* 立即冲刷新全部绘制命令与自动批。 */
XGE_API int xgeFlush(void);
/* 查询渲染线程能力；当前恒报 bSupported=0（预留）。 */
XGE_API int xgeRenderThreadCapsGet(xge_render_thread_caps_t* pCaps);
/* 渲染线程 EGL 上下文设置；当前恒返回 XGE_ERROR_UNSUPPORTED（预留接口）。 */
XGE_API int xgeRenderThreadEGLSet(const xge_egl_desc_t* pDesc);
/* 关闭渲染线程真实生效；开启恒返回 XGE_ERROR_UNSUPPORTED（无后端提供持久共享上下文）。 */
XGE_API int xgeRenderThreadSet(int bEnabled);
/* 读取渲染线程开关状态（当前实现下开启恒失败，故恒为关闭）。 */
XGE_API int xgeRenderThreadGet(void);
/* 以颜色清屏（RGBA8）。 */
XGE_API void xgeClear(uint32_t iColor);
/* 呈现当前帧到屏幕（手动模式）。 */
XGE_API void xgePresent(void);
/* 登记脏矩形并请求按需渲染；当前仅记录统计，不驱动局部重绘。 */
XGE_API void xgeInvalidateRect(xge_rect_t tRect);
#if XGE_HAS_DEBUGMODE
/* Dirty rect inspection is a debug feature; xge keeps only the runtime
 * invalidation mechanism. Build with -DXGE_DEBUGMODE=1 and link xgedbg. */
XGE_API int xgedbgDirtyRectCount(void);
/* 读取调试脏矩形记录（仅 XGE_DEBUGMODE 编译）。 */
XGE_API int xgedbgDirtyRectGet(int iIndex, xge_rect_t* pRect);
/* 清空调试脏矩形记录（仅 XGE_DEBUGMODE 编译）。 */
XGE_API void xgedbgDirtyRectClear(void);
#endif
/* 按 r/g/b/a 分量合成颜色值。 */
XGE_API uint32_t xgeColorRGBA(int iR, int iG, int iB, int iA);
/* 把颜色值分解为 r/g/b/a 分量。 */
XGE_API xge_color_t xgeColorUnpack(uint32_t iColor);
/* 设置全局混合模式（NONE/ALPHA/ADD/MULTIPLY/SCREEN/DARKEN/LIGHTEN）：
 * 注意：OVERLAY..LUMINOSITY 等高级模式当前仅告警并忽略，高级合成仅在 ShapeEx 内可用。 */
XGE_API void xgeBlendSet(int iBlend);
/* 读取全局混合模式。 */
XGE_API int xgeBlendGet(void);
/* 开关深度测试。 */
XGE_API void xgeDepthTestSet(int bEnabled);
/* 读取深度测试开关。 */
XGE_API int xgeDepthTestGet(void);
/* 重置为默认 2D 正交相机（Y 轴向下，原点左上）。 */
#if XGE_ENABLE_2D
XGE_API xge_camera_t xgeCameraDefault(float fWidth, float fHeight);
/* 设置透视相机（2.5D 顶点经 CPU 投影后绘制）。 */
XGE_API xge_camera_t xgeCameraPerspective(float fWidth, float fHeight, float fFovY, float fNearZ, float fFarZ);
/* 设置相机（正交或透视）。 */
XGE_API void xgeCameraSet(const xge_camera_t* pCamera);
/* 读取当前相机参数。 */
XGE_API xge_camera_t xgeCameraGet(void);
/* 世界坐标转屏幕坐标（按当前相机）。 */
XGE_API xge_vec2_t xgeWorldToScreen(xge_vec2_t tPoint);
/* 屏幕坐标转世界坐标（按当前相机）。 */
XGE_API xge_vec2_t xgeScreenToWorld(xge_vec2_t tPoint);
#endif
/* 注册资源 scheme provider（最多 8 个，自定义 URI 解析）。 */
XGE_API int xgeResourceProviderAdd(const xge_resource_provider_t* pProvider);
/* 移除全部自定义 provider。 */
XGE_API void xgeResourceProviderClear(void);
/* 注册 xpack 打包资源 provider（res:// 打包路径）。 */
XGE_API int xgeResourceXPackProviderAdd(const xge_xpack_provider_t* pProvider);
/* 按 URI 加载资源（依次查询 provider 链）。 */
XGE_API int xgeResourceLoad(const char* sURI, xge_resource_t* pResource);
/* 包装内存块为资源对象（不复制，调用方保证生命周期）。 */
XGE_API int xgeResourceLoadMemory(const void* pData, int iSize, xge_resource_t* pResource);
/* 释放资源对象。 */
XGE_API void xgeResourceFree(xge_resource_t* pResource);
/* 解压 zstd 数据块（引擎内建解压器）。 */
XGE_API int xgeZstdDecompress(void* pOutput, int iOutputCapacity, const void* pInput, int iInputSize, int* pOutputSize);
/* 初始化微信小程序后端（切 WebGL2，由宿主 JS 驱动帧）。 */
XGE_API int xgeMiniProgramInit(const xge_miniprogram_desc_t* pDesc);
/* 以画布尺寸与 DPR 简化初始化小程序后端。 */
XGE_API int xgeMiniProgramInitSimple(int iWidth, int iHeight, float fDevicePixelRatio);
/* 释放小程序后端。 */
XGE_API void xgeMiniProgramUnit(void);
/* 注册宿主 JS 桥回调（request_frame/load_resource/audio 等）。 */
XGE_API int xgeMiniProgramSetBridge(const xge_miniprogram_bridge_t* pBridge);
/* fTimeSeconds is a finite, monotonic host timestamp in seconds. */
XGE_API int xgeMiniProgramFrame(double fTimeSeconds);
/* 通知画布尺寸/像素比变更。 */
XGE_API int xgeMiniProgramResize(int iWidth, int iHeight, float fDevicePixelRatio);
/* 批量注入触摸事件。 */
XGE_API int xgeMiniProgramTouch(int iPhase, const xge_miniprogram_touch_t* pTouches, int iCount);
/* 注入单个触摸点事件。 */
XGE_API int xgeMiniProgramTouchOne(int iPhase, int iId, float fX, float fY, float fForce);
/* 注入文本输入码点。 */
XGE_API int xgeMiniProgramText(uint32_t iCodepoint);
/* 请求宿主调度下一帧。 */
XGE_API int xgeMiniProgramRequestFrame(void);
/* 音频指令转发（实际播放由宿主 JS 侧执行）。 */
XGE_API int xgeMiniProgramAudioCommand(int iCommand, int iHandle, const void* pData, int iSize);
/* 输出 EGL 能力信息。 */
XGE_API int xgeEGLCapsGet(xge_egl_caps_t* pCaps);
/* 初始化 EGL 离屏上下文（PBuffer/Surfaceless）；预留：当前无构建脚本启用。 */
XGE_API int xgeEGLInit(xge_egl_context_t* pContext, const xge_egl_desc_t* pDesc);
/* 释放 EGL 上下文。 */
XGE_API void xgeEGLUnit(xge_egl_context_t* pContext);
/* 绑定 EGL 上下文到当前线程。 */
XGE_API int xgeEGLMakeCurrent(xge_egl_context_t* pContext);
/* 初始化离屏渲染上下文（自带目标尺寸）。 */
XGE_API int xgeOffscreenInit(xge_offscreen pOffscreen, int iWidth, int iHeight);
/* 释放离屏渲染上下文。 */
XGE_API void xgeOffscreenUnit(xge_offscreen pOffscreen);
/* 取离屏上下文的渲染目标。 */
XGE_API xge_render_target xgeOffscreenRenderTarget(xge_offscreen pOffscreen);
/* 读回离屏渲染结果像素。 */
XGE_API int xgeOffscreenReadPixels(xge_offscreen pOffscreen, void* pPixels, int iStride);
/* 异步请求重置为空。 */
XGE_API void xgeAsyncRequestInit(xge_async_request pRequest);
/* 释放请求（未完成请求会先取消）。 */
XGE_API void xgeAsyncRequestFree(xge_async_request pRequest);
/* 取消未完成请求（回调不再触发）。 */
XGE_API int xgeAsyncRequestCancel(xge_async_request pRequest);
/* Enables worker decoding for file-backed IMAGE and TEXTURE requests only.
 * Requests handled by a registered resource provider, and FONT/SOUND requests,
 * are completed synchronously on the caller thread. Completion callbacks may
 * release their request; no request field may be read after the callback. */
XGE_API int xgeAsyncThreadingSet(int bEnabled);
/* 读取异步线程开关。 */
XGE_API int xgeAsyncThreadingGet(void);
/* Returns an XGE_ASYNC_* status. A completion callback may release pRequest. */
XGE_API int xgeAsyncPoll(xge_async_request pRequest);
/* 异步解码图片（可走工作线程）。 */
XGE_API int xgeAsyncImageLoad(xge_async_request pRequest, xge_image pImage, const char* sPath, uint32_t iFlags, xge_async_proc onComplete, void* pUser);
/* 异步加载纹理（可走工作线程）。 */
XGE_API int xgeAsyncTextureLoad(xge_async_request pRequest, xge_texture pTexture, const char* sPath, uint32_t iFlags, xge_async_proc onComplete, void* pUser);
/* 异步加载字体；当前同步执行（仅图片类支持线程）。 */
#if XGE_ENABLE_TEXT
XGE_API int xgeAsyncFontLoad(xge_async_request pRequest, xge_font pFont, const char* sPath, float fSize, xge_async_proc onComplete, void* pUser);
#endif
/* 异步加载音效；当前同步执行（仅图片类支持线程）。 */
#if XGE_ENABLE_AUDIO
XGE_API int xgeAsyncSoundLoad(xge_async_request pRequest, xge_sound pSound, const char* sPath, xge_async_proc onComplete, void* pUser);
/* 初始化音频引擎（惰性设计：使用任何音频 API 前需手动调用一次）。 */
XGE_API int xgeAudioInit(void);
/* 关闭音频引擎并释放全部声音资源。 */
XGE_API void xgeAudioUnit(void);
/* 音频引擎是否已初始化。 */
XGE_API int xgeAudioIsReady(void);
/* Immutable interleaved float PCM. Decode/info/samples/ref/free need no window,
 * audio device or engine init and may run on CPU workers with owned references.
 * Decode copies its input; error sets *out=NULL. Cancellation reports INVALID_STATE. */
XGE_API int xgeAudioClipDecodeMemory(const xge_audio_clip_decode_t *desc,xge_audio_clip **out);
XGE_API xge_audio_clip_info_t xgeAudioClipInfo(const xge_audio_clip *clip);
XGE_API const float *xgeAudioClipSamples(const xge_audio_clip *clip); /* Borrowed. */
XGE_API int xgeAudioClipAddRef(xge_audio_clip *clip);
XGE_API void xgeAudioClipFree(xge_audio_clip *clip);
/* Audio control thread; sound must be zero initialized/freed. Independent
 * cursor/loop per sound, shared clip retained until sound free. No file IO,
 * fallback or device/global listener changes; trim range uses source PCM frames. */
XGE_API int xgeSoundFromClip(xge_sound sound,const xge_sound_clip_desc_t *desc);
/* 设置引擎总音量 [0,1]。 */
XGE_API void xgeAudioSetVolume(float fVolume);
/* 读取引擎总音量。 */
XGE_API float xgeAudioGetVolume(void);
/* 设置 3D 听者位置与朝向。 */
XGE_API void xgeAudioListenerSet(const xge_audio_listener_t* pListener);
/* 读取 3D 听者状态。 */
XGE_API xge_audio_listener_t xgeAudioListenerGet(void);
/* 创建音频组（组音量与淡入淡出）。 */
XGE_API int xgeAudioGroupInit(xge_audio_group pGroup);
/* 释放音频组。 */
XGE_API void xgeAudioGroupFree(xge_audio_group pGroup);
/* 设置音频组音量。 */
XGE_API void xgeAudioGroupSetVolume(xge_audio_group pGroup, float fVolume);
/* 读取音频组音量。 */
XGE_API float xgeAudioGroupGetVolume(xge_audio_group pGroup);
/* 在 iMilliseconds 毫秒内把组音量从 fFrom 渐变到 fTo。 */
XGE_API void xgeAudioGroupFade(xge_audio_group pGroup, float fFrom, float fTo, int iMilliseconds);
/* 加载音效文件（整块解码缓存）。 */
XGE_API int xgeSoundLoad(xge_sound pSound, const char* sPath);
/* 加载音效并挂到指定音频组。 */
XGE_API int xgeSoundLoadGroup(xge_sound pSound, const char* sPath, xge_audio_group pGroup);
/* 设置全局兜底音效（加载失败时替代）。 */
XGE_API int xgeSoundFallbackSet(const char* sPath);
/* 获取全局兜底音效。 */
XGE_API int xgeSoundFallbackGet(xge_sound pSound);
/* 清除全局兜底音效。 */
XGE_API void xgeSoundFallbackClear(void);
/* 触发音效播放（可重叠）。 */
XGE_API int xgeSoundPlay(xge_sound pSound);
/* 停止音效全部实例。 */
XGE_API int xgeSoundStop(xge_sound pSound);
/* 暂停音效。 */
XGE_API int xgeSoundPause(xge_sound pSound);
/* 恢复音效。 */
XGE_API int xgeSoundResume(xge_sound pSound);
/* 音效引用计数加一。 */
XGE_API int xgeSoundAddRef(xge_sound pSound);
/* 音效引用计数减一，归零时释放。 */
XGE_API void xgeSoundFree(xge_sound pSound);
/* 设置循环播放。 */
XGE_API void xgeSoundSetLoop(xge_sound pSound, int bLoop);
/* 设置音效音量。 */
XGE_API void xgeSoundSetVolume(xge_sound pSound, float fVolume);
/* 设置 3D 空间位置（配合听者衰减）。 */
XGE_API void xgeSoundSetPosition(xge_sound pSound, float fX, float fY, float fZ);
/* 在 iMilliseconds 毫秒内把音量从 fFrom 渐变到 fTo。 */
XGE_API void xgeSoundFade(xge_sound pSound, float fFrom, float fTo, int iMilliseconds);
/* 音效是否有实例在播。 */
XGE_API int xgeSoundIsPlaying(xge_sound pSound);
/* 加载音乐文件；实现与 xgeSoundLoad 同构（语义分层用）。 */
XGE_API int xgeMusicLoad(xge_music pMusic, const char* sPath);
/* 加载音乐并挂到指定音频组。 */
XGE_API int xgeMusicLoadGroup(xge_music pMusic, const char* sPath, xge_audio_group pGroup);
/* 播放音乐。 */
XGE_API int xgeMusicPlay(xge_music pMusic);
/* 停止音乐。 */
XGE_API int xgeMusicStop(xge_music pMusic);
/* 暂停音乐。 */
XGE_API int xgeMusicPause(xge_music pMusic);
/* 恢复音乐。 */
XGE_API int xgeMusicResume(xge_music pMusic);
/* 释放音乐句柄。 */
XGE_API void xgeMusicFree(xge_music pMusic);
/* 设置循环播放。 */
XGE_API void xgeMusicSetLoop(xge_music pMusic, int bLoop);
/* 设置音乐音量。 */
XGE_API void xgeMusicSetVolume(xge_music pMusic, float fVolume);
/* 在 iMilliseconds 毫秒内渐变音乐音量。 */
XGE_API void xgeMusicFade(xge_music pMusic, float fFrom, float fTo, int iMilliseconds);
/* 音乐是否在播。 */
XGE_API int xgeMusicIsPlaying(xge_music pMusic);
/* 以流式解码打开长音频（边播边解码，适合 BGM 长文件）。 */
XGE_API int xgeStreamOpen(xge_stream pStream, const char* sPath);
/* 流式打开并挂到指定音频组。 */
XGE_API int xgeStreamOpenGroup(xge_stream pStream, const char* sPath, xge_audio_group pGroup);
/* 播放流。 */
XGE_API int xgeStreamPlay(xge_stream pStream);
/* 停止流。 */
XGE_API int xgeStreamStop(xge_stream pStream);
/* 暂停流。 */
XGE_API int xgeStreamPause(xge_stream pStream);
/* 恢复流。 */
XGE_API int xgeStreamResume(xge_stream pStream);
/* 关闭并释放流。 */
XGE_API void xgeStreamClose(xge_stream pStream);
/* 设置循环播放。 */
XGE_API void xgeStreamSetLoop(xge_stream pStream, int bLoop);
/* 设置流音量。 */
XGE_API void xgeStreamSetVolume(xge_stream pStream, float fVolume);
/* 设置 3D 空间位置。 */
XGE_API void xgeStreamSetPosition(xge_stream pStream, float fX, float fY, float fZ);
/* 在 iMilliseconds 毫秒内渐变流音量。 */
XGE_API void xgeStreamFade(xge_stream pStream, float fFrom, float fTo, int iMilliseconds);
/* 流是否在播。 */
XGE_API int xgeStreamIsPlaying(xge_stream pStream);
#endif
/* 迭代解码下一个 UTF-8 码点并推进 *psText；非法序列返回错误。 */
#if XGE_ENABLE_TEXT
XGE_API int xgeTextUTF8Next(const char** psText, uint32_t* pCodepoint);
/* 从文件解析字体 face（读取度量与命名表）；pDesc 可为 NULL。 */
XGE_API int xgeFontFaceLoad(xge_font_face* ppFace, const char* sPath, const xge_font_face_desc_t* pDesc);
/* 从内存解析字体 face。 */
XGE_API int xgeFontFaceLoadMemory(xge_font_face* ppFace, const void* pData, int iSize, const xge_font_face_desc_t* pDesc);
/* face 引用计数加一。 */
XGE_API int xgeFontFaceAddRef(xge_font_face pFace);
/* face 引用计数减一，归零时释放。 */
XGE_API void xgeFontFaceFree(xge_font_face pFace);
/* 输出 face 静态信息（字重/宽度/斜体/家族名等）。 */
XGE_API int xgeFontFaceGetInfo(xge_font_face pFace, xge_font_face_info_t* pInfo);
/* 创建空字体族。 */
XGE_API int xgeFontFamilyCreate(xge_font_family* ppFamily);
/* 释放字体族（不释放其内 face）。 */
XGE_API void xgeFontFamilyFree(xge_font_family pFamily);
/* 向字体族添加候选 face。 */
XGE_API int xgeFontFamilyAddFace(xge_font_family pFamily, xge_font_face pFace);
/* 按字重解析最匹配的 face。 */
XGE_API int xgeFontFamilyResolve(xge_font_family pFamily, int iWeight, int iSlant, xge_font_face* ppFace);
/* 按字重/宽度/斜体打分解析最匹配的 face；无候选时返回 XGE_ERROR_RESOURCE_FAILED。 */
XGE_API int xgeFontFamilyResolveEx(xge_font_family pFamily, int iWeight, int iStretch, int iSlant, xge_font_face* ppFace);
/* 以 face 创建字体实例（em 或像素尺寸由 desc 指定）。 */
XGE_API int xgeFontCreate(xge_font pFont, xge_font_face pFace, const xge_font_instance_desc_t* pDesc);
/* 按像素尺寸克隆缩放现有实例；XRF 位图字体不支持缩放（XGE_ERROR_UNSUPPORTED）。 */
XGE_API int xgeFontCreateSized(xge_font pFont, xge_font pSource, float fPixelSize);
/* 一步加载 TTF/TTC 文件为指定像素尺寸的字体实例。 */
XGE_API int xgeFontLoad(xge_font pFont, const char* sPath, float fSize);
/* 从内存一步加载字体实例。 */
XGE_API int xgeFontLoadMemory(xge_font pFont, const void* pData, int iSize, float fSize);
/* 加载自研 XRF 位图字体文件。 */
XGE_API int xgeFontLoadXRF(xge_font pFont, const char* sPath);
/* 从内存加载 XRF 位图字体。 */
XGE_API int xgeFontLoadXRFMemory(xge_font pFont, const void* pData, int iSize);
/* 把 TTF 字形烘焙为 XRF 数据块；*ppData 由调用方持有并负责释放。 */
XGE_API int xgeFontBuildXRFMemory(xge_font pFont, uint32_t iFirstCodepoint, uint32_t iCount, void** ppData, int* pSize);
/* 烘焙指定码点区间的字形并写出 XRF 文件。 */
XGE_API int xgeFontSaveXRF(xge_font pFont, const char* sPath, uint32_t iFirstCodepoint, uint32_t iCount);
/* TTF+XRF 成对加载：XRF 有效时直接使用，否则回退解析 TTF。 */
XGE_API int xgeFontLoadCached(xge_font pFont, const char* sTTFPath, const char* sXRFPath, float fSize, uint32_t iFirstCodepoint, uint32_t iCount);
/* 字体实例引用计数加一。 */
XGE_API int xgeFontAddRef(xge_font pFont);
/* 字体实例引用计数减一，归零时释放。 */
XGE_API void xgeFontFree(xge_font pFont);
/* 设置当前字体实例的后备字体。 */
XGE_API void xgeFontSetFallback(xge_font pFont, xge_font pFallback);
/* 设置全局后备字体（缺字形时兜底）。 */
XGE_API int xgeFontFallbackSet(const char* sPath, float fSize);
/* 从内存设置全局后备字体。 */
XGE_API int xgeFontFallbackSetMemory(const void* pData, int iSize, float fSize);
/* 获取全局后备字体实例（无则返回 NULL）。 */
XGE_API int xgeFontFallbackGet(xge_font pFont, float fSize);
/* 按尺寸与标志获取全局后备字体实例，惰性创建。 */
XGE_API int xgeFontFallbackGetEx(xge_font pFont, float fSize, uint32_t iFlags);
/* 清除全局后备字体。 */
XGE_API void xgeFontFallbackClear(void);
/* 输出字体度量（行高/基线/升部降部等）。 */
XGE_API int xgeFontGetMetrics(xge_font pFont, xge_font_metrics_t* pMetrics);
/* 输出字形图集缓存统计。 */
XGE_API int xgeFontCacheGetStats(xge_font pFont, xge_font_cache_stats_t* pStats);
/* 清空字形图集缓存。 */
XGE_API void xgeFontCacheClear(xge_font pFont);
/* 按码点取字形度量（含步进宽度）；含后备链解析。 */
XGE_API int xgeFontGlyphGet(xge_font pFont, uint32_t iCodepoint, xge_glyph_metrics_t* pMetrics);
/* 按字形索引取字形度量（不走后备链）。 */
XGE_API int xgeFontGlyphGetByIndex(xge_font pFont, int iGlyph, xge_glyph_metrics_t* pMetrics);
/* 按码点光栅化字形位图；用后需 xgeGlyphBitmapFree 释放。 */
XGE_API int xgeFontGlyphRasterize(xge_font pFont, uint32_t iCodepoint, xge_glyph_bitmap_t* pBitmap);
/* 按字形索引光栅化位图；用后需 xgeGlyphBitmapFree 释放。 */
XGE_API int xgeFontGlyphRasterizeByIndex(xge_font pFont, int iGlyph, xge_glyph_bitmap_t* pBitmap);
/* 按码点经图集缓存取字形（返回图集子矩形，自动入图集）。 */
XGE_API int xgeFontGlyphAtlasGet(xge_font pFont, uint32_t iCodepoint, xge_glyph_t* pGlyph);
/* 按字形索引经图集缓存取字形。 */
XGE_API int xgeFontGlyphAtlasGetByIndex(xge_font pFont, int iGlyph, xge_glyph_t* pGlyph);
/* 释放 xgeFontGlyphRasterize* 输出的位图。 */
XGE_API void xgeGlyphBitmapFree(xge_glyph_bitmap_t* pBitmap);
#endif
/* 创建空 emoji 包。 */
#if XGE_ENABLE_EMOJI
XGE_API int xgeEmojiPackCreate(xge_emoji_pack* ppPack);
/* 加载内置 emoji 包（Twemoji 核心，Zstd 压缩）。 */
XGE_API int xgeEmojiPackLoadBuiltin(xge_emoji_pack* ppPack);
/* emoji 包引用计数加一。 */
XGE_API int xgeEmojiPackAddRef(xge_emoji_pack pPack);
/* emoji 包引用计数减一，归零时释放。 */
XGE_API void xgeEmojiPackFree(xge_emoji_pack pPack);
/* 向包添加一个 SVG emoji（sSequence 为码点序列）；*pEmojiId 输出编号。 */
XGE_API int xgeEmojiPackAddSvgMemory(xge_emoji_pack pPack, const char* sSequence, const void* pSvgData, int iSvgSize, const xge_emoji_metrics_t* pMetrics, uint32_t* pEmojiId);
/* 在文本头部做 emoji 最长匹配（Trie 检索）。 */
XGE_API int xgeEmojiPackMatch(xge_emoji_pack pPack, const char* sText, int iTextSize, xge_emoji_match_t* pMatch);
/* 设为全局默认 emoji 包（文本整形时使用）。 */
XGE_API int xgeEmojiPackSetDefault(xge_emoji_pack pPack);
/* 获取全局默认 emoji 包。 */
XGE_API int xgeEmojiPackGetDefault(xge_emoji_pack* ppPack);
/* 清除全局默认 emoji 包。 */
XGE_API void xgeEmojiPackClearDefault(void);
#endif
/* 整形 UTF-8 文本为字形 run（kerning/emoji 识别由 desc 开关）。 */
#if XGE_ENABLE_TEXT
XGE_API int xgeTextShape(const xge_text_shape_desc_t* pDesc, xge_glyph_run_t* pRun);
/* 释放整形输出的字形 run。 */
XGE_API void xgeGlyphRunFree(xge_glyph_run_t* pRun);
/* Return owned run storage in bytes; shared fonts and atlas resources are excluded. */
XGE_API size_t xgeGlyphRunRetainedBytes(const xge_glyph_run_t* pRun);
/* 测量字形 run 的包围盒与总步进。 */
XGE_API xge_vec2_t xgeGlyphRunMeasure(const xge_glyph_run_t* pRun);
/* 命中测试：输出命中的 cluster 与是否处于后半边界。 */
XGE_API int xgeGlyphRunHitTest(const xge_glyph_run_t* pRun, float fX, float fY, uint32_t* pCluster, int* pTrailing);
/* 按图集绘制字形 run。 */
XGE_API void xgeGlyphRunDraw(const xge_glyph_run_t* pRun, float fX, float fY, uint32_t iColor, uint32_t iFlags);
/* 按 cluster 分段绘制字形 run（支持逐段着色）。 */
XGE_API void xgeGlyphRunDrawSpans(const xge_glyph_run_t* pRun, float fX, float fY, uint32_t iColor, uint32_t iFlags, const xge_text_paint_span_t* pSpans, int iSpanCount);
/* 绘制字形 run 并叠加装饰线（下划线/上划线/删除线/波浪线等）。 */
XGE_API void xgeGlyphRunDrawDecorated(const xge_glyph_run_t* pRun, float fX, float fY, uint32_t iColor, uint32_t iFlags, const xge_text_decoration_t* pDecorations, int iDecorationCount);
#endif
/* 把 TrueType 字形轮廓追加为 ShapeEx 路径（文本矢量化）。 */
#if XGE_ENABLE_TEXT && XGE_ENABLE_SHAPE_EX
XGE_API int xgeFontGlyphOutlineAppendShapeEx(xge_font pFont, int iGlyph, xge_shape_ex pShape, float fPenX, float fBaselineY);
/* 把字形 run 全部轮廓追加为 ShapeEx 路径。 */
XGE_API int xgeGlyphRunAppendShapeEx(const xge_glyph_run_t* pRun, xge_shape_ex pShape, float fX, float fY);
#endif
/* 测量单行文本宽度（像素）。 */
#if XGE_ENABLE_TEXT
XGE_API xge_vec2_t xgeTextMeasure(xge_font pFont, const char* sText);
/* 在基线 (fX,fY) 绘制单行文本。 */
XGE_API void xgeTextDraw(xge_font pFont, const char* sText, float fX, float fY, uint32_t iColor);
/* 在矩形内绘制文本（按矩形约束排版）。 */
XGE_API void xgeTextDrawRect(xge_font pFont, const char* sText, xge_rect_t tRect, uint32_t iColor, uint32_t iFlags);
#endif
/* 解码图像文件到内存位图（RGBA8）。 */
XGE_API int xgeImageLoad(xge_image pImage, const char* sPath);
/* 解码图像文件；iFlags 控制加载选项。 */
XGE_API int xgeImageLoadEx(xge_image pImage, const char* sPath, uint32_t iFlags);
/* 从内存解码图像。 */
XGE_API int xgeImageLoadMemory(xge_image pImage, const void* pData, int iSize);
/* 从内存解码图像；iFlags 控制加载选项。 */
XGE_API int xgeImageLoadMemoryEx(xge_image pImage, const void* pData, int iSize, uint32_t iFlags);
/* Read encoded dimensions without allocating the decoded pixel buffer. */
XGE_API int xgeImageInfoMemory(const void* pData, int iSize, int* pWidth, int* pHeight);
/* 取图像像素首地址（调用方只读）。 */
XGE_API void* xgeImageGetPixels(xge_image pImage);
/* 就地预乘 alpha。 */
XGE_API void xgeImagePremultiply(xge_image pImage);
/* 把像素缓冲写出为 PNG 文件。 */
XGE_API int xgeImageSavePNG(const char* sPath, int iWidth, int iHeight, const void* pPixels, int iStride);
/* pPixels are straight RGBA by default. Pass XGE_IMAGE_PREMULTIPLIED when
 * saving premultiplied RGBA, such as an external render-target capture. */
XGE_API int xgeImageSavePNGEx(const char* sPath, int iWidth, int iHeight, const void* pPixels, int iStride, uint32_t iFlags);
/* Encode RGBA8 pixels to a PNG byte buffer owned by the caller (xrtFree).
 * Straight alpha is the default; PREMULTIPLIED is converted before encoding.
 * The filtered RGBA input is limited to 256 MiB. On failure, outputs are
 * reset to NULL/zero. */
XGE_API int xgeImageEncodePNGEx(int iWidth, int iHeight, const void* pPixels,
	int iStride, uint32_t iFlags, void** ppData, size_t* pSize);
/* 用默认直通 alpha 策略编码 RGBA8 PNG；返回缓冲由调用方 xrtFree。 */
XGE_API int xgeImageEncodePNG(int iWidth, int iHeight, const void* pPixels,
	int iStride, void** ppData, size_t* pSize);
/* 释放图像位图。 */
XGE_API void xgeImageFree(xge_image pImage);
/* pTexture must be zero-initialized before first use and freed before reuse.
 * Creating over a live texture returns XGE_ERROR_INVALID_STATE. */
XGE_API int xgeTextureCreateRGBA(xge_texture pTexture, int iWidth, int iHeight, const void* pPixels);
/* Create from RGBA pixels with XGE_TEXTURE_COMPRESS_* flags. Updates and CPU
 * readback retain their existing RGBA semantics. Unsupported packing falls back. */
XGE_API int xgeTextureCreateRGBAEx(xge_texture pTexture, int iWidth, int iHeight, const void* pPixels, uint32_t iFlags);
/* Query actual storage without making it a requirement for ordinary drawing. */
XGE_API int xgeTextureGetStorageInfo(xge_texture pTexture, xge_texture_storage_info_t* pInfo);
/* 创建 YUV420P 三平面视频纹理。 */
XGE_API int xgeTextureCreateYUV420P(xge_texture pTexture, int iWidth, int iHeight);
/* 以解码图像创建纹理。 */
XGE_API int xgeTextureCreateFromImage(xge_texture pTexture, const xge_image_t* pImage);
/* Create from decoded pixels with an explicit texture compression policy. */
XGE_API int xgeTextureCreateFromImageEx(xge_texture pTexture, const xge_image_t* pImage, uint32_t iFlags);
/* 从文件加载并创建纹理（stb 解码，支持常见 PNG/JPG 等）。 */
XGE_API int xgeTextureLoad(xge_texture pTexture, const char* sPath);
/* 从文件加载纹理；iFlags 可组合 XGE_IMAGE_* 与 XGE_TEXTURE_COMPRESS_*。 */
XGE_API int xgeTextureLoadEx(xge_texture pTexture, const char* sPath, uint32_t iFlags);
/* 从内存解码创建纹理。 */
XGE_API int xgeTextureLoadMemory(xge_texture pTexture, const void* pData, int iSize);
/* 从内存解码创建纹理；iFlags 可组合 XGE_IMAGE_* 与 XGE_TEXTURE_COMPRESS_*。 */
XGE_API int xgeTextureLoadMemoryEx(xge_texture pTexture, const void* pData, int iSize, uint32_t iFlags);
/* 纹理引用计数加一。 */
XGE_API int xgeTextureAddRef(xge_texture pTexture);
/* 更新纹理子区域（脏区上传，行距 iStride 字节）。 */
XGE_API int xgeTextureUpdateRGBA(xge_texture pTexture, int iX, int iY, int iWidth, int iHeight, const void* pPixels, int iStride);
/* 更新 YUV420P 三平面（各自独立行距）。 */
XGE_API int xgeTextureUpdateYUV420P(xge_texture pTexture, const void* pY, int iStrideY, const void* pU, int iStrideU, const void* pV, int iStrideV);
/* 重置采样参数为默认（线性过滤/夹持寻址）。 */
XGE_API xge_sampler_t xgeSamplerDefault(void);
/* 设置采样参数（过滤/寻址）。 */
XGE_API int xgeTextureSetSampler(xge_texture pTexture, const xge_sampler_t* pSampler);
/* 读取当前采样参数。 */
XGE_API xge_sampler_t xgeTextureGetSampler(xge_texture pTexture);
/* 设置全局兜底纹理（纹理加载失败时替代）。 */
XGE_API int xgeTextureFallbackSetRGBA(int iWidth, int iHeight, const void* pPixels);
/* 获取全局兜底纹理实例。 */
XGE_API int xgeTextureFallbackGet(xge_texture pTexture);
/* 清除全局兜底纹理。 */
XGE_API void xgeTextureFallbackClear(void);
/* 将纹理加入延迟上传队列（GL 未就绪时挂起，帧内自动 flush）。 */
XGE_API int xgeTextureUploadQueue(xge_texture pTexture);
/* 立即冲刷全部待上传纹理队列。 */
XGE_API int xgeTextureUploadFlush(void);
/* SDK 3 image-coordinate contract; compare with XGE_TEXTURE_COORDINATE_CONTRACT. */
XGE_API int xgeTextureCoordinateContract(void);
/* Inspect native storage mapping; never needed for standard image drawing. */
XGE_API xge_texture_uv_transform_t xgeTextureUVTransform(const xge_texture_t* pTexture);
XGE_API xge_vec2_t xgeTextureMapUV(const xge_texture_t* pTexture, xge_vec2_t tUV);
/* 读回 top-down CPU 影子副本；GPU 渲染目标须用 xgeRenderTargetReadPixels。 */
XGE_API int xgeTextureReadPixels(xge_texture pTexture, void* pPixels, int iStride);
/* 纹理引用计数减一，归零时释放。 */
XGE_API void xgeTextureFree(xge_texture pTexture);
/* 绑定窗口帧缓冲为渲染目标。 */
XGE_API int xgeRenderTargetWindow(xge_render_target pTarget);
/* 创建 FBO 离屏渲染目标。 */
XGE_API int xgeRenderTargetCreate(xge_render_target pTarget, int iWidth, int iHeight);
/* 调整渲染目标尺寸（内容不保留）。 */
XGE_API int xgeRenderTargetResize(xge_render_target pTarget, int iWidth, int iHeight);
/* 把窗口帧缓冲指定区域捕获为渲染目标内容（截图/审计用）。 */
XGE_API int xgeRenderTargetCaptureCurrent(xge_render_target pTarget, int iSrcX, int iSrcY);
/* 读回 top-down straight RGBA8（左上原点，行距 iStride 字节）。 */
XGE_API int xgeRenderTargetReadPixels(xge_render_target pTarget, void* pPixels, int iStride);
/* 取逻辑图片纹理；直接绘制即可，不要为渲染目标添加 FLIP_Y。 */
XGE_API xge_texture xgeRenderTargetTexture(xge_render_target pTarget);
/* 释放渲染目标。 */
XGE_API void xgeRenderTargetFree(xge_render_target pTarget);
/* 创建 GPU 缓冲（顶点/索引）；XGE_BUFFER_UNIFORM 当前不支持。 */
XGE_API int xgeBufferCreate(xge_buffer pBuffer, int iType, int iUsage, const void* pData, int iSize);
/* 更新缓冲区间数据。 */
XGE_API int xgeBufferUpdate(xge_buffer pBuffer, int iOffset, const void* pData, int iSize);
/* 上传缓冲到 GPU。 */
XGE_API int xgeBufferUpload(xge_buffer pBuffer);
/* 释放缓冲。 */
XGE_API void xgeBufferFree(xge_buffer pBuffer);
/* 初始化 pass 描述（目标/清除标志/清除色）。 */
XGE_API void xgePassInit(xge_pass pPass, xge_render_target pTarget, uint32_t iClearFlags, uint32_t iClearColor);
/* 进入 pass：切换目标、视口并按标志清屏，保存现场。 */
XGE_API int xgePassBegin(xge_pass pPass);
/* 退出 pass：恢复进入前的帧缓冲/视口/裁剪现场。 */
XGE_API int xgePassEnd(xge_pass pPass);
/* 编译顶点+片段着色器对。 */
XGE_API int xgeShaderCreate(xge_shader pShader, const char* sVertexSource, const char* sFragmentSource);
/* 着色器引用计数加一。 */
XGE_API int xgeShaderAddRef(xge_shader pShader);
/* 着色器引用计数减一，归零时释放。 */
XGE_API void xgeShaderFree(xge_shader pShader);
/* 按名称设置 int uniform。 */
XGE_API int xgeShaderUniform1i(xge_shader pShader, const char* sName, int iX);
/* 按名称设置 float uniform。 */
XGE_API int xgeShaderUniform1f(xge_shader pShader, const char* sName, float fX);
/* 按名称设置 vec2 uniform。 */
XGE_API int xgeShaderUniform2f(xge_shader pShader, const char* sName, float fX, float fY);
/* 按名称设置 vec3 uniform。 */
XGE_API int xgeShaderUniform3f(xge_shader pShader, const char* sName, float fX, float fY, float fZ);
/* 按名称设置 vec4 uniform。 */
XGE_API int xgeShaderUniform4f(xge_shader pShader, const char* sName, float fX, float fY, float fZ, float fW);
/* 创建着色器变体集（按宏组合缓存编译产物）。 */
XGE_API int xgeShaderVariantSetInit(xge_shader_variant_set pSet, const char* sVertexSource, const char* sFragmentSource);
/* 释放变体集。 */
XGE_API void xgeShaderVariantSetFree(xge_shader_variant_set pSet);
/* 按宏定义组合取（或编译并缓存）变体着色器。 */
XGE_API int xgeShaderVariantGet(xge_shader_variant_set pSet, uint32_t iKey, const xge_shader_define_t* pDefines, int iDefineCount, xge_shader* ppShader);
/* 材质重置为默认（内置纹理着色器）。 */
#if XGE_ENABLE_2D
XGE_API void xgeMaterialInit(xge_material pMaterial);
/* 释放材质（不释放其引用的 shader/纹理）。 */
XGE_API void xgeMaterialFree(xge_material pMaterial);
/* 绑定自定义着色器。 */
XGE_API void xgeMaterialSetShader(xge_material pMaterial, xge_shader pShader);
/* 设置主纹理槽。 */
XGE_API void xgeMaterialSetTexture(xge_material pMaterial, xge_texture pTexture);
/* 设置第二纹理槽。 */
XGE_API void xgeMaterialSetTexture2(xge_material pMaterial, xge_texture pTexture);
/* 设置第三纹理槽。 */
XGE_API void xgeMaterialSetTexture3(xge_material pMaterial, xge_texture pTexture);
/* 设置材质着色（乘色）。 */
XGE_API void xgeMaterialSetColor(xge_material pMaterial, uint32_t iColor);
/* 设置材质混合模式。 */
XGE_API void xgeMaterialSetBlend(xge_material pMaterial, int iBlend);
/* 以材质绘制一个纹理项。 */
XGE_API void xgeMaterialDraw(xge_material pMaterial, const xge_draw_t* pDraw);
/* 创建静态网格（顶点 + uint16 索引）。 */
XGE_API int xgeMeshCreate(xge_mesh pMesh, const xge_vertex_t* pVertices, int iVertexCount, const uint16_t* pIndices, int iIndexCount, uint32_t iFlags);
/* 更新网格顶点与索引数据。 */
XGE_API int xgeMeshUpdate(xge_mesh pMesh, const xge_vertex_t* pVertices, int iVertexCount, const uint16_t* pIndices, int iIndexCount);
/* 释放网格。 */
XGE_API void xgeMeshFree(xge_mesh pMesh);
/* 绘制网格；当前实现每次调用全量重传顶点（适合低频静态图）。 */
XGE_API void xgeMeshDraw(xge_mesh pMesh, xge_texture pTexture, uint32_t iFlags);
/* 立即绘制纹理于 (fX,fY)，保持原始尺寸。 */
XGE_API void xgeDraw(xge_texture pTexture, float fX, float fY);
/* 按完整绘制描述（缩放/旋转/混合/裁剪等）立即绘制纹理。 */
XGE_API void xgeDrawEx(const xge_draw_t* pDraw);
/* 绘制 2.5D 透视四边形，顶点含深度分量，由相机投影。 */
XGE_API void xgeDrawQuad3D(xge_texture pTexture, const xge_vertex_t* pVertices, uint32_t iFlags);
/* 像素对齐立即绘制纹理。 */
XGE_API void xgeDrawPx(xge_texture pTexture, int iX, int iY);
/* 以源区域初始化九宫格，切口取区域的三等分。 */
XGE_API void xgeNinePatchInitSimple(xge_nine_patch pPatch, xge_texture pTexture, xge_rect_t tSrc);
/* 以源区域和四条切口线初始化九宫格。 */
XGE_API void xgeNinePatchInit(xge_nine_patch pPatch, xge_texture pTexture, xge_rect_t tSrc, float fX1, float fY1, float fX2, float fY2);
/* 设置九宫格拉伸模式（拉伸/平铺）。 */
XGE_API void xgeNinePatchSetMode(xge_nine_patch pPatch, int iMode);
/* 设置九宫格着色（乘色）。 */
XGE_API void xgeNinePatchSetColor(xge_nine_patch pPatch, uint32_t iColor);
/* 绘制九宫格到目标矩形。 */
XGE_API void xgeNinePatchDraw(const xge_nine_patch_t* pPatch, xge_rect_t tDst, uint32_t iFlags);
/* 创建精灵批（绑定纹理与容量）。 */
XGE_API int xgeSpriteBatchInit(xge_sprite_batch pBatch, xge_texture pTexture, int iCapacity, uint32_t iFlags);
/* 释放精灵批。 */
XGE_API void xgeSpriteBatchFree(xge_sprite_batch pBatch);
/* 清空批内容（保留容量）。 */
XGE_API void xgeSpriteBatchClear(xge_sprite_batch pBatch);
/* 追加一个绘制项（xge_draw_t 描述）。 */
XGE_API int xgeSpriteBatchAdd(xge_sprite_batch pBatch, const xge_draw_t* pDraw);
/* 提交绘制批内全部项。 */
XGE_API int xgeSpriteBatchFlush(xge_sprite_batch pBatch);
/* Retarget only an empty batch; retained vertex storage is reused. */
XGE_API int xgeSpriteBatchSetTexture(xge_sprite_batch pBatch, xge_texture pTexture);
/* NULL material selects the default shader. Custom material tint is uColor. */
XGE_API int xgeSpriteBatchFlushMaterial(xge_sprite_batch pBatch, const xge_material_t* pMaterial);
/* 绘制实心点；fSize 为直径，像素单位。 */
XGE_API void xgeShapePoint(float fX, float fY, float fSize, uint32_t iColor);
/* 绘制像素对齐实心点。 */
XGE_API void xgeShapePointPx(float fX, float fY, float fSize, uint32_t iColor);
/* 绘制线段；fWidth 为线宽。 */
XGE_API void xgeShapeLine(float fX0, float fY0, float fX1, float fY1, float fWidth, uint32_t iColor);
/* 绘制像素对齐线段。 */
XGE_API void xgeShapeLinePx(float fX0, float fY0, float fX1, float fY1, float fWidth, uint32_t iColor);
/* 绘制实心矩形。 */
XGE_API void xgeShapeRectFill(xge_rect_t tRect, uint32_t iColor);
/* 绘制像素对齐实心矩形。 */
XGE_API void xgeShapeRectFillPx(xge_rect_t tRect, uint32_t iColor);
/* 绘制矩形描边，线宽居中于边界。 */
XGE_API void xgeShapeRectStroke(xge_rect_t tRect, float fWidth, uint32_t iColor);
/* 绘制像素对齐矩形描边。 */
XGE_API void xgeShapeRectStrokePx(xge_rect_t tRect, float fWidth, uint32_t iColor);
/* 以整数像素坐标绘制实心矩形。 */
XGE_API void xgeShapeRectFillPixels(xge_rect_i_t tRect, uint32_t iColor);
/* 按四边各自宽度绘制矩形边框。 */
XGE_API void xgeShapeRectBorderPixels(xge_rect_i_t tRect, xge_edges_i_t tBorder, uint32_t iColor);
/* 以 (fX,fY) 为圆心绘制实心圆。 */
XGE_API void xgeShapeCircleFill(float fX, float fY, float fRadius, uint32_t iColor);
/* 绘制像素对齐实心圆。 */
XGE_API void xgeShapeCircleFillPx(float fX, float fY, float fRadius, uint32_t iColor);
/* 绘制圆描边。 */
XGE_API void xgeShapeCircleStroke(float fX, float fY, float fRadius, float fWidth, uint32_t iColor);
/* 绘制像素对齐圆描边。 */
XGE_API void xgeShapeCircleStrokePx(float fX, float fY, float fRadius, float fWidth, uint32_t iColor);
/* 绘制圆弧线段；角度为弧度，自 fStartRadians 到 fEndRadians。 */
XGE_API void xgeShapeArc(float fX, float fY, float fRadius, float fStartRadians, float fEndRadians, float fWidth, uint32_t iColor);
/* 绘制像素对齐圆弧线段。 */
XGE_API void xgeShapeArcPx(float fX, float fY, float fRadius, float fStartRadians, float fEndRadians, float fWidth, uint32_t iColor);
/* 绘制实心三角形。 */
XGE_API void xgeShapeTriangleFill(xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC, uint32_t iColor);
/* 绘制像素对齐实心三角形。 */
XGE_API void xgeShapeTriangleFillPx(xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC, uint32_t iColor);
/* 绘制三角形描边。 */
XGE_API void xgeShapeTriangleStroke(xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC, float fWidth, uint32_t iColor);
/* 绘制像素对齐三角形描边。 */
XGE_API void xgeShapeTriangleStrokePx(xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC, float fWidth, uint32_t iColor);
/* 以两轴半径绘制实心椭圆。 */
XGE_API void xgeShapeEllipseFill(float fX, float fY, float fRadiusX, float fRadiusY, uint32_t iColor);
/* 绘制像素对齐实心椭圆。 */
XGE_API void xgeShapeEllipseFillPx(float fX, float fY, float fRadiusX, float fRadiusY, uint32_t iColor);
/* 绘制椭圆描边。 */
XGE_API void xgeShapeEllipseStroke(float fX, float fY, float fRadiusX, float fRadiusY, float fWidth, uint32_t iColor);
/* 绘制像素对齐椭圆描边。 */
XGE_API void xgeShapeEllipseStrokePx(float fX, float fY, float fRadiusX, float fRadiusY, float fWidth, uint32_t iColor);
/* 绘制扇形填充；角度为弧度。 */
XGE_API void xgeShapePieFill(float fX, float fY, float fRadiusX, float fRadiusY, float fStartRadians, float fEndRadians, uint32_t iColor);
/* 绘制像素对齐扇形填充。 */
XGE_API void xgeShapePieFillPx(float fX, float fY, float fRadiusX, float fRadiusY, float fStartRadians, float fEndRadians, uint32_t iColor);
/* 绘制弓形（弦切）填充；角度为弧度。 */
XGE_API void xgeShapeChordFill(float fX, float fY, float fRadiusX, float fRadiusY, float fStartRadians, float fEndRadians, uint32_t iColor);
/* 绘制像素对齐弓形填充。 */
XGE_API void xgeShapeChordFillPx(float fX, float fY, float fRadiusX, float fRadiusY, float fStartRadians, float fEndRadians, uint32_t iColor);
/* 绘制胶囊形（矩形加半圆端）填充。 */
XGE_API void xgeShapeCapsuleFill(xge_rect_t tRect, uint32_t iColor);
/* 绘制像素对齐胶囊形填充。 */
XGE_API void xgeShapeCapsuleFillPx(xge_rect_t tRect, uint32_t iColor);
/* 绘制胶囊形描边。 */
XGE_API void xgeShapeCapsuleStroke(xge_rect_t tRect, float fWidth, uint32_t iColor);
/* 绘制像素对齐胶囊形描边。 */
XGE_API void xgeShapeCapsuleStrokePx(xge_rect_t tRect, float fWidth, uint32_t iColor);
/* 设置立即形状渲染模式（SDF 或网格抗锯齿）。 */
XGE_API void xgeShapeRenderModeSet(int iMode);
/* 读取立即形状渲染模式。 */
XGE_API int xgeShapeRenderModeGet(void);
/* 设置圆角矩形圆角模式。 */
XGE_API void xgeShapeRoundRectModeSet(int iMode);
/* 读取圆角矩形圆角模式。 */
XGE_API int xgeShapeRoundRectModeGet(void);
/* 绘制四角同半径实心圆角矩形。 */
XGE_API void xgeShapeRoundRectFill(xge_rect_t tRect, float fRadius, uint32_t iColor);
/* 绘制像素对齐实心圆角矩形。 */
XGE_API void xgeShapeRoundRectFillPx(xge_rect_t tRect, float fRadius, uint32_t iColor);
/* 绘制四角独立半径的实心圆角矩形。 */
XGE_API void xgeShapeRoundRectFillEx(xge_rect_t tRect, xge_shape_round_rect_t tRadii, int iMode, uint32_t iColor);
/* 绘制像素对齐、四角独立半径的实心圆角矩形。 */
XGE_API void xgeShapeRoundRectFillExPx(xge_rect_t tRect, xge_shape_round_rect_t tRadii, int iMode, uint32_t iColor);
/* 绘制四角同半径圆角矩形描边。 */
XGE_API void xgeShapeRoundRectStroke(xge_rect_t tRect, float fRadius, float fWidth, uint32_t iColor);
/* 绘制像素对齐圆角矩形描边。 */
XGE_API void xgeShapeRoundRectStrokePx(xge_rect_t tRect, float fRadius, float fWidth, uint32_t iColor);
/* 绘制四角独立半径的圆角矩形描边。 */
XGE_API void xgeShapeRoundRectStrokeEx(xge_rect_t tRect, xge_shape_round_rect_t tRadii, int iMode, float fWidth, uint32_t iColor);
/* 绘制像素对齐、四角独立半径的圆角矩形描边。 */
XGE_API void xgeShapeRoundRectStrokeExPx(xge_rect_t tRect, xge_shape_round_rect_t tRadii, int iMode, float fWidth, uint32_t iColor);
/* 一次绘制圆角矩形填充与描边。 */
XGE_API void xgeShapeRoundRectDraw(xge_rect_t tRect, float fRadius, uint32_t iFillColor, float fStrokeWidth, uint32_t iStrokeColor);
/* 像素对齐一次绘制圆角矩形填充与描边。 */
XGE_API void xgeShapeRoundRectDrawPx(xge_rect_t tRect, float fRadius, uint32_t iFillColor, float fStrokeWidth, uint32_t iStrokeColor);
/* 四角独立半径一次绘制圆角矩形填充与描边。 */
XGE_API void xgeShapeRoundRectDrawEx(xge_rect_t tRect, xge_shape_round_rect_t tRadii, int iMode, uint32_t iFillColor, float fStrokeWidth, uint32_t iStrokeColor);
/* 像素对齐、四角独立半径一次绘制填充与描边。 */
XGE_API void xgeShapeRoundRectDrawExPx(xge_rect_t tRect, xge_shape_round_rect_t tRadii, int iMode, uint32_t iFillColor, float fStrokeWidth, uint32_t iStrokeColor);
/* 绘制多边形填充。 */
XGE_API void xgeShapePolygonFill(const xge_vec2_t* pPoints, int iCount, uint32_t iColor);
/* 绘制像素对齐多边形填充。 */
XGE_API void xgeShapePolygonFillPx(const xge_vec2_t* pPoints, int iCount, uint32_t iColor);
/* 以顶点缓冲加索引列表绘制带色三角形网格。 */
XGE_API int xgeShapeMeshFill(const xge_shape_vertex_t* pVertices, int iVertexCount, const uint32_t* pIndices, int iIndexCount);
/* 绘制像素对齐三角形网格。 */
XGE_API int xgeShapeMeshFillPx(const xge_shape_vertex_t* pVertices, int iVertexCount, const uint32_t* pIndices, int iIndexCount);
#endif
/* 矩阵重置为单位阵。 */
#if XGE_ENABLE_SHAPE_EX
XGE_API int xgeShapeExMatrixIdentity(xge_shape_ex_matrix_t* pMatrix);
/* 矩阵复合：pOut = pParent * pLocal。 */
XGE_API int xgeShapeExMatrixMultiply(xge_shape_ex_matrix_t* pOut, const xge_shape_ex_matrix_t* pParent, const xge_shape_ex_matrix_t* pLocal);
/* 矩阵左乘平移。 */
XGE_API int xgeShapeExMatrixTranslate(xge_shape_ex_matrix_t* pMatrix, float fTX, float fTY);
/* 矩阵左乘缩放。 */
XGE_API int xgeShapeExMatrixScale(xge_shape_ex_matrix_t* pMatrix, float fSX, float fSY);
/* 矩阵左乘旋转（弧度）。 */
XGE_API int xgeShapeExMatrixRotate(xge_shape_ex_matrix_t* pMatrix, float fRadians);
/* 矩阵左乘斜切（X/Y 弧度）。 */
XGE_API int xgeShapeExMatrixSkew(xge_shape_ex_matrix_t* pMatrix, float fXRadians, float fYRadians);
/* 求逆矩阵；不可逆时返回错误。 */
XGE_API int xgeShapeExMatrixInvert(xge_shape_ex_matrix_t* pOut, const xge_shape_ex_matrix_t* pMatrix);
/* 以矩阵变换点（含平移）。 */
XGE_API int xgeShapeExMatrixPoint(xge_vec2_t* pOut, const xge_shape_ex_matrix_t* pMatrix, xge_vec2_t tPoint);
/* 以矩阵变换向量（不含平移）。 */
XGE_API int xgeShapeExMatrixVector(xge_vec2_t* pOut, const xge_shape_ex_matrix_t* pMatrix, xge_vec2_t tVector);
/* 变换矩形并输出轴对齐包围盒。 */
XGE_API int xgeShapeExMatrixRectBounds(xge_rect_t* pOut, const xge_shape_ex_matrix_t* pMatrix, xge_rect_t tRect);
/* 输出矩阵的平均缩放因子（non-scaling 描边用）。 */
XGE_API int xgeShapeExMatrixStrokeScale(float* pScale, const xge_shape_ex_matrix_t* pMatrix);
/* 创建空矢量路径对象。 */
XGE_API int xgeShapeExCreate(xge_shape_ex* ppShape);
/* 路径引用计数加一。 */
XGE_API int xgeShapeExAddRef(xge_shape_ex pShape);
/* 读取路径引用计数。 */
XGE_API int xgeShapeExRefCountGet(xge_shape_ex pShape, int* pRefCount);
/* 由名称散列出形状 id（跨进程稳定）。 */
XGE_API uint32_t xgeShapeExIdFromName(const char* sName);
/* 设置形状 id。 */
XGE_API int xgeShapeExId(xge_shape_ex pShape, uint32_t iId);
/* 读取形状 id。 */
XGE_API int xgeShapeExIdGet(xge_shape_ex pShape, uint32_t* pId);
/* 取所属父场景；未挂载时输出 NULL。 */
XGE_API int xgeShapeExParentGet(xge_shape_ex pShape, xge_shape_ex_scene* ppParentScene);
/* 深拷贝路径（不含父级关系）。 */
XGE_API int xgeShapeExClone(xge_shape_ex pShape, xge_shape_ex* ppClone);
/* 路径引用计数减一，归零时释放。 */
XGE_API void xgeShapeExDestroy(xge_shape_ex pShape);
/* 清空路径与全部绘制属性。 */
XGE_API int xgeShapeExReset(xge_shape_ex pShape);
/* 移动当前点到指定位置，开始新子路径。 */
XGE_API int xgeShapeExMoveTo(xge_shape_ex pShape, float fX, float fY);
/* 从当前点连直线。 */
XGE_API int xgeShapeExLineTo(xge_shape_ex pShape, float fX, float fY);
/* 追加二次贝塞尔曲线。 */
XGE_API int xgeShapeExQuadTo(xge_shape_ex pShape, float fCX, float fCY, float fX, float fY);
/* 追加三次贝塞尔曲线。 */
XGE_API int xgeShapeExCubicTo(xge_shape_ex pShape, float fC1X, float fC1Y, float fC2X, float fC2Y, float fX, float fY);
/* 追加椭圆弧到指定终点（SVG arc 参数语义）。 */
XGE_API int xgeShapeExArcTo(xge_shape_ex pShape, float fRX, float fRY, float fAxisDegrees, int bLargeArc, int bSweep, float fX, float fY);
/* 闭合当前子路径。 */
XGE_API int xgeShapeExClose(xge_shape_ex pShape);
/* 以三角形顶点直接追加闭合子路径。 */
XGE_API int xgeShapeExAppendTriangle(xge_shape_ex pShape, float fX1, float fY1, float fX2, float fY2, float fX3, float fY3, int bClockwise);
/* 以线段两端点追加开放子路径。 */
XGE_API int xgeShapeExAppendLine(xge_shape_ex pShape, float fX1, float fY1, float fX2, float fY2);
/* 以点列追加折线子路径。 */
XGE_API int xgeShapeExAppendPolyline(xge_shape_ex pShape, const xge_vec2_t* pPoints, int iPointCount);
/* 以点列追加闭合多边形子路径。 */
XGE_API int xgeShapeExAppendPolygon(xge_shape_ex pShape, const xge_vec2_t* pPoints, int iPointCount);
/* 追加矩形子路径（可带圆角半径）。 */
XGE_API int xgeShapeExAppendRect(xge_shape_ex pShape, float fX, float fY, float fW, float fH, float fRX, float fRY, int bClockwise);
/* 追加胶囊形子路径。 */
XGE_API int xgeShapeExAppendCapsule(xge_shape_ex pShape, float fX, float fY, float fW, float fH, int bClockwise);
/* 追加圆形子路径（两轴半径可不同）。 */
XGE_API int xgeShapeExAppendCircle(xge_shape_ex pShape, float fCX, float fCY, float fRX, float fRY, int bClockwise);
/* 追加椭圆子路径。 */
XGE_API int xgeShapeExAppendEllipse(xge_shape_ex pShape, float fCX, float fCY, float fRX, float fRY, int bClockwise);
/* 追加圆弧开放子路径（起止弧度）。 */
XGE_API int xgeShapeExAppendArc(xge_shape_ex pShape, float fCX, float fCY, float fRX, float fRY, float fStartRadians, float fEndRadians);
/* 追加扇形闭合子路径。 */
XGE_API int xgeShapeExAppendPie(xge_shape_ex pShape, float fCX, float fCY, float fRX, float fRY, float fStartRadians, float fEndRadians);
/* 追加弓形闭合子路径。 */
XGE_API int xgeShapeExAppendChord(xge_shape_ex pShape, float fCX, float fCY, float fRX, float fRY, float fStartRadians, float fEndRadians);
/* 以命令流 + 点列批量追加路径。 */
XGE_API int xgeShapeExAppendPath(xge_shape_ex pShape, const uint8_t* pCommands, int iCommandCount, const xge_vec2_t* pPoints, int iPointCount);
/* 解析 SVG path data 字符串并追加。 */
XGE_API int xgeShapeExAppendSvgPath(xge_shape_ex pShape, const char* sPath);
/* 输出内部命令流与点列（只读视图）。 */
XGE_API int xgeShapeExGetPath(xge_shape_ex pShape, const uint8_t** ppCommands, int* pCommandCount, const xge_vec2_t** ppPoints, int* pPointCount);
/* 把路径序列化为 SVG path data；缓冲不足时输出所需大小。 */
XGE_API int xgeShapeExGetSvgPathData(xge_shape_ex pShape, char* sBuffer, int iBufferSize, int* pRequiredSize);
/* 设置纯色填充。 */
XGE_API int xgeShapeExFillColor(xge_shape_ex pShape, uint32_t iColor);
/* 读取填充类型（纯色/线性/径向）。 */
XGE_API int xgeShapeExFillTypeGet(xge_shape_ex pShape, int* pType);
/* 读取纯色填充色；非纯色填充时返回错误。 */
XGE_API int xgeShapeExFillColorGet(xge_shape_ex pShape, uint32_t* pColor);
/* 设置线性渐变填充；iUnits 选用户空间/对象包围盒。 */
XGE_API int xgeShapeExFillLinearGradient(xge_shape_ex pShape, float fX1, float fY1, float fX2, float fY2, int iUnits, const xge_shape_ex_color_stop_t* pStops, int iStopCount);
/* 输出线性渐变填充参数与色标视图。 */
XGE_API int xgeShapeExFillLinearGradientGet(xge_shape_ex pShape, float* pX1, float* pY1, float* pX2, float* pY2, int* pUnits, const xge_shape_ex_color_stop_t** ppStops, int* pStopCount);
/* 设置径向渐变填充（圆心/焦点半径版）。 */
XGE_API int xgeShapeExFillRadialGradient(xge_shape_ex pShape, float fCX, float fCY, float fRadius, float fFX, float fFY, int iUnits, const xge_shape_ex_color_stop_t* pStops, int iStopCount);
/* 设置径向渐变填充（含焦点半径扩展）。 */
XGE_API int xgeShapeExFillRadialGradientEx(xge_shape_ex pShape, float fCX, float fCY, float fRadius, float fFX, float fFY, float fFocalRadius, int iUnits, const xge_shape_ex_color_stop_t* pStops, int iStopCount);
/* 输出径向渐变参数。 */
XGE_API int xgeShapeExFillRadialGradientGet(xge_shape_ex pShape, float* pCX, float* pCY, float* pRadius, float* pFX, float* pFY, int* pUnits, const xge_shape_ex_color_stop_t** ppStops, int* pStopCount);
/* 输出径向渐变参数（含焦点半径）。 */
XGE_API int xgeShapeExFillRadialGradientGetEx(xge_shape_ex pShape, float* pCX, float* pCY, float* pRadius, float* pFX, float* pFY, float* pFocalRadius, int* pUnits, const xge_shape_ex_color_stop_t** ppStops, int* pStopCount);
/* 设置填充渐变扩散模式（Pad/Reflect/Repeat）。 */
XGE_API int xgeShapeExFillGradientSpread(xge_shape_ex pShape, int iSpread);
/* 读取填充渐变扩散模式。 */
XGE_API int xgeShapeExFillGradientSpreadGet(xge_shape_ex pShape, int* pSpread);
/* 设置填充渐变梯度空间的变换矩阵。 */
XGE_API int xgeShapeExFillGradientTransformSet(xge_shape_ex pShape, const xge_shape_ex_matrix_t* pMatrix);
/* 重置填充渐变变换。 */
XGE_API int xgeShapeExFillGradientTransformIdentity(xge_shape_ex pShape);
/* 读取填充渐变变换。 */
XGE_API int xgeShapeExFillGradientTransformGet(xge_shape_ex pShape, xge_shape_ex_matrix_t* pMatrix);
/* 设置纯色描边。 */
XGE_API int xgeShapeExStrokeColor(xge_shape_ex pShape, uint32_t iColor);
/* 读取描边类型（纯色/线性/径向）。 */
XGE_API int xgeShapeExStrokeTypeGet(xge_shape_ex pShape, int* pType);
/* 读取纯色描边色；非纯色时返回错误。 */
XGE_API int xgeShapeExStrokeColorGet(xge_shape_ex pShape, uint32_t* pColor);
/* 设置描边线性渐变。 */
XGE_API int xgeShapeExStrokeLinearGradient(xge_shape_ex pShape, float fX1, float fY1, float fX2, float fY2, int iUnits, const xge_shape_ex_color_stop_t* pStops, int iStopCount);
/* 输出描边线性渐变参数。 */
XGE_API int xgeShapeExStrokeLinearGradientGet(xge_shape_ex pShape, float* pX1, float* pY1, float* pX2, float* pY2, int* pUnits, const xge_shape_ex_color_stop_t** ppStops, int* pStopCount);
/* 设置描边径向渐变。 */
XGE_API int xgeShapeExStrokeRadialGradient(xge_shape_ex pShape, float fCX, float fCY, float fRadius, float fFX, float fFY, int iUnits, const xge_shape_ex_color_stop_t* pStops, int iStopCount);
/* 设置描边径向渐变（含焦点半径）。 */
XGE_API int xgeShapeExStrokeRadialGradientEx(xge_shape_ex pShape, float fCX, float fCY, float fRadius, float fFX, float fFY, float fFocalRadius, int iUnits, const xge_shape_ex_color_stop_t* pStops, int iStopCount);
/* 输出描边径向渐变参数。 */
XGE_API int xgeShapeExStrokeRadialGradientGet(xge_shape_ex pShape, float* pCX, float* pCY, float* pRadius, float* pFX, float* pFY, int* pUnits, const xge_shape_ex_color_stop_t** ppStops, int* pStopCount);
/* 输出描边径向渐变参数（含焦点半径）。 */
XGE_API int xgeShapeExStrokeRadialGradientGetEx(xge_shape_ex pShape, float* pCX, float* pCY, float* pRadius, float* pFX, float* pFY, float* pFocalRadius, int* pUnits, const xge_shape_ex_color_stop_t** ppStops, int* pStopCount);
/* 设置描边渐变扩散模式。 */
XGE_API int xgeShapeExStrokeGradientSpread(xge_shape_ex pShape, int iSpread);
/* 读取描边渐变扩散模式。 */
XGE_API int xgeShapeExStrokeGradientSpreadGet(xge_shape_ex pShape, int* pSpread);
/* 设置描边渐变梯度变换。 */
XGE_API int xgeShapeExStrokeGradientTransformSet(xge_shape_ex pShape, const xge_shape_ex_matrix_t* pMatrix);
/* 重置描边渐变变换。 */
XGE_API int xgeShapeExStrokeGradientTransformIdentity(xge_shape_ex pShape);
/* 读取描边渐变变换。 */
XGE_API int xgeShapeExStrokeGradientTransformGet(xge_shape_ex pShape, xge_shape_ex_matrix_t* pMatrix);
/* 设置描边宽度（宽度居中于路径）。 */
XGE_API int xgeShapeExStrokeWidth(xge_shape_ex pShape, float fWidth);
/* 读取描边宽度。 */
XGE_API int xgeShapeExStrokeWidthGet(xge_shape_ex pShape, float* pWidth);
/* 设置线帽（Butt/Round/Square）。 */
XGE_API int xgeShapeExStrokeCap(xge_shape_ex pShape, int iCap);
/* 读取线帽。 */
XGE_API int xgeShapeExStrokeCapGet(xge_shape_ex pShape, int* pCap);
/* 设置拐角连接（Miter/Round/Bevel）。 */
XGE_API int xgeShapeExStrokeJoin(xge_shape_ex pShape, int iJoin);
/* 读取拐角连接。 */
XGE_API int xgeShapeExStrokeJoinGet(xge_shape_ex pShape, int* pJoin);
/* 设置斜接比例上限。 */
XGE_API int xgeShapeExStrokeMiterLimit(xge_shape_ex pShape, float fLimit);
/* 读取斜接上限。 */
XGE_API int xgeShapeExStrokeMiterLimitGet(xge_shape_ex pShape, float* pLimit);
/* 开关 non-scaling 描边（缩放时保持像素线宽）。 */
XGE_API int xgeShapeExStrokeNonScaling(xge_shape_ex pShape, int bNonScaling);
/* 读取 non-scaling 开关。 */
XGE_API int xgeShapeExStrokeNonScalingGet(xge_shape_ex pShape, int* pNonScaling);
/* 设置虚线模式（长短交替）与相位偏移。 */
XGE_API int xgeShapeExStrokeDash(xge_shape_ex pShape, const float* pDashPattern, int iDashCount, float fDashOffset);
/* 输出虚线模式视图与偏移。 */
XGE_API int xgeShapeExStrokeDashGet(xge_shape_ex pShape, const float** ppDashPattern, int* pDashCount, float* pDashOffset);
/* 设置路径裁剪区间 [fBegin,fEnd]（路径动画）；bSimultaneous 双向同步。 */
XGE_API int xgeShapeExTrimPath(xge_shape_ex pShape, float fBegin, float fEnd, int bSimultaneous);
/* 输出路径裁剪区间与启用状态。 */
XGE_API int xgeShapeExTrimPathGet(xge_shape_ex pShape, float* pBegin, float* pEnd, int* pSimultaneous, int* pEnabled);
/* 取消路径裁剪。 */
XGE_API int xgeShapeExTrimClear(xge_shape_ex pShape);
/* 设置填充规则（NonZero/EvenOdd）。 */
XGE_API int xgeShapeExFillRule(xge_shape_ex pShape, int iRule);
/* 读取填充规则。 */
XGE_API int xgeShapeExFillRuleGet(xge_shape_ex pShape, int* pRule);
/* 设置绘制顺序（先描边或先填充）。 */
XGE_API int xgeShapeExPaintOrder(xge_shape_ex pShape, int bStrokeFirst);
/* 读取绘制顺序。 */
XGE_API int xgeShapeExPaintOrderGet(xge_shape_ex pShape, int* pStrokeFirst);
/* 设置整体不透明度 [0,1]。 */
XGE_API int xgeShapeExOpacity(xge_shape_ex pShape, float fOpacity);
/* 读取整体不透明度。 */
XGE_API int xgeShapeExOpacityGet(xge_shape_ex pShape, float* pOpacity);
/* 设置可见性。 */
XGE_API int xgeShapeExVisible(xge_shape_ex pShape, int bVisible);
/* 读取可见性。 */
XGE_API int xgeShapeExVisibleGet(xge_shape_ex pShape, int* pVisible);
/* 设置混合模式（ShapeEx 内部离屏合成支持全部 18 种）。 */
XGE_API int xgeShapeExBlend(xge_shape_ex pShape, int iBlend);
/* 恢复默认混合。 */
XGE_API int xgeShapeExBlendClear(xge_shape_ex pShape);
/* 输出混合模式与是否显式设置。 */
XGE_API int xgeShapeExBlendGet(xge_shape_ex pShape, int* pBlend, int* pBlendSet);
/* 以路径为遮罩；iMethod 选 Alpha/Luma。 */
XGE_API int xgeShapeExMaskShapeSet(xge_shape_ex pShape, xge_shape_ex pTarget, int iMethod);
/* 以场景为遮罩；iMethod 选 Alpha/Luma。 */
XGE_API int xgeShapeExMaskSceneSet(xge_shape_ex pShape, xge_shape_ex_scene pTarget, int iMethod);
/* 移除遮罩。 */
XGE_API int xgeShapeExMaskClear(xge_shape_ex pShape);
/* 输出当前遮罩（方法/目标类型/句柄）。 */
XGE_API int xgeShapeExMaskGet(xge_shape_ex pShape, int* pMethod, int* pTargetType, xge_shape_ex* ppTargetShape, xge_shape_ex_scene* ppTargetScene);
/* 以回调绘制源内容并与遮罩场景合成（高级自定义遮罩入口）。 */
XGE_API int xgeShapeExMaskCompositeScene(xge_shape_ex_scene pMaskScene, int iMethod, int iOutputBlend, xge_rect_t tSourceBounds, xge_shape_ex_draw_proc pSourceDraw, void* pSourceUser, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, int bScreenSpace);
/* 计算路径轴对齐包围盒（含容差细分）。 */
XGE_API int xgeShapeExGetBounds(xge_shape_ex pShape, float fTolerance, xge_rect_t* pBounds);
/* 计算路径经自身变换后的定向包围盒（4 顶点）。 */
XGE_API int xgeShapeExGetOBB(xge_shape_ex pShape, float fTolerance, xge_vec2_t* pPoints4);
/* 快速判断包围盒与矩形是否相交。 */
XGE_API int xgeShapeExBoundsIntersects(xge_shape_ex pShape, xge_rect_t tRect, float fTolerance, int* pIntersects);
/* 精确判断路径与矩形是否相交。 */
XGE_API int xgeShapeExIntersects(xge_shape_ex pShape, xge_rect_t tRect, float fTolerance, int* pIntersects);
/* 精确相交判断（叠加父级矩阵）。 */
XGE_API int xgeShapeExIntersectsEx(xge_shape_ex pShape, xge_rect_t tRect, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, int* pIntersects);
/* 精确命中测试（按填充规则）。 */
XGE_API int xgeShapeExContainsPoint(xge_shape_ex pShape, float fX, float fY, float fTolerance, int* pContains);
/* 精确命中测试（叠加父级矩阵）。 */
XGE_API int xgeShapeExContainsPointEx(xge_shape_ex pShape, float fX, float fY, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, int* pContains);
/* 计算路径总长度（含容差细分）。 */
XGE_API int xgeShapeExGetLength(xge_shape_ex pShape, float fTolerance, float* pLength);
/* 取路径上指定弧长处的点与切线。 */
XGE_API int xgeShapeExGetPointAtLength(xge_shape_ex pShape, float fDistance, float fTolerance, xge_vec2_t* pPoint, xge_vec2_t* pTangent);
/* 创建路径测量器（预计算弧长表，多次查询高效）。 */
XGE_API int xgeShapeExPathMeasureCreate(xge_shape_ex_path_measure* ppMeasure, xge_shape_ex pShape, const xge_shape_ex_matrix_t* pMatrix, float fTolerance);
/* 释放路径测量器。 */
XGE_API void xgeShapeExPathMeasureDestroy(xge_shape_ex_path_measure pMeasure);
/* 读取测量器路径总长。 */
XGE_API int xgeShapeExPathMeasureGetLength(xge_shape_ex_path_measure pMeasure, float* pLength);
/* 取指定弧长处的点与切线。 */
XGE_API int xgeShapeExPathMeasureGetPointAtLength(xge_shape_ex_path_measure pMeasure, float fDistance, xge_vec2_t* pPoint, xge_vec2_t* pTangent);
/* 设置路径级矩形裁剪。 */
XGE_API int xgeShapeExClipRectSet(xge_shape_ex pShape, xge_rect_t tRect);
/* 输出矩形裁剪与启用状态。 */
XGE_API int xgeShapeExClipRectGet(xge_shape_ex pShape, xge_rect_t* pRect, int* pEnabled);
/* 追加一个路径裁剪（模板位求交）。 */
XGE_API int xgeShapeExClipShapeAdd(xge_shape_ex pShape, xge_shape_ex pClipShape);
/* 追加路径裁剪并指定模板模式。 */
XGE_API int xgeShapeExClipShapeAddEx(xge_shape_ex pShape, xge_shape_ex pClipShape, int iMode);
/* 读取路径裁剪数量。 */
XGE_API int xgeShapeExClipShapeGetCount(xge_shape_ex pShape, int* pCount);
/* 按索引取路径裁剪。 */
XGE_API int xgeShapeExClipShapeGetAt(xge_shape_ex pShape, int iIndex, xge_shape_ex* ppClipShape);
/* 按索引取路径裁剪及模式。 */
XGE_API int xgeShapeExClipShapeGetAtEx(xge_shape_ex pShape, int iIndex, xge_shape_ex* ppClipShape, int* pMode);
/* 清空全部路径裁剪。 */
XGE_API int xgeShapeExClipShapeClear(xge_shape_ex pShape);
/* 清空全部裁剪（含矩形）。 */
XGE_API int xgeShapeExClipClear(xge_shape_ex pShape);
/* 以模板缓冲裁剪开始（成对使用 StencilClipEnd）。 */
XGE_API int xgeShapeExStencilClipBegin(xge_shape_ex pClipShape, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, int* pApplied);
/* 像素对齐的模板裁剪开始。 */
XGE_API int xgeShapeExStencilClipBeginPx(xge_shape_ex pClipShape, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, int* pApplied);
/* 结束模板裁剪并恢复模板状态（与 Begin 成对）。 */
XGE_API int xgeShapeExStencilClipEnd(int bApplied, int iRet);
/* 以矩阵覆盖路径变换。 */
XGE_API int xgeShapeExTransformSet(xge_shape_ex pShape, const xge_shape_ex_matrix_t* pMatrix);
/* 重置路径变换。 */
XGE_API int xgeShapeExTransformIdentity(xge_shape_ex pShape);
/* 读取路径变换。 */
XGE_API int xgeShapeExTransformGet(xge_shape_ex pShape, xge_shape_ex_matrix_t* pMatrix);
/* 路径变换追加平移。 */
XGE_API int xgeShapeExTransformTranslate(xge_shape_ex pShape, float fTX, float fTY);
/* 路径变换追加缩放。 */
XGE_API int xgeShapeExTransformScale(xge_shape_ex pShape, float fSX, float fSY);
/* 路径变换追加旋转（弧度）。 */
XGE_API int xgeShapeExTransformRotate(xge_shape_ex pShape, float fRadians);
/* 路径变换追加斜切。 */
XGE_API int xgeShapeExTransformSkew(xge_shape_ex pShape, float fXRadians, float fYRadians);
/* 绘制路径（按 paint-order 应用填充与描边）。 */
XGE_API int xgeShapeExDraw(xge_shape_ex pShape, float fTolerance);
/* 像素对齐绘制路径。 */
XGE_API int xgeShapeExDrawPx(xge_shape_ex pShape, float fTolerance);
/* 绘制路径（叠加父级矩阵与父级不透明度）。 */
XGE_API int xgeShapeExDrawEx(xge_shape_ex pShape, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, float fParentOpacity);
/* 像素对齐绘制（叠加父级矩阵与透明度）。 */
XGE_API int xgeShapeExDrawPxEx(xge_shape_ex pShape, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, float fParentOpacity);
/* 创建空场景（形状容器节点）。 */
XGE_API int xgeShapeExSceneCreate(xge_shape_ex_scene* ppScene);
/* 场景引用计数加一。 */
XGE_API int xgeShapeExSceneAddRef(xge_shape_ex_scene pScene);
/* 读取场景引用计数。 */
XGE_API int xgeShapeExSceneRefCountGet(xge_shape_ex_scene pScene, int* pRefCount);
/* 设置场景 id。 */
XGE_API int xgeShapeExSceneId(xge_shape_ex_scene pScene, uint32_t iId);
/* 读取场景 id。 */
XGE_API int xgeShapeExSceneIdGet(xge_shape_ex_scene pScene, uint32_t* pId);
/* 取父场景；无父时输出 NULL。 */
XGE_API int xgeShapeExSceneParentGet(xge_shape_ex_scene pScene, xge_shape_ex_scene* ppParentScene);
/* 深拷贝场景树。 */
XGE_API int xgeShapeExSceneClone(xge_shape_ex_scene pScene, xge_shape_ex_scene* ppClone);
/* 场景引用计数减一，归零时释放。 */
XGE_API void xgeShapeExSceneDestroy(xge_shape_ex_scene pScene);
/* 清空场景子节点（不销毁子对象本身）。 */
XGE_API int xgeShapeExSceneClear(xge_shape_ex_scene pScene);
/* 追加子路径到场景。 */
XGE_API int xgeShapeExSceneAdd(xge_shape_ex_scene pScene, xge_shape_ex pShape);
/* 追加子场景（形成场景树）。 */
XGE_API int xgeShapeExSceneAddScene(xge_shape_ex_scene pScene, xge_shape_ex_scene pChildScene);
/* 在指定子节点前插入路径。 */
XGE_API int xgeShapeExSceneInsert(xge_shape_ex_scene pScene, xge_shape_ex pShape, xge_shape_ex pBefore);
/* 按索引插入路径。 */
XGE_API int xgeShapeExSceneInsertShapeAt(xge_shape_ex_scene pScene, xge_shape_ex pShape, int iIndex);
/* 按索引插入子场景。 */
XGE_API int xgeShapeExSceneInsertSceneAt(xge_shape_ex_scene pScene, xge_shape_ex_scene pChildScene, int iIndex);
/* 从场景移除子路径（不销毁）。 */
XGE_API int xgeShapeExSceneRemove(xge_shape_ex_scene pScene, xge_shape_ex pShape);
/* 从场景移除子场景（不销毁）。 */
XGE_API int xgeShapeExSceneRemoveScene(xge_shape_ex_scene pScene, xge_shape_ex_scene pChildScene);
/* 读取直接子节点数量。 */
XGE_API int xgeShapeExSceneGetCount(xge_shape_ex_scene pScene, int* pCount);
/* 按索引取子路径。 */
XGE_API int xgeShapeExSceneGetAt(xge_shape_ex_scene pScene, int iIndex, xge_shape_ex* ppShape);
/* 按索引取子节点（含类型判别，路径或子场景）。 */
XGE_API int xgeShapeExSceneChildGetAt(xge_shape_ex_scene pScene, int iIndex, xge_shape_ex_scene_child_t* pChild);
/* 深度优先遍历场景树并回调每个节点。 */
XGE_API int xgeShapeExSceneTraverse(xge_shape_ex_scene pScene, xge_shape_ex_scene_visit_proc onPaint, void* pUser);
/* 以矩阵覆盖场景变换（作用于全部子节点）。 */
XGE_API int xgeShapeExSceneTransformSet(xge_shape_ex_scene pScene, const xge_shape_ex_matrix_t* pMatrix);
/* 重置场景变换。 */
XGE_API int xgeShapeExSceneTransformIdentity(xge_shape_ex_scene pScene);
/* 读取场景变换。 */
XGE_API int xgeShapeExSceneTransformGet(xge_shape_ex_scene pScene, xge_shape_ex_matrix_t* pMatrix);
/* 场景变换追加平移。 */
XGE_API int xgeShapeExSceneTransformTranslate(xge_shape_ex_scene pScene, float fTX, float fTY);
/* 场景变换追加缩放。 */
XGE_API int xgeShapeExSceneTransformScale(xge_shape_ex_scene pScene, float fSX, float fSY);
/* 场景变换追加旋转（弧度）。 */
XGE_API int xgeShapeExSceneTransformRotate(xge_shape_ex_scene pScene, float fRadians);
/* 场景变换追加斜切。 */
XGE_API int xgeShapeExSceneTransformSkew(xge_shape_ex_scene pScene, float fXRadians, float fYRadians);
/* 设置场景整体不透明度。 */
XGE_API int xgeShapeExSceneOpacity(xge_shape_ex_scene pScene, float fOpacity);
/* 读取场景不透明度。 */
XGE_API int xgeShapeExSceneOpacityGet(xge_shape_ex_scene pScene, float* pOpacity);
/* 设置场景可见性。 */
XGE_API int xgeShapeExSceneVisible(xge_shape_ex_scene pScene, int bVisible);
/* 读取场景可见性。 */
XGE_API int xgeShapeExSceneVisibleGet(xge_shape_ex_scene pScene, int* pVisible);
/* 设置场景混合模式（离屏合成整棵子树）。 */
XGE_API int xgeShapeExSceneBlend(xge_shape_ex_scene pScene, int iBlend);
/* 恢复场景默认混合。 */
XGE_API int xgeShapeExSceneBlendClear(xge_shape_ex_scene pScene);
/* 输出场景混合模式与是否显式设置。 */
XGE_API int xgeShapeExSceneBlendGet(xge_shape_ex_scene pScene, int* pBlend, int* pBlendSet);
/* 场景级以路径为遮罩。 */
XGE_API int xgeShapeExSceneMaskShapeSet(xge_shape_ex_scene pScene, xge_shape_ex pTarget, int iMethod);
/* 场景级以场景为遮罩。 */
XGE_API int xgeShapeExSceneMaskSceneSet(xge_shape_ex_scene pScene, xge_shape_ex_scene pTarget, int iMethod);
/* 移除场景遮罩。 */
XGE_API int xgeShapeExSceneMaskClear(xge_shape_ex_scene pScene);
/* 输出场景遮罩（方法/目标类型/句柄）。 */
XGE_API int xgeShapeExSceneMaskGet(xge_shape_ex_scene pScene, int* pMethod, int* pTargetType, xge_shape_ex* ppTargetShape, xge_shape_ex_scene* ppTargetScene);
/* 场景级矩形裁剪。 */
XGE_API int xgeShapeExSceneClipRectSet(xge_shape_ex_scene pScene, xge_rect_t tRect);
/* 输出场景矩形裁剪与启用状态。 */
XGE_API int xgeShapeExSceneClipRectGet(xge_shape_ex_scene pScene, xge_rect_t* pRect, int* pEnabled);
/* 场景级追加路径裁剪。 */
XGE_API int xgeShapeExSceneClipShapeAdd(xge_shape_ex_scene pScene, xge_shape_ex pClipShape);
/* 场景级追加路径裁剪并指定模板模式。 */
XGE_API int xgeShapeExSceneClipShapeAddEx(xge_shape_ex_scene pScene, xge_shape_ex pClipShape, int iMode);
/* 读取场景路径裁剪数量。 */
XGE_API int xgeShapeExSceneClipShapeGetCount(xge_shape_ex_scene pScene, int* pCount);
/* 按索引取场景路径裁剪。 */
XGE_API int xgeShapeExSceneClipShapeGetAt(xge_shape_ex_scene pScene, int iIndex, xge_shape_ex* ppClipShape);
/* 按索引取场景路径裁剪及模式。 */
XGE_API int xgeShapeExSceneClipShapeGetAtEx(xge_shape_ex_scene pScene, int iIndex, xge_shape_ex* ppClipShape, int* pMode);
/* 清空场景路径裁剪。 */
XGE_API int xgeShapeExSceneClipShapeClear(xge_shape_ex_scene pScene);
/* 清空场景全部裁剪。 */
XGE_API int xgeShapeExSceneClipClear(xge_shape_ex_scene pScene);
/* 清空场景级滤镜特效。 */
XGE_API int xgeShapeExSceneEffectClear(xge_shape_ex_scene pScene);
/* 场景高斯模糊（sigma/方向/边框策略/质量档）。 */
XGE_API int xgeShapeExSceneEffectGaussianBlur(xge_shape_ex_scene pScene, float fSigma, int iDirection, int iBorder, int iQuality);
/* 场景投影（颜色/角度/距离/模糊）。 */
XGE_API int xgeShapeExSceneEffectDropShadow(xge_shape_ex_scene pScene, uint32_t iColor, float fAngleDegrees, float fDistance, float fSigma, int iQuality);
/* 以颜色填充场景 alpha 区域。 */
XGE_API int xgeShapeExSceneEffectFill(xge_shape_ex_scene pScene, uint32_t iColor);
/* 双色映射调色（黑/白端点 + 强度）。 */
XGE_API int xgeShapeExSceneEffectTint(xge_shape_ex_scene pScene, uint32_t iBlackColor, uint32_t iWhiteColor, float fIntensity);
/* 三色调映射（阴影/中间/高光）。 */
XGE_API int xgeShapeExSceneEffectTritone(xge_shape_ex_scene pScene, uint32_t iShadowColor, uint32_t iMidtoneColor, uint32_t iHighlightColor, int iBlend);
/* 读取场景特效数量。 */
XGE_API int xgeShapeExSceneEffectGetCount(xge_shape_ex_scene pScene, int* pCount);
/* 按索引输出特效描述。 */
XGE_API int xgeShapeExSceneEffectGetAt(xge_shape_ex_scene pScene, int iIndex, xge_shape_ex_scene_effect_t* pEffect);
/* 计算场景内容包围盒。 */
XGE_API int xgeShapeExSceneGetBounds(xge_shape_ex_scene pScene, float fTolerance, xge_rect_t* pBounds);
/* 计算场景定向包围盒（4 顶点）。 */
XGE_API int xgeShapeExSceneGetOBB(xge_shape_ex_scene pScene, float fTolerance, xge_vec2_t* pPoints4);
/* 快速判断场景包围盒与矩形相交。 */
XGE_API int xgeShapeExSceneBoundsIntersects(xge_shape_ex_scene pScene, xge_rect_t tRect, float fTolerance, int* pIntersects);
/* 精确判断场景内容与矩形相交。 */
XGE_API int xgeShapeExSceneIntersects(xge_shape_ex_scene pScene, xge_rect_t tRect, float fTolerance, int* pIntersects);
/* 精确相交判断（叠加父级矩阵）。 */
XGE_API int xgeShapeExSceneIntersectsEx(xge_shape_ex_scene pScene, xge_rect_t tRect, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, int* pIntersects);
/* 场景内容命中测试。 */
XGE_API int xgeShapeExSceneContainsPoint(xge_shape_ex_scene pScene, float fX, float fY, float fTolerance, int* pContains);
/* 场景命中测试（叠加父级矩阵）。 */
XGE_API int xgeShapeExSceneContainsPointEx(xge_shape_ex_scene pScene, float fX, float fY, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, int* pContains);
/* 命中测试并输出命中的子路径；未命中返回错误。 */
XGE_API int xgeShapeExSceneHitTest(xge_shape_ex_scene pScene, float fX, float fY, float fTolerance, xge_shape_ex* ppShape);
/* 命中测试（叠加父级矩阵）。 */
XGE_API int xgeShapeExSceneHitTestEx(xge_shape_ex_scene pScene, float fX, float fY, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, xge_shape_ex* ppShape);
/* 绘制整棵场景树。 */
XGE_API int xgeShapeExSceneDraw(xge_shape_ex_scene pScene, float fTolerance);
/* 像素对齐绘制场景树。 */
XGE_API int xgeShapeExSceneDrawPx(xge_shape_ex_scene pScene, float fTolerance);
/* 绘制场景树（叠加父级矩阵与透明度）。 */
XGE_API int xgeShapeExSceneDrawEx(xge_shape_ex_scene pScene, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, float fParentOpacity);
/* 像素对齐绘制（叠加父级矩阵与透明度）。 */
XGE_API int xgeShapeExSceneDrawPxEx(xge_shape_ex_scene pScene, float fTolerance, const xge_shape_ex_matrix_t* pParentMatrix, float fParentOpacity);
#endif
/* 创建空 SVG 文档；*ppSvg 输出句柄，失败时不被触碰。 */
#if XGE_ENABLE_SVG
XGE_API int xgeSvgCreate(xge_svg* ppSvg);
/* 释放文档引用；计数归零时销毁，此后句柄无效。 */
XGE_API void xgeSvgDestroy(xge_svg pSvg);
/* 清空文档内容（保留句柄）。 */
XGE_API int xgeSvgClear(xge_svg pSvg);
/* 深拷贝文档；*ppClone 为独立新句柄（引用计数 1）。 */
XGE_API int xgeSvgClone(xge_svg pSvg, xge_svg* ppClone);
/* 从 URI 解析并替换文档内容；sURI 走资源 provider（支持 res:// 打包路径）。 */
XGE_API int xgeSvgLoad(xge_svg pSvg, const char* sURI);
/* 按 URI 全局缓存加载：命中返回共享引用，未命中加载后入缓存。 */
XGE_API int xgeSvgLoadCached(const char* sURI, xge_svg* ppSvg);
/* 从内存缓冲解析 SVG 文本，语义同 xgeSvgLoad。 */
XGE_API int xgeSvgLoadMemory(xge_svg pSvg, const void* pData, int iSize);
/* 引用计数加一。 */
XGE_API int xgeSvgAddRef(xge_svg pSvg);
/* 读取当前引用计数。 */
XGE_API int xgeSvgRefCountGet(xge_svg pSvg, int* pRefCount);
/* 使指定 URI 的缓存条目失效（下次重新加载）；条目不存在返回 XGE_ERROR_NOT_FOUND。 */
XGE_API int xgeSvgCacheInvalidate(const char* sURI);
/* 清空全部 SVG 文档缓存。 */
XGE_API void xgeSvgCacheClear(void);
/* 覆盖文档渲染尺寸（用户单位）。 */
XGE_API int xgeSvgSetSize(xge_svg pSvg, float fWidth, float fHeight);
/* 读取文档渲染尺寸；尚未设置时返回 XGE_ERROR_INVALID_STATE。 */
XGE_API int xgeSvgGetSize(xge_svg pSvg, float* pWidth, float* pHeight);
/* 设置渲染原点偏移。 */
XGE_API int xgeSvgSetOrigin(xge_svg pSvg, float fX, float fY);
/* 读取渲染原点。 */
XGE_API int xgeSvgGetOrigin(xge_svg pSvg, float* pX, float* pY);
/* 输出根元素 viewBox；文档未声明时不修改 pViewBox。 */
XGE_API int xgeSvgGetViewBox(xge_svg pSvg, xge_rect_t* pViewBox);
/* 按 id 查找文档节点；无匹配返回 XGE_ERROR_NOT_FOUND。 */
XGE_API int xgeSvgPaintGetById(xge_svg pSvg, uint32_t iId, xge_svg_paint* ppPaint);
/* 按名称查找文档节点（首个匹配）。 */
XGE_API int xgeSvgPaintGetByName(xge_svg pSvg, const char* sName, xge_svg_paint* ppPaint);
/* 获取文档根节点。 */
XGE_API int xgeSvgPaintGetPicture(xge_svg pSvg, xge_svg_paint* ppPaint);
/* 读取节点数值 id。 */
XGE_API int xgeSvgPaintIdGet(xge_svg_paint pPaint, uint32_t* pId);
/* 读取节点名称；未命名返回 XGE_ERROR_NOT_FOUND。 */
XGE_API int xgeSvgPaintNameGet(xge_svg_paint pPaint, const char** ppName);
/* 读取节点元素类型编码（标签种类）。 */
XGE_API int xgeSvgPaintTypeGet(xge_svg_paint pPaint, int* pType);
/* 以矩阵整体覆盖节点变换。 */
XGE_API int xgeSvgPaintTransformSet(xge_svg_paint pPaint, const xge_shape_ex_matrix_t* pMatrix);
/* 重置节点变换为单位矩阵。 */
XGE_API int xgeSvgPaintTransformIdentity(xge_svg_paint pPaint);
/* 读取节点当前累计变换矩阵。 */
XGE_API int xgeSvgPaintTransformGet(xge_svg_paint pPaint, xge_shape_ex_matrix_t* pMatrix);
/* 在当前变换上追加平移。 */
XGE_API int xgeSvgPaintTransformTranslate(xge_svg_paint pPaint, float fTX, float fTY);
/* 在当前变换上追加缩放。 */
XGE_API int xgeSvgPaintTransformScale(xge_svg_paint pPaint, float fSX, float fSY);
/* 在当前变换上追加旋转，角度为弧度。 */
XGE_API int xgeSvgPaintTransformRotate(xge_svg_paint pPaint, float fRadians);
/* 设置节点不透明度，范围 [0,1]。 */
XGE_API int xgeSvgPaintOpacitySet(xge_svg_paint pPaint, float fOpacity);
/* 读取节点不透明度。 */
XGE_API int xgeSvgPaintOpacityGet(xge_svg_paint pPaint, float* pOpacity);
/* 设置节点可见性（0 隐藏）。 */
XGE_API int xgeSvgPaintVisibleSet(xge_svg_paint pPaint, int bVisible);
/* 读取节点可见性。 */
XGE_API int xgeSvgPaintVisibleGet(xge_svg_paint pPaint, int* pVisible);
/* 设置节点混合模式（ShapeEx 合成模式编码）。 */
XGE_API int xgeSvgPaintBlendSet(xge_svg_paint pPaint, int iBlend);
/* 读取节点混合模式。 */
XGE_API int xgeSvgPaintBlendGet(xge_svg_paint pPaint, int* pBlend);
/* 以 ShapeEx 形状作为节点遮罩；iMethod 指定 Alpha/Luma。 */
XGE_API int xgeSvgPaintMaskShapeSet(xge_svg_paint pPaint, xge_shape_ex pTarget, int iMethod);
/* 以 ShapeEx 场景作为节点遮罩；iMethod 指定 Alpha/Luma。 */
XGE_API int xgeSvgPaintMaskSceneSet(xge_svg_paint pPaint, xge_shape_ex_scene pTarget, int iMethod);
/* 移除节点遮罩。 */
XGE_API int xgeSvgPaintMaskClear(xge_svg_paint pPaint);
/* 读取当前遮罩：方法、目标类型及形状/场景句柄。 */
XGE_API int xgeSvgPaintMaskGet(xge_svg_paint pPaint, int* pMethod, int* pTargetType, xge_shape_ex* ppTargetShape, xge_shape_ex_scene* ppTargetScene);
/* 追加一个 ShapeEx 形状作为节点裁剪。 */
XGE_API int xgeSvgPaintClipShapeSet(xge_svg_paint pPaint, xge_shape_ex pClipShape);
/* 清空节点裁剪。 */
XGE_API int xgeSvgPaintClipClear(xge_svg_paint pPaint);
/* 输出裁剪包络矩形与启用状态。 */
XGE_API int xgeSvgPaintClipRectGet(xge_svg_paint pPaint, xge_rect_t* pRect, int* pEnabled);
/* 读取裁剪形状数量。 */
XGE_API int xgeSvgPaintClipShapeGetCount(xge_svg_paint pPaint, int* pCount);
/* 按索引取裁剪形状；越界返回 XGE_ERROR_NOT_FOUND。 */
XGE_API int xgeSvgPaintClipShapeGetAt(xge_svg_paint pPaint, int iIndex, xge_shape_ex* ppClipShape);
/* 按索引取裁剪形状及其裁剪模式。 */
XGE_API int xgeSvgPaintClipShapeGetAtEx(xge_svg_paint pPaint, int iIndex, xge_shape_ex* ppClipShape, int* pMode);
/* 取节点绑定的 ShapeEx 图形；非图形类节点返回 XGE_ERROR_NOT_FOUND。 */
XGE_API int xgeSvgPaintShapeGet(xge_svg_paint pPaint, xge_shape_ex* ppShape);
/* 取节点所属文档。 */
XGE_API int xgeSvgPaintOwnerGet(xge_svg_paint pPaint, xge_svg* ppSvg);
/* 取父节点；根节点时输出 NULL。 */
XGE_API int xgeSvgPaintParentGet(xge_svg_paint pPaint, xge_svg_paint* ppParent);
/* 读取直接子节点数量。 */
XGE_API int xgeSvgPaintChildGetCount(xge_svg_paint pPaint, int* pCount);
/* 按索引取子节点；越界返回 XGE_ERROR_NOT_FOUND。 */
XGE_API int xgeSvgPaintChildGetAt(xge_svg_paint pPaint, int iIndex, xge_svg_paint* ppChild);
/* 深度优先遍历子树并回调每个节点（含自身）。 */
XGE_API int xgeSvgPaintTraverse(xge_svg_paint pPaint, xge_svg_paint_visit_proc onPaint, void* pUser);
/* 计算节点局部包围盒；fTolerance 为曲线细分容差。 */
XGE_API int xgeSvgPaintGetBounds(xge_svg_paint pPaint, float fTolerance, xge_rect_t* pBounds);
/* 计算节点经自身变换后的定向包围盒，输出 4 顶点。 */
XGE_API int xgeSvgPaintGetOBB(xge_svg_paint pPaint, float fTolerance, xge_vec2_t* pPoints4);
/* 判断节点与矩形是否相交（含容差）。 */
XGE_API int xgeSvgPaintIntersects(xge_svg_paint pPaint, xge_rect_t tRect, float fTolerance, int* pIntersects);
/* 设置 preserveAspectRatio 策略字符串（如 "xMidYMid meet"）。 */
XGE_API int xgeSvgSetPreserveAspectRatio(xge_svg pSvg, const char* sValue);
/* 输出 preserveAspectRatio 解析结果（水平/垂直对齐与 meet/slice）。 */
XGE_API int xgeSvgGetPreserveAspectRatio(xge_svg pSvg, int* pAlignX, int* pAlignY, int* pMeetOrSlice);
/* 计算绘制到 tDst 时经 viewBox 与纵横比适配后的实际视口。 */
XGE_API int xgeSvgGetDrawViewport(xge_svg pSvg, xge_rect_t tDst, xge_rect_t* pViewport);
/* 计算文档内容包围盒（用户坐标）。 */
XGE_API int xgeSvgGetBounds(xge_svg pSvg, float fTolerance, xge_rect_t* pBounds);
/* 计算绘制到 tDst 后的屏幕空间包围盒。 */
XGE_API int xgeSvgGetDrawBounds(xge_svg pSvg, xge_rect_t tDst, float fTolerance, xge_rect_t* pBounds);
/* 用户坐标命中测试（含容差）。 */
XGE_API int xgeSvgContainsPoint(xge_svg pSvg, float fX, float fY, float fTolerance, int* pContains);
/* 绘制空间命中测试：坐标先经 tDst 适配变换再测试。 */
XGE_API int xgeSvgDrawContainsPoint(xge_svg pSvg, xge_rect_t tDst, float fX, float fY, float fTolerance, int* pContains);
/* 绘制到目标矩形（自动 viewBox/纵横比适配；浮点坐标）。 */
XGE_API int xgeSvgDraw(xge_svg pSvg, xge_rect_t tDst, float fTolerance);
/* 像素对齐版本的 xgeSvgDraw。 */
XGE_API int xgeSvgDrawPx(xge_svg pSvg, xge_rect_t tDst, float fTolerance);
/* 离屏光栅化 URI 文档到调用方缓冲（RGBA8，行距 iStride 字节）。 */
XGE_API int xgeSvgRasterize(const char* sURI, int iWidth, int iHeight, void* pPixels, int iStride);
/* 离屏光栅化内存文档，语义同 xgeSvgRasterize。 */
XGE_API int xgeSvgRasterizeMemory(const void* pData, int iSize, int iWidth, int iHeight, void* pPixels, int iStride);
/* 加载 URI 文档并光栅化到纹理（iWidth×iHeight）。 */
XGE_API int xgeSvgTextureLoad(xge_texture pTexture, const char* sURI, int iWidth, int iHeight);
/* Rasterize an SVG URI with XGE_TEXTURE_COMPRESS_* creation flags. */
XGE_API int xgeSvgTextureLoadEx(xge_texture pTexture, const char* sURI, int iWidth, int iHeight, uint32_t iFlags);
/* 加载内存文档并光栅化到纹理。 */
XGE_API int xgeSvgTextureLoadMemory(xge_texture pTexture, const void* pData, int iSize, int iWidth, int iHeight);
/* Rasterize SVG memory with XGE_TEXTURE_COMPRESS_* creation flags. */
XGE_API int xgeSvgTextureLoadMemoryEx(xge_texture pTexture, const void* pData, int iSize, int iWidth, int iHeight, uint32_t iFlags);
#endif
/* 创建形状批容器并预分配三角形容量。 */
#if XGE_ENABLE_2D
XGE_API int xgeShapeBatchInit(xge_shape_batch pBatch, uint32_t iColor, int iTriangleCapacity, uint32_t iFlags);
/* 释放形状批容器。 */
XGE_API void xgeShapeBatchFree(xge_shape_batch pBatch);
/* 清空批内容（保留容量）。 */
XGE_API void xgeShapeBatchClear(xge_shape_batch pBatch);
/* 向批追加一个三角形。 */
XGE_API int xgeShapeBatchTriangleFill(xge_shape_batch pBatch, xge_vec2_t tA, xge_vec2_t tB, xge_vec2_t tC);
/* 向批追加一个矩形（两个三角形）。 */
XGE_API int xgeShapeBatchRectFill(xge_shape_batch pBatch, xge_rect_t tRect);
/* 一次性提交并绘制批内全部三角形。 */
XGE_API int xgeShapeBatchFlush(xge_shape_batch pBatch);
#endif
/* 设置浮点视口矩形。 */
XGE_API void xgeViewportSet(xge_rect_t tRect);
/* 读取当前视口。 */
XGE_API xge_rect_t xgeViewportGet(void);
/* 恢复视口为全屏。 */
XGE_API void xgeViewportClear(void);
/* 设置浮点像素裁剪矩形。 */
XGE_API void xgeClipSet(xge_rect_t tRect);
/* 读取当前裁剪矩形。 */
XGE_API xge_rect_t xgeClipGet(void);
/* 清除裁剪，恢复全屏。 */
XGE_API void xgeClipClear(void);
/* 浮点矩形四舍五入取整。 */
XGE_API xge_rect_i_t xgeRectToPixelsNearest(xge_rect_t tRect);
/* 浮点矩形向外取整（包围盒扩张）。 */
XGE_API xge_rect_i_t xgeRectToPixelsOutward(xge_rect_t tRect);
/* 浮点矩形向内取整（收缩到整数内接）。 */
XGE_API xge_rect_i_t xgeRectToPixelsInward(xge_rect_t tRect);
/* 以整数像素坐标设置视口。 */
XGE_API void xgeViewportSetPixels(xge_rect_i_t tRect);
/* 读取整数像素视口。 */
XGE_API xge_rect_i_t xgeViewportGetPixels(void);
/* 以整数像素坐标设置裁剪矩形。 */
XGE_API void xgeClipSetPixels(xge_rect_i_t tRect);
/* 读取整数像素裁剪矩形。 */
XGE_API xge_rect_i_t xgeClipGetPixels(void);
typedef struct xge_clipboard_item_t {
	const char* sFormat;
	const void* pData;
	size_t iDataSize;
} xge_clipboard_item_t;

#define XGE_CLIPBOARD_FORMAT_TEXT_UTF8 "text/plain;charset=utf-8"
#define XGE_CLIPBOARD_FORMAT_HTML "text/html"
#define XGE_CLIPBOARD_FORMAT_IMAGE_PNG "image/png"

/* 多格式写剪贴板（多格式仅 Windows，其他平台取首文本项）。
 * Windows 的有效 PNG 项额外提供带 alpha 的 CF_DIBV5 位图表示。 */
XGE_API int xgeClipboardSetItems(const xge_clipboard_item_t* pItems, int iItemCount);
/* 按格式读剪贴板数据（多格式仅 Windows）；Windows image/png 可从
 * 常见未压缩 CF_DIB(V5) 转换。 */
XGE_API int xgeClipboardGetData(const char* sFormat, void* pData, size_t iCapacity);
/* 写纯文本到剪贴板。 */
XGE_API void xgeClipboardSetText(const char* sText);
/* 读剪贴板纯文本；空剪贴板返回 NULL。 */
XGE_API const char* xgeClipboardGetText(void);

/* 按键当前是否按住。 */
XGE_API int xgeKeyDown(int iKey);
/* 按键本帧是否刚按下。 */
XGE_API int xgeKeyPressed(int iKey);
/* 按键本帧是否产生自动重复。 */
XGE_API int xgeKeyRepeated(int iKey);
/* 按键本帧是否刚释放。 */
XGE_API int xgeKeyReleased(int iKey);
/* 标记按键已消费，屏蔽本帧后续默认处理。 */
XGE_API void xgeInputConsumeKey(int iKey);
/* 查询按键是否已被消费。 */
XGE_API int xgeInputKeyConsumed(int iKey);
/* 输出鼠标当前位置。 */
XGE_API void xgeMouseGet(float* pX, float* pY);
/* 输出鼠标本帧位移增量。 */
XGE_API void xgeMouseGetDelta(float* pDX, float* pDY);
/* 输出滚轮本帧滚动量（X/Y）。 */
XGE_API void xgeMouseGetWheel(float* pX, float* pY);
/* 鼠标键当前是否按住。 */
XGE_API int xgeMouseDown(int iButton);
/* 获取引擎默认文本字体实例。 */
XGE_API uint32_t xgeTextGet(void);
/*
 * Ordered input is the preferred API for UI integrations. The legacy key,
 * text and IME getters remain available as compatibility views; consumers
 * should choose one model and must not expect two independent deliveries.
 */
XGE_API int xgeInputEventGet(xge_input_event_t* pEvent);
/* 向输入事件队列追加合成事件。 */
XGE_API int xgeInputEventPost(const xge_input_event_t* pEvent);
/* 待轮询事件数量。 */
XGE_API int xgeInputEventPendingCount(void);
/* 队列饱和丢弃的事件累计数（诊断用）。 */
XGE_API uint64_t xgeInputEventDroppedCount(void);
/* 读取输入法启用状态。 */
XGE_API int xgeImeGetEnabled(void);
/* 开关输入法（完整实现仅 Windows TSF，其他平台无后端）。 */
XGE_API int xgeImeSetEnabled(int bEnabled);
/* 读取输入法模式（NATIVE/COMPOSITION/FULL）。 */
XGE_API int xgeImeGetMode(void);
/* 设置输入法模式；FULL 为自绘候选窗模式（仅 Windows TSF）。 */
XGE_API int xgeImeSetMode(int iMode);
/* FULL mode hides native candidate UI only while a complete presenter is ready. */
XGE_API int xgeImeSetCandidatePresenterReady(int bReady);
/* 设置组合串目标矩形（候选窗定位锚点）。 */
XGE_API int xgeImeSetCandidateRect(xge_rect_t tRect);
/* 读取候选窗锚点矩形。 */
XGE_API int xgeImeGetCandidateRect(xge_rect_t* pRect);
/* 注册文本客户端回调（组合串提交/替换范围/快照回推）。 */
XGE_API int xgeImeSetTextClient(const xge_ime_text_client_t* pClient);
/* 通知输入法文本客户端内容已变更（同步组合状态）。 */
XGE_API int xgeImeRefreshTextClient(void);
/* 输出候选列表信息（条数/页/当前选择）。 */
XGE_API int xgeImeCandidateGetInfo(xge_ime_candidate_info_t* pInfo);
/* Returns the complete UTF-8 byte length, excluding the terminator. */
XGE_API int xgeImeCandidateGetText(int iIndex, char* sText, int iCapacity);
/* 选中指定候选并发送给输入法。 */
XGE_API int xgeImeCandidateSelect(int iIndex);
/* 结束候选交互（自绘窗关闭并提交状态）。 */
XGE_API int xgeImeCandidateFinalize(void);
/* sText remains valid until the next xgeImeEventGet call or xgeUnit. */
XGE_API int xgeImeEventGet(xge_ime_event_t* pEvent);
/* 当前触摸点数量（最多 8 点）。 */
XGE_API int xgeTouchGetCount(void);
/* 按索引取触摸点状态。 */
XGE_API int xgeTouchGet(int iIndex, xge_touch_point_t* pPoint);
/* 按触摸 ID 查找触摸点；不存在返回 XGE_ERROR_NOT_FOUND。 */
XGE_API int xgeTouchFind(uint64_t iId, xge_touch_point_t* pPoint);
/* 手柄是否连接。 */
XGE_API int xgeGamepadConnected(int iGamepad);
/* 输出手柄完整状态（按钮位与轴值）。 */
XGE_API int xgeGamepadGetState(int iGamepad, xge_gamepad_state_t* pState);
/* 手柄按钮当前是否按住。 */
XGE_API int xgeGamepadButtonDown(int iGamepad, uint32_t iButton);
/* 手柄按钮本帧是否刚按下。 */
XGE_API int xgeGamepadButtonPressed(int iGamepad, uint32_t iButton);
/* 手柄按钮本帧是否刚释放。 */
XGE_API int xgeGamepadButtonReleased(int iGamepad, uint32_t iButton);
/* 读取手柄轴值 [-1,1]。 */
XGE_API float xgeGamepadAxis(int iGamepad, int iAxis);
/* 注入手柄连接状态（引擎无系统手柄事件源，需外部桥接）。 */
XGE_API int xgeGamepadSetConnected(int iGamepad, int bConnected);
/* 注入手柄完整状态（引擎无系统手柄事件源，需外部桥接）。 */
XGE_API int xgeGamepadSetState(int iGamepad, const xge_gamepad_state_t* pState);

/* CPU 2D particles. Simulation needs no window, GPU, Sprite, XUI or animation player.
 * All world operations are single-threaded; callbacks must not mutate the world.
 * Descriptions are copied. Renderer resource bindings are separate from simulation.
 * See docs/guide/PARTICLES.md for units, time, ownership and capacity contracts. */
#define XGE_PARTICLE_MAX_KEYS 8
#define XGE_PARTICLE_MAX_BURSTS 16
#define XGE_PARTICLE_MAX_EMITTERS 16
#define XGE_PARTICLE_NO_EMITTER (-1)
#define XGE_PARTICLE_CURVE_LINEAR 0
#define XGE_PARTICLE_CURVE_STEP 1
#define XGE_PARTICLE_CURVE_HERMITE 2
#define XGE_PARTICLE_SHAPE_POINT 0
#define XGE_PARTICLE_SHAPE_LINE 1
#define XGE_PARTICLE_SHAPE_RECT 2
#define XGE_PARTICLE_SHAPE_CIRCLE 3
#define XGE_PARTICLE_SHAPE_RING 4
#define XGE_PARTICLE_SHAPE_CONE 5
#define XGE_PARTICLE_SPACE_WORLD 0
#define XGE_PARTICLE_SPACE_LOCAL 1
#define XGE_PARTICLE_PLAYING 0
#define XGE_PARTICLE_PAUSED 1
#define XGE_PARTICLE_DRAINING 2
#define XGE_PARTICLE_FINISHED 3
#define XGE_PARTICLE_EVENT_BIRTH 0
#define XGE_PARTICLE_EVENT_DEATH 1
#define XGE_PARTICLE_EVENT_COLLISION 2
#define XGE_PARTICLE_EVENT_FINISHED 3
#define XGE_PARTICLE_COLLISION_NONE 0
#define XGE_PARTICLE_COLLISION_BOUNCE 1
#define XGE_PARTICLE_COLLISION_KILL 2
#define XGE_PARTICLE_CULL_DRAW 0
#define XGE_PARTICLE_CULL_PAUSE 1
#define XGE_PARTICLE_CULL_CLEAR 2

typedef struct xge_particle_definition_t* xge_particle_definition;
typedef struct xge_particle_world_t* xge_particle_world;
typedef struct xge_particle_renderer_t* xge_particle_renderer;
typedef uint64_t xge_particle_effect; /* World-scoped generational handle; zero invalid. */

typedef struct xge_particle_range_t { float fMin, fMax; } xge_particle_range_t;
typedef struct xge_particle_key_t {
	float fTime, fValue, fInTangent, fOutTangent;
} xge_particle_key_t;
typedef struct xge_particle_curve_t {
	int iCount, iInterpolation;
	xge_particle_key_t arrKeys[XGE_PARTICLE_MAX_KEYS];
} xge_particle_curve_t;
typedef struct xge_particle_color_key_t { float fTime; uint32_t iColor; } xge_particle_color_key_t;
typedef struct xge_particle_gradient_t {
	int iCount;
	xge_particle_color_key_t arrKeys[XGE_PARTICLE_MAX_KEYS];
} xge_particle_gradient_t;
typedef struct xge_particle_burst_t { float fTime; uint32_t iCount; } xge_particle_burst_t;
typedef struct xge_particle_subemitter_t {
	int iEmitter;
	uint32_t iCount;
	float fInheritVelocity;
} xge_particle_subemitter_t;

typedef struct xge_particle_emitter_t {
	char sName[64], sTexture[128], sMaterial[64]; /* Logical resource names, not loaded by the CPU core. */
	uint32_t iMaxParticles;
	int bAutomatic, bLoop, iSpace, iShape;
	float fDelay, fDuration, fRate, fRateOverDistance;
	xge_vec2_t tOffset, tShapeSize;
	float fRadius, fInnerRadius, fDirection, fSpread;
	xge_particle_range_t tLife, tSpeed, tSize, tRotation, tAngularVelocity, tStartFrame;
	uint32_t iColorMin, iColorMax;
	xge_particle_curve_t tSizeOverLife, tSpeedOverLife, tAlphaOverLife;
	xge_particle_gradient_t tColorOverLife;
	xge_vec2_t tGravity;
	float fDrag, fRadialAcceleration, fTangentialAcceleration, fInheritVelocity;
	float fNoiseStrength, fNoiseFrequency, fNoiseScroll;
	float fAspect, fStretch;
	int bAlignVelocity, bScreenSpace, iBlend, iLayer, iOrder;
	xge_rect_t tTextureRect; /* Pixel rectangle; zero width/height selects the whole texture. */
	int iColumns, iRows;
	float fFramesPerSecond; /* Zero fits the sheet to particle lifetime. */
	int iCollision;
	float fCollisionRadius, fRestitution, fFriction;
	int iBurstCount;
	xge_particle_burst_t arrBursts[XGE_PARTICLE_MAX_BURSTS];
	xge_particle_subemitter_t arrSubEmitters[3]; /* Birth, death, collision; acyclic only. */
} xge_particle_emitter_t;

typedef struct xge_particle_transform_t {
	xge_vec2_t tPosition, tScale;
	float fRotation; /* Radians; scale components must be positive. */
} xge_particle_transform_t;
typedef struct xge_particle_parameters_t {
	float fRateScale, fSpeedScale, fSizeScale, fTimeScale;
	uint32_t iTint;
} xge_particle_parameters_t;
typedef struct xge_particle_play_t {
	xge_particle_transform_t tTransform;
	xge_particle_parameters_t tParameters;
	uint64_t iSeed;
	int bAutoRelease, iCullPolicy;
} xge_particle_play_t;

typedef struct xge_particle_hit_t {
	xge_vec2_t tPosition, tNormal; /* Swept particle center and outward unit normal in world/screen space. */
	float fFraction; /* [0,1] along the queried segment. */
} xge_particle_hit_t;
typedef int (*xge_particle_collision_proc)(xge_vec2_t tFrom, xge_vec2_t tTo,
	float fRadius, xge_particle_hit_t* pHit, void* pUser);
typedef struct xge_particle_world_desc_t {
/* 五个容量上限：效果实例/全局粒子/事件队列/单步生成/单次更新子步（超限丢弃并计数）。 */
	uint32_t iMaxEffects, iMaxParticles, iMaxEvents, iMaxSpawnPerStep, iMaxSubsteps;
/* 固定仿真步长（秒）。 */
	float fFixedStep;
/* 碰撞响应回调；NULL 关闭碰撞。 */
	xge_particle_collision_proc pCollision;
/* 碰撞回调用户指针。 */
	void* pCollisionUser;
} xge_particle_world_desc_t;
typedef struct xge_particle_event_t {
	int iType, iEmitter;
	xge_particle_effect iEffect;
	uint64_t iParticle;
	xge_vec2_t tPosition, tVelocity;
} xge_particle_event_t;
typedef struct xge_particle_snapshot_t {
	xge_particle_effect iEffect;
	xge_particle_definition pDefinition; /* Borrowed during Visit only. */
	uint64_t iParticle;
	int iEmitter;
	xge_vec2_t tPosition, tVelocity, tSize;
	float fAge, fLife, fRotation, fFrame;
	uint32_t iColor;
} xge_particle_snapshot_t;
typedef int (*xge_particle_visit_proc)(const xge_particle_snapshot_t* pParticle, void* pUser);
typedef struct xge_particle_stats_t {
	uint32_t iActiveEffects, iCachedEffects, iLiveParticles, iPeakParticles, iPendingEvents;
	uint64_t iSpawned, iDroppedParticles, iDroppedEvents, iCollisions, iSteps;
	double fDroppedTime, fLastUpdateSeconds;
} xge_particle_stats_t;

/* 发射器描述重置为默认值。 */
#if XGE_ENABLE_PARTICLES
XGE_API void xgeParticleEmitterInit(xge_particle_emitter_t* pEmitter);
/* 世界描述重置为默认值。 */
XGE_API void xgeParticleWorldDescInit(xge_particle_world_desc_t* pDesc);
/* 播放参数重置为默认值。 */
XGE_API void xgeParticlePlayInit(xge_particle_play_t* pPlay);
/* 采样 Hermite 关键帧曲线；无关键帧时返回 fDefault。 */
XGE_API float xgeParticleCurveEval(const xge_particle_curve_t* pCurve, float fTime, float fDefault);
/* 采样颜色渐变；无关键帧时返回 iDefault。 */
XGE_API uint32_t xgeParticleGradientEval(const xge_particle_gradient_t* pGradient, float fTime, uint32_t iDefault);
/* 以发射器数组创建粒子定义。 */
XGE_API int xgeParticleDefinitionCreate(xge_particle_definition* ppDefinition,
	const xge_particle_emitter_t* pEmitters, int iCount);
/* 定义引用计数加一。 */
XGE_API void xgeParticleDefinitionAddRef(xge_particle_definition pDefinition);
/* 定义引用计数减一，归零时释放。 */
XGE_API void xgeParticleDefinitionFree(xge_particle_definition pDefinition);
/* 读取定义内发射器数量。 */
XGE_API int xgeParticleDefinitionCount(xge_particle_definition pDefinition);
/* 按索引取出发射器描述。 */
XGE_API int xgeParticleDefinitionGet(xge_particle_definition pDefinition, int iEmitter, xge_particle_emitter_t* pOut);
/* JSON and XSON share schema version 1. Output is UTF-8, freed with xrtFree.
 * Invalid input leaves *ppDefinition NULL. Error text is optional and bounded. */
XGE_API int xgeParticleDefinitionParse(xge_particle_definition* ppDefinition, const char* sText,
	size_t iSize, int bXson, char* sError, size_t iErrorSize);
/* 从 URI 加载并解析定义；失败时 sError 输出原因。 */
XGE_API int xgeParticleDefinitionLoad(xge_particle_definition* ppDefinition, const char* sURI,
	int bXson, char* sError, size_t iErrorSize);
/* 序列化定义为 JSON/XSON 文本；*ppText 由调用方释放。 */
XGE_API int xgeParticleDefinitionStringify(xge_particle_definition pDefinition, int bXson, char** ppText, size_t* pSize);
/* 创建粒子仿真世界（纯 CPU 定步长）。 */
XGE_API int xgeParticleWorldCreate(xge_particle_world* ppWorld, const xge_particle_world_desc_t* pDesc);
/* 释放世界（不影响定义）。 */
XGE_API void xgeParticleWorldFree(xge_particle_world pWorld);
/* Reserve caches inactive instances and their buffers; partial success is retained on OOM. */
XGE_API int xgeParticleWorldReserve(xge_particle_world pWorld, xge_particle_definition pDefinition, uint32_t iCount);
/* 在世界中播放一个效果实例；*pEffect 输出效果句柄。 */
XGE_API int xgeParticlePlay(xge_particle_world pWorld, xge_particle_definition pDefinition,
	const xge_particle_play_t* pPlay, xge_particle_effect* pEffect);
/* 释放效果实例（立即移除）。 */
XGE_API int xgeParticleRelease(xge_particle_world pWorld, xge_particle_effect iEffect);
/* 以指定随机种子重置效果重新播放。 */
XGE_API int xgeParticleRestart(xge_particle_world pWorld, xge_particle_effect iEffect, uint64_t iSeed);
/* 暂停/继续效果仿真。 */
XGE_API int xgeParticlePause(xge_particle_world pWorld, xge_particle_effect iEffect, int bPaused);
/* 停止效果；bClear 为真时立即清空粒子。 */
XGE_API int xgeParticleStop(xge_particle_world pWorld, xge_particle_effect iEffect, int bClear);
/* 设置效果实例的发射变换。 */
XGE_API int xgeParticleSetTransform(xge_particle_world pWorld, xge_particle_effect iEffect, const xge_particle_transform_t* pTransform);
/* 覆盖效果实例的运行参数。 */
XGE_API int xgeParticleSetParameters(xge_particle_world pWorld, xge_particle_effect iEffect, const xge_particle_parameters_t* pParameters);
/* 设置效果实例可见性。 */
XGE_API int xgeParticleSetVisible(xge_particle_world pWorld, xge_particle_effect iEffect, int bVisible);
/* 读取效果实例状态（播放/暂停/结束）。 */
XGE_API int xgeParticleState(xge_particle_world pWorld, xge_particle_effect iEffect);
/* 输出效果粒子包围盒。 */
XGE_API int xgeParticleBounds(xge_particle_world pWorld, xge_particle_effect iEffect, xge_rect_t* pBounds);
/* Manual emission returns actual count through pSpawned; capacity loss is reported in stats. */
XGE_API int xgeParticleEmit(xge_particle_world pWorld, xge_particle_effect iEffect, int iEmitter, uint32_t iCount, uint32_t* pSpawned);
/* 推进世界仿真（定步长累积，fDeltaSeconds 秒）。 */
XGE_API int xgeParticleUpdate(xge_particle_world pWorld, double fDeltaSeconds);
/* Prewarm simulates this effect only; public events suppressed, internal subemitters still run. */
XGE_API int xgeParticlePrewarm(xge_particle_world pWorld, xge_particle_effect iEffect, float fSeconds);
/* 遍历世界存活粒子并回调（自绘渲染用）。 */
XGE_API int xgeParticleVisit(xge_particle_world pWorld, xge_particle_visit_proc pVisit, void* pUser);
/* 轮询世界事件队列（碰撞/死亡等）；无事件返回非零。 */
XGE_API int xgeParticleEventPoll(xge_particle_world pWorld, xge_particle_event_t* pEvent);
/* 读取世界运行统计（存活数/丢弃计数等）。 */
XGE_API xge_particle_stats_t xgeParticleStats(xge_particle_world pWorld);
/* Optional collision query helpers; plane's allowed side is dot(P, normal) >= offset. */
XGE_API int xgeParticleCollidePlane(xge_vec2_t tFrom, xge_vec2_t tTo, float fRadius,
	xge_vec2_t tNormal, float fOffset, xge_particle_hit_t* pHit);
/* 粒子扫掠与矩形碰撞检测（纯函数，世界无关）。 */
XGE_API int xgeParticleCollideRect(xge_vec2_t tFrom, xge_vec2_t tTo, float fRadius,
	xge_rect_t tRect, xge_particle_hit_t* pHit);

/* Renderer holds references to definitions, textures and shader resources.
 * Their caller-owned C structs must remain alive until unbound/renderer destruction. */
XGE_API int xgeParticleRendererCreate(xge_particle_renderer* ppRenderer, uint32_t iCapacity, uint32_t iBatchCapacity);
/* 释放粒子渲染器。 */
XGE_API void xgeParticleRendererFree(xge_particle_renderer pRenderer);
/* 为定义的指定发射器绑定纹理/材质。 */
XGE_API int xgeParticleRendererBind(xge_particle_renderer pRenderer, xge_particle_definition pDefinition,
	int iEmitter, xge_texture pTexture, const xge_material_t* pMaterial);
/* 解除定义的渲染绑定。 */
XGE_API int xgeParticleRendererUnbind(xge_particle_renderer pRenderer, xge_particle_definition pDefinition);
/* Explicit view rectangle uses the same coordinates as particles; NULL disables draw culling.
 * Missing bindings draw a white quad. Simulation culling is controlled via SetVisible. */
XGE_API int xgeParticleRender(xge_particle_renderer pRenderer, xge_particle_world pWorld, const xge_rect_t* pView);
#endif



#if XGE_ENABLE_3D
/* Right-handed, Y up, metres and radians; column-major matrices. Scene mutation
 * and GPU calls are serialized by the caller. Nodes belong to their scene;
 * zero is the root parent. Stale and foreign handles are rejected. Destroying
 * a node destroys descendants. Reparenting retains the local matrix. */
typedef struct xge3d_scene xge3d_scene;
typedef struct xge3d_mesh xge3d_mesh;
typedef struct xge3d_renderer xge3d_renderer;
typedef struct xge3d_target xge3d_target;
typedef struct xge3d_texture xge3d_texture;
typedef struct xge3d_material xge3d_material;
typedef struct xge3d_environment xge3d_environment;
#if XGE3D_ENABLE_ASYNC
typedef struct xge3d_loader xge3d_loader;
#endif
#if XGE3D_ENABLE_TERRAIN
typedef struct xge3d_terrain xge3d_terrain;
#endif
#if XGE3D_ENABLE_ANIMATION
typedef struct xge3d_clip xge3d_clip;
typedef struct xge3d_animator xge3d_animator;
#endif
#if XGE3D_ENABLE_MODEL
typedef struct xge3d_model xge3d_model;
#endif
typedef struct xge3d_vec3_t { float x, y, z; } xge3d_vec3_t;
typedef struct xge3d_dvec3_t { double x, y, z; } xge3d_dvec3_t;
typedef struct xge3d_aabb_t { xge3d_vec3_t min, max; } xge3d_aabb_t;
typedef struct xge3d_sphere_t { xge3d_vec3_t center; float radius; } xge3d_sphere_t;
typedef struct xge3d_quat_t { float x, y, z, w; } xge3d_quat_t;
typedef struct xge3d_mat4_t { float m[16]; } xge3d_mat4_t;
typedef struct xge3d_node_t { uint64_t slot, scene; } xge3d_node_t;
typedef struct xge3d_transform_t {
    xge3d_vec3_t position;
    xge3d_quat_t rotation;
    xge3d_vec3_t scale;
} xge3d_transform_t;
typedef struct xge3d_camera_t {
    xge3d_mat4_t view, projection;
    float near_z, far_z;
} xge3d_camera_t;
/* Vertices remain in local coordinates. Tangent.w is handedness. */
typedef struct xge3d_vertex_t {
    xge3d_vec3_t position, normal;
    float uv[2], tangent[4], uv1[2];
#if XGE3D_ENABLE_ANIMATION
    uint16_t joints[4];
    float weights[4];
#endif
} xge3d_vertex_t;
/* Sampler values follow glTF's standard numeric values; 0 selects defaults. */
typedef struct xge3d_sampler_t { int min_filter, mag_filter, wrap_u, wrap_v; } xge3d_sampler_t;
typedef struct xge3d_texture_desc_t {
    const xge_image_t *image; /* RGBA8, straight alpha; copied on creation. */
    int srgb;               /* 1 for color, 0 for normal/MR/occlusion data. */
    xge3d_sampler_t sampler;
} xge3d_texture_desc_t;
#define XGE3D_MAP_BASE_COLOR 0
#define XGE3D_MAP_METALLIC_ROUGHNESS 1
#define XGE3D_MAP_NORMAL 2
#define XGE3D_MAP_OCCLUSION 3
#define XGE3D_MAP_EMISSIVE 4
#define XGE3D_MAP_COUNT 5
#define XGE3D_ALPHA_OPAQUE 0
#define XGE3D_ALPHA_MASK 1
#define XGE3D_ALPHA_BLEND 2
typedef struct xge3d_texture_binding_t {
    xge3d_texture *texture;
    int texcoord; /* 0 or 1 */
    float offset[2], scale[2], rotation;
} xge3d_texture_binding_t;
typedef struct xge3d_material_desc_t {
    float base_color[4], metallic, roughness, emissive[3];
    float normal_scale, occlusion_strength, alpha_cutoff;
    int alpha_mode, double_sided, unlit;
    xge3d_texture_binding_t maps[XGE3D_MAP_COUNT];
} xge3d_material_desc_t;
typedef struct xge3d_mesh_desc_t {
    const xge3d_vertex_t *vertices;
    size_t vertex_count;
    const void *indices;
    size_t index_count;
    int index_bits; /* 16 or 32 for indexed triangles; 0 for non-indexed. */
} xge3d_mesh_desc_t;
typedef struct xge3d_lod_t { xge3d_mesh *mesh;float min_distance; } xge3d_lod_t;
typedef struct xge3d_ray_t { xge3d_vec3_t origin, direction; } xge3d_ray_t;
/* Equal square RGBA8 faces in +X,-X,+Y,-Y,+Z,-Z order. Rows follow GL cube
 * faces: +X(-Z,-Y), -X(+Z,-Y), +Y(+X,+Z), -Y(+X,-Z), +Z(+X,-Y), -Z(-X,-Y).
 * These pairs are each face's left-to-right and top-to-bottom world axes. */
typedef struct xge3d_environment_desc_t {
    const xge_image_t *faces[6];
    int srgb;
} xge3d_environment_desc_t;
typedef struct xge3d_cube_level_t { const xge_image_t *faces[6]; } xge3d_cube_level_t;
#if XGE3D_ENABLE_IBL
typedef struct xge3d_ibl_desc_t {
    xge3d_cube_level_t irradiance; /* Cosine convolution / pi, linear RGBA8. */
    const xge3d_cube_level_t *prefiltered; /* GGX; full mip chain, roughness 0..1. */
    size_t level_count;
    const xge_image_t *brdf; /* Linear RGBA8: R=scale, G=bias; X=NdotV, Y=roughness. */
} xge3d_ibl_desc_t;
#endif
typedef struct xge3d_hit_t {
    xge3d_node_t node, model_root;
    float distance;
    xge3d_vec3_t position, normal;
    size_t triangle;
} xge3d_hit_t;
#if XGE3D_ENABLE_LIGHTING
#define XGE3D_LIGHT_DIRECTIONAL 0
#define XGE3D_LIGHT_POINT 1
#define XGE3D_LIGHT_SPOT 2
#define XGE3D_MAX_LIGHTS 8
typedef struct xge3d_light_desc_t {
    int type;
    xge3d_vec3_t color, direction; /* Linear RGB; local direction of light travel. */
    float intensity, range, inner_angle, outer_angle;
#if XGE3D_ENABLE_SHADOW
    int casts_shadow; /* Directional/spot only; point shadows are unsupported. */
#endif
} xge3d_light_desc_t;
/* Intensity is lux for directional lights, candela for point/spot. Positional
 * lights use inverse-square falloff; range is metres, 0 means unbounded. Angles are
 * radians, 0 <= inner < outer < pi/2. Node transform supplies position/axes. */
XGE_API xge3d_light_desc_t xge3dLightDefault(int type);
XGE_API int xge3dNodeSetLight(xge3d_scene *scene, xge3d_node_t node, const xge3d_light_desc_t *light);
#endif
#if XGE3D_ENABLE_SHADOW
#define XGE3D_MAX_CASCADES 4
#define XGE3D_MAX_SHADOW_SPOTS 2
typedef struct xge3d_shadow_settings_t {
    int resolution, cascades; /* Per map, 1..4 cascades; one sun and two spots. */
    float distance, split_lambda, blend, bias, normal_bias, filter_radius, depth_padding;
} xge3d_shadow_settings_t;
XGE_API xge3d_shadow_settings_t xge3dShadowDefault(void);
#endif
#if XGE3D_ENABLE_FOG
typedef struct xge3d_fog_settings_t {
    xge3d_vec3_t color; /* Linear RGB, blended before exposure and sRGB conversion. */
    float start, end; /* Metres along camera forward; 0 <= start < end. Sky is unchanged. */
} xge3d_fog_settings_t;
XGE_API xge3d_fog_settings_t xge3dFogDefault(void);
#endif
typedef struct xge3d_render_desc_t {
    const xge3d_camera_t *camera;
    xge3d_target *target; /* NULL: current framebuffer and viewport. */
    float clear_color[4];
    uint32_t clear_flags;
    xge3d_environment *environment; /* Borrowed for this call; NULL has no sky. */
    float exposure; /* 0 selects 1; otherwise a positive linear multiplier. */
#if XGE3D_ENABLE_SHADOW
    const xge3d_shadow_settings_t *shadows; /* NULL disables shadow passes. */
#endif
#if XGE3D_ENABLE_IBL
    float ibl_intensity; /* >=0, default 0 disables indirect lighting. */
#endif
    int disable_culling; /* 0 uses conservative world bounds, including skin. */
    int disable_instancing; /* 0 batches compatible static opaque/mask meshes. */
#if XGE3D_ENABLE_FOG
    const xge3d_fog_settings_t *fog; /* Borrowed for this call; NULL disables fog. */
#endif
} xge3d_render_desc_t;
typedef struct xge3d_render_stats_t {
    uint32_t draw_calls;
    uint64_t triangles, upload_bytes;
    uint32_t shadow_draw_calls, shadow_maps;
    uint32_t visible_meshes, culled_meshes;
    uint32_t instanced_draw_calls;
    uint64_t instances; /* Instances submitted in instanced calls, all passes. */
} xge3d_render_stats_t;
#define XGE3D_CLEAR_COLOR 1u
#define XGE3D_CLEAR_DEPTH 2u

#define XGE3D_TRANSFORM_IDENTITY { {0,0,0}, {0,0,0,1}, {1,1,1} }

/* Create/load clears output on failure; do not replace live objects. CPU scene
 * and camera APIs work without xgeInit. Free scenes before attached resources. */
XGE_API int xge3dSceneCreate(xge3d_scene **out);
XGE_API void xge3dSceneFree(xge3d_scene *scene);
XGE_API size_t xge3dSceneNodeCount(const xge3d_scene *scene);
/* Borrowed scene handles; out-of-range returns the zero handle. */
XGE_API xge3d_node_t xge3dSceneNodeAt(const xge3d_scene *scene, size_t index);
/* Scene matrices, cameras, rays and hits use coordinates relative to origin.
 * Rebase keeps root global translations in double precision. Applications
 * retain global game state; set the nearby origin before placing distant data. */
XGE_API int xge3dSceneSetOrigin(xge3d_scene *scene, xge3d_dvec3_t origin);
XGE_API xge3d_dvec3_t xge3dSceneOrigin(const xge3d_scene *scene);
XGE_API int xge3dNodeSetGlobalPosition(xge3d_scene *scene, xge3d_node_t root, xge3d_dvec3_t position);
XGE_API int xge3dRelativePosition(xge3d_dvec3_t origin, xge3d_dvec3_t global, xge3d_vec3_t *out);
XGE_API int xge3dCameraLookAtGlobal(xge3d_camera_t *camera, xge3d_dvec3_t origin,
    xge3d_dvec3_t eye, xge3d_dvec3_t target, xge3d_vec3_t up);
#if XGE3D_ENABLE_ANIMATION
/* Borrowed mesh-local joint matrices; invalidated by pose mutation or deletion.
 * Joint indices follow the source glTF Skin order. NOT_FOUND means no skin. */
XGE_API int xge3dNodeSkinMatrices(xge3d_scene *scene, xge3d_node_t node,
    const xge3d_mat4_t **out_matrices, size_t *out_count);
XGE_API size_t xge3dRendererMaxJoints(const xge3d_renderer *renderer);
#endif
XGE_API int xge3dNodeCreate(xge3d_scene *scene, xge3d_node_t parent, xge3d_node_t *out);
XGE_API int xge3dNodeDestroy(xge3d_scene *scene, xge3d_node_t node);
XGE_API int xge3dNodeSetParent(xge3d_scene *scene, xge3d_node_t node, xge3d_node_t parent);
XGE_API int xge3dNodeSetTransform(xge3d_scene *scene, xge3d_node_t node, const xge3d_transform_t *transform);
/* Exact affine matrices preserve imported transforms, including shear. */
XGE_API int xge3dNodeSetMatrix(xge3d_scene *scene, xge3d_node_t node, const xge3d_mat4_t *matrix);
/* Parent-local matrix; root translations are relative to the scene origin.
 * Does not decompose imported affine transforms; failure leaves output intact. */
XGE_API int xge3dNodeGetLocalMatrix(xge3d_scene *scene,xge3d_node_t node,xge3d_mat4_t *out);
XGE_API int xge3dNodeGetWorldMatrix(xge3d_scene *scene, xge3d_node_t node, xge3d_mat4_t *out);
XGE_API int xge3dNodeSetVisible(xge3d_scene *scene, xge3d_node_t node, int visible);
/* Mesh creation copies CPU data, uploading once if a context is active;
 * otherwise it uploads on first render. Nodes retain meshes; caller releases
 * its reference with MeshFree. GPU upload/update/release and scene destruction
 * with uploaded meshes must run on the owning context thread. */
XGE_API int xge3dMeshCreate(const xge3d_mesh_desc_t *desc, xge3d_mesh **out);
XGE_API int xge3dMeshUpdate(xge3d_mesh *mesh, const xge3d_mesh_desc_t *desc);
XGE_API void xge3dMeshFree(xge3d_mesh *mesh);
XGE_API int xge3dNodeSetMesh(xge3d_scene *scene, xge3d_node_t node, xge3d_mesh *mesh);
/* Static nodes only: copy/retain up to 3 additional meshes, with positive,
 * strictly increasing distances from the camera to the node origin. Base mesh
 * is level 0. Shadows use the same camera-selected level; bounds include all
 * levels and picking/NodeGetMesh use level 0. NULL,0 clears; SetMesh clears LOD. */
XGE_API int xge3dNodeSetLods(xge3d_scene *scene, xge3d_node_t node, const xge3d_lod_t *levels, size_t count);
#if XGE3D_ENABLE_TERRAIN
#define XGE3D_HEIGHT_U16 1
#define XGE3D_HEIGHT_F32 2
typedef struct xge3d_terrain_desc_t {
    const void *heights;size_t width,depth,stride; /* Samples/row bytes; 0 stride is packed. */
    int format;float cell_size,height_scale,height_offset;
    size_t chunk_cells;int lod_count;float lod_distances[3],skirt_depth;
    xge3d_material *material;
} xge3d_terrain_desc_t;
typedef struct xge3d_terrain_info_t {
    size_t width,depth,chunk_count;int lod_count;xge3d_aabb_t bounds; /* Surface; excludes skirts. */
} xge3d_terrain_info_t;
XGE_API xge3d_terrain_desc_t xge3dTerrainDefault(void);
/* Copy uint16/float height samples without an 8-bit conversion. Fixed chunks,
 * 1..4 levels and vertical skirts; no world streaming or game rules. */
XGE_API int xge3dTerrainCreate(const xge3d_terrain_desc_t *desc,xge3d_terrain **out);
XGE_API void xge3dTerrainFree(xge3d_terrain *terrain);
XGE_API xge3d_terrain_info_t xge3dTerrainInfo(const xge3d_terrain *terrain);
/* Instance nodes retain all meshes/materials; TerrainFree may follow creation.
 * On failure no partial instance remains. Root is an ordinary scene node. */
XGE_API int xge3dTerrainInstantiate(xge3d_scene *scene,xge3d_terrain *terrain,xge3d_node_t parent,xge3d_node_t *out_root);
/* Borrow mesh and chunk position in terrain-local coordinates. */
XGE_API int xge3dTerrainChunkMesh(const xge3d_terrain *terrain,size_t chunk,int lod,
    const xge3d_mesh **out_mesh,xge3d_vec3_t *out_position);
/* Queries use terrain-local coordinates and an explicit mesh level (0 is the
 * detailed surface). Height interpolates actual triangles, not bilinearly;
 * ray queries include skirts. Rendered chunks can have different levels. */
XGE_API int xge3dTerrainSample(const xge3d_terrain *terrain,int lod,float x,float z,float *out_height,xge3d_vec3_t *out_normal);
XGE_API int xge3dTerrainRaycast(const xge3d_terrain *terrain,int lod,const xge3d_ray_t *ray,
    float max_distance,xge3d_hit_t *out_hit,size_t *out_chunk);
#endif
/* Materials retain their textures; nodes retain materials. Default initializes
 * glTF metallic/roughness factors and identity UV transforms. GetDesc borrows
 * texture pointers. Release all uploaded references on the context thread. */
XGE_API int xge3dTextureCreate(const xge3d_texture_desc_t *desc, xge3d_texture **out);
XGE_API void xge3dTextureFree(xge3d_texture *texture);
XGE_API xge3d_material_desc_t xge3dMaterialDefault(void);
XGE_API int xge3dMaterialCreate(const xge3d_material_desc_t *desc, xge3d_material **out);
XGE_API void xge3dMaterialFree(xge3d_material *material);
XGE_API int xge3dMaterialGetDesc(const xge3d_material *material, xge3d_material_desc_t *out);
XGE_API int xge3dNodeSetMaterial(xge3d_scene *scene, xge3d_node_t node, xge3d_material *material);
/* Apply to mesh nodes under root, including imported multi-primitive children.
 * Unrelated scene nodes are unchanged. Validation/allocation failure publishes
 * no changes; NULL assigns the default material to the selected mesh nodes,
 * rather than restoring the imported model's original material. */
XGE_API int xge3dNodeSetMaterialTree(xge3d_scene *scene,xge3d_node_t root,xge3d_material *material);
XGE_API int xge3dNodeSetColor(xge3d_scene *scene, xge3d_node_t node, const float rgba[4]);
/* Copies all faces on the CPU; first render uploads once. Release uploaded
 * environments on their context thread before xgeUnit. */
XGE_API int xge3dEnvironmentCreate(const xge3d_environment_desc_t *desc, xge3d_environment **out);
XGE_API void xge3dEnvironmentFree(xge3d_environment *environment);
#if XGE3D_ENABLE_IBL
/* Copies precomputed LDR data transactionally. NULL removes IBL. Uploaded data
 * can only be replaced on the owning context thread. Sky faces stay intact. */
XGE_API int xge3dEnvironmentSetIBL(xge3d_environment *environment, const xge3d_ibl_desc_t *desc);
#endif
/* Renderer/targets require the active context. All GPU objects must be freed
 * before context destruction. Existing targets remain valid if resize fails. */
XGE_API int xge3dRendererCreate(xge3d_renderer **out);
XGE_API void xge3dRendererFree(xge3d_renderer *renderer);
XGE_API int xge3dTargetCreate(int width, int height, xge3d_target **out);
XGE_API int xge3dTargetResize(xge3d_target *target, int width, int height);
XGE_API void xge3dTargetFree(xge3d_target *target);
/* Borrowed texture: do not modify/free it; valid until resize or target free. */
XGE_API const xge_texture_t *xge3dTargetTexture(const xge3d_target *target);
/* Top-down RGBA8 readback; stride >= width*4 and storage >= stride*height. */
XGE_API int xge3dTargetReadPixels(xge3d_target *target, void *rgba, size_t size, int stride);
XGE_API int xge3dRender(xge3d_renderer *renderer, xge3d_scene *scene,
    const xge3d_render_desc_t *desc, xge3d_render_stats_t *stats);
#if XGE3D_ENABLE_MODEL
typedef struct xge3d_model_info_t {
    size_t node_count, primitive_count, material_count, animation_count, skin_count;
    size_t truncated_weight_vertices;
} xge3d_model_info_t;
/* glTF 2.0/GLB, loaded through xgeResource; memory input is copied. base_uri is
 * the logical model filename used to resolve relative buffers and images.
 * Instances retain the model and shared meshes. Failure clears output and
 * publishes no partial instance. Unknown required extensions return unsupported. */
XGE_API int xge3dModelLoad(const char *uri, xge3d_model **out);
XGE_API int xge3dModelLoadMemory(const void *data, size_t size, const char *base_uri, xge3d_model **out);
XGE_API void xge3dModelFree(xge3d_model *model);
/* Consume one owned model reference on the graphics thread. Shared models
 * simply release that reference and return a NULL token. An exclusive model
 * becomes unavailable for new instances/borrows and is transferred to token.
 * Step releases ownership of shared resources and detaches exclusive GPU
 * objects in bounded record groups; it retains CPU payload for later cleanup.
 * A driver call cannot be preempted. Zero operations makes no progress.
 * Once complete, Free may run on a CPU worker (source-provider destructors
 * must support that thread). Free of an incomplete token is synchronous and
 * restricted to its creating graphics thread, for final shutdown only. */
typedef struct xge3d_model_retirement xge3d_model_retirement;
typedef struct xge3d_release_budget_t {size_t operations;uint64_t microseconds;} xge3d_release_budget_t;
typedef struct xge3d_retirement_info_t {uint32_t size;size_t operations,total_operations;int completed;} xge3d_retirement_info_t;
XGE_API int xge3dModelRetireBegin(xge3d_model **owned,xge3d_model_retirement **out);
XGE_API int xge3dModelRetireStep(xge3d_model_retirement *token,const xge3d_release_budget_t *budget,int *completed);
XGE_API int xge3dModelRetireInfo(const xge3d_model_retirement *token,xge3d_retirement_info_t *out);
XGE_API int xge3dModelRetireFree(xge3d_model_retirement *token);
XGE_API xge3d_model_info_t xge3dModelInfo(const xge3d_model *model);
/* Borrowed material; valid while the model or its instance remains alive. */
XGE_API const xge3d_material *xge3dModelMaterial(const xge3d_model *model, size_t index);
XGE_API const char *xge3dModelNodeName(const xge3d_model *model, size_t index);
#if XGE3D_ENABLE_ANIMATION
/* SIZE_MAX for invalid indices; result is a source node index. */
XGE_API size_t xge3dModelSkinJointNode(const xge3d_model *model, size_t skin, size_t joint);
#endif
XGE_API int xge3dModelInstantiate(xge3d_scene *scene, xge3d_model *model,
    xge3d_node_t parent, xge3d_node_t *out_root);
/* Source node indices are stable and correspond to ModelInfo/ModelNodeName.
 * Nodes outside the selected glTF scene return not found. */
XGE_API int xge3dModelInstanceNode(xge3d_scene *scene, xge3d_node_t root,
    size_t source_index, xge3d_node_t *out);
#if XGE3D_ENABLE_ASYNC
#define XGE3D_LOADER_RESOURCE_PROVIDERS 1u
typedef struct xge3d_loader_desc_t { size_t max_requests;uint32_t flags; } xge3d_loader_desc_t;
typedef struct xge3d_request_t { uint64_t slot,loader; } xge3d_request_t;
typedef enum xge3d_request_state_t {
    XGE3D_REQUEST_QUEUED,XGE3D_REQUEST_LOADING,XGE3D_REQUEST_CPU_READY,
    XGE3D_REQUEST_UPLOADING,XGE3D_REQUEST_READY,XGE3D_REQUEST_FAILED,XGE3D_REQUEST_CANCELLED
} xge3d_request_state_t;
typedef struct xge3d_request_info_t {
    xge3d_request_state_t state;int result,pending_cleanup;uint64_t uploaded_bytes,total_bytes;
} xge3d_request_info_t;
typedef struct xge3d_upload_budget_t { uint64_t bytes,microseconds;uint32_t operations; } xge3d_upload_budget_t;
typedef struct xge3d_upload_stats_t {
    uint64_t upload_bytes,min_next_bytes;uint32_t operations,completed,failed;
} xge3d_upload_stats_t;
/* Context-thread prewarming of an independently created material texture.
 * Uploads whole RGBA pixels in bounded chunks; allocation and mip generation
 * each count as an operation. bytes counts transferred base-level pixels.
 * completed==1 permits rendering; do not publish a partially prepared texture
 * to a material/node. Zero budgets make no progress; an already ready texture
 * reports completion without another upload. microseconds is a soft limit
 * checked between GL operations, not a preemption of a driver call.
 * Saves/restores GL bindings and unpack state, including pixel-unpack buffers. */
XGE_API int xge3dTexturePrepare(xge3d_texture *texture,const xge3d_upload_budget_t *budget,
    xge3d_upload_stats_t *out_stats);
/* Prewarm a CPU-created mesh on its GPU owner thread. Storage allocation and
 * each partial transfer count as operations; bytes counts transferred payload.
 * Zero budgets do nothing. Only publish after completed==1; rendering a partial
 * mesh fails instead of drawing uninitialized storage. Bindings are restored.
 * CPU data stays immutable; MeshUpdate replaces any pending preparation. */
XGE_API int xge3dMeshPrepare(xge3d_mesh *mesh,const xge3d_upload_budget_t *budget,
    xge3d_upload_stats_t *out_stats);
/* One xrt CPU worker per loader; 0 max_requests selects 64 (max 1024).
 * Capacity includes cancelled jobs until their CPU cleanup ends. URI is copied.
 * All public loader calls use its creating thread. Default IO is plain files
 * and file://. RESOURCE_PROVIDERS opts into xgeResource callbacks on the worker:
 * callbacks and their user data must be thread-safe, and registration stays
 * unchanged until pending CPU work ends. Provider user data must outlive models. */
XGE_API int xge3dLoaderCreate(const xge3d_loader_desc_t *desc,xge3d_loader **out);
/* Cancel all, wait for the CPU worker and release private resources. Free may
 * wait for indivisible file/provider reads; call before destroying the context. */
XGE_API int xge3dLoaderFree(xge3d_loader *loader);
XGE_API int xge3dLoaderRequest(xge3d_loader *loader,const char *uri,xge3d_request_t *out_request);
XGE_API int xge3dLoaderStatus(xge3d_loader *loader,xge3d_request_t request,xge3d_request_info_t *out_info);
/* Pump on the GPU context thread: hard byte/operation caps, soft time limit
 * checked between operations. 0 bytes pauses uploads; 0 operations pauses all
 * GPU work. Cancelled models retire within the operation/time budget even
 * when bytes is 0. 0 time disables the clock limit. Large buffers/textures upload in pieces. A texture
 * pixel needs >=4 bytes, reported through min_next_bytes when blocked. Bytes
 * count transferred pixels/vertices/indices, not storage or generated mip data.
 * A job failure appears in Status and out_stats.failed; other jobs can proceed.
 * Models with no GPU resources can become READY without a context. */
XGE_API int xge3dLoaderPump(xge3d_loader *loader,const xge3d_upload_budget_t *budget,xge3d_upload_stats_t *out_stats);
/* Cancel is idempotent and prevents publication immediately. Pump retires GPU
 * partial uploads on the owner; the worker reclaims CPU payload outside locks.
 * Continue pumping (bytes may be 0), or Free at final shutdown. Release also
 * invalidates the handle, so stale results cannot reach a reused slot. */
XGE_API int xge3dLoaderCancel(xge3d_loader *loader,xge3d_request_t request);
XGE_API int xge3dLoaderRelease(xge3d_loader *loader,xge3d_request_t request);
/* Only READY can transfer a complete model. Success invalidates the request;
 * caller owns the model and may instantiate it. Failure clears output. */
XGE_API int xge3dLoaderTake(xge3d_loader *loader,xge3d_request_t request,xge3d_model **out_model);
#endif
#if XGE3D_ENABLE_ANIMATION
typedef struct xge3d_clip_info_t { const char *name; float duration; size_t track_count, node_count; } xge3d_clip_info_t;
typedef struct xge3d_bone_binding_t { size_t source, target; } xge3d_bone_binding_t;
typedef struct xge3d_animation_layer_t {
    float time, speed, weight;
    int loop;
    const float *mask; size_t mask_count; /* Target source-node order; NULL=all 1. */
    const xge3d_bone_binding_t *bindings; size_t binding_count;
} xge3d_animation_layer_t;
typedef struct xge3d_animation_event_t { int layer;uint32_t id;float time; } xge3d_animation_event_t;
typedef struct xge3d_root_motion_t { xge3d_vec3_t translation;xge3d_quat_t rotation; } xge3d_root_motion_t;
#define XGE3D_ROOT_X 1u
#define XGE3D_ROOT_Y 2u
#define XGE3D_ROOT_Z 4u
#define XGE3D_ROOT_ROTATION 8u
/* Clips retain source data, including independent motion-only glTF files.
 * Animator borrows scene; free it before destroying scene. Up to eight layers
 * blend TRS against the asset rest pose. Different source rigs require offline
 * retargeting; runtime name/index binding does not adjust bone lengths or axes. */
XGE_API int xge3dClipFromModel(xge3d_model *model, size_t animation, xge3d_clip **out);
XGE_API int xge3dClipLoad(const char *uri, size_t animation, xge3d_clip **out);
XGE_API void xge3dClipFree(xge3d_clip *clip);
XGE_API xge3d_clip_info_t xge3dClipInfo(const xge3d_clip *clip);
XGE_API const char *xge3dClipNodeName(const xge3d_clip *clip, size_t source_node);
XGE_API int xge3dAnimatorCreate(xge3d_scene *scene, xge3d_node_t model_root, xge3d_animator **out);
XGE_API void xge3dAnimatorFree(xge3d_animator *animator);
XGE_API xge3d_animation_layer_t xge3dAnimationLayerDefault(void);
/* NULL clears a layer. Failed binding preserves the old layer. Automatic
 * binding uses unique names, or source indices for clips from the same model. */
XGE_API int xge3dAnimatorSetLayer(xge3d_animator *animator, int layer, xge3d_clip *clip, const xge3d_animation_layer_t *desc);
XGE_API int xge3dAnimatorSetLayerWeight(xge3d_animator *animator, int layer, float weight);
XGE_API float xge3dAnimatorLayerTime(const xge3d_animator *animator, int layer);
XGE_API int xge3dAnimatorUpdate(xge3d_animator *animator, float delta_seconds);
/* Up to 1024 events are copied in ascending clip-time order per layer. The
 * input layer field is ignored. First successful Update includes its start
 * time; later updates emit (old,new], including each loop's zero/end marks.
 * Layer 0 is queued before layer 1; zero-weight layers still advance/events.
 * Events survive until polled. More than 4096 pending events makes Update
 * fail before publishing time, pose, motion or events. */
XGE_API int xge3dAnimatorSetEvents(xge3d_animator *animator, int layer, const xge3d_animation_event_t *events, size_t count);
XGE_API int xge3dAnimatorNextEvent(xge3d_animator *animator, xge3d_animation_event_t *out);
/* Selected components are removed from the bone pose. Motion accumulates in
 * that bone's parent coordinates until Take; the application applies movement
 * after its own collision checks. flags=0 disables extraction. */
XGE_API int xge3dAnimatorSetRootMotion(xge3d_animator *animator, size_t target_node, uint32_t flags);
XGE_API int xge3dAnimatorTakeRootMotion(xge3d_animator *animator, xge3d_root_motion_t *out);
/* Bone nodes can parent ordinary scene nodes to implement sockets. */
XGE_API int xge3dAnimatorBoneNode(xge3d_animator *animator, const char *name, xge3d_node_t *out);
#endif
#endif
/* Screen coordinates are framebuffer pixels from the top-left. Ray origin is
 * on the near plane. SceneRaycast normalizes nonzero directions and returns
 * distances in metres; max_distance must be positive. Visible mesh triangles
 * are tested on both sides; no hit returns NOT_FOUND with a cleared output. */
XGE_API int xge3dCameraScreenRay(const xge3d_camera_t *camera, float x, float y,
    float width, float height, xge3d_ray_t *out);
XGE_API int xge3dSceneRaycast(xge3d_scene *scene, const xge3d_ray_t *ray,
    float max_distance, xge3d_hit_t *out);
/* Borrowed immutable CPU geometry, valid until mesh update or final release. */
XGE_API int xge3dMeshGetData(const xge3d_mesh *mesh, xge3d_mesh_desc_t *out);
XGE_API int xge3dMeshGetBounds(const xge3d_mesh *mesh, xge3d_aabb_t *out);
/* Borrowed immutable mesh, invalidated by node replacement/deletion. */
XGE_API int xge3dNodeGetMesh(xge3d_scene *scene, xge3d_node_t node, const xge3d_mesh **out);
/* Conservative world bounds; descendants are optional, visibility is ignored.
 * Empty geometry returns NOT_FOUND. Skin bounds follow the current pose. */
XGE_API int xge3dNodeGetBounds(xge3d_scene *scene, xge3d_node_t node, int descendants, xge3d_aabb_t *out);
/* Basic queries normalize ray directions; starts inside volumes return 0 m.
 * No hit clears the output and returns NOT_FOUND. */
XGE_API int xge3dRayAabb(const xge3d_ray_t *ray, const xge3d_aabb_t *box, float max_distance, float *out_distance);
XGE_API int xge3dRaySphere(const xge3d_ray_t *ray, const xge3d_sphere_t *sphere, float max_distance, float *out_distance);
XGE_API int xge3dRayTriangle(const xge3d_ray_t *ray, const xge3d_vec3_t triangle[3], float max_distance, xge3d_hit_t *out);
/* Predicates return 1 for overlap, 0 for separation or invalid arguments. */
XGE_API int xge3dAabbIntersects(const xge3d_aabb_t *a, const xge3d_aabb_t *b);
XGE_API int xge3dSphereIntersectsAabb(const xge3d_sphere_t *sphere, const xge3d_aabb_t *box);
XGE_API int xge3dCameraIntersectsAabb(const xge3d_camera_t *camera, const xge3d_aabb_t *box);
/* Returns visible mesh handles; count is the total required capacity. A short
 * output receives its prefix then returns BUFFER_TOO_SMALL. NULL/capacity=0
 * is a count-only call and returns OK. This is conservative broad phase. */
XGE_API int xge3dSceneQueryAabb(xge3d_scene *scene, const xge3d_aabb_t *box,
    xge3d_node_t *nodes, size_t capacity, size_t *out_count);
/* Foundation additions are separate from legacy descriptors to preserve ABI. */
typedef struct xge3d_work_budget_t { size_t nodes; uint64_t microseconds; } xge3d_work_budget_t;
typedef struct xge3d_query_filter_t { uint32_t layers; int include_hidden; } xge3d_query_filter_t;
typedef struct xge3d_scene_stats_t {
    uint32_t size;
    uint64_t world_updates, bounds_updates, spatial_builds, spatial_refits,
        query_tree_visits, query_mesh_tests, destroyed_nodes;
    size_t nodes, spatial_bytes;
} xge3d_scene_stats_t;
typedef struct xge3d_renderer_options_t {
    uint32_t size, render_layers, shadow_layers, max_lights;
    int profiling; /* Optional nonblocking GPU timestamps where supported. */
} xge3d_renderer_options_t;
typedef struct xge3d_render_profile_t {
    uint32_t size;
    uint64_t total_cpu_us, collect_cpu_us, shadow_cpu_us, submit_cpu_us, gpu_ns;
    uint64_t gpu_sample; /* Render-call sequence of the completed GPU sample. */
    uint32_t candidate_lights, selected_lights, skipped_lights;
    int gpu_available; /* 0 means unsupported, disabled, or no completed sample. */
} xge3d_render_profile_t;
typedef struct xge3d_resource_info_t {
    uint32_t size;
    uint64_t cpu_payload_bytes, gpu_payload_bytes;
    size_t vertices, indices, textures;
    uint32_t references;
} xge3d_resource_info_t;
/* Payload estimates, not allocator overhead or actual driver/VRAM residency. */
XGE_API int xge3dMeshResourceInfo(const xge3d_mesh *mesh, xge3d_resource_info_t *out);
XGE_API int xge3dTextureResourceInfo(const xge3d_texture *texture, xge3d_resource_info_t *out);
XGE_API int xge3dSceneStats(xge3d_scene *scene, xge3d_scene_stats_t *out, int reset);
/* Exact query output order is unspecified. Hidden visual geometry can be queried
 * independently; this does not create physics or gameplay collision objects. */
XGE_API int xge3dSceneQueryAabbEx(xge3d_scene *scene, const xge3d_aabb_t *box,
    const xge3d_query_filter_t *filter, xge3d_node_t *nodes, size_t capacity, size_t *out_count);
XGE_API int xge3dSceneRaycastEx(xge3d_scene *scene, const xge3d_ray_t *ray,
    float max_distance, const xge3d_query_filter_t *filter, xge3d_hit_t *out);
XGE_API int xge3dScenePrepareSpatial(xge3d_scene *scene);
/* Layers apply to this node, visibility still inherits through the hierarchy. */
XGE_API int xge3dNodeSetLayers(xge3d_scene *scene, xge3d_node_t node,
    uint32_t render_layers, uint32_t query_layers, uint32_t shadow_layers);
/* Hide and lock a subtree, then remove at most budget.nodes leaf nodes. Zero
 * nodes pauses; time is a soft limit between nodes. Continue with the same root
 * until completed=1. All borrowed subtree handles become unusable immediately.
 * GPU resources must be retired on their context thread. SceneFree drains it. */
XGE_API int xge3dNodeDestroyStep(xge3d_scene *scene, xge3d_node_t root,
    const xge3d_work_budget_t *budget, int *completed);
XGE_API xge3d_renderer_options_t xge3dRendererOptionsDefault(void);
XGE_API int xge3dRendererSetOptions(xge3d_renderer *renderer, const xge3d_renderer_options_t *options);
XGE_API int xge3dRendererProfile(const xge3d_renderer *renderer, xge3d_render_profile_t *out);
/* Preallocate the shadow atlas explicitly (0 means no atlas). One allocation is
 * one soft-budget operation; a driver allocation cannot be preempted. */
#if XGE3D_ENABLE_ASYNC
XGE_API int xge3dRendererPrepare(xge3d_renderer *renderer, int shadow_resolution,
    const xge3d_upload_budget_t *budget, xge3d_upload_stats_t *out);
#endif
XGE_API int xge3dMeshAddRef(xge3d_mesh *mesh);
XGE_API int xge3dMaterialAddRef(xge3d_material *material);
XGE_API int xge3dTextureAddRef(xge3d_texture *texture);
#if XGE3D_ENABLE_MODEL
typedef struct xge3d_instance_builder xge3d_instance_builder;
/* Retained asset storage for admission budgets. Includes encoded source,
 * glTF parser allocations/external buffers, container capacities, meshes,
 * materials and texture pixels. GPU required includes the full mip chain.
 * Excludes temporary decode peaks, scene instances/animators, allocator and
 * driver overhead. Shared resources are conservatively counted per asset.
 * Query immutable loaded assets without concurrent mutation/retirement. */
typedef struct xge3d_asset_memory_t {
    size_t size;
    uint64_t cpu_storage_bytes, gpu_required_bytes, source_bytes, parser_bytes;
} xge3d_asset_memory_t;
/* Shared quota for live XRT heap requests made by bounded model/clip loads,
 * including parser, decoder scratch, queued results and retained allocations.
 * Charges follow blocks across threads and survive scope/owner destruction.
 * Not RSS/VRAM: excludes pool backing, allocator/OS/driver overhead, instance
 * and animator allocations outside a load, and external allocator callbacks.
 * Realloc reserves old+new until the old block is released. Zero-byte requests
 * cost one byte. Default loaders/legacy synchronous loads remain unlimited. */
typedef struct xallocationbudget xge3d_decode_budget;
typedef struct xge3d_decode_budget_info_t {
    size_t size;
    uint64_t limit,live_bytes,peak_bytes,live_allocations,rejections;
} xge3d_decode_budget_info_t;
XGE_API int xge3dDecodeBudgetCreate(uint64_t bytes,xge3d_decode_budget **out);
XGE_API void xge3dDecodeBudgetFree(xge3d_decode_budget *budget);
XGE_API int xge3dDecodeBudgetInfo(xge3d_decode_budget *budget,xge3d_decode_budget_info_t *out);
XGE_API int xge3dDecodeBudgetSetLimit(xge3d_decode_budget *budget,uint64_t bytes);
XGE_API int xge3dModelLoadBudget(const char *uri,xge3d_decode_budget *budget,xge3d_model **out);
XGE_API int xge3dModelLoadMemoryBudget(const void *data,size_t size,const char *base_uri,
    xge3d_decode_budget *budget,xge3d_model **out);
#if XGE3D_ENABLE_ANIMATION
XGE_API int xge3dClipLoadBudget(const char *uri,size_t animation,xge3d_decode_budget *budget,xge3d_clip **out);
#endif
XGE_API int xge3dModelMemoryInfo(const xge3d_model *model, xge3d_asset_memory_t *out);
#if XGE3D_ENABLE_ASYNC
/* Owner thread, only while loader has no requests. Retains budget; NULL
 * removes it. The shared quota must cover retained models and decode scratch.
 * An exhausted quota fails the request without allocating the denied block. */
XGE_API int xge3dLoaderSetDecodeBudget(xge3d_loader *loader,xge3d_decode_budget *budget);
/* Available from CPU_READY through READY, before Take. Cached CPU admission
 * footprint; does not expose a model pointer or start GPU work. */
XGE_API int xge3dLoaderMemoryInfo(xge3d_loader *loader, xge3d_request_t request,
    xge3d_asset_memory_t *out);
#endif
#if XGE3D_ENABLE_ANIMATION
/* Includes retained source model and decoded track capacities. Two clips
 * sharing one source model each include it: conservative, never deduplicated. */
XGE_API int xge3dClipMemoryInfo(const xge3d_clip *clip, xge3d_asset_memory_t *out);
#endif
XGE_API int xge3dModelAddRef(xge3d_model *model);
XGE_API int xge3dModelResourceInfo(const xge3d_model *model, xge3d_resource_info_t *out);
/* Staging remains hidden. Take publishes the whole instance; failures never
 * publish a partial instance. Free cancels synchronously, suitable for shutdown. */
XGE_API int xge3dModelInstanceBegin(xge3d_scene *scene, xge3d_model *model,
    xge3d_node_t parent, xge3d_instance_builder **out);
XGE_API int xge3dModelInstanceStep(xge3d_instance_builder *builder,
    const xge3d_work_budget_t *budget, int *completed);
XGE_API int xge3dModelInstanceTake(xge3d_instance_builder *builder, xge3d_node_t *out);
XGE_API void xge3dModelInstanceFree(xge3d_instance_builder *builder);
#endif
#if XGE3D_ENABLE_ANIMATION
#define XGE3D_ANIMATION_MAX_LAYERS 8
#define XGE3D_ANIMATION_OVERRIDE 0
#define XGE3D_ANIMATION_ADDITIVE 1
typedef struct xge3d_skin_binding_t {
    const xge3d_node_t *joints;
    const xge3d_mat4_t *inverse_bind; /* NULL selects identity. */
    size_t count;
} xge3d_skin_binding_t;
/* Explicit joint order follows the mesh's vertex indices. Copies bindings,
 * validates all joints/weights; failure preserves the old skin. NULL unbinds.
 * Multiple meshes may use the SAME joint nodes without extra animators. */
XGE_API int xge3dNodeBindSkin(xge3d_scene *scene, xge3d_node_t node, const xge3d_skin_binding_t *binding);
#if XGE3D_ENABLE_ASYNC
XGE_API int xge3dNodePrepareSkin(xge3d_scene *scene, xge3d_node_t node,
    const xge3d_upload_budget_t *budget, xge3d_upload_stats_t *out);
#endif
XGE_API int xge3dAnimatorSetLayerMode(xge3d_animator *animator, int layer, int mode);
/* Source-model node indices. Postprocess after Update, before rendering; the
 * next Update resamples the clip. No gameplay or IK policy is implicit. */
XGE_API int xge3dAnimatorGetPose(xge3d_animator *animator, size_t node, xge3d_transform_t *out);
XGE_API int xge3dAnimatorSetPose(xge3d_animator *animator, size_t node, const xge3d_transform_t *pose);
#endif
XGE_API int xge3dTransformMatrix(const xge3d_transform_t *transform, xge3d_mat4_t *out);
XGE_API int xge3dCameraPerspective(xge3d_camera_t *out, float fov_y, float aspect, float near_z, float far_z);
XGE_API int xge3dCameraOrthographic(xge3d_camera_t *out, float width, float height, float near_z, float far_z);
XGE_API int xge3dCameraLookAt(xge3d_camera_t *camera, xge3d_vec3_t eye, xge3d_vec3_t target, xge3d_vec3_t up);
#endif

#ifdef __cplusplus
}
#endif

#endif /* XGE_H */
