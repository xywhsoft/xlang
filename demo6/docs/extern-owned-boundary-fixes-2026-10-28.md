# C 所有权返回链路：引用、传输及默认实参边界修复

本轮修复最后一次审计确认的六个问题。普通 `@owned string` 直接调用仍接管 C 返回的原指针；本轮没有改变运行时对象布局、分配器协议或所有权转移约定。

## 修复范围与阶段提交

| 问题 | 修复与边界 | 提交 |
| --- | --- | --- |
| `Array/List/Dict<error>` 安全索引将有效元素扩宽为 `error?` 后中止进程 | 编译器分别记录源 Error 盒身份和可空结果类型；IR 验证器与 C 后端共同验证来源，读取时不重装盒、不修改共享盒 | `727da4ce8` |
| Error 队列标量接口与批量接口使用不同槽位表示 | SPSC/MPSC/MPMC 标量 push/pop 复用 Channel 值编解码计划，按既有 `xvalue*` 槽位编码/解码；修复双向标量/批量互通 | `9db11dedb` |
| 单独的 `channel<error?>` 模块缺少 Error 编解码依赖 | 从同一编译期 codec 计划推导声明及依赖，覆盖独立接收模块，不再依赖 Array 等其他操作偶然引入符号 | `9db11dedb` |
| 实例方法默认实参中的 `self` 使用错误存储，逃逸默认闭包也可能丢失接收者 | 将已求值的接收者绑定到默认实参词法环境；隐式字段和显式 `self` 使用同一绑定，默认闭包按已有捕获 ABI 保留接收者 | `c508ac39b` |
| 绑定方法有默认实参时可通过语义检查，却不能绑定成省略参数的函数类型 | 认证完整来源签名，按经过验证的较短目标签名调用既有方法分派入口；仅省略有默认值的尾部形参，不放宽任意动态闭包转换 | `2ca9d8ecc` |
| extern 默认实参无法引用前序参数，或错误读取同名全局变量 | 顶层 extern、native create/destroy 和 extern 成员分别建立形参作用域，默认表达式只看已声明的前序参数；保留原默认实参求值及适配机制 | `3a54d4e95` |

代码主要在 `src/sem/xl6_sem.c`、`src/ir/xl6_ir*.c/.h` 和 `src/cgen/`；没有为这些修复增加运行时对象字段。

## 耐久回归

`tools/check_extern_owned_string.py` 已纳入以下用例，覆盖 JIT、独立 C 产物和 AOT 执行；该门禁同时调用 `tools/check_extern_owned_string_public.py` 的独立 C/IR 检查。

- `extern_owned_error_safe_index.xl`：非空及可空 Error 的 Array/List/Dict 安全索引、缺失元素、空接收者、离开容器后的引用，以及 64 个同步 OOM 前缀。
- `extern_owned_error_transports.xl`：三类队列的标量与批量双向互通、可空载荷和 Channel，以及 160 个同步 OOM 前缀。
- `extern_owned_error_channel_receive.xl`：独立接收模块，避免其他容器操作掩盖依赖遗漏。
- `extern_owned_method_receiver_defaults.xl`：默认实参的接收者、隐式字段、继承、单次求值、默认闭包及逃逸生命周期，以及 96 个同步 OOM 前缀。
- `extern_owned_foreign_prefix_defaults.xl`：前序形参、全局同名遮蔽、命名适配器、native 静态方法和任务调用。
- `extern_owned_bound_method_defaults.xl`：缩短签名的绑定方法、逐次默认实参求值、继承和返回本地实例绑定方法，以及 128 个同步 OOM 前缀。
- `extern_owned_native_bound_defaults.xl` 及配套 C：native 实例的 `@owned string` 方法，按省略默认参数的函数签名逃逸后调用。
- `xlir_error_transport_contract.c`：破坏源/结果类型身份、载荷类型、绑定方法来源认证和调用签名等六种 IR 合同，均须在输出任何 C/头文件字节之前拒绝。

本轮六种变异加上既有门禁的 39 种变异，共验证 45 种非法 IR。OOM 检查要求成功路径及异常展开均保持所有权平衡。

## 验收证据

使用隔离目录从源码构建的 Windows `driver-68` 和 Linux `linux-68`；没有覆盖主工作区的 release 二进制。Windows 构建记录的 2543 项输入在收尾时重新校验，全部一致。构建记录保留源码、SDK 头、对象和产物的哈希。

| 检查阶段 | Windows | Linux |
| --- | ---: | ---: |
| 公开编译链路专项门禁 | 149 / 149 | 149 / 149 |
| 独立 C 消费者及 C/IR 合同 | 29 / 29 | 29 / 29 |
| 任务相关定向回归 | 18 / 18 | 18 / 18 |
| 原始问题及对照用例：26 个用例 × emit/JIT/exe/AOT | 104 / 104 | 104 / 104 |
| 合计检查阶段 | 300 / 300 | 300 / 300 |

149 项结果中包含调用独立 C 门禁的汇总项；这里统计门禁执行阶段，并不将 300 解释为 300 个彼此独立的语言用例。

归档入口为本地忽略目录 `build/owned-string/reference-boundaries-68/acceptance.json`。其中记录以下原始结果和两份源码构建记录的路径、哈希以及验证状态：

- `acceptance-windows-68b/result.json` 与其 `independent-C/result.json`。
- `acceptance-linux-68/result.json` 与其 `independent-C/result.json`。
- `task-regression-{windows,linux}-68-focused/result.json`。
- `confirmation-reference-68/{windows,linux}/result.json`。

## 性能口径

普通 `@owned string` 直接返回继续通过原指针一致性、一次生产者分配及最终释放的 Windows/Linux 测试；本轮的来源身份与签名认证规则由编译器解析和验证，不额外创建适配闭包或增加一次方法调用层。

绑定方法继续使用现有 Value/callable 分派，其字符串返回投影仍有既有的复制成本。本轮恢复这条调用路径的正确性，没有将其改造成与直接 C 调用等成本的专用闭包 ABI。因此不能以这批功能测试宣称所有动态调用路径都已零拷贝。Error 队列同样复用原有值盒编解码，而非承诺原始 `xerror*` 直接入槽。

主线合入时保留其他线程尚未提交的修改；这些修改不属于上述隔离源码版本的验收结论。
