# 引擎初始化的标题与代码寿命

`xge.init(width,height,title,flags,runMode)` 仍使用栈上的原生描述结构。
原生 `xgeInitWithCode` 保存标题副本；调用方随后修改或释放字符串，不会改变以后
`run()` 创建的窗口。`setTitle()` 直接调用原生 checked setter，初始化后和运行中
均可使用；分配失败保留原有标题，跨创建线程调用会报错。

初始化通过一个冷路径适配交付代码租约。即使没有 frame/scene 回调，已初始化
的引擎仍持有独立代码引用。语言模块 Close 后，原生排队工作仍可完成；最终 Unit
清空资源和自己的代码槽，再通过驻留 Drop 保护物理返回尾部。Busy 或尚未完成的
清理不能提前释放该引用。`initNative()` 使用同一代码合同，但描述结构中的其他
指针仍由显式 C 调用方管理。初始化后应以 `finally` 调用 `unit()`。

两个入口共用 `xl6Xge3InitWithCode`。`xl6Xge3InitPacked` 是编译期 C 宏别名；
私有包的按需源组列出两种调用拼写，保证只用 packed 入口时也选中初始化适配。
它不是第二个 DLL 导出，也没有另一层转发函数。普通绘图和资源方法保持直接原生
调用。本批不修改编译器、IR 或 Cgen。

冷路径成本已实测。相同标题和输入队列，驻留 C owner 为 2 次分配／61,452 字节；
语言 JIT 和独立静态 AOT 在 Windows/Linux 均为 3 次／61,516 字节。语言额外的
1 次／64 字节用于代码租约，关闭及作用域退出后计数归零。C 对照代码在进程中
常驻，不承担动态模块卸载，不能据此宣称两者的代码寿命开销完全等价。另有一次
调用期 C 字符串 NUL 检查、原生标题长度扫描／复制、租约准入和引用维护；这些
工作不进入每帧绘图。没有增加逐方法包装或动态装箱。

原生新增的 nullable 标题快照用于 C 和验收。全局 extern 的 nullable 借用 C 字符串
返回目前缺少统一语言合同，尚未公开语言 getter；实例方法的既有 nullable 字符串
规则不能直接替代它。这项通用 C 互操能力留在完整接入清单中。

本批两端各 20 项原生初始化、44 项既有图形资源检查、8 项真实模块 Close／最终
卸载、8 项严格 C/C++ 头及实际链接检查通过。语言资源和真实 OpenGL JIT/静态
AOT 为 Windows 30 项、Linux 24 项；冷成本另有两端测量。这里的“项”包括
构建、运行和导入检查，不表示相同数量的独立测试场景。

候选为 native r24、Windows 完整 O2 编译器 r13、Linux 完整 O2 编译器 r5，使用
同一 XRT。候选排除了并行 XUI/LSP 修改，未替换正式 release/IDE。后端实际
调度、拥有式会话／离屏、EGL 配置和两阶段关闭仍需继续开发。

证据索引：`dev/release-materials/provenance/xge-initialization-20261110.json`。

2026-11-13 后继：用户批准的[通用借用字符串结果合同](foreign-cstring-results.md)
已补齐上述全局 getter 缺口，公开 `xge.getTitle() as string?`。返回内容在原生调用
结束前复制，区分 NULL／空标题，修改标题或关闭引擎后快照仍有效；一次读取需
扫描、分配和复制。两端源码专项及 Windows 完整 O2 r32 的 19 项实际 JIT／动态／
静态程序、示例和公共头检查通过。完整编译器后继见
`foreign-cstring-windows-20261113.json`；Linux 完整后继及最终安装另行记录。

Linux r25 的同等 19 项标题验收亦已通过，后继索引为
`foreign-cstring-linux-20261113.json`。两端均覆盖完整编译器、JIT／动态／静态
独立程序及公共头；实际平台窗口更新和最终配套安装继续单列。
