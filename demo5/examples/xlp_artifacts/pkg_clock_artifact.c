#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XL_EXPORT __declspec(dllexport)
#else
#define XL_EXPORT
#endif

XL_EXPORT int pkg_clock_artifact_probe(void)
{
	return 123;
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

XL_EXPORT double pkg_clock_artifact_float_value(void)
{
	return 3.5;
}

XL_EXPORT double pkg_clock_artifact_add_half(double value)
{
	return value + 0.5;
}
