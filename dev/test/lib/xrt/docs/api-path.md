# Path 路径处理库

> 文件路径操作和解析功能

[English](api-path.en.md) | [中文](api-path.md) | [返回索引](README.md)

---

## 📑 目录

- [路径信息](#路径信息)
- [路径判断](#路径判断)
- [路径操作](#路径操作)
- [使用场景](#使用场景)
- [最佳实践](#最佳实践)

---

## 路径信息

### xrtPathGetDir

获取路径的目录部分。

**函数原型：**
```c
XXAPI str xrtPathGetDir(str sPath, size_t iSize);
```

**参数：**
- `sPath` - 文件路径
- `iSize` - 长度（0表示自动计算）

**返回值：**
- 目录路径（不含末尾分隔符和文件名）
- 目录长度存储在 `xCore.iRet`
- 如果路径中没有目录分隔符，返回 `xCore.sNull`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Windows 路径
    str dir1 = xrtPathGetDir((str)"C:\\folder\\file.txt", 0);
    printf("目录: %s\n", dir1);  // "C:\folder"
    printf("长度: %" PRId64 "\n", xCore.iRet);  // 9
    xrtFree(dir1);
    
    // Linux 路径
    str dir2 = xrtPathGetDir((str)"/home/user/file.txt", 0);
    printf("目录: %s\n", dir2);  // "/home/user"
    xrtFree(dir2);
    
    // 末尾有分隔符的路径
    str dir3 = xrtPathGetDir((str)"C:\\folder\\", 0);
    printf("目录: %s\n", dir3);  // "C:\folder"
    xrtFree(dir3);
    
    // 纯文件名（无目录）
    str dir4 = xrtPathGetDir((str)"file.txt", 0);
    if (dir4 == xCore.sNull) {
        printf("无目录部分\n");
    }
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 同时支持 `/` 和 `\\` 作为路径分隔符
- 不包含末尾的分隔符

---

### xrtPathGetNameExt

获取文件名（含扩展名）。

**函数原型：**
```c
XXAPI str xrtPathGetNameExt(str sPath, size_t iSize);
```

**参数：**
- `sPath` - 文件路径
- `iSize` - 长度（0表示自动计算）

**返回值：**
- 文件名字符串（含扩展名）
- 文件名长度存储在 `xCore.iRet`
- 如果路径以分隔符结尾，返回 `xCore.sNull`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 获取文件名 + 扩展名
    str file1 = xrtPathGetNameExt((str)"C:\\folder\\file.txt", 0);
    printf("文件名: %s\n", file1);  // "file.txt"
    printf("长度: %" PRId64 "\n", xCore.iRet);  // 8
    xrtFree(file1);
    
    // Linux 路径
    str file2 = xrtPathGetNameExt((str)"/home/user/document.pdf", 0);
    printf("文件名: %s\n", file2);  // "document.pdf"
    xrtFree(file2);
    
    // 纯文件名（无路径）
    str file3 = xrtPathGetNameExt((str)"readme.md", 0);
    printf("文件名: %s\n", file3);  // "readme.md"
    xrtFree(file3);
    
    xrtUnit();
    return 0;
}
```

---

### xrtPathGetName

获取文件名（不含扩展名）。

**函数原型：**
```c
XXAPI str xrtPathGetName(str sPath, size_t iSize);
```

**参数：**
- `sPath` - 文件路径
- `iSize` - 长度（0表示自动计算）

**返回值：**
- 文件名字符串（不含扩展名）
- 文件名长度存储在 `xCore.iRet`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 获取不含扩展名的文件名
    str name1 = xrtPathGetName((str)"C:\\folder\\file.txt", 0);
    printf("文件名: %s\n", name1);  // "file"
    printf("长度: %" PRId64 "\n", xCore.iRet);  // 4
    xrtFree(name1);
    
    // 无扩展名的文件
    str name2 = xrtPathGetName((str)"Makefile", 0);
    printf("文件名: %s\n", name2);  // "Makefile"
    xrtFree(name2);
    
    // 多个点号的文件名
    str name3 = xrtPathGetName((str)"archive.tar.gz", 0);
    printf("文件名: %s\n", name3);  // "archive.tar"
    xrtFree(name3);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 只移除最后一个 `.` 及其后的扩展名

---

### xrtPathGetExt

获取文件扩展名。

**函数原型：**
```c
XXAPI str xrtPathGetExt(str sPath, size_t iSize);
```

**参数：**
- `sPath` - 文件路径
- `iSize` - 长度（0表示自动计算）

**返回值：**
- 扩展名字符串（**不含** `.`）
- 扩展名长度存储在 `xCore.iRet`
- 如果无扩展名，返回 `xCore.sNull`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 获取扩展名
    str ext1 = xrtPathGetExt((str)"file.txt", 0);
    printf("扩展名: %s\n", ext1);  // "txt"（注意：不含点号）
    printf("长度: %" PRId64 "\n", xCore.iRet);  // 3
    xrtFree(ext1);
    
    // 多个点号的文件
    str ext2 = xrtPathGetExt((str)"archive.tar.gz", 0);
    printf("扩展名: %s\n", ext2);  // "gz"
    xrtFree(ext2);
    
    // 无扩展名的文件
    str ext3 = xrtPathGetExt((str)"Makefile", 0);
    if (ext3 == xCore.sNull) {
        printf("无扩展名\n");
    }
    
    // 路径中含点，但文件无扩展名
    str ext4 = xrtPathGetExt((str)"/home/user.name/config", 0);
    if (ext4 == xCore.sNull) {
        printf("目录中的点不算扩展名\n");
    }
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 返回的扩展名**不含**前导点号 `.`
- 如果点号出现在路径分隔符之前，则认为是目录名中的点，不是扩展名

---

## 路径判断

### xrtPathIsAbs

判断是否为绝对路径。

**函数原型：**
```c
XXAPI bool xrtPathIsAbs(str sPath, size_t iSize);
```

**参数：**
- `sPath` - 文件路径
- `iSize` - 长度（0表示自动计算）

**返回值：**
- `TRUE` - 绝对路径
- `FALSE` - 相对路径

**判断规则：**
- Linux/macOS：以 `/` 开头为绝对路径
- Windows：含有 `:` 为绝对路径（如 `C:\`）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // Windows 绝对路径
    printf("C:\\folder: %s\n", 
        xrtPathIsAbs((str)"C:\\folder", 0) ? "绝对" : "相对");  // 绝对
    
    // Linux 绝对路径
    printf("/home/user: %s\n", 
        xrtPathIsAbs((str)"/home/user", 0) ? "绝对" : "相对");  // 绝对
    
    // 相对路径
    printf("./folder: %s\n", 
        xrtPathIsAbs((str)"./folder", 0) ? "绝对" : "相对");  // 相对
    printf("folder/file: %s\n", 
        xrtPathIsAbs((str)"folder/file", 0) ? "绝对" : "相对");  // 相对
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 该函数是跨平台的，同时支持 Windows 和 Unix 风格的路径判断

---

## 路径操作

### xrtPathJoin

拼接多个路径段。

**函数原型：**
```c
XXAPI str xrtPathJoin(uint iCount, ...);
```

**参数：**
- `iCount` - 路径段数量
- `...` - 路径段（str 类型可变参数）

**返回值：**
- 拼接后的完整路径
- 路径长度存储在 `xCore.iRet`
- 如果路径超过 4094 字符，返回 `xCore.sNull`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 拼接多个路径段
    str path1 = xrtPathJoin(3, (str)"C:\\", (str)"folder", (str)"file.txt");
    printf("路径: %s\n", path1);  // Windows: "C:\folder\file.txt"
    printf("长度: %" PRId64 "\n", xCore.iRet);
    xrtFree(path1);
    
    // Linux 路径
    str path2 = xrtPathJoin(3, (str)"/home", (str)"user", (str)"document.pdf");
    printf("路径: %s\n", path2);  // "/home/user/document.pdf"
    xrtFree(path2);
    
    // 使用程序路径
    str config = xrtPathJoin(3, xCore.AppPath, (str)"config", (str)"app.ini");
    printf("配置文件: %s\n", config);
    xrtFree(config);
    
    // 路径段已含分隔符（不会重复添加）
    str path3 = xrtPathJoin(2, (str)"C:\\folder\\", (str)"file.txt");
    printf("路径: %s\n", path3);  // "C:\folder\file.txt"
    xrtFree(path3);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 自动在路径段之间添加分隔符（Windows 用 `\`，Linux 用 `/`）
- 如果路径段已以分隔符结尾，不会重复添加
- NULL 或空字符串的路径段会被跳过
- 最大支持 4094 字符的路径

---

### xrtPathRandom

生成随机路径字符串。

**函数原型：**
```c
XXAPI str xrtPathRandom(str sHead, size_t iHeadSize, str sFoot, size_t iFootSize, size_t iLen);
```

**参数：**
- `sHead` - 路径前缀（可为 NULL）
- `iHeadSize` - 前缀长度（0表示自动计算）
- `sFoot` - 路径后缀（可为 NULL，通常为扩展名）
- `iFootSize` - 后缀长度（0表示自动计算）
- `iLen` - 随机部分的长度

**返回值：**
- 格式为 `{sHead}{random}{sFoot}` 的路径字符串
- 路径长度存储在 `xCore.iRet`

**内存释放：** ✅ 需要 `xrtFree` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 生成临时文件名
    str temp1 = xrtPathRandom((str)"/tmp/", 0, (str)".tmp", 0, 8);
    printf("临时文件: %s\n", temp1);  // 例如: "/tmp/A3xK9mPq.tmp"
    xrtFree(temp1);
    
    // 生成随机文件夹名
    str folder = xrtPathRandom((str)"cache_", 0, NULL, 0, 6);
    printf("缓存文件夹: %s\n", folder);  // 例如: "cache_Bz7YnL"
    xrtFree(folder);
    
    // 生成唯一文件名
    str unique = xrtPathRandom(NULL, 0, (str)".dat", 0, 16);
    printf("唯一文件: %s\n", unique);  // 例如: "Kx9pM2nVqR5sT8wL.dat"
    xrtFree(unique);
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 随机部分使用字母数字（A-Z, a-z, 0-9），不含特殊字符
- 常用于生成临时文件名、缓存文件名等
- 不会检查文件是否已存在，需要调用者自行检查

---

## 使用场景

### 1. 路径解析

```c
#include "xrt.h"
#include <stdio.h>

void ParsePath(str path) {
    str dir = xrtPathGetDir(path, 0);
    str name = xrtPathGetName(path, 0);
    str ext = xrtPathGetExt(path, 0);
    
    printf("目录: %s\n", dir != xCore.sNull ? (char*)dir : "(无)");
    printf("文件名: %s\n", name != xCore.sNull ? (char*)name : "(无)");
    printf("扩展名: %s\n", ext != xCore.sNull ? (char*)ext : "(无)");
    
    if (dir != xCore.sNull) xrtFree(dir);
    if (name != xCore.sNull) xrtFree(name);
    if (ext != xCore.sNull) xrtFree(ext);
}

int main() {
    xrtInit();
    
    ParsePath((str)"C:\\folder\\file.txt");
    printf("---\n");
    ParsePath((str)"/home/user/document.pdf");
    
    xrtUnit();
    return 0;
}
```

---

### 2. 路径构建

```c
#include "xrt.h"
#include <stdio.h>

str BuildConfigPath() {
    return xrtPathJoin(3, xCore.AppPath, (str)"config", (str)"app.ini");
}

int main() {
    xrtInit();
    
    str config = BuildConfigPath();
    printf("配置文件: %s\n", config);
    xrtFree(config);
    
    xrtUnit();
    return 0;
}
```

---

### 3. 临时文件处理

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 创建临时文件路径
#if defined(_WIN32) || defined(_WIN64)
    str temp_dir = (str)"C:\\Temp\\";
#else
    str temp_dir = (str)"/tmp/";
#endif
    
    str temp_file = xrtPathRandom(temp_dir, 0, (str)".tmp", 0, 8);
    printf("临时文件: %s\n", temp_file);
    
    // 使用临时文件...
    
    xrtFree(temp_file);
    xrtUnit();
    return 0;
}
```

---

### 4. 路径安全检查

```c
#include "xrt.h"
#include <stdio.h>

bool IsValidFilePath(str path) {
    // 必须是绝对路径
    if (!xrtPathIsAbs(path, 0)) {
        printf("错误: 必须使用绝对路径\n");
        return FALSE;
    }
    
    // 必须有扩展名
    str ext = xrtPathGetExt(path, 0);
    if (ext == xCore.sNull) {
        printf("错误: 缺少文件扩展名\n");
        return FALSE;
    }
    xrtFree(ext);
    
    return TRUE;
}

int main() {
    xrtInit();
    
    printf("检查 C:\\test.txt: %s\n", 
        IsValidFilePath((str)"C:\\test.txt") ? "有效" : "无效");
    printf("检查 ./relative: %s\n", 
        IsValidFilePath((str)"./relative") ? "有效" : "无效");
    
    xrtUnit();
    return 0;
}
```

---

## 最佳实践

### 1. 检查返回值

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    str path = (str)"filename_without_dir";
    str dir = xrtPathGetDir(path, 0);
    
    // 始终检查返回值
    if (dir == xCore.sNull) {
        printf("路径不包含目录\n");
    } else {
        printf("目录: %s\n", dir);
        xrtFree(dir);
    }
    
    xrtUnit();
    return 0;
}
```

---

### 2. 跨平台路径处理

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 使用 xrtPathJoin 自动处理分隔符
    // 不要手动拼接路径字符串
    
    // ✓ 正确：使用 xrtPathJoin
    str good = xrtPathJoin(3, xCore.AppPath, (str)"data", (str)"file.txt");
    printf("正确: %s\n", good);
    xrtFree(good);
    
    // ✗ 不推荐：手动拼接
    // str bad = xrtFormat("%s\\data\\file.txt", xCore.AppPath);
    
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [File 文件操作](api-file.md)
- [String 字符串处理](api-string.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#path-路径处理库)

</div>
