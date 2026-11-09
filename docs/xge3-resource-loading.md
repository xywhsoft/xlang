# Resource 与资源提供者

本批扩展阶段 2 的资源部分；场景、异步请求、宿主后端和 EGL 并未因此完成。
业务入口全部是直接声明，不增加 xlang 专用的逐方法 C 转发函数。

| xlang 入口 | 原生映射 | 语义 |
| --- | --- | --- |
| loadResource(uri) | xgeResourceLoad | 独立拥有的 Resource；URI 使用 checked C 字符串 |
| resourceFromMemory(bytes/string/Buffer) | xgeResourceLoadMemory | 以实际字节长度复制，保留内嵌 NUL，空输入有效 |
| Resource.size/providerIndex/bytes | 原生字段 | bytes 在受保护作用域内形成语言值副本；关闭 Resource 后副本仍有效 |
| addResourceProvider(scheme, read) | xgeResourceProviderAddOwned | 全局外部根；持有闭包和代码至 clear/Unit |
| addXPackProvider(root, read) | xgeResourceXPackProviderAddOwned | 复制 root，移除请求路径前导分隔符，拼接 pack 路径并拒绝 .. 分段 |
| resourceProviderClear() | xgeResourceProviderClearChecked | 先摘下全部注册再释放；活动加载继续持有快照；清理错误 checked 交付 |
| ResourceOutput.set(bytes/string/Buffer) | xgeResourceOutputSet | 单次精确复制，替换失败保留旧缓冲；当前调用失败后不发布半成品 |
| decompressZstd(output, bytes/Buffer) | xgeZstdDecompress | 调用方 Buffer 容量、实际输入长度、checked 格式/容量错误；返回实际输出量 |

Zstd 的输出是调用方 Buffer，遵循原生原地写入合同：失败时其中可能已有部分
写入，调用方应丢弃或重置内容；这不同于新建 Resource 的原子交付合同。

ResourcePath 与 ResourceOutput 是 callback 期视图。仅在回调内访问它们；保存后
再次访问会 checked 拒绝。需要保存 URI 时读取 path.text 得到语言 string 副本。
Owned Provider 成功返回的资源只拥有数据，不继续保留回调，避免隐藏的闭包引用环。

XPack 回调可以捕获实际的 pack 对象并调用其读取方法，无须把裸 pack/user 指针
交给语言使用者。原生仍保留显式 C Provider/XPack Provider，但它们借用的用户
状态必须按 C 合同自行保活，不能与新的拥有式入口混淆。

原生实现修复了可移动注册表和回调后的释放信息读取：每条注册是不可变的拥有记录，
登记/查找/摘除用短锁，调用任意回调时不持锁。加载和原始 C 结果分别持有真实引用，
Provider 自清空、替换小程序桥或 Init 都不会擦除当前释放信息。Unit 也清理初始化前
的注册。注册表元数据可并发登记/读取/清空；回调可变状态及单个 Resource 仍须由
调用方同步，不能据此声称语言变量自动线程安全。

成本包括注册对象/字符串、回调 Hold/Drop、快照引用、查找短锁、XPack 路径分配、
输出复制，以及 Resource.bytes 的语言值复制。原始 C Provider 的结果改为保留原
注册，省去每次加载再分配释放快照。没有新增原始算法、副本 XRT 或操作码分发。

本批还压实了通用 c_length 字节字段：void*/const void* 可作为明确长度的二进制
来源；文本仍只允许字符存储，错误指针和错误长度 ABI 必须拒绝。非空 bytes 字段
的 NULL+0 是空值；可空字段仍保留 null，原生按值返回视图的 NULL 合同保持独立。

专项覆盖 NUL、高字节、空数据、语言值独立性、回调期满、回调自清空、失败后恢复、
OOM 前缀、总体 live count/bytes、非法/重复释放、原始 XPack/小程序自移除，以及
四读线程与登记/清空的竞态。Zstd 使用本地固定的无压缩帧做正确性/容量检查。
没有进行网络连接、安全性质的测试或硬件性能宣称。

完整编译器、Native SDK/库、Windows 动态/静态包和验收证据必须配套。此批候选仍
在独立 F:/xlang-validation/xge-full-20261110/ 中；没有覆盖正式 release/IDE。
最终结果索引见 dev/release-materials/provenance/xge-resource-loading-20261110.json。

ResourceFree 保持私有生命周期入口，由 Resource.close、析构和创建失败回滚调用；
不把裸释放函数作为重复的公共方法。全量清单工具的“公开声明”计数不包含这类
私有辅助声明，不能把该数值当作功能完成率。

2026-11-10 最终验收：完整 O2/TinyCC/VFS 候选 `compiler-full-r9` 重建成功，
915.344 秒，构建输入无漂移。Windows 语言层通过 45 项进程检查（含 JIT、
静态 AOT、OOM、GPU 像素回归及 PE 导入表）；24 项通用字节字段检查包含
内嵌编译器的真实 JIT/独立 AOT，6 项原有视图回归通过；生成的 Resource
公开头通过 C11/C++11/C++17 严格编译。原生 Windows/Linux 各 24 项通过。

配套 Native 实现提交为 `0b9e76b4`，冻结源是 `native-source-r10`；
动态/静态包是 `packages-resource-host-r2`、`packages-resource-static-r2`。
Native 冻结清单记录的是前一提交加工作区差异；本批实际测试源码另由原生报告
逐项固定，不把后续文档修改当作引擎输入。
