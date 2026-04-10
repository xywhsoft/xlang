#if defined(_WIN32) || defined(_WIN64)
#define XL_EXPORT __declspec(dllexport)
#else
#define XL_EXPORT
#endif

XL_EXPORT int pkg_clock_artifact_probe(void)
{
	return 123;
}
