# Array 结构体数组库

> 动态结构体数组，支持任意大小的结构体元素

[English](api-array.en.md) | [中文](api-array.md) | [返回索引](README.md)

---

## 📑 目录

- [常量定义](#常量定义)
- [数据结构](#数据结构)
- [数组操作](#数组操作)
- [元素访问](#元素访问)
- [使用场景](#使用场景)
- [最佳实践](#最佳实践)

---

## 常量定义

| 常量 | 值 | 说明 |
|------|-----|------|
| `XARRAY_PREASSIGNSTEP` | `256` | 默认预分配步长（元素个数） |

### 索引规则

```
成员编号规则（0为不存在的成员编号）：
┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬────┐
│01│02│03│04│05│06│07│08│09│10│11│12│ .. │
└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴────┘
```

**重要：** 索引从 **1** 开始，不是从 0 开始！

---

## 数据结构

### xarray_struct

结构体数组内存管理器数据结构。

**定义：**
```c
typedef struct {
    str Memory;         // 管理器内存指针
    uint32 ItemLength;  // 成员占用内存长度
    uint32 Count;       // 管理器中存在多少成员
    uint32 AllocCount;  // 已经申请的结构数量
    uint32 AllocStep;   // 预分配内存步长
} xarray_struct, *xarray;
```

**成员说明：**
- `Memory` - 连续内存块指针，存储所有元素
- `ItemLength` - 每个元素占用的字节数（由创建时指定）
- `Count` - 当前元素个数
- `AllocCount` - 已分配的容量（元素个数）
- `AllocStep` - 每次扩容增加的元素个数（默认 256）

---

## 数组操作

### xrtArrayCreate

创建结构体数组。

**函数原型：**
```c
XXAPI xarray xrtArrayCreate(uint32 iItemLength);
```

**参数：**
- `iItemLength` - 每个元素的大小（字节）

**返回值：**
- 成功：数组对象
- 失败：`NULL`

**内存释放：** ✅ 需要 `xrtArrayDestroy` 释放

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    char name[32];
    float score;
} Student;

int main() {
    xrtInit();
    
    // 创建学生数组
    xarray students = xrtArrayCreate(sizeof(Student));
    if (!students) {
        printf("创建失败\n");
        xrtUnit();
        return 1;
    }
    
    printf("元素大小: %u 字节\n", students->ItemLength);  // 40 或更多
    printf("当前数量: %u\n", students->Count);            // 0
    
    xrtArrayDestroy(students);
    xrtUnit();
    return 0;
}
```

---

### xrtArrayDestroy

销毁结构体数组，释放所有内存。

**函数原型：**
```c
XXAPI void xrtArrayDestroy(xarray pArr);
```

**参数：**
- `pArr` - 数组对象

**说明：**
- 同时释放内部内存和数组结构体
- 如果元素包含动态分配的内存，需要先手动释放

---

### xrtArrayInit

初始化数组的数据结构（用于内嵌数组的对象使用）。

**函数原型：**
```c
XXAPI void xrtArrayInit(xarray pArr, uint32 iItemLength);
```

**参数：**
- `pArr` - 数组对象指针
- `iItemLength` - 每个元素的大小（字节）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int value;
} Item;

typedef struct {
    char name[32];
    xarray_struct items;  // 内嵌数组
} Container;

int main() {
    xrtInit();
    
    Container cont;
    strcpy(cont.name, "MyContainer");
    
    // 初始化内嵌数组
    xrtArrayInit(&cont.items, sizeof(Item));
    
    // 使用数组
    uint32 pos = xrtArrayAppend(&cont.items, 1);
    Item* item = (Item*)xrtArrayGet(&cont.items, pos);
    item->value = 100;
    
    printf("%s: 有 %u 个元素\n", cont.name, cont.items.Count);
    
    // 只释放数组内部内存，不释放结构体本身
    xrtArrayUnit(&cont.items);
    
    xrtUnit();
    return 0;
}
```

---

### xrtArrayUnit

释放数组的数据结构（但不会释放数组结构体本身的内存）。

**函数原型：**
```c
XXAPI void xrtArrayUnit(xarray pArr);
```

**参数：**
- `pArr` - 数组对象

**说明：**
- 释放内部内存，重置 `Count` 和 `AllocCount` 为 0
- 不释放 `xarray` 结构体本身
- 与 `xrtArrayInit` 配对使用

**别名：**
- `xrtArrayRemoveAll` - 删除所有成员
- `xrtArrayClear` - 清空管理器

---

### xrtArrayAlloc

预分配或裁剪数组内存。

**函数原型：**
```c
XXAPI bool xrtArrayAlloc(xarray pArr, uint32 iCount);
```

**参数：**
- `pArr` - 数组对象
- `iCount` - 目标容量（元素个数）

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int x, y;
} Point;

int main() {
    xrtInit();
    
    xarray points = xrtArrayCreate(sizeof(Point));
    
    // 预分配 1000 个元素的空间
    xrtArrayAlloc(points, 1000);
    printf("已分配容量: %u\n", points->AllocCount);  // 1000
    printf("实际元素数: %u\n", points->Count);       // 0
    
    // 添加元素不会触发内存分配
    for (int i = 0; i < 100; i++) {
        uint32 pos = xrtArrayAppend(points, 1);
        Point* p = (Point*)xrtArrayGet(points, pos);
        p->x = i;
        p->y = i * 2;
    }
    printf("添加后容量: %u\n", points->AllocCount);  // 仍然 1000
    
    // 裁剪多余空间
    xrtArrayAlloc(points, points->Count);
    printf("裁剪后容量: %u\n", points->AllocCount);  // 100
    
    xrtArrayDestroy(points);
    xrtUnit();
    return 0;
}
```

---

### xrtArrayInsert

在指定位置插入元素。

**函数原型：**
```c
XXAPI uint32 xrtArrayInsert(xarray pArr, uint32 iPos, uint32 iCount);
```

**参数：**
- `pArr` - 数组对象
- `iPos` - 插入位置（0 = 头部插入，Count = 末尾插入）
- `iCount` - 要插入的元素数量（0 会被视为 1）

**返回值：**
- 成功：第一个新元素的索引（1-based）
- 失败：`0`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    char name[16];
} Item;

int main() {
    xrtInit();
    
    xarray arr = xrtArrayCreate(sizeof(Item));
    
    // 添加 B 和 D
    uint32 pos1 = xrtArrayAppend(arr, 1);
    strcpy(((Item*)xrtArrayGet(arr, pos1))->name, "B");
    
    uint32 pos2 = xrtArrayAppend(arr, 1);
    strcpy(((Item*)xrtArrayGet(arr, pos2))->name, "D");
    
    // 在头部插入 A
    uint32 pos3 = xrtArrayInsert(arr, 0, 1);
    strcpy(((Item*)xrtArrayGet(arr, pos3))->name, "A");
    
    // 在位置 2 后插入 C（即在 B 和 D 之间）
    uint32 pos4 = xrtArrayInsert(arr, 2, 1);
    strcpy(((Item*)xrtArrayGet(arr, pos4))->name, "C");
    
    // 打印: A B C D
    for (uint32 i = 1; i <= arr->Count; i++) {
        Item* item = (Item*)xrtArrayGet(arr, i);
        printf("%s ", item->name);
    }
    printf("\n");
    
    xrtArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtArrayAppend

在末尾添加元素。

**函数原型：**
```c
XXAPI uint32 xrtArrayAppend(xarray pArr, uint32 iCount);
```

**参数：**
- `pArr` - 数组对象
- `iCount` - 要添加的元素数量

**返回值：**
- 成功：第一个新元素的索引（1-based）
- 失败：`0`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    char name[32];
} User;

int main() {
    xrtInit();
    
    xarray users = xrtArrayCreate(sizeof(User));
    
    // 添加单个用户
    uint32 pos = xrtArrayAppend(users, 1);
    User* u1 = (User*)xrtArrayGet(users, pos);
    u1->id = 1;
    strcpy(u1->name, "Alice");
    
    // 批量添加 3 个用户
    uint32 start = xrtArrayAppend(users, 3);
    for (uint32 i = 0; i < 3; i++) {
        User* u = (User*)xrtArrayGet(users, start + i);
        u->id = 2 + i;
        sprintf(u->name, "User%d", 2 + i);
    }
    
    printf("共 %u 个用户:\n", users->Count);
    for (uint32 i = 1; i <= users->Count; i++) {
        User* u = (User*)xrtArrayGet(users, i);
        printf("  %d: %s\n", u->id, u->name);
    }
    
    xrtArrayDestroy(users);
    xrtUnit();
    return 0;
}
```

---

### xrtArraySwap

交换两个元素的位置。

**函数原型：**
```c
XXAPI bool xrtArraySwap(xarray pArr, uint32 iPosA, uint32 iPosB);
```

**参数：**
- `pArr` - 数组对象
- `iPosA` - 第一个元素索引（1-based）
- `iPosB` - 第二个元素索引（1-based）

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败（索引越界）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int value;
} Num;

int main() {
    xrtInit();
    
    xarray arr = xrtArrayCreate(sizeof(Num));
    
    // 添加 1, 2, 3
    for (int i = 1; i <= 3; i++) {
        uint32 pos = xrtArrayAppend(arr, 1);
        ((Num*)xrtArrayGet(arr, pos))->value = i;
    }
    
    printf("交换前: ");
    for (uint32 i = 1; i <= arr->Count; i++) {
        printf("%d ", ((Num*)xrtArrayGet(arr, i))->value);
    }
    printf("\n");  // 1 2 3
    
    // 交换第 1 个和第 3 个
    xrtArraySwap(arr, 1, 3);
    
    printf("交换后: ");
    for (uint32 i = 1; i <= arr->Count; i++) {
        printf("%d ", ((Num*)xrtArrayGet(arr, i))->value);
    }
    printf("\n");  // 3 2 1
    
    xrtArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtArrayRemove

删除元素。

**函数原型：**
```c
XXAPI bool xrtArrayRemove(xarray pArr, uint32 iPos, uint32 iCount);
```

**参数：**
- `pArr` - 数组对象
- `iPos` - 起始位置（1-based）
- `iCount` - 要删除的数量

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败（索引越界或数量为 0）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int value;
} Num;

void PrintArray(xarray arr) {
    printf("[");
    for (uint32 i = 1; i <= arr->Count; i++) {
        if (i > 1) printf(", ");
        printf("%d", ((Num*)xrtArrayGet(arr, i))->value);
    }
    printf("]\n");
}

int main() {
    xrtInit();
    
    xarray arr = xrtArrayCreate(sizeof(Num));
    
    // 添加 1-5
    for (int i = 1; i <= 5; i++) {
        uint32 pos = xrtArrayAppend(arr, 1);
        ((Num*)xrtArrayGet(arr, pos))->value = i;
    }
    
    PrintArray(arr);  // [1, 2, 3, 4, 5]
    
    // 删除第 2 个元素
    xrtArrayRemove(arr, 2, 1);
    PrintArray(arr);  // [1, 3, 4, 5]
    
    // 删除第 2-3 个元素
    xrtArrayRemove(arr, 2, 2);
    PrintArray(arr);  // [1, 5]
    
    xrtArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtArraySort

对数组进行排序。

**函数原型：**
```c
XXAPI bool xrtArraySort(xarray pArr, ptr procCompar);
```

**参数：**
- `pArr` - 数组对象
- `procCompar` - 比较函数指针（同 `qsort` 的比较函数）

**返回值：**
- `TRUE` - 成功
- `FALSE` - 失败

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int score;
    char name[16];
} Student;

// 按分数降序排序
int CompareByScore(const void* a, const void* b) {
    Student* sa = (Student*)a;
    Student* sb = (Student*)b;
    return sb->score - sa->score;  // 降序
}

int main() {
    xrtInit();
    
    xarray students = xrtArrayCreate(sizeof(Student));
    
    // 添加学生
    const char* names[] = {"Alice", "Bob", "Charlie"};
    int scores[] = {85, 92, 78};
    
    for (int i = 0; i < 3; i++) {
        uint32 pos = xrtArrayAppend(students, 1);
        Student* s = (Student*)xrtArrayGet(students, pos);
        s->score = scores[i];
        strcpy(s->name, names[i]);
    }
    
    printf("排序前:\n");
    for (uint32 i = 1; i <= students->Count; i++) {
        Student* s = (Student*)xrtArrayGet(students, i);
        printf("  %s: %d\n", s->name, s->score);
    }
    
    // 按分数排序
    xrtArraySort(students, CompareByScore);
    
    printf("排序后（按分数降序）:\n");
    for (uint32 i = 1; i <= students->Count; i++) {
        Student* s = (Student*)xrtArrayGet(students, i);
        printf("  %s: %d\n", s->name, s->score);
    }
    
    xrtArrayDestroy(students);
    xrtUnit();
    return 0;
}
```

---

## 元素访问

### xrtArrayGet

获取元素指针（安全版）。

**函数原型：**
```c
XXAPI ptr xrtArrayGet(xarray pArr, uint32 iPos);
```

**参数：**
- `pArr` - 数组对象
- `iPos` - 元素索引（1-based）

**返回值：**
- 成功：元素指针
- 失败：`NULL`（索引越界）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    xarray arr = xrtArrayCreate(sizeof(Item));
    uint32 pos = xrtArrayAppend(arr, 1);
    ((Item*)xrtArrayGet(arr, pos))->value = 42;
    
    // 正常访问
    Item* item1 = (Item*)xrtArrayGet(arr, 1);
    printf("值: %d\n", item1->value);  // 42
    
    // 越界访问（安全）
    Item* item2 = (Item*)xrtArrayGet(arr, 100);
    if (item2 == NULL) {
        printf("索引越界\n");
    }
    
    xrtArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

### xrtArrayGet_Unsafe

获取元素指针（不安全版，无边界检查）。

**函数原型：**
```c
XXAPI ptr xrtArrayGet_Unsafe(xarray pArr, uint32 iPos);
```

**警告：** 调用前必须确保索引有效，否则可能导致内存越界访问！

---

### xrtArrayGet_Inline

获取元素指针（内联版，无边界检查）。

**函数原型：**
```c
static inline ptr xrtArrayGet_Inline(xarray pArr, uint32 iPos)
{
    return &(pArr->Memory[(iPos - 1) * pArr->ItemLength]);
}
```

**说明：**
- 用于高性能遍历场景
- 无函数调用开销
- 不检查边界，调用前必须确保索引有效

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Num;

int main() {
    xrtInit();
    
    xarray arr = xrtArrayCreate(sizeof(Num));
    for (int i = 0; i < 1000; i++) {
        uint32 pos = xrtArrayAppend(arr, 1);
        ((Num*)xrtArrayGet(arr, pos))->value = i;
    }
    
    // 高性能遍历（已知索引有效）
    long sum = 0;
    for (uint32 i = 1; i <= arr->Count; i++) {
        Num* n = (Num*)xrtArrayGet_Inline(arr, i);
        sum += n->value;
    }
    printf("总和: %ld\n", sum);  // 499500
    
    xrtArrayDestroy(arr);
    xrtUnit();
    return 0;
}
```

---

## 使用场景

### 1. 批量数据处理

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    double value;
    int processed;
} DataRecord;

int main() {
    xrtInit();
    
    xarray records = xrtArrayCreate(sizeof(DataRecord));
    
    // 预分配空间（避免频繁扩容）
    xrtArrayAlloc(records, 10000);
    
    // 批量添加数据
    for (int i = 0; i < 10000; i++) {
        uint32 pos = xrtArrayAppend(records, 1);
        DataRecord* r = (DataRecord*)xrtArrayGet(records, pos);
        r->id = i + 1;
        r->value = i * 1.5;
        r->processed = 0;
    }
    
    // 批量处理
    int processedCount = 0;
    for (uint32 i = 1; i <= records->Count; i++) {
        DataRecord* r = (DataRecord*)xrtArrayGet_Inline(records, i);
        if (r->value > 5000) {
            r->processed = 1;
            processedCount++;
        }
    }
    
    printf("已处理 %d 条记录\n", processedCount);
    
    xrtArrayDestroy(records);
    xrtUnit();
    return 0;
}
```

---

### 2. 任务优先级队列

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    int priority;
    char task[64];
} Task;

int ComparePriority(const void* a, const void* b) {
    Task* ta = (Task*)a;
    Task* tb = (Task*)b;
    return tb->priority - ta->priority;  // 优先级高的在前
}

int main() {
    xrtInit();
    
    xarray tasks = xrtArrayCreate(sizeof(Task));
    
    // 添加任务
    struct { int priority; const char* name; } taskData[] = {
        {3, "发送邮件"},
        {1, "代码审查"},
        {5, "紧急修复"},
        {2, "文档编写"}
    };
    
    for (int i = 0; i < 4; i++) {
        uint32 pos = xrtArrayAppend(tasks, 1);
        Task* t = (Task*)xrtArrayGet(tasks, pos);
        t->id = i + 1;
        t->priority = taskData[i].priority;
        strcpy(t->task, taskData[i].name);
    }
    
    // 按优先级排序
    xrtArraySort(tasks, ComparePriority);
    
    printf("任务队列（按优先级）:\n");
    for (uint32 i = 1; i <= tasks->Count; i++) {
        Task* t = (Task*)xrtArrayGet(tasks, i);
        printf("  [优先级 %d] %s\n", t->priority, t->task);
    }
    
    xrtArrayDestroy(tasks);
    xrtUnit();
    return 0;
}
```

---

## 最佳实践

### 1. 预分配内存

```c
// ✓ 推荐：已知数量时预分配
xrtArrayAlloc(arr, 1000);
for (int i = 0; i < 1000; i++) {
    xrtArrayAppend(arr, 1);
}
```

### 2. 正确释放动态内存

```c
// ✓ 正确：释放数组前先释放元素的动态内存
for (uint32 i = 1; i <= arr->Count; i++) {
    DynamicItem* it = (DynamicItem*)xrtArrayGet(arr, i);
    xrtFree(it->name);
    xrtFree(it->data);
}
xrtArrayDestroy(arr);
```

### 3. 高性能遍历

```c
// ✓ 高性能方式：使用内联版本
uint32 count = arr->Count;  // 缓存 Count
for (uint32 i = 1; i <= count; i++) {
    Num* n = (Num*)xrtArrayGet_Inline(arr, i);
    sum += n->value;
}
```

---

## 与 PtrArray 的区别

| 特性 | Array | PtrArray |
|------|-------|----------|
| **存储方式** | 连续内存存储结构体本身 | 存储指针 |
| **元素大小** | 任意固定大小 | 固定 `sizeof(ptr)` |
| **内存布局** | 缓存友好 | 指针间接访问 |
| **适用场景** | 大量小型结构体 | 大型对象或需要共享引用 |
| **排序性能** | 需要移动整个结构体 | 只需交换指针 |

---

## 相关文档

- [PtrArray 指针数组](api-ptrarray.md)
- [BSMM 块结构内存管理](api-bsmm.md)
- [List 列表](api-list.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#array-结构体数组库)

</div>
