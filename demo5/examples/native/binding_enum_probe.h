typedef enum ProbeMode {
	PROBE_MODE_OFF = 0,
	PROBE_MODE_ON,
	PROBE_MODE_HEX = 0x2a,
	PROBE_MODE_SHIFT = 1 << 5,
	PROBE_MODE_EXPR = PROBE_MODE_SHIFT | 0x0a
} ProbeMode, ProbeModeAlias, *ProbeModePtr;

enum ProbePlain {
	PROBE_PLAIN_A = -1,
	PROBE_PLAIN_B
};

int pkg_clock_artifact_bool_score(ProbeMode flag);
int pkg_clock_artifact_enum_alias_score(ProbeModeAlias flag);
