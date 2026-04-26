# Charset 字符集转换库

> 字符编码转换、检测和处理功能

[English](api-charset.en.md) | [中文](api-charset.md) | [返回索引](README.md)

---

## 📑 目录

- [字符集常量](#字符集常量)
- [UTF编码转换](#utf编码转换)
- [字节序转换](#字节序转换)
- [通用编码转换](#通用编码转换)
- [编码检测](#编码检测)
- [辅助函数](#辅助函数)

---

## 字符集常量

### 字符集代码页

```c
#define XRT_CP_AUTO         -2      // 自动识别字符集
#define XRT_CP_BINARY       -1      // 二进制文件
#define XRT_CP_OEM          0       // 本机OEM字符集
#define XRT_CP_UTF8         65001   // UTF-8
#define XRT_CP_UTF16        1200    // UTF-16 (小端序)
#define XRT_CP_UTF16_BE     1201    // UTF-16 (大端序)
#define XRT_CP_UTF32        65005   // UTF-32 (小端序)
#define XRT_CP_UTF32_BE     65006   // UTF-32 (大端序)
```

### BOM 标记

```c
#define XRT_CP_BOM          0x40000000  // 带BOM标记
#define XRT_MASK_BOM        0xBFFFFFFF  // BOM掩码
```

**使用方式：**
```c
// 使用带BOM的UTF-8
int charset = XRT_CP_UTF8 | XRT_CP_BOM;

// 去除BOM标记
int pure_charset = charset & XRT_MASK_BOM;
```

---

## UTF编码转换

### xrtUTF8to16

UTF-8 转 UTF-16

**函数原型：**
```c
XXAPI u16str xrtUTF8to16(u8str sText, size_t iSize);
```

**参数：**
- `sText` - UTF-8 字符串
- `iSize` - 字节长度（0表示自动计算到`\0`）

**返回值：**
- 成功：返回 UTF-16 字符串指针
- 失败：返回 `NULL`

**内存释放：** ✅ 需要使用 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str utf8 = (str)"你好，世界！";
    u16str utf16 = xrtUTF8to16(utf8, 0);
    if (utf16) {
        printf("UTF-16 字符数: %" PRId64 "\n", xCore.iRet);
        xrtFree(utf16);
    }
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 转换后的字符数存储在 `xCore.iRet` 中
- 超出 UTF-16 范围的字符（5-6字节 UTF-8）会被替换为 `0xFFFD`

---

### xrtUTF8to32

UTF-8 转 UTF-32

**函数原型：**
```c
XXAPI u32str xrtUTF8to32(u8str sText, size_t iSize);
```

**参数：**
- `sText` - UTF-8 字符串
- `iSize` - 字节长度（0表示自动计算）

**返回值：**
- 成功：返回 UTF-32 字符串指针
- 失败：返回 `NULL`

**内存释放：** ✅ 需要使用 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str utf8 = (str)"Hello 世界";
    u32str utf32 = xrtUTF8to32(utf8, 0);
    if (utf32) {
        printf("UTF-32 字符数: %" PRId64 "\n", xCore.iRet);  // 8
        // UTF-32 每个字符占 4 字节
        xrtFree(utf32);
    }
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- UTF-32 可以表示所有 Unicode 字符，包括 5-6 字节的 UTF-8 字符
- 转换后的字符数存储在 `xCore.iRet` 中

---

### xrtUTF16to8

UTF-16 转 UTF-8

**函数原型：**
```c
XXAPI u8str xrtUTF16to8(u16str sText, size_t iSize);
```

**参数：**
- `sText` - UTF-16 字符串
- `iSize` - 字符数（0表示自动计算到`\0`）

**返回值：**
- 成功：返回 UTF-8 字符串指针
- 失败：返回 `NULL`

**内存释放：** ✅ 需要使用 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 注意：Windows 上 L"你好" 是 UTF-16
    u16str utf16 = (u16str)L"你好世界";
    u8str utf8 = xrtUTF16to8(utf16, 0);
    if (utf8) {
        printf("UTF-8: %s\n", utf8);
        printf("UTF-8 字节数: %" PRId64 "\n", xCore.iRet);
        xrtFree(utf8);
    }
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 转换后的字节数存储在 `xCore.iRet` 中
- 错误的代理对会被替换为 `0xEFBFBD`（UTF-8 的替换字符）

---

### xrtUTF16to32

UTF-16 转 UTF-32

**函数原型：**
```c
XXAPI u32str xrtUTF16to32(u16str sText, size_t iSize);
```

**参数：**
- `sText` - UTF-16 字符串
- `iSize` - 字符数（0表示自动计算）

**返回值：**
- 成功：返回 UTF-32 字符串指针
- 失败：返回 `NULL`

**内存释放：** ✅ 需要使用 `xrtFree` 释放

---

### xrtUTF32to8

UTF-32 转 UTF-8

**函数原型：**
```c
XXAPI u8str xrtUTF32to8(u32str sText, size_t iSize);
```

**参数：**
- `sText` - UTF-32 字符串
- `iSize` - 字符数（0表示自动计算）

**返回值：**
- 成功：返回 UTF-8 字符串指针
- 失败：返回 `NULL`

**内存释放：** ✅ 需要使用 `xrtFree` 释放

---

### xrtUTF32to16

UTF-32 转 UTF-16

**函数原型：**
```c
XXAPI u16str xrtUTF32to16(u32str sText, size_t iSize);
```

**参数：**
- `sText` - UTF-32 字符串
- `iSize` - 字符数（0表示自动计算）

**返回值：**
- 成功：返回 UTF-16 字符串指针
- 失败：返回 `NULL`

**内存释放：** ✅ 需要使用 `xrtFree` 释放

---

## 字节序转换

### xrtUTF16LEtoBE

UTF-16 小端序与大端序转换

**函数原型：**
```c
XXAPI u16str xrtUTF16LEtoBE(u16str sText, size_t iSize, bool bSrcRevise);
```

**参数：**
- `sText` - UTF-16 字符串
- `iSize` - 字符数（0表示自动计算）
- `bSrcRevise` - 是否直接修改源数据
  - `TRUE` - 直接修改源字符串，返回源指针
  - `FALSE` - 创建新字符串，需要 `xrtFree` 释放

**返回值：**
- `bSrcRevise=TRUE`：返回源指针 `sText`
- `bSrcRevise=FALSE`：返回新分配的字符串

**内存释放：** 
- `bSrcRevise=TRUE` - ⭕ 无需释放
- `bSrcRevise=FALSE` - ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 不修改源数据，创建新字符串
    u16str le_text = (u16str)L"Hello";
    u16str be_text = xrtUTF16LEtoBE(le_text, 0, FALSE);
    printf("转换完成，原始数据不变\n");
    xrtFree(be_text);
    
    // 直接修改源数据（需要是可写内存）
    u16str text = xrtMalloc(10 * sizeof(u16));
    text[0] = 0x4F60;  // '你'
    text[1] = 0x597D;  // '好'
    text[2] = 0;
    
    printf("转换前: 0x%04X 0x%04X\n", text[0], text[1]);
    xrtUTF16LEtoBE(text, 2, TRUE);  // 原地转换
    printf("转换后: 0x%04X 0x%04X\n", text[0], text[1]);
    
    xrtFree(text);
    xrtUnit();
    return 0;
}
```

---

### xrtUTF32LEtoBE

UTF-32 小端序与大端序转换

**函数原型：**
```c
XXAPI u32str xrtUTF32LEtoBE(u32str sText, size_t iSize, bool bSrcRevise);
```

**参数：**
- `sText` - UTF-32 字符串
- `iSize` - 字符数（0表示自动计算）
- `bSrcRevise` - 是否直接修改源数据

**返回值：**
- `bSrcRevise=TRUE`：返回源指针
- `bSrcRevise=FALSE`：返回新字符串

**内存释放：**
- `bSrcRevise=TRUE` - ⭕ 无需释放
- `bSrcRevise=FALSE` - ✅ 需要 `xrtFree` 释放

---

## 通用编码转换

### xrtConvCharset

任意字符集转换

**函数原型：**
```c
XXAPI ptr xrtConvCharset(ptr sText, size_t iSize, int iInCP, int iOutCP);
```

**参数：**
- `sText` - 源字符串
- `iSize` - 源字符串字节长度（0表示自动计算）
- `iInCP` - 输入字符集代码页
- `iOutCP` - 输出字符集代码页

**返回值：**
- 成功：返回转换后的字符串
- 失败：返回 `NULL`

**内存释放：** ✅ 需要使用 `xrtFree` 释放

**支持的字符集：**
- `XRT_CP_AUTO` - 自动检测（仅作为输入）
- `XRT_CP_BINARY` - 二进制（不转换）
- `XRT_CP_OEM` - 本机字符集（Windows: ANSI, Linux: UTF-8）
- `XRT_CP_UTF8` - UTF-8
- `XRT_CP_UTF16` - UTF-16 (LE)
- `XRT_CP_UTF16_BE` - UTF-16 (BE)
- `XRT_CP_UTF32` - UTF-32 (LE)
- `XRT_CP_UTF32_BE` - UTF-32 (BE)

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // UTF-8 转 UTF-16
    str utf8 = (str)"你好";
    u16str utf16 = xrtConvCharset(utf8, 0, XRT_CP_UTF8, XRT_CP_UTF16);
    if (utf16) {
        printf("转换成功\n");
        xrtFree(utf16);
    }
    
    // 本机编码转 UTF-8（Windows 上可能是 GBK）
    str oem = (str)"本机编码文本";
    str utf8_out = xrtConvCharset(oem, 0, XRT_CP_OEM, XRT_CP_UTF8);
    if (utf8_out) {
        printf("UTF-8: %s\n", utf8_out);
        xrtFree(utf8_out);
    }
    
    // UTF-16 转 UTF-32 BE
    u16str src16 = (u16str)L"Test";
    u32str dst32be = xrtConvCharset(src16, 0, XRT_CP_UTF16, XRT_CP_UTF32_BE);
    if (dst32be) {
        printf("UTF-32 BE 转换成功\n");
        xrtFree(dst32be);
    }
    
    xrtUnit();
    return 0;
}
```

**平台说明：**
- Windows：支持所有 Windows 代码页（通过 Win32 API）
- Linux/macOS：仅支持 UTF-8/16/32 互转，OEM 固定为 UTF-8

---

## 编码检测

### xrtIsUTF8

判断是否为合法的 UTF-8 字符串

**函数原型：**
```c
XXAPI bool xrtIsUTF8(str sText, size_t iSize);
```

**参数：**
- `sText` - 要检测的字符串
- `iSize` - 字节长度（0表示自动计算）

**返回值：**
- `TRUE` - 是合法的 UTF-8
- `FALSE` - 不是 UTF-8 或含有非法序列

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str text1 = (str)"Hello 世界";
    bool is_utf8 = xrtIsUTF8(text1, 0);  // TRUE
    printf("\"你好世界\" 是 UTF-8: %s\n", is_utf8 ? "true" : "false");
    
    // 非法 UTF-8 序列
    unsigned char text2[] = {0xFF, 0xFE, 0x00};
    is_utf8 = xrtIsUTF8((str)text2, 2);  // FALSE
    printf("0xFF 0xFE 是 UTF-8: %s\n", is_utf8 ? "true" : "false");
    
    // 空字符串返回 TRUE
    is_utf8 = xrtIsUTF8((str)"", 0);  // TRUE
    printf("空字符串是 UTF-8: %s\n", is_utf8 ? "true" : "false");
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- `NULL` 输入返回 `FALSE`
- 空字符串返回 `TRUE`
- 检测规则：检查多字节序列是否以 `10xxxxxx` 开头，检查是否包含 `0xFE`/`0xFF`

---

### xrtDetectCharset

自动检测字符集编码

**函数原型：**
```c
XXAPI int xrtDetectCharset(ptr sText, size_t iSize, bool bBOM);
```

**参数：**
- `sText` - 要检测的数据
- `iSize` - 数据长度
- `bBOM` - 检测到的BOM信息（通过返回值传递）

**返回值：**
- 检测到的字符集代码页（可能包含 `XRT_CP_BOM` 标记）
- 如果带BOM，返回值会包含 `XRT_CP_BOM` 位

**检测规则：**
1. 首先检测 BOM 标记
2. 然后检测是否为合法的 UTF-8
3. 根据 `\0` 的分布推测 UTF-16/UTF-32
4. 都不符合则返回 `XRT_CP_OEM` 或 `XRT_CP_BINARY`

**BOM 标记：**
- UTF-8 BOM: `EF BB BF`
- UTF-16 LE BOM: `FF FE`
- UTF-16 BE BOM: `FE FF`
- UTF-32 LE BOM: `FF FE 00 00`
- UTF-32 BE BOM: `00 00 FE FF`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 模拟文件数据（带 UTF-8 BOM）
    unsigned char data_with_bom[] = {0xEF, 0xBB, 0xBF, 'H', 'e', 'l', 'l', 'o'};
    size_t size = sizeof(data_with_bom);
    
    int charset = xrtDetectCharset(data_with_bom, size, TRUE);
    
    // 检查是否有 BOM
    if (charset & XRT_CP_BOM) {
        printf("检测到 BOM 标记\n");
        int pure_charset = charset & XRT_MASK_BOM;  // 去除 BOM 标记
        
        switch (pure_charset) {
            case XRT_CP_UTF8:
                printf("编码: UTF-8 with BOM\n");
                break;
            case XRT_CP_UTF16:
                printf("编码: UTF-16 LE with BOM\n");
                break;
            case XRT_CP_UTF16_BE:
                printf("编码: UTF-16 BE with BOM\n");
                break;
        }
    } else {
        printf("无 BOM 标记\n");
    }
    
    // 检测无 BOM 的文本
    str plain_utf8 = (str)"你好世界";
    charset = xrtDetectCharset(plain_utf8, 0, TRUE);
    printf("纯 UTF-8 检测结果: %d (XRT_CP_UTF8=%d)\n", charset, XRT_CP_UTF8);
    
    xrtUnit();
    return 0;
}
```

**检测策略：**
1. 首先检测 BOM 标记
2. 然后检测是否为合法的 UTF-8
3. 根据 `\0` 的分布推测 UTF-16/UTF-32
4. 都不符合则返回 `XRT_CP_OEM`（Windows）或 `XRT_CP_BINARY`（Linux）

---

### xrtGetCharSize

获取字符集的字符大小

**函数原型：**
```c
XXAPI int xrtGetCharSize(int iCP);
```

**参数：**
- `iCP` - 字符集代码页

**返回值：**
- `1` - 单字节编码（UTF-8、OEM 等）
- `2` - 双字节编码（UTF-16、UTF-16 BE）
- `4` - 四字节编码（UTF-32、UTF-32 BE）

**字符大小说明：**
- `XRT_CP_UTF8` → `1`（注意：这是基本单位，实际字符可能 1-6 字节）
- `XRT_CP_UTF16` / `XRT_CP_UTF16_BE` → `2`
- `XRT_CP_UTF32` / `XRT_CP_UTF32_BE` → `4`
- `XRT_CP_OEM` → `1`（基本单位）
- 其他 → `1`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    int size_utf8 = xrtGetCharSize(XRT_CP_UTF8);      // 1
    int size_utf16 = xrtGetCharSize(XRT_CP_UTF16);    // 2
    int size_utf32 = xrtGetCharSize(XRT_CP_UTF32);    // 4
    int size_utf16be = xrtGetCharSize(XRT_CP_UTF16_BE); // 2
    
    printf("UTF-8 基本单位: %d 字节\n", size_utf8);
    printf("UTF-16 单位: %d 字节\n", size_utf16);
    printf("UTF-32 单位: %d 字节\n", size_utf32);
    
    // 带 BOM 的字符集也能正确处理（会自动去除 BOM 标记）
    int size_with_bom = xrtGetCharSize(XRT_CP_UTF16 | XRT_CP_BOM);  // 2
    printf("带BOM的UTF-16: %d 字节\n", size_with_bom);
    
    xrtUnit();
    return 0;
}
```

---

## 辅助函数

### 字符串长度函数

这些函数在 `base.h` 中定义，补充了 UTF-16/32 支持：

```c
// UTF-16 字符串长度（字符数）
XXAPI size_t u16len(u16str sText);

// UTF-32 字符串长度（字符数）
XXAPI size_t u32len(u32str sText);
```

**示例：**
```c
u16str text16 = L"Hello";
size_t len16 = u16len(text16);  // 5

u32str text32 = U"World";
size_t len32 = u32len(text32);  // 5
```

---

## 使用场景

### 1. 跨平台文本处理

```c
#include "xrt.h"
#include <stdio.h>

void DisplayMessage(str utf8_text) {
#if defined(_WIN32) || defined(_WIN64)
    // Windows: 转换为 UTF-16 用于 Win32 API
    u16str wtext = xrtUTF8to16(utf8_text, 0);
    // MessageBoxW(NULL, (LPCWSTR)wtext, L"Title", MB_OK);
    printf("Windows: 已转换为 UTF-16\n");
    xrtFree(wtext);
#else
    // Linux/macOS: 直接使用 UTF-8
    printf("%s\n", utf8_text);
#endif
}

int main() {
    xrtInit();
    DisplayMessage((str)"你好世界");
    xrtUnit();
    return 0;
}
```

---

### 2. 文件编码转换

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 读取文件
    ptr file_data = xrtFileGetAll((str)"input.txt");
    if (file_data == NULL) {
        printf("文件读取失败\n");
        xrtUnit();
        return 1;
    }
    size_t file_size = xCore.iRet;
    
    // 检测编码
    int detected = xrtDetectCharset(file_data, file_size, TRUE);
    int charset = detected & XRT_MASK_BOM;
    printf("检测到编码: %d\n", charset);
    
    // 转换为 UTF-8
    str utf8_text = xrtConvCharset(file_data, file_size, charset, XRT_CP_UTF8);
    if (utf8_text) {
        printf("转换成功，UTF-8 内容:\n%s\n", utf8_text);
        
        // 保存为 UTF-8 文件
        xrtFilePutAll((str)"output.txt", utf8_text, 0);
        xrtFree(utf8_text);
    }
    
    xrtFree(file_data);
    xrtUnit();
    return 0;
}
```

---

### 3. 网络数据处理

```c
#include "xrt.h"
#include <stdio.h>

// 模拟网络接收的 UTF-16 数据
u16str SimulateReceiveFromNetwork() {
    u16str data = xrtMalloc(20 * sizeof(u16));
    data[0] = 0x4F60;  // '你'
    data[1] = 0x597D;  // '好'
    data[2] = 0;
    return data;
}

void ProcessText(str text) {
    printf("处理文本: %s\n", text);
}

int main() {
    xrtInit();
    
    // 接收 UTF-16 数据
    u16str received_data = SimulateReceiveFromNetwork();
    
    // 转换为 UTF-8 处理
    str utf8_data = xrtUTF16to8(received_data, 0);
    if (utf8_data) {
        ProcessText(utf8_data);
        xrtFree(utf8_data);
    }
    
    xrtFree(received_data);
    xrtUnit();
    return 0;
}
```

---

### 4. 国际化字符串

```c
#include "xrt.h"
#include <stdio.h>

void DisplayText(u16str wtext) {
    // 转回 UTF-8 显示
    str utf8 = xrtUTF16to8(wtext, 0);
    printf("显示: %s\n", utf8);
    xrtFree(utf8);
}

int main() {
    xrtInit();
    
    // 多语言文本处理
    str chinese = (str)"你好世界";
    str english = (str)"Hello World";
    str japanese = (str)"こんにちは";
    
    // 统一转换为 UTF-16
    u16str wchinese = xrtUTF8to16(chinese, 0);
    u16str wenglish = xrtUTF8to16(english, 0);
    u16str wjapanese = xrtUTF8to16(japanese, 0);
    
    DisplayText(wchinese);
    DisplayText(wenglish);
    DisplayText(wjapanese);
    
    xrtFree(wchinese);
    xrtFree(wenglish);
    xrtFree(wjapanese);
    
    xrtUnit();
    return 0;
}
```

---

## 最佳实践

### 1. 统一使用 UTF-8

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ✅ 推荐：内部统一使用 UTF-8
    str internal_text = (str)"内部文本";
    
    // 只在需要时才转换
#if defined(_WIN32) || defined(_WIN64)
    u16str display_text = xrtUTF8to16(internal_text, 0);
    printf("已转换为 UTF-16 用于 Win32 API\n");
    // ... 使用 display_text 调用 Windows API
    xrtFree(display_text);
#else
    printf("直接使用 UTF-8: %s\n", internal_text);
#endif
    
    xrtUnit();
    return 0;
}
```

---

### 2. 自动编码检测

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ✅ 处理未知编码的文件
    ptr data = xrtFileGetAll((str)"unknown.txt");
    if (data == NULL) {
        printf("文件读取失败\n");
        xrtUnit();
        return 1;
    }
    size_t size = xCore.iRet;
    
    // 检测编码
    int charset = xrtDetectCharset(data, size, TRUE);
    int pure_charset = charset & XRT_MASK_BOM;
    printf("检测到编码: %d\n", pure_charset);
    
    // 转换为 UTF-8
    str utf8 = xrtConvCharset(data, size, pure_charset, XRT_CP_UTF8);
    if (utf8) {
        printf("UTF-8 内容:\n%s\n", utf8);
        xrtFree(utf8);
    }
    
    xrtFree(data);
    xrtUnit();
    return 0;
}
```

---

### 3. BOM 处理

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 模拟带 BOM 的文件数据
    unsigned char data[] = {0xEF, 0xBB, 0xBF, 'H', 'e', 'l', 'l', 'o', '\0'};
    size_t size = sizeof(data) - 1;
    
    // 检测编码
    int charset = xrtDetectCharset(data, size, TRUE);
    
    if (charset & XRT_CP_BOM) {
        printf("有 BOM 标记\n");
        
        // 跳过 BOM 字节
        ptr text_start = data;
        size_t bom_size = 0;
        
        switch (charset & XRT_MASK_BOM) {
            case XRT_CP_UTF8:     bom_size = 3; break;  // EF BB BF
            case XRT_CP_UTF16:    bom_size = 2; break;  // FF FE
            case XRT_CP_UTF16_BE: bom_size = 2; break;  // FE FF
            case XRT_CP_UTF32:    bom_size = 4; break;  // FF FE 00 00
            case XRT_CP_UTF32_BE: bom_size = 4; break;  // 00 00 FE FF
        }
        
        printf("BOM 大小: %zu 字节\n", bom_size);
        printf("跳过 BOM 后的内容: %s\n", (char*)(text_start + bom_size));
    } else {
        printf("无 BOM 标记\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

### 4. 字节序兼容性

```c
#include "xrt.h"
#include <stdio.h>

void SendToNetwork(u16str data) {
    printf("发送数据: 0x%04X 0x%04X\n", data[0], data[1]);
}

int main() {
    xrtInit();
    
    // 模拟本地 UTF-16 数据
    u16str local_text = xrtMalloc(10 * sizeof(u16));
    local_text[0] = 0x4F60;  // '你' LE
    local_text[1] = 0x597D;  // '好' LE
    local_text[2] = 0;
    
    printf("本地数据 (LE): 0x%04X 0x%04X\n", local_text[0], local_text[1]);
    
    // 网络传输：统一使用大端序
    u16str be_text = xrtUTF16LEtoBE(local_text, 2, FALSE);
    printf("网络数据 (BE): 0x%04X 0x%04X\n", be_text[0], be_text[1]);
    SendToNetwork(be_text);
    xrtFree(be_text);
    
    // 接收大端序数据并转换回本机序
    u16str received_be = xrtMalloc(10 * sizeof(u16));
    received_be[0] = 0x604F;  // '你' BE
    received_be[1] = 0x7D59;  // '好' BE
    received_be[2] = 0;
    
    u16str le_text = xrtUTF16LEtoBE(received_be, 2, FALSE);
    printf("转换后 (LE): 0x%04X 0x%04X\n", le_text[0], le_text[1]);
    xrtFree(le_text);
    xrtFree(received_be);
    
    xrtFree(local_text);
    xrtUnit();
    return 0;
}
```

---

## 性能提示

### 1. 避免重复转换

```c
#include "xrt.h"
#include <stdio.h>

void Display(u16str text) {
    str utf8 = xrtUTF16to8(text, 0);
    printf("%s\n", utf8);
    xrtFree(utf8);
}

int main() {
    xrtInit();
    
    // 模拟数据
    str utf8_array[3] = {
        (str)"文本1",
        (str)"文本2",
        (str)"文本3"
    };
    int count = 3;
    
    // ❌ 低效：重复转换
    printf("低效方式:\n");
    for (int i = 0; i < count; i++) {
        u16str wtext = xrtUTF8to16(utf8_array[i], 0);
        Display(wtext);
        xrtFree(wtext);
    }
    
    // ✅ 高效：预转换
    printf("高效方式:\n");
    u16str* wtext_array = xrtMalloc(count * sizeof(u16str));
    for (int i = 0; i < count; i++) {
        wtext_array[i] = xrtUTF8to16(utf8_array[i], 0);
    }
    // 多次使用已转换的数据
    for (int i = 0; i < count; i++) {
        Display(wtext_array[i]);
    }
    // 统一释放
    for (int i = 0; i < count; i++) {
        xrtFree(wtext_array[i]);
    }
    xrtFree(wtext_array);
    
    xrtUnit();
    return 0;
}
```

---

### 2. 使用原地转换

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 分配可写内存并填充数据
    u16str text = xrtMalloc(10 * sizeof(u16));
    text[0] = 0x0048;  // 'H'
    text[1] = 0x0069;  // 'i'
    text[2] = 0;
    
    printf("转换前: 0x%04X 0x%04X\n", text[0], text[1]);
    
    // ✅ 原地转换，无需额外内存
    xrtUTF16LEtoBE(text, 2, TRUE);
    printf("转换后: 0x%04X 0x%04X\n", text[0], text[1]);
    
    // 再转回来
    xrtUTF16LEtoBE(text, 2, TRUE);
    printf("转回后: 0x%04X 0x%04X\n", text[0], text[1]);
    
    xrtFree(text);
    xrtUnit();
    return 0;
}
```

---

## 常见错误

### 1. 忘记释放内存

```c
#include "xrt.h"
#include <stdio.h>

void UseText(u16str text) {
    str utf8 = xrtUTF16to8(text, 0);
    printf("%s\n", utf8);
    xrtFree(utf8);
}

int main() {
    xrtInit();
    
    // ❌ 内存泄漏
    u16str text1 = xrtUTF8to16((str)"text", 0);
    UseText(text1);
    // 忘记 xrtFree(text1);
    
    // ✅ 正确
    u16str text2 = xrtUTF8to16((str)"text", 0);
    UseText(text2);
    xrtFree(text2);  // 记得释放
    
    xrtUnit();
    return 0;
}
```

---

### 2. 字符数与字节数混淆

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

int main() {
    xrtInit();
    
    str utf8 = (str)"你好";  // 6字节，2个汉字
    size_t byte_len = strlen((char*)utf8);
    printf("UTF-8 字节数: %zu\n", byte_len);  // 6
    
    // ❌ 错误：把字符数当作字节数
    // u16str wrong = xrtUTF8to16(utf8, 2);  // 错误！2是字节数，不是字符数
    
    // ✅ 正确：UTF-8 转换时用字节数，或用 0 自动计算
    u16str correct1 = xrtUTF8to16(utf8, byte_len);  // 用字节数
    u16str correct2 = xrtUTF8to16(utf8, 0);         // 自动计算
    
    printf("UTF-16 字符数: %" PRId64 "\n", xCore.iRet);  // 2
    
    xrtFree(correct1);
    xrtFree(correct2);
    
    xrtUnit();
    return 0;
}
```

**说明：**
- UTF-8 转换函数的 `iSize` 参数是**字节数**
- UTF-16/UTF-32 转换函数的 `iSize` 参数是**字符数**
- 推荐使用 `0` 让函数自动计算长度

---

## 相关文档

- [类型定义](types.md) - 字符串类型定义
- [String 字符串处理](api-string.md) - 字符串操作
- [File 文件操作](api-file.md) - 文件编码处理
- [主索引](README.md) - 返回文档首页

---

<div align="center">

[⬆️ 返回顶部](#charset-字符集转换库)

</div>
