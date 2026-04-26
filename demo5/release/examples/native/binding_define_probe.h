typedef int PROBE_ALIAS_INT;

#define PROBE_ANSWER 42
#define PROBE_ANSWER_ALIAS PROBE_ANSWER
#define PROBE_ANSWER_ALIAS_2 PROBE_ANSWER_ALIAS
#define PROBE_HEX_VALUE 0x2aU
#define PROBE_BIN_VALUE 0b101010U
#define PROBE_SEP_VALUE 1'000
#define PROBE_OCT_VALUE 052
#define PROBE_RATIO 2.5
#define PROBE_RATIO_ALIAS PROBE_RATIO
#define PROBE_RATIO_ALIAS_2 PROBE_RATIO_ALIAS
#define PROBE_TEXT "hello"
#define PROBE_TEXT_JOIN "hello" " world"
#define PROBE_TEXT_PAREN ("wrapped")
#define PROBE_TEXT_U8 u8"utf8"
#define PROBE_TEXT_ALIAS PROBE_TEXT_JOIN
#define PROBE_CHAR 'A'
#define PROBE_CHAR_NEWLINE '\n'
#define PROBE_CHAR_HEX '\x2a'
#define PROBE_CHAR_OCT '\052'
#define PROBE_CHAR_WIDE L'\x2a'
#define PROBE_CHAR_U u'\052'
#define PROBE_CHAR_U8 u8'\n'
#define PROBE_VOID_NULL ((void*)0)
#define PROBE_HANDLE_NULL ((HANDLE)NULL)
#define PROBE_LPVOID_NULL ((LPVOID)0)
#define PROBE_NULL NULL
#define PROBE_TRUE_VALUE TRUE
#define PROBE_FALSE_VALUE FALSE
#define PROBE_SHIFT (1 << 3)
#define PROBE_MASK (PROBE_SHIFT | 2)
#define PROBE_BIN_EXPR (0b100000 | 0b001010)
#define PROBE_SEP_EXPR (1'000 + 24)
#define PROBE_OCT_EXPR (040 | 012)
#define PROBE_CHAR_PREFIX_EXPR (L'\x20' | u'\x0a')
#define PROBE_MATH ((PROBE_MASK + 5) * 2)
#define PROBE_CAST ((unsigned long)(PROBE_MASK + 1))
#define PROBE_EQ (PROBE_MASK == 10)
#define PROBE_NE (PROBE_MASK != 9)
#define PROBE_CMP (PROBE_MASK >= 10)
#define PROBE_LOGIC ((PROBE_EQ && PROBE_NE) || 0)
#define PROBE_CONST_TERNARY (PROBE_EQ ? PROBE_ANSWER : 7)
#define PROBE_CONST_TERNARY_NAKED (PROBE_MASK ? PROBE_ANSWER : 7)
#define PROBE_CONST_TERNARY_FALSE (0 ? 7 : PROBE_ANSWER)
#define PROBE_SIZE_INT ((int)sizeof(int))
#define PROBE_SIZE_PTR ((int)sizeof(void*))
#define PROBE_SIZE_CONST_CHAR_PTR ((int)sizeof(char const *))
#define PROBE_SKIP_FUNC(x) ((x) + 1)
#define PROBE_ADD(x, y) ((x) + (y))
#define PROBE_MIX(x, y) (((x) << 1) | (y))
#define PROBE_WITH_CONST(x) ((x) + PROBE_ANSWER)
#define PROBE_SIZE_ADD(x) ((x) + (int)sizeof(unsigned short))
#define PROBE_BOOL_MATH(x) ((x) + TRUE + FALSE)
#define PROBE_IS_ANSWER(x) ((x) == PROBE_ANSWER)
#define PROBE_RANGE_OK(x) (((x) >= 40) && ((x) <= 42))
#define PROBE_NOT_ZERO(x) (!(x))
#define PROBE_MIN(x, y) (((x) < (y)) ? (x) : (y))
#define PROBE_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define PROBE_PICK_OR_ANSWER(x) ((x) ? (x) : PROBE_ANSWER)
#define PROBE_BOOL_TERNARY(x) (((x) > 0) ? PROBE_ANSWER : 7)
#define PROBE_CAST_ALIAS(x) ((PROBE_ALIAS_INT)(x))
#define PROBE_GET_ANSWER() (PROBE_ANSWER)
#define PROBE_MULTILINE(x) \
	((x) + \
	 PROBE_ANSWER)
