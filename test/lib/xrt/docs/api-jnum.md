# JNUM 数值转换库

> JSON数字与字符串高性能转换

[English](api-jnum.en.md) | [中文](api-jnum.md) | [返回索引](README.md)

---

## 📑 目录

- [类型定义](#类型定义)
- [数字转字符串](#数字转字符串)
- [字符串转数字](#字符串转数字)
- [数字解析](#数字解析)
- [使用场景](#使用场景)

---

## 类型定义

### jnum_type_t

数值类型枚举

```c
typedef enum {
    JNUM_NULL,      // 空值
    JNUM_BOOL,      // 布尔值 (true/false)
    JNUM_INT,       // 32位有符号整数
    JNUM_HEX,       // 32位无符号十六进制整数
    JNUM_LINT,      // 64位有符号整数
    JNUM_LHEX,      // 64位无符号十六进制整数
    JNUM_DOUBLE,    // 双精度浮点数
} jnum_type_t;
```

---

### jnum_value_t

数值联合体

```c
typedef union {
    bool vbool;         // 布尔值
    int32_t vint;       // 32位有符号整数
    uint32_t vhex;      // 32位无符号整数
    int64_t vlint;      // 64位有符号整数
    uint64_t vlhex;     // 64位无符号整数
    double vdbl;        // 双精度浮点数
} jnum_value_t;
```

---

## 数字转字符串

### xrtI32ToStr / xrtI64ToStr

整数转字符串

**函数原型：**
```c
XXAPI int xrtI32ToStr(int32_t num, char* buffer);
XXAPI int xrtI64ToStr(int64_t num, char* buffer);
```

**参数：**
- `num` - 整数值
- `buffer` - 输出缓冲区（需预分配足够空间）

**返回值：**
- 写入的字符数

**示例：**
```c
char buf[32];
int len = xrtI32ToStr(12345, buf);
printf("%s (length: %d)\n", buf, len);  // "12345" (length: 5)

len = xrtI64ToStr(-999, buf);
printf("%s\n", buf);  // "-999"
```

---

### xrtU32ToStr / xrtU64ToStr

无符号整数转十六进制字符串

**函数原型：**
```c
XXAPI int xrtU32ToStr(uint32_t num, char* buffer);
XXAPI int xrtU64ToStr(uint64_t num, char* buffer);
```

**参数：**
- `num` - 无符号整数值
- `buffer` - 输出缓冲区（需预分配足够空间）

**返回值：**
- 写入的字符数

**说明：**
- 输出格式为十六进制，带 `0x` 前缀
- 小写字母表示（a-f）

**示例：**
```c
char buf[32];
int len = xrtU32ToStr(255, buf);
printf("%s\n", buf);  // "0xff"

len = xrtU64ToStr(0xDEADBEEF, buf);
printf("%s\n", buf);  // "0xdeadbeef"
```

---

### xrtNumToStr

浮点数转字符串

**函数原型：**
```c
XXAPI int xrtNumToStr(double num, char* buffer);
```

**参数：**
- `num` - 浮点数值
- `buffer` - 输出缓冲区

**返回值：**
- 写入的字符数

**示例：**
```c
char buf[64];
int len = xrtNumToStr(3.14159, buf);
printf("%s\n", buf);  // "3.14159"
```

---

## 字符串转数字

### xrtStrToI32 / xrtStrToI64

字符串转整数

**函数原型：**
```c
XXAPI int32_t xrtStrToI32(const char* pStr);
XXAPI int64_t xrtStrToI64(const char* pStr);
```

**参数：**
- `pStr` - 数字字符串

**返回值：**
- 整数值

**示例：**
```c
int32_t n1 = xrtStrToI32("12345");    // 12345
int64_t n2 = xrtStrToI64("-999");     // -999
```

---

### xrtStrToU32 / xrtStrToU64

字符串转无符号整数

**函数原型：**
```c
XXAPI uint32_t xrtStrToU32(const char* pStr);
XXAPI uint64_t xrtStrToU64(const char* pStr);
```

---

### xrtStrToNum

字符串转浮点数

**函数原型：**
```c
XXAPI double xrtStrToNum(const char* pStr);
```

**参数：**
- `pStr` - 数字字符串

**返回值：**
- 浮点数值

**支持格式：**
- 普通格式：`3.14`, `-2.5`
- 科学计数法：`1.23e10`, `4.56E-5`
- 整数：`100` → `100.0`

**示例：**
```c
double n1 = xrtStrToNum("3.14159");      // 3.14159
double n2 = xrtStrToNum("-2.5");         // -2.5
double n3 = xrtStrToNum("1.23e10");      // 12300000000.0
```

---

## 数字解析

### xrtParseNum

解析数字字符串（带类型识别）

**函数原型：**
```c
XXAPI int xrtParseNum(const char *str, jnum_type_t *type, jnum_value_t *value);
```

**参数：**
- `str` - 数字字符串
- `type` - 输出：数字类型
- `value` - 输出：数字值

**返回值：**
- 解析的字符数
- 解析失败时返回 0，`type` 设为 `JNUM_NULL`

**支持格式：**
- 十进制整数：`123`, `-456`
- 十六进制：`0xFF`, `0x1234ABCD`
- 浮点数：`3.14`, `-2.5e10`
- 正负号：`+123`, `-456`

**自动类型判断：**
- 值在 `INT32` 范围内 → `JNUM_INT`
- 值在 `INT64` 范围内 → `JNUM_LINT`
- 十六进制32位 → `JNUM_HEX`
- 十六进制64位 → `JNUM_LHEX`
- 含小数点或指数 → `JNUM_DOUBLE`

**示例：**
```c
jnum_type_t type;
jnum_value_t value;

int len = xrtParseNum("12345", &type, &value);
if (type == JNUM_INT) {
    printf("Int: %d\n", value.vint);
}

len = xrtParseNum("0xFF", &type, &value);
if (type == JNUM_HEX) {
    printf("Hex: 0x%X\n", value.vhex);
}

len = xrtParseNum("3.14", &type, &value);
if (type == JNUM_DOUBLE) {
    printf("Double: %f\n", value.vdbl);
}

// 大整数自动识别为 JNUM_LINT
len = xrtParseNum("9999999999", &type, &value);
if (type == JNUM_LINT) {
    printf("Long Int: %lld\n", value.vlint);
}
```

---

### xrtParseNumSkipSpace

跳过前导空白字符后解析数字

**函数原型：**
```c
static inline int xrtParseNumSkipSpace(const char *str, jnum_type_t *type, jnum_value_t *value);
```

**参数：**
- `str` - 数字字符串（可包含前导空白）
- `type` - 输出：数字类型
- `value` - 输出：数字值

**返回值：**
- 解析的总字符数（包含跳过的空白字符）

**跳过的空白字符：**
- 空格 `' '`
- 制表符 `\t`
- 换行符 `\n`, `\r`
- 换页符 `\f`
- 退格符 `\b`
- 垂直制表符 `\v`

**示例：**
```c
jnum_type_t type;
jnum_value_t value;

int len = xrtParseNumSkipSpace("   123", &type, &value);
printf("Parsed %d chars, value: %d\n", len, value.vint);  // Parsed 6 chars, value: 123
```

---

## 使用场景

### 1. 数字格式化

```c
void FormatNumber(int64_t num) {
    char buf[32];
    int len = xrtI64ToStr(num, buf);
    printf("Number: %s\n", buf);
}
```

---

### 2. 数字解析

```c
void ParseNumber(const char* str) {
    jnum_type_t type;
    jnum_value_t value;
    
    xrtParseNum(str, &type, &value);
    
    switch (type) {
        case JNUM_INT:
            printf("Integer: %d\n", value.vint);
            break;
        case JNUM_DOUBLE:
            printf("Double: %f\n", value.vdbl);
            break;
        default:
            break;
    }
}
```

---

### 3. 数据转换

```c
// 整数转字符串
void IntToString(int32_t value, char* output) {
    xrtI32ToStr(value, output);
}

// 字符串转整数
int32_t StringToInt(const char* input) {
    return xrtStrToI32(input);
}
```

---

## 性能特点

### 高性能转换

JNUM 使用优化算法：
- **比 sprintf/sscanf 快数倍**
- **比 atoi/atof 更准确**
- **支持完整的 JSON 数字格式**

### 基准测试参考

```c
// sprintf: ~500ns per call
// xrtI64ToStr: ~100ns per call (5x faster)

// sscanf: ~800ns per call
// xrtStrToI64: ~150ns per call (5x faster)
```

---

## 精度说明

### 整数精度

- 支持 64位有符号整数
- 范围：-9,223,372,036,854,775,808 ~ 9,223,372,036,854,775,807

### 浮点数精度

- 使用 IEEE 754 双精度（64位）
- 有效数字：约15-17位
- 范围：±2.23e-308 ~ ±1.80e+308

---

## 边界情况处理

### 整数溢出

```c
// 超出 INT64 范围的值会转为 JNUM_DOUBLE 类型
jnum_type_t type;
jnum_value_t value;
xrtParseNum("99999999999999999999", &type, &value);
// type == JNUM_DOUBLE
```

### 浮点数特殊值

```c
char buf[32];

// 特殊值转字符串
xrtNumToStr(INFINITY, buf);   // "inf"
xrtNumToStr(-INFINITY, buf);  // "-inf"
xrtNumToStr(NAN, buf);        // "nan"
```

---

## 最佳实践

### 1. 验证输入

```c
bool IsValidNumber(const char* text) {
    jnum_type_t type;
    jnum_value_t value;
    int len = xrtParseNum(text, &type, &value);
    return (len > 0 && type != JNUM_NULL);
}

double SafeToDouble(const char* text) {
    jnum_type_t type;
    jnum_value_t value;
    if (xrtParseNum(text, &type, &value) > 0) {
        switch (type) {
        case JNUM_INT:    return (double)value.vint;
        case JNUM_LINT:   return (double)value.vlint;
        case JNUM_HEX:    return (double)value.vhex;
        case JNUM_LHEX:   return (double)value.vlhex;
        case JNUM_DOUBLE: return value.vdbl;
        default:          return 0.0;
        }
    }
    return 0.0;
}
```

---

### 2. 缓冲区管理

```c
// JNUM 使用用户提供的缓冲区，无需额外内存管理
char buffer[32];  // 32字节足够存储任何数字

// ✅ 直接使用栈上缓冲区
xrtI64ToStr(123456789, buffer);
printf("%s\n", buffer);

// ✅ 批量转换时复用缓冲区
for (int i = 0; i < 1000000; i++) {
    xrtI32ToStr(i, buffer);
    ProcessString(buffer);
}
```

---

### 3. 缓冲区大小建议

```c
// 推荐的缓冲区大小
#define INT32_BUF_SIZE   12   // "-2147483648" + '\0'
#define INT64_BUF_SIZE   21   // "-9223372036854775808" + '\0'
#define HEX32_BUF_SIZE   11   // "0xffffffff" + '\0'
#define HEX64_BUF_SIZE   19   // "0xffffffffffffffff" + '\0'
#define DOUBLE_BUF_SIZE  32   // 足够存储任何双精度浮点数
```

---

## 常见错误

### 1. 缓冲区过小

```c
// ❌ 错误：缓冲区可能溢出
char buf[8];
xrtI64ToStr(123456789012345, buf);  // 需要16字节

// ✅ 正确：使用足够大的缓冲区
char buf[32];
xrtI64ToStr(123456789012345, buf);
```

---

### 2. 未检查解析结果

```c
// ❌ 错误：未检查返回值
jnum_type_t type;
jnum_value_t value;
xrtParseNum("abc", &type, &value);
int n = value.vint;  // type 可能是 JNUM_NULL

// ✅ 正确：检查类型后再使用
jnum_type_t type;
jnum_value_t value;
if (xrtParseNum("123", &type, &value) > 0 && type == JNUM_INT) {
    int n = value.vint;
}
```

---

### 3. 类型混淆

```c
// ❌ 错误：使用错误的联合体成员
jnum_type_t type;
jnum_value_t value;
xrtParseNum("3.14", &type, &value);
int n = value.vint;  // 错误！type 是 JNUM_DOUBLE

// ✅ 正确：根据类型选择成员
switch (type) {
case JNUM_INT:    printf("%d", value.vint);   break;
case JNUM_LINT:   printf("%lld", value.vlint); break;
case JNUM_DOUBLE: printf("%f", value.vdbl);   break;
// ...
}
```

---

## 与标准库对比

| 函数 | 标准库 | JNUM | 优势 |
|------|--------|------|------|
| 整数→字符串 | `sprintf` | `xrtI32ToStr` / `xrtI64ToStr` | 5x faster |
| 字符串→整数 | `atoi/strtol` | `xrtStrToI32` / `xrtStrToI64` | 更安全，自动类型识别 |
| 浮点→字符串 | `sprintf` | `xrtNumToStr` | 智能精度，无尾零 |
| 字符串→浮点 | `atof/strtod` | `xrtStrToNum` | 更快 |
| 通用解析 | 无 | `xrtParseNum` | 自动类型识别 |

---

## 相关文档

- [JSON 处理](api-json.md) - JSON解析和生成
- [Value 动态类型](api-value.md) - 动态类型转换
- [String 字符串处理](api-string.md) - 字符串操作
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#jnum-数值转换库)

</div>
