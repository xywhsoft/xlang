# DynStack 动态栈库

> 无深度限制的动态栈，自动扩展容量

[English](api-dynstack.en.md) | [中文](api-dynstack.md) | [返回索引](README.md)

---

## 📑 目录

- [概述](#概述)
- [数据结构](#数据结构)
- [栈操作](#栈操作)
- [压栈操作](#压栈操作)
- [出栈操作](#出栈操作)
- [栈访问](#栈访问)
- [使用场景](#使用场景)
- [与静态栈对比](#与静态栈对比)
- [最佳实践](#最佳实践)

---

## 概述

DynStack（Dynamic Stack）是一个无深度限制的动态栈，支持结构体和指针两种模式。与静态栈不同，动态栈会在需要时自动扩展容量。

### 核心特点

- **无深度限制**：自动扩展，无需预设最大容量
- **分块管理**：每块存储 256 个元素，按需分配新块
- **延迟释放**：出栈时延迟释放内存块，避免临界震荡
- **两种模式**：支持结构体模式和指针模式

### 内存布局

```
DynStack 结构：
┌──────────────────────────────────────────────────────┐
│                    xdynstack_struct                   │
├─────────────┬─────────────┬──────────────────────────┤
│ ItemLength  │    Count    │         MMU (PtrArray)   │
│   (4字节)   │   (4字节)   │       内存块指针数组      │
└─────────────┴─────────────┴──────────────────────────┘
                                      │
                    ┌─────────────────┼─────────────────┐
                    ▼                 ▼                 ▼
              ┌──────────┐      ┌──────────┐      ┌──────────┐
              │ Block 0  │      │ Block 1  │      │ Block 2  │
              │ 256元素  │      │ 256元素  │      │ 256元素  │
              └──────────┘      └──────────┘      └──────────┘
```

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

### xdynstack_struct

动态栈结构。

**定义：**
```c
typedef struct {
    uint32 ItemLength;      // 栈成员占用内存长度
    uint32 Count;           // 栈中存在多少成员（栈顶位置）
    xparray_struct MMU;     // 内存块管理器（PtrArray）
} xdynstack_struct, *xdynstack;
```

**成员说明：**

| 成员 | 说明 |
|------|------|
| `ItemLength` | 每个元素的大小（字节）。指针模式时为 `sizeof(ptr)` |
| `Count` | 当前栈中元素数量，也是栈顶位置 |
| `MMU` | 内存块管理器，每块存储 256 个元素 |

**模式说明：**

| 模式 | ItemLength | 存储内容 |
|------|------------|----------|
| 结构体模式 | `sizeof(结构体)` | 结构体数据 |
| 指针模式 | `sizeof(ptr)` | 指针值 |

---

## 栈操作

### xrtDynStackCreate

创建动态栈。

**函数原型：**
```c
XXAPI xdynstack xrtDynStackCreate(uint32 iItemLength);
```

**参数：**
- `iItemLength` - 元素大小（字节）

**返回值：**
- 成功：动态栈对象
- 失败：`NULL`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int id;
    char name[32];
} Task;

int main() {
    xrtInit();
    
    // 创建结构体栈
    xdynstack taskStack = xrtDynStackCreate(sizeof(Task));
    printf("创建结构体栈，元素大小: %u\n", taskStack->ItemLength);
    
    // 创建指针栈
    xdynstack ptrStack = xrtDynStackCreate(sizeof(ptr));
    printf("创建指针栈，元素大小: %u\n", ptrStack->ItemLength);
    
    xrtDynStackDestroy(taskStack);
    xrtDynStackDestroy(ptrStack);
    xrtUnit();
    return 0;
}
```

---

### xrtDynStackDestroy

销毁动态栈。

**函数原型：**
```c
XXAPI void xrtDynStackDestroy(xdynstack objSTK);
```

**参数：**
- `objSTK` - 动态栈对象

**说明：**
- 释放所有内存块
- 释放 MMU 管理器
- 释放栈结构体本身

---

### xrtDynStackInit

初始化动态栈（用于栈上或嵌入式使用）。

**函数原型：**
```c
XXAPI void xrtDynStackInit(xdynstack objSTK, uint32 iItemLength);
```

**参数：**
- `objSTK` - 动态栈指针
- `iItemLength` - 元素大小

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    // 栈上分配
    xdynstack_struct stack;
    xrtDynStackInit(&stack, sizeof(int));
    
    // 使用栈
    int* p = (int*)xrtDynStackPush(&stack);
    *p = 42;
    printf("压入: %d\n", *p);
    
    int* top = (int*)xrtDynStackPop(&stack);
    printf("弹出: %d\n", *top);
    
    xrtDynStackUnit(&stack);
    xrtUnit();
    return 0;
}
```

---

### xrtDynStackUnit

释放动态栈资源（不释放结构体本身）。

**函数原型：**
```c
XXAPI void xrtDynStackUnit(xdynstack objSTK);
```

**参数：**
- `objSTK` - 动态栈对象

**说明：**
- 与 `xrtDynStackInit` 配对使用
- 释放所有内存块和 MMU，但不释放 `objSTK` 本身

---

## 压栈操作

### xrtDynStackPush

压栈并返回元素指针（结构体模式）。

**函数原型：**
```c
XXAPI ptr xrtDynStackPush(xdynstack objSTK);
```

**参数：**
- `objSTK` - 动态栈对象

**返回值：**
- 成功：新元素的指针（可直接写入数据）
- 失败：`NULL`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int x, y;
} Point;

int main() {
    xrtInit();
    
    xdynstack stack = xrtDynStackCreate(sizeof(Point));
    
    // 压入点 (10, 20)
    Point* p1 = (Point*)xrtDynStackPush(stack);
    p1->x = 10;
    p1->y = 20;
    
    // 压入点 (30, 40)
    Point* p2 = (Point*)xrtDynStackPush(stack);
    p2->x = 30;
    p2->y = 40;
    
    printf("栈中元素: %u\n", stack->Count);  // 2
    
    xrtDynStackDestroy(stack);
    xrtUnit();
    return 0;
}
```

**内存分配策略：**
```
压栈流程：
1. 计算目标块号：iBlock = Count >> 8 (Count / 256)
2. 如果块已存在 → 直接使用
3. 如果块不存在 → 分配新块（256 个元素）
4. 计算块内偏移：iPos = Count & 0xFF (Count % 256)
5. 返回元素指针
```

---

### xrtDynStackPushData

压栈并复制数据。

**函数原型：**
```c
XXAPI uint32 xrtDynStackPushData(xdynstack objSTK, ptr pData);
```

**参数：**
- `objSTK` - 动态栈对象
- `pData` - 源数据指针

**返回值：**
- 成功：当前栈元素数量
- 失败：`0`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int value;
} Item;

int main() {
    xrtInit();
    
    xdynstack stack = xrtDynStackCreate(sizeof(Item));
    
    Item items[] = {{100}, {200}, {300}};
    for (int i = 0; i < 3; i++) {
        uint32 count = xrtDynStackPushData(stack, &items[i]);
        printf("压入 %d，当前数量: %u\n", items[i].value, count);
    }
    
    xrtDynStackDestroy(stack);
    xrtUnit();
    return 0;
}
```

---

### xrtDynStackPushPtr

压栈指针值（指针模式）。

**函数原型：**
```c
XXAPI uint32 xrtDynStackPushPtr(xdynstack objSTK, ptr pVal);
```

**参数：**
- `objSTK` - 动态栈对象
- `pVal` - 要压入的指针值

**返回值：**
- 成功：当前栈元素数量
- 失败：`0`

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xdynstack stack = xrtDynStackCreate(sizeof(ptr));
    
    // 压入字符串指针
    xrtDynStackPushPtr(stack, (ptr)"First");
    xrtDynStackPushPtr(stack, (ptr)"Second");
    xrtDynStackPushPtr(stack, (ptr)"Third");
    
    printf("栈中指针数量: %u\n", stack->Count);  // 3
    
    // 弹出并打印
    while (stack->Count > 0) {
        str s = (str)xrtDynStackPopPtr(stack);
        printf("弹出: %s\n", s);
    }
    
    xrtDynStackDestroy(stack);
    xrtUnit();
    return 0;
}
```

---

## 出栈操作

### xrtDynStackPop

出栈并返回元素指针（结构体模式）。

**函数原型：**
```c
XXAPI ptr xrtDynStackPop(xdynstack objSTK);
```

**参数：**
- `objSTK` - 动态栈对象

**返回值：**
- 栈顶元素指针

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int value;
} Item;

int main() {
    xrtInit();
    
    xdynstack stack = xrtDynStackCreate(sizeof(Item));
    
    // 压入
    for (int i = 1; i <= 5; i++) {
        Item* p = (Item*)xrtDynStackPush(stack);
        p->value = i * 10;
    }
    
    // 弹出
    while (stack->Count > 0) {
        Item* p = (Item*)xrtDynStackPop(stack);
        printf("弹出: %d\n", p->value);
    }
    // 输出: 50, 40, 30, 20, 10
    
    xrtDynStackDestroy(stack);
    xrtUnit();
    return 0;
}
```

**延迟释放策略：**
```
出栈时内存释放规则：
- 条件：(块数 * 256) > (Count + 288)
- 即：当空闲容量超过 288 个元素时，释放最后一个块
- 目的：避免临界状态频繁分配/释放
```

---

### xrtDynStackPopPtr

出栈并返回指针值（指针模式）。

**函数原型：**
```c
XXAPI ptr xrtDynStackPopPtr(xdynstack objSTK);
```

**参数：**
- `objSTK` - 动态栈对象

**返回值：**
- 栈顶存储的指针值（不是元素指针）

---

## 栈访问

### xrtDynStackTop

获取栈顶元素指针（不弹出）。

**函数原型：**
```c
XXAPI ptr xrtDynStackTop(xdynstack objSTK);
```

**参数：**
- `objSTK` - 动态栈对象

**返回值：**
- 栈顶元素指针

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xdynstack stack = xrtDynStackCreate(sizeof(int));
    
    int* p = (int*)xrtDynStackPush(stack);
    *p = 100;
    
    // 查看栈顶（不弹出）
    int* top = (int*)xrtDynStackTop(stack);
    printf("栈顶: %d, 数量: %u\n", *top, stack->Count);  // 100, 1
    
    // 修改栈顶
    *top = 200;
    printf("修改后栈顶: %d\n", *(int*)xrtDynStackTop(stack));  // 200
    
    xrtDynStackDestroy(stack);
    xrtUnit();
    return 0;
}
```

---

### xrtDynStackTopPtr

获取栈顶指针值（指针模式，不弹出）。

**函数原型：**
```c
XXAPI ptr xrtDynStackTopPtr(xdynstack objSTK);
```

---

### xrtDynStackGetPos

获取任意位置元素指针（安全版）。

**函数原型：**
```c
XXAPI ptr xrtDynStackGetPos(xdynstack objSTK, uint32 iPos);
```

**参数：**
- `objSTK` - 动态栈对象
- `iPos` - 位置索引（1-based）

**返回值：**
- 成功：元素指针
- 失败：`NULL`（索引越界）

**示例：**
```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xdynstack stack = xrtDynStackCreate(sizeof(int));
    
    // 压入 5 个元素
    for (int i = 1; i <= 5; i++) {
        int* p = (int*)xrtDynStackPush(stack);
        *p = i * 10;
    }
    
    // 随机访问（注意：索引从 1 开始）
    int* item1 = (int*)xrtDynStackGetPos(stack, 1);  // 第一个
    int* item3 = (int*)xrtDynStackGetPos(stack, 3);  // 第三个
    int* item5 = (int*)xrtDynStackGetPos(stack, 5);  // 第五个
    int* itemX = (int*)xrtDynStackGetPos(stack, 10); // 越界
    
    printf("位置1: %d\n", item1 ? *item1 : -1);  // 10
    printf("位置3: %d\n", item3 ? *item3 : -1);  // 30
    printf("位置5: %d\n", item5 ? *item5 : -1);  // 50
    printf("位置10: %s\n", itemX ? "存在" : "NULL");  // NULL
    
    xrtDynStackDestroy(stack);
    xrtUnit();
    return 0;
}
```

---

### xrtDynStackGetPos_Unsafe

获取任意位置元素指针（不安全版）。

**函数原型：**
```c
XXAPI ptr xrtDynStackGetPos_Unsafe(xdynstack objSTK, uint32 iPos);
```

**⚠️ 警告：** 不检查边界，索引越界会导致未定义行为！

---

### xrtDynStackGetPosPtr / xrtDynStackGetPosPtr_Unsafe

获取任意位置的指针值（指针模式）。

**函数原型：**
```c
XXAPI ptr xrtDynStackGetPosPtr(xdynstack objSTK, uint32 iPos);
XXAPI ptr xrtDynStackGetPosPtr_Unsafe(xdynstack objSTK, uint32 iPos);
```

---

## 使用场景

### 1. 深度优先搜索 (DFS)

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int node;
    int depth;
} DFSState;

void DFS(int startNode) {
    xdynstack stack = xrtDynStackCreate(sizeof(DFSState));
    
    // 压入起始节点
    DFSState* s = (DFSState*)xrtDynStackPush(stack);
    s->node = startNode;
    s->depth = 0;
    
    while (stack->Count > 0) {
        DFSState* current = (DFSState*)xrtDynStackPop(stack);
        printf("访问节点 %d，深度 %d\n", current->node, current->depth);
        
        // 模拟添加子节点（实际应用中根据图结构）
        if (current->depth < 3) {
            for (int i = 0; i < 2; i++) {
                DFSState* child = (DFSState*)xrtDynStackPush(stack);
                child->node = current->node * 10 + i;
                child->depth = current->depth + 1;
            }
        }
    }
    
    xrtDynStackDestroy(stack);
}

int main() {
    xrtInit();
    DFS(1);
    xrtUnit();
    return 0;
}
```

---

### 2. 表达式求值

```c
#include "xrt.h"
#include <stdio.h>
#include <ctype.h>

int EvaluatePostfix(str expr) {
    xdynstack stack = xrtDynStackCreate(sizeof(int));
    
    while (*expr) {
        if (isdigit(*expr)) {
            // 数字入栈
            int* p = (int*)xrtDynStackPush(stack);
            *p = *expr - '0';
        } else if (*expr == '+' || *expr == '*') {
            // 运算符：弹出两个操作数
            int* b = (int*)xrtDynStackPop(stack);
            int* a = (int*)xrtDynStackPop(stack);
            int* result = (int*)xrtDynStackPush(stack);
            
            if (*expr == '+') {
                *result = *a + *b;
            } else {
                *result = (*a) * (*b);
            }
        }
        expr++;
    }
    
    int* result = (int*)xrtDynStackTop(stack);
    int value = *result;
    xrtDynStackDestroy(stack);
    return value;
}

int main() {
    xrtInit();
    
    // 后缀表达式: 3 4 + 2 * = (3+4)*2 = 14
    int result = EvaluatePostfix((str)"34+2*");
    printf("结果: %d\n", result);  // 14
    
    xrtUnit();
    return 0;
}
```

---

### 3. 撤销/重做功能

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    char action[64];
    int data;
} Operation;

typedef struct {
    xdynstack_struct undoStack;
    xdynstack_struct redoStack;
} Editor;

void EditorInit(Editor* e) {
    xrtDynStackInit(&e->undoStack, sizeof(Operation));
    xrtDynStackInit(&e->redoStack, sizeof(Operation));
}

void EditorUnit(Editor* e) {
    xrtDynStackUnit(&e->undoStack);
    xrtDynStackUnit(&e->redoStack);
}

void DoAction(Editor* e, str action, int data) {
    Operation* op = (Operation*)xrtDynStackPush(&e->undoStack);
    strncpy(op->action, (char*)action, 63);
    op->data = data;
    printf("执行: %s %d\n", action, data);
    
    // 清空重做栈
    while (e->redoStack.Count > 0) {
        xrtDynStackPop(&e->redoStack);
    }
}

void Undo(Editor* e) {
    if (e->undoStack.Count == 0) {
        printf("无可撤销操作\n");
        return;
    }
    
    Operation* op = (Operation*)xrtDynStackPop(&e->undoStack);
    printf("撤销: %s %d\n", op->action, op->data);
    
    // 移动到重做栈
    Operation* redo = (Operation*)xrtDynStackPush(&e->redoStack);
    memcpy(redo, op, sizeof(Operation));
}

int main() {
    xrtInit();
    
    Editor editor;
    EditorInit(&editor);
    
    DoAction(&editor, (str)"Insert", 100);
    DoAction(&editor, (str)"Delete", 50);
    DoAction(&editor, (str)"Modify", 75);
    
    Undo(&editor);  // 撤销 Modify
    Undo(&editor);  // 撤销 Delete
    
    EditorUnit(&editor);
    xrtUnit();
    return 0;
}
```

---

## 与静态栈对比

| 特性 | 静态栈 (Stack) | 动态栈 (DynStack) |
|------|----------------|-------------------|
| **容量** | 固定（创建时指定） | 无限（自动扩展） |
| **内存布局** | 连续内存 | 分块内存（256元素/块） |
| **内存效率** | 预分配，可能浪费 | 按需分配，更节省 |
| **访问速度** | O(1)，直接偏移 | O(1)，两级寻址 |
| **适用场景** | 深度可预知 | 深度不可预知 |
| **内存开销** | 较小 | 较大（MMU管理器） |

### 选择建议

- **使用静态栈**：深度可预知、追求极致性能
- **使用动态栈**：深度不可预知、递归深度大

---

## 最佳实践

### 1. 选择合适的栈类型

```c
#include "xrt.h"
#include <stdio.h>

typedef struct { int value; } Item;

int main() {
    xrtInit();
    
    // 场景1：深度可预知 → 静态栈
    xstack staticStack = xrtStackCreate(sizeof(Item), 100);
    printf("静态栈：最大深度 100\n");
    xrtStackDestroy(staticStack);
    
    // 场景2：深度不可预知 → 动态栈
    xdynstack dynStack = xrtDynStackCreate(sizeof(Item));
    printf("动态栈：无深度限制\n");
    xrtDynStackDestroy(dynStack);
    
    xrtUnit();
    return 0;
}
```

---

### 2. 检查压栈结果

```c
#include "xrt.h"
#include <stdio.h>

int main() {
    xrtInit();
    
    xdynstack stack = xrtDynStackCreate(sizeof(int));
    
    // 安全压栈
    int* p = (int*)xrtDynStackPush(stack);
    if (p == NULL) {
        printf("内存分配失败！\n");
        xrtDynStackDestroy(stack);
        xrtUnit();
        return 1;
    }
    *p = 42;
    printf("压栈成功: %d\n", *p);
    
    xrtDynStackDestroy(stack);
    xrtUnit();
    return 0;
}
```

---

### 3. 使用 PushData 简化代码

```c
#include "xrt.h"
#include <stdio.h>

typedef struct {
    int x, y;
} Point;

int main() {
    xrtInit();
    
    xdynstack stack = xrtDynStackCreate(sizeof(Point));
    
    // 方式1：Push + 赋值
    Point* p1 = (Point*)xrtDynStackPush(stack);
    p1->x = 10;
    p1->y = 20;
    
    // 方式2：PushData（更简洁）
    Point pt = {30, 40};
    xrtDynStackPushData(stack, &pt);
    
    printf("栈中元素: %u\n", stack->Count);  // 2
    
    xrtDynStackDestroy(stack);
    xrtUnit();
    return 0;
}
```

---

## 相关文档

- [Stack 静态栈](api-stack.md)
- [PtrArray 指针数组](api-ptrarray.md)
- [主索引](README.md)

---

<div align="center">

[⬆️ 返回顶部](#dynstack-动态栈库)

</div>
