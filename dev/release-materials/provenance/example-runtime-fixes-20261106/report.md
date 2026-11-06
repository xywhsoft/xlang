# 指定范例修复验收

改动已合入主线，主线 `release/win_x64` 的 55 项、`release/linux_x64` 的 48 项 JIT/独立程序验收全部通过。Redis 在两个平台均使用临时端口、随机认证密码和独立键前缀，JIT 与独立程序均通过；串口使用 LOOPBACK。Windows 剪贴板写入测试使用独立 window station。

| 原问题 | 实际处理 |
| --- | --- |
| cli / com / clipboard 拒绝原生容器 | 增加公共 `xl_value_read.h`，认证原生 array/dict 拥有者后借用槽位；删除输入快照需求，包清单显式声明驻留符号，JIT/AOT 均选择同一按需组件。 |
| shapes / xneural abort | 基类表示投影改用封闭的名义类型准入集合，保留原对象；显式错误类型转换仍走 checked 路径。两套原范例分别通过 43 / 30 项内部检查。 |
| dynobj 静态字段动态索引 | 统一全局/局部擦除值的 `[]` 读写与 object 动态字典协议。`.id` 访问声明字段，`["id"]` 访问动态字典；二者可独立同名。修正范例中不符该协议的断言。 |
| prodcon checked 合同 / 等待 | 补全全局、局部和类字段 channel 默认构造；保存 joinable thread 的拥有者，避免循环内丢弃临时 thread 时提前 join。3 个生产者、2 个消费者完整收发 10 项。 |
| serial 退出阶段错误 | 上游 XRT 修复 finalizer 最后存活借用下的字段替换，避免再次 retain 正在析构的接收者；同步到 xlang 固定快照。无需硬件。 |
| copy_clone / class_c_export / deepclone | 修正浅复制共享嵌套引用、方法实际返回值，以及 Set 中对象身份语义的范例断言。 |
| 11_concurrency | 将空 future 的类型错误捕获移至实际发生的构造边界；等待取消进入终态后再构造竞争失败；同步空 thread 动作的当前错误类型。 |
| stdlib_fs_advanced | 通过 `time.fromUnixMs` 构造运行时原生时间，避免将手写 Unix 微秒值直接传给 `setTimes`。两个平台均通过。 |
| COM C 边界补充检查 | 错误路径仅清理已经初始化的 VARIANT；拒绝非数组和超过 64 项参数时不会清理越界/未初始化槽位。去掉重复初始化。 |
| 独立 C 范例 | 提供编译期 wire-only 测试适配，继续只链接单份 XRT；CLI/clipboard 的 C 源 JIT/AOT 及 CLI 48 组、83 个分配故障点通过。语言包使用完整驻留原生视图。 |

读取测试将“下一次分配”设为失败，再打开原生 array/dict 视图并连续读取 2,000 次，Windows、Linux JIT/AOT 均通过；同时检查嵌入 NUL、中文、稳定指针、无 owning scalar shell、越界、复制 scope、未知 handle 策略及重入写拒绝。C 边界另通过 CLI/defaults、COM、隔离剪贴板测试。

这里承诺的是输入读取没有容器快照、字符串复制和堆分配。仍有作用域认证、原生拥有者 pin/unpin、修改保护和槽位类型判断；64 位平台 scope 为 136 字节，借用描述符为 32 字节。CLI 将原生字符串默认值放入独立拥有的 XRT 返回结果时仍需复制字符串；已有 wire xvalue 默认值可以保留引用。COM BSTR 和剪贴板 UTF-16/CF_HDROP 转换沿用 Windows API 的要求。因此本次不宣称整个调用与手写 C 的指令开销完全相同。详细合同见 `docs/native-container-read-views.md`。

同一接收者的 scope 独占且同步，不能跨 await、线程或复制活动 scope；未知策略和不支持的原生槽位布局 checked 拒绝，不回退到复制。此次是指定范例与对应边界的验收，未执行整个项目的全部验收矩阵。

补充发现：`xl c-bridge --json --c-environment` 在捕获的原始 0.6.0 DLL 和新 DLL 上均报告 `cannot collect bounded native C configuration and builtin headers`。基线与当前证据保存在本目录；这是本轮未修改的既存描述器问题，不影响上述范例的运行和构建。

核心阶段提交包括 `dca0f1a64`、`b6b646556`、`de07d1225`、`01ab116a4`、`397a34279`；后续 COM、安全测试、独立 C 兼容和发布物分阶段提交。上游 XRT 已提交并合入 `e2a0b36d`、`0e90f84f`、`0a44e3f8`。并发线程的主线提交、未提交语义补全改动和旧 release 目录迁移删除均已保留；本轮未重建 IDE。

`receipt.json` 记录产物哈希、构建来源和测试范围；源码与构建记录在 sibling `dev/release-materials/sources/example-runtime-fixes-20261106/`。历史验收材料保持原字节，只更新活动交付索引。没有执行外部发布。
