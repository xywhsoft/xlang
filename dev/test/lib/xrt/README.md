# XRT - X Runtime Library

<div align="center">

<img src="logo/logo.png" alt="XRT Logo" width="480"/>

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-blue.svg)](https://github.com)
[![Language](https://img.shields.io/badge/language-C-brightgreen.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![API Lines](https://img.shields.io/badge/API-2320%20lines-orange.svg)](xrt.h)
[![Modules](https://img.shields.io/badge/modules-32-green.svg)](lib/)

**轻量级、高性能、功能完备的 C 语言运行时库**

[English](README.en.md) | 简体中文

**[📚 查看完整 API 文档](docs/README.md)**

</div>

---

## 📖 项目简介

**xrt**（X Runtime Library）是一个功能完备的 C 语言运行时库，为 C 语言开发者提供现代化、高效的基础设施支持。项目包含 **32 个功能模块**、**2320 行 API 声明**、**31 个测试模块**、**33 份 API 文档**，涵盖内存管理、字符集转换、文件处理、数据结构、动态类型系统、JSON 处理、模板引擎等完整功能链。

该库采用**单头文件设计**，零外部依赖，支持跨平台编译，让 C 语言开发者能够像使用现代高级语言一样便捷地进行开发。

### ✨ 核心特性

- 🚀 **零依赖设计**：除标准 C 库外无任何外部依赖，无需配置复杂的依赖环境
- 📦 **单头文件架构**：核心 API 统一定义在 `xrt.h` 中，2320 行完整声明，引入即用
- 🔧 **32 模块化子库**：功能独立解耦，可按需选择使用，最小化代码体积
- 🌍 **全平台支持**：Windows、Linux、macOS 三大平台，x86/x64/ARM64 多架构
- 🔨 **四大编译器兼容**：TCC（极速编译）、GCC、Clang、MSVC 全面支持
- ⚡ **极致性能优化**：多级内存池、26位引用计数、inline 函数优化、二叉树索引
- 🎯 **16 种动态类型**：完整的 Value 类型系统，支持自动内存管理和类型转换
- 💾 **智能内存管理**：32 槽位环形临时内存、GC 标记回收、多层次内存池架构
- 📝 **企业级模板引擎**：支持变量替换、条件判断、循环迭代、子模板嵌套、脚本扩展
- 🔐 **生产级稳定性**：31 个测试模块全面覆盖，久经验证的代码质量

---

## 📈 项目优势

### 🏆 为什么选择 XRT？

| 特性 | 传统 C 库 | XRT |
|------|---------|-----|
| 依赖管理 | 需要配置多个头文件和链接库 | 单头文件 `xrt.h` 即可使用全部功能 |
| 内存管理 | 手动 malloc/free，易产生内存泄漏 | 26位引用计数自动释放 + GC 标记回收 |
| 临时内存 | 需要手动跟踪和释放 | 32槽位环形自动释放，无内存泄漏风险 |
| 数据类型 | 基础类型，缺乏抽象 | 16 种动态类型，支持 Array/List/Dict/Coll |
| JSON 处理 | 需引入第三方库 | 内置 SAX 模式解析/生成，高效低内存 |
| 字符集 | 跨平台处理复杂 | UTF-8/16/32 互转，自动检测编码 |
| 字符串操作 | 函数分散，使用不便 | 统一 API，支持查找/替换/分割/格式化 |
| 模板引擎 | 无内置支持 | 完整语法：if/for/foreach/define/include |
| 跨平台 | 需要大量条件编译 | 统一抽象层，代码一次编写多平台运行 |
| 编译速度 | 依赖编译器 | TCC 支持下毫秒级编译 |

### 📊 性能优势

- **内存池架构**：采用二叉树索引的固定大小内存块（FSB），分配时间复杂度 O(log n)
- **高效哈希算法**：32位使用 nmhash32x，64位使用 rapidhash，BSD-2 协议
- **AVL 平衡树**：字典和集合采用 AVL 树实现，查找/插入/删除均为 O(log n)
- **内联函数优化**：关键路径提供 `_Inline` 版本，消除函数调用开销
- **PCG 随机数**：使用 PCG 算法生成高质量伪随机数，支持 32/64 位
- **256 元素内存页**：内存管理单元采用 256 元素/页设计，快速分配和释放

---

## 🎯 主要功能模块

### 🔹 基础设施层（5 个模块）

| 模块 | 头文件 | 功能描述 | 主要 API | 特色功能 |
|------|--------|---------|----------|----------|
| [**base**](docs/api-base.md) | `base.h` | 内存管理、错误处理 | `xrtMalloc`, `xrtFree`, `xrtTempMemory` | 32槽位环形临时内存自动释放 |
| [**charset**](docs/api-charset.md) | `charset.h` | 字符集转换 | `xrtUTF8to16`, `xrtUTF16to8`, `xrtConvCharset` | UTF-8/16/32 互转，支持 BOM 检测 |
| [**hash**](docs/api-hash.md) | `hash.h` | 哈希计算 | `xrtHash32`, `xrtHash64`, `xrtHash64_Nano` | nmhash32x + rapidhash 高性能算法 |
| [**math**](docs/api-math.md) | `math.h` | 随机数生成 | `xrtRand32`, `xrtRand64`, `xrtRandRange` | PCG 算法，支持种子设置 |
| [**time**](docs/api-time.md) | `time.h` | 时间处理 | `xrtNow`, `xrtDateSerial`, `xrtDateAdd` | 支持年/月/日/时/分/秒/星期计算 |

### 🔹 系统交互层（5 个模块）

| 模块 | 头文件 | 功能描述 | 主要 API | 特色功能 |
|------|--------|---------|----------|----------|
| [**os**](docs/api-os.md) | `os.h` | 系统操作 | `xrtRun`, `xrtStart`, `xrtChain` | 进程启动、文件打开、链式调用 |
| [**file**](docs/api-file.md) | `file.h` | 文件操作 | `xrtOpen`, `xrtFileReadAll`, `xrtDirScan` | 支持字符集自动转换，递归目录遍历 |
| [**path**](docs/api-path.md) | `path.h` | 路径处理 | `xrtPathGetName`, `xrtPathGetDir`, `xrtPathJoin` | 跨平台路径处理，支持随机路径生成 |
| [**network**](docs/api-network.md) | `network.h` | 网络功能 | `xrtGetLocalIP`, `xrtGetLocalMAC`, `xrtGetLocalName` | 获取本机网络信息 |
| [**thread**](docs/api-thread.md) | `thread.h` | 线程管理 | `xrtThreadCreate` | 跨平台线程创建 |

### 🔹 字符串处理层（3 个模块）

| 模块 | 头文件 | 功能描述 | 主要 API | 特色功能 |
|------|--------|---------|----------|----------|
| [**string**](docs/api-string.md) | `string.h` | 字符串操作 | `xrtFindStr`, `xrtReplace`, `xrtSplit`, `xrtFormat` | 支持大小写贪婪/惰性查找、Base64 编解码 |
| [**jnum**](docs/api-jnum.md) | `jnum.h` | 数字转换 | `xrtI64ToStr`, `xrtParseNum`, `xrtStrToNum` | 高性能数字解析，支持十六进制 |
| [**template**](docs/api-template.md) | `template.h` | 模板引擎 | `xteParse`, `xteMake`, `xteLexer` | if/for/foreach/define/include/script 完整语法 |

### 🔹 数据结构层（9 个模块）

| 模块 | 头文件 | 功能描述 | 数据结构类型 | 特色功能 |
|------|--------|---------|-------------|----------|
| [**buffer**](docs/api-buffer.md) | `buffer.h` | 动态缓冲区 | `xbuffer` | 自动扩容，支持 64KB 步进预分配 |
| [**array**](docs/api-array.md) | `array.h` | 结构体数组 | `xarray` | 256 步进扩容，支持排序/交换 |
| [**array_point**](docs/api-ptrarray.md) | `array_point.h` | 指针数组 | `xparray` | 专用指针存储，快速空雙填充 |
| [**stack**](docs/api-stack.md) | `stack.h` | 静态栈 | `xstack` | 固定栈深度，高性能压栈/出栈 |
| [**stack_dyn**](docs/api-dynstack.md) | `stack_dyn.h` | 动态栈 | `xdynstack` | 256 步进自动扩容，无栈深度限制 |
| [**llist**](docs/api-llist.md) | `llist.h` | 双向链表 | `xllist` | 内置内存池，O(1) 插入/删除 |
| [**avltree**](docs/api-avltree.md) | `avltree.h` | AVL 平衡树 | `xavltree` | 内置节点缓存，支持父子树关联 |
| [**dict**](docs/api-dict.md) | `dict.h` | 字典（字符串键值对） | `xdict` | AVL 树 + 内存池实现，快速查找 |
| [**list**](docs/api-list.md) | `list.h` | 列表（整数索引） | `xlist` | AVL 树实现，支持稀疏数据存储 |

### 🔹 内存管理层（4 个模块）

| 模块 | 头文件 | 功能描述 | 适用场景 | 特色功能 |
|------|--------|---------|----------|----------|
| [**bsmm**](docs/api-bsmm.md) | `bsmm.h` | 块结构内存管理 | 固定大小结构体分配 | 256 元素/页，释放链表复用 |
| [**memunit**](docs/api-memunit.md) | `memunit.h` | 内存单元管理 | 256 字节页管理 | 支持 GC 标记位，快速批量回收 |
| [**mempool_fs**](docs/api-mempool-fs.md) | `mempool_fs.h` | 固定大小内存池 | 高频小对象分配 | 空闲/满载链表分组，智能选择分配单元 |
| [**mempool**](docs/api-mempool.md) | `mempool.h` | 通用内存池 | 多级内存块管理 | 二叉树索引 FSB，支持 15/31 级分块 |

### 🔹 高级功能层（3 个模块）

| 模块 | 头文件 | 功能描述 | 特性 | 详细说明 |
|------|--------|---------|------|----------|
| [**value**](docs/api-value.md) | `value.h` | 动态类型系统 | 16 种数据类型，26位引用计数 | Empty/Null/Bool/Int/Float/Text/Time/Point/Func/Array/List/Coll/Table/Struct/Object/Custom |
| [**json**](docs/api-json.md) | `json.h` | JSON 处理 | SAX 解析/生成，无 DOM 开销 | 支持注释、尾逗号、十六进制、特殊浮点数 |
| [**xid**](docs/api-xid.md) | `xid.h` | 分布式 ID | 192 位唯一 ID | 时间戳 + IP + CPU时钟 + 随机数组合 |

---

## 📦 核心系统详解

### 🎨 Value 动态类型系统

XRT 提供了一套完整的动态类型系统，让 C 语言也能像脚本语言一样灵活处理数据：

| 类型 | 说明 | 类型 | 说明 |
|------|------|------|------|
| **Empty** | 不存在的数据 | **Null** | 空值 |
| **Bool** | 布尔值 (true/false) | **Int** | 64位整数 |
| **Float** | 双精度浮点数 | **Text** | 字符串 |
| **Time** | 时间戳 | **Point** | 指针 |
| **Func** | 函数指针 | **Array** | 动态数组 |
| **List** | 整数索引列表 | **Coll** | 集合（去重） |
| **Table** | 字符串键值对字典 | **Struct** | 结构体 |
| **Object** | 对象 | **Custom** | 自定义类型 |

**特色功能**：
- 🔄 **26位引用计数**：最大支持 6700万+ 引用，自动转为静态值防溢出
- 📦 **集合操作**：差集、交集、并集、对称差集运算
- 🔗 **父子关联**：List/Coll/Table 支持父级继承查找
- 📋 **深浅拷贝**：`xvoCopy` 浅拷贝，`xvoDeepCopy` 深拷贝
- 📊 **调试输出**：`xvoPrintValue` 输出完整数据结构

### 📝 模板引擎详解

企业级模板引擎，支持完整的控制流和数据绑定：

| 语法 | 说明 | 示例 |
|------|------|------|
| `{$var}` | 变量替换 | `{$name}` → 输出变量值 |
| `{%num}` | 数字格式化 | `{%price:¥%.2f}` → 格式化数字 |
| `{&time}` | 时间格式化 | `{&date:YYYY-MM-DD}` → 格式化时间 |
| `{?bool:a:b}` | 条件表达式 | `{?active:Yes:No}` → 根据条件选择 |
| `{*arr:tpl}` | 数组迭代 | 套用子模板处理数组 |
| `{@func:args}` | 函数调用 | 调用外部函数处理 |
| `{=sub}` | 子模板调用 | 引用预定义子模板 |
| `{!comment}` | 注释 | 模板注释，不输出 |

**高级语法**：
```
{#define:子模板名}内容{#end}        // 定义子模板
{#if:表达式}内容{#elseif}内容{#else}内容{#end}  // 条件判断
{#for:1:10:1}内容{#end}               // 计数循环
{#foreach:变量名}内容{#end}           // 迭代循环
{#include:文件名}                      // 包含外部文件
{#script:语言}代码{#end}                // 嵌入脚本
```

### 📄 JSON 处理特性

**SAX 模式解析**（事件驱动，低内存占用）：
- 支持 C 风格单行/多行注释（可配置）
- 支持数组/对象尾部逗号
- 支持十六进制数字（0x 前缀）
- 支持特殊浮点数（nan, inf, -inf）
- 支持单值 JSON（非对象/数组开头）
- 支持转义字符有效性检查

**SAX 模式生成**（增量式写入，智能扩容）：
- 支持格式化/压缩输出
- 跨平台换行符处理
- 自动逗号和缩进管理

---

## 🚀 快速开始

### 📥 安装

```bash
# 克隆仓库
git clone https://gitee.com/xywhsoft/xrt
cd xrt
```

### 🔨 编译

#### Windows 平台

```batch
# 使用 TCC 编译 64 位测试程序
build_TCC_TEST_x64.bat

# 使用 GCC 编译 64 位 DLL
build_GCC_DLL_x64.bat
```

#### Linux/macOS 平台

```bash
# 编译测试程序
bash build_test.sh
```

### 📝 使用示例

#### 基础使用

```c
#include "xrt.h"

int main() {
    // 初始化库
    xrtGlobalData* xCore = xrtInit();
    
    // 使用字符串功能
    str text = xrtReplace("Hello World", 0, "World", 0, "XRT", 0);
    printf("%s\n", text);  // 输出: Hello XRT
    xrtFree(text);
    
    // 时间处理
    xtime now = xrtNow();
    str timeStr = xrtTimeToStr(now, XRT_TIME_FORMAT_DATETIME);
    printf("当前时间: %s\n", timeStr);
    xrtFree(timeStr);
    
    // 清理资源
    xrtUnit();
    return 0;
}
```

#### 动态类型系统（Value）

```c
#include "xrt.h"

int main() {
    xrtInit();
    
    // 创建数组
    xvalue arr = xvoCreateArray();
    xvoArrayAppendInt(arr, 123);
    xvoArrayAppendText(arr, "Hello", 0, FALSE);
    xvoArrayAppendFloat(arr, 3.14);
    
    // 创建表（字典）
    xvalue table = xvoCreateTable();
    xvoTableSetText(table, "name", 0, "XRT", 0, FALSE);
    xvoTableSetInt(table, "version", 0, 1);
    xvoTableSetBool(table, "active", 0, TRUE);
    
    // 获取值
    str name = xvoTableGetText(table, "name", 0);
    printf("Name: %s\n", name);
    
    // 释放资源（引用计数自动管理）
    xvoUnref(arr);
    xvoUnref(table);
    xrtUnit();
    return 0;
}
```

#### JSON 处理

```c
#include "xrt.h"

int main() {
    xrtInit();
    
    // 解析 JSON
    str jsonText = "{\"name\":\"xrt\",\"version\":1.0,\"features\":[\"fast\",\"simple\"]}";
    xvalue json = xrtParseJSON(jsonText, 0);
    
    // 读取值
    str name = xvoTableGetText(json, "name", 0);
    printf("Project: %s\n", name);
    
    // 生成 JSON（格式化输出）
    xvalue newJson = xvoCreateTable();
    xvoTableSetText(newJson, "status", 0, "ok", 0, FALSE);
    xvoTableSetInt(newJson, "code", 0, 200);
    
    size_t len = 0;
    str output = xrtStringifyJSON(newJson, TRUE, &len);  // TRUE = 格式化
    printf("%s\n", output);
    xrtFree(output);
    
    xvoUnref(json);
    xvoUnref(newJson);
    xrtUnit();
    return 0;
}
```

#### 模板引擎

```c
#include "xrt.h"

int main() {
    xrtInit();
    
    // 创建模板变量
    xvalue vars = xvoCreateTable();
    xvoTableSetText(vars, "title", 0, "XRT Library", 0, FALSE);
    xvoTableSetInt(vars, "count", 0, 42);
    
    // 解析模板
    str template = "Welcome to {$title}! Count: {%count}";
    XTE_LiteObject tpl = xteParse(template, 0, NULL);
    
    // 生成输出
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

## 📁 项目结构

```
xrt/
├── lib/                    # 32 个功能子库 (273KB+ 源码)
│   ├── base.h             # 基础内存管理、错误处理
│   ├── charset.h          # 字符集转换 (UTF-8/16/32, 27.7KB)
│   ├── string.h           # 字符串处理 (33.3KB)
│   ├── jnum.h             # 数字解析 (67.2KB)
│   ├── value.h            # 动态类型系统 (41.8KB)
│   ├── json.h             # JSON SAX 解析/生成 (60.6KB)
│   ├── template.h         # 模板引擎 (45.8KB)
│   ├── hash.h             # 高性能哈希 (41.4KB)
│   ├── file.h             # 文件操作 (46.2KB)
│   ├── mempool.h          # 通用内存池 (16.0KB)
│   ├── dict.h             # 字典 (AVL树实现)
│   ├── avltree.h          # AVL 平衡树
│   └── ...                # 其他 20 个模块
├── docs/                   # API 文档 (33 份, 500KB+)
│   ├── api-base.md        # 基础库文档
│   ├── api-value.md       # Value 类型文档 (29.5KB)
│   ├── api-json.md        # JSON 库文档
│   ├── api-template.md    # 模板引擎文档
│   ├── types.md           # 类型定义文档
│   └── ...                # 各模块文档
├── test/                   # 测试文件 (31 个模块)
│   ├── test_base.h
│   ├── test_value.h
│   ├── test_json.h
│   ├── test_template.h
│   └── ...
├── release/                # 编译输出目录
│   ├── x64/               # 64位编译输出
│   └── x86/               # 32位编译输出
├── xrt.h                  # 主头文件（API 声明，2320 行，89.3KB）
├── xrt.c                  # 主实现文件（包含所有 lib/*.h）
├── test.c                 # 测试入口
├── build_TCC_*.bat        # TCC 构建脚本（Windows）
├── build_GCC_*.bat        # GCC 构建脚本（Windows）
├── build*.sh              # Linux/macOS 构建脚本
├── LICENSE                # MIT 开源协议
├── README.md              # 中文文档
└── README.en.md           # 英文文档
```

---

## 🔧 构建选项

### 支持的编译器

| 编译器 | 特点 | 适用场景 |
|--------|------|----------|
| **TCC** | 毫秒级编译速度 | 快速开发调试、脚本式使用 |
| **GCC** | 成熟稳定，优化好 | 生产环境，跨平台编译 |
| **Clang** | LLVM 后端，诊断信息清晰 | macOS/iOS 开发 |
| **MSVC** | Windows 原生支持 | Visual Studio 集成 |

### 支持的平台

| 平台 | 架构 | 状态 |
|------|------|------|
| **Windows** | x86, x64 | ✅ 完全支持 |
| **Linux** | x86, x64 | ✅ 完全支持 |
| **macOS** | x64, ARM64 | ✅ 完全支持 |

### 构建目标

| 目标 | 说明 | 输出 |
|------|------|------|
| **DLL** | 动态链接库 | `xrt.dll` / `libxrt.so` |
| **OBJ** | 静态目标文件 | `xrt.o` |
| **TEST** | 可执行测试程序 | `test.exe` / `test` |

---

## 📚 API 文档

> 📖 **完整文档**：请查阅 [docs 目录](docs/README.md) 获取详细的 API 使用文档和示例。

### 内存管理

```c
ptr xrtMalloc(size_t iSize);                    // 分配内存
ptr xrtCalloc(size_t iNum, size_t iSize);       // 分配并清零
ptr xrtRealloc(ptr pMem, size_t iSize);         // 重新分配
void xrtFree(ptr pmem);                          // 释放内存
ptr xrtTempMemory(size_t iSize);                // 临时内存（32槽位环形自动释放）
```

### 字符集转换

```c
u16str xrtUTF8to16(u8str sText, size_t iSize);   // UTF-8 转 UTF-16
u8str xrtUTF16to8(u16str sText, size_t iSize);   // UTF-16 转 UTF-8
u32str xrtUTF8to32(u8str sText, size_t iSize);   // UTF-8 转 UTF-32
ptr xrtConvCharset(ptr sText, size_t iSize, int iInCP, int iOutCP);  // 任意编码转换
bool xrtIsUTF8(str sText, size_t iSize);         // 判断是否 UTF-8
int xrtDetectCharset(ptr sText, size_t iSize, bool bBOM);  // 自动检测编码
```

### 字符串操作

```c
str xrtCopyStr(str sText, size_t iSize);         // 复制字符串
str xrtFindStr(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bCase);
str xrtReplace(str sText, size_t iSize, str sSubText, size_t iSubSize, str sRepText, size_t iRepSize);
str* xrtSplit(str sText, size_t iSize, str sSepText, size_t iSepSize, bool bSrcRevise);
str xrtFormat(str sFormat, ...);                 // 格式化字符串
str xrtTrim(str sText, size_t iSize, str sSubText, size_t iSubSize, bool bSrcRevise);
str xrtBase64Encode(ptr pMem, size_t iSize, str sTable);  // Base64 编码
ptr xrtBase64Decode(str sText, size_t iSize, str sTable);  // Base64 解码
```

### 文件操作

```c
xfile xrtOpen(str sPath, int bReadOnly, int iCharset);  // 打开文件
void xrtClose(xfile objFile);                    // 关闭文件
str xrtFileReadAll(str sPath, int iCharset);     // 读取全部内容
int xrtFileWriteAll(str sPath, str sText, size_t iSize, int iCharset);
bool xrtFileExists(str sPath);                   // 判断文件是否存在
bool xrtDirExists(str sPath);                    // 判断目录是否存在
int xrtDirScan(str sPath, int bRecu, ptr pProc, ptr Param);  // 遍历目录
bool xrtDirCreateAll(str sPath);                 // 创建多级目录
```

### 时间处理

```c
xtime xrtNow();                                   // 当前日期时间
xtime xrtDateSerial(int64 iYear, int iMonth, int iDay);  // 构建日期
xtime xrtDateTimeSerial(int64 iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond);
str xrtTimeToStr(xtime iTime, int iFormat);      // 时间转字符串
xtime xrtDateAdd(int interval, int64 iValue, xtime iTime);  // 时间加减
int64 xrtDateDiff(int interval, xtime iTime1, xtime iTime2);  // 时间差
int64 xrtYear(xtime iTime);                      // 获取年份
int xrtMonth(xtime iTime);                       // 获取月份
int xrtDay(xtime iTime);                         // 获取日期
```

### Value 动态类型

```c
// 创建值
xvalue xvoCreateNull();                          // 空值
xvalue xvoCreateBool(bool bVal);                 // 布尔值
xvalue xvoCreateInt(int64 iVal);                 // 整数
xvalue xvoCreateFloat(double fVal);              // 浮点数
xvalue xvoCreateText(ptr sVal, uint32 iSize, bool bColloc);  // 字符串
xvalue xvoCreateTime(xtime tVal);                // 时间
xvalue xvoCreateArray();                         // 数组
xvalue xvoCreateList();                          // 列表
xvalue xvoCreateColl();                          // 集合
xvalue xvoCreateTable();                         // 表（字典）

// 引用计数
void xvoAddRef(xvalue pVal);                     // 增加引用
void xvoUnref(xvalue pVal);                      // 减少引用（归零自动释放）

// 数组操作
xvoArrayAppendInt(arr, value);                   // 追加整数
xvoArrayAppendText(arr, str, len, colloc);       // 追加字符串
xvalue xvoArrayGetValue(xvalue pArr, uint32 index);  // 获取值
uint32 xvoArrayItemCount(xvalue pArr);           // 获取数量

// 表操作
xvoTableSetInt(table, key, keylen, value);       // 设置整数
xvoTableSetText(table, key, keylen, str, len, colloc);  // 设置字符串
str xvoTableGetText(xvalue pTbl, str key, uint32 kl);  // 获取字符串
```

### JSON 处理

```c
xvalue xrtParseJSON(str sText, size_t iSize);    // 解析 JSON 字符串
xvalue xrtParseJSON_File(str sFile);             // 解析 JSON 文件
str xrtStringifyJSON(xvalue varVal, int bFormat, size_t* pRetSize);  // 生成 JSON
int xrtStringifyJSON_File(str sFile, xvalue varVal, int bFormat);    // 保存到文件
```

---

## 🎤 设计亮点

### 1. 💾 环形临时内存

32 个临时内存槽位循环使用，自动释放，消除内存泄漏风险：

```c
str temp1 = xrtTempMemory(1024);  // 使用槽位 0
str temp2 = xrtTempMemory(2048);  // 使用槽位 1
// ... 循环使用到槽位 31 后，自动释放槽位 0
// 无需手动 free，适合函数内临时返回值
```

### 2. 🔄 引用计数 GC

Value 类型系统采用 26 位引用计数自动管理内存（最大 67,108,863 次引用）：

```c
xvalue v = xvoCreateInt(123);  // RefCount = 1
xvoAddRef(v);                   // RefCount = 2
xvoUnref(v);                    // RefCount = 1
xvoUnref(v);                    // RefCount = 0，自动释放
// 超过最大引用次数时自动转为静态值，避免溢出
```

### 3. 🏭 多级内存池架构

| 内存池 | 功能 | 特点 |
|--------|------|------|
| **BSMM** | 块结构内存管理 | 256 元素/页，释放链表复用 |
| **MemUnit** | 256 字节页管理 | 支持 GC 标记回收 |
| **FSMemPool** | 固定大小内存池 | 空闲/满载链表分组 |
| **MemPool** | 通用内存池 | 二叉树 FSB 索引，15/31 级分块 |

### 4. 🎲 分布式 ID 生成

192 位唯一 ID，可用于分布式系统标识：

```c
// 结构：时间戳(64bit) + IP地址(32bit) + CPU时钟(32bit) + 随机数(64bit)
str xid = xrtMakeXIDS();     // 生成全局唯一 ID 字符串
xid objXID = xrtMakeXID();   // 生成 XID 结构体
bool same = xrtCompXID(xid1, xid2);  // 比较两个 XID
xrtFree(xid);
```

### 5. ✨ 16 种 Value 数据类型

Value 类型采用紧凑的 16 字节结构：

| 类型 | 说明 | 类型 | 说明 |
|------|------|------|------|
| Empty | 不存在的数据 | Null | 空值 |
| Bool | 布尔值 | Int | 64位整数 |
| Float | 双精度浮点 | Text | 字符串 |
| Time | 时间戳 | Point | 指针 |
| Func | 函数指针 | Array | 数组 |
| List | 列表 | Coll | 集合 |
| Table | 表/字典 | Struct | 结构体 |
| Object | 对象 | Custom | 自定义 |

### 6. 🚀 高性能哈希算法

```c
uint32 hash32 = xrtHash32(data, len);           // 32位 nmhash32x
uint64 hash64 = xrtHash64(data, len);           // 64位 rapidhash
uint64 nano = xrtHash64_Nano(data, len);        // 超轻量版本
```

### 7. 📊 JSON SAX 模式

采用事件驱动的 SAX 模式解析/生成 JSON，内存效率高：

```c
// 解析：流式读取，边解析边回调
xrtJsonParseSAX(jsonText, len, callback);

// 生成：增量式写入，智能扩容
json_sax_print_hd hd = xrtJsonPrintStart(&choice);
xrtJsonPrintObject(hd, NULL, JSON_SAX_START);
xrtJsonPrintString(hd, &key, &value);
xrtJsonPrintObject(hd, NULL, JSON_SAX_FINISH);
str result = xrtJsonPrintFinish(hd, &len, NULL);
```

### 8. 📝 全面字符集支持

```c
// UTF-8/16/32 相互转换
u16str utf16 = xrtUTF8to16(utf8Text, 0);     // UTF-8 → UTF-16
u8str utf8 = xrtUTF16to8(utf16Text, 0);      // UTF-16 → UTF-8
u32str utf32 = xrtUTF8to32(utf8Text, 0);     // UTF-8 → UTF-32

// 自动检测编码
bool isUtf8 = xrtIsUTF8(text, len);          // 判断是否 UTF-8
int charset = xrtDetectCharset(text, len, TRUE);  // 自动检测编码（包括 BOM）

// 任意编码转换
ptr result = xrtConvCharset(text, len, XRT_CP_UTF8, XRT_CP_UTF16);
```

---

## 🧪 测试

项目包含 **31 个测试模块**，100% 覆盖所有功能：

| 测试类别 | 测试模块 |
|----------|----------|
| 基础功能 | base, charset, os, math, string, path, time, file, thread, hash, network, xid |
| 数据结构 | buffer, array_ptr, array_struct, bsmm, memunit, mempool_fs, mempool |
| 栈结构 | stack_ptr, stack, dynstack_ptr, dynstack |
| 树/字典 | llist, avltree, dict, list |
| 高级功能 | value, json, template |

```c
// 在 test.c 中启用需要测试的模块
Test_Base(xCore);        // 基础功能测试
Test_Charset(xCore);     // 字符集测试
Test_String(xCore);      // 字符串测试
Test_File(xCore);        // 文件测试
Test_Value_Basic(xCore); // Value 类型测试
Test_JSON(xCore);        // JSON 测试
Test_Template(xCore);    // 模板引擎测试
Test_Dict(xCore);        // 字典测试
Test_MemPool(xCore);     // 内存池测试
// ...
```

运行测试：

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

## 💡 适用场景

XRT 适合以下开发场景：

### 🛠️ 工具类开发
- 命令行工具、系统辅助程序
- 文件处理、批量转换工具
- 配置文件解析器

### 🌐 服务端开发
- 轻量级 Web 服务
- JSON API 服务
- 模板驱动的内容生成

### 💻 嵌入式开发
- 资源受限环境
- 需要精细内存控制
- 跨平台嵌入式系统

### 📚 学习用途
- C 语言数据结构学习
- 内存管理机制研究
- 编译器原理实践（模板引擎、JSON 解析）

---

## 📄 许可证

本项目采用 [MIT License](LICENSE) 开源协议。

```
Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction...
```

---

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 提交 Pull Request

---

## 📧 联系方式

- **作者**：xLeaves [xywhsoft]
- **邮箱**：xywhsoft@qq.com
- **项目主页**：[Git](https://gitee.com/xywhsoft/xrt)

---

## 🌟 Star History

如果这个项目对你有帮助，请给一个 ⭐ Star 支持一下！

---

## 🙏 致谢

感谢以下开源项目为 XRT 提供的支持：

| 项目 | 功能 | 许可证 |
|------|------|--------|
| [LJSON](https://github.com/lengjingzju/json) | jnum 和 json 功能 | MIT License |
| [nmhash32x](https://github.com/gzm55/hash-garage) | 32位高性能哈希算法 | BSD-2-Clause License |
| [rapidhash](https://github.com/Nicoshev/rapidhash) | 64位高性能哈希算法 | MIT License |
| [PCG](https://github.com/imneme/pcg-c) | 高质量伪随机数生成器 | Apache-2.0 / MIT License |

---

<div align="center">

**Made with ❤️ by xLeaves**

</div>
