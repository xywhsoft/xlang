# JSON 处理库

> JSON SAX 解析与生成 + Value 系统

[English](api-json.en.md) | [中文](api-json.md) | [返回索引](README.md)

---

## 📑 目录

- [重要说明](#重要说明)
- [JSON类型系统](#json类型系统)
- [SAX解析](#sax解析)
- [JSON生成](#json生成)
- [推荐方式：使用Value系统](#推荐方式使用value系统)

---

## 重要说明

**xrt.h 中的 JSON 库提供两种使用方式：**

1. **SAX API（底层）：** 流式解析，适合处理大文件或性能关键场景
2. **Value 系统（推荐）：** 提供类似 JSON 的数据结构和简单 API，**强烈推荐日常使用**

---

## JSON类型系统

### JSON类型枚举

```c
typedef enum {
    JSON_NULL = 0,      // null，无值
    JSON_BOOL,          // true/false，值在 vbool
    JSON_INT,           // int32，值在 vint
    JSON_HEX,           // uint32 十六进制，值在 vhex
    JSON_LINT,          // int64，值在 vlint
    JSON_LHEX,          // uint64 十六进制，值在 vlhex
    JSON_DOUBLE,        // double，值在 vdbl
    JSON_STRING,        // 字符串，值在 vstr
    JSON_ARRAY,         // 数组，子节点在 head
    JSON_OBJECT         // 对象，子节点在 head
} json_type_t;
```

### JSON字符串信息

```c
// 字符串信息结构
typedef struct {
    uint32_t type:4;      // json对象的类型（作为键时有效）
    uint32_t escaped:1;   // 是否含转义字符
    uint32_t alloced:1;   // 是否在堆中分配（SAX接口）
    uint32_t reserved:2;  // 保留位
    uint32_t len:24;      // 字符串长度
} json_strinfo_t;

// 带信息的字符串
typedef struct {
    char *str;            // 字符串数据
    json_strinfo_t info;  // 字符串信息
} json_string_t;
```

### JSON数值类型

```c
// 数字类型的值（支持长整数和十六进制数）
typedef union {
    bool vbool;
    int32_t vint;
    uint32_t vhex;
    int64_t vlint;
    uint64_t vlhex;
    double vdbl;
} json_number_t;
```

---

## SAX解析

### 核心类型

```c
// SAX命令（指示数组/对象的开始和结束）
typedef enum {
    JSON_SAX_START = 0,   // 左括号 [ 或 {
    JSON_SAX_FINISH       // 右括号 ] 或 }
} json_sax_cmd_t;

// SAX解析值
typedef union {
    json_number_t vnum;   // 数字类型的值
    json_string_t vstr;   // 字符串类型的值
    json_sax_cmd_t vcmd;  // 集合对象的开始/结束
} json_sax_value_t;

// SAX解析器状态
typedef struct {
    int total;              // 数组深度的大小
    int index;              // 当前索引
    json_string_t *array;   // 存储类型和键的深度信息
    json_sax_value_t value; // 当前值
} json_sax_parser_t;

// 回调返回值
typedef enum {
    JSON_SAX_PARSE_CONTINUE = 0,  // 继续解析
    JSON_SAX_PARSE_STOP           // 停止解析
} json_sax_ret_t;

// 回调函数类型
typedef json_sax_ret_t (*json_sax_cb_t)(json_sax_parser_t *parser);
```

### xrtJsonParseSAX

SAX风格解析JSON（流式处理）

```c
XXAPI int xrtJsonParseSAX(str text, size_t str_len, json_sax_cb_t cb);
```

**参数：**
| 参数 | 说明 |
|------|------|
| `text` | JSON字符串 |
| `str_len` | 字符串长度 |
| `cb` | SAX回调函数 |

**返回值：** 成功返回 0，失败返回 -1

**示例：**
```c
json_sax_ret_t MyJSONCallback(json_sax_parser_t *parser) {
    json_string_t *jkey = &parser->array[parser->index];
    json_type_t type = jkey->info.type;
    
    switch (type) {
        case JSON_STRING:
            printf("String: %.*s\n", 
                   parser->value.vstr.info.len, 
                   parser->value.vstr.str);
            break;
        case JSON_INT:
            printf("Int: %d\n", parser->value.vnum.vint);
            break;
        case JSON_DOUBLE:
            printf("Double: %f\n", parser->value.vnum.vdbl);
            break;
        case JSON_BOOL:
            printf("Bool: %s\n", parser->value.vnum.vbool ? "true" : "false");
            break;
        case JSON_NULL:
            printf("Null\n");
            break;
        case JSON_ARRAY:
        case JSON_OBJECT:
            if (parser->value.vcmd == JSON_SAX_START)
                printf("%s start\n", type == JSON_ARRAY ? "Array" : "Object");
            else
                printf("%s finish\n", type == JSON_ARRAY ? "Array" : "Object");
            break;
    }
    
    return JSON_SAX_PARSE_CONTINUE;
}

str json = "{\"name\":\"Tom\",\"age\":25}";
xrtJsonParseSAX(json, strlen(json), MyJSONCallback);
```

---

## JSON生成

### 打印配置

```c
// 打印缓冲
typedef struct {
    size_t size;      // 缓冲区大小
    char *p;          // 缓冲区指针
} json_print_ptr_t;

// 打印参数设置
typedef struct {
    size_t str_len;       // 预估字符串长度
    size_t plus_size;     // 扩容步长（默认8192）
    size_t item_size;     // 单项预估大小
    int item_total;       // 预估项数（默认1024）
    bool format_flag;     // 是否格式化输出
    json_print_ptr_t *ptr; // 可选：复用缓冲区
} json_print_choice_t;
```

### xrtJsonPrintStart

启动SAX打印器

```c
XXAPI json_sax_print_hd xrtJsonPrintStart(json_print_choice_t *choice);
```

**返回值：** 成功返回句柄，失败返回 NULL

**便捷函数：**
```c
// 格式化输出
json_sax_print_hd json_sax_print_format_start(int item_total, json_print_ptr_t *ptr);

// 压缩输出
json_sax_print_hd json_sax_print_unformat_start(int item_total, json_print_ptr_t *ptr);
```

### xrtJsonPrintValue

打印JSON值

```c
XXAPI int xrtJsonPrintValue(json_sax_print_hd handle, json_type_t type, json_string_t *jkey, const void *value);
```

**便捷函数：**
```c
int xrtJsonPrintNull(json_sax_print_hd handle, json_string_t *jkey);
int xrtJsonPrintBool(json_sax_print_hd handle, json_string_t *jkey, bool value);
int xrtJsonPrintInt(json_sax_print_hd handle, json_string_t *jkey, int32_t value);
int xrtJsonPrintHex(json_sax_print_hd handle, json_string_t *jkey, uint32_t value);
int xrtJsonPrintInt64(json_sax_print_hd handle, json_string_t *jkey, int64_t value);
int xrtJsonPrintHex64(json_sax_print_hd handle, json_string_t *jkey, uint64_t value);
int xrtJsonPrintDouble(json_sax_print_hd handle, json_string_t *jkey, double value);
int xrtJsonPrintString(json_sax_print_hd handle, json_string_t *jkey, json_string_t *value);
int xrtJsonPrintArray(json_sax_print_hd handle, json_string_t *jkey, json_sax_cmd_t value);
int xrtJsonPrintObject(json_sax_print_hd handle, json_string_t *jkey, json_sax_cmd_t value);
```

**辅助宏：**
```c
// 数组操作
xrtJsonPrintArrayStart(handle, jkey)   // 开始数组
xrtJsonPrintArrayFinish(handle)        // 结束数组

// 对象操作
xrtJsonPrintObjectStart(handle, jkey)  // 开始对象
xrtJsonPrintObjectFinish(handle)       // 结束对象
```

### xrtJsonPrintFinish

结束打印并获取结果

```c
XXAPI char* xrtJsonPrintFinish(json_sax_print_hd handle, size_t *length, json_print_ptr_t *ptr);
```

**参数：**
| 参数 | 说明 |
|------|------|
| `handle` | 打印器句柄 |
| `length` | 输出：结果字符串长度 |
| `ptr` | 输出：可复用的缓冲区（可选） |

**返回值：** JSON字符串（需要调用 `xrtFree` 释放）

### xrtJsonUpdateStringInfo

更新字符串信息（自动计算长度和转义标记）

```c
static inline void xrtJsonUpdateStringInfo(json_string_t *jstr);
```

**示例：**
```c
json_print_choice_t choice = {0};
choice.format_flag = true;  // 格式化输出
json_sax_print_hd handle = xrtJsonPrintStart(&choice);

// 生成 {"name":"Tom","age":25}
json_string_t key_name = {"name", {0}};
json_string_t key_age = {"age", {0}};
json_string_t val_name = {"Tom", {0}};

xrtJsonPrintObjectStart(handle, NULL);
xrtJsonUpdateStringInfo(&val_name);
xrtJsonPrintString(handle, &key_name, &val_name);
xrtJsonPrintInt(handle, &key_age, 25);
xrtJsonPrintObjectFinish(handle);

size_t len;
char* result = xrtJsonPrintFinish(handle, &len, NULL);
printf("%s\n", result);
xrtFree(result);
```

---

## 推荐方式：使用Value系统

**强烈推荐使用 [Value 动态类型系统](api-value.md) 来处理 JSON 数据！**

### JSON 解析与序列化

```c
// 从字符串解析 JSON
XXAPI xvalue xrtParseJSON(str sText, size_t iSize);

// 从文件解析 JSON
XXAPI xvalue xrtParseJSON_File(str sFile);

// 将 xvalue 转换为 JSON 字符串
XXAPI str xrtStringifyJSON(xvalue varVal, int bFormat, size_t* pRetSize);

// 将 xvalue 写入 JSON 文件
XXAPI int xrtStringifyJSON_File(str sFile, xvalue varVal, int bFormat);
```

**参数说明：**
| 参数 | 说明 |
|------|------|
| `sText` | JSON 字符串 |
| `iSize` | 字符串长度（0表示自动计算） |
| `sFile` | 文件路径 |
| `bFormat` | 是否格式化输出 |
| `pRetSize` | 输出：结果字符串长度 |

**示例：解析与序列化**
```c
// 解析 JSON 字符串
str jsonStr = "{\"name\":\"Tom\",\"age\":25}";
xvalue data = xrtParseJSON(jsonStr, 0);

// 访问数据
xvalue name = xvoTableGetValue(data, "name", 4);
printf("Name: %s\n", xvoGetText(name));

// 修改数据
xvoTableSetInt(data, "age", 3, 26);

// 序列化为 JSON
size_t len;
str result = xrtStringifyJSON(data, TRUE, &len);  // 格式化输出
printf("%s\n", result);
xrtFree(result);

xvoUnref(data);
```

**示例：文件操作**
```c
// 从文件加载
xvalue config = xrtParseJSON_File("config.json");

// 修改配置
xvoTableSetText(config, "version", 7, "2.0", 0, FALSE);

// 保存到文件
xrtStringifyJSON_File("config.json", config, TRUE);

xvoUnref(config);
```

### 创建 JSON 结构

```c
// 创建对象
xvalue user = xvoCreateTable();
xvoTableSetText(user, "name", 4, "Tom", 0, FALSE);
xvoTableSetInt(user, "age", 3, 25);
xvoTableSetBool(user, "active", 6, TRUE);

// 创建数组
xvalue tags = xvoCreateArray();
xvoArrayAddText(tags, "developer", 0, FALSE);
xvoArrayAddText(tags, "golang", 0, FALSE);
xvoTableSetValue(user, "tags", 4, tags, TRUE);

// 访问数据
xvalue name = xvoTableGetValue(user, "name", 4);
printf("Name: %s\n", xvoGetText(name));

xvalue age = xvoTableGetValue(user, "age", 3);
printf("Age: %lld\n", xvoGetInt(age));

xvoUnref(user);
```

### 嵌套结构

```c
// 创建嵌套对象
xvalue config = xvoCreateTable();

xvalue server = xvoCreateTable();
xvoTableSetText(server, "host", 4, "localhost", 0, FALSE);
xvoTableSetInt(server, "port", 4, 8080);
xvoTableSetValue(config, "server", 6, server, TRUE);

xvalue db = xvoCreateTable();
xvoTableSetText(db, "name", 4, "mydb", 0, FALSE);
xvoTableSetValue(config, "database", 8, db, TRUE);

// 访问嵌套数据
xvalue server_obj = xvoTableGetValue(config, "server", 6);
xvalue host = xvoTableGetValue(server_obj, "host", 4);
printf("Host: %s\n", xvoGetText(host));

xvoUnref(config);
```

### 数组操作

```c
xvalue users = xvoCreateArray();

// 添加对象到数组
for (int i = 0; i < 3; i++) {
    xvalue user = xvoCreateTable();
    xvoTableSetInt(user, "id", 2, i);
    xvoArrayAddValue(users, user);
}

// 遍历数组
uint32 count = xvoArrayItemCount(users);
for (uint32 i = 0; i < count; i++) {
    xvalue user = xvoArrayGetValue(users, i);
    xvalue id = xvoTableGetValue(user, "id", 2);
    printf("User ID: %lld\n", xvoGetInt(id));
}

xvoUnref(users);
```

---

## 相关文档

- **[Value 动态类型系统](api-value.md)** - 推荐使用，提供完整的 JSON 数据处理能力
- [JNUM 数值转换](api-jnum.md) - 数字解析
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#json-处理库)

</div>
