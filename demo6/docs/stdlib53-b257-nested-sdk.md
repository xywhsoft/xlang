# 默认和嵌套 Context 的 SDK 解析

默认 Context 现在自动加入编译器模块旁的 `include` 目录，不再要求
调用者手动添加声明 SDK。此前外层源码驱动能编译，但 `import xlang`
创建的内层 Context 找不到 `__xl_runtime/xruntime_decl.h`，导致
`stdlib_string_bridge_boundaries` 在正常调用处失败。该缺陷已修复。

## 默认路径和公开行为

SDK 路径来自 `xlGetModuleDir`，即加载模块或宿主可执行文件的位置，
不是 Context 的工作目录。已有项目源码路径、用户 include 配置和
TCC 工具链配置保留。该改动不声明任意用户头文件的沙箱隔离能力。

公开 API 的默认 Context 和语言层 `xlang.Context()` 均覆盖正常执行、
编译错误后复用，以及切换到空工作目录。字符串实际长度和内嵌 NUL
边界保持原合同：普通字符串内容可以含 NUL；要求 C 文本的 source、
路径参数仍明确拒绝内嵌 NUL，不截断输入。

## 回归结果

Windows 和 Linux 分别从隔离源码构建 326 和 325 个翻译单元。
没有单对象替换或旧编译器回退。

| 验证 | Windows | Linux |
| --- | --- | --- |
| SDK 专项 | 13 阶段通过 | 13 阶段通过 |
| 三组声明和严格 C | 301 夹具，601 阶段通过 | 301 夹具，601 阶段通过 |
| 三组实际 JIT 和 module | 301 项清单，586 阶段，零失败 | 301 项清单，586 阶段，零失败 |

SDK 专项包括严格 C 的 O0/O2、原失败夹具、嵌套 Context、空工作目录
宿主的三轮创建／失败／恢复／销毁，以及去掉模块旁 SDK 的反例。
反例必须报出缺少声明头，不能借助测试工作目录偷偷通过。

实际运行清单每端有 292 项执行通过、1 项预期语义拒绝、8 项尚需
专用宿主或平台／产品环境；后者不计为通过。相比前一批，唯一实际
运行失败由 1 项降为 0 项，未减少断言或移除失败夹具。

证据目录为 `E:/xlang-validation/stdlib53-b257-` 下的
`nested-{win,linux}-dev2`、`runtime-{win,linux}-dev1` 和
`discovery-{win,linux}-dev1`。完整源码构建为
`build-win-dev1` 及 Linux `/var/tmp/xlang-stdlib53-b257-build-linux-dev1`。
消费源码、声明头、对象、执行产物和日志按哈希固定。

## 发布验收和剩余工作

这批验证使用实际源码编译器和文件系统 SDK，不代表发布 DLL、CLI
或嵌入式 VFS 已验收。额外尝试 compiler-import AOT 时，两个平台均
因验证包缺少生成的 `tcc_builtin_resources_runtime.c` 失败；该项保留
为生成资源包后的发布验收要求，没有用空资源替代或计为通过。

专用 Promise／图退役／析构错误宿主、独立 borrowed 原生 SSA、
工作线程及图 OOM、模块卸载与错误压力、53 模块函数覆盖和真实发布
产物仍需完成。WaitQueue 的已批准 MPMC 和拥有接收合同不变。
CRT 未决策为零，XRT 其余 19 组保持未决策，对象池最后。
