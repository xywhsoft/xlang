struct BindingPlain {
	int x;
	int y;
};

struct BindingMulti {
	int a, b;
	const char *name, *label;
};

typedef struct BindingTagged {
	int x;
	int y;
} BindingAlias;

typedef struct BindingAliasPair {
	int a;
	int b;
} BindingAliasOne, BindingAliasTwo, *BindingAliasPtr;

typedef struct BindingLateTag BindingLateAlias;
struct BindingLateTag {
	int x;
	int y;
};

struct BindingInner {
	int32_t x;
	int32_t y;
};

struct BindingOuter {
	struct BindingInner inner;
	int32_t z;
	double scale;
};

typedef enum BindingFieldMode {
	BINDING_FIELD_MODE_A = 1,
	BINDING_FIELD_MODE_B = 2
} BindingFieldMode;

typedef int BindingScore;

struct BindingEnumFields {
	BindingFieldMode mode;
	enum BindingFieldMode rawMode;
	BindingScore score;
};

typedef struct BindingBitfield {
	unsigned int flags : 1;
	unsigned int mode : 3;
	int value;
} BindingBitfield, *BindingBitfieldPtr;

struct BindingInlineArray {
	int count;
	char name[16];
};

BindingAlias pkg_clock_artifact_pair_make(int x, int y);
int pkg_clock_artifact_pair_value_sum(struct BindingTagged pair);
BindingLateAlias pkg_clock_artifact_pair_add(BindingLateAlias pair, int delta);
int pkg_clock_artifact_outer_score(struct BindingOuter outer);
int pkg_clock_artifact_enum_fields_score(struct BindingEnumFields fields);
