# Template Engine Library

> XTE (X Template Engine) Lightweight Template Engine

[English](api-template.en.md) | [中文](api-template.md) | [Back to Index](README.en.md)

---

## 📑 Table of Contents

- [Template Syntax](#template-syntax)
- [Data Structures](#data-structures)
- [Lexical Analysis](#lexical-analysis)
- [Template Parsing](#template-parsing)
- [Template Generation](#template-generation)
- [Advanced Features](#advanced-features)

---

## Template Syntax

### Token Types

```c
// Maximum supported parameter count
#define XTE_PARAM_MAXCOUNT  6

// Basic Tokens
#define XTE_TK_TEXT         0       // Text content
#define XTE_TK_COMMEN       1       // Comment: {! * }
#define XTE_TK_VAR          0x100   // Variable substitution: {$ * : *}
#define XTE_TK_NUM          0x101   // Numeric variable substitution: {% * : *}
#define XTE_TK_TIME         0x102   // Time variable substitution: {& * : *}
#define XTE_TK_BOOL         0x103   // Boolean conditional substitution: {? * : * : *}
#define XTE_TK_ARR          0x104   // Array substitution: {* * : *}
#define XTE_TK_PROC         0x105   // Function or procedure substitution: {@ * : * ...}
#define XTE_TK_SUBTEMPLATE  0x106   // Sub-template substitution: {= * : *}
#define XTE_TK_SYMBOL       0xFFFF  // Predefined symbol: {# * : *}
#define XTE_MODE_BLOCK      0xFFFE  // Data block collection mode (ends with {#end})

// Token Extension Numbers
#define XTE_TK_INCLUDE      0x10000 // Include external file: {#include *}
#define XTE_TK_DEFINE       0x10001 // Define sub-template: {#define *}
#define XTE_TK_SCRIPT       0x10002 // Script block
#define XTE_TK_IF           0x20000 // Conditional statement: {#if *}
#define XTE_TK_ELSEIF       0x20001 // {#elseif *}
#define XTE_TK_ELSE         0x20002 // {#else}
#define XTE_TK_FOR          0x30000 // Loop statement: {#for *}
#define XTE_TK_FOREACH      0x30001 // Iteration loop statement: {#foreach *}
#define XTE_TK_END          0xFFFFFF // Statement end: {#end}
#define XTE_TK_USER         0x1000000 // User-defined extension starting number

// Identifier Types
#define XTE_IDTPE_DEFAULT   0       // Single statement
#define XTE_IDTPE_BLOCK     1       // Independent statement block (ends with {#end})
```

---

### Basic Syntax

#### Variable Substitution

```
Name: {$ name : Unknown}
Age: {% age : 0}
Time: {& createTime : YYYY-MM-DD}
```

**Description:**
- `{$ name : Unknown}` - String variable, default value "Unknown"
- `{% age : 0}` - Numeric variable, default value 0
- `{& time : format}` - Time variable, with specified format

---

#### Conditional Statements

```
{#if active}
  User is activated
{#elseif pending}
  Pending activation
{#else}
  Not activated
{#end}
```

---

#### Loops

```
{#foreach users}
  {$ name} - {$ email}
{#end}

{#for i 0 10}
  Item {% i}
{#end}
```

---

#### Built-in Variables

| Variable | Description | Available Scope |
|----------|-------------|-----------------|
| `__self` | Current substitution value | Inside sub-template |
| `__index` | Current loop index | Inside `for`, `foreach` loops |
| `__value` | Current iteration value (for string type) | Inside `foreach` loop |

**Example:**
```
{#define item}
  Current value: {$ __self}
{#end}

{#foreach items}
  Index: {% __index}, Value: {$ __value}
{#end}
```

---

#### Comments

```
{! This is a comment, will not output }
```

---

## Data Structures

### XTE_IdentInfo - Identifier Information

Used to define custom identifiers.

```c
typedef struct {
    char* Ident;              // Identifier
    uint32 TokenIndex;        // Corresponding Token number
    unsigned short Type;      // 0 = single statement, 1 = independent block (ends with {#end})
    unsigned short Size;      // Identifier length
    unsigned short MinParamCount; // Minimum parameter count
    unsigned short MaxParamCount; // Maximum parameter count
    uint32 Hash;              // Identifier hash value
} XTE_IdentInfo_Struct, *XTE_IdentInfo;
```

---

### XTE_TokenItem - Token Item

Represents a single parsed Token.

```c
typedef struct {
    uint32 Type;                          // Token definition number
    char* Text;                           // Associated text
    size_t Size;                          // Associated text length
    uint32 ParamCount;                    // Parameter count
    char* ParamText[XTE_PARAM_MAXCOUNT];  // Parameter text
    uint32 ParamSize[XTE_PARAM_MAXCOUNT]; // Parameter length
    XTE_IdentInfo IdentInfo;              // Identifier info struct pointer for identifier statements
    uint32 RefLine;                       // Statement line number in source file
    uint32 RefLinePos;                    // Statement position in source line
    uint32 RefPos;                        // Statement position in source file
    uint32 RefSize;                       // Statement length in source file
} XTE_TokenItem_Struct, *XTE_TokenItem;
```

---

### XTE_TokenList - Token List

Result structure returned by lexical analysis.

```c
typedef struct {
    int Success;              // Whether parsing succeeded
    int ErrorCode;            // Error code (0=success)
    const char* ErrorDesc;    // Error description
    uint32 ErrorLine;         // Error line number
    uint32 ErrorLinePos;      // Error line position
    uint32 ErrorPos;          // Error position
    uint32 ErrorRefLine;      // Error reference line
    uint32 ErrorRefLinePos;   // Error reference line position
    uint32 ErrorRefPos;       // Error reference position
    xarray_struct Tokens;     // Token list
} XTE_TokenList_Struct, *XTE_TokenList;
```

---

### XTE_LiteObject - Template Object

Compiled template object structure.

```c
typedef struct {
    int Success;              // Whether parsing succeeded
    int ErrorCode;            // Error code (0=success)
    const char* ErrorDesc;    // Error description
    uint32 ErrorLine;         // Error line number
    uint32 ErrorLinePos;      // Error line position
    uint32 ErrorPos;          // Error position
    uint32 ErrorRefLine;      // Error reference line
    uint32 ErrorRefLinePos;   // Error reference line position
    uint32 ErrorRefPos;       // Error reference position
    xarray Tokens;            // Token list
    xparray_struct Actions;   // Compiled action list
    xdict_struct SubTemplates;// Sub-template list (hash table)
} XTE_LiteStruct, *XTE_LiteObject;
```

---

## Lexical Analysis

### xteCreateIdentList

Create keyword list for custom extensions.

**Prototype:**
```c
XXAPI xarray xteCreateIdentList();
```

**Return Value:**
- Success: Keyword list
- Failure: `NULL`

---

### xteDestroyIdentList

Destroy keyword list.

**Prototype:**
```c
XXAPI void xteDestroyIdentList(xarray objList);
```

---

### xteAddIdentToList

Add a keyword to the list.

**Prototype:**
```c
XXAPI int xteAddIdentToList(
    xarray objList,
    char* sID,
    uint32 iSize,
    uint32 iIndex,
    uint32 iType,
    uint32 iMinParamCount,
    uint32 iMaxParamCount
);
```

**Parameters:**
- `objList` - Keyword list
- `sID` - Identifier string
- `iSize` - Identifier length (0 for auto-calculate)
- `iIndex` - Corresponding Token number
- `iType` - Identifier type (`XTE_IDTPE_DEFAULT` or `XTE_IDTPE_BLOCK`)
- `iMinParamCount` - Minimum parameter count
- `iMaxParamCount` - Maximum parameter count

**Return Value:**
- Success: Index of new item
- Failure: `0`

**Example:**
```c
xarray identList = xteCreateIdentList();
xteAddIdentToList(identList, "myTag", 0, XTE_TK_USER, XTE_IDTPE_DEFAULT, 1, 3);
xteAddIdentToList(identList, "myBlock", 0, XTE_TK_USER + 1, XTE_IDTPE_BLOCK, 0, 1);
// Use identList for lexical analysis...
xteDestroyIdentList(identList);
```

---

### xteLexer

Parse template file into Token list.

**Prototype:**
```c
XXAPI XTE_TokenList xteLexer(
    char* sText,
    size_t iSize,
    xarray objIdentList,
    char* sBracket
);
```

**Parameters:**
- `sText` - Template text
- `iSize` - Length (0 for auto-calculate)
- `objIdentList` - Identifier list (supports `{#xxx}` syntax, can pass `NULL`)
- `sBracket` - Bracket characters (e.g., `"{}"` or `NULL` for default)

**Return Value:**
- `XTE_TokenList` struct pointer

**Release:** ✅ Requires `xteLexerFree` to release

---

### xteLexerFree

Release Token list.

**Prototype:**
```c
XXAPI void xteLexerFree(XTE_TokenList arrToken);
```

---

### xteParseFromTokenList

Convert Token list to template object.

**Prototype:**
```c
XXAPI XTE_LiteObject xteParseFromTokenList(XTE_TokenList objToks);
```

**Parameters:**
- `objToks` - Token list (will be released after call)

**Return Value:**
- `XTE_LiteObject` template object pointer

**Note:** The passed `objToks` will be released by the function, cannot be used after call.

---

## Template Parsing

### xteParse

Parse template text

**Prototype:**
```c
XXAPI XTE_LiteObject xteParse(char* sText, size_t iSize, char* sBracket);
```

**Parameters:**
- `sText` - Template text
- `iSize` - Length (0 for auto)
- `sBracket` - Bracket characters (usually `"{}"` or `NULL` for default)

**Return Value:**
- Success: Template object pointer
- Failure: `NULL`

**Release:** ✅ Requires `xteParseFree` to release

**Example:**
```c
str template_text = 
    "Hello {$ name}!\n"
    "{#if vip}\n"
    "  You are a VIP user\n"
    "{#end}";

XTE_LiteObject tpl = xteParse(template_text, 0, NULL);
if (tpl) {
    if (!tpl->Success) {
        printf("Parse error: %s\n", tpl->ErrorDesc);
        printf("Line: %u, Pos: %u\n", tpl->ErrorLine, tpl->ErrorPos);
    }
    xteParseFree(tpl);
}
```

---

### xteParseFree

Release template object

**Prototype:**
```c
XXAPI void xteParseFree(XTE_LiteObject objLite);
```

---

## Template Generation

### xteMakeActions

Generate document from action list (low-level function).

**Prototype:**
```c
XXAPI char* xteMakeActions(
    xparray arrAction,
    XTE_LiteObject objTemplate,
    xvalue tblVal,
    xvalue tblRoot,
    xvalue tblENV,
    xdict tblInclude,
    size_t* pRetSize
);
```

**Parameters:**
- `arrAction` - Action list
- `objTemplate` - Template object
- `tblVal` - Current data table
- `tblRoot` - Root data table (for sub-template access to parent data)
- `tblENV` - Environment variable table (optional, pass `NULL`)
- `tblInclude` - Included template dictionary (optional, pass `NULL`)
- `pRetSize` - Output: generated text length

**Return Value:**
- Success: Generated text
- Failure: `NULL`

**Memory Release:** ✅ Requires `xrtFree` to release

---

### xteMake

Generate document from template

**Prototype:**
```c
XXAPI char* xteMake(
    XTE_LiteObject objTemplate,
    xvalue tblVal,
    xvalue tblENV,
    xdict tblInclude,
    size_t* pRetSize
);
```

**Parameters:**
- `objTemplate` - Template object
- `tblVal` - Data table (xvalue Table or Array type)
- `tblENV` - Environment variable table (optional, pass `NULL`)
- `tblInclude` - Included template dictionary (optional, pass `NULL`)
- `pRetSize` - Output: generated text length

**Return Value:**
- Success: Generated text
- Failure: `NULL`

**Memory Release:** ✅ Requires `xrtFree` to release

**Example:**
```c
// Prepare template
str template_text = 
    "User Information:\n"
    "Name: {$ name : Unknown}\n"
    "Age: {% age : 0}\n"
    "{#if vip}\n"
    "VIP Level: {% vipLevel}\n"
    "{#end}";

XTE_LiteObject tpl = xteParse(template_text, 0, NULL);
if (!tpl || !tpl->Success) {
    printf("Template parse error\n");
    return;
}

// Prepare data
xvalue data = xvoCreateTable();
xvoTableSet(data, "name", xvoCreateText("John", 0, FALSE));
xvoTableSet(data, "age", xvoCreateInt(28));
xvoTableSet(data, "vip", xvoCreateBool(TRUE));
xvoTableSet(data, "vipLevel", xvoCreateInt(5));

// Generate document
size_t size;
str result = xteMake(tpl, data, NULL, NULL, &size);

if (result) {
    printf("%s\n", result);
    xrtFree(result);
}

// Cleanup
xvoUnref(data);
xteParseFree(tpl);
```

**Output:**
```
User Information:
Name: John
Age: 28
VIP Level: 5
```

---

## Advanced Features

### 1. Looping Arrays

```c
str template_text = 
    "{#foreach users}\n"
    "  {$ name} - {$ email}\n"
    "{#end}";

XTE_LiteObject tpl = xteParse(template_text, 0, NULL);

// Prepare data
xvalue users = xvoCreateArray();

xvalue user1 = xvoCreateTable();
xvoTableSet(user1, "name", xvoCreateText("Alice", 0, FALSE));
xvoTableSet(user1, "email", xvoCreateText("alice@example.com", 0, FALSE));
xvoArrayAdd(users, user1);

xvalue user2 = xvoCreateTable();
xvoTableSet(user2, "name", xvoCreateText("Bob", 0, FALSE));
xvoTableSet(user2, "email", xvoCreateText("bob@example.com", 0, FALSE));
xvoArrayAdd(users, user2);

xvalue data = xvoCreateTable();
xvoTableSet(data, "users", users);

// Generate
size_t size;
str result = xteMake(tpl, data, NULL, NULL, &size);
printf("%s", result);

xrtFree(result);
xvoUnref(data);
xteParseFree(tpl);
```

**Output:**
```
  Alice - alice@example.com
  Bob - bob@example.com
```

---

### 2. Sub-templates

```c
str template_text = 
    "{#define userCard}\n"
    "  Name: {$ name}\n"
    "  Age: {% age}\n"
    "{#end}\n"
    "\n"
    "{#foreach users}\n"
    "  {= userCard}\n"
    "{#end}";

XTE_LiteObject tpl = xteParse(template_text, 0, NULL);

// Prepare data...
// Use {= userCard} to call sub-template
```

**Sub-template Special Variables:**
- Inside sub-template, use `{$ __self}` to access substitution value
- Sub-templates cannot be nested

---

### 3. Including External Templates

```c
// Main template
str main_template = 
    "{#include header}\n"
    "Main content\n"
    "{#include footer}";

// Prepare included templates
xdict includes = xdictCreate();

str header_tpl_text = "<header>Website Header</header>";
XTE_LiteObject header_tpl = xteParse(header_tpl_text, 0, NULL);
xdictSet(includes, "header", header_tpl);

str footer_tpl_text = "<footer>Website Footer</footer>";
XTE_LiteObject footer_tpl = xteParse(footer_tpl_text, 0, NULL);
xdictSet(includes, "footer", footer_tpl);

// Main template
XTE_LiteObject main_tpl = xteParse(main_template, 0, NULL);

// Generate (pass includes)
size_t size;
str result = xteMake(main_tpl, NULL, NULL, includes, &size);

printf("%s\n", result);

// Cleanup
xrtFree(result);
xteParseFree(main_tpl);
xteParseFree(header_tpl);
xteParseFree(footer_tpl);
xdictFree(includes);
```

---

## Use Cases

### 1. HTML Generation

```c
str html_template = 
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head><title>{$ title}</title></head>\n"
    "<body>\n"
    "  <h1>{$ title}</h1>\n"
    "  <ul>\n"
    "  {#foreach items}\n"
    "    <li>{$ name}: {$ value}</li>\n"
    "  {#end}\n"
    "  </ul>\n"
    "</body>\n"
    "</html>";

// Parse and generate...
```

---

### 2. Email Template

```c
str email_template = 
    "Dear {$ name},\n"
    "\n"
    "Your order {$ orderId} has been {$ status}.\n"
    "\n"
    "{#if hasTracking}\n"
    "Tracking number: {$ trackingNumber}\n"
    "{#end}\n"
    "\n"
    "Thank you for your purchase!";

// Use...
```

---

### 3. Code Generation

```c
str code_template = 
    "class {$ className} {\n"
    "{#foreach fields}\n"
    "    private {% type} {$ name};\n"
    "{#end}\n"
    "\n"
    "{#foreach fields}\n"
    "    public {% type} get{$ Name}() {\n"
    "        return this.{$ name};\n"
    "    }\n"
    "{#end}\n"
    "}";

// Generate class code...
```

---

### 4. Report Generation

```c
str report_template = 
    "Sales Report - {& date : YYYY-MM-DD}\n"
    "=====================================\n"
    "{#foreach items}\n"
    "{$ product}: {% quantity} units, Amount {% amount}\n"
    "{#end}\n"
    "-------------------------------------\n"
    "Total: {% total}";

// Generate report...
```

---

## Best Practices

### 1. Error Handling

```c
XTE_LiteObject tpl = xteParse(template_text, 0, NULL);
if (!tpl) {
    fprintf(stderr, "Template allocation failed\n");
    return;
}

if (!tpl->Success) {
    fprintf(stderr, "Template parse error:\n");
    fprintf(stderr, "  Error: %s\n", tpl->ErrorDesc);
    fprintf(stderr, "  Line: %u, Pos: %u\n", 
        tpl->ErrorLine, tpl->ErrorPos);
    xteParseFree(tpl);
    return;
}

// Use template...
xteParseFree(tpl);
```

---

### 2. Template Caching

```c
typedef struct {
    xdict templates;
} TemplateCache;

TemplateCache* CreateCache() {
    TemplateCache* cache = xrtMalloc(sizeof(TemplateCache));
    cache->templates = xdictCreate();
    return cache;
}

XTE_LiteObject GetTemplate(TemplateCache* cache, str name, str text) {
    XTE_LiteObject tpl = xdictGet(cache->templates, name);
    if (!tpl) {
        tpl = xteParse(text, 0, NULL);
        if (tpl && tpl->Success) {
            xdictSet(cache->templates, name, tpl);
        }
    }
    return tpl;
}
```

---

### 3. Data Preparation

```c
xvalue PrepareUserData(User* user) {
    xvalue data = xvoCreateTable();
    xvoTableSet(data, "id", xvoCreateInt(user->id));
    xvoTableSet(data, "name", xvoCreateText(user->name, 0, FALSE));
    xvoTableSet(data, "email", xvoCreateText(user->email, 0, FALSE));
    xvoTableSet(data, "vip", xvoCreateBool(user->is_vip));
    return data;
}

str RenderUserCard(User* user) {
    xvalue data = PrepareUserData(user);
    
    XTE_LiteObject tpl = GetCachedTemplate("user_card");
    size_t size;
    str result = xteMake(tpl, data, NULL, NULL, &size);
    
    xvoUnref(data);
    return result;
}
```

---

## Performance Tips

### 1. Reuse Template Objects

```c
// ❌ Inefficient: Parse every time
for (int i = 0; i < 1000; i++) {
    XTE_LiteObject tpl = xteParse(template_text, 0, NULL);
    str result = xteMake(tpl, data[i], NULL, NULL, &size);
    xteParseFree(tpl);
    xrtFree(result);
}

// ✅ Efficient: Parse once, reuse multiple times
XTE_LiteObject tpl = xteParse(template_text, 0, NULL);
for (int i = 0; i < 1000; i++) {
    str result = xteMake(tpl, data[i], NULL, NULL, &size);
    ProcessResult(result);
    xrtFree(result);
}
xteParseFree(tpl);
```

---

### 2. Pre-compile Templates

```c
// Pre-compile at startup
void InitTemplates() {
    g_user_template = xteParse(LoadFile("user.tpl"), 0, NULL);
    g_order_template = xteParse(LoadFile("order.tpl"), 0, NULL);
    g_report_template = xteParse(LoadFile("report.tpl"), 0, NULL);
}

// Generate directly when using
str RenderUser(xvalue data) {
    size_t size;
    return xteMake(g_user_template, data, NULL, NULL, &size);
}
```

---

## Common Errors

### 1. Forgetting to Release

```c
// ❌ Memory leak
XTE_LiteObject tpl = xteParse(text, 0, NULL);
str result = xteMake(tpl, data, NULL, NULL, &size);
// Forgot xteParseFree(tpl) and xrtFree(result)

// ✅ Correct
XTE_LiteObject tpl = xteParse(text, 0, NULL);
str result = xteMake(tpl, data, NULL, NULL, &size);
UseResult(result);
xrtFree(result);
xteParseFree(tpl);
```

---

### 2. Syntax Errors

```c
// ❌ Forgot {#end}
str bad_template = 
    "{#if condition}\n"
    "  Content\n";
    // Missing {#end}

// ✅ Correct
str good_template = 
    "{#if condition}\n"
    "  Content\n"
    "{#end}";
```

---

## Error Codes

| Error Code | Description | Explanation |
|------------|-------------|-------------|
| 0 | success | Success |
| 1 | malloc failed | Memory allocation failed |
| 2 | token list append failed | Token list append failed |
| 3 | unrecognized symbols | Unrecognized symbols |
| 4 | empty symbols are not allowed | Empty symbols are not allowed |
| 5 | too many parameters | Too many parameters (max 6) |
| 6 | statement not ended | Statement not ended (missing `{#end}`) |
| 7 | Undefined identifier | Undefined identifier |
| 8 | Missing parameters | Missing parameters |
| 9 | Nesting of define statements is not allowed | Define statements cannot be nested |
| 10 | syntax error | Syntax error |

---

## Escape Characters

| Symbol | Description | Escape Method | Example |
|--------|-------------|---------------|---------|
| `{` | Template start symbol | `{{` | `{{` → `{` |
| `}` | Template end symbol | `\}` | `\}` → `}` |
| `:` | Parameter separator | `\:` | `\:` → `:` |
| `\` | Escape character in statement | `\\` | `\\` → `\` |

**Note:** Escape characters only take effect inside template statements; these characters in plain text do not need escaping.

---

## Related Documents

- [Value Dynamic Type System](api-value.en.md) - Template data model
- [String Processing](api-string.en.md) - String operations
- [Dict Dictionary](api-dict.en.md) - Include template management
- [File Operations](api-file.en.md) - Reading template files
- [Main Index](README.en.md)

---

<div align="center">

[⬆️ Back to Top](#template-engine-library)

</div>
