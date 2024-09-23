


/*
	类型结构（二进制）：
		前16位：0000 0000 0000 0000
			第 1 位为变体类型标记，为 0x80000000 表示为变体类型，为 0 时表示为编译时确定的静态类型
			第 2 - 14 位为子类型表达区域
				第 2 - 3 位可表示复数的子类型
				第 4 - 6 位可表示浮点数的子类型（含货币类型）		[余 1 个空位可供扩展]
				第 7 - 10 位可表示整数的子类型					[余 5 个空位可供扩展]
				第 11 - 12 位可表示字符串的子类型
				第 13 位可获取函数是 xfunc 或 cfunc 子类型（需配合主类型一起判断）
				第 14 位可获取逻辑是 false 或 true 子类型（需配合主类型一起判断）
			第 15 - 16 位为数字细分类型表达区域
		后16位：0000 0000 0000 0000
			第 1 - 8 位为预留空间，暂无含义
			第 9 - 16 位为主类型表达区域
*/



// 主类型定义（范围 0 - 255）
#define XLVMT_EMPTY				0				// 无值，不占用内存
#define XLVMT_NULL				1				// 4 字节，单一值 null
#define XLVMT_BOOL				2				// 4 字节，true 或 false，Type and 1 可以取值
#define XLVMT_NUMBER			3				// 多个子类型
#define XLVMT_STRING			4				// 多个子类型
#define XLVMT_BINARY			5				// 4 + 4 + sizeof(ptr) 字节
#define XLVMT_COLLECT			6
#define XLVMT_ARRAY				7
#define XLVMT_STRUCT			8
#define XLVMT_TABLE				9
#define XLVMT_OBJECT			10
#define XLVMT_FUNCTION			11				// 多个子类型
#define XLVMT_POINT				253				// 4 + sizeof(ptr) 字节，保存指针地址
#define XLVMT_CARRAY			254				// 4 + 4 + sizeof(ptr) 字节，保存成员长度 + 指针地址
#define XLVMT_CUSTOM			255				// 4 + 4 + ? 字节，保存额外数据长度，及额外数据本身



// 数字细分类型定义
#define XLVT_NUM_INT			0x00000000		// 数字细分类型：整数
#define XLVT_NUM_FLOAT			0x00010000		// 数字细分类型：浮点数
#define XLVT_NUM_CURR			0x00020000		// 数字细分类型：货币
#define XLVT_NUM_COMPLEX		0x00030000		// 数字细分类型：复数

// 逻辑类型子类型定义
#define XLVT_BOOL_FALSE			0x00000000		// 逻辑值：false
#define XLVT_BOOL_TRUE			0x00040000		// 逻辑值：true

// 函数类型子类型定义
#define XLVT_XFUNC				0x00000000		// 4 + sizeof(ptr) + ? 字节，包含函数指针、函数定义信息
#define XLVT_CFUNC				0x00080000		// 4 + sizeof(ptr) 字节，保存函数指针

// 字符串类型子类型定义
#define XLVT_ASTR				0x00100000		// 4 + 4 + sizeof(ptr) 字节
#define XLVT_WSTR				0x00200000		// 4 + 4 + sizeof(ptr) 字节
#define XLVT_USTR				0x00300000		// 4 + 4 + sizeof(ptr) 字节

// 整数类型子类型定义
#define XLVT_I8					0x00400000		// 4 + 4 字节
#define XLVT_U8					0x00800000		// 4 + 4 字节
#define XLVT_I16				0x00C00000		// 4 + 4 字节
#define XLVT_U16				0x01000000		// 4 + 4 字节
#define XLVT_I32				0x01400000		// 4 + 4 字节
#define XLVT_U32				0x01800000		// 4 + 4 字节
#define XLVT_I64				0x01C00000		// 4 + 8 字节
#define XLVT_U64				0x02000000		// 4 + 8 字节
#define XLVT_I128				0x02400000		// 4 + 16 字节
#define XLVT_U128				0x02800000		// 4 + 16 字节

// 浮点数类型子类型定义（含货币类型）
#define XLVT_F32				0x04000000		// 4 + 4 字节
#define XLVT_F64				0x08000000		// 4 + 8 字节
#define XLVT_F128				0x0C000000		// 4 + 16 字节
#define XLVT_CURR				0x10000000		// 4 + 8 字节
#define XLVT_LONGCURR			0x14000000		// 4 + 16 字节
#define XLVT_BIGNUM				0x18000000		// 4 + ? 字节					暂未实现

// 复数类型子类型定义
#define XLVT_COMPLEXF32			0x20000000		// 4 + 8 字节					暂未实现
#define XLVT_COMPLEXF64			0x40000000		// 4 + 16 字节					暂未实现
#define XLVT_COMPLEXF128		0x60000000		// 4 + 32 字节					暂未实现

// 变体类型标记（无此标记为编译时确定的静态类型）
#define XLVT_VARIANT			0x80000000		// 指示为变体类型

// 各种类型掩码定义
#define XLVT_MASK_BOOL			0x00040000		// 逻辑子类型掩码，可获取 true 或 false
#define XLVT_MASK_FUNC			0x00080000		// 函数子类型掩码，可获取 xfunc 或 cfunc
#define XLVT_MASK_STR			0x00300000		// 字符串子类型掩码，可获取 astr、wstr、ustr
#define XLVT_MASK_INT			0x03C00000		// 整数子类型掩码，可获取 8、16、32、64、128 有无符号，共计 10 种子类型
#define XLVT_MASK_FLOAT			0x1C000000		// 浮点数子类型掩码，可获取 f32、f64、f128、curr、longcurr、bignum 共计 6 种子类型
#define XLVT_MASK_COMPLEX		0x60000000		// 复数子类型掩码，可获取 c32、c64、c128 共计 3 种子类型
#define XLVT_MASK_NUM			0x7FC00000		// 数字子类型掩码，可以获取所有数字子类型
#define XLVT_MASK_SUBTYPE		0x7FFC0000		// 子类型掩码，可以获取所有子类型
#define XLVT_MASK_NUMTYPE		0x00030000		// 数字类型分类掩码，可以获取 int、float、curr、complex 四种细分类型



// 数据类型父类（4字节，只能表示 null 或 bool）
typedef struct {
	uint32 Type;
} xVariantStruct, *xVariant;

// 数据类型（8字节子类）
typedef struct {
	uint32 Type;
	union {
		int8 iVal8;
		int16 iVal16;
		int32 iVal32;
		uint8 uVal8;
		uint16 uVal16;
		uint32 uVal32;
		float fVal32;
	}
} xVariantStruct8, *xVariant8;

// 数据类型（12字节子类）
typedef struct {
	uint32 Type;
	union {
		int64 iVal64;
		uint64 uVal64;
		double dVal64;
		int64 iCurr64;
		ptr pVal;
	}
} xVariantStruct12, *xVariant12;

// 数据类型（16字节子类）
typedef struct {
	uint32 Type;
	uint32 Length;
	union {
		astr sValA;
		wstr sValW;
		ustr sValU;
		ptr pVal;
	}
} xVariantStruct16, *xVariant16;

// 数据类型（20字节子类）
typedef struct {
	uint32 Type;
	union {
		int128 iVal128;
		uint128 uVal128;
		double dVal128;
		int128 iCurr128;
		ptr pVal;
	}
} xVariantStruct20, *xVariant20;

// 数据类型（20字节子类 : carray）
typedef struct {
	uint32 Type;
	uint32 ItemType;
	uint32 ItemLength;
	ptr pArray;
} xVariantStruct_CArray, *xVariant_CArray;

// 数据类型（20字节子类 : custom）
typedef struct {
	uint32 Type;
	uint32 ItemType;
	uint32 ItemLength;
	ptr pArray;
} xVariantStruct_Custom, *xVariant_Custom;

// 数据类型（20字节子类 : xfunc）
typedef struct {
	uint32 Type;
	uint32 ItemType;
	uint32 ItemLength;
	ptr pArray;
} xVariantStruct_XFunc, *xVariant_XFunc;


