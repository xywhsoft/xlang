typedef struct {
	int x;
	int y;
} NativePair2;

NativePair2
pkg_clock_artifact_pair_make(
	int x,
	int y
);

int
pkg_clock_artifact_pair_value_sum(
	NativePair2 pair
);
