/* Pad to keep the deliberately wrong-width read bounded in the initial probe. */
struct { int count; int padding; } xlir_pkg_bad_count = {42, 0};
