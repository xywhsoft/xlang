# File 文件操作库

> 文件读写、目录管理功能

[English](api-file.en.md) | [中文](api-file.md) | [返回索引](README.md)

---

## 📑 目录

- [常量定义](#常量定义)
- [数据类型](#数据类型)
- [文件打开与关闭](#文件打开与关闭)
- [文件定位](#文件定位)
- [文件读写](#文件读写)
- [快捷读写](#快捷读写)
- [文件信息](#文件信息)
- [文件操作](#文件操作)
- [目录操作](#目录操作)
- [使用场景](#使用场景)

---

## 常量定义

### 游标定位常量

| 常量 | 值 | 说明 |
|------|-----|------|
| `XRT_SEEK_SET` | `0` | 从文件开始位置 |
| `XRT_SEEK_CUR` | `1` | 从当前位置 |
| `XRT_SEEK_END` | `2` | 从文件末尾位置 |

---

## 数据类型

### xfile_struct

文件对象结构体。

**定义：**
```c
typedef struct {
#if defined(_WIN32) || defined(_WIN64)
    HANDLE obj;     // Windows 文件句柄
#else
    int idx;        // Linux/macOS 文件描述符
#endif
    int Charset;    // 文件字符集
    int BOM;        // BOM 长度（0/2/3/4）
} xfile_struct;

typedef xfile_struct* xfile;
```

**成员说明：**
- `obj`/`idx` - 平台相关的文件句柄
- `Charset` - 打开文件时指定的字符集
- `BOM` - 检测到的 BOM 字节数

---

## 文件打开与关闭

### xrtOpen

打开文件。

**函数原型：**
```c
XXAPI xfile xrtOpen(str sPath, int bReadOnly, int iCharset);
```

**参数：**
- `sPath` - 文件路径（UTF-8 编码）
- `bReadOnly` - 是否只读
  - `TRUE` - 只读模式（文件必须存在）
  - `FALSE` - 读写模式（文件不存在则创建）
- `iCharset` - 字符集（见 Charset 库）
  - `XRT_CP_AUTO` - 自动检测
  - `XRT_CP_UTF8` - UTF-8 编码
  - `XRT_CP_UTF8 | XRT_CP_BOM` - UTF-8 带 BOM
  - 其他字符集...

**返回值：**
- 成功：文件对象指针 `xfile`
- 失败：`NULL`（错误信息存储在 `xCore.LastError`）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 打开文件读写（自动检测编码）
    xfile f = xrtOpen((str)"test.txt", FALSE, XRT_CP_AUTO);
    if (f) {
        printf("文件打开成功\n");
        printf("检测到的字符集: %d\n", f->Charset);
        printf("BOM 长度: %d\n", f->BOM);
        xrtClose(f);
    } else {
        printf("打开失败: %s\n", xCore.LastError);
    }
    
    // 只读打开 UTF-8 文件
    xfile f2 = xrtOpen((str)"config.ini", TRUE, XRT_CP_UTF8);
    if (f2) {
        // 读取操作...
        xrtClose(f2);
    }
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 空文件默认使用 UTF-8 编码
- 指定 BOM 标志时，写模式会自动写入 BOM
- 最多读取 64KB 数据用于编码推测

---

### xrtClose

关闭文件。

**函数原型：**
```c
XXAPI void xrtClose(xfile objFile);
```

**参数：**
- `objFile` - 文件对象

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xfile f = xrtOpen((str)"test.txt", FALSE, XRT_CP_UTF8);
    if (f) {
        xrtWrite(f, (str)"Hello World", 0);
        xrtClose(f);  // 关闭文件，释放资源
    }
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 传入 `NULL` 安全，不会崩溃
- 内部会释放 `xfile` 结构体内存

---

## 文件定位

### xrtSeek

设置文件游标位置。

**函数原型：**
```c
XXAPI size_t xrtSeek(xfile objFile, long iOffset, int iMoveMethod);
```

**参数：**
- `objFile` - 文件对象
- `iOffset` - 偏移量（字节）
- `iMoveMethod` - 起始位置
  - `XRT_SEEK_SET` (0) - 文件开头
  - `XRT_SEEK_CUR` (1) - 当前位置
  - `XRT_SEEK_END` (2) - 文件末尾

**返回值：**
- 新的游标位置

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xfile f = xrtOpen((str)"data.bin", TRUE, -1);  // -1 表示二进制
    if (f) {
        // 移动到文件开头
        xrtSeek(f, 0, XRT_SEEK_SET);
        
        // 移动到文件末尾
        xrtSeek(f, 0, XRT_SEEK_END);
        
        // 从当前位置后移 10 字节
        xrtSeek(f, 10, XRT_SEEK_CUR);
        
        // 从当前位置前移 5 字节
        xrtSeek(f, -5, XRT_SEEK_CUR);
        
        xrtClose(f);
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtTell

获取当前游标位置。

**函数原型：**
```c
XXAPI size_t xrtTell(xfile objFile);
```

**参数：**
- `objFile` - 文件对象

**返回值：**
- 当前游标位置（字节偏移）

---

### xrtGetEOF

获取文件末尾位置（文件大小）。

**函数原型：**
```c
XXAPI size_t xrtGetEOF(xfile objFile);
```

**参数：**
- `objFile` - 文件对象

**返回值：**
- 文件大小（字节）

---

### xrtEOF

检查是否已读取到文件末尾。

**函数原型：**
```c
XXAPI bool xrtEOF(xfile objFile);
```

**参数：**
- `objFile` - 文件对象

**返回值：**
- `TRUE` - 已到末尾
- `FALSE` - 未到末尾

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xfile f = xrtOpen((str)"data.txt", TRUE, XRT_CP_UTF8);
    if (f) {
        while (!xrtEOF(f)) {
            str chunk = xrtRead(f, 1024);
            if (chunk && chunk != xCore.sNull) {
                printf("%s", chunk);
                xrtFree(chunk);
            }
        }
        xrtClose(f);
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtSetEOF

设置文件末尾（截断文件）。

**函数原型：**
```c
XXAPI bool xrtSetEOF(xfile objFile);
```

**参数：**
- `objFile` - 文件对象

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败

**说明：**
- 将文件截断为当前游标位置

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xfile f = xrtOpen((str)"test.txt", FALSE, XRT_CP_UTF8);
    if (f) {
        xrtWrite(f, (str)"Hello World", 0);  // 写入 11 字节
        xrtSeek(f, 5, XRT_SEEK_SET);          // 移动到第 5 字节
        xrtSetEOF(f);                          // 截断文件，保留 "Hello"
        xrtClose(f);
    }
    
    xrtUnit();
    return 0;
}
```

---

## 文件读写

### xrtRead

从文件读取文本数据。

**函数原型：**
```c
XXAPI str xrtRead(xfile objFile, size_t iSize);
```

**参数：**
- `objFile` - 文件对象
- `iSize` - 要读取的字节数

**返回值：**
- 读取的文本字符串（已转换为 UTF-8）
- 实际读取长度存储在 `xCore.iRet`
- 失败返回 `xCore.sNull`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xfile f = xrtOpen((str)"data.txt", TRUE, XRT_CP_AUTO);
    if (f) {
        str text = xrtRead(f, 100);
        if (text && text != xCore.sNull) {
            printf("读取了 %" PRId64 " 字节: %s\n", xCore.iRet, text);
            xrtFree(text);
        }
        xrtClose(f);
    }
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 会根据文件字符集自动转换为 UTF-8
- 内部保留 4 字节用于字符串结尾（支持 UTF-32）

---

### xrtWrite

向文件写入文本数据。

**函数原型：**
```c
XXAPI size_t xrtWrite(xfile objFile, str sText, size_t iSize);
```

**参数：**
- `objFile` - 文件对象
- `sText` - 要写入的字符串（UTF-8）
- `iSize` - 字节数（0 表示自动计算）

**返回值：**
- 实际写入的字节数

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xfile f = xrtOpen((str)"output.txt", FALSE, XRT_CP_UTF8);
    if (f) {
        size_t written = xrtWrite(f, (str)"Hello World\n", 0);
        printf("写入了 %zu 字节\n", written);
        xrtClose(f);
    }
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 输入必须是 UTF-8，会自动转换为文件字符集

---

### xrtGet

从文件读取二进制数据。

**函数原型：**
```c
XXAPI ptr xrtGet(xfile objFile, size_t iSize);
```

**参数：**
- `objFile` - 文件对象
- `iSize` - 要读取的字节数

**返回值：**
- 二进制数据指针
- 实际读取长度存储在 `xCore.iRet`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xfile f = xrtOpen((str)"image.bin", TRUE, -1);  // -1 表示二进制
    if (f) {
        ptr data = xrtGet(f, 1024);
        if (data && data != xCore.sNull) {
            printf("读取了 %" PRId64 " 字节\n", xCore.iRet);
            // 处理二进制数据...
            xrtFree(data);
        }
        xrtClose(f);
    }
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 不进行字符集转换，用于读取二进制文件

---

### xrtPut

向文件写入二进制数据。

**函数原型：**
```c
XXAPI int xrtPut(xfile objFile, ptr pBuff, size_t iSize);
```

**参数：**
- `objFile` - 文件对象
- `pBuff` - 二进制数据指针
- `iSize` - 字节数

**返回值：**
- 实际写入的字节数

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xfile f = xrtOpen((str)"data.bin", FALSE, -1);
    if (f) {
        uint8 buffer[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
        int written = xrtPut(f, buffer, 16);
        printf("写入了 %d 字节\n", written);
        xrtClose(f);
    }
    
    xrtUnit();
    return 0;
}
```

---

## 快捷读写

### xrtFileReadAll

读取整个文件内容。

**函数原型：**
```c
XXAPI str xrtFileReadAll(str sPath, int iCharset);
```

**参数：**
- `sPath` - 文件路径
- `iCharset` - 字符集（`XRT_CP_AUTO` 自动检测）

**返回值：**
- 文件内容（UTF-8 编码）
- 文件大小存储在 `xCore.iRet`
- 失败返回 `xCore.sNull`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str content = xrtFileReadAll((str)"config.ini", XRT_CP_AUTO);
    if (content && content != xCore.sNull) {
        printf("文件大小: %" PRId64 " 字节\n", xCore.iRet);
        printf("内容:\n%s\n", content);
        xrtFree(content);
    } else {
        printf("读取失败\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtFileWriteAll

写入并覆盖整个文件内容。

**函数原型：**
```c
XXAPI int xrtFileWriteAll(str sPath, str sText, size_t iSize, int iCharset);
```

**参数：**
- `sPath` - 文件路径
- `sText` - 要写入的内容（UTF-8）
- `iSize` - 长度（0 自动计算）
- `iCharset` - 目标字符集

**返回值：**
- 成功：写入字节数
- 失败：0

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str data = (str)"[config]\nname=test\nvalue=123";
    int ok = xrtFileWriteAll((str)"config.ini", data, 0, XRT_CP_UTF8);
    if (ok) {
        printf("写入成功: %d 字节\n", ok);
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtFileAppend

向文件追加写入数据。

**函数原型：**
```c
XXAPI int xrtFileAppend(str sPath, str sText, size_t iSize, int iCharset);
```

**参数：**
- `sPath` - 文件路径
- `sText` - 要追加的内容（UTF-8）
- `iSize` - 长度（0 自动计算）
- `iCharset` - 目标字符集

**返回值：**
- 成功：写入字节数
- 失败：0

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

void AppendLog(str message) {
    str line = xrtFormat((str)"[%s] %s\n", xrtNowStr(), message);
    xrtFileAppend((str)"app.log", line, 0, XRT_CP_UTF8);
    xrtFree(line);
}

int main() {
    xrtInit();
    
    AppendLog((str)"应用程序启动");
    AppendLog((str)"正在处理...");
    AppendLog((str)"应用程序结束");
    
    xrtUnit();
    return 0;
}
```

---

### xrtFileGetAll

读取整个文件为二进制数据。

**函数原型：**
```c
XXAPI ptr xrtFileGetAll(str sPath);
```

**参数：**
- `sPath` - 文件路径

**返回值：**
- 二进制数据指针
- 文件大小存储在 `xCore.iRet`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    ptr data = xrtFileGetAll((str)"image.png");
    if (data && data != xCore.sNull) {
        printf("文件大小: %" PRId64 " 字节\n", xCore.iRet);
        // 处理二进制数据...
        xrtFree(data);
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtFilePutAll

写入二进制数据到文件。

**函数原型：**
```c
XXAPI int xrtFilePutAll(str sPath, ptr pBuff, size_t iSize);
```

**参数：**
- `sPath` - 文件路径
- `pBuff` - 二进制数据指针
- `iSize` - 字节数

**返回值：**
- 成功：写入字节数
- 失败：0

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    uint8 header[4] = {0x89, 0x50, 0x4E, 0x47};  // PNG 魔数
    int ok = xrtFilePutAll((str)"header.bin", header, 4);
    printf("写入结果: %d\n", ok);
    
    xrtUnit();
    return 0;
}
```

---

## 文件信息

### xrtPathExists

判断路径是否存在（文件或目录）。

**函数原型：**
```c
XXAPI bool xrtPathExists(str sPath);
```

**参数：**
- `sPath` - 路径

**返回值：**
- `TRUE` - 存在
- `FALSE` - 不存在

---

### xrtFileExists

判断文件是否存在。

**函数原型：**
```c
XXAPI bool xrtFileExists(str sPath);
```

**参数：**
- `sPath` - 文件路径

**返回值：**
- `TRUE` - 文件存在
- `FALSE` - 文件不存在或是目录

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    if (xrtFileExists((str)"config.ini")) {
        printf("配置文件存在\n");
    } else {
        printf("配置文件不存在\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

### xrtDirExists

判断目录是否存在。

**函数原型：**
```c
XXAPI bool xrtDirExists(str sPath);
```

**参数：**
- `sPath` - 目录路径

**返回值：**
- `TRUE` - 目录存在
- `FALSE` - 目录不存在或是文件

---

### xrtFileGetSize

获取文件大小。

**函数原型：**
```c
XXAPI size_t xrtFileGetSize(str sPath);
```

**参数：**
- `sPath` - 文件路径

**返回值：**
- 文件大小（字节）
- 失败返回 0

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    size_t size = xrtFileGetSize((str)"data.bin");
    printf("文件大小: %zu 字节\n", size);
    
    xrtUnit();
    return 0;
}
```

---

### xrtFileSetSize

设置文件大小。

**函数原型：**
```c
XXAPI bool xrtFileSetSize(str sPath, size_t iSize);
```

**参数：**
- `sPath` - 文件路径
- `iSize` - 新大小（字节）

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败

**说明：**
- 可用于截断文件或预分配文件空间

---

### xrtFileGetAttr

获取文件属性。

**函数原型：**
```c
XXAPI int xrtFileGetAttr(str sPath);
```

**参数：**
- `sPath` - 文件路径

**返回值：**
- Windows：`GetFileAttributes` 返回值
- Linux：`stat.st_mode` 值

---

### xrtFileSetAttr

设置文件属性。

**函数原型：**
```c
XXAPI bool xrtFileSetAttr(str sPath, int iAttr);
```

**参数：**
- `sPath` - 文件路径
- `iAttr` - 新属性值

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败

---

### xrtFileGetAccessTime

获取文件最后访问时间。

**函数原型：**
```c
XXAPI int64 xrtFileGetAccessTime(str sPath);
```

**参数：**
- `sPath` - 文件路径

**返回值：**
- XRT 时间戳（可用 `xrtTimeToStr` 转换）
- 失败返回 0

---

### xrtFileGetChangeTime

获取文件修改时间。

**函数原型：**
```c
XXAPI int64 xrtFileGetChangeTime(str sPath);
```

**参数：**
- `sPath` - 文件路径

**返回值：**
- XRT 时间戳
- 失败返回 0

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    int64 mtime = xrtFileGetChangeTime((str)"test.txt");
    if (mtime) {
        str timeStr = xrtTimeToStr(mtime, XRT_TIME_FORMAT_DATETIME);
        printf("修改时间: %s\n", timeStr);
        xrtFree(timeStr);
    }
    
    xrtUnit();
    return 0;
}
```

---

## 文件操作

### xrtFileCopy

复制文件。

**函数原型：**
```c
XXAPI bool xrtFileCopy(str sSrc, str sDst, bool bReWrite);
```

**参数：**
- `sSrc` - 源文件路径
- `sDst` - 目标文件路径
- `bReWrite` - 是否覆盖已存在的目标文件

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 复制文件，覆盖已存在的
    bool ok = xrtFileCopy((str)"source.txt", (str)"backup.txt", TRUE);
    printf("复制结果: %s\n", ok ? "成功" : "失败");
    
    xrtUnit();
    return 0;
}
```

---

### xrtFileMove

移动或重命名文件。

**函数原型：**
```c
XXAPI bool xrtFileMove(str sSrc, str sDst, bool bReWrite);
```

**参数：**
- `sSrc` - 源文件路径
- `sDst` - 目标文件路径
- `bReWrite` - 是否覆盖已存在的目标文件

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败

**补充说明：**
- 跨分区移动时会自动使用 复制+删除 实现

---

### xrtFileDelete

删除文件。

**函数原型：**
```c
XXAPI bool xrtFileDelete(str sPath);
```

**参数：**
- `sPath` - 文件路径

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    if (xrtFileExists((str)"temp.txt")) {
        bool ok = xrtFileDelete((str)"temp.txt");
        printf("删除结果: %s\n", ok ? "成功" : "失败");
    }
    
    xrtUnit();
    return 0;
}
```

---

## 目录操作

### xrtDirScan

扫描文件夹。

**函数原型：**
```c
XXAPI int xrtDirScan(str sPath, int bRecu, ptr pProc, ptr Param);
```

**参数：**
- `sPath` - 目录路径
- `bRecu` - 是否递归扫描子目录
- `pProc` - 回调函数
- `Param` - 回调参数

**回调函数原型：**
```c
int callback(str sPath, size_t iSize, int bDir, ptr pData, ptr Param);
```

**回调参数说明：**
- `sPath` - 文件/目录路径
- `iSize` - 路径长度
- `bDir` - 类型标识
  - `0` - 文件
  - `1` - 目录（进入时）
  - `2` - 目录（离开时）
- `pData` - 平台相关数据（Windows: `WIN32_FIND_DATAW*`，Linux: `struct dirent*`）
- `Param` - 用户参数

**返回值：**
- 扫描到的文件数量

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int OnFile(str sPath, size_t iSize, int bDir, ptr pData, ptr Param) {
    int* count = (int*)Param;
    if (bDir == 0) {
        printf("文件: %s\n", sPath);
        (*count)++;
    } else if (bDir == 1) {
        printf("进入目录: %s\n", sPath);
    } else if (bDir == 2) {
        printf("离开目录: %s\n", sPath);
    }
    return FALSE;  // 返回 TRUE 停止扫描
}

int main() {
    xrtInit();
    
    int fileCount = 0;
    int total = xrtDirScan((str)".", TRUE, OnFile, &fileCount);
    printf("总共扫描到 %d 个文件\n", total);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDirCreate

创建文件夹。

**函数原型：**
```c
XXAPI bool xrtDirCreate(str sPath);
```

**参数：**
- `sPath` - 目录路径

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败（目录已存在或父目录不存在）

---

### xrtDirCreateAll

创建多级文件夹。

**函数原型：**
```c
XXAPI bool xrtDirCreateAll(str sPath);
```

**参数：**
- `sPath` - 目录路径

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 创建多级目录
    bool ok = xrtDirCreateAll((str)"data/cache/temp");
    printf("创建结果: %s\n", ok ? "成功" : "失败");
    
    xrtUnit();
    return 0;
}
```

---

### xrtDirCopy

复制文件夹。

**函数原型：**
```c
XXAPI int xrtDirCopy(str sSrc, str sDst, bool bReWrite);
```

**参数：**
- `sSrc` - 源目录路径
- `sDst` - 目标目录路径
- `bReWrite` - 是否覆盖已存在的文件

**返回值：**
- 复制的文件数量

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    int count = xrtDirCopy((str)"project", (str)"backup/project", TRUE);
    printf("复制了 %d 个文件\n", count);
    
    xrtUnit();
    return 0;
}
```

---

### xrtDirMove

移动文件夹。

**函数原型：**
```c
XXAPI int xrtDirMove(str sSrc, str sDst, bool bReWrite);
```

**参数：**
- `sSrc` - 源目录路径
- `sDst` - 目标目录路径
- `bReWrite` - 是否覆盖已存在的文件

**返回值：**
- 移动的文件数量

---

### xrtDirDelete

删除文件夹（包含所有内容）。

**函数原型：**
```c
XXAPI int xrtDirDelete(str sPath);
```

**参数：**
- `sPath` - 目录路径

**返回值：**
- 删除的文件数量

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    if (xrtDirExists((str)"temp")) {
        int count = xrtDirDelete((str)"temp");
        printf("删除了 %d 个文件\n", count);
    }
    
    xrtUnit();
    return 0;
}
```

**警告：**
- 该函数会递归删除所有内容，请谨慎使用

---

## 使用场景

### 1. 配置文件读取

```c
#include "xrt.h"
#include <stdio.h>

str LoadConfig(str path) {
    if (!xrtFileExists(path)) {
        return NULL;
    }
    return xrtFileReadAll(path, XRT_CP_UTF8);
}

int main() {
    xrtInit();
    
    str config = LoadConfig((str)"config.ini");
    if (config) {
        printf("配置内容:\n%s\n", config);
        xrtFree(config);
    } else {
        printf("配置文件不存在\n");
    }
    
    xrtUnit();
    return 0;
}
```

---

### 2. 日志文件追加写入

```c
#include "xrt.h"
#include <stdio.h>

void AppendLog(str message) {
    str timeStr = xrtTimeToStr(xrtNow(), XRT_TIME_FORMAT_DATETIME);
    str line = xrtFormat((str)"[%s] %s\n", timeStr, message);
    xrtFileAppend((str)"app.log", line, 0, XRT_CP_UTF8);
    xrtFree(line);
    xrtFree(timeStr);
}

int main() {
    xrtInit();
    
    AppendLog((str)"应用程序启动");
    AppendLog((str)"正在处理数据...");
    AppendLog((str)"应用程序结束");
    
    printf("日志已写入 app.log\n");
    
    xrtUnit();
    return 0;
}
```

---

### 3. 批量文件处理

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int fileCount;
    int64 totalSize;
} ScanInfo;

int OnFile(str sPath, size_t iSize, int bDir, ptr pData, ptr Param) {
    ScanInfo* info = (ScanInfo*)Param;
    
    if (bDir == 0) {  // 文件
        info->fileCount++;
        info->totalSize += xrtFileGetSize(sPath);
        printf("文件: %s\n", sPath);
    } else if (bDir == 1) {  // 进入目录
        printf("进入: %s\n", sPath);
    }
    
    return FALSE;  // 继续扫描
}

int main() {
    xrtInit();
    
    ScanInfo info = {0, 0};
    int total = xrtDirScan((str)".", TRUE, OnFile, &info);
    
    printf("\n扫描完成：\n");
    printf("文件数: %d\n", info.fileCount);
    printf("总大小: %" PRId64 " 字节\n", info.totalSize);
    
    xrtUnit();
    return 0;
}
```

---

### 4. 文件备份

```c
#include "xrt.h"
#include <stdio.h>

int BackupFile(str srcPath) {
    if (!xrtFileExists(srcPath)) {
        printf("源文件不存在: %s\n", srcPath);
        return FALSE;
    }
    
    // 生成备份文件名
    str timeStr = xrtTimeToStr(xrtNow(), XRT_TIME_FORMAT_COMPACT);
    str backupPath = xrtFormat((str)"%s.%s.bak", srcPath, timeStr);
    xrtFree(timeStr);
    
    // 复制文件
    bool ok = xrtFileCopy(srcPath, backupPath, FALSE);
    if (ok) {
        printf("备份成功: %s\n", backupPath);
    } else {
        printf("备份失败\n");
    }
    
    xrtFree(backupPath);
    return ok;
}

int main() {
    xrtInit();
    
    // 创建测试文件
    xrtFileWriteAll((str)"test.txt", (str)"测试内容", 0, XRT_CP_UTF8);
    
    // 备份文件
    BackupFile((str)"test.txt");
    
    xrtUnit();
    return 0;
}
```

---

### 5. 二进制文件读写

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int magic;
    int version;
    int dataSize;
} FileHeader;

int main() {
    xrtInit();
    
    // 写入二进制文件
    xfile f = xrtOpen((str)"data.bin", FALSE, -1);  // -1 表示二进制模式
    if (f) {
        FileHeader header = { 0x12345678, 1, 100 };
        xrtPut(f, &header, sizeof(FileHeader));
        
        // 写入数据
        uint8 data[100];
        for (int i = 0; i < 100; i++) data[i] = (uint8)i;
        xrtPut(f, data, 100);
        
        xrtClose(f);
        printf("写入完成\n");
    }
    
    // 读取二进制文件
    f = xrtOpen((str)"data.bin", TRUE, -1);
    if (f) {
        FileHeader header;
        ptr headerData = xrtGet(f, sizeof(FileHeader));
        if (headerData) {
            memcpy(&header, headerData, sizeof(FileHeader));
            xrtFree(headerData);
            printf("Magic: 0x%X\n", header.magic);
            printf("Version: %d\n", header.version);
            printf("DataSize: %d\n", header.dataSize);
        }
        xrtClose(f);
    }
    
    xrtUnit();
    return 0;
}
```

---

## 最佳实践

### 1. 始终检查文件操作结果

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xfile f = xrtOpen((str)"important.txt", FALSE, XRT_CP_UTF8);
    if (!f) {
        printf("打开文件失败: %s\n", xCore.LastError);
        xrtUnit();
        return 1;
    }
    
    size_t written = xrtWrite(f, (str)"重要数据", 0);
    if (written == 0) {
        printf("写入失败: %s\n", xCore.LastError);
    }
    
    xrtClose(f);
    xrtUnit();
    return 0;
}
```

---

### 2. 使用临时文件进行安全写入

```c
#include "xrt.h"
#include <stdio.h>

int SafeWriteFile(str path, str content) {
    // 写入临时文件
    str tempPath = xrtPathRandom((str)".", (str)".tmp");
    int ok = xrtFileWriteAll(tempPath, content, 0, XRT_CP_UTF8);
    
    if (ok) {
        // 删除旧文件并重命名
        if (xrtFileExists(path)) {
            xrtFileDelete(path);
        }
        ok = xrtFileMove(tempPath, path, TRUE);
    }
    
    // 清理临时文件
    if (xrtFileExists(tempPath)) {
        xrtFileDelete(tempPath);
    }
    
    xrtFree(tempPath);
    return ok;
}

int main() {
    xrtInit();
    
    int ok = SafeWriteFile((str)"config.ini", (str)"[config]\nkey=value");
    printf("安全写入: %s\n", ok ? "成功" : "失败");
    
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [Path 路径处理](api-path.md)
- [Charset 字符集转换](api-charset.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#file-文件操作库)

</div>
