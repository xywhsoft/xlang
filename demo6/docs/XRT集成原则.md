# xlang DEMO6 XRT 集成原则

本文档记录 DEMO6 与 XRT 的集成硬约束。

## 1. XRT 优先原则

能够使用 XRT 实现的功能，必须使用 XRT 对应功能。

严格要求：

- 不在 xlang 仓库里重复制造 XRT 已经存在的功能。
- 不为了方便在 xlang runtime 或标准库中复制一份 XRT 逻辑。
- 不用 xlang 专用实现替代 XRT 已有实现。
- 实现功能前必须先查询 XRT 是否已有对应函数、类型、工具或测试。
- 对 XRT 已有功能，应优先通过 C 混编声明暴露到 xlang。

这条规则适用于：

- 标准库。
- 隐性类方法。
- 容器操作。
- 字符串、时间、路径、文件、网络、编码、哈希、加密等功能。
- 类型转换、格式化、解析等基础运行时能力。

## 2. 开发前必须查询 XRT

实现 DEMO6 功能时，应先检查 XRT：

```text
D:\GIT\xrt\lib
D:\GIT\xrt\test
D:\GIT\xrt\docs
D:\GIT\xrt\singlehead
```

查询目标：

- 是否已有对应函数。
- 是否已有相近函数可以复用。
- 是否已有测试覆盖。
- 是否存在 BUG 或边界行为不符合 xlang 需求。

如果 XRT 已有能力：

- xlang 侧只做声明、类型映射、命名绑定或极薄适配。
- 不重新实现核心逻辑。

如果 XRT 有 BUG：

- 进入 `D:\GIT\xrt` 修复。
- 在 XRT `test` 目录补充测试。
- 运行 XRT 测试。
- 运行 `singlehead/single_head_maker.exe`。
- 将生成后的单头文件同步回 xlang。

## 3. XRT 缺口处理原则

如果 XRT 暂时缺少某个语言基础能力，允许在 xlang 仓库中实现临时补全层。

但必须遵守：

- 补全层必须放在单独文件中。
- 补全层不能混入普通 xlang 语言实现代码。
- 补全层函数使用 XRT 风格命名。
- 补全层代码风格参考 XRT `lib/time.h`、`lib/string.h` 等文件。
- 补全层应尽量只依赖 XRT 基础设施。
- 补全层要为后续合并回 XRT 做准备。
- 后续合并到 XRT 后，应尽量不改变 xlang 侧调用代码。

推荐命名方向：

```text
xrtXxx...
```

示例：

```c
// 临时补全：后续计划合并到 XRT
xrtBool xrtValueToInt(...);
xrtBool xrtValueToFloat(...);
xrtBool xrtValueToString(...);
```

注意：

- 具体函数名以后续实际 XRT 命名规范为准。
- 本文示例只表达“使用 XRT 命名方式”的原则。

## 4. 类型转换是重点缺口

当前已明确：XRT 对语言基础功能仍有空缺，尤其是类型隐式转换和强制转换能力。

DEMO6 需要重点关注：

- `int(expr)`。
- `float(expr)`。
- `string(expr)`。
- `bool(expr)`。
- `time(expr)`。
- `point(expr)`。
- `type(expr)`。
- 字符串到数值。
- 数值到字符串。
- 时间到字符串。
- 字符串到时间。
- `var` 到具体类型。
- 具体类型到 `var`。
- 容器元素转换。

处理原则：

- XRT 已有解析/格式化函数时直接使用。
- XRT 缺少统一转换函数时，在 xlang 侧临时补全层实现。
- 临时补全层使用 XRT 命名方式。
- 后续将成熟实现迁移回 XRT。

## 5. 与标准库的关系

DEMO6 标准库不应成为第二套 XRT。

标准库层应做：

- 声明 XRT 函数。
- 映射 xlang 符号名。
- 绑定隐性类方法。
- 处理必要的 xlang 类型签名。

标准库层不应做：

- 重新实现字符串库。
- 重新实现时间库。
- 重新实现文件/路径/网络库。
- 重新实现容器算法。
- 重新实现编码、哈希、加密等 XRT 已有能力。

## 6. 与隐性类的关系

隐性类方法优先绑定到 XRT。

示例方向：

```xlang
auto text = 123.toString()
auto parts = "1,2".split(",")
```

生成方向：

```c
xrtIntToStr(...);
xrtSplit(...);
```

规则：

- 隐性类只是 xlang 调用外观。
- 实现应优先来自 XRT。
- xlang 不为隐性类额外复制一套标准库逻辑。

## 7. 验收要求

实现任何 runtime/stdlib/隐性类功能时，代码审查应检查：

- 是否查询过 XRT。
- 是否复用了 XRT 已有函数。
- 如果没有复用，原因是否明确。
- 如果是 XRT 缺口，是否放在单独补全层。
- 补全层命名是否接近 XRT 风格。
- 是否方便后续迁移回 XRT。
- 是否避免改变 xlang 侧调用代码。

这条原则是 DEMO6 稳定性和长期维护性的硬约束。

## 8. 动态值的线程发布

文件作用域 `var`、静态泛型容器以及 class 拥有的动态字段可能被 thread、task 或 native callback 访问，
因此初始化和重绑定都必须在发布边界进入 `XRT_OBJMODE_SHARED`，不能在某个回调入口临时修补。

XRT 提供以下模式化复制 API：

```c
xvalue xvoCopyEx(xvalue value, uint32 mode);
xvalue xvoDeepCopyEx(xvalue value, uint32 mode);
```

- `XRT_OBJMODE_LOCAL` 保持原有 copy/deep-copy 语义。
- `XRT_OBJMODE_SHARED` 递归创建共享 array/list/set/dict 根，并将标量值头切换为原子引用计数模式。
- 向共享 xvalue 容器写入本地复合值时，XRT 自动创建共享图副本；array/list/set/dict 的公开写入 API 使用同一规则。
- 共享发布不会替用户消除数据竞争；多个线程写同一对象时仍必须使用 mutex/rwlock 等同步原语。
- 当前共享值图最多允许 256 层容器嵌套；循环引用会以发布失败结束，不会无限递归。

生成代码通过内部 `xl6_xrt_move_to_shared` 完成拥有权转移，原本地临时值在共享副本建立后立即释放。

## 9. xvalue 内存池与容器写时复制

`xvalue_struct` 保持固定长度，函数签名、class、native-backed class 等扩展数据继续放在独立对象中，不向值外壳追加可变字段。

- xvalue 外壳统一使用 `xrtMalloc` / `xrtFree`，直接复用 XRT 线程本地 size-class 池，不再维护第二套专用对象池。
- 动态 array/list/set/dict 复制时创建独立 xvalue 外壳并共享容器 backing。
- 第一次写入通过 `xvoEnsureUnique` 分离 backing；嵌套写入通过 `xvoIndexGetMutableI64`、`xvoIndexGetMutableKey` 或 `xvoIndexGetMutableValue` 逐层分离。
- 普通读取使用只读借用接口，不承担分离成本。
- `xvoGetArray/List/Coll/Set/Table` 返回的底层对象只允许只读访问；需要直接修改底层对象时必须使用对应的 `xvoGetMutable*` 接口。
- 静态泛型容器保持 eager value-copy。它们直接暴露稳定 C 结构和 `xtarray/xtlist/xtset/xtdict` ABI；引入透明 COW 会要求所有 C 写 API 改为二级句柄，并增加每次写入的间接访问。该取舍是正式语义，不是兼容回退。

## 10. 裁剪与依赖元数据

- 标准库函数依赖集中保存在 `src/stdlib/xl6_stdlib_deps.inc`，展开过程按有向依赖闭包注入所需符号。
- 标准库查找索引由 `tools/generate_stdlib_index.ps1` 生成只读哈希表，不在运行时初始化，不依赖 TLS。
- XRT 裁剪由 `src/compile/xl6_xrt_trim.c` 根据 HIR/生成代码实际使用的模块产生宏开关。
- `xl.dll` 保留运行脚本所需模块；编译产物按实际依赖裁剪，未使用模块不进入最终 XRT 实现单元。
