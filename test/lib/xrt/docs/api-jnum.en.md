# JNUM Number Conversion Library

> High-performance JSON number and string conversion

[English](api-jnum.en.md) | [中文](api-jnum.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Type Definitions](#type-definitions)
- [Number to String](#number-to-string)
- [String to Number](#string-to-number)
- [Number Parsing](#number-parsing)
- [Use Cases](#use-cases)

---

## Type Definitions

### jnum_type_t

Number type enumeration

```c
typedef enum {
    JNUM_NULL,      // Null value
    JNUM_BOOL,      // Boolean (true/false)
    JNUM_INT,       // 32-bit signed integer
    JNUM_HEX,       // 32-bit unsigned hexadecimal integer
    JNUM_LINT,      // 64-bit signed integer
    JNUM_LHEX,      // 64-bit unsigned hexadecimal integer
    JNUM_DOUBLE,    // Double precision floating point
} jnum_type_t;
```

---

### jnum_value_t

Number value union

```c
typedef union {
    bool vbool;         // Boolean value
    int32_t vint;       // 32-bit signed integer
    uint32_t vhex;      // 32-bit unsigned integer
    int64_t vlint;      // 64-bit signed integer
    uint64_t vlhex;     // 64-bit unsigned integer
    double vdbl;        // Double precision floating point
} jnum_value_t;
```

---

## Number to String

### xrtI32ToStr / xrtI64ToStr

Integer to string

**Function Prototype:**
```c
XXAPI int xrtI32ToStr(int32_t num, char* buffer);
XXAPI int xrtI64ToStr(int64_t num, char* buffer);
```

**Parameters:**
- `num` - Integer value
- `buffer` - Output buffer (must be pre-allocated with sufficient space)

**Return Value:**
- Number of characters written

**Example:**
```c
char buf[32];
int len = xrtI32ToStr(12345, buf);
printf("%s (length: %d)\n", buf, len);  // "12345" (length: 5)

len = xrtI64ToStr(-999, buf);
printf("%s\n", buf);  // "-999"
```

---

### xrtU32ToStr / xrtU64ToStr

Unsigned integer to hexadecimal string

**Function Prototype:**
```c
XXAPI int xrtU32ToStr(uint32_t num, char* buffer);
XXAPI int xrtU64ToStr(uint64_t num, char* buffer);
```

**Notes:**
- Output format is hexadecimal with `0x` prefix
- Lowercase letters (a-f)

**Example:**
```c
char buf[32];
int len = xrtU32ToStr(255, buf);
printf("%s\n", buf);  // "0xff"

len = xrtU64ToStr(0xDEADBEEF, buf);
printf("%s\n", buf);  // "0xdeadbeef"
```

---

### xrtNumToStr

Floating point to string

**Function Prototype:**
```c
XXAPI int xrtNumToStr(double num, char* buffer);
```

**Example:**
```c
char buf[64];
int len = xrtNumToStr(3.14159, buf);
printf("%s\n", buf);  // "3.14159"
```

---

## String to Number

### xrtStrToI32 / xrtStrToI64

String to integer

**Function Prototype:**
```c
XXAPI int32_t xrtStrToI32(const char* pStr);
XXAPI int64_t xrtStrToI64(const char* pStr);
```

**Example:**
```c
int32_t n1 = xrtStrToI32("12345");    // 12345
int64_t n2 = xrtStrToI64("-999");     // -999
```

---

### xrtStrToNum

String to floating point

**Function Prototype:**
```c
XXAPI double xrtStrToNum(const char* pStr);
```

**Supported Formats:**
- Normal format: `3.14`, `-2.5`
- Scientific notation: `1.23e10`, `4.56E-5`
- Integer: `100` → `100.0`

**Example:**
```c
double n1 = xrtStrToNum("3.14159");      // 3.14159
double n2 = xrtStrToNum("-2.5");         // -2.5
double n3 = xrtStrToNum("1.23e10");      // 12300000000.0
```

---

## Number Parsing

### xrtParseNum

Parse number string (with type recognition)

**Function Prototype:**
```c
XXAPI int xrtParseNum(const char *str, jnum_type_t *type, jnum_value_t *value);
```

**Parameters:**
- `str` - Number string
- `type` - Output: Number type
- `value` - Output: Number value

**Return Value:**
- Number of characters parsed
- Returns 0 on parse failure, `type` set to `JNUM_NULL`

**Supported Formats:**
- Decimal integers: `123`, `-456`
- Hexadecimal: `0xFF`, `0x1234ABCD`
- Floating point: `3.14`, `-2.5e10`
- Signs: `+123`, `-456`

**Automatic Type Detection:**
- Value in `INT32` range → `JNUM_INT`
- Value in `INT64` range → `JNUM_LINT`
- 32-bit hexadecimal → `JNUM_HEX`
- 64-bit hexadecimal → `JNUM_LHEX`
- Contains decimal point or exponent → `JNUM_DOUBLE`

**Example:**
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

// Large integer automatically recognized as JNUM_LINT
len = xrtParseNum("9999999999", &type, &value);
if (type == JNUM_LINT) {
    printf("Long Int: %lld\n", value.vlint);
}
```

---

### xrtParseNumSkipSpace

Parse number after skipping leading whitespace

**Function Prototype:**
```c
static inline int xrtParseNumSkipSpace(const char *str, jnum_type_t *type, jnum_value_t *value);
```

**Return Value:**
- Total characters parsed (including skipped whitespace)

**Skipped Whitespace:**
- Space `' '`
- Tab `\t`
- Newline `\n`, `\r`
- Form feed `\f`
- Backspace `\b`
- Vertical tab `\v`

---

## Use Cases

### 1. Number Formatting

```c
void FormatNumber(int64_t num) {
    char buf[32];
    int len = xrtI64ToStr(num, buf);
    printf("Number: %s\n", buf);
}
```

### 2. Number Parsing

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

## Performance

### High-Performance Conversion

JNUM uses optimized algorithms:
- **Several times faster than sprintf/sscanf**
- **More accurate than atoi/atof**
- **Supports complete JSON number format**

### Benchmark Reference

```c
// sprintf: ~500ns per call
// xrtI64ToStr: ~100ns per call (5x faster)

// sscanf: ~800ns per call
// xrtStrToI64: ~150ns per call (5x faster)
```

---

## Best Practices

### 1. Input Validation

```c
bool IsValidNumber(const char* text) {
    jnum_type_t type;
    jnum_value_t value;
    int len = xrtParseNum(text, &type, &value);
    return (len > 0 && type != JNUM_NULL);
}
```

### 2. Buffer Size Recommendations

```c
// Recommended buffer sizes
#define INT32_BUF_SIZE   12   // "-2147483648" + '\0'
#define INT64_BUF_SIZE   21   // "-9223372036854775808" + '\0'
#define HEX32_BUF_SIZE   11   // "0xffffffff" + '\0'
#define HEX64_BUF_SIZE   19   // "0xffffffffffffffff" + '\0'
#define DOUBLE_BUF_SIZE  32   // Sufficient for any double
```

---

## Comparison with Standard Library

| Function | Standard Library | JNUM | Advantage |
|----------|-----------------|------|-----------|
| Int→String | `sprintf` | `xrtI32ToStr` / `xrtI64ToStr` | 5x faster |
| String→Int | `atoi/strtol` | `xrtStrToI32` / `xrtStrToI64` | Safer, auto type detection |
| Float→String | `sprintf` | `xrtNumToStr` | Smart precision, no trailing zeros |
| String→Float | `atof/strtod` | `xrtStrToNum` | Faster |
| General Parse | N/A | `xrtParseNum` | Auto type detection |

---

## Related Documentation

- [JSON Processing](api-json.en.md) - JSON parsing and generation
- [Value Dynamic Type](api-value.en.md) - Dynamic type conversion
- [String Processing](api-string.en.md) - String operations
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#jnum-number-conversion-library)

</div>
