# XRT API 完整文档

> XRT (X Runtime Library) API 参考手册

---

## 📚 文档导航

### 🔹 基础类型定义
- [基础类型定义](types.md) - 所有基础数据类型、宏定义和全局结构

### 🔹 核心功能模块

| 模块 | 文档链接 | 说明 |
|------|---------|------|
| **Base** | [基础函数库](api-base.md) | 内存管理、错误处理 |
| **Charset** | [字符集转换](api-charset.md) | UTF-8/16/32 编码转换 |
| **OS** | [操作系统](api-os.md) | 进程管理、系统调用 |
| **Math** | [数学运算](api-math.md) | 随机数生成 |
| **String** | [字符串处理](api-string.md) | 字符串操作、编码解码 |
| **Path** | [路径处理](api-path.md) | 文件路径操作 |
| **Time** | [时间处理](api-time.md) | 日期时间计算 |
| **File** | [文件操作](api-file.md) | 文件读写、目录管理 |
| **Thread** | [线程管理](api-thread.md) | 多线程支持 |
| **Hash** | [哈希计算](api-hash.md) | 32/64位哈希函数 |
| **Network** | [网络功能](api-network.md) | 本机网络信息 |
| **XID** | [分布式ID](api-xid.md) | 唯一ID生成器 |

### 🔹 数据结构模块

| 模块 | 文档链接 | 说明 |
|------|---------|------|
| **Buffer** | [动态缓冲区](api-buffer.md) | 内存缓冲区管理 |
| **Array** | [结构体数组](api-array.md) | 动态数组 |
| **PtrArray** | [指针数组](api-ptrarray.md) | 指针动态数组 |
| **BSMM** | [块结构内存](api-bsmm.md) | 块内存管理器 |
| **MemUnit** | [内存单元](api-memunit.md) | 256字节页管理 |
| **FSMemPool** | [固定内存池](api-mempool-fs.md) | 固定大小内存池 |
| **Stack** | [静态栈](api-stack.md) | 固定深度栈 |
| **DynStack** | [动态栈](api-dynstack.md) | 可变深度栈 |
| **LList** | [双向链表](api-llist.md) | 链表数据结构 |
| **LList-Base** | [链表基础操作](api-llist-base.md) | 链表底层操作 |
| **AVLTree** | [AVL树](api-avltree.md) | 平衡二叉树 |
| **AVLTree-Base** | [AVL树基础操作](api-avltree-base.md) | AVL树底层操作 |
| **MemPool** | [通用内存池](api-mempool.md) | 多级内存池 |
| **Dict** | [字典](api-dict.md) | 键值对存储 |
| **List** | [列表](api-list.md) | 整数索引列表 |

### 🔹 高级功能模块

| 模块 | 文档链接 | 说明 |
|------|---------|------|
| **Value** | [动态类型系统](api-value.md) | 15种动态数据类型 |
| **JNUM** | [数值转换](api-jnum.md) | 数字与字符串转换 |
| **JSON** | [JSON处理](api-json.md) | JSON 解析与生成 |
| **Template** | [模板引擎](api-template.md) | 字符串模板处理 |

---

## 📖 快速索引

### 类型定义速查

#### 基础类型
```c
typedef unsigned char* str;        // 字符串
typedef char i8;                    // 8位整数
typedef unsigned char u8;           // 无符号8位整数
typedef short i16;                  // 16位整数
typedef unsigned short u16;         // 无符号16位整数
typedef int i32;                    // 32位整数
typedef unsigned int u32;           // 无符号32位整数
typedef long long i64;              // 64位整数
typedef unsigned long long u64;     // 无符号64位整数
typedef float f32;                  // 32位浮点数
typedef double f64;                 // 64位浮点数
typedef void* ptr;                  // 通用指针
typedef int64 xtime;                // 时间类型
```

#### 数据结构类型
```c
typedef struct xbuffer_struct* xbuffer;       // 缓冲区
typedef struct xparray_struct* xparray;       // 指针数组
typedef struct xarray_struct* xarray;         // 结构体数组
typedef struct xbsmm_struct* xbsmm;           // 块内存管理器
typedef struct xmemunit_struct* xmemunit;     // 内存单元
typedef struct xfsmempool_struct* xfsmempool; // 固定大小内存池
typedef struct xstack_struct* xstack;         // 静态栈
typedef struct xdynstack_struct* xdynstack;   // 动态栈
typedef struct xllist_struct* xllist;         // 双向链表
typedef struct xavltree_struct* xavltree;     // AVL树
typedef struct xmempool_struct* xmempool;     // 通用内存池
typedef struct xdict_struct* xdict;           // 字典
typedef struct xlist_struct* xlist;           // 列表
typedef struct xvalue_struct* xvalue;         // 动态类型
typedef struct xfile_struct* xfile;           // 文件对象
typedef struct xthread_struct* xthread;       // 线程对象
typedef struct xid_struct* xid;               // 分布式ID
```

### 常用函数速查

#### 初始化与清理
```c
xrtGlobalData* xrtInit();           // 初始化库
void xrtUnit();                      // 释放库资源
```

#### 内存管理
```c
ptr xrtMalloc(size_t iSize);                 // 分配内存
ptr xrtCalloc(size_t iNum, size_t iSize);   // 分配并清零
ptr xrtRealloc(ptr pMem, size_t iSize);     // 重新分配
void xrtFree(ptr pmem);                      // 释放内存
ptr xrtTempMemory(size_t iSize);            // 临时内存（自动释放）
```

#### 字符串操作
```c
str xrtCopyStr(str sText, size_t iSize);                     // 复制字符串
str xrtFindStr(str sText, size_t iSize, str sSubText, ...); // 查找字符串
str xrtReplace(str sText, ..., str sRepText, ...);          // 替换字符串
str* xrtSplit(str sText, ..., str sSepText, ...);           // 分割字符串
str xrtFormat(str sFormat, ...);                             // 格式化字符串
```

#### 文件操作
```c
xfile xrtOpen(str sPath, int bReadOnly, int iCharset);  // 打开文件
void xrtClose(xfile objFile);                            // 关闭文件
str xrtFileReadAll(str sPath, int iCharset);            // 读取全部内容
bool xrtFileWriteAll(str sPath, str sText, ...);        // 写入全部内容
bool xrtFileExists(str sPath);                           // 判断文件是否存在
```

#### 时间处理
```c
xtime xrtNow();                                          // 当前时间
xtime xrtDateSerial(int64 iYear, int iMonth, int iDay); // 构建日期
str xrtTimeToStr(xtime iTime, int iFormat);             // 时间转字符串
xtime xrtDateAdd(int interval, int64 iValue, xtime);    // 时间加减
```

#### Value 动态类型
```c
xvalue xvoCreateNull();                                  // 创建 null
xvalue xvoCreateInt(int64 iVal);                        // 创建整数
xvalue xvoCreateText(ptr sVal, uint32 iSize, bool);     // 创建字符串
xvalue xvoCreateArray();                                 // 创建数组
xvalue xvoCreateTable();                                 // 创建表
void xvoUnref(xvalue pVal);                             // 释放引用
```

---

## 🎯 使用指南

### 1. 库初始化

在使用任何 XRT 功能之前，必须先初始化：

```c
#include "xrt.h"

int main() {
    // 初始化 XRT 库
    xrtGlobalData* xCore = xrtInit();
    
    // 使用 XRT 功能
    // ...
    
    // 清理资源
    xrtUnit();
    return 0;
}
```

### 2. 内存管理规范

**需要释放的函数：**
- 所有注释中标注"需使用 xrtFree 释放"的函数
- 使用 `xrtMalloc/xrtCalloc/xrtRealloc` 分配的内存

**无需释放的函数：**
- `xrtTempMemory` - 使用环形临时内存，自动管理
- 返回常量字符串的函数

### 3. 字符集处理

XRT 支持多种字符集编码：

```c
// 字符集常量
XRT_CP_AUTO     // 自动识别
XRT_CP_BINARY   // 二进制
XRT_CP_OEM      // 本机编码
XRT_CP_UTF8     // UTF-8
XRT_CP_UTF16    // UTF-16
XRT_CP_UTF32    // UTF-32
```

### 4. 错误处理

```c
// 设置错误回调
void OnError(str sError) {
    printf("Error: %s\n", sError);
}

xCore->OnError = OnError;

// 检查错误
if (xCore->LastError != xCore->sNull) {
    printf("Last Error: %s\n", xCore->LastError);
}
```

---

## 📊 API 统计

| 类别 | 数量 |
|------|------|
| 基础类型定义 | 30+ |
| 宏定义 | 150+ |
| 数据结构 | 25+ |
| 函数声明 | 300+ |
| 内联函数 | 20+ |

---

## 📝 文档约定

### 函数原型说明

```c
XXAPI 返回类型 函数名(参数类型 参数名, ...);
```

- `XXAPI` - 导出标记（DLL编译时使用）
- 参数说明中的 `size_t iSize` - 当值为 0 时会自动计算字符串长度
- `bool bSrcRevise` - TRUE 时会修改源数据，FALSE 时返回新分配的内存

### 内存释放标注

- ✅ **需释放** - 必须使用 `xrtFree` 释放
- ⭕ **自动管理** - 由库自动管理
- 🔄 **引用计数** - 使用 `xvoUnref` 释放

---

## 🔗 相关链接

- [项目主页](../README.md)
- [快速开始](../README.md#快速开始)
- [示例代码](../test/)
- [许可证](../README.md#许可证)

---

<div align="center">

**XRT API 文档** | 版本 1.0 | 最后更新: 2025-01

</div>
