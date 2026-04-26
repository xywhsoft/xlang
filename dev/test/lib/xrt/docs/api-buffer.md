# Buffer 动态缓冲区库

> 可变大小内存缓冲区，支持二进制数据和多种字符串编码

[English](api-buffer.en.md) | [中文](api-buffer.md) | [返回索引](README.md)

---

## 📑 目录

- [常量定义](#常量定义)
- [数据结构](#数据结构)
- [缓冲区操作](#缓冲区操作)
- [数据读写](#数据读写)
- [使用场景](#使用场景)
- [最佳实践](#最佳实践)

---

## 常量定义

### 字符串模式常量

| 常量 | 值 | 说明 |
|------|-----|------|
| `XBUF_BINARY` | `0` | 二进制模式，不自动添加终止符 |
| `XBUF_ANSI` | `1` | ANSI 字符串，追加 1 字节 `\0` |
| `XBUF_UTF8` | `1` | UTF-8 字符串，追加 1 字节 `\0` |
| `XBUF_UTF16` | `2` | UTF-16 字符串，追加 2 字节 `\0\0` |
| `XBUF_UTF32` | `4` | UTF-32 字符串，追加 4 字节 `\0\0\0\0` |

### 内存分配常量

| 常量 | 值 | 说明 |
|------|-----|------|
| `XBUFFER_ALLOC_STEP` | `0x10000` (64KB) | 默认预分配步长 |

---

## 数据结构

### xbuffer_struct

缓冲区管理器数据结构。

**定义：**
```c
typedef struct {
    char* Buffer;       // 内存缓冲区指针
    uint32 Length;      // 当前数据长度（字节）
    uint32 AllocLength; // 已分配内存长度（字节）
    uint32 AllocStep;   // 预分配内存步长
} xbuffer_struct, *xbuffer;
```

**成员说明：**
- `Buffer` - 实际数据存储区指针，可直接读取
- `Length` - 当前已写入的有效数据长度
- `AllocLength` - 已分配的总内存大小
- `AllocStep` - 扩容时的增量大小

---

## 缓冲区操作

### xrtBufferCreate

创建缓冲区管理器。

**函数原型：**
```c
XXAPI xbuffer xrtBufferCreate(uint32 iStep);
```

**参数：**
- `iStep` - 预分配步长（0 使用默认值 64KB）

**返回值：**
- 成功：缓冲区对象指针
- 失败：`NULL`

**内存释放：** ✅ 需要 `xrtBufferDestroy` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 使用默认步长
    xbuffer buf1 = xrtBufferCreate(0);
    printf("默认步长: %u\n", buf1->AllocStep);  // 65536
    
    // 自定义步长（适合小数据）
    xbuffer buf2 = xrtBufferCreate(1024);
    printf("自定义步长: %u\n", buf2->AllocStep);  // 1024
    
    xrtBufferDestroy(buf1);
    xrtBufferDestroy(buf2);
    
    xrtUnit();
    return 0;
}
```

---

### xrtBufferDestroy

销毁缓冲区管理器。

**函数原型：**
```c
XXAPI void xrtBufferDestroy(xbuffer pBuf);
```

**参数：**
- `pBuf` - 缓冲区对象指针

**说明：**
- 释放内部缓冲区内存和管理器结构体
- 传入 `NULL` 不会崩溃（安全检查）

---

### xrtBufferInit

初始化缓冲区管理器（用于栈上或嵌入式结构体）。

**函数原型：**
```c
XXAPI void xrtBufferInit(xbuffer pBuf, uint32 iStep);
```

**参数：**
- `pBuf` - 缓冲区结构体指针
- `iStep` - 预分配步长（0 使用默认值）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 栈上分配结构体
    xbuffer_struct buf;
    xrtBufferInit(&buf, 4096);  // 4KB 步长
    
    xrtBufferAppend(&buf, (ptr)"Hello", 5, XBUF_ANSI);
    printf("数据: %s\n", buf.Buffer);
    printf("长度: %u\n", buf.Length);
    
    xrtBufferUnit(&buf);  // 释放内部缓冲区，不释放结构体
    
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 与 `xrtBufferCreate` 的区别：不分配结构体内存
- 适用于将缓冲区嵌入其他结构体中

---

### xrtBufferUnit

清空缓冲区（释放内部内存，不释放结构体）。

**函数原型：**
```c
XXAPI void xrtBufferUnit(xbuffer pBuf);
```

**参数：**
- `pBuf` - 缓冲区对象指针

**说明：**
- 释放 `Buffer` 指向的内存
- 将 `Length` 和 `AllocLength` 置为 0
- 不释放 `xbuffer_struct` 本身

**别名：** `xrtBufferClear`

---

### xrtBufferMalloc

预分配或调整缓冲区内存大小。

**函数原型：**
```c
XXAPI bool xrtBufferMalloc(xbuffer pBuf, uint32 iCount);
```

**参数：**
- `pBuf` - 缓冲区对象
- `iCount` - 目标内存大小（字节）

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败（内存分配失败）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xbuffer buf = xrtBufferCreate(0);
    
    // 预分配 1MB 内存
    if (xrtBufferMalloc(buf, 1024 * 1024)) {
        printf("预分配成功: %u 字节\n", buf->AllocLength);
    }
    
    // 写入数据不会触发重新分配
    for (int i = 0; i < 1000; i++) {
        xrtBufferAppend(buf, (ptr)"data", 4, XBUF_BINARY);
    }
    printf("已用: %u 字节\n", buf->Length);  // 4000
    
    // 裁剪内存（释放多余空间）
    xrtBufferMalloc(buf, buf->Length);
    printf("裁剪后: %u 字节\n", buf->AllocLength);  // 4000
    
    xrtBufferDestroy(buf);
    xrtUnit();
    return 0;
}
```

**补充说明：**
- `iCount > AllocLength`：扩展内存
- `iCount < AllocLength`：裁剪内存
- `iCount = 0`：清空缓冲区

---

## 数据读写

### xrtBufferInsert

在指定位置插入数据。

**函数原型：**
```c
XXAPI bool xrtBufferInsert(xbuffer pBuf, uint32 iPos, ptr pData, uint32 iSize, uint32 bStrMode);
```

**参数：**
- `pBuf` - 缓冲区对象
- `iPos` - 插入位置（字节偏移）
- `pData` - 数据指针
- `iSize` - 数据大小（字节，0 表示自动计算字符串长度）
- `bStrMode` - 字符串模式（`XBUF_*` 常量）

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xbuffer buf = xrtBufferCreate(0);
    
    // 写入 "World"
    xrtBufferInsert(buf, 0, (ptr)"World", 5, XBUF_ANSI);
    printf("%s\n", buf->Buffer);  // "World"
    
    // 在开头插入 "Hello "
    xrtBufferInsert(buf, 0, (ptr)"Hello ", 6, XBUF_BINARY);
    printf("%s\n", buf->Buffer);  // "Hello "（注意：这里会覆盖）
    
    xrtBufferDestroy(buf);
    xrtUnit();
    return 0;
}
```

**补充说明：**
- `iSize = 0` 时，根据 `bStrMode` 自动计算长度
- `bStrMode > 0` 时，自动在末尾追加终止符
- 注意：不是真正的“插入”，而是从指定位置“覆盖”写入

---

### xrtBufferAppend

在缓冲区末尾追加数据。

**函数原型：**
```c
XXAPI bool xrtBufferAppend(xbuffer pBuf, ptr pData, uint32 iSize, uint32 bStrMode);
```

**参数：**
- `pBuf` - 缓冲区对象
- `pData` - 数据指针
- `iSize` - 数据大小（字节，0 表示自动计算）
- `bStrMode` - 字符串模式（`XBUF_*` 常量）

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xbuffer buf = xrtBufferCreate(1024);
    
    // 追加二进制数据
    uint32 header = 0x12345678;
    xrtBufferAppend(buf, &header, sizeof(header), XBUF_BINARY);
    printf("长度: %u\n", buf->Length);  // 4
    
    // 追加 ANSI 字符串
    xrtBufferAppend(buf, (ptr)"Hello", 5, XBUF_ANSI);
    printf("长度: %u\n", buf->Length);  // 9
    
    // 追加时自动计算长度
    xrtBufferAppend(buf, (ptr)" World", 0, XBUF_UTF8);
    printf("数据: %s\n", &buf->Buffer[4]);  // "Hello World"
    
    xrtBufferDestroy(buf);
    xrtUnit();
    return 0;
}
```

**补充说明：**
- 内部调用 `xrtBufferInsert(pBuf, pBuf->Length, ...)`
- 自动扩展缓冲区内存（按 `AllocStep` 增量）
- 字符串模式会自动追加终止符，但终止符不计入 `Length`

---

### 直接访问缓冲区数据

缓冲区数据可以通过结构体成员直接访问：

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xbuffer buf = xrtBufferCreate(0);
    xrtBufferAppend(buf, (ptr)"Hello World", 0, XBUF_UTF8);
    
    // 直接读取数据
    char* data = buf->Buffer;       // 数据指针
    uint32 len = buf->Length;       // 有效数据长度
    uint32 cap = buf->AllocLength;  // 已分配容量
    
    printf("数据: %s\n", data);
    printf("长度: %u\n", len);
    printf("容量: %u\n", cap);
    
    // 遍历二进制数据
    for (uint32 i = 0; i < len; i++) {
        printf("%02X ", (unsigned char)buf->Buffer[i]);
    }
    printf("\n");
    
    xrtBufferDestroy(buf);
    xrtUnit();
    return 0;
}
```

---

## 使用场景

### 1. 动态数据包构建

```c
#include "xrt.h"
#include <stdio.h>

xbuffer BuildPacket(uint32 cmd, ptr payload, uint32 payloadSize) {
    xbuffer buf = xrtBufferCreate(256);
    
    // 包头
    uint32 header = 0xAABBCCDD;
    xrtBufferAppend(buf, &header, 4, XBUF_BINARY);
    
    // 命令码
    xrtBufferAppend(buf, &cmd, 4, XBUF_BINARY);
    
    // 负载长度
    xrtBufferAppend(buf, &payloadSize, 4, XBUF_BINARY);
    
    // 负载数据
    if (payloadSize > 0) {
        xrtBufferAppend(buf, payload, payloadSize, XBUF_BINARY);
    }
    
    return buf;
}

int main() {
    xrtInit();
    
    str data = (str)"Hello, World!";
    xbuffer packet = BuildPacket(0x01, data, 13);
    
    printf("数据包大小: %u 字节\n", packet->Length);  // 25
    
    // 打印十六进制
    for (uint32 i = 0; i < packet->Length; i++) {
        printf("%02X ", (unsigned char)packet->Buffer[i]);
    }
    printf("\n");
    
    xrtBufferDestroy(packet);
    xrtUnit();
    return 0;
}
```

---

### 2. 字符串拼接

```c
#include "xrt.h"
#include <stdio.h>

str BuildSQL(str table, str* fields, int fieldCount) {
    xbuffer buf = xrtBufferCreate(256);
    
    xrtBufferAppend(buf, (ptr)"SELECT ", 0, XBUF_ANSI);
    
    for (int i = 0; i < fieldCount; i++) {
        if (i > 0) {
            xrtBufferAppend(buf, (ptr)", ", 2, XBUF_BINARY);
        }
        xrtBufferAppend(buf, fields[i], 0, XBUF_BINARY);
    }
    
    xrtBufferAppend(buf, (ptr)" FROM ", 6, XBUF_BINARY);
    xrtBufferAppend(buf, table, 0, XBUF_ANSI);
    
    // 复制结果（因为要销毁缓冲区）
    str result = xrtCopyStr((str)buf->Buffer, buf->Length);
    xrtBufferDestroy(buf);
    
    return result;
}

int main() {
    xrtInit();
    
    str fields[] = {(str)"id", (str)"name", (str)"age"};
    str sql = BuildSQL((str)"users", fields, 3);
    
    printf("SQL: %s\n", sql);  // "SELECT id, name, age FROM users"
    xrtFree(sql);
    
    xrtUnit();
    return 0;
}
```

---

### 3. 文件内容缓存

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xbuffer cache = xrtBufferCreate(0);
    
    // 模拟多次读取并缓存
    for (int i = 0; i < 5; i++) {
        str chunk = xrtFormat((str)"Chunk %d data. ", i);
        xrtBufferAppend(cache, chunk, 0, XBUF_BINARY);
        xrtFree(chunk);
    }
    
    // 添加终止符
    xrtBufferAppend(cache, (ptr)"\0", 1, XBUF_BINARY);
    
    printf("缓存内容: %s\n", cache->Buffer);
    printf("总大小: %u 字节\n", cache->Length);
    
    xrtBufferDestroy(cache);
    xrtUnit();
    return 0;
}
```

---

### 4. UTF-16 字符串缓冲区

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xbuffer buf = xrtBufferCreate(256);
    
    // 转换并追加 UTF-16 字符串
    u16str text1 = xrtUTF8to16((str)"你好", 0);
    xrtBufferAppend(buf, text1, 0, XBUF_UTF16);
    xrtFree(text1);
    
    u16str text2 = xrtUTF8to16((str)"世界", 0);
    // 覆盖终止符位置，不使用 XBUF_UTF16 避免重复追加
    xrtBufferInsert(buf, buf->Length, text2, u16len(text2) * 2, XBUF_UTF16);
    xrtFree(text2);
    
    printf("UTF-16 字节数: %u\n", buf->Length);
    
    // 转回 UTF-8 显示
    str utf8 = xrtUTF16to8((u16str)buf->Buffer, buf->Length / 2);
    printf("内容: %s\n", utf8);
    xrtFree(utf8);
    
    xrtBufferDestroy(buf);
    xrtUnit();
    return 0;
}
```

---

## 最佳实践

### 1. 预分配内存

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // ✗ 不推荐：频繁扩容
    xbuffer buf1 = xrtBufferCreate(64);  // 小步长
    for (int i = 0; i < 10000; i++) {
        xrtBufferAppend(buf1, (ptr)"data", 4, XBUF_BINARY);
    }
    printf("方式1完成\n");
    
    // ✓ 推荐：预分配已知大小
    xbuffer buf2 = xrtBufferCreate(0);
    xrtBufferMalloc(buf2, 10000 * 4);  // 预分配 40KB
    for (int i = 0; i < 10000; i++) {
        xrtBufferAppend(buf2, (ptr)"data", 4, XBUF_BINARY);
    }
    printf("方式2完成\n");
    
    xrtBufferDestroy(buf1);
    xrtBufferDestroy(buf2);
    xrtUnit();
    return 0;
}
```

---

### 2. 嵌入式缓冲区

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    str name;
    xbuffer_struct data;  // 嵌入式缓冲区
} MyObject;

MyObject* CreateObject(str name) {
    MyObject* obj = xrtMalloc(sizeof(MyObject));
    obj->name = xrtCopyStr(name, 0);
    xrtBufferInit(&obj->data, 1024);  // 初始化嵌入的缓冲区
    return obj;
}

void DestroyObject(MyObject* obj) {
    if (obj) {
        xrtFree(obj->name);
        xrtBufferUnit(&obj->data);  // 释放缓冲区内容
        xrtFree(obj);
    }
}

int main() {
    xrtInit();
    
    MyObject* obj = CreateObject((str)"TestObject");
    
    // 使用嵌入的缓冲区
    xrtBufferAppend(&obj->data, (ptr)"payload data", 0, XBUF_UTF8);
    printf("对象名: %s\n", obj->name);
    printf("数据: %s\n", obj->data.Buffer);
    
    DestroyObject(obj);
    xrtUnit();
    return 0;
}
```

---

### 3. 内存裁剪

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xbuffer buf = xrtBufferCreate(0);
    
    // 写入少量数据，但分配了大量内存
    xrtBufferAppend(buf, (ptr)"Small data", 10, XBUF_ANSI);
    printf("写入前 - 数据: %u, 分配: %u\n", buf->Length, buf->AllocLength);
    
    // 裁剪多余内存
    xrtBufferMalloc(buf, buf->Length + 1);  // +1 保留终止符
    printf("裁剪后 - 数据: %u, 分配: %u\n", buf->Length, buf->AllocLength);
    
    xrtBufferDestroy(buf);
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [Base 基础函数库](api-base.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#buffer-动态缓冲区库)

</div>
