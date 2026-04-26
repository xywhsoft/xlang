# JSON Processing Library

> JSON SAX Parsing and Generation + Value System

[English](api-json.en.md) | [中文](api-json.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Important Notes](#important-notes)
- [JSON Type System](#json-type-system)
- [SAX Parsing](#sax-parsing)
- [JSON Generation](#json-generation)
- [Recommended: Using Value System](#recommended-using-value-system)

---

## Important Notes

**The JSON library in xrt.h provides two usage methods:**

1. **SAX API (Low-level):** Stream parsing, suitable for processing large files or performance-critical scenarios
2. **Value System (Recommended):** Provides JSON-like data structures and simple API, **strongly recommended for daily use**

---

## JSON Type System

### JSON Type Enumeration

```c
typedef enum {
    JSON_NULL = 0,      // null, no value
    JSON_BOOL,          // true/false, value in vbool
    JSON_INT,           // int32, value in vint
    JSON_HEX,           // uint32 hexadecimal, value in vhex
    JSON_LINT,          // int64, value in vlint
    JSON_LHEX,          // uint64 hexadecimal, value in vlhex
    JSON_DOUBLE,        // double, value in vdbl
    JSON_STRING,        // string, value in vstr
    JSON_ARRAY,         // array, child nodes in head
    JSON_OBJECT         // object, child nodes in head
} json_type_t;
```

### JSON String Info

```c
// String info structure
typedef struct {
    uint32_t type:4;      // JSON object type (valid when used as key)
    uint32_t escaped:1;   // Contains escape characters
    uint32_t alloced:1;   // Allocated on heap (SAX interface)
    uint32_t reserved:2;  // Reserved bits
    uint32_t len:24;      // String length
} json_strinfo_t;

// String with info
typedef struct {
    char *str;            // String data
    json_strinfo_t info;  // String info
} json_string_t;
```

### JSON Number Type

```c
// Number type value (supports long integers and hexadecimal)
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

## SAX Parsing

### Core Types

```c
// SAX command (indicates start and end of array/object)
typedef enum {
    JSON_SAX_START = 0,   // Left bracket [ or {
    JSON_SAX_FINISH       // Right bracket ] or }
} json_sax_cmd_t;

// SAX parse value
typedef union {
    json_number_t vnum;   // Number type value
    json_string_t vstr;   // String type value
    json_sax_cmd_t vcmd;  // Collection object start/end
} json_sax_value_t;

// SAX parser state
typedef struct {
    int total;              // Array depth size
    int index;              // Current index
    json_string_t *array;   // Stores type and key depth info
    json_sax_value_t value; // Current value
} json_sax_parser_t;

// Callback return value
typedef enum {
    JSON_SAX_PARSE_CONTINUE = 0,  // Continue parsing
    JSON_SAX_PARSE_STOP           // Stop parsing
} json_sax_ret_t;

// Callback function type
typedef json_sax_ret_t (*json_sax_cb_t)(json_sax_parser_t *parser);
```

### xrtJsonParseSAX

SAX-style JSON parsing (stream processing)

```c
XXAPI int xrtJsonParseSAX(str text, size_t str_len, json_sax_cb_t cb);
```

**Parameters:**
| Parameter | Description |
|-----------|-------------|
| `text` | JSON string |
| `str_len` | String length |
| `cb` | SAX callback function |

**Return Value:** Returns 0 on success, -1 on failure

**Example:**
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

## JSON Generation

### Print Configuration

```c
// Print buffer
typedef struct {
    size_t size;      // Buffer size
    char *p;          // Buffer pointer
} json_print_ptr_t;

// Print parameter settings
typedef struct {
    size_t str_len;       // Estimated string length
    size_t plus_size;     // Expansion step (default 8192)
    size_t item_size;     // Single item estimated size
    int item_total;       // Estimated item count (default 1024)
    bool format_flag;     // Whether to format output
    json_print_ptr_t *ptr; // Optional: reuse buffer
} json_print_choice_t;
```

### xrtJsonPrintStart

Start SAX printer

```c
XXAPI json_sax_print_hd xrtJsonPrintStart(json_print_choice_t *choice);
```

**Return Value:** Returns handle on success, NULL on failure

### xrtJsonPrintValue

Print JSON value

```c
XXAPI int xrtJsonPrintValue(json_sax_print_hd handle, json_type_t type, json_string_t *jkey, const void *value);
```

**Convenience Functions:**
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

**Helper Macros:**
```c
// Array operations
xrtJsonPrintArrayStart(handle, jkey)   // Start array
xrtJsonPrintArrayFinish(handle)        // End array

// Object operations
xrtJsonPrintObjectStart(handle, jkey)  // Start object
xrtJsonPrintObjectFinish(handle)       // End object
```

### xrtJsonPrintFinish

Finish printing and get result

```c
XXAPI char* xrtJsonPrintFinish(json_sax_print_hd handle, size_t *length, json_print_ptr_t *ptr);
```

**Return Value:** JSON string (requires `xrtFree` to release)

---

## Recommended: Using Value System

**Strongly recommended to use [Value Dynamic Type System](api-value.en.md) for JSON data processing!**

### JSON Parsing and Serialization

```c
// Parse JSON from string
XXAPI xvalue xrtParseJSON(str sText, size_t iSize);

// Parse JSON from file
XXAPI xvalue xrtParseJSON_File(str sFile);

// Convert xvalue to JSON string
XXAPI str xrtStringifyJSON(xvalue varVal, int bFormat, size_t* pRetSize);

// Write xvalue to JSON file
XXAPI int xrtStringifyJSON_File(str sFile, xvalue varVal, int bFormat);
```

**Parameter Description:**
| Parameter | Description |
|-----------|-------------|
| `sText` | JSON string |
| `iSize` | String length (0 for auto-calculate) |
| `sFile` | File path |
| `bFormat` | Whether to format output |
| `pRetSize` | Output: result string length |

**Example: Parsing and Serialization**
```c
// Parse JSON string
str jsonStr = "{\"name\":\"Tom\",\"age\":25}";
xvalue data = xrtParseJSON(jsonStr, 0);

// Access data
xvalue name = xvoTableGetValue(data, "name", 4);
printf("Name: %s\n", xvoGetText(name));

// Modify data
xvoTableSetInt(data, "age", 3, 26);

// Serialize to JSON
size_t len;
str result = xrtStringifyJSON(data, TRUE, &len);  // Formatted output
printf("%s\n", result);
xrtFree(result);

xvoUnref(data);
```

**Example: File Operations**
```c
// Load from file
xvalue config = xrtParseJSON_File("config.json");

// Modify configuration
xvoTableSetText(config, "version", 7, "2.0", 0, FALSE);

// Save to file
xrtStringifyJSON_File("config.json", config, TRUE);

xvoUnref(config);
```

### Creating JSON Structures

```c
// Create object
xvalue user = xvoCreateTable();
xvoTableSetText(user, "name", 4, "Tom", 0, FALSE);
xvoTableSetInt(user, "age", 3, 25);
xvoTableSetBool(user, "active", 6, TRUE);

// Create array
xvalue tags = xvoCreateArray();
xvoArrayAddText(tags, "developer", 0, FALSE);
xvoArrayAddText(tags, "golang", 0, FALSE);
xvoTableSetValue(user, "tags", 4, tags, TRUE);

// Access data
xvalue name = xvoTableGetValue(user, "name", 4);
printf("Name: %s\n", xvoGetText(name));

xvalue age = xvoTableGetValue(user, "age", 3);
printf("Age: %lld\n", xvoGetInt(age));

xvoUnref(user);
```

### Nested Structures

```c
// Create nested object
xvalue config = xvoCreateTable();

xvalue server = xvoCreateTable();
xvoTableSetText(server, "host", 4, "localhost", 0, FALSE);
xvoTableSetInt(server, "port", 4, 8080);
xvoTableSetValue(config, "server", 6, server, TRUE);

xvalue db = xvoCreateTable();
xvoTableSetText(db, "name", 4, "mydb", 0, FALSE);
xvoTableSetValue(config, "database", 8, db, TRUE);

// Access nested data
xvalue server_obj = xvoTableGetValue(config, "server", 6);
xvalue host = xvoTableGetValue(server_obj, "host", 4);
printf("Host: %s\n", xvoGetText(host));

xvoUnref(config);
```

### Array Operations

```c
xvalue users = xvoCreateArray();

// Add objects to array
for (int i = 0; i < 3; i++) {
    xvalue user = xvoCreateTable();
    xvoTableSetInt(user, "id", 2, i);
    xvoArrayAddValue(users, user);
}

// Iterate array
uint32 count = xvoArrayItemCount(users);
for (uint32 i = 0; i < count; i++) {
    xvalue user = xvoArrayGetValue(users, i);
    xvalue id = xvoTableGetValue(user, "id", 2);
    printf("User ID: %lld\n", xvoGetInt(id));
}

xvoUnref(users);
```

---

## Related Documentation

- **[Value Dynamic Type System](api-value.en.md)** - Recommended, provides complete JSON data processing capabilities
- [JNUM Number Conversion](api-jnum.en.md) - Number parsing
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#json-processing-library)

</div>
