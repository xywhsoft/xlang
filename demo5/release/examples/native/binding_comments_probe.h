/* Header noise that should not affect binding import. */
#define PROBE_URL "http://example.test/a//b"
#define PROBE_BLOCK_VALUE (/* inline expression comment */ 40 + 2)

/*
 * Multi-line block comments are common in vendor headers.
 */
int WINAPI pkg_clock_artifact_add_one(
	int value /* parameter note */
);
