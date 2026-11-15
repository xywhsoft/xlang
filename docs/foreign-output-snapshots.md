# C 输出记录字符串与拥有值

这两项合同用于直接声明已有 C API，不添加业务转发函数。

## 输出记录字段快照

```xl
extern "nativeRead" @header("native.h") @no_raise @c_output_record(1,"NativeInfo","borrowed") @c_output_size(1,"iSize") @c_status(0) @c_output_field(0,"kind") @c_output_field(1,"title","cstring") func read(Resource input) as (c_int,string?)
```

上例表示物理参数 1 是 `NativeInfo*`。字段必须是直接成员；字符串字段的准确
C 类型是 `const char*`。不接受成员表达式、裸指针结果、内嵌字符数组或隐含编码转换。
`string?` 保留 NULL 与空串的区别；非空 `string` 遇到 NULL 会报 checked 错误。

原生状态和错误检查成功后，字符串在输入／回调作用域结束前复制为独立拥有值。
POD、字符串及独立 retain 资源可以组成同一结果；只有所有字段和退出清理成功，
结果才会交付给调用者。复制或保留失败时，已转换字段全部回收，赋值目标保持原值。
这不撤销原生生产者已经完成的操作。

一个借用输出记录可以有不同逻辑返回类型的字段投影，原生记录名、物理输出槽、
大小初始化、输入映射、头文件、状态和效果合同必须相同。同一逻辑返回类型的
重复声明继续要求相同的字段／拥有映射。每个投影分别做字段类型与生命周期检查。

当前 Document 提供：

- `DocumentSnapshot.nodeTextInfo(node)`、`DocumentTransaction.nodeTextInfo(node)`：
  `(resource, info, title, linkTarget, linkTitle)`，五项均为 `string?`。
- `DocumentSnapshot.linkInfo(range)`：`(linked, mixed, uri, title)`，后两项为 `string?`。

已有 `nodeInfo` 和 `linkState` 的返回形状保持不变。嵌套的 `tAttributes.sLanguage`、
固定字符数组及其他未声明字段仍不由本合同推导。

## 接管一个 XRT 拥有值

```xl
extern "nativeSave" @header("native.h") @no_raise @c_output(1,0) @c_output_view(1,"owned_value") func save(Resource input) as var
```

物理输出准确为 `xvalue**`，逻辑返回必须是非可空写法的 `var`；`var` 自身可以
表示 null。原生转移一份来自同一个驻留 XRT 的拥有引用，调用者直接接管原指针，
没有 retain、深拷贝、字节复制或新增值外壳。正常作用域退出按普通 `var` 释放。
NULL 结果表示语言 null。声明要求 `@no_raise`，不允许 `@pure`。

原生状态失败或错误待交付时，非 NULL 输出仍由生成代码 release；与其他输出组成
元组时，后续字段转换失败也会释放已接管值。生产者必须返回有效的拥有引用，
不能把借用值、临时栈数据或其他 XRT 实例的值冒充拥有结果。结果树中的资源及代码
必须已经持有完整生命周期；接管不会扫描或补建缺失的拥有边。

Dock 对应入口为 `DockPanel.saveState() as var`、`DockPanel.loadState(state)` 和
`xui.dockStateCounts(state)`。状态独立于控件存活。载入遵循原生语义：结构检查
通过后再重建布局，后续失败不承诺恢复整个旧布局。`StateFree` 由普通 `var` 清理替代。

## 成本与验证

字符串需要扫描 NUL 并复制非空数据，不能称为零成本。整组临时拥有清理和资源
检查照常存在。拥有值接管本身不扫描结果树、不分配、不同步、不复制；原生 Dock
序列化仍分配自己的对象、数组和标量。

零复制不等于整个调用零开销。当前生成的接管段在成功路径仍有三次 `xrtGetError`、
一次 `xrtTakeError`、一次空指针 `xrtValueRelease` 和一次 `xrtSetErrorTake` 调用，
用于检查和保护清理错误；原生状态分支、输入作用域、线程／代码租约与最终赋值
检查另计。测试中的分配计数不覆盖这些函数调用和拥有管理成本。

专项入口：`tools/check_foreign_output_snapshots.py`、`tools/check_xui_state_outputs.py`。
它们要求实际完整构建的编译器，覆盖 C11／C++11／C++17 头文件、JIT、独立 AOT、
原生错误、NULL／空串、作用域失效、拥有指针身份及逐点 OOM。后者还比较同一 SDK
的手写 C O0／O2 主机／静态程序。IR 夹具为 `foreign_output_snapshots_ir.c`，并复测
既有保留资源输出记录及 C-string 输出合同。

匹配的构建、运行结果、分配计数、图像读回和范围限制以本批 provenance 为准。
这些内存计数不构成 CPU 性能等价或整个 XGE 接入完成的结论。

本批 Windows／Linux 均完成完整 O2 构建及 64 项步骤：通用合同 27、真实接口与
文档范例 30、Dock 窗口范例 7。每个平台的通用测试覆盖四种执行路径，每族各
64 个 OOM 前缀；记录结果在 JIT／TCC AOT 下触发 5 个失败点，在 GCC O0／O2
测试宿主下触发 4 个，混合结果均为 3 个。真实 Document 文本元组为 5 个失败点；
Dock 为 207 个前缀中的 199 个，手写 C 与三个语言执行路径一致。

拥有值 1000 次调用的分配／释放均为 1000／1000，Dock 100 次保存均为
19900／19900，与手写 C 计数一致，重分配为 0。Windows 的新旧 IR 合同共拒绝
462 种变异，并另行执行旧输出记录和 C-string 回归。两个平台的三种窗口路径
分别具有一致的像素读回；Linux 使用本地虚拟 X11 显示。GCC 验证从各自完整
构建的 VFS 清单重新提取 516 个私有头，不依赖复制来的旧 SDK 私有头目录。
