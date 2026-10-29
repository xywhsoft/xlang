# FixedStack 标准库合同

`FixedStack<T>` 是普通泛型受管理类，提供固定逻辑容量的 LIFO 容器。
无需显式导入；使用 `FixedStack::<T>(capacity)` 构造，容量非负。
底座是已有 typed Stack 的槽位、准入描述、拥有图和退役机制。
不公开 XRT 的裸字节 fixed-stack，也不新增专用 Cgen 或原生 ABI。

```xl
auto values = FixedStack::<bytes>(2)
assert(values.push(b"a\0b"))
auto present, value = values.tryPop()
assert(present and value != null and value == b"a\0b")
```

## 公开面

| 方法 | 返回与边界 |
|---|---|
| `count()` / `capacity()` | 当前元素数 / 构造时指定的逻辑容量 |
| `empty()` / `full()` | 是否为空 / 是否达到逻辑容量；容量零时二者都为 true |
| `push(T value)` | 成功 true；满栈 false，内容不变 |
| `pop()` / `peek()` / `peekAt(int depth)` | 拥有的 T；空栈/越界按 Stack 报错；depth=0 为栈顶 |
| `tryPop()` / `tryPeek()` / `tryPeekAt(int depth)` | `(bool, T?)`；状态区分取到元素与无元素 |
| `clear()` | 释放元素，逻辑容量不变，容器可继续使用 |
| `clone()` / `copy(value)` / `clone(value)` | 独立栈内容及相同逻辑容量；元素复制遵循 Stack 的拥有合同 |
| `equals(FixedStack<T> other)` | 按 Stack 比较内容；逻辑容量不参与相等判断 |

没有 `reserve`、`trim`、可写容量或原生指针入口。构造时预留空间；底层
分配器可向上取整，但 `capacity()` 始终报告公开的精确逻辑容量。
普通 push 不允许容器扩容；元素自身的值复制仍可能分配并报 MemoryError，
因此不能把固定容量误认为“所有操作都无分配/不会失败”。

## 值与资源

赋值/传参引用同一个受管理容器。`copy`/`clone` 复制容器存储，不改变
已有对象元素的引用模型。string/bytes 维持基础值的复制与写时隔离；
pop/peek/try 结果必须能独立于原栈或后续 finally 清理存活。
不支持共享无锁操作，跨线程访问由调用者同步。

`T?` 在 T 已可空时不会产生第二层可空标签；var 的值域也已包含 null。
例如 `FixedStack<int?>.tryPop()` 的 `(true, null)` 表示取到了 null 元素，
而 `(false, null)` 表示没有元素。显式书写 `int??` 仍在源码阶段被拒绝。
类型域沿用 Stack 的准入限制，不能以这个普通类绕过线性/未闭合元素 ABI。

## IR 与验收

开放模板的 Stack 调用保持精确逻辑 TypeId，标为 `generic-dependent`，
不是可执行物理合同。单态化统一闭合 receiver、payload、product、拥有
返回及 persistence TypeId；只有 VERIFIED 具体记录才能生成指令。
重复 `(bool,T?)` 签名和具体 nullable 共享一个规范 TypeId，不以源层的
ABI 家族标记区分同一个结构类型。

`tools/check_stdlib_fixed_stack.py` 覆盖三份实际语言程序、三个语义负例和一个解析负例、
C/header 前合同变异、O0/O2、JIT/AOT、每分配前缀失败与双重模块终结。
Linux sanitizer 只覆盖生成 C/宿主，不声称共享库已全源插桩。
独立 native artifact 闭包验证另行运行，专项通过不代表所有容器或全仓发布验收。
