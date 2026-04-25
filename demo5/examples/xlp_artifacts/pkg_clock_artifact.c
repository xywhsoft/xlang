#include <stdint.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XL_EXPORT __declspec(dllexport)
#else
#define XL_EXPORT
#endif

typedef unsigned int XL_ArtifactUInt32;
typedef long long XL_ArtifactInt64;
typedef const char* XL_ArtifactStr;
typedef void* XL_ArtifactPtr;

typedef struct XL_ArtifactXValueStruct {
	union {
		struct {
			XL_ArtifactUInt32 Type:4;
			XL_ArtifactUInt32 Reserve:1;
			XL_ArtifactUInt32 IsStatic:1;
			XL_ArtifactUInt32 RefCount:26;
		};
		volatile XL_ArtifactUInt32 Header;
	};
	XL_ArtifactUInt32 Size;
	union {
		int vBool;
		XL_ArtifactInt64 vInt;
		double vFloat;
		XL_ArtifactStr vText;
		XL_ArtifactPtr vPoint;
	};
} *XL_ArtifactXValue;

#define XL_ARTIFACT_XVO_DT_INT 2
#define XL_ARTIFACT_XVO_DT_TEXT 4

XL_EXPORT int pkg_clock_artifact_probe(void)
{
	return 123;
}

static int g_pkg_clock_artifact_point_sentinel = 77;
static int g_pkg_clock_artifact_point_release_count = 0;

XL_EXPORT void* pkg_clock_artifact_point_slot = &g_pkg_clock_artifact_point_sentinel;

XL_EXPORT int pkg_clock_artifact_bool_slot = 1;
XL_EXPORT double pkg_clock_artifact_float_slot = 2.5;
XL_EXPORT int32_t pkg_clock_artifact_i32_slot = 1200;
XL_EXPORT uint64_t pkg_clock_artifact_u64_slot = 9000000000ULL;

XL_EXPORT void* pkg_clock_artifact_point_sentinel(void)
{
	return &g_pkg_clock_artifact_point_sentinel;
}

XL_EXPORT void* pkg_clock_artifact_point_create(void)
{
	return &g_pkg_clock_artifact_point_sentinel;
}

XL_EXPORT int pkg_clock_artifact_point_score(void* value)
{
	return (value == &g_pkg_clock_artifact_point_sentinel) ? 7 : 0;
}

XL_EXPORT void pkg_clock_artifact_point_release(void* value)
{
	if ( value == &g_pkg_clock_artifact_point_sentinel ) {
		g_pkg_clock_artifact_point_release_count += 1;
	}
}

XL_EXPORT void pkg_clock_artifact_point_release_reset(void)
{
	g_pkg_clock_artifact_point_release_count = 0;
}

XL_EXPORT int pkg_clock_artifact_point_release_count(void)
{
	return g_pkg_clock_artifact_point_release_count;
}

XL_EXPORT int pkg_clock_artifact_point_is_sentinel(void)
{
	return pkg_clock_artifact_point_slot == &g_pkg_clock_artifact_point_sentinel;
}

XL_EXPORT int pkg_clock_artifact_point_is_null(void)
{
	return pkg_clock_artifact_point_slot == NULL;
}

XL_EXPORT int pkg_clock_artifact_bool_slot_is_true(void)
{
	return pkg_clock_artifact_bool_slot != 0;
}

XL_EXPORT int pkg_clock_artifact_bool_slot_is_false(void)
{
	return pkg_clock_artifact_bool_slot == 0;
}

XL_EXPORT int pkg_clock_artifact_float_slot_score(void)
{
	return (int)(pkg_clock_artifact_float_slot * 10.0);
}

XL_EXPORT int pkg_clock_artifact_i32_slot_score(void)
{
	return (int)(pkg_clock_artifact_i32_slot / 100);
}

XL_EXPORT int pkg_clock_artifact_u64_slot_score(void)
{
	return (int)(pkg_clock_artifact_u64_slot / 1000000000ULL);
}

XL_EXPORT const char* pkg_clock_artifact_text_slot = "probe-text";

XL_EXPORT int pkg_clock_artifact_text_slot_is_probe(void)
{
	return (pkg_clock_artifact_text_slot != NULL) && (strcmp(pkg_clock_artifact_text_slot, "probe-text") == 0);
}

XL_EXPORT int pkg_clock_artifact_text_slot_is_updated(void)
{
	return (pkg_clock_artifact_text_slot != NULL) && (strcmp(pkg_clock_artifact_text_slot, "updated-text") == 0);
}

typedef struct XL_PkgClockArtifactPair {
	int x;
	int y;
} XL_PkgClockArtifactPair;

typedef struct XL_PkgClockArtifactLayout {
	int8_t tag;
	int32_t count;
	double weight;
} XL_PkgClockArtifactLayout;

typedef struct XL_PkgClockArtifactInner {
	int32_t x;
	int32_t y;
} XL_PkgClockArtifactInner;

typedef struct XL_PkgClockArtifactOuter {
	XL_PkgClockArtifactInner inner;
	int32_t z;
	double scale;
} XL_PkgClockArtifactOuter;

typedef struct XL_PkgClockArtifactEnumFields {
	int mode;
	int rawMode;
	int score;
} XL_PkgClockArtifactEnumFields;

typedef struct XL_PkgClockArtifactLarge {
	int64_t a;
	int64_t b;
	int64_t c;
	int64_t d;
	double weight;
	double bias;
} XL_PkgClockArtifactLarge;

XL_EXPORT XL_PkgClockArtifactPair pkg_clock_artifact_pair_slot = { 12, 30 };
XL_EXPORT XL_PkgClockArtifactLayout pkg_clock_artifact_layout_slot = { 3, 20, 19.5 };
XL_EXPORT XL_PkgClockArtifactOuter pkg_clock_artifact_outer_slot = { { 10, 20 }, 9, 3.5 };

XL_EXPORT int pkg_clock_artifact_pair_slot_sum(void)
{
	return pkg_clock_artifact_pair_slot.x + pkg_clock_artifact_pair_slot.y;
}

XL_EXPORT int pkg_clock_artifact_pair_slot_is_updated(void)
{
	return (pkg_clock_artifact_pair_slot.x == 20) && (pkg_clock_artifact_pair_slot.y == 22);
}

XL_EXPORT XL_PkgClockArtifactPair pkg_clock_artifact_pair_make(int x, int y)
{
	XL_PkgClockArtifactPair pair;
	pair.x = x;
	pair.y = y;
	return pair;
}

XL_EXPORT int pkg_clock_artifact_pair_value_sum(XL_PkgClockArtifactPair pair)
{
	return pair.x + pair.y;
}

XL_EXPORT XL_PkgClockArtifactPair pkg_clock_artifact_pair_add(XL_PkgClockArtifactPair pair, int delta)
{
	pair.x += delta;
	pair.y += delta;
	return pair;
}

XL_EXPORT XL_PkgClockArtifactLayout pkg_clock_artifact_layout_make(int8_t tag, int32_t count, double weight)
{
	XL_PkgClockArtifactLayout layout;
	layout.tag = tag;
	layout.count = count;
	layout.weight = weight;
	return layout;
}

XL_EXPORT int pkg_clock_artifact_layout_score(XL_PkgClockArtifactLayout layout)
{
	return (int)layout.tag + (int)layout.count + (int)layout.weight;
}

XL_EXPORT XL_PkgClockArtifactLayout pkg_clock_artifact_layout_add(XL_PkgClockArtifactLayout layout, int32_t delta)
{
	layout.count += delta;
	layout.weight += (double)delta;
	return layout;
}

XL_EXPORT int pkg_clock_artifact_layout_slot_score(void)
{
	return pkg_clock_artifact_layout_score(pkg_clock_artifact_layout_slot);
}

XL_EXPORT XL_PkgClockArtifactOuter pkg_clock_artifact_outer_make(int32_t x, int32_t y, int32_t z, double scale)
{
	XL_PkgClockArtifactOuter outer;
	outer.inner.x = x;
	outer.inner.y = y;
	outer.z = z;
	outer.scale = scale;
	return outer;
}

XL_EXPORT int pkg_clock_artifact_outer_score(XL_PkgClockArtifactOuter outer)
{
	return outer.inner.x + outer.inner.y + outer.z + (int)outer.scale;
}

XL_EXPORT int pkg_clock_artifact_enum_fields_score(XL_PkgClockArtifactEnumFields fields)
{
	return fields.mode + fields.rawMode + fields.score;
}

XL_EXPORT XL_PkgClockArtifactOuter pkg_clock_artifact_outer_add(XL_PkgClockArtifactOuter outer, int32_t delta)
{
	outer.inner.x += delta;
	outer.inner.y += delta;
	outer.z += delta;
	outer.scale += (double)delta;
	return outer;
}

XL_EXPORT int pkg_clock_artifact_outer_slot_score(void)
{
	return pkg_clock_artifact_outer_score(pkg_clock_artifact_outer_slot);
}

XL_EXPORT XL_PkgClockArtifactLarge pkg_clock_artifact_large_make(int64_t a, int64_t b, int64_t c, int64_t d, double weight, double bias)
{
	XL_PkgClockArtifactLarge large;
	large.a = a;
	large.b = b;
	large.c = c;
	large.d = d;
	large.weight = weight;
	large.bias = bias;
	return large;
}

XL_EXPORT int pkg_clock_artifact_large_score(XL_PkgClockArtifactLarge large)
{
	return (int)(large.a + large.b + large.c + large.d + (int64_t)large.weight + (int64_t)large.bias);
}

XL_EXPORT XL_PkgClockArtifactLarge pkg_clock_artifact_large_add(XL_PkgClockArtifactLarge large, int64_t delta)
{
	large.a += delta;
	large.b += delta;
	large.c += delta;
	large.d += delta;
	large.weight += (double)delta;
	large.bias += (double)delta;
	return large;
}

typedef int (*XL_PkgClockArtifactCallback)(int value, const char* text);
typedef void (*XL_PkgClockArtifactVoidCallback)(void);

XL_EXPORT int pkg_clock_artifact_callback_accept(XL_PkgClockArtifactCallback cb)
{
	if ( cb == NULL ) {
		return -1;
	}
	return cb(37, "hello");
}

XL_EXPORT XL_PkgClockArtifactCallback pkg_clock_artifact_callback_identity(XL_PkgClockArtifactCallback cb)
{
	return cb;
}

XL_EXPORT void pkg_clock_artifact_callback_void(XL_PkgClockArtifactVoidCallback cb)
{
	if ( cb != NULL ) {
		cb();
	}
}

XL_EXPORT int pkg_clock_artifact_add_one(int value)
{
	return value + 1;
}

XL_EXPORT const char* pkg_clock_artifact_text(void)
{
	return "probe-text";
}

XL_EXPORT int pkg_clock_artifact_text_len(const char* value)
{
	if ( value == NULL ) {
		return 0;
	}
	return (int)strlen(value);
}

XL_EXPORT void pkg_clock_artifact_touch(void)
{
}

XL_EXPORT int pkg_clock_artifact_text_plus_len(const char* value, int extra)
{
	if ( value == NULL ) {
		return extra;
	}
	return (int)strlen(value) + extra;
}

XL_EXPORT int pkg_clock_artifact_is_probe_text(void)
{
	return 1;
}

XL_EXPORT int pkg_clock_artifact_bool_score(int flag)
{
	return flag ? 7 : 3;
}

XL_EXPORT int pkg_clock_artifact_enum_alias_score(int flag)
{
	return flag + 40;
}

XL_EXPORT double pkg_clock_artifact_float_value(void)
{
	return 3.5;
}

XL_EXPORT double pkg_clock_artifact_add_half(double value)
{
	return value + 0.5;
}

XL_EXPORT int32_t pkg_clock_artifact_i32_add(int32_t value, int32_t delta)
{
	return value + delta;
}

XL_EXPORT uint32_t pkg_clock_artifact_u32_add(uint32_t value, uint32_t delta)
{
	return value + delta;
}

XL_EXPORT int64_t pkg_clock_artifact_i64_add(int64_t value, int64_t delta)
{
	return value + delta;
}

XL_EXPORT uint64_t pkg_clock_artifact_u64_add(uint64_t value, uint64_t delta)
{
	return value + delta;
}

XL_EXPORT int8_t pkg_clock_artifact_i8_add(int8_t value, int8_t delta)
{
	return (int8_t)(value + delta);
}

XL_EXPORT uint8_t pkg_clock_artifact_u8_add(uint8_t value, uint8_t delta)
{
	return (uint8_t)(value + delta);
}

XL_EXPORT int16_t pkg_clock_artifact_i16_add(int16_t value, int16_t delta)
{
	return (int16_t)(value + delta);
}

XL_EXPORT uint16_t pkg_clock_artifact_u16_add(uint16_t value, uint16_t delta)
{
	return (uint16_t)(value + delta);
}

XL_EXPORT float pkg_clock_artifact_f32_add(float value, float delta)
{
	return value + delta;
}

XL_EXPORT double pkg_clock_artifact_f64_add(double value, double delta)
{
	return value + delta;
}

XL_EXPORT int pkg_clock_artifact_var_int_plus(XL_ArtifactXValue value, int extra)
{
	if ( (value == 0) || (value->Type != XL_ARTIFACT_XVO_DT_INT) ) {
		return -1000;
	}
	return (int)value->vInt + extra;
}

XL_EXPORT int pkg_clock_artifact_var_text_len(XL_ArtifactXValue value)
{
	if ( (value == 0) || (value->Type != XL_ARTIFACT_XVO_DT_TEXT) || (value->vText == 0) ) {
		return -1000;
	}
	return (int)value->Size;
}
