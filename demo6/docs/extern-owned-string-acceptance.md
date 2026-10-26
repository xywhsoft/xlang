# 普通 string 接管 C 返回分配：专项验收

日期：2026-10-26。本轮补齐 XACME 可空溯源返回、no_raise 可变参数文本准备、
原生可变参数函数值与独立 C 消费者门禁。编译器源码构建冻结于
`f184dccc84d8854f72989686c67d781b4c58b8b8`；随后只追加测试和文档。
后续合入的 ui.auto 调整不改变这两个构建回执的编译器输入哈希。
实际输入与对象以各 receipt 的 SHA-256 为准，不能用既有 release 产物替代。

## 构建与运行

证据目录相对 demo6，位于隔离工作树的忽略目录 `build/owned-string/`。
Windows 使用公共源码驱动，Linux 使用实际 CLI；均执行 JIT、AOT 构建及
生成后的独立程序，C 消费者包含实际生成的公开头文件。

| 项目 | Windows | Linux / WSL |
| --- | --- | --- |
| 当前源码构建 | driver-27/build.json，1286 编译单元 | linux-11/result.json，1288 编译单元 |
| 字符串专项 | accept-27-final/result.json，55 项 | accept-linux-11-final/result.json，55 项 |
| 独立 C/XLIR 合同 | accept-27-final/independent-C/result.json，15 项 | accept-linux-11-final/independent-C/result.json，15 项 |
| XACME 原生与语言专项 | xacme-27/result.json，50 项 | xacme-linux-11/result.json，50 项 |
| 普通 cstring/cbytes 回归 | regression-26/result.json，36 用例 JIT/AOT、4 项所有权合同 | regression-linux-10/result.json，同一组 116 项检查 |

36 用例回归在合入后续 TLS 提交之前的 `6f1d4457eadaac12d46cf0fd772536e2192754bc`
完成；最终源码构建又执行上表字符串和 XACME 专项。语法差分见
parser-diff-3/xlir_parser_corpus.results.json：3533 个用例，没有接受状态或匿名函数 AST 数退化。

Windows 驱动 SHA-256：
`41f4e0815b1b54a84bfdcaccd8bbe4f3fa0bf3fab6e58707ac958527aeb0a960`。
Linux CLI SHA-256：
`feaadad13ae1929547d4d9e8a8d5c67bb4e7a4dcd841a13700d66e43fd1732c5`；
其共享编译库 SHA-256：
`2c46638f2cdd6d46a199779f7d57f238f0021a7983b542c30acf7adf3e7b77b0`。
输入与对象哈希均核对；Windows resident 对象为 O0，Linux 为 Os。
没有替换主工作区 release 二进制；正式产品使用须正常重建。

独立 C 消费者直接包含实际输出头文件，生成 C、生产者和消费者分别编译，
不经过 xlang 的 C sidecar 头文件注入。Windows 使用源码 SDK 镜像，Linux
显式镜像与 VFS 同版本的私有头文件。O0/O2 分别验证可空非空值、空缺值、
普通 string、多返回值、两种生产者失败及 120 个 OOM 前缀，存活数恢复基线。
另有 6 项 XLIR 入口/签名/元数据/捕获/回调擦除/实参扰动，均在 C 和头文件输出前拒绝。

专项覆盖：原指针见证、生产者一次分配且接管零额外分配、一次释放、普通
借用返回不变、NULL 与空字符串、非终止失败载荷、无诊断失败、16 个 OOM
前缀及存活数恢复、函数值、实例方法/getter、协议与动态分派、包公开声明、
导入和 worker、公开 C 消费者的成功/可空/两类失败结果。另验证只转发可空
结果、无任何字符串操作的最小导出单元，确保回滚释放依赖不会缺失。
旧 cstring 与跨包用例、普通字符串公开返回回归和 11 项拒绝检查均在同一门中。
新增原生可变参数函数值还覆盖传参、别名、重绑定、临时函数值与临时字符串
实参、worker、C 默认提升、文本/二进制 span 和 32 个 OOM 前缀。计数检查
证明已构造函数值的接管调用只有生产者的一次 malloc，没有额外 owner/装箱/复制分配。

XACME 使用 `D:/GIT/xrt-xacme-bridge` 的本地模拟 CA/DNS，无云凭据。
覆盖基本接口、自有结果、模块退役、签发/续期/存储流程、旧证书缺失 CA 溯源
的可空返回与真实 IO 失败，以及独立范例 read_account；
独立 cryptography 消费者还核对已提交证书与私钥的公钥一致。
API 清单 43 个函数、38 个常量及上游快照检查通过。

## 性能结果与限制

接管直接生成 `result.Data = native(...); result.Size = strlen(result.Data);`，
没有正文复制、C owner 分配、运行时来源标签或逐次读取分支。可失败入口仍须
处理原有错误作用域；非空 NULL 和过长结果必须失败。普通字符串布局不变。

先前冻结代码的 `benchmark-3/result.json` 比较私有可空 no-raise 转发函数和手写 C，
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

原生可变参数函数值另外读取自身的静态效果/拥有权元数据；返回字符串时
按合同接管或复制。C 文本尾参执行 NUL 检查；可失败调用/准备保留 foreign
scope。no_raise 且没有可失败文本准备的自有可空返回可跳过该 scope。
没有为普通 string 增加来源判断，本轮没有测量或声称函数值整条调用等速。

## 门禁范围

先前四个源码构建 profile 的 shipping guard 及五项负向控制记录在
shipping-current.json；该历史静态检查不是本轮发布二进制验收。
全局 test/example/docs 索引检查仍有既存失败，日志分别为
test-wiring-current.log、example-wiring-2.log、docs-index-final.log；
包括未登记用例、过期 stdlib_xson 引用和其他标准库文档缺项。
本轮 source-wiring-26.log、test-wiring-26.log 和 c-fixture-wiring-final.log
仍报告全局既有索引缺项，例如 stdlib_xson_c.c 过期引用。本轮新增文件的
对应引用已登记，未出现在这些缺项中；不据此宣称全项目总门通过。

重跑专项使用 `tools/check_extern_owned_string.py --directory 新目录`，
Windows 配 `--driver`，正常 CLI 配 `--cli`。XACME 使用 tools/check_xacme.py，
同样须新目录和对应候选。完整 CLI 验收还须提供 --runtime-build：
Windows 公共源码驱动使用 build.json；本轮 Linux CLI 使用 result.json。
无回执的功能检查会标记 independentCConsumer=false，不能当作独立 C 验收。
全部证据保留失败日志，不覆盖旧目录；早期 Linux 缺少物理 SDK 的失败也保留。
