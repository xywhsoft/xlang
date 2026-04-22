#define PROBE_API __declspec(dllexport)
#define PROBE_API_ALIAS PROBE_API
#define PROBE_EXTERN_API extern
#define PROBE_EXTERN_API_ALIAS PROBE_EXTERN_API
#define PROBE_EMPTY_API
#define PROBE_EMPTY_API_ALIAS PROBE_EMPTY_API

PROBE_API_ALIAS int pkg_clock_artifact_add_one(int value);
PROBE_EXTERN_API_ALIAS DECLSPEC_IMPORT int pkg_clock_artifact_bool_score(_In_range_(0, 1) int __restrict flag);
PROBE_EMPTY_API_ALIAS int pkg_clock_artifact_text_len(_In_reads_(5) const char* restrict text);
