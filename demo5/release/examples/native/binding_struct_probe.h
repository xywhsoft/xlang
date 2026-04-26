typedef struct { int x; int y; } NativePair;
extern NativePair pkg_clock_artifact_pair_slot;
NativePair pkg_clock_artifact_pair_make(int x, int y);
int pkg_clock_artifact_pair_value_sum(NativePair pair);
int pkg_clock_artifact_pair_slot_sum(void);
int pkg_clock_artifact_pair_slot_is_updated(void);
