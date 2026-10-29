# 普通 string 接管 C 返回分配：专项验收

## 2026-10-30：条件回调、默认方法组和源码验收入口

解决 `review-boundaries-111` 的三项发现，未修改 XRT、字符串布局或运行时
分派协议。实现按阶段提交：

| 提交 | 调整 |
| --- | --- |
| `e58f5b684` | 源码宿主从选定 TCC 核心序列化资源，使用包含静态编译器对象的核心，恢复公开源码门禁 |
| `3badd410f` | 条件表达式和 `??` 分支接收精确回调签名，保持惰性接收者求值及原返回 ABI |
| `b44d82d3c` | 普通、extern 和签名声明的回调默认值在原作用域完成精确方法绑定 |
| `5e9074b5b` | Linux 源码宿主只链接并加载同一套 TCC 核心，避免 ELF 符号插入混用两套内部状态 |
| `7bfa60e2a` | 默认值仅通过较短函数签名调用时，也保留完整类型记录；补充嵌套、流收窄、赋值和类参数签名回归 |
| `59c97e30f` | 独立 C 生产者和内嵌 runtime 测试显式选择所需 XRT 声明，适配最新 SDK 的头文件依赖 |

编译器复用已有显式精确绑定：只在选中的语义上下文规范化方法叶子，重载
候选探测不执行这种规范化。访问、泛型约束、参数包、结果拥有权和已选声明继续
由原合同校验。没有新增包装调用层或运行时类型分支；任意擦除后的 var 分支
仍不能获得 compiler-only owner 回调 ABI 的静态授权。

| 验收阶段 | Windows | Linux / WSL |
| --- | --- | --- |
| 当前源码候选 | driver-125，1,378 单元，O0 | 1,377 单元的规范源码构建，随后重编唯一变动的语义单元 |
| C/头文件、JIT、AOT、动态库和嵌套编译交付门 | 9 步 | 7 步；无动态库步骤 |
| 字符串专项主门 | 257 步通过 | 257 步通过 |
| 独立 C O0/O2、IR 篡改、失败与退休合同 | 51 步通过 | 51 步通过 |
| 原审计的 25 个程序，各执行生成、JIT、AOT 构建和运行 | 100 步通过 | 100 步通过 |

主门包含独立 C 子门的调用步骤，表中分层记录，不将它们当作互不重叠的
程序数量。新条件回调扫描 512 个分配失败前缀，默认回调扫描 384 个前缀，
逐次核对 XRT 存活分配、外部分配器及 native 句柄平衡，并要求同时出现成功
和失败。两项新增 IR 合同验证一个接收者、一次目标调用、所需结果复制数，
并要求删除选择证据或破坏绑定时在 C/头文件输出前拒绝。

Linux 最后一次语义修改以完整规范构建为基线增量重编。GCC 实际依赖检查
覆盖 45 个编译器单元，确认只有 xl6_sem.c 依赖该变动；所有对象、SDK 头及
输入哈希重新核对。暂存树中 6,107 个构建输入与工作树逐字节一致。两端门禁
均使用真实发布 TCC 核心；未用替代运行时伪造源码通过记录。

同一原生条件回调的分配对照，在绑定完成后测量一次调用与返回准入：修复前
原始分支为指针改变、4 次 malloc、1 次 calloc；修复后普通写法与分支显式
转换写法均保留原指针，只有生产者自身 1 次 malloc，calloc/realloc/memdup
和区间内 free 均为 0。最终 Windows 候选的两分支 JIT/AOT 均验证此合同。
精确 cstring/cbytes 则保持原载荷指针和既有的一次 owner 记录分配。

上述区间不包含闭包绑定、接收者/代码生命周期和冷初始化。默认回调需要时
仍构造其既有闭包；普通返回签名要求的 owner 结果复制仍执行。没有声称
整个 OOP/闭包调用与手写 C 等速。

最终汇总及回执位于根目录 `build/owned-string/callable-final-125/`。
早期失败和中止记录保留，未计入通过项。验收使用隔离工作树中的已提交源码，
不把主工作区其他线程的未提交实现算入本轮测试范围。交付源码与测试，未替换
主工作区的 release 二进制；正式使用需要正常重建。

源码入口可复用 `tools/build_xlir_public_source_driver.ps1` 和 Linux 对应构建器，
随后运行 `tools/check_source_driver_delivery.py --build <build.json> --directory <新目录>`，
再运行 `tools/check_extern_owned_string.py --driver <源码驱动> --runtime-build <build.json>
--directory <新目录>`。Linux 还需设置 `XL6_SOURCE_NATIVE_ROOT` 指向其真实 TCC
配置和核心目录。所有输出目录必须全新。

2026-10-28 的最新类型化绑定方法、Error 提取和 nullable 实参验收见
[后续验收](extern-owned-typed-boundaries-2026-10-28.md)，合并主线后两平台各通过 329 个检查阶段。

上一批验收日期：2026-10-26；2026-10-27 的任务 C 标量补充见文末。该批修复可变参数函数值的编译期 OOM 清理、借用返回的
no_raise 复制失败传播，并补齐静态方法函数值。此前的 XACME 可空溯源返回、
C 可变参数文本准备、原生函数值与独立 C 消费者门禁继续保留。
源码构建冻结于 `e79b786d8`，包含主线 TLS peer-chain 更新；随后只追加文档
及旧调用测试夹具的 int64_t 可移植性修正，不改变编译器输入哈希。
实际输入与对象以各 receipt 的 SHA-256 为准，不能用既有 release 产物替代。

## 构建与运行

证据目录相对 demo6，位于隔离工作树的忽略目录 `build/owned-string/`。
Windows 使用公共源码驱动，Linux 使用实际 CLI；均执行 JIT、AOT 构建及
生成后的独立程序，C 消费者包含实际生成的公开头文件。

| 项目 | Windows | Linux / WSL |
| --- | --- | --- |
| 当前源码构建 | driver-33/build.json，1287 编译单元 | linux-12/result.json，1289 编译单元 |
| 字符串专项 | accept-33/result.json，66 项 | accept-linux-12/result.json，66 项 |
| 独立 C/XLIR 合同 | accept-33/independent-C/result.json，17 项 | accept-linux-12/independent-C/result.json，17 项 |
| XACME 原生与语言专项 | xacme-33/result.json，50 项 | xacme-linux-12/result.json，50 项 |
| 普通 cstring/cbytes 回归 | regression-33/result.json，36 用例 JIT/AOT、4 项所有权合同 | regression-linux-12/result.json，同一组 116 项检查 |
| 旧函数值和静态调用 | static-regression-33-portable/result.json，7 用例、21 项 | static-regression-linux-12-final/result.json，同组 21 项 |

上表均使用本轮冻结的源码候选。本次未改语法；此前语法差分见
parser-diff-3/xlir_parser_corpus.results.json：3533 个用例，没有接受状态或匿名函数 AST 数退化。

Windows 驱动 SHA-256：
`ebd4b37ca78008ddff3c6769683ea523cee7a24eef139315b8bc9b7ee06fa089`。
Linux CLI SHA-256：
`feaadad13ae1929547d4d9e8a8d5c67bb4e7a4dcd841a13700d66e43fd1732c5`；
其共享编译库 SHA-256：
`ecaf594d284ca454524365d21e6518e6766424c9f817fcef5161273d1c8b4186`。
输入与对象哈希均核对；Windows resident 对象为 O0，Linux 为 Os。
没有替换主工作区 release 二进制；正式产品使用须正常重建。

独立 C 消费者直接包含实际输出头文件，生成 C、生产者和消费者分别编译，
不经过 xlang 的 C sidecar 头文件注入。Windows 使用源码 SDK 镜像，Linux
显式镜像与 VFS 同版本的私有头文件。O0/O2 分别验证可空非空值、空缺值、
普通 string、多返回值、两种生产者失败及 120 个 OOM 前缀，存活数恢复基线。
另有 6 项 XLIR 入口/签名/元数据/捕获/回调擦除/实参扰动，均在 C 和头文件输出前拒绝。
编译期 OOM 用真实解析/语义/Lowering 模块定点注入：四个调用元数据数组和
首个参数名分别分配失败，均正常回滚；通用释放器也接受名称数组尚未分配的模块。

专项覆盖：原指针见证、生产者一次分配且接管零额外分配、一次释放、普通
借用返回不变、NULL 与空字符串、非终止失败载荷、无诊断失败、16 个 OOM
前缀及存活数恢复、函数值、实例方法/getter、协议与动态分派、包公开声明、
导入和 worker、公开 C 消费者的成功/可空/两类失败结果。另验证只转发可空
结果、无任何字符串操作的最小导出单元，确保回滚释放依赖不会缺失。
旧 cstring 与跨包用例、普通字符串公开返回回归和 14 项拒绝检查均在同一门中。
新增原生可变参数函数值还覆盖传参、别名、重绑定、临时函数值与临时字符串
实参、worker、C 默认提升、文本/二进制 span 和 32 个 OOM 前缀。计数检查
证明已构造函数值的接管调用只有生产者的一次 malloc，没有额外 owner/装箱/复制分配。
新增静态方法值覆盖普通/原生/async 方法、默认参数、显式重载选择、继承、
包导入、原生 C 回调和 32 个 OOM 前缀；固定和 C 可变参数自有返回保持原指针，
调用点只有生产者分配。private、歧义和缺少接收者的实例方法在生成 C 前拒绝。
借用返回的 OOM 覆盖直接调用、转发、固定/可变参数函数值、协议、getter 和
动态方法；失败不向后续 C 观察者传入 Data=NULL 的非空字符串，存活数恢复基线。
失败借用生产者返回地址 1，带诊断或无诊断时均先检查结果，不读取该地址。

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
本轮 test-wiring-33.log 仍报告既有 dynamic/primitive 前缀、过期 stdlib_xson
及 x509_public 等登记缺项。额外旧 cgen_c_callback_interop 夹具的
callback_box_destroy C 原型冲突在旧 driver-27 也可复现（callback-baseline-27.log），
未计入 7 个通过的旧调用用例。新静态 C 回调由独立的可移植夹具验证。

重跑专项使用 `tools/check_extern_owned_string.py --directory 新目录`，
Windows 配 `--driver`，正常 CLI 配 `--cli`。XACME 使用 tools/check_xacme.py，
同样须新目录和对应候选。完整 CLI 验收还须提供 --runtime-build：
Windows 公共源码驱动使用 build.json；本轮 Linux CLI 使用 result.json。
无回执的功能检查会标记 independentCConsumer=false，不能当作独立 C 验收。
全部证据保留失败日志，不覆盖旧目录；早期 Linux 缺少物理 SDK 的失败也保留。

## 2026-10-27：task.spawn 原生 C 标量参数

修复提交 `cc6b3bf55`，源码基线 `fc216d5aa`。共享任务参数准入复用既有的
20 种 C 标量分类，保留 DEFAULT/BYVAL、VALUE 和 trivial ownership 限制。
已有帧生成、调用、释放和校验程序直接消费这些类型；未修改运行时布局、
provider 或 Cgen，也未为此引入单库代理函数。

| 验收 | Windows | Linux / WSL |
| --- | --- | --- |
| 完整源码候选 | driver-38/build.json，1295 编译单元，O0 | linux-13/result.json，1297 编译单元，Os |
| 字符串专项 JIT/AOT/生成代码及拒绝项 | accept-38-final/result.json，71 项 | accept-linux-13/result.json，71 项 |
| 独立 C/XLIR 合同 | accept-38-final/independent-C/result.json，19 项 | accept-linux-13/independent-C/result.json，19 项 |
| 原始复现及已有任务回归 | task-regression-windows-38-focused/result.json，18 项 | task-regression-linux-38-focused/result.json，18 项 |

任务用例验证全部 20 种 C 标量，包含 uint64 全范围、带符号负值、平台相关的
long/size_t/ptrdiff_t/wchar_t 边界和 float/double。另检查实际生成帧的 20 个
字段均为对应 C 类型，避免仅凭数值往返误认归一化传输为精确原生传输。
固定 extern 和类静态 @owned 生产者均直接提交；覆盖可空结果、void、Future
复读、失败非终止载荷、无诊断失败、worker OOM 和 32 个调用线程分配前缀。
这些任务前缀检查失败传播和后续复用，不以 Future 就绪时的全局 LiveCount
宣称后台任务已经静默；原有独立 C 门的 120 个分配前缀继续验证字符串准入的
分配平衡。六种损坏签名/捕获合同，包括 byref/out 调用者存储，均在 C/头文件
输出前拒绝；原生 C 可变参数函数值也保持禁止直接提交的边界。

额外回归为原始自有返回复现、普通 c_int 返回、任务闭包参数 ABI、20 种
原生 Future 结果、f-string 中任务实参和共享闭包并发释放，均执行 JIT/AOT。
大型旧用例 cgen_task_spawn.xl 在新候选双端报告 TypeError；修改前的冻结
driver-33 复现相同错误（task-spawn-baseline-33.log），未计入通过项。
全局 test-wiring 仍有既存 dynamic/primitive 前缀、x509_public 登记和过期
stdlib_xson 引用问题，不能将本专项表述为全仓发布总门通过。

Windows 候选 SHA-256：
`2cfc299ee39914e8253691e2a46c5c6be2b790399574ea58080c0f3f34951972`；
Linux 共享编译库 SHA-256：
`4a8bc65400ad70c3cf27539d8c907ec5a5172af14e48bedb8e4bb4dab7ebfbb0`。
全部编译输入、对象和测试输入哈希已核对，汇总见 task-final-review-38.json。
本次未测量或声称整个任务调用与手写 C 等速；普通任务的既有捕获、发布和
字符串复制边界不变。主工作区 release 二进制未替换，正式使用须正常重建。

## 2026-10-27：原生函数值与任务组合的四项补齐

实现提交 `37a6d76ef`，合同与说明提交 `fbd336012`。验收源码包含主线
`17c270dc2` 的已提交内容；不读取其他线程未提交的实现。

- 顶层和 static extern 的省略默认参数可以绑定为较短函数值签名。extern
  参数没有局部符号时只省略参数重绑定，仍走普通默认表达式、拥有者和错误
  事务。验证原生/普通整数、借用字符串、多个默认值、每次调用执行以及失败
  时跳过生产者；没有给扩展库增加代理函数。
- 泛型 task.spawn/all/race/whenAny 在特化后重算载荷 ABI，使用与 Typed XLIR
  相同的合同校验。七种 spawn/聚合形式均覆盖，包括字面量列表和已存储数组、
  uint64 全范围、托管字符串及生产者失败后的复用。
- 非空流证明在解析 typedef 后继续传入任务绑定和 lowering。函数值、普通
  捕获参数和可变参数尾部消费已经物化的值类型；无证明或赋值后证明失效的
  可空函数必须拒绝。损坏 IR 时删除该证明也必须在 C/头文件输出前拒绝。
- task.all 使用既有 native scalar codec 聚合全部 20 种 C 标量，保留宽度、
  符号、平台限制和动态身份。c_bool 沿用共享 bool 单例，类型化投影仍返回
  c_bool。另以 C 端位模式检查负零、quiet NaN 载荷及无穷值。

| 验收 | Windows | Linux / WSL |
| --- | --- | --- |
| 源码候选 | driver-43/build.json，1295 编译单元，O0 | linux-16/result.json，1297 编译单元，Os |
| 字符串专项 | accept-windows-43/result.json，89 项 | accept-linux-local-43/result.json，89 项 |
| 独立 C/XLIR 合同 | accept-windows-43/independent-C/result.json，21 项 | accept-linux-local-43/independent-C/result.json，21 项 |
| 已有任务回归 | task-regression-windows-43-focused/result.json，18 项 | task-regression-linux-local-43-focused/result.json，18 项 |

独立 C 门继续运行 O0/O2、120 个分配失败前缀、六项原生可变参数合同、六项
任务标量合同和五项元数据分配/部分销毁控制，并新增九项任务/载荷/非空证明
破坏控制。未用 Future 就绪时的全局分配计数推断后台任务已经退休。

Windows 候选 SHA-256：
`6bff1ecc2d1a7ed6aac896c932d51ac8334670eb18c24871bc91805953ac9217`；
Linux 共享编译库 SHA-256：
`5b822de3066b5121990815331c0010cf653d212b25cae94fae452404f0183dba`。
编译输入、对象、暂存头文件和测试输入重新核验，见 task-final-windows-43.json
及 task-final-linux-43.json。Linux 最终运行使用本地临时目录中的同字节程序和
9518 个同字节源码/测试文件，逐一比对原工作树及归档哈希，证明见
linux-core-43a-verified.json。之前挂载盘运行的超时记录保留，不计入通过项。

没有为普通 string 增加分支，也没有修改 xrt 或增加运行时泛型分派。
task.all 的结果数组和元素装箱仍有既有成本；本轮不是整条任务调用零开销的
性能测量。全局 test-wiring 仍报告既有 x509_public 登记、dynamic/primitive
前缀及过期 stdlib_xson 引用，新增用例没有登记缺项；不宣称全仓总门通过。
本轮不替换主工作区发布二进制，正式使用须正常重建。

合并时继续同步主线 `ac4b1ced5`。相对验收候选，唯一变化的编译输入是另一
线程 `b00919e2d` 的 xl6_xacme.c 局部数组初始化兼容性修复；本轮语义、IR、
Cgen 和任务运行时输入均未变化。额外运行 check_xacme_c_initialization.py：
GCC O0/O2/Os 的旧、新及显式诊断行号对照共九次编译通过，三组对照对象
逐字节相等；嵌入式 TCC 的旧版失败、新版成功控制也通过。证据为
merge-xacme-43/results.json。这是合并差异的 C 编译控制，不冒称已验收
ACME 业务或整个主线发布产品。
