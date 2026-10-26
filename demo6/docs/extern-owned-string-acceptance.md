# 普通 string 接管 C 返回分配：专项验收

日期：2026-10-26。功能代码冻结于 `939244254ec1388ef80f1bbbe3a6d123f6869e52`；
后续为文档补充及独立 tar 修复合并。编译器和用例的实际输入以各 receipt 的
SHA-256 为准，不能用预先存在的 release 二进制代替本批候选。

## 构建与运行

证据目录相对 demo6，位于隔离工作树的忽略目录 `build/owned-string/`。
Windows 使用公共源码驱动，Linux 使用实际 CLI；均执行 JIT、AOT 构建及
生成后的独立程序，C 消费者包含实际生成的公开头文件。

| 项目 | Windows | Linux / WSL |
| --- | --- | --- |
| 当前源码构建 | driver-10/build.json，1276 编译单元 | linux-9/result.json，1278 编译单元 |
| 字符串专项 | windows-current/result.json，44 项 | linux-current/result.json，44 项 |
| XACME 原生与语言专项 | xacme-windows-current/result.json，47 项 | xacme-linux-current/result.json，47 项 |

Windows 驱动 SHA-256：
`50fe32cae287295231df6ffef1df43651b5f68216e3105a0afcc332259703dcc`。
Linux CLI SHA-256：
`feaadad13ae1929547d4d9e8a8d5c67bb4e7a4dcd841a13700d66e43fd1732c5`；
其共享编译库 SHA-256：
`f8605f0c307a3987453b0e9e8bff7c2c1f19334ffa88bb428289e4fe7732a895`。
输入与对象哈希均重新核对；Windows resident 对象为 O0，Linux 为 Os。
未替换主工作区的 release 二进制，正式使用须正常重建产品。

专项覆盖：原指针见证、生产者一次分配且接管零额外分配、一次释放、普通
借用返回不变、NULL 与空字符串、非终止失败载荷、无诊断失败、16 个 OOM
前缀及存活数恢复、函数值、实例方法/getter、协议与动态分派、包公开声明、
导入和 worker、公开 C 消费者的成功/可空/两类失败结果。另验证只转发可空
结果、无任何字符串操作的最小导出单元，确保回滚释放依赖不会缺失。
旧 cstring 与跨包用例、普通字符串公开返回回归和九类拒绝检查均在同一门中。

XACME 使用 `D:/GIT/xrt-xacme-bridge` 的本地模拟 CA/DNS，无云凭据。
覆盖基本接口、自有结果、模块退役、签发/续期/存储流程及独立范例 read_account；
独立 cryptography 消费者还核对已提交证书与私钥的公钥一致。
API 清单 43 个函数、38 个常量及上游快照检查通过。

## 性能结果与限制

接管直接生成 `result.Data = native(...); result.Size = strlen(result.Data);`，
没有正文复制、C owner 分配、运行时来源标签或逐次读取分支。可失败入口仍须
处理原有错误作用域；非空 NULL 和过长结果必须失败。普通字符串布局不变。

`benchmark-3/result.json` 比较实际生成的私有可空 no-raise 转发函数和手写 C，
两者使用同一 noinline 生产者、XRT 分配、strlen 及释放：32 字节分配、9 字节
文本，每轮 300000 次，7 轮交错执行，Windows GCC O2 编译测量代码。

| 测量 | 每次中位耗时 |
| --- | --- |
| 手写 C | 356.667 ns |
| 实际生成函数 | 456.667 ns |

此样本完整转发函数约慢 28.0%，生成函数中仍有一次既有 Slot runtime Apply。
测量不含公开 C 导出的 entry 事务，也不构成发布版、其他长度或整个库的性能
保证。不能将本批的零复制、零额外分配表述为整个语言调用零损耗。
动态装箱的 realloc、绑定实例方法解箱，以及普通赋值/捕获/任务的复制边界
见 [功能合同](extern-owned-string.md)，本批没有改写这些已有设计。

## 门禁范围

四个源码构建 profile 的 shipping guard 及五项负向控制通过，记录在
shipping-current.json。该静态检查不是发布二进制测试。
全局 test/example/docs 索引检查仍有既存失败，日志分别为
test-wiring-current.log、example-wiring-2.log、docs-index-final.log；
包括未登记用例、过期 stdlib_xson 引用和其他标准库文档缺项。
本功能自己的用例、范例和文档入口已登记；不据此宣称全项目总门通过。

重跑专项使用 `tools/check_extern_owned_string.py --directory 新目录`，
Windows 配 `--driver`，正常 CLI 配 `--cli`。XACME 使用 tools/check_xacme.py，
同样须新目录和对应候选；全部证据保留失败日志，不覆盖旧验收目录。
