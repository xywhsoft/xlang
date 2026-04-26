# XRT - X Runtime Library

<div align="center">

<img src="logo/logo.png" alt="XRT Logo" width="480"/>

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-blue.svg)](https://github.com)
[![Language](https://img.shields.io/badge/language-C-brightgreen.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![API Lines](https://img.shields.io/badge/API-2320%20lines-orange.svg)](xrt.h)
[![Modules](https://img.shields.io/badge/modules-32-green.svg)](lib/)

**Lightweight, High-Performance, Feature-Complete C Runtime Library**

English | [简体中文](README.md)

**[📚 View Complete API Documentation](docs/README.en.md)**

</div>

---

## 📖 Project Introduction

**XRT** (X Runtime Library) is a feature-complete C runtime library that provides modern, efficient infrastructure support for C developers. The project includes **32 functional modules**, **2320 lines of API declarations**, **31 test modules**, and **33 API documents**, covering memory management, charset conversion, file handling, data structures, dynamic type systems, JSON processing, template engines, and more.

The library adopts a **single-header design** with zero external dependencies and supports cross-platform compilation, enabling C developers to develop as conveniently as with modern high-level languages.

### ✨ Core Features

- 🚀 **Zero Dependencies**: No external dependencies except the standard C library, no complex dependency configuration needed
- 📦 **Single-Header Architecture**: Core APIs are unified in `xrt.h` with 2320 complete declarations, ready to use upon inclusion
- 🔧 **32 Modular Sub-libraries**: Independent and decoupled functionality, selectable on demand, minimizing code size
- 🌍 **Full Platform Support**: Windows, Linux, macOS across x86/x64/ARM64 architectures
- 🔨 **Four Compiler Compatibility**: TCC (ultra-fast compilation), GCC, Clang, MSVC fully supported
- ⚡ **Ultimate Performance Optimization**: Multi-level memory pools, 26-bit reference counting, inline function optimization, binary tree indexing
- 🎯 **16 Dynamic Types**: Complete Value type system with automatic memory management and type conversion
- 💾 **Smart Memory Management**: 32-slot circular temporary memory, GC mark-sweep, multi-level memory pool architecture
- 📝 **Enterprise-Grade Template Engine**: Supports variable substitution, conditionals, loops, sub-template nesting, script extensions
- 🔐 **Production-Grade Stability**: 31 test modules with comprehensive coverage, battle-tested code quality

---

## 📈 Project Advantages

### 🏆 Why Choose XRT?

| Feature | Traditional C Libraries | XRT |
|---------|------------------------|-----|
| Dependency Management | Requires configuring multiple headers and link libraries | Single header `xrt.h` for all functionality |
| Memory Management | Manual malloc/free, prone to memory leaks | 26-bit reference counting auto-release + GC mark-sweep |
| Temporary Memory | Manual tracking and release required | 32-slot circular auto-release, no memory leak risk |
| Data Types | Basic types, lacking abstraction | 16 dynamic types, supporting Array/List/Dict/Coll |
| JSON Processing | Requires third-party libraries | Built-in SAX mode parsing/generation, efficient and low memory |
| Charset | Complex cross-platform handling | UTF-8/16/32 inter-conversion, automatic encoding detection |
| String Operations | Scattered functions, inconvenient to use | Unified API, supporting find/replace/split/format |
| Template Engine | No built-in support | Complete syntax: if/for/foreach/define/include |
| Cross-Platform | Requires extensive conditional compilation | Unified abstraction layer, write once run anywhere |
| Compilation Speed | Depends on compiler | Millisecond-level compilation with TCC support |

### 📊 Performance Advantages

- **Memory Pool Architecture**: Binary tree indexed Fixed-Size Blocks (FSB), allocation time complexity O(log n)
- **Efficient Hash Algorithms**: 32-bit uses nmhash32x, 64-bit uses rapidhash, BSD-2 license
- **AVL Balanced Trees**: Dictionary and collection use AVL tree implementation, O(log n) for find/insert/delete
- **Inline Function Optimization**: Critical paths provide `_Inline` versions, eliminating function call overhead
- **PCG Random Numbers**: Uses PCG algorithm for high-quality pseudo-random numbers, supports 32/64 bit
- **256-Element Memory Pages**: Memory management units use 256 elements/page design for fast allocation and release

---

## 🎯 Main Functional Modules

### 🔹 Infrastructure Layer (5 Modules)

| Module | Header | Description | Main APIs | Key Features |
|--------|--------|-------------|-----------|--------------|
| [**base**](docs/api-base.en.md) | `base.h` | Memory management, error handling | `xrtMalloc`, `xrtFree`, `xrtTempMemory` | 32-slot circular temporary memory auto-release |
| [**charset**](docs/api-charset.en.md) | `charset.h` | Charset conversion | `xrtUTF8to16`, `xrtUTF16to8`, `xrtConvCharset` | UTF-8/16/32 inter-conversion, BOM detection |
| [**hash**](docs/api-hash.en.md) | `hash.h` | Hash calculation | `xrtHash32`, `xrtHash64`, `xrtHash64_Nano` | nmhash32x + rapidhash high-performance algorithms |
| [**math**](docs/api-math.en.md) | `math.h` | Random number generation | `xrtRand32`, `xrtRand64`, `xrtRandRange` | PCG algorithm, seed setting support |
| [**time**](docs/api-time.en.md) | `time.h` | Time processing | `xrtNow`, `xrtDateSerial`, `xrtDateAdd` | Year/month/day/hour/minute/second/weekday calculations |

### 🔹 System Interaction Layer (5 Modules)

| Module | Header | Description | Main APIs | Key Features |
|--------|--------|-------------|-----------|--------------|
| [**os**](docs/api-os.en.md) | `os.h` | System operations | `xrtRun`, `xrtStart`, `xrtChain` | Process launch, file opening, chain calls |
| [**file**](docs/api-file.en.md) | `file.h` | File operations | `xrtOpen`, `xrtFileReadAll`, `xrtDirScan` | Auto charset conversion, recursive directory traversal |
| [**path**](docs/api-path.en.md) | `path.h` | Path handling | `xrtPathGetName`, `xrtPathGetDir`, `xrtPathJoin` | Cross-platform path handling, random path generation |
| [**network**](docs/api-network.en.md) | `network.h` | Network functions | `xrtGetLocalIP`, `xrtGetLocalMAC`, `xrtGetLocalName` | Get local network information |
| [**thread**](docs/api-thread.en.md) | `thread.h` | Thread management | `xrtThreadCreate` | Cross-platform thread creation |

### 🔹 String Processing Layer (3 Modules)

| Module | Header | Description | Main APIs | Key Features |
|--------|--------|-------------|-----------|--------------|
| [**string**](docs/api-string.en.md) | `string.h` | String operations | `xrtFindStr`, `xrtReplace`, `xrtSplit`, `xrtFormat` | Case-sensitive greedy/lazy search, Base64 encoding/decoding |
| [**jnum**](docs/api-jnum.en.md) | `jnum.h` | Number conversion | `xrtI64ToStr`, `xrtParseNum`, `xrtStrToNum` | High-performance number parsing, hexadecimal support |
| [**template**](docs/api-template.en.md) | `template.h` | Template engine | `xteParse`, `xteMake`, `xteLexer` | if/for/foreach/define/include/script complete syntax |

### 🔹 Data Structure Layer (9 Modules)

| Module | Header | Description | Data Structure Type | Key Features |
|--------|--------|-------------|---------------------|--------------|
| [**buffer**](docs/api-buffer.en.md) | `buffer.h` | Dynamic buffer | `xbuffer` | Auto expansion, 64KB step pre-allocation |
| [**array**](docs/api-array.en.md) | `array.h` | Struct array | `xarray` | 256-step expansion, sorting/swapping support |
| [**array_point**](docs/api-ptrarray.en.md) | `array_point.h` | Pointer array | `xparray` | Dedicated pointer storage, fast null filling |
| [**stack**](docs/api-stack.en.md) | `stack.h` | Static stack | `xstack` | Fixed stack depth, high-performance push/pop |
| [**stack_dyn**](docs/api-dynstack.en.md) | `stack_dyn.h` | Dynamic stack | `xdynstack` | 256-step auto expansion, no stack depth limit |
| [**llist**](docs/api-llist.en.md) | `llist.h` | Doubly linked list | `xllist` | Built-in memory pool, O(1) insert/delete |
| [**avltree**](docs/api-avltree.en.md) | `avltree.h` | AVL balanced tree | `xavltree` | Built-in node cache, parent-child tree association |
| [**dict**](docs/api-dict.en.md) | `dict.h` | Dictionary (string key-value pairs) | `xdict` | AVL tree + memory pool implementation, fast lookup |
| [**list**](docs/api-list.en.md) | `list.h` | List (integer index) | `xlist` | AVL tree implementation, sparse data storage support |

### 🔹 Memory Management Layer (4 Modules)

| Module | Header | Description | Use Cases | Key Features |
|--------|--------|-------------|-----------|--------------|
| [**bsmm**](docs/api-bsmm.en.md) | `bsmm.h` | Block structure memory management | Fixed-size struct allocation | 256 elements/page, release list reuse |
| [**memunit**](docs/api-memunit.en.md) | `memunit.h` | Memory unit management | 256-byte page management | GC mark bit support, fast batch reclamation |
| [**mempool_fs**](docs/api-mempool-fs.en.md) | `mempool_fs.h` | Fixed-size memory pool | High-frequency small object allocation | Free/full list grouping, smart allocation unit selection |
| [**mempool**](docs/api-mempool.en.md) | `mempool.h` | General memory pool | Multi-level memory block management | Binary tree indexed FSB, 15/31 level partitioning |

### 🔹 Advanced Features Layer (3 Modules)

| Module | Header | Description | Features | Details |
|--------|--------|-------------|----------|---------|
| [**value**](docs/api-value.en.md) | `value.h` | Dynamic type system | 16 data types, 26-bit reference counting | Empty/Null/Bool/Int/Float/Text/Time/Point/Func/Array/List/Coll/Table/Struct/Object/Custom |
| [**json**](docs/api-json.en.md) | `json.h` | JSON processing | SAX parsing/generation, no DOM overhead | Comments, trailing commas, hexadecimal, special floats support |
| [**xid**](docs/api-xid.en.md) | `xid.h` | Distributed ID | 192-bit unique ID | Timestamp + IP + CPU clock + random number combination |

---

## 📦 Core Systems Explained

### 🎨 Value Dynamic Type System

XRT provides a complete dynamic type system, allowing C to handle data as flexibly as scripting languages:

| Type | Description | Type | Description |
|------|-------------|------|-------------|
| **Empty** | Non-existent data | **Null** | Null value |
| **Bool** | Boolean (true/false) | **Int** | 64-bit integer |
| **Float** | Double precision float | **Text** | String |
| **Time** | Timestamp | **Point** | Pointer |
| **Func** | Function pointer | **Array** | Dynamic array |
| **List** | Integer-indexed list | **Coll** | Collection (deduplicated) |
| **Table** | String key-value dictionary | **Struct** | Struct |
| **Object** | Object | **Custom** | Custom type |

**Key Features**:
- 🔄 **26-bit Reference Counting**: Supports up to 67+ million references, auto-converts to static value on overflow
- 📦 **Set Operations**: Difference, intersection, union, symmetric difference operations
- 🔗 **Parent-Child Association**: List/Coll/Table support parent inheritance lookup
- 📋 **Deep/Shallow Copy**: `xvoCopy` shallow copy, `xvoDeepCopy` deep copy
- 📊 **Debug Output**: `xvoPrintValue` outputs complete data structure

### 📝 Template Engine Details

Enterprise-grade template engine with complete control flow and data binding:

| Syntax | Description | Example |
|--------|-------------|---------|
| `{$var}` | Variable substitution | `{$name}` → outputs variable value |
| `{%num}` | Number formatting | `{%price:¥%.2f}` → formats number |
| `{&time}` | Time formatting | `{&date:YYYY-MM-DD}` → formats time |
| `{?bool:a:b}` | Conditional expression | `{?active:Yes:No}` → selects based on condition |
| `{*arr:tpl}` | Array iteration | Applies sub-template to process array |
| `{@func:args}` | Function call | Calls external function for processing |
| `{=sub}` | Sub-template call | References predefined sub-template |
| `{!comment}` | Comment | Template comment, not output |

**Advanced Syntax**:
```
{#define:subtemplate_name}content{#end}        // Define sub-template
{#if:expression}content{#elseif}content{#else}content{#end}  // Conditionals
{#for:1:10:1}content{#end}               // Counting loop
{#foreach:variable_name}content{#end}    // Iteration loop
{#include:filename}                       // Include external file
{#script:language}code{#end}             // Embed script
```

### 📄 JSON Processing Features

**SAX Mode Parsing** (Event-driven, low memory footprint):
- Supports C-style single/multi-line comments (configurable)
- Supports trailing commas in arrays/objects
- Supports hexadecimal numbers (0x prefix)
- Supports special floats (nan, inf, -inf)
- Supports single-value JSON (non-object/array start)
- Supports escape character validity checking

**SAX Mode Generation** (Incremental writing, smart expansion):
- Supports formatted/compressed output
- Cross-platform newline handling
- Automatic comma and indentation management

---

## 🚀 Quick Start

### 📥 Installation

```bash
# Clone repository
git clone https://gitee.com/xywhsoft/xrt
cd xrt
```

### 🔨 Compilation

#### Windows Platform

```batch
# Use TCC to compile 64-bit test program
build_TCC_TEST_x64.bat

# Use GCC to compile 64-bit DLL
build_GCC_DLL_x64.bat
```

#### Linux/macOS Platform

```bash
# Compile test program
bash build_test.sh
```

### 📝 Usage Examples

#### Basic Usage

```c
#include "xrt.h"

int main() {
    // Initialize library
    xrtGlobalData* xCore = xrtInit();
    
    // Use string functionality
    str text = xrtReplace("Hello World", 0, "World", 0, "XRT", 0);
    printf("%s\n", text);  // Output: Hello XRT
    xrtFree(text);
    
    // Time processing
    xtime now = xrtNow();
    str timeStr = xrtTimeToStr(now, XRT_TIME_FORMAT_DATETIME);
    printf("Current time: %s\n", timeStr);
    xrtFree(timeStr);
    
    // Cleanup resources
    xrtUnit();
    return 0;
}
```

#### Dynamic Type System (Value)

```c
#include "xrt.h"

int main() {
    xrtInit();
    
    // Create array
    xvalue arr = xvoCreateArray();
    xvoArrayAppendInt(arr, 123);
    xvoArrayAppendText(arr, "Hello", 0, FALSE);
    xvoArrayAppendFloat(arr, 3.14);
    
    // Create table (dictionary)
    xvalue table = xvoCreateTable();
    xvoTableSetText(table, "name", 0, "XRT", 0, FALSE);
    xvoTableSetInt(table, "version", 0, 1);
    xvoTableSetBool(table, "active", 0, TRUE);
    
    // Get value
    str name = xvoTableGetText(table, "name", 0);
    printf("Name: %s\n", name);
    
    // Release resources (reference counting auto-managed)
    xvoUnref(arr);
    xvoUnref(table);
    xrtUnit();
    return 0;
}
```

#### JSON Processing

```c
#include "xrt.h"

int main() {
    xrtInit();
    
    // Parse JSON
    str jsonText = "{\"name\":\"xrt\",\"version\":1.0,\"features\":[\"fast\",\"simple\"]}";
    xvalue json = xrtParseJSON(jsonText, 0);
    
    // Read value
    str name = xvoTableGetText(json, "name", 0);
    printf("Project: %s\n", name);
    
    // Generate JSON (formatted output)
    xvalue newJson = xvoCreateTable();
    xvoTableSetText(newJson, "status", 0, "ok", 0, FALSE);
    xvoTableSetInt(newJson, "code", 0, 200);
    
    size_t len = 0;
    str output = xrtStringifyJSON(newJson, TRUE, &len);  // TRUE = formatted
    printf("%s\n", output);
    xrtFree(output);
    
    xvoUnref(json);
    xvoUnref(newJson);
    xrtUnit();
    return 0;
}
```

#### Template Engine

```c
#include "xrt.h"

int main() {
    xrtInit();
    
    // Create template variables
    xvalue vars = xvoCreateTable();
    xvoTableSetText(vars, "title", 0, "XRT Library", 0, FALSE);
    xvoTableSetInt(vars, "count", 0, 42);
    
    // Parse template
    str template = "Welcome to {$title}! Count: {%count}";
    XTE_LiteObject tpl = xteParse(template, 0, NULL);
    
    // Generate output
    size_t len = 0;
    str result = xteMake(tpl, vars, NULL, NULL, &len);
    printf("%s\n", result);  // Welcome to XRT Library! Count: 42
    xrtFree(result);
    
    xteParseFree(tpl);
    xvoUnref(vars);
    xrtUnit();
    return 0;
}
```

---

## 📁 Project Structure

```
xrt/
├── lib/                    # 32 functional sub-libraries (273KB+ source)
│   ├── base.h             # Basic memory management, error handling
│   ├── charset.h          # Charset conversion (UTF-8/16/32, 27.7KB)
│   ├── string.h           # String processing (33.3KB)
│   ├── jnum.h             # Number parsing (67.2KB)
│   ├── value.h            # Dynamic type system (41.8KB)
│   ├── json.h             # JSON SAX parsing/generation (60.6KB)
│   ├── template.h         # Template engine (45.8KB)
│   ├── hash.h             # High-performance hashing (41.4KB)
│   ├── file.h             # File operations (46.2KB)
│   ├── mempool.h          # General memory pool (16.0KB)
│   ├── dict.h             # Dictionary (AVL tree implementation)
│   ├── avltree.h          # AVL balanced tree
│   └── ...                # Other 20 modules
├── docs/                   # API documentation (33 files, 500KB+)
│   ├── api-base.md        # Base library documentation
│   ├── api-value.md       # Value type documentation (29.5KB)
│   ├── api-json.md        # JSON library documentation
│   ├── api-template.md    # Template engine documentation
│   ├── types.md           # Type definitions documentation
│   └── ...                # Module documentation
├── test/                   # Test files (31 modules)
│   ├── test_base.h
│   ├── test_value.h
│   ├── test_json.h
│   ├── test_template.h
│   └── ...
├── release/                # Compilation output directory
│   ├── x64/               # 64-bit compilation output
│   └── x86/               # 32-bit compilation output
├── xrt.h                  # Main header file (API declarations, 2320 lines, 89.3KB)
├── xrt.c                  # Main implementation file (includes all lib/*.h)
├── test.c                 # Test entry point
├── build_TCC_*.bat        # TCC build scripts (Windows)
├── build_GCC_*.bat        # GCC build scripts (Windows)
├── build*.sh              # Linux/macOS build scripts
├── LICENSE                # MIT open source license
├── README.md              # Chinese documentation
└── README.en.md           # English documentation
```

---

## 🔧 Build Options

### Supported Compilers

| Compiler | Features | Use Cases |
|----------|----------|-----------|
| **TCC** | Millisecond-level compilation speed | Rapid development debugging, script-like usage |
| **GCC** | Mature, stable, good optimization | Production environment, cross-platform compilation |
| **Clang** | LLVM backend, clear diagnostics | macOS/iOS development |
| **MSVC** | Native Windows support | Visual Studio integration |

### Supported Platforms

| Platform | Architectures | Status |
|----------|---------------|--------|
| **Windows** | x86, x64 | ✅ Fully Supported |
| **Linux** | x86, x64 | ✅ Fully Supported |
| **macOS** | x64, ARM64 | ✅ Fully Supported |

### Build Targets

| Target | Description | Output |
|--------|-------------|--------|
| **DLL** | Dynamic link library | `xrt.dll` / `libxrt.so` |
| **OBJ** | Static object file | `xrt.o` |
| **TEST** | Executable test program | `test.exe` / `test` |

---

## 📚 API Documentation

> 📖 **Complete Documentation**: Please refer to the [docs directory](docs/README.md) for detailed API usage documentation and examples.

### Memory Management

```c
ptr xrtMalloc(size_t iSize);                    // Allocate memory
ptr xrtCalloc(size_t iNum, size_t iSize);       // Allocate and zero
ptr xrtRealloc(ptr pMem, size_t iSize);         // Reallocate
void xrtFree(ptr pmem);                          // Free memory
ptr xrtTempMemory(size_t iSize);                // Temporary memory (32-slot circular auto-release)
```

### Charset Conversion

```c
u16str xrtUTF8to16(u8str sText, size_t iSize);   // UTF-8 to UTF-16
u8str xrtUTF16to8(u16str sText, size_t iSize);   // UTF-16 to UTF-8
u32str xrtUTF8to32(u8str sText, size_t iSize);   // UTF-8 to UTF-32
ptr xrtConvCharset(ptr sText, size_t iSize, int iInCP, int iOutCP);  // Any encoding conversion
bool xrtIsUTF8(str sText, size_t iSize);         // Check if UTF-8
int xrtDetectCharset(ptr sText, size_t iSize, bool bBOM);  // Auto-detect encoding
```

### String Operations

```c
str xrtCopyStr(str sText, size_t iSize);         // Copy string
str xrtFindStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
str xrtReplace(str sText, size_t iSize, str sSubText, size_t iSubSize, str sRepText, size_t iRepSize);
str* xrtSplit(str sText, size_t iSize, str sSepText, size_t iSepSize, bool bSrcRevise);
str xrtFormat(str sFormat, ...);                 // Format string
str xrtTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise);
str xrtBase64Encode(ptr pMem, size_t iSize, str sTable);  // Base64 encode
ptr xrtBase64Decode(str sText, size_t iSize, str sTable);  // Base64 decode
```

### File Operations

```c
xfile xrtOpen(str sPath, int bReadOnly, int iCharset);  // Open file
void xrtClose(xfile objFile);                    // Close file
str xrtFileReadAll(str sPath, int iCharset);     // Read all content
int xrtFileWriteAll(str sPath, str sText, size_t iSize, int iCharset);
bool xrtFileExists(str sPath);                   // Check if file exists
bool xrtDirExists(str sPath);                    // Check if directory exists
int xrtDirScan(str sPath, int bRecu, ptr pProc, ptr Param);  // Traverse directory
bool xrtDirCreateAll(str sPath);                 // Create multi-level directory
```

### Time Processing

```c
xtime xrtNow();                                   // Current date time
xtime xrtDateSerial(int64 iYear, int iMonth, int iDay);  // Build date
xtime xrtDateTimeSerial(int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond);
str xrtTimeToStr(xtime iTime, int iFormat);      // Time to string
xtime xrtDateAdd(int interval, int64 iValue, xtime iTime);  // Add/subtract time
int64 xrtDateDiff(int interval, xtime iTime1, xtime iTime2);  // Time difference
int64 xrtYear(xtime iTime);                      // Get year
int xrtMonth(xtime iTime);                       // Get month
int xrtDay(xtime iTime);                         // Get day
```

### Value Dynamic Types

```c
// Create values
xvalue xvoCreateNull();                          // Null value
xvalue xvoCreateBool(bool bVal);                 // Boolean
xvalue xvoCreateInt(int64 iVal);                 // Integer
xvalue xvoCreateFloat(double fVal);              // Float
xvalue xvoCreateText(ptr sVal, uint32 iSize, bool bColloc);  // String
xvalue xvoCreateTime(xtime tVal);                // Time
xvalue xvoCreateArray();                         // Array
xvalue xvoCreateList();                          // List
xvalue xvoCreateColl();                          // Collection
xvalue xvoCreateTable();                         // Table (dictionary)

// Reference counting
void xvoAddRef(xvalue pVal);                     // Increase reference
void xvoUnref(xvalue pVal);                      // Decrease reference (auto-release when zero)

// Array operations
xvoArrayAppendInt(arr, value);                   // Append integer
xvoArrayAppendText(arr, str, len, colloc);       // Append string
xvalue xvoArrayGetValue(xvalue pArr, uint32 index);  // Get value
uint32 xvoArrayItemCount(xvalue pArr);           // Get count

// Table operations
xvoTableSetInt(table, key, keylen, value);       // Set integer
xvoTableSetText(table, key, keylen, str, len, colloc);  // Set string
str xvoTableGetText(xvalue pTbl, str key, uint32 kl);  // Get string
```

### JSON Processing

```c
xvalue xrtParseJSON(str sText, size_t iSize);    // Parse JSON string
xvalue xrtParseJSON_File(str sFile);             // Parse JSON file
str xrtStringifyJSON(xvalue varVal, int bFormat, size_t* pRetSize);  // Generate JSON
int xrtStringifyJSON_File(str sFile, xvalue varVal, int bFormat);    // Save to file
```

---

## 🎤 Design Highlights

### 1. 💾 Circular Temporary Memory

32 temporary memory slots used in rotation, auto-released, eliminating memory leak risks:

```c
str temp1 = xrtTempMemory(1024);  // Uses slot 0
str temp2 = xrtTempMemory(2048);  // Uses slot 1
// ... cycles to slot 31, then auto-releases slot 0
// No manual free needed, ideal for function return values
```

### 2. 🔄 Reference Counting GC

Value type system uses 26-bit reference counting for automatic memory management (max 67,108,863 references):

```c
xvalue v = xvoCreateInt(123);  // RefCount = 1
xvoAddRef(v);                   // RefCount = 2
xvoUnref(v);                    // RefCount = 1
xvoUnref(v);                    // RefCount = 0, auto-released
// Auto-converts to static value when exceeding max references, preventing overflow
```

### 3. 🏭 Multi-Level Memory Pool Architecture

| Memory Pool | Function | Features |
|-------------|----------|----------|
| **BSMM** | Block structure memory management | 256 elements/page, release list reuse |
| **MemUnit** | 256-byte page management | GC mark-sweep support |
| **FSMemPool** | Fixed-size memory pool | Free/full list grouping |
| **MemPool** | General memory pool | Binary tree FSB index, 15/31 level partitioning |

### 4. 🎲 Distributed ID Generation

192-bit unique ID for distributed system identification:

```c
// Structure: Timestamp(64bit) + IP Address(32bit) + CPU Clock(32bit) + Random(64bit)
str xid = xrtMakeXIDS();     // Generate globally unique ID string
xid objXID = xrtMakeXID();   // Generate XID struct
bool same = xrtCompXID(xid1, xid2);  // Compare two XIDs
xrtFree(xid);
```

### 5. ✨ 16 Value Data Types

Value type uses compact 16-byte structure:

| Type | Description | Type | Description |
|------|-------------|------|-------------|
| Empty | Non-existent data | Null | Null value |
| Bool | Boolean | Int | 64-bit integer |
| Float | Double precision float | Text | String |
| Time | Timestamp | Point | Pointer |
| Func | Function pointer | Array | Array |
| List | List | Coll | Collection |
| Table | Table/Dictionary | Struct | Struct |
| Object | Object | Custom | Custom |

### 6. 🚀 High-Performance Hash Algorithms

```c
uint32 hash32 = xrtHash32(data, len);           // 32-bit nmhash32x
uint64 hash64 = xrtHash64(data, len);           // 64-bit rapidhash
uint64 nano = xrtHash64_Nano(data, len);        // Ultra-lightweight version
```

### 7. 📊 JSON SAX Mode

Event-driven SAX mode for parsing/generating JSON with high memory efficiency:

```c
// Parsing: Streaming read, parse and callback
xrtJsonParseSAX(jsonText, len, callback);

// Generation: Incremental writing, smart expansion
json_sax_print_hd hd = xrtJsonPrintStart(&choice);
xrtJsonPrintObject(hd, NULL, JSON_SAX_START);
xrtJsonPrintString(hd, &key, &value);
xrtJsonPrintObject(hd, NULL, JSON_SAX_FINISH);
str result = xrtJsonPrintFinish(hd, &len, NULL);
```

### 8. 📝 Comprehensive Charset Support

```c
// UTF-8/16/32 inter-conversion
u16str utf16 = xrtUTF8to16(utf8Text, 0);     // UTF-8 → UTF-16
u8str utf8 = xrtUTF16to8(utf16Text, 0);      // UTF-16 → UTF-8
u32str utf32 = xrtUTF8to32(utf8Text, 0);     // UTF-8 → UTF-32

// Auto-detect encoding
bool isUtf8 = xrtIsUTF8(text, len);          // Check if UTF-8
int charset = xrtDetectCharset(text, len, TRUE);  // Auto-detect encoding (including BOM)

// Any encoding conversion
ptr result = xrtConvCharset(text, len, XRT_CP_UTF8, XRT_CP_UTF16);
```

---

## 🧪 Testing

The project includes **31 test modules** with 100% coverage of all functionality:

| Test Category | Test Modules |
|---------------|--------------|
| Basic Functions | base, charset, os, math, string, path, time, file, thread, hash, network, xid |
| Data Structures | buffer, array_ptr, array_struct, bsmm, memunit, mempool_fs, mempool |
| Stack Structures | stack_ptr, stack, dynstack_ptr, dynstack |
| Trees/Dictionaries | llist, avltree, dict, list |
| Advanced Features | value, json, template |

```c
// Enable test modules in test.c
Test_Base(xCore);        // Basic functionality tests
Test_Charset(xCore);     // Charset tests
Test_String(xCore);      // String tests
Test_File(xCore);        // File tests
Test_Value_Basic(xCore); // Value type tests
Test_JSON(xCore);        // JSON tests
Test_Template(xCore);    // Template engine tests
Test_Dict(xCore);        // Dictionary tests
Test_MemPool(xCore);     // Memory pool tests
// ...
```

Running tests:

```batch
# Windows
build_TCC_TEST_x64.bat
cd release\x64
test.exe

# Linux/macOS
bash build_test.sh
./test
```

---

## 💡 Use Cases

XRT is suitable for the following development scenarios:

### 🛠️ Tool Development
- Command-line tools, system utilities
- File processing, batch conversion tools
- Configuration file parsers

### 🌐 Server-Side Development
- Lightweight web services
- JSON API services
- Template-driven content generation

### 💻 Embedded Development
- Resource-constrained environments
- Fine-grained memory control needed
- Cross-platform embedded systems

### 📚 Learning Purposes
- C language data structure learning
- Memory management mechanism research
- Compiler principles practice (template engine, JSON parsing)

---

## 📄 License

This project is open-sourced under the [MIT License](LICENSE).

```
Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction...
```

---

## 🤝 Contributing

Issues and Pull Requests are welcome!

1. Fork this repository
2. Create a feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Submit a Pull Request

---

## 📧 Contact

- **Author**: xLeaves [xywhsoft]
- **Email**: xywhsoft@qq.com
- **Project Homepage**: [Git](https://gitee.com/xywhsoft/xrt)

---

## 🌟 Star History

If this project helps you, please give it a ⭐ Star!

---

## 🙏 Acknowledgments

Thanks to the following open-source projects for their support:

| Project | Functionality | License |
|---------|---------------|---------|
| [LJSON](https://github.com/lengjingzju/json) | jnum and json functionality | MIT License |
| [nmhash32x](https://github.com/gzm55/hash-garage) | 32-bit high-performance hash algorithm | BSD-2-Clause License |
| [rapidhash](https://github.com/Nicoshev/rapidhash) | 64-bit high-performance hash algorithm | MIT License |
| [PCG](https://github.com/imneme/pcg-c) | High-quality pseudo-random number generator | Apache-2.0 / MIT License |

---

<div align="center">

**Made with ❤️ by xLeaves**

</div>
