# XLIR 批次收敛流程

按用户 2026-09-10 的要求，后续以“分批发现、按根因修复、批次统一验证”推进。
语言及统一 IR 的完成标准不变；测试通过数不是完成百分比，合法语言能力不能
通过改成预期拒绝或放宽验证器结项。

## 执行方式

1. 固定一批源码及输入，用明确清单集中发现问题。独立专项失败后继续收集，
   不在同一个批次运行中编辑编译器、测试或运行时依赖。
2. 把失败归并为类型接纳、IR 表达、lowering、所有权、运行时等根因。一项
   根因可以覆盖多个失败，但每个源级反例仍保留自己的预期行为。
3. 批量修复同一语义主题，期间只做必要编译和快速自测。源码稳定后统一跑
   受影响专项；跨主题批次完成后再执行完整重型回归。
4. 报告已关闭、仍未解决、新发现及重新出现的问题。新的覆盖可能增加总数，
   不隐瞒新问题以制造单调下降。最终验收必须来自同一源码版本。

## 批次入口

`tools/run_xlir_batch.ps1 -ManifestPath tools/batches/xlir_generator_lifetime.json`

清单 `SchemaVersion: 1`、`Name` 和非空 `Gates` 明确选择独立的
`check_xlir_*.ps1`，不隐式执行所有套件，也不调用 `common.bat`。
`-ListOnly` 只列出选择，不构建、不运行测试。

- 每个专项都保存 stdout、stderr、真实子进程退出码、耗时和脚本指纹；
  专项失败不会让后续独立专项停止，整批仍返回非零。
- 启动或调度故障与专项失败区分，尚未执行的项目不得计为通过。
- 批次前后检查编译器、用例/宿主、脚本、运行时及发布依赖输入指纹；变化
  使整批失效，即使子进程全部返回零也不能宣称同源码通过。
- 默认使用唯一报告目录，拒绝覆盖已有报告或复用日志目录。
- SchemaVersion 1 和未隔离的旧专项继续独占执行。SchemaVersion 2 明确提供
  Script、Arguments 和非空 Resources；只有资源不相交的专项才允许并行，
  `*` 表示整个工作区独占。入口仍用活文件句柄防止多个批次互相覆盖；并发上限
  由 MaxParallel 指定，默认仍为 1。资源声明必须先审计真实文件写入位置，
  不根据测试名称或“看起来独立”自动推断。
- 两个源码驱动构建器现共用 `tools/xlir_c_object_cache.ps1`。每次重新预处理
  实际 C 输入，按完整预处理内容、源路径、编译参数、GCC/cc1/as 及伴随 DLL
  指纹复用对象文件；保留注释中的 fallthrough 警告语义。不按时间戳跳过构建，
  不缓存链接或测试通过结果。未知编译选项拒绝；未纳入输入的汇编文件指令、
  PCH 或工具路径覆盖保守禁用复用。损坏缓存保留原件并重新编译。
- 构建器要求新输出目录，记录每单元预处理/编译耗时、命中/拒绝、对象指纹，
  并在结束复核输入和工具链。`-NoCache` 提供独立重编译对照。

入口自测：`test/cases/xlir_batch_runner_test.ps1`。测试使用隔离的临时工程，
验证失败后继续、进程异常、输入变化、证据防覆盖和清单校验。

对象生命周期的整组入口为 `tools/run_xlir_ownership_batch.ps1`，接收一次已
完成的 PublicDirectory、ContractDirectory 和新的 Directory。四个合同驱动
分别为公共生成、module IR、frame IR、generator IR，生成驱动不能用仅执行
IR 断言的模块驱动代替。该入口汇总 frame、generator、module、原对象逃逸、
常驻 finalizer 和原公开入口六个专项，默认并行上限 3。

公开驱动每次调用使用输出前缀相邻的私有临时目录；对象和模块 TCC 宿主显式
传入私有生成文件/声明头搜索目录。没有更改原运行断言、次数或失败退出码。
`-Phase discovery` 是集中发现记录，不能当成发布准入；源码修复稳定后使用
新的目录执行 verification。每轮报告本批全部结果，原硬门仍红时整批返回非零。

## 下一修复批次：generator 拥有值与实例化上下文

| 根因 | 当前证据 | 验收要求 |
| --- | --- | --- |
| generator 产出类型复用通道复制白名单 | 对象产出在正式前端被拒绝；隔离副本接纳 | 使用语言结果值规则，IR 精确验证转移；不放宽通道合同 |
| 字段初始化声明来源与实际执行上下文未充分区分 | 隔离驱动在 finally 内构造对象时被 closing CFG 拒绝 | 显式且可验证的实例化上下文，保留诊断来源与异常路线 |
| 拥有型产出在关闭及析构失败时的优先级尚未证明 | 已有 item outcome 源程序及运行断言，尚未生成通过 | 正例严格运行、失败注入、精确析构与畸形 IR 拒绝 |

已有 5F71A07C 源码的 505 个生成用例、关闭专项及影响面结果继续有效，
不为启动新流程重跑这些已完成检查。生产源码发生本批修复后，才统一重验。

### 首轮集中发现结果

同一批 10 个源程序分别使用正式 v35 驱动与只修改结果值接纳规则的隔离 v1
驱动，共执行 20 次，完整收集仅约 4 秒。驱动复用自已有构建，不含重新编译
编译器的时间。所有程序均完成收集，不能将收集脚本退出零视为语言验收通过。
证据位于 `build/tmp/xlir_generator_item_batch_v4_be31d3f303184db786f62b004fa09853/results.json`。

正式驱动 1/10 生成通过；隔离驱动 4/10 生成通过。该批是主动选择的缺口探针，
不是全仓接纳率；三个新接纳的对象/array/函数值样例尚未实际运行。

| 根因编号 | 归并后的证据 | 状态 |
| --- | --- | --- |
| GI-01 结果值接纳 | 对象、array、函数值在隔离规则下恢复生成；正式规则仍拒绝 | 修复仅在隔离副本，需与下列合同共同验收 |
| GI-02 实例化执行上下文 | 对象产出清理、标量 generator 的 finally 内构造、嵌套字段初始化均被关闭边界拒绝 | 三种源形态复现，同一 IR 表达问题 |
| GI-03 yield 转换的失败效果 | `generator<var>` 产出整数需装箱，yield 缺少 canonical 异常退出计划 | 新发现，不能误计为 GI-01 已解决 |
| GI-04 frame 接纳 | `var` 参数仍被独立 frame 白名单拒绝 | 已知未完成，不通过禁止捕获结项 |
| GI-05 嵌套载荷 ABI | `generator<future<int>>` 在隔离前端后被 type pool 拒绝 | 独立能力缺口，需明确结果转移及释放 ABI |

下一步按这些根因修复，而不是遇到第一个初始化错误就反复跑全量。修复期间
保留源码快照和快速探针；一批稳定后统一进行严格 C、运行时故障/内存、IR 变异
与受影响专项验证。上述发现阶段未修改生产编译器或 Cgen，未重建发布物或修改独立 XRT。

### 第一批修复与快速重验

生产源码随后落地 GI-01 的结果值接纳规则和 GI-03 的 yield 元素转换效果。
`xl6IrReturnHasCheckedXValueErasure` 原先把 yield 的目标当作函数返回的
generator 句柄；现在从 canonical generator TypeId 提取元素 TypeId，再使用
已有的隐式转换效果合同生成异常退出计划。没有给 Cgen 增加特殊生成逻辑。

新源码指纹为 `356CBF6251BFD3C34B1235A75450D6BFF2379144839D01283C438553288A9D1B`，
两个生产文件的差量快照见 `build/tmp/xlir_generator_item_v6_checkpoint.json`。
语法检查退出零（a2aa38），同源码通用驱动构建退出零（4dedd9）。一次构建后
复用驱动执行相同 10 个探针，正式源码从 1/10 生成通过提高到 5/10，失败数
从 9 减为 5（df9ab1）。证据为
`build/tmp/xlir_generator_item_batch_v7_827ed0624b094a64ab46dc202763b3e6/results.json`。

对象、array、函数值与 var 四项恢复生成后统一严格 C 编译并各运行 100 次，
全部结果正确，逐例及全进程内存平衡（8762cc）。这 400 次是正常耗尽的基础
运行证明，尚不包括分配失败、析构失败、提前关闭、JIT/AOT 和对应 IR 变异；
不能据此将 GI-01/GI-03 的完整验收标为完成。

当前余下 5 个失败分别归于 GI-02 的三种初始化源形态、GI-04 的 var 捕获、
GI-05 的 Future 产出 ABI。旧 5F71A07C 的 505 样例及重型回归只证明旧快照，
不转记为 356CBF62 的结果。等本批其余修复稳定后再统一执行重型验收。

### 第二批修复：显式实例化上下文与拥有型载荷

GI-02 现在由 XLIR 的 `instantiations` arena 表达：每次字段初始化记录所属函数、
实例化位置、布局、字段、receiver、嵌套父上下文、连续指令区间和唯一提交指令。
诊断 origin/sourceSpan 保留声明位置，executionOrigin 单独表达执行位置。
关闭 CFG 使用验证过的执行来源；Cgen 不追溯 AST，也不按样例放宽边界。
验证器拒绝上下文缺失、错配、区间漏洞、非法嵌套，以及连同所有注解一起删除
上下文并重建 generator 缓存的伪造。原生包装器的字段提交同样必须记录上下文。

中间快照 `177B631E` 的同组生成提高到 8/10，剩余两项继续归于 GI-04/GI-05。
随后 generator 参数改用语言结果值接纳规则，byref/out 限制不变；实际捕获的
copy/retain/take 和析构仍由 XLIR frame 合同验证。Future 产出增加显式
`MOVE_FUTURE` ABI，Cgen 根据已验证的 yield ABI 选择释放动作，不从类型补猜。

当前生产源码为 `CC92C4BE8F3A49AE5157DD0297B6D1A4680DE3AE98B92CE3B7B0B6D8FADB9B27`，
358 项源码指纹复核一致；差量快照见
`build/tmp/xlir_generator_payload_v25_checkpoint.json`（基于 v17/v6/v39 链）。
同组生成失败数依次为 **9 → 5 → 2 → 0**，即当前 10/10 通过。
这只关闭本组生成复现，不代表五类语义能力的全部验收完成。

冻结此源码后统一验证，现有终态证据如下：

- 既有样例 505/505 生成通过：v20 fast 503 项及 task/example 两个慢项的
  独立 receipt 均已 `passed`、退出零，源码及驱动指纹前后一致。
- v22 两专项批次通过，3,793 项输入前后相同。六种载荷基础运行合计 600 次；
  实例化基础运行 300 次与拥有型 item 关闭/错误优先级 2,700 次全部内存平衡。
  两专项每轮分别拒绝 18/246 项 IR 变异，各执行两轮，并检查 C/H/JSON 确定性。
- 新的 `xlir_generator_payload_lifetime` 独立严格运行通过 2,400 次：var/Future
  两种产出、未启动销毁/自然结束/两种提前退出、正常/诊断错误/false-NULL 析构。
  调用方先释放 seed，证明 frame 保活；逐例检查唯一析构、finally 次数及内存。
  此处 Future 为 ready Future，不构成 pending/cancel 或分配失败证明。

生命周期用例已纳入固定 payload 门，并补入 yield 装箱失败效果、各相关表一致
伪造 Future 裸指针复制合同的反例。十门统一影响面批次 v24 已启动，报告为
`build/tmp/xlir_generator_payload_v24_lifetime_batch.json`；本段写入时尚未全部终止，
不能将旧快照重型回归或新增反例仅已启动的状态计为本次通过。

剩余验收包括真实分配失败、pending Future/cancel、其他合法结果类型与复杂
所有权组合，以及长期目标中的 native/callable、并发载荷、DLL 安全卸载、
同源码发布准入和 legacy 退役。此次未重建发布物、未修改独立 XRT 或运行 common.bat。

### 同源码后续发现：保留新问题，不混入已收敛计数

v24 已有九门终态通过，最后 loop cleanup 门仍在运行。payload 固定门的加强
反例真实通过：每轮共 34 项变异（包括一致伪造 Future ABI 和 yield 转换效果），
两轮确定性通过，600 次基础运行加 2,400 次生命周期运行内存平衡。批次总结果
仍须等最后门及最终输入指纹复核，不能将九个专项通过写成整批通过。

等待长门时，使用同一个 v18 驱动在 `build/tmp` 的隔离产物上追加下一轮发现，
不改生产或固定测试输入，不重复构建编译器：

| 编号 | 复现与归因 | 当前状态 |
| --- | --- | --- |
| GP-01 error 产出 | `generator<error>` 参数和产出通过前端后在 IR type pool 被拒绝；yield ABI 未覆盖 error | 生成失败，待显式转移/释放合同与运行验证 |
| GP-02 嵌套 generator 产出 | `generator<generator<int>>` 在 IR type pool 被拒绝；缺少相应产出拥有权合同 | 生成失败，需同时证明内外流关闭及失败优先级 |
| GP-03 创建失败被捕获后仍向外传播 | frame/handle 两个分配失败点中，catch 已执行，但导出入口仍失败；var 和 int 两种产出都复现 | 真实运行缺陷，必须修正创建错误结果与作用域合同 |

四种类型探针的全部结果保存在 `build/tmp/xlir_generator_payload_v26_discovery.json`。
另两种可空整数/对象均生成通过且各 100 次严格运行内存平衡（cb7edd）。
GP-01/02 与旧十探针不是同一分母；本次是新增两个类型复现，不是旧样例回归。

GP-03 的 var 探针先测得四个分配点，再用独立进程收齐八组捕获/不捕获故障，
其中六组通过，捕获 frame/handle 创建失败的两组退出 3。两个 yield 装箱
分配失败能够正确被捕获，不能因此把本缺陷归于装箱转换。全部退出码及日志
见 `build/tmp/xlir_generator_payload_v28_fault_discovery.json`。测试宿主的一处
诊断 signedness 警告先修正后才得到运行结果，未降低严格 C 编译标准。

v29 改为 scalar generator，基线分配数为二；四组失败注入中未捕获路径通过，
两个捕获路径再次失败且打印 `catch body executed`（c7af79）。生成 C 的 catch
跳转存在；创建失败清理中的 `xl6ErrorScopePropagateFailure` 提前锁存到导出
入口的外层 scope，catch 取走诊断后不能清除这个失败结果。下一批应将 launch
操作的结果/错误物理 ABI 与局部错误作用域表达完整，并补 false-NULL、捕获
清理的首错误优先级及畸形 IR 反例；不得靠清空外层 scope 或识别 catch 修补。

新发现计数为三类待处理问题；原十探针的零失败只保留为原批结果。此记录时
源码仍为 CC92C4BE，未在 v24 冻结批次运行中修改任何生产/固定测试输入。
源码编码检查通过：6,172 个文本文件（dcbe83）；范围内 diff --check 退出零，
仅有既有 LF/CRLF 提示（3b247d）。

### 2026-09-10：显式 launch outcome 批次与新增影响面问题

补齐上一段终态：CC92C4BE 的 v24 十门已全部通过，报告
`build/tmp/xlir_generator_payload_v24_lifetime_batch.json`，完成于 21:51:28 +08。
它仅证明该源码，不能移用于后续修改。

GP-01/02/03 已实施统一修复，不在 Cgen 中识别 catch 或回查 AST：

- error 和嵌套 generator 产出使用明确的 MOVE_ERROR/MOVE_GENERATOR yield ABI。
  嵌套循环消费命名 generator 后，IR 显式写回空值，保留外层绑定的正确清理状态。
- async/generator 创建统一返回 RESULT_CARRIER。创建失败及 capture 回滚在局部
  error scope 内完成，调用方 ERROR_TEST 消费成功位；不再提前锁存外层失败状态。
- 成功创建不得清空调用方原有诊断。第一版边界出现此回归，独立探针复现后已修正，
  并以固定宿主断言检查原诊断对象保留。严格 C 的一处缩进警告也已修正，未降警告级别。
- C 宿主通过 test-only adapter 显式观察 carrier；该适配仅属于测试 foreign 边界，
  不构成生产旧 ABI 回退，也不允许只取 `.value` 丢弃无诊断失败位。

当前生产源码指纹为
`A97FAF35E4ACC183FED2C25000C8DF43B13D21DF85CD097728152787F11F2323`。
358 项完整源码及逐文件字节校验快照见 `build/tmp/xlir_launch_v50_checkpoint.json`
与 `build/tmp/xlir_launch_v50_source`；驱动为 `build/test/xlir_launch_v48_driver.exe`，
SHA256 `59DE7E4A4DDD61987591CD8F93A2353B2138E1BE06CFE97912BB18D735CEE22A`。
此快照不是依赖行尾一致性假设的隔离源码副本。

v47 四门统一验证全部通过（`build/tmp/xlir_launch_v47_batch.json`），3,810 项
输入指纹前后一致：

- payload 门 15 个 fixture，两轮各 83 项非法 IR 变异，C/H/JSON/launch binding
  确定性检查；1,000 次基础、2,400 次生命周期、480 次分配场景及 30 次 pool
  模块生命周期通过，合计 3,910 次，含 380 次真实分配失败。
- virtual activation 两轮各 31 项变异、6 个前端反例，900 次真实创建分配失败；
  严格 C/JIT/AOT 的 suspension、generator 结束/失败/提前关闭检查通过。
- owned activation 两轮各 56 项变异，1,100 次真实分配失败及抛错析构优先级，
  严格 C/JIT/AOT 的 ready/pool/generator 生命周期与模块收尾通过。
- pool 故障测试保留真实 pool 路径，在线程池完成 join 后检查每模块内存；不把
  异步工作仍存活时的逐调用分配数误写成泄漏，也未用 ready 路径代替 pool。

BBAC0CBD 前一源码的 v43 fast/task/example 三份 receipt 已证明 505/505 生成通过。
A97FAF35 增加了成功创建诊断保留修复，尚未对它重跑 505；两者不能混记。
v38 的 pool 测试同步问题、v42 的旧宿主 ABI 和 v46 的共同格式警告均保留在原始
失败报告中，v47 是修复后的独立终态，不覆盖历史失败证据。

后续 v49 十八门影响面批次使用同一冻结输入，报告
`build/tmp/xlir_launch_v49_affected_batch.json`。本段记录时为 11 通过、6 失败、
1 运行中（loop cleanup）。失败未阻止后续门执行，也未重跑已通过的四门。

| 根因组 | 已收集证据 | 状态与下一步 |
| --- | --- | --- |
| LA-01 测试头不自包含 | terminal outcome / match cleanup 均缺少 assert 声明 | 等冻结批次终止后统一补共用头，再复验两门 |
| LA-02 合成 SSA owner 的关闭路径 | calls/cfg/views 三门在合成图辅助器退出活跃 owner 断言失败 | 隔离复现定位到 GENERATOR_CLOSE_TEST 的正常关闭边；仍待合法 IR 清理表达和完整运行证明，不能只删断言 |
| LA-03 网络 native Future 布局 | admission 在 net_fast_path 首次失败；当前源码驱动再现同一错误 | 追加收齐余下 fixture，net_send_file / tls_server_loopback 同错；file_async / process_extended 生成通过；属于未解决合同缺口 |

LA-02 的隔离实验位于 `build/tmp/xlir_launch_v51_debug`：简单向关闭路径塞
STORE+RELEASE 被现有 cleanup-only verifier 正确拒绝，直接跳过活跃 owner
又被 owner consumption verifier 拒绝。两种尝试均未合入生产或固定测试。
下一步需明确 SSA owner 的关闭销毁合同，不能放宽 verifier 或伪造来源绕过检查。
LA-03 补充发现报告为 `build/tmp/xlir_launch_v52_admission_discovery.json`。
admission 旧门仍使用发布 CLI 且在内部遇首个失败即终止，后续须改为当前源码
驱动、逐 fixture 收集退出结果，再统一严格 C/运行；原门失败不作豁免。

本轮只关闭有对应证据的具体复现；新发现单列，不改旧组分母。完整并发载荷、
复杂所有权、native/callable、DLL/API 代码租约与安全卸载、同源码发布准入及
legacy 物理退役仍未完成。未修改独立 XRT、未重建 release、未运行 common.bat。

### 2026-09-10：VALUE_RELEASE 收敛关闭路径拥有权

v49 已于 22:42:56 +08 终止，18 门全部执行，12 通过、6 失败；3,811 项输入
前后均为 E886F7F3…65C062。最后 loop cleanup 门通过两轮各 2,526 项变异，
12,000 次运行、2,000 次真实诊断分配失败、679,200 次平衡分配及 JIT/AOT。
至此才解除冻结；隔离实验没有改变该批次输入。

LA-02 不是删除合成图断言即可解决的问题。生产 IR 新增 `VALUE_RELEASE`：
单个操作消费一个精确 TypeId 的独立 SSA owner，不要求先安装到存储槽；
类型决定销毁方式，可能抛错的销毁必须属于已验证的 cleanup error scope。
它不是非拥有型 DISCARD，也不复用只接受 storage symbol 的 RELEASE。
operand ownership、借用失效、关闭路径和 cleanup 验证同步接纳此操作；
continuation 复用通用 TRANSFER_BEFORE_INSTRUCTION，销毁前清空 resident
初始化位。sync/async/generator 共用同一个机械销毁 emitter，保留 detach-before-destroy。

合成 IR 辅助器改为为分配失败和显式关闭边生成 VALUE_RELEASE，并在 yield
改为透明 alias 时清除不再适用的 close 元数据。generator calls 的正常边
使用两个调用 alias，关闭边使用两个 resident root，因此首个 suspension 的
四个活跃 view 均被检查；未减少物理 owner 数或跳过关闭路径。
LA-01 同时通过让 `xlir_launch_test.h` 自包含 assert 头修复。

当前源码指纹为
`13F4FAC74FBC69E28FBCCDFFB73B076098A4D26186E6F43DC90642434A5F4DD6`；
358 项编译源码的完整快照与字节校验见 `build/tmp/xlir_launch_v56_checkpoint.json`
和 `build/tmp/xlir_launch_v56_source`。快照同时拷贝 src/README.md、parser.out，
但这两个非编译源码不在延续使用的 358 项源码指纹分母中。

统一复验报告 `build/tmp/xlir_launch_v54_owner_batch.json`：3,812 项输入不变，
五个运行门全部通过，静态门因旧辅助函数名产生一项新失败。批次结束后更新
静态门为新名称，并增加 VALUE_RELEASE 与 GENERATOR_CLOSE_TEST 共同存在的
检查；单独重验退出零（6eaa47，脚本指纹已录入 v56），未重跑五个运行门。
因此不把原始 v54 的 failed 状态改写为 passed。

- owner views：aliased 两轮各 50 个非法 IR 变异，single 44 个；两种形式
  各 300 次完成/提前关闭运行、100 次真实分配失败，以及严格 C/JIT/AOT 通过。
  新反例覆盖丢弃代替销毁、伪造 storage、缺失 operand、错误 type 和无效 scope。
- owner CFG：四台机器、29 项变异，3,500 次运行；540 次真实分配失败中
  20 次发生在 owner 已驻留 frame 之后，分支/循环重获/存储转移全部内存平衡。
- owner calls：两轮各 254 项变异，8,500 次运行、7,200 次真实 suspension；
  960 次 setup、1,600 次 result-box、300 次启动分配失败；严格 C/JIT/AOT 通过。
- terminal outcome：2,200 次运行、3,000 次真实 suspension；match cleanup
  同步与 async 检查通过，async 1,600 次 suspension；均严格 C/JIT/AOT 通过。

原 v49 的六个失败门已有五个关闭，剩 LA-03 网络准入门。隔离诊断 v55 明确：
失败对象是 NetBytes，布局、字段和参数存在，`xl6_xrt_net_bytes_destroy` 的
已验证调用合同是 MAY_RAISE + FOREIGN_SCOPE，而网络 wrapper 只接受 NO_RAISE。
不是缺少类名，也不能靠按类名特判或把外部声明强改为不抛错解决。下一批需统一
native handle 销毁 callback 与网络结果 wrapper 的错误/拥有权合同，并将
admission 改为同源码、逐 fixture 收集。v55 的 fprintf 仅位于隔离源码，未合入。

VALUE_RELEASE 的本轮真实运行覆盖 text SSA owner；其他拥有型结果的单指令
销毁，尤其抛错析构、聚合值、重入和 diagnostic-free failure，仍须专门补验。
现有类型合同和复用 emitter 不是这些运行性质已证明的替代物。完整 505 生成、
重型全影响面和同源码 release 准入也没有借用旧源码结果宣布完成。

### 2026-09-10：网络 wrapper 与 native 销毁边界统一

当前源码 `0B2D5C489C51070A3742EE9566FFDF63FFA468022D509207725B364D46725FDA`
已将 native handle 和 TCP/TLS 包装结果的销毁 callback 合并到同一个机械 emitter。
目标的精确符号与错误物理 ABI 来自已验证 CallTarget；同时接受一致的
NO_RAISE/NONE 和 MAY_RAISE/FOREIGN_SCOPE，不再要求网络析构必然不抛错。
网络包装不再用硬编码 XRT free 替代类的已验证 native destructor；分配回滚也
使用同一 callback。未修改 NetBytes 声明以改变效果。

`tools/check_xlir_native_take_reentry.ps1` 已切换到当前源码验证驱动，默认通过
`tools/build_xlir_validation_driver.ps1` 构建一次，可传入同批已构建的 SourceDriver。
增强的固定宿主验证 600 种执行组合：100 轮 × 三种析构结果 × 有/无先前错误。
要求正常、带诊断失败、false-NULL 失败的成功位准确，先前错误对象不被替换，
外层 cleanup 结束后恢复调用方诊断，逐例内存平衡。首次测试错误地要求外层
cleanup 结束后清空调用方诊断，断言失败（762e47）；对照 scope 的明确合同
修正为检查恢复原对象，未修改生产错误作用域实现。

加强后 native TAKE 门退出零（90e403）：2,000 次真实分配失败、2,000 次成功，
原阶段 6,000 个 native owner 关闭；新的 600 组合同时在严格 C/JIT/AOT 三路
执行。静态合同检查通过（74b93f）。新固定门
`tools/check_xlir_network_wrapper_abi.ps1` 收齐三个 fixture 的 C/H/JSON 两轮
生成、字节确定性和严格 C，3/3 通过（02c009），记录为
`build/tmp/xlir_launch_v61_network_wrapper.json`。门明确只证明生成/严格 C，
不声称网络运行通过。源码完整快照为 `build/tmp/xlir_launch_v62_checkpoint.json`
及同名 source 目录；其中保守 pending 标签不覆盖 v61 已通过的实际终态。

LA-03 的原三个 C 生成复现已关闭，但补做真实链接/运行后发现下一层问题，
不得混入旧分母或以标准库最低标准为由跳过：

| 后续组 | 当前证据 | 状态 |
| --- | --- | --- |
| NW-01 resolver/UDP 组件缺失 | net_fast_path 链接缺少 29 个 resolver/UDP bridge 符号，v59 保存完整链接诊断 | 待将可达 bridge 的组件闭包显式接入 verified IR 驱动的生成 |
| NW-02 string-repeat 桥接缺失 | net_send_file 链接缺少 xl6_xrt_string_repeat | 待补齐对应运行时组件选择/链接证明 |
| NW-03 TLS 类型不兼容 | TLS 已链接，证书/ALPN/listen/config 检查通过；连接阶段失败，kind=2/code=1，value has an incompatible type | 待排查 exact native/Future payload ABI，不能修改断言或忽略错误 |

链接批次记录为 `build/tmp/xlir_launch_v59_network_runtime.json`。其中 TLS 子进程
仅保存 PID/日志，不能据此声称获取了退出码；确认旧 PID 已消失后，独立 v60
诊断运行实际退出 1（208e3a），保存了上述错误内容。没有因观察超时重启。
未运行 common.bat、未修改独立 XRT、未重建发布物。完整 admission 仍未关闭；
VALUE_RELEASE 更多类型/析构效果与长期语言、并发、卸载及发布要求继续保留。

### 2026-09-10：DB418173 字符串与 TLS 投影批次；恢复执行位置缺口

本批继续采用“分组修复、小检查、冻结后统一复验”。新增 STRING_REPEAT 闭合
桥接合同：精确的托管字符串/I64 参数与原生指针返回 ABI 由 IR 验证；组件选择
读取该合同，只输出所需桥接。字符串 bridge 的分类改为查询闭合登记表，不再
依赖连续枚举范围；新枚举追加以避免重编号。未把字符串算法搬入 Cgen。

TLS 连接类型错误的根因是共享同步便利层把 Future 中的语言类对象整体传给
原生 handle getter。现在共享组件显式接收 stream/bytes 两个借用投影接口：
XLIR 由已验证的唯一类布局和 native field 输出精确字段投影，legacy 保持自己的
原生 handle 表示。同步 connect 与 recvOwned 都使用对应接口；不增加按运行值
形状猜测的 fallback，也不改变全局原生 getter。原生 retain/take 仍由共享组件
执行，投影本身只借用。legacy Cgen 严格语法检查通过（9dd465）。

正式批次 `build/tmp/xlir_launch_v70_batch.json` 已终态，源码/测试/工具输入
指纹前后同为 `539ED9F0…1DA76A75`；驱动 `xlir_launch_v70_driver.exe` 仅构建
一次，网络运行重用 wrapper 门生成的 C，没有再次构建编译器或重复生成。
4/4 受影响门退出零：

- 静态 XLIR 合同。
- string-repeat：两个非法签名拒绝、C/H/JSON 确定性、最小组件选择、Unicode/
  空串/零/负数边界；600 组运行含 300 次真实分配失败，逐例内存平衡。
- network wrapper：3/3 fixture 的 C/H/JSON 确定性与严格 C；TLS 新固定宿主
  850 组包含 350 次真实包装分配失败、500 次成功，逐例内存平衡。NetBytes
  子检查使用带显式 no-op destructor 的借用哨兵，仅证明字段表示，不宣称真实
  NetBytes 生命周期或网络运行已通过。初版宿主错误地给 HandleTake 传空 ops，
  v68 保存该失败；对照 API 修正测试后小检查与正式 v70 均通过，未改生产 API。
- native TAKE 重入门：原有 2,000 次分配失败/2,000 次成功/6,000 个 owner 关闭，
  及 600 组析构结果和首错误组合，严格 C/JIT/AOT 全部通过。

358 项完整源码的只读快照为 `build/tmp/xlir_launch_v71_source/src`，逐项 SHA
核对成功；checkpoint 为 `build/tmp/xlir_launch_v71_checkpoint.json`，源码
指纹 `DB4181734EDBF96D269AFC746F9E69FE91B60F4CFF04FB1369C65269683677E7`。

统一运行准入仍明确失败：

| 根因组 | 本批变化 | 当前证据 |
| --- | --- | --- |
| NW-01 resolver/UDP 组件 | 未关闭 | fast_path 仍链接失败，v70 保存链接日志 |
| NW-02 repeat 组件缺失 | 原缺陷关闭 | send-file 已严格链接；独立 repeat 门通过 |
| NW-03 TLS 类 payload 投影 | 原表示缺陷关闭，非 TLS 全运行关闭 | connect/地址检查通过，投影与包装分配回滚固定门通过 |
| NW-04 await 后恢复执行位置 | 新发现，语言层优先 | send-file 和 TLS 正式各 3 次运行全部退出 1，分别停在 path_range_drain/tls_client_writable |

NW-04 的 TCP 隔离诊断 8/8 复现（96b8ce）：XRT 明确报告
`a network worker cannot block on its own operation`。当前
`xl6IrCgenEmitAsyncMachineWatch` 在 Future 通知线程内直接调用语言 resume；
若 await 挂起，恢复代码会继承完成该 Future 的网络 worker，而已完成的 await
可能仍在调用线程内继续。这解释了加入日志后出现偶发通过的现象；不把一次通过
视为修复。TLS 同样有等待失败，但本批尚未捕获其底层同一诊断，不把共同根因
推断冒充已经单独证明。正式六次运行都没有触发宿主 90 秒硬期限，无遗留进程。

下一批应先压实语言 continuation 的执行位置/派发合同，使“待完成”和“已完成”
两条 await 路径具有明确可验证的恢复规则，同时覆盖 frame 的发布/串行化、
派发失败、取消、首错误和卸载 lease；Cgen 只消费该合同。不能把同步标准库
调用改成别的测试、给网络 await 加名称特例，或在通知回调里临时创建线程。
resolver/UDP 组件仍保留独立组。新发现增加独立分母，不改写旧批次结果。

本批未运行完整 505 生成或重型全覆盖，不借用旧源码结果；未修改独立 XRT，
未运行 common.bat，未重建发布 CLI/DLL。长期目标和完整语言准入仍未完成。

### 2026-09-10：NW-04 确定性回归与运行时交接基础层

新增 `tools/check_xlir_async_resume_isolation.ps1`，复用 DB418173 的 v70 源码
驱动生成 C/H/IR，两轮确定性与严格 C 链接通过。固定宿主不使用网络或延时，
实际执行 400 组：ready/pending × 成功/失败/取消/关闭 × 50 轮。终态值、finally
次数和生产者诊断保存均无失败，但 200 个 pending 全部在生产者线程执行语言
恢复（eda568）；质量门退出 1 并保存全部结果，不把预期红灯算作准入通过。
初次严格编译暴露测试宿主缺少 FutureDone 声明，已仅在宿主补齐公开声明。

新增 `src/runtime/xl6_continuation_queue.{h,c}`：纯 C、无 AST/IR/Cgen 依赖，
实现预留容量、注册提交/就绪/Watch 释放三事件交接、无分配发布、执行后释放
和 busy 生命周期屏障。它不创建线程，也未接入生产 Cgen 或发布运行库；
是消除 producer 内联恢复所需的传输基础层，不是 NW-04 已修复的证据。

固定门 `tools/check_xlir_continuation_queue.ps1` 严格 O2 构建及运行通过
（e48d26）：600 组事件排列、50 组 FIFO 双项、100 组真实三生产者并发交接，
其中执行器可在生产者线程返回前释放嵌入 ticket 的 frame；两次真实创建失败，
发布零分配、首诊断保持及逐例内存平衡均通过。完整执行服务、阻塞进展能力、
IR 派发合同和模块代码 lease 仍未接入；详见 `XLIR恢复执行与派发合同.md`。

统一复验 `build/tmp/xlir_launch_v72_dispatch_batch.json` 已终态（1e1e81）：
队列基础层与静态合同通过，语言隔离门仍失败，全部门均执行且输入未变。原
358 项编译器源码与 DB418173 快照逐项相同，因此复用 v70 驱动；新增两个
runtime 文件单独严格编译，不冒充已被旧驱动或发布物包含。

### 2026-09-11：NW-04 独立执行服务与阻塞准入

新增 `src/runtime/xl6_continuation_executor.{h,c}`，在队列上提供独立 worker，
不把工作交给 Future 生产者。队列新增条件变量等待；关闭后已预留、尚未完成的
ticket 仍保活，最后一个 live lease 退休才允许 worker 退出与最终 join。
busy/self-destroy 返回失败而不销毁。独立执行器明确给出进入阻塞区的准入：
最后一个非阻塞 worker 必须先建立替补；线程上限或分配失败不进入阻塞调用。
嵌套进入只计一次，退出恢复容量；无每个 await 临时创建线程的逻辑。

严格 O2 小批验证（6ae960）通过：100 个生产者隔离交付；300 个真实 Future
Watch 的三种注册/完成顺序和四类结果；150 个阻塞/上限/补位故障组合，含 50 次
真实补位分配失败；九个真实创建失败预算点。结果、一次释放、生产者诊断、
关闭/busy/join 和逐例内存平衡通过。队列旧门同时复验通过（42b051），新增
条件变量使创建失败预算点由旧实现两项变为当前三项，旧报告不改写。

这仍是运行时基础层，不是生成语言代码已切换。当前 358 项编译器源码仍未
改变；现有隔离红灯不得删除。下一步必须在 IR 中显式建立恢复派发和 may-block
调用边界，绑定注册失败/阻塞准入失败的 cleanup，与模块代码 lease 一起接入；
不能只在 Cgen 外部调用前塞入保护，从而制造未被 IR 表达的潜在失败。

加强后又通过 50 组多 worker 饱和场景（513148）：两个初始 worker 同时进入
阻塞区，有替补额度时第三个 worker 执行解阻工作；无额度时恰好一个阻塞准入
被拒绝，该 worker 返回后仍能执行解阻工作，不把整个执行域耗尽。
正式统一批次 `build/tmp/xlir_launch_v73_executor_batch.json` 终态为三个门通过、
语言隔离门失败（87a0f4），全部收集、输入未变，未重新构建未修改的编译器。
运行时新增/改动独立验证；生产语言恢复仍内联于 Watch 通知线程，NW-04 未关闭。

### 2026-09-11：B47C93F0 调用执行边界进入 IR

CallTarget 新增 executionBoundaryAbi，构建时在 callable role 定型后写入，
验证器独立检查，文本与 JSON dump 显示相同事实。内部语言 CFG 与外部可能
阻塞边界分开，外部 C/native factory 一律保守；NO_RAISE 不被错误解释为
nonblocking。未更改被调用者 errorAbi 或参数消费合同，未插入隐藏的 Cgen
准入失败，也未声称已有 execution-domain 派发。

固定门 `tools/check_xlir_call_execution.ps1` 通过：四类 fixture 覆盖全部
五种 CallTarget kind，共 58 个目标（33 个外部、4 个 native factory、6 个
NO_RAISE 外部），174 项缺失/非法/错配变异全部由 verifier/C/头文件三层拒绝。
这确认描述与守门，不代替阻塞保护的运行证明。

统一批次 `build/tmp/xlir_launch_v75_ir_batch.json` 全收集且输入未变：调用
执行合同、队列、执行服务、静态合同、repeat、native TAKE 六门通过；语言
隔离门仍有 200 个 pending 生产者线程执行失败，结果/清理/诊断维度无失败。
三个网络 fixture 当前源码生成成功，C/H 与 v70 基线逐字节相同，JSON 增加
执行边界信息；未重跑已知仍失败的网络运行，避免重复耗时。

362 项完整源码（原编译器集合加四个独立 runtime 文件）快照已逐项核对，
见 `build/tmp/xlir_launch_v76_checkpoint.json` 与同名 source 目录。按该报告
SourceRecords 顺序计算的指纹为
`B47C93F0E5EA1BA6AF16E8F17737647553E978DA5BA1E96F193ECA37F243DAB4`。
runtime 仍单独编译验证，没有冒充已进入编译器驱动/发布 DLL。下一步仍需
把调用前的准入失败、尚未消费的 owner 回退、析构责任与恢复派发一起降低为
明确执行合同；当前字段不是这部分完成的替代物。

### 2026-09-11：4A38936E 外部调用准备失败的拥有输入回收

本轮先发现再修复一个实际语言缺陷：FOREIGN_SCOPE Begin 失败跳过 native
调用，但 CALL 已消费的 byval 字符串没有退休。v77 原始三分支测试在带诊断、
false/NULL 两条路径各泄漏一个 owner/四字节（59d6f0，退出 1）。这是公共 ABI
边界模拟，非真实 allocator/OS 失败注入。需要修正此前“未调用即未消费”的
简写：调用方已移交给 CALL 边界，尚未移交给 native callee；失败应由边界回收。

生产修复只涉及三个 IR 文件和三个 XLIR Cgen 文件。新增 preparationFailureAbi，
verifier 独立检查与 dump；直接调用、协议 witness 共用逆序退休/首失败保持
实现，复用现有类型释放 ABI，不加 AST 回溯或名字猜测。原三分支复验泄漏为零；
扩展固定门 600 组覆盖两拥有参数逆序释放、借用对应的调用方 owner 最后释放、
清理再报错、原错误身份与 false/NULL 保持。360 项执行/准备合同变异被拒绝。

发现批次 `build/tmp/xlir_launch_v79_prepare_batch.json` 八门六绿两红，输入未变。
除既有异步隔离红门，协议旧测试在 32 次调用、2 个 no-raise witness 时提前
触发至少 4 个的计数断言；v74 与 v78 驱动生成的相同 fixture 均为这一结果。
没有下调门槛：增加两个明确 scalar-only 的无异常方法及对应 raising 实现，
原用例保留，slot 9→11、调用 32→36、no-raise witness 2→4，变异 137 项。
随后公共 entry 故障提前命中参数构造中的不变量检查（退出 3，栈在构造函数），
将参数构造放到注入之前，使故障命中真正待测调用，保留失败码与析构检查。
协议门最终严格 C/JIT/AOT 均通过，严格运行计 32,100 次平衡分配；没有把测试
准备错误记成语言修复，也没有用降低断言绕过运行验证。

最终统一批次 `build/tmp/xlir_launch_v80_prepare_batch.json` 八门七绿一红：
调用合同、foreign preparation、协议 outcome、静态合同、repeat、native TAKE、
network wrapper 通过；异步隔离仍在 400 用例中有 200 个 pending 线程违规，
语义/诊断维度零失败，继续作为真实失败计入总结果（68ca79，退出 1）。
三网络 fixture C/H 与 v75 逐字节相同，JSON 增加准备合同；不重复运行已知
未修复的网络集成失败。源/测试/工具/lib/发布依赖和复用驱动的批前后指纹一致：
`713976142BADD058261556696BBE4731B1D88715E57631ED5066E6F800F6CE59`。

362 项源码快照见 `build/tmp/xlir_launch_v81_checkpoint.json` 与 source 目录，
指纹 `4A38936E998D14F43E8F689CAA05BB5BB9DDD308A6DF5E55E6A744CEF5447AC9`。
本轮不修改发布 DLL/CLI 或 sibling XRT；无新的全量 505 门准入声明。
NW-04 的生产执行器接入、阻塞准入、代码 lease 和其余长期目标继续开放。

### 2026-09-11：C531A952 后继 frame 交接与共用 Future Watch 运行时

接入检查确认：当前 Cgen 每次恢复后的 await 都分配后继 frame；如果直接套用
原根任务 Reserve，容量一或 Close 后会拒绝同一 activation 的后继。运行时
新增 ReserveSuccessor，继承根容量，同时保留前后两个物理 lease；后继不得
早于前驱 release 返回执行。分离 active（根 activation）与 live（物理 ticket）
计数，至多一个后继、注册失败可 Abort/重试、根容量最终归还、busy 不释放。
不向生产 Cgen 塞入未被 IR 表达的派发路径。

新增 continuation_future 共用 Watch 注册层：READY/PENDING 都只发送 ticket
事件；Commit 由完成 frame 交接的调用方发出。transportOwned 防止 pending
Watch 在事件尚未到达时被 Abort 或重复初始化，错误注册仍允许调用方回退。
它不接管语言 source lease，也不改变 capture-before-source-release 合同。

新增固定门覆盖 240 条 32-frame 链（120 open、120 closed，共 7,680 frame）、
六种事件顺序、后继撤销重试、根容量复用与零分配发布；300 组注册边界检查，
其中 200 次公共 WatchAdd ABI 故障模拟（诊断和 false/NULL），不冒充真实分配
故障；300 组实际 Future 的四终态 × 三种退休时序 × 25 次，使用两个 worker
验证前驱物理释放屏障、生产者诊断及内存平衡。定点门  fae140 退出 0。

统一冻结批次 `build/tmp/xlir_launch_v82_handoff_batch.json`（18b3bf，退出 1）
完整收集：原队列门、原执行器门、新后继门均通过；生产隔离门仍有 200 个
pending 用例违规，继续失败，未取消测试或借窄门宣称 NW-04 关闭。批前后输入
一致；358 项编译器源码与 v81 逐项相同，复用 v78 驱动，没有重编译未修改的
编译器或重跑无关网络/标准库门。源/测试/工具/lib/发布依赖指纹：
`64AB3A3F42FD635B814D449A5DA4726CDA3360807BE7756CEAC9527F55DA342E`。

364 项当前源码快照 `build/tmp/xlir_launch_v83_checkpoint.json`，指纹
`C531A95290F01903674AFEC3ED20392B0E3998AF0EEBCCD8947BA72FDF1FC86B`。
只修改/新增四个 runtime 源头、一个固定测试和一个工具，生成程序及发布 DLL
尚未接入这些函数。剩余生产派发、blocking admission、模块代码 lease 与所有
完整语言/发布要求保留；本轮解决的是实际接入所需的交接寿命规则，不是最终准入。

### 2026-09-11：363A9B59 宿主停用事务修复，销毁失败单独入账

宿主 API 原来在清理前提交 activated=0。初始源码复现
`build/test/xlir_module_deactivation_baseline.exe`（84c99a，退出 1）：无诊断
失败后重试成功返回，但 attempts=1、artifact_live=1，没有重新清理。只修改
`src/api/xl_api.c`，内部 activation 改为 inactive/active/deactivating/
deactivation-pending 四状态，成功才提交停用，失败可重试且不能重新激活；
清理重入及其他公开入口仍持 lease 时的停用被拒绝。公开 C ABI 保持不变。

固定源码门通过 400 组事务：false/NULL、busy 诊断、已清理但报告失败、legacy
void 入口；3,600 次 Activate/Deactivate/Run 重入拒绝，入口计数恢复及逐例
分配平衡。两种 XL_ENABLE_EXE_COMPILER 配置严格编译。测试直接包含生产 API，
以 whole-program 消除未测编译路径，仅替换 TCC 查符号；不是发布 API 准入。

独立新缺口：Destroy 不观察 checked unit 的失败结果。新增默认红门提供
checked 和 void 包装（对应生成模块接口），busy 清理后假 TCC 删除回调仍
执行一次：attempts=1 / deletes=1 / live-code-deletes=1。删除记录器不真的
释放执行中 JIT 页，不能夸大成真实 JIT 崩溃测试。需与代码 lease/deferred
retirement 一起修复，不能把停用门通过当作销毁安全。

统一批次 `build/tmp/xlir_launch_v84_module_batch.json`（77a6d5，退出 1）四门
全收集且输入未变：停用与 DLL API 静态边界通过；新销毁门和既有 200 个 pending
异步隔离失败保留为红。新增缺陷单独入账，不与以前不同门集合计算完成率。
批输入指纹 `023F983A89B5336D81C449A75A7DA156E7B9C1D30390545692049E7A252BCBA8`。
API 源码不在生成驱动编译单元中，其他源文件与 v83 逐项相同，复用 v78 生成
驱动并单独构建源 API 宿主；没有重跑无关网络/队列门或重建发布产品。
364 项源码快照 `build/tmp/xlir_launch_v85_checkpoint.json`：
`363A9B59821FB56CEED63397CD1A5063CBC85B4F1727C81444FE1EB454D80199`。
生产派发、销毁/代码 lease、阻塞准入及其余语言与发布要求继续开放。

### 2026-09-11：4E597A9B 宿主延迟退休与 ticket 代码引用

修复 v84 新增的销毁失败根因：Destroy 优先观察 checked unit 的明确结果，
失败或入口/代码 lease 尚存时保留整个模块；失败的一次性运行由宿主待退休队列
拥有，不丢失最后一个重试入口。最后一个 lease 归还及正常 RuntimeUnit 有界
收集可再次尝试；清理在 API 锁外运行，首错误保持，成功才删除代码、卸载依赖。
不建议用户重复调用消费式 Destroy 的原始句柄，也没有后台重试承诺。

新增宿主 CodeOwner 可选绑定合同，以及队列/执行器 ReserveOwned。根 ticket
取得代码引用，后继继承同一引用，撤销后继不丢失引用；最后一个 frame release
返回后才由常驻运行时释放引用。普通入口、代码引用、物理 ticket 分别计数，
不能把 Future 已发布或后继已创建当作代码栈已经退出。

定点修复后统一冻结复验 `build/tmp/xlir_launch_v86_module_batch.json`
（b250f1，退出 1）：停用、退休、DLL API 静态边界、队列、执行器、后继交接
六门通过；异步隔离门保留原 400 用例中的 200 个 pending 线程违规，语义和
诊断维度零失败。此前红的退休门转绿，没有撤掉隔离检查，也不把 6/7 当作
整个长期任务完成率。批前后输入一致：
`4F716C824F16BE7D28AF08E6B786539C03C452E7B321267564C057C91F3117AB`。

退休门含 400 组清理事务、100 组代码引用/真实依赖 DLL 卸载、200 组 owned
ticket 撤销/后继链，以及绑定回滚、锁外清理和内存平衡。TCC 删除是记录器，
依赖卸载是真实行为；这不是实际 JIT 页释放或发布 DLL 的完整卸载准入。
复用编译输入未变的 v78 生成驱动，源 API 另建合同执行程序；没有重建发布
CLI/DLL、修改 sibling XRT 或重复运行无关全量门。

365 项源码快照 `build/tmp/xlir_launch_v87_checkpoint.json`：
`4E597A9BF325CD7DDD6BBACAF104D0B6FF25C2688BDFA72A4CCB40F12F081DCF`。
生产 Cgen 尚未发出 code-owner 绑定或 owned ticket 派发，故下一批必须将执行域、
准入、拥有权与失败路径落实到经验证 IR，再机械生成调用。NW-04、阻塞准入、
逃逸回调/闭包的代码寿命、语言和发布全量要求均继续开放。

### 2026-09-11：6F0F90C5 生成绑定入口及受影响生命周期统一复验

将代码拥有者绑定从宿主私有能力接入真实生成模块：ModuleAbi.codeOwnerAbi
由 lowering 定型、verifier 依据初始化函数重算，dump 与 C/header 一致。Cgen
仅机械发出该合同的绑定入口和首次初始化封闭动作。复制一次且不可更换，失败
初始化及 unit/init 均不重开绑定；扩展前缀允许、无效大小/回调拒绝。新增符号
被语言链接占用会在生成前明确拒绝，未用生成 C 编译错误代替 IR 验证。

定点检查包含 10 个 IR 变异和 2 个真实 export/import 非法源文件，全部拒绝且
C/header 无输出；五种生成模块模式与源码 API、常驻队列集成通过。bound 模式
使用宿主外部入口 lease 预留根 ticket，Close 后后继共用同一代码引用，实际
语言 export 执行结束且最后 frame release 返回后才请求删除。TCC 删除为记录器，
不能称为真实 JIT 页卸载；生产 async 尚未主动预留 owned ticket。

初始测试将完整源码 API 头和独立生成 C 合并成一个编译单元，触发既有运行时
声明冲突。按实际嵌入方式分离编译单元后通过；没有修改生成物或回避生命周期
检查。另加复用源码驱动的四 fixture 生命周期窄门，保留原宿主的全部严格 C
断言；它不替代旧综合门中发布 CLI/DLL 的准入。

统一冻结 `build/tmp/xlir_launch_v88_binding_batch.json`（2e9cc1，退出 1）十二
项全收集、十一绿一红：生成绑定、四 fixture 生命周期、owner consumption、
静态合同、DLL API 静态边界、停用、退休、队列、执行器、交接、模块 task pool
均通过；生产隔离仍为 400 用例中的 200 个 pending 线程违规，语义/诊断零失败。
既有测试分母未减少，未把 11/12 称为长期目标完成率。

生命周期门：普通模块 1,427 次分配及 1,221 个语言对象平衡，清理门 8,800 用例、
6,800 次真实诊断分配失败、188,400 次平衡分配。所有权门保留 337 RuntimeOp、
1,158 个 consuming 输入、470 个拒绝变异和 47 项 CFG 检查。模块池门验证源码
合同及生成 C 的严格运行/JIT/AOT，含 210 次 worker/callback 卸载；这不是发布
编译器与新源码同源的证明。

批前后源/测试/工具/lib/发布依赖与新驱动输入指纹一致：
`61A9B7E8330CD579FD98247BA8D8E3F661C1E9E4BA316784BBD7931E39EAE7BE`。
源码本轮已变，因此重建 v88 驱动后统一复用，未借用 v78 生成结果。366 项源码
快照 `build/tmp/xlir_launch_v89_checkpoint.json`：
`6F0F90C5041A45818C35916BC539B19CCAD53ED06019FA305C4C52EC2173F237`。
无发布产品重建、无 sibling XRT 修改、无新全量准入声明。下一批是生产 async
执行域/阻塞准入/owned ticket 的 IR 与发射接入，NW-04 及其余长期要求不变。

### 2026-09-11：D33B9E11 通用阻塞的根准入与退休 hook 执行信用

接入审查明确：逐 foreign 调用补位会增加准入失败，不能直接套到 NO_RAISE
及必须执行的析构上。新增 Guaranteed 执行器策略，在根 ticket 发布前保证
worker 数覆盖所有已接收根 activation，资源失败沿已有 setup-failure 退出。
后继继承同一信用，线程按高水位复用；最坏线程数达到接收根数，capacity 必须
作为显式资源预算。并非无界线程或对源程序循环等待的自动解决承诺。

补齐关键边界：最后 frame 释放后，宿主代码退休 hook 仍可运行清理并阻塞，
根 active/live 信用必须保持到该 hook 返回，且期间队列 Destroy 返回 busy。
v89 冻结队列只添加只读计数入口的对照，在新策略 host-drop 依赖链中五秒超时
（251817，退出 1）；新实现同一检查通过。真实分配故障还发现旧诊断被新内存
错误替换，已在执行器 ReserveOwned 边界隔离并恢复先前诊断；无先前错误时
保留资源错误。测试初始将“没有旧诊断”误断言成“新错误也必须 NULL”，按实际
分配失败的诊断合同修正，未削弱首错误或无诊断拒绝测试。

固定新门：100 组四根阻塞依赖链，50 组四线程并发准入，100 次真实 worker 与
线程对象分配失败（两个确定性预算点），75 组容量/Close/Abort/嵌套及 false-
NULL 拒绝检查；所有分配及代码引用平衡。并未穷举所有 OS 线程创建失败。

统一冻结 `build/tmp/xlir_launch_v90_progress_batch.json`（06a567，退出 1）
八项全收集、七绿一红：进展新门、原队列/执行器/交接、宿主停用/退休、真实
生成绑定通过；原 400 用例隔离门仍有 200 pending 线程违规，语义/诊断零失败。
批前后输入相同：
`48C4B90E0C302F065C4CAE3CFE97F8AEB84C4FE429612DCE874866557122FFDF`。
除四个 standalone runtime 源头外与 v89 逐项一致，编译器输入不变，复用 v88
驱动，只重新编译受影响的运行时/宿主测试，不重复无关全量检查或发布构建。

366 项源码快照 `build/tmp/xlir_launch_v91_checkpoint.json`：
`D33B9E11BBBE2FE8196436EFF8E9DE7661205364FC58CB3528C265D712FFC67F`。
本轮证明的是通用阻塞无需增加每次调用失败路径的运行时准入策略。生产 IR/Cgen
尚未选择该策略或接入派发，NW-04、模块服务生命周期、逃逸代码引用和发布同源
验收仍全部开放；完整目标未变，不以 7/8 代替整体完成率。

### 2026-09-11：4494EF34 生产派发与失败根因分批收敛

生产 IR/Cgen 已接 Guaranteed 执行器：显式机器执行域、模块服务需求、owned
root/后继预留及提交、busy 可重试退出。源码变更后仅重建一次 v92 驱动，后续
生成统一复用。驱动 SHA256：
`5AB13FE3ECD414C84B7FC9EAB873D3FF21667A5AE88CA60196168F58157194FD`。

完整批次 `build/tmp/xlir_launch_v92_dispatch_batch.json`（a7b8df，退出 1）
十四门全收集，十二绿两红。原隔离问题由 200/400 降至 0/400；其余所有权、
绑定、四模块生命周期、宿主停用/退休、队列/执行器/交接/进展及模块池通过。
保留 337 RuntimeOp、1,158 consuming 输入、470 拒绝变异及 47 CFG 检查。
执行域/绑定合同补至 21 个 IR 拒绝变异及 2 个真实保留符号冲突源文件。

两项红门分开处理：静态合同仍要求 async 生成器直接发出 xrtFutureWatchAdd，
属于迁移后的检查位置失配；修正为常驻 transport 必须使用 Watch、生成器
必须使用 owned ticket/service 且禁止直接 Watch，未删除原取消/拥有权检查。
新派发门四个 lane 暴露同一语言语义缺口：首次或后继注册 false/NULL 被旧
setup-failure 逻辑补造 MemoryError；保留失败，下一批先补 IR 失败载体。

批前后完整输入相同：
`F9394EF3CB7CB69A6B9224E27C09BD2E51089E375BDC655AFD3AF3651B928387`。
静态门修正后，`build/tmp/xlir_launch_v94_contract_followup.json`（744b74，
退出 1）逐项证明除该脚本外所有输入仍相同，只重跑静态门，其他十三门标记
为已核验输入不变的复用结果，最终十三绿一红；不冒充再次全跑。

新增派发门十二个独立 lane 全收集：八个 lane 合计 200 次通过且分配平衡，
四个失败 lane 在首个 false/NULL 断言退出。两容量都覆盖单信用后继；容量 2
额外覆盖满容量拒绝及无 blocking guard 的真实外部调用阻塞进展。隔离门仍
完整 400 用例，语义、诊断及生产者执行失败均为零。源码 API/符号/裁剪选择
严格语法检查另行通过。无诊断缺口为新增发现，不混记为旧隔离问题复发。

366 项不可变源码快照 `build/tmp/xlir_launch_v93_checkpoint.json`：
`4494EF3493232CC033A5FF544A0BD88948A7805B7C931263AE8A662EDEF0C674`。
发布 CLI/DLL、VFS 未重建，sibling XRT 未修改。完整源码宿主+生成 async
卸载、旧综合 async 门的异步时序迁移、TCP/TLS 和发布同源仍待验证；14 门
结果不是长期目标百分比。继续按共同根因成批修复，再统一验证受影响集合。

### 2026-09-11：875A92E1 统一 AWAIT 状态获取，十五门全绿

前轮归因继续落实为 IR 合同，而非生成器专用补丁：新增 AWAIT 独立状态 ABI，
lowering/verifier/dump 一致，所有执行形式的 ERROR_TEST 消费物理成功位。
machine setup failure 不再补造 MemoryError；source lease 清理保留失败位。
blocking/generator/machine 使用同一 typed payload 获取事务，删去重复各类型
包装。相关七个 Cgen 文件较 v93 净减 8,524 字节。

核对 XRT 实现发现上轮测试的终态 NULL 要求不准确：PromiseReject 拒绝 NULL，
原 Future 物理合同在发布边界转换为静态 INTERNAL。终态断言修正，并新增更强
的清理前边界观察和 finally 次数检查。相同新 fixture 用旧 v92 驱动仍在
四个 false/NULL lane 失败（a94359 输出；日志保存脚本本身退出 0，日志内
门退出异常），新驱动 300 次全部通过。另新增成功语句计数，旧驱动暴露阻塞
await 缺失载荷仍进入成功语句（7090a4 同样为日志展示外壳退出 0），新驱动
300 次模块事务通过。两项基线不拿“保存日志的进程退出码”冒充测试状态。
对应日志 `build/tmp/xlir_launch_v95_dispatch_baseline.log` 和
`build/tmp/xlir_launch_v96_await_baseline.log`。

最终统一批次 `build/tmp/xlir_launch_v96_dispatch_batch.json`（30163e，退出 0）
十五门全收集全绿：既有十四门全部保留，增加阻塞/generator await 状态门。
原注册失败转绿；隔离仍 400/400，派发 300/300，获取事务 300/300，均保留
精确语义、finally 和内存检查。代码绑定/执行域门新增 14 个 AWAIT 拒绝变异，
总计 35 个 IR 变异及 2 个源链接冲突。owner consumption 仍为 337 RuntimeOp、
1,158 consuming 输入、470 拒绝变异、47 CFG 检查；模块及运行时旧门未缩减。

共享获取实现删除旧包装后，更新静态门使其验证共同 ABI/状态载体而非过时函数
名。验证驱动按源码变化重建，最终整批复用 v96，未为每个 gate 再编同一驱动。
批前后输入一致：
`A4ED4E7046065B857862DEB1BD4678A2F5910C385C7B144452EE71595BCE1E44`。
驱动 SHA256：
`41CB6D665E101D2E1854A1E8C1F61CC537931581F688065FAB18DB0FFB0219E5`。
366 项不可变源码快照 `build/tmp/xlir_launch_v97_checkpoint.json`：
`875A92E12993E1B1E0596BE544527D5539AB08F1F45DE3A9CF208B34A81E65EB`。

这证明本批失败载体、清理和获取根因已收敛，不等于所有 async 或长期目标
完成。下一批聚焦实际生成 async 联合宿主生命周期及 TCP/TLS 集成，保留
原综合终态/取消/owner 门、逃逸代码和同源发布等完整要求。没有重建发布
CLI/DLL/VFS，也没有修改 sibling XRT。

### 2026-09-11：96A44564 联合真实卸载与准备失败输出

本轮新建生成 DLL + 源码宿主 API + 常驻执行器联合门。实际 FreeLibrary 后
验证模块不再加载，五种模式完成 125 次卸载且内存平衡：正常/失败/取消输入，
恢复中请求退休，失败初始化留有 pending。第六模式保留已完成 Future 载荷，
在代码仍被 payload destructor 引用时观察到宿主删除尝试；断言在真正卸载
前阻止它。模式分开进程执行、失败后继续收集，不把 150 个计划执行全算通过。
测试专用机器入口及 TCC lookup/delete 替代的界限详见派发合同；公开 Future
C 导出仍被语言拒绝，不能夸大为已支持公开导出或真实 TCC 重定位测试。

生成 DLL 的八个 continuation 函数导入自 resident EXE，objdump 导入表已核对
（f3e88a），不是把执行器放进卸载代码。公开代码绑定/源码宿主/实际生成恢复
在这次验证中确实联合运行，较前轮分开证明更强，但逃逸载荷仍然未准入。

严格优化编译发现两个初始化边界：AWAIT 恢复输出缺少与 launch 一致的中性
初始化；FOREIGN_SCOPE 准备失败时，原本只清空 native class field 结果，
遗漏其他拥有型返回值。已按准备失败 IR ABI 每次 CALL 之前清空物理结果。
新门 200 次三迭代事务（200 次 native 进入、400 次准备拒绝）保持成功/失败
计数、false/NULL、清理及内存平衡。旧 v96 驱动在相同优化严格编译中失败，
日志 `build/tmp/xlir_launch_v98_prepare_baseline.log`；cbc32e 是保存并展示日志
的外壳退出 0，不能当成旧测试通过。未降编译等级或屏蔽未初始化警告。

统一批次 `build/tmp/xlir_launch_v98_dispatch_batch.json`（eac8f1，退出 1）
十八门全收集、十七绿一红：保留 v96 的十五门全绿，再加旧 foreign preparation
门、新结果准备门、联合生命周期门。当前红门仅联合模式 3 的逃逸结果代码
寿命，其他五模式 125 次实际卸载通过。失败初始化故意触发的 AssertionError
日志是预期场景，不是遗漏的门失败。所有权变异及三组 await/隔离计数保持。

批前后输入相同：
`D75ABFF6EB4E39524646357024E3459DCA3BFACB77C340A99779CDACC10DCE38`。
驱动 v98 SHA256：
`CC73C00FB953B5130092D655EBBDF1AF17B8FF2B6361FAC563121B8D1C1B7B12`。
366 项不可变源码快照 `build/tmp/xlir_launch_v99_checkpoint.json`：
`96A445641747955B7898BD412486E2AC35B3768945EBE2AA9997F0611DB4B868`。

下一批优先处理逃逸代码能力与执行能力的区分、closing 时完成能力继承和
模块自持结果的环；不能只换一个整数析构函数就宣称完整解决。TCP/TLS、
其余综合 async 门及同源发布仍开放。未重建发布产品，未修改 sibling XRT。

### 2026-09-11：0489633D 驻留能力基础层，按输入证明复用

新增共享 CodeLease 与 resident CodeDrop，以一次宿主 acquire 支撑 closing
之后的已有能力继承，在生成析构返回后归还最后代码引用。保留通用 callback/
context，不按整数 payload 特判。真实 DLL 测试 350 个生命周期、200,000 对
并发引用、100 次实际分配失败通过；JIT/AOT 各有单独 10 次生命周期 smoke，
不把其非 debug allocator 环境计入全量故障和内存验证。首轮 C-host 发现
测试 DLL 与 CLI 使用不同 XRT 实例，改为宿主提供 allocator/error hook；
没有通过放松错误身份断言绕过问题。生产状态机尚未接入新能力。

v100 `build/tmp/xlir_launch_v100_lease_batch.json`（208efc，退出 1）只重跑
五项相关门及新增 code lease 门；其余十三项沿用 v98 的结果，并明确 Reused。
回溯校验覆盖完整 source/test/tools/lib/发布依赖/构建清单：排除六个新增
组件/测试文件，两个 API 源文件取 v99 原始字节，四个清单仅去除新增组件行，
得到的原输入指纹与 v98 完全一致，因此没有借用其他已变化输入的测试结果。
编译器、IR、Cgen 未改变，继续复用 v98 驱动，不重复 owner 大批变异或重建。

本轮前后输入一致：
`7A5CE5489F42D8C5D7F24E87A5565604884430014A1056AA3A26D3AAC2395AF0`。
结果账本十九项中十八绿一红；其中六项为本轮运行、十三项为经验证复用，不是
重新全跑十九门。真实生成 async 联合模式 3 仍在实际卸载之前拒绝：返回结果
需要代码而宿主欲退休。新运行时能力通过不等于该生产问题已解决。

368 项不可变源码快照 `build/tmp/xlir_launch_v101_checkpoint.json`：
`0489633D8024F518DD0D9FBC4D30B73805EF2F293AED489A33DECB898EA516D7`。
源码 API/符号/新 runtime 严格语法检查通过。打包和符号清单已接线，但发布
CLI/DLL/VFS 未重建、Linux 未实测，sibling XRT 未修改。下一步是 code-bearing
对象及派生值的 IR 拥有合同与模块自持引用环，原生产红门及完整长期范围不变。

### 2026-09-11：5E4523B7 普通载荷红门关闭，逃逸对象范围仍保留

本批按根因迁移五组普通 Future box 释放行为到常驻运行时，统一使用已验证
AsyncResultAbi 的映射；移除三处重复选择及普通类型生成 free helper。没有
把 xvalue/class/closure 的代码身份等同于 box，也未给所有值盲加 codeLeases。

分批发现两个相关问题：只移动释放函数实现仍可能传入生成 DLL 的 PE 导入
跳板；新 accessor 的 `xl6Future` 前缀会误选整组 Future bridge 私有源。
分别改为宿主返回实际回调地址，以及独立的 `xl6PayloadDropGet` 名称。新增
依赖裁剪门验证 private source 所需 value/coroutine，不强制 full runtime。

v103 首轮统一验证十五项全绿；命名/私有源选择收尾后，尝试反向还原输入以
复用 owner 验证，但完整字节指纹未匹配，未把该尝试当成证明。v104 因此全部
重跑十五项并通过（52da92，退出 0），没有复用该 owner 结果，也未隐藏首次
输入还原失败。两个批次报告均保留，v104 是最终同输入验证基线。

`build/tmp/xlir_launch_v104_payload_batch.json` 前后相同：
`4240455ACB679F65850664840724C93ED4D156246DB3C3D24AB52C3C57422E6D`。
驱动 v104 SHA256：
`4A7D884291C397B6AA61061657E24D65C798EBA19F538A590093E12E535AB3FA`。
370 项源码快照 `build/tmp/xlir_launch_v105_checkpoint.json`：
`5E4523B7C6F72C3FECDCAEA5C50A93C60F90A8CE7B097210FFF076BEBA06C2A8`。

联合门六模式共 150 次真实 DLL 卸载、finally 与内存平衡；旧失败模式 3
先卸载，再读取/释放结果，同时验证真正的 callback 地址属于宿主 EXE。
新增五组 payload 门 1,500 次 native 事务精确平衡，JIT/AOT 各 1,500 次，
覆盖拒绝发布、空内层和嵌套 Future 独立保留。owner 470 变异 / 47 CFG、
派发 300、隔离 400、阻塞/generator 300、module pool 原门均保留通过。

旧队列、executor、handoff、progress、code lease 五项本轮未重跑，继续保留
历史结果但不计入本轮十五项通过；因此不能写成“二十项全新验证全绿”。
源码 API/符号/runtime/trim 严格语法通过；6,260 文本编码检查及 diff whitespace
检查通过。未重建发布产品，未修改 sibling XRT。

继续开放：code-bearing closure/class/native 对象取出后的代码引用、module
自持根环及全局资源依赖、one-shot TCC 删除、其余综合 async/TCP/TLS、同源
发布与 Linux 验收。Linux 源清单已有新 runtime，但 Linux VFS 的 continuation /
code-owner / payload 私有头接线仍需补齐；旧发布驱动的生成文本合同也仍需
在同源发布阶段迁移。当前消除的是已复现的普通结果容器红门，不代表整个
逃逸值准入或长期任务完成。

### 2026-09-11：6AEB38B9 一次性执行统一，六相关门加一新门

本批继续检查对象/模块生命周期，发现直接脚本运行还绕过代码绑定与退休。
删除 xlCompileAndRunUnit 的独立 raw-TCC 执行/删除逻辑，复用模块编译、
checked 入口及 resident 退休拥有者；并修复有打印 hook 时宿主错误文本丢失。
没有修改 IR/Cgen，也没有替代尚未解决的 code-bearing 对象及自持环合同。

新门初轮收集四个失败：两个暴露 context 只打印不保存错误，另两个是测试
错误地把 ready 输入等同于同步完成。前者修复宿主错误捕获，后者按已验证的
异步派发语义检查成功或精确 busy，并增加独立的无 async 成功/退出码场景。
七场景全部保留且通过，175 次真实 DLL 卸载、入口绑定、finally→全局清理、
卸载后错误文本及精确内存平衡。实际 TCC 编译/重定位/页删除仍未由此门证明。

v106 `build/tmp/xlir_launch_v106_one_shot_batch.json`（f52351，退出 0）账本
十六项全部绿：六项相关门重跑、一个新门、九项复用。复用前将唯一 API 改动
取回 v105 原始字节、逆还原两个共享测试 harness、排除四个新增 fixture/tool，
完整输入 SHA256 精确匹配 v104；未变化的编译器继续使用 v104 驱动，因此
没有再次构建驱动或重跑约百秒的 owner 变异。另五项更早 runtime 历史结果
仍单列 NotRerun，不计入当前十六项，更不能称十六项全新执行。

批前后输入相同：
`7759641C21E979B7275C1F97B6EAAE31F921A0F96C392B6FEAEF837D5EAFD8C9`。
驱动 SHA256 仍为 `4A7D884291C397B6AA61061657E24D65C798EBA19F538A590093E12E535AB3FA`。
370 项不可变源码快照 `build/tmp/xlir_launch_v107_checkpoint.json`：
`6AEB38B9BF86489B87FA264B59A16EC5EBD3E60E3FD29DB1CDE7BBFFC3346CD9`。

源码 API 的两种配置、代码绑定、停用/退休、旧联合 async 六场景 150 次
卸载与新 one-shot 门均通过。没有重建 release CLI/DLL/VFS，没有改 sibling
XRT。后续仍需对象代码身份和模块自持环方案、实际 TCC 联合验证、完整 async /
网络门、Linux 私有头与同源发布准入；长期目标未完成。

### 2026-09-11：v109 真实 TCC 门及跨平台私有头清单

新增两个真实 TCC 提供者门，复用原 one-shot/async 场景和源码宿主退休逻辑，
不再用 DLL 卸载代替 TCC 页删除。325 个执行事务均实际编译/重定位/删除，
VirtualQuery 确认入口页由 COMMIT+EXECUTE 变为 MEM_FREE，含页删除后仍读取/
释放已完成 Future 的场景。另有 26 次显式 setup 编译/删除，未混入执行数。

初轮七种 one-shot 场景都因冷启动多出的 16 字节而失败。随后识别了唯一
活动分配 `<static libtcc>`，并要求第二次 setup 及所有执行事务精确平衡；
没有 reset 计数或取消内存断言。这里只核验 XRT 记账和 TCC 可执行页，不
夸大为 TCC/CRT 全堆泄漏审计。编译环境使用现有 release 的真实 TCC/XRT，
语言生成器及 dispatcher/payload 则来自当前源码，尚非同源产品发布验收。

Linux 两个 VFS 清单原先缺少 code_owner、code_lease、continuation queue /
executor / future、future_payload 私有头，共 12 个失败项。已统一补齐，
新包装门验证 24 个映射及 20 处源码清单；没有据此宣称 Linux 跑通。

v108 `build/tmp/xlir_launch_v108_tcc_batch.json`（351a29，退出 0）当前账本
十九项全绿：原三相关门加三个新门，共六项实测；十三项精确输入证明复用。
复用前排除五个新增测试/tool，逆还原两个共享 harness 和两个 Linux 清单，
全输入指纹与 v106 相同，生产源码和驱动没有变化。另五项更早 runtime 历史
结果仍保留于 NotRerun，不纳入当前十九项，也不把十九项写成全部重新执行。

批前后输入一致：
`4B445C8294C801D7F573F3158E4EBA8032046178E6DE9490F69C34006D39A70B`。
驱动保持 v104，370 项源码仍为
`6AEB38B9BF86489B87FA264B59A16EC5EBD3E60E3FD29DB1CDE7BBFFC3346CD9`。
`build/tmp/xlir_launch_v109_checkpoint.json` 校验生产源码未变，复用 v107 源码
快照，只另存九个变化的验证/打包文件及实际 TCC 所在 xl.dll 的 SHA256。

未重建发布产品、未修改 sibling XRT。公开前端 API 完整链、code-bearing
对象与模块自持环、其他 async/网络门、Linux 执行及同源发布仍需完成。

### 2026-09-11：v111 表达式准入修复与对象逃逸批次

新对象用例首先暴露 `xcall(makeCallback(...))` 被 IR 的 NAME 限制拒绝。
已在 typed admission、lowering admission 和 verifier 中去掉这一语法形状限制，
保留精确 callable TypeId、借用输入、owned xvalue 结果和 may-raise 合同；
lowering 复用既有借用临时锚点和失败清理事务，Cgen 没有修改。
八种用例覆盖返回值、变量名、收窄可空变量、类字段、容器元素、条件表达式、
有参数匿名函数和 producer 抛错。严格 C 下 800 个用例、另 100 次真实
callable 分配失败均通过，验证单次求值、借用源存活、临时 owner 释放及内存
平衡；显式 warm-up 另计，不混入上述数量。

`check_xlir_object_escape` 收集六种真实 TCC 场景，每种 25 次：closure、class、
container、detached-child 的 automatic_owner_missing 均为 25；internal-only
和 trivial-value 两个对照通过。前四种在观察自动租约缺失后才由测试宿主
显式加 pin，验证 Destroy 请求后全局状态/调用/析构仍可用，最后释放该 pin，
确认 TCC 入口页 MEM_FREE 及内存平衡。这是安全诊断，不是自动保活实现，
四种场景仍返回非零。内部全局闭包对照用于防止后续引入 module→closure→module
自持环；不得仅靠外层容器或 Future pin 代替子对象自己的代码寿命合同。

扩展用例另外发现并保留两个最小失败：

- `xlir_callable_coalesce_expression.xl`：可空闭包的 `??` 被 lowering 拒绝。
- `xlir_xcall_zero_parameter_literal.xl`：直接传无参数块式匿名函数时报解析错误。

二者由 `check_xlir_callable_admission` 批量记录；日后准入成功还会继续严格
编译生成 C、运行断言和模块清理，不能只让解析通过便记作修复。

`build/tmp/xlir_launch_v111_batch.json` 当前账本 22 项：15 项本轮通过、5 项
无影响复用、2 个新门报红（对象门四种失败，准入门两种失败）。原 19 项无
回退；三个新门中表达式门已绿。另五项更早 runtime 历史结果继续单列
NotRerun。对比以固定用例基线为准，新覆盖发现独立记账，不伪造总失败数
单调下降；当前剩余新发现归为三个根因组，不代表长期任务仅剩三项。

逆还原两个 IR 源文件和一个共享 TCC harness，并排除十二个新增测试/tool 后，
输入精确匹配 v108。只有独立 API/runtime/packaging 五门复用，所有受影响
语言生成门重跑。批前后输入相同：
`D454F5455BB08FB283EB9963106F45DD5805AB28DB469EC76D7942BC975574C6`。
最终驱动 v111：`F9BC7285DC06930C867C99A5EBB6E4BBD9C978D809C2AE35F7B730520E58AFED`。
未重建 release CLI/DLL/VFS，未修改 sibling XRT，未将红门豁免或声称目标完成。

### 2026-09-11：v115–v117 托管指针合并、解析修复及残留变体

`??` 在 lowering 中统一为已验证的托管指针 owner 事务：复用字符串复制/
引用计数指针保留合同，对 owned 左操作数建立显式锚点，present 分支转移，
fallback 分支先释放左临时值再求值；nullable fallback 的结果使用显式
NULLABLE_WRAP 保留 TypeId。没有修改 Cgen。二十类表达式的 2,000 次运行及
100 次真实 callable 分配失败均通过，包括原八类、惰性分支、borrowed 左/
右源、nullable/nullable 合并、fallback 抛错、字符串复制与内存平衡。

解析器的原始失败来自补齐匿名函数体 SEP 后再次把已识别的 `func()` 当成
函数类型。现在补分隔符时保留 parser token 分类，不再重分类；同时要求
函数类型后的声明名确有合法后继，避免把函数体开头的调用/泛型类型误作
声明名。原零参数 return-body 用例及十二种嵌套、void、具名参数、函数返回
函数、容器/可空类型上下文通过。与旧 parser 对比的 1,903 个文件没有已有
准入/匿名函数 AST 数量回退，恰有两个本轮修复的 fixture 新通过。

核心批次 `build/tmp/xlir_launch_v115_batch.json`：24 门中 18 项实测通过、
5 项无影响复用、对象保活门报红；19 项受影响/新增门本轮执行。随后补查
发现 body 第一句为赋值时仍会被声明名规则误分类，不能据原用例变绿宣称
整个 parser 根因已闭合。保留 `xlir_xcall_assignment_literal.xl`，并增加
无返回型/显式 int 返回型两种解析用例，十四上下文中这两种仍失败。

补充批次 `build/tmp/xlir_launch_v116_batch.json` 仅重跑三项相关测试：
admission 门保留原两用例的成功和新增赋值开头用例的失败；parser 专项保留
240 次成功和 40 次失败；语料扩展至 1,904 文件，1,873 原成功、29 原拒绝
保持，两个预期新通过，无回退。源码和 v115 驱动未变，逆还原新增测试后
输入精确匹配核心批次，未重复执行其它语言/运行时门。

最新账本三门红，对应两个未闭合根因组：对象自动保活，以及匿名函数体
首句为赋值的分类边界。原六种失败场景中的 `??` 与 return-body 解析已过，
四个对象逃逸场景仍失败；新增变体独立记账。后续需从完整声明/表达式上下文
修复分类，不得给 `xcall` 或 Cgen 加源名称特判。解析语料差分只证明解析，
不等同于所有语言语义和运行时都验证完毕。

v115 输入：`B143290FF3DEF8A42FBB97F1F0D9EA2852FDE6B7431BE0500CC25AFB6B21CBB0`。
驱动 SHA256：`61613E5030E4698927592E739D604E3C7E2CDD6CCB337932CF1DE5FC99EF5567`。
同源源码及验证文件由 `build/tmp/xlir_launch_v117_checkpoint.json` 冻结；
未重建发布产品、未修改 sibling XRT，长期目标仍未完成。

### 2026-09-11：v118–v122 赋值开头匿名函数与动态结果需求

赋值开头的匿名函数与换行声明存在真实语法歧义：仅凭签名后的 `name =`
不能判为声明。解析分类现检查该签名是否在外层分隔符之前拥有完整 END，
嵌套块和函数各自消费 END，避免把参数默认值里的 lambda 认成参数类型的
函数体。不依赖标识符拼写。22 种有效上下文与 4 种缺失 END 的反例，各
20 轮通过；包括换行函数参数、含 lambda 默认值的参数/构造器、嵌套函数、
函数返回函数、try/finally 和后置循环。1,904 文件解析差分无回退：1,874
原通过、28 原拒绝保持，赋值最小用例和扩展表达式 fixture 两项新增通过。

运行阶段继续发现旧动态函数调用总取首个返回值，导致 void callable 作为
语句调用也失败。新增显式 `DISCARD_TO_OWNED_UNIT` IR 结果需求：动态函数
语句调用允许零或多结果，完整释放结果向量后返回 owned unit 状态载体；
表达式调用仍要求并保留首值，setter 保持独立 unit 合同。typed/lowered
verifier 与 lowering 使用同一需求规则；Cgen 只读取已验证枚举，不追溯
源语句，不用“缺少返回值就忽略错误”修补运行时。

新增 `--dynamic-results` 合同测试：typed/lowered 合计 first=6、discard=6，
30 次错误需求/未知枚举/同时篡改 site 与 instruction 均被 verifier 拒绝，
错误 IR 不输出 C/声明头。25 类表达式各 100 轮，加 100 次真实 callable
分配失败，验证 void 语句成功、void 表达式失败、多结果完整释放、错误不被
discard 吞掉、捕获 cell/对象存活及精确内存平衡。所有原用例保留。

v120 核心批次重新执行 19 门，独立 API/runtime/packaging 五门经逆还原
全部变化输入后复用。唯一额外失败是旧静态架构检查要求 lowering 直接出现
setter 枚举；v121 将其同步为必须调用统一需求规则，同时检查两级 verifier
及 Cgen 不恢复源上下文。只重跑该静态门，输入精确还原到 v120；未重新构建
驱动或重复运行其余门。四个较早发布驱动门中的 helper 拼写也已同步，但
它们尚未执行，不纳入当前通过数或同源产品准入。

最新固定账本 24 门：23 通过（含五门无影响复用），对象自动保活一门仍红；
其四种失败模式不豁免。此前解析两门转绿，新发现的 void 动态语句问题也已
修复。另五项更早 runtime 历史结果仍为 NotRerun，不计入本轮新执行结果。

v121 输入：`FB352EE0504D707C0230E4A800CCA284E320545259397584E8C09042762AF0E7`。
最终驱动 v120：`149FD3FE459780A2E8C661D368C8204A91C4596F910D49D90200030424183180`。
370 项源码 SHA256：`842D45755CF8CDD2DCA7ACCE1F64A1888C5F5A79DF6095AC1FE9F81A717D6DDC`，
由 `build/tmp/xlir_launch_v122_checkpoint.json` 连同十二个验证文件冻结。
下一项重点为 code-bearing 对象自动保活与模块自持环；完整异步/网络门、
公开编译链、Linux 执行和同源发布准入等长期要求仍未全部完成。

### 2026-09-11：v123 对象拥有图批量发现，未宣称修复

重新核对完整 xruntime 头，确认已有 runtime-object 强/弱引用、实例 Trace/
Drop、对象图环收集及显式 Value 根接口。新探针在当前快照源码和现有驻留 DLL
分别执行 1,200 个事务，验证直接强引用和显式根可用，同时复现扁平化共享
Value/COW 图会漏掉宿主别名、跨实例独立 trace 会重复计算共享边的边界。
每套包含 300 个分配失败和 100 个重复边拒绝，均检查失败原子性及内存平衡。
这不是 XRT 完整追踪合同的反例，而是不完整接入模型的反例；探针独立记为
capability probe，不纳入语言验收通过数。

对象门保留原六模式，增加共享全局 callable、类、COW 容器、抽离子项四模式。
250 次真实 TCC 事务全部收集：原四失败和两通过保持，新增四种自动保活失败。
仍用测试专用守护安全执行调用/析构，验证全局最后清理和真实代码页释放；
没有将手动守护改作生产方案，也没有用 expected-failure 把对象门涂绿。

本轮只重跑对象门及新增运行时探针，不重建未变化的编译驱动。逆还原三个
变化测试/tool、排除两个新增文件后，输入精确匹配 v121；批前后输入相同：
`DD3EFC7E47920A58CF258B37ACE8C0AE0E13379ECE0F36115BB24DECC15EA7D8`。
`build/tmp/xlir_launch_v123_batch.json` 继承其余未受影响结果，固定账本仍为
23 绿、1 红；五个更早 runtime 历史门仍独立 NotRerun。源码/驱动/发布产物
不变，未修改 sibling XRT。

接入约束及两条 ABI 路线见 `XLIR对象拥有图接入评估.md`。若保留未登记宿主
`xrtValueRetain` 的现有合同，需要完整共享拥有图或等价运行时支持；另一条
显式根登记路线会改变嵌入合同。跨仓库扩展及合同变更均不能未经确认实施。

### 2026-09-11：v124 四组异步源码合同批量复验

对象拥有图跨仓库变更尚未获确认，本轮未修改 XRT，转向仍在当前仓库范围内
的异步终态、frame 清理、结果提交、取消 Watch 四组旧门。新增
`tools/check_xlir_async_source_batch.ps1`：共用一批编译器对象构建四个独立
mutation 驱动，每组分别记录合同、重复生成/确定性、严格 C 构建、运行结果；
不因一组失败跳过其它组。构建清单对源码、头文件、驱动及运行时做 SHA256
检查，验证批次也记录 3,506 个输入及前后稳定性，不凭时间戳复用编译结果。

第一批四组合同和严格 C 全通过；运行阶段后三组失败，原因是旧宿主仍假定
producer 发布后恢复逻辑已经同步执行，同时在主线程查询 worker 的线程局部
分配故障状态。新增测试专用 `xlir_async_source_sync` 和 Watch 观察适配器：
通过真实 code-owner 最终 release 等待物理 frame 退休，在执行故障注入的
worker 上记录/清除 fault 状态；取消测试等第二个真实 Watch 注册成功，才在
cleanup await 阶段请求取消。没有把恢复改成 inline，没有 sleep 猜时序，
没有删除既有终态/析构顺序/错误身份/分配失败断言。

单独的终态组也加上物理退休屏障，避免 Future 已发布但帧尚未释放时进行
内存快照。清理尊重最终 owner hook 返回之后 resident queue credit 才释放
的短暂 busy 窗口，随后正常关闭并 join executor。所有等待有界。

最终四组均通过，重复 C/声明头/JSON/IR binding 完全一致：

- 终态传播：2,200 次运行、3,000 次实际挂起，469 个错误 IR 变体被拒绝。
- frame 清理：4,800 次运行、1,440 个发布观察者、1,200 个真实分配失败，
  其中 480 个发生在 Promise 创建之前；7 个错误 IR 变体被拒绝。
- 结果提交：2,265 次运行、2,250 个真实分配失败（1,350 个失败 Future、
  900 个发布前启动失败）；五种 payload ABI，15 个错误 IR 变体被拒绝。
- 取消 Watch：34,560 次运行、20,520 次取消请求、34,560 个发布观察者；
  14 个错误 IR 变体被拒绝，含八个跨挂起点保留但不再读取的 owner handoff。

合计 43,825 次运行、3,450 个真实分配失败、505 个不同错误 IR 变体；每种
变体要求 verifier 拒绝且 C/声明头不输出。每个事务及整轮均保持精确 XRT
内存平衡。最终报告 `build/test/xlir_async_source_batch.results.json`，共享
源码构建清单 `build/test/xlir_launch_v124_contracts/build.json`。

370 项编译器/运行时源码保持 v122 SHA，原 v120 通用驱动和 release DLL
均未改变。本轮没有运行四个旧脚本中的 C-host JIT/AOT 路径；它们保留这些
产品检查，并补齐新测试宿主所需的源码运行时/观察器输入，不能把本次源码
专项结果冒充完整旧门通过。原 24 门账本仍另列，对象自动保活仍红；新四组
源码证据独立记录，不用更换分母展示总进度。完整网络、公开编译入口、Linux
与同源产品准入仍未完成，默认后端不变。

### 2026-09-11：v125 网络三组实际链接/运行复验

新增 `tools/check_xlir_network_source_batch.ps1` 和常驻源码宿主，沿用未变化
的 v120 通用驱动，不重建 release，不调用 common.bat。三组都执行重复
C/声明头/JSON 生成及字节确定性检查，再分别记录严格 C 编译、链接和运行。
成功链接的程序各运行三次；除退出零外，必须出现 fixture 最终完成标记。
宿主等待 continuation executor 物理退出后清理模块，等待及进程运行均有界。
测试仅使用原 fixture 的本地 loopback，不修改 TLS 断言或放宽超时。

当前结果：

- `stdlib_net_send_file`：链接通过，三轮运行/清理通过，均输出
  `stdlib net send file ok`。原 range、backpressure、大文件异步接收和错误参数
  检查保持；此前工作线程内同步等待的旧失败在当前调度下未复现。
- `stdlib_tls_server_loopback`：链接通过，三轮运行/清理通过，均输出
  `tls-server-loopback-ok`。连接、双向传输、peek/consume、统计和关闭检查
  保持；旧 native/Future payload 类型错误在当前投影下未复现。
- `stdlib_net_fast_path`：生成/确定性/严格 C 编译通过，链接仍缺 29 个不同
  resolver/UDP bridge 符号，未进入运行。没有把失败 fixture 从批次删掉。

最终证据在 `build/test/xlir_network_source_batch.results.json`；每个运行都有
独立 stdout/stderr，链接失败记录完整诊断、去重符号和对应 IR 调用合同。
29 个符号全部对应可达 `transitive` CallTarget，且 `runtimeBridgeName=none`：
不是一组已注册组件漏加系统库，而是这些编译器提供的标准库桥仍被当作普通
extern。已有参数的 `native-class-handle-borrow` 等物理 ABI 信息不足以表达
其组件提供者和 native Future 转换合同。

下一组实施边界：在 IR 的闭合登记/验证层补齐 resolver、地址列表、UDP、
packet/batch、engine/resolver/UDP stats 的桥接角色及精确 ABI；异步解析/
接收必须指明 native payload 与语言类结果的拥有关系，再提取可复用的纯
运行时组件供 Cgen 选择。不得从 Cgen 扫描名称/AST 来临时拉入旧后端，亦
不得把整个 legacy 网络初始化器并入生成物掩盖缺失合同。

本轮没有修改 370 项编译器/运行时源码。两种成功场景证明当前静态常驻宿主
上的网络集成，不证明公开编译入口、实际卸载、全堆泄漏审计或 Linux。原
24 门账本与 v124 四组异步源码专项仍另列；本网络批次是 2 组通过、1 组失败，
不是总体完成度。对象拥有图跨仓库修改仍等待确认，长期目标未完成。

### 2026-09-11：v127 resolver/UDP 合同收口与统一回归

本轮先补闭合的 IR 合同，不将缺符号诊断转移成 Cgen 名称猜测。新增
`src/ir/xl6_ir_net_datagram_bridge.c`，显式登记 v125 发现的 29 个桥接目标：
地址列表、异步解析、UDP socket/packet/batch、异步接收及三类统计。
lowering 和 verifier 共用登记表；未知相似名称不会自动获得组件身份。
验证参数数量、借用/值传递、native-class handle 投影、物理表示、返回形式，
以及一个 native TAKE 构造器和两个 managed class Future 结果。

这些目标保留 `MAY_RAISE + FOREIGN_SCOPE`，参数准备失败仍按逆序退休拥有
输入；“属于编译器提供的组件”不再错误地隐含 `XRT_THREAD_ERROR`。后者会
改变原 native TAKE 结果的独占回滚合同，故 lowering 和最终 verifier 都
明确区分。Future payload 的具体 native 类型、拥有布局和转换提供者仍需
下一批实现，当前合同检查不能代替这些实现。

运行时提供者尚未接通，因此 Cgen 在输出 C 和声明头之前明确拒绝这些可达
组件，不输出半成品，也不回退到 legacy AST 后端。旧 UDP 函数体使用
`uint16_t` 端口、`int` 布尔结果、带整数返回的析构函数，而当前物理声明
要求相应的 `int64_t`、`bool`、`void`；不能直接包含旧实现。后续还必须
明确 resolver 的 WithPort 新拥有结果、packet 的借用/ref 转换、Buffer
数组元素的验证投影以及失败回滚，才能移除此临时准入拒绝。

共用一次源码构建产出六个测试驱动，清单为
`build/test/xlir_launch_v127_contracts/build.json`。本轮统一验证结果：

- 新 resolver/UDP 合同：29 项登记、313 个错误 IR 变体拒绝；每个变体
  要求 module verifier 拒绝且 C/声明头零输出。另有 29 项直接 matcher
  检查，拒绝误用 XRT_THREAD_ERROR。有效 IR 的提供者缺失也明确零输出。
- 原有 24 道门全部重新运行：23 通过、对象自动保活 1 失败；与 v123
  相比新增回归为零。十种对象用法中八种自动租约仍缺失，原断言未放宽。
  这 24 门执行累计约 160.5 秒，不含编译器构建与其它专项时间。
- 四组异步源码合同、重复生成、严格 C 和运行全通过：43,825 次执行、
  3,450 次真实分配失败、505 种错误 IR；事务及整轮内存平衡保持。
- 网络原三组保留：sendfile/TLS 各三次运行通过；fast_path 因提供者
  缺失在生成前拒绝，未进入编译/运行。仍为 2 通过、1 未解决，不算修复。
- 编码检查通过 6,297 项文本；三份批次报告均确认各自输入前后未变。

并行运行合同驱动时发现异步脚本无条件覆盖同目录已加载的 `xl.dll`，
导致 Windows 文件占用错误。脚本改为核对构建清单 SHA256，一致则只读
共享，不覆盖；不一致才复制且复核。修正后的四组测试全部通过。此故障
单独列为验证调度问题，不混入语言回归数。

证据冻结在 `build/tmp/xlir_launch_v127_evidence/`，包含原 24 门账本、
异步和网络专项报告、驱动构建清单、新合同及日志。失败生成对应的旧 C
文件不作为本轮产物收录。release CLI/DLL/VFS 未重建，兄弟 XRT 仓库
未修改；旧五项 runtime 门仍单列未重跑。公开编译入口、Linux、同源产品
准入与对象拥有图仍未完成，默认后端不变，长期目标保持进行中。

本轮是合同收口且未发现回归，未解决根因数并未下降。后续继续按固定用例
账本归并根因、成组修复、统一复验；新增覆盖单列，不能删除红项、放宽
断言或把明确拒绝误计为功能修复来制造下降曲线。

### 2026-09-11：v133 resolver/UDP 提供者接通与 mapper 拥有关系验收

本轮移除 v127 的临时提供者拒绝，新增独立
`src/cgen/xl6_cgen_net_datagram.c` 运行时组件，以及只消费已验证 IR 的
`xl6_ir_cgen_emit_net_datagram.c` 对象/Future 适配层。组件选择不扫描 AST
或函数名；29 个登记目标的参数、返回值、析构函数使用明确的物理 ABI。
地址解析保留 host/端口/family 参数检查；UDP 保留原收发、批次、move、
统计语义。接收结果不按运行时值的形状猜类型。

IR 补强 native 角色验证：每个 H 接收者必须绑定所属组件的 native 析构
合同；recvBatch、recvAsync 和 UDP stats 明确以 socket 为接收者，而非
以组件结果类型猜测。Future payload 必须有匹配的 native 字段/析构角色
及引用身份布局。同一桥接角色的结果类型不允许歧义。Buffer 数组借用则
独立验证其 native 字段，允许 Buffer 自定义相等性，不要求引用相等。

两条 Future mapper 的拥有关系：

- resolver：从源 Future 借用列表，`WithPort` 返回一个拥有引用；同端口
  路径增加引用，不同端口路径复制地址列表；语言 wrapper 消费这个拥有值。
- UDP：从源 Future 借用 packet，增加 native 引用后装入语言 wrapper；
  wrapper 和最终 boxed xvalue 任何一步失败都释放已取得的拥有值。
- 源 Future 的失败、取消、关闭通过 XRT Then 合同透传，取消输出会请求
  取消源；正常输出以 owned xvalue box 发布，未读结果也有析构路径。

去掉 Cgen direct-call 边界中按 runtimeBridge 有无重新推断错误 ABI 的
残留逻辑。后端仅检查已验证的 FOREIGN_SCOPE/XRT_THREAD_ERROR/result
carrier 物理形式，具体桥与错误合同的合法组合仍由 IR verifier 验证。
这使同步 CFG 和 async machine 共用的调用发射器遵守同一 IR 合同。

首次实际执行 fast_path 暴露旧 TCP sendVec 把 Buffer 语言对象当作裸句柄
读取的问题。TCP/UDP 统一按 Buffer TypeId 校验、IR 字段投影后借用 xbuffer；
不复制或消费数组元素。TCP 向量发送组件按可达数组调用选择；无使用时不
输出带未解析投影的函数。legacy 收集器保留原向量功能选择，并做源码语法
编译检查，未重建任何 release 产物。

当前专项证据：

- 原网络三组全部通过：fast_path 从 v127 未实现状态变为实际链接及三次
  完整运行通过；sendfile/TLS 各三次通过，原断言和完成标记未放宽。
- 新覆盖单列两组：独立异步 resolver 无 TCP 依赖；UDP 非 Buffer 元素
  拒绝、17 段堆分配向量/批次、实际 async packet、Buffer 借用后仍有效。
  两组各三次运行通过；五组 C/声明头/JSON 重复生成均字节一致。
- native/bridge 负向合同：339 个错误 IR 变体被拒绝且 C/声明头零输出，
  另有 29 项错误 ABI matcher 检查；有效 IR 正常生成组件。
- mapper 专项：503 个事务、500 次真实分配失败，覆盖同端口 ref（8 个
  分配点）、换端口 copy（9 个）、packet ref（8 个）。逐点故障每种重复
  20 次；失败 Future 必须携带内存错误且无值，借用源仍有效。每次事务和
  mapper 循环保持精确内存平衡，无 double/invalid free 或 UAF 计数增长。
- 四组异步源码合同再次全通过：43,825 次执行、3,450 次真实分配失败、
  505 个错误 IR 变体；原事务内存与终态/取消断言保持。
- 静态门精确登记新的独立运行时发射器，并新增该组件不得依赖 AST、HIR、
  semantic 或 legacy context 的检查；未放宽禁止回溯的规则。
- 编码检查 6,303 项通过；legacy Cgen 源码语法编译通过。
- 原有 24 门最终全部重跑：23 通过、对象自动保活 1 项仍失败，没有新增
  回归。该门十个模式中八个仍缺少自动租约，测试保护不计作生产修复。

使用 `build/test/xlir_launch_v132_contracts/build.json` 中的同一批编译器
构建。原 24 门最终复验与冻结证据分别为 `build/tmp/xlir_launch_v133_batch.json`
和 `build/tmp/xlir_launch_v133_evidence/`；历史失败尝试不覆盖为通过。

mapper 专项在真实 native 对象准备完毕、网络引擎关闭后检查映射事务，不
声称整个网络引擎/进程的堆分配平衡，也不证明 generated callback 卸载安全。
对象自动保活、公开编译入口、其它未覆盖网络 API、Linux 与同源产品准入
仍需后续完成；兄弟 XRT 修改尚未获确认，默认后端保持不变。

### 2026-09-11：v136 当前源码公开编译入口批量验证

此前实际 TCC 生命周期门先通过专用驱动生成 C，再手工加入 TCC；它们验证
了执行及退休机制，但不覆盖公开 xlc 上下文的完整编译路径。本轮新增
`build_xlir_public_source_driver.ps1`、`xlir_public_source_driver.c` 和
`check_xlir_public_source_batch.ps1`，从 56 个翻译单元独立构建验证程序。
API、导入、stdlib、语义、XLIR、Cgen、continuation 服务及 TCC 辅助工具
均来自当前源码；XRT 和 TCC 核心仍动态使用现有 release/xl.dll。

驱动使用 SDK 的公开声明和真实实现，不 include xl_api.c，不替换符号查询、
执行或删除钩子，也不预先生成被执行程序的 C。符号检查确认 xlcCreateContext、
xlcCompile、xlcRun、xlcRelocateModule、XLIR lowering/Cgen 和默认运行时
符号登记实现在验证 exe 的代码段；导入表未依赖 xlc.dll。项目源码严格警告
编译通过；未修改的 TCC 辅助源码单独保留其既有警告，不冒称第三方零警告。

初始 v134 批次：十组发射/确定性均通过；四组基础语言/导入执行通过，其余
六组实际运行因缺失 __xl_runtime 的声明头失败。原始报告和日志保存在
`build/tmp/xlir_launch_v134_initial/`，不将失败覆盖为历史通过。

修正的是验证环境的部署配置：按照仓库
`tools/tcc_vfs_manifest_win64_xlc.txt` 的现有九项私有头映射，在独立构建
目录创建磁盘 include/__xl_runtime 包，逐文件核对源/目标 SHA256，再用
公开 xlcAddIncludePath 配置。未重建 VFS 或 release，未借用 build/test
中旧测试残留的声明头，也没有在 Cgen 添加缺头时的兼容路径。

最终构建清单为 `build/test/xlir_launch_v136_public/build.json`；输入、
九个部署声明头、驱动和全部对象文件都有哈希记录。十组用例覆盖：

- 基础表达式、源导入、导入类/union/别名、可空函数默认值；
- async try/finally 返回、覆盖返回及抛错；
- fast_path、sendfile、TLS loopback、独立 resolver、UDP 边界五组网络场景。

每组经过公开 AddSourceFile/Compile 后，分别重复发射 C、声明头和 lowered
IR（字节一致），执行 xlcRun，以及 RelocateModule → RunModuleMain →
DestroyModule。十组全部通过；每个执行进程要求退出零并出现完成标记，
各阶段独立记录，45 秒有界，空 stdout 也能正确记录失败并继续其它组。

另以同一上下文执行十轮 AddSourceString 恢复测试：有效程序 → 语义错误
（发射失败且保留原输出 KEEP）→ 可编译但运行断言失败 → 新有效程序。
确认缓存/源替换生效、最后错误清除、成功退出；十次 AssertionError 是
主动注入的预期负向结果，不是忽略的运行失败。

本轮未改生产源码。v133 原 24 门的全部已有输入逐项哈希仍完全相同，因此
复用其 23 通过、对象自动保活 1 失败的固定账本，不重复运行；公开入口新
批次另列，不扩大原门的分母。报告与冻结证据分别为
`build/test/xlir_public_source_batch.results.json` 和
`build/tmp/xlir_launch_v136_evidence/`。

校正状态表述：当前源码的 xlcCreateContext 已默认 XLC_BACKEND_XLIR，SDK
也如此声明，本批刻意未调用 SetBackend 覆盖默认值。此前“默认后端不变”
只能表示本轮未调整后端选择、未重建发布物，不能解释为源码还默认 legacy。
本证据仍不是同源 release/VFS 发布准入，也不覆盖全部公开 API、跨语言 C
导入/外部二进制、Linux、完整堆审计或逃逸对象卸载安全。长期目标未完成。

### v137–v139：公开 C sidecar ABI 与源码包边界

新增 `check_xlir_public_interop_batch.ps1`，复用真实公开入口源码驱动；
驱动通过 xlcAddCSourceFile 装载一个或多个 C 文件。源码包查找仅临时设置
进程级 XLANG_PACKAGE_PATH，结束时恢复原状态，包括原先变量不存在的情况。
不修改用户级环境、release、VFS 或相邻 xrt 仓库。

首轮 v137 固定 11 组：9 个正向用例通过，2 个负向 ABI 用例失败。具体为
语言 int 对应的 int64_t 被 C int 实现、void 原生析构被 long long 实现，
均未被拒绝。首轮原始 C/头/IR/日志与报告保留在
`build/tmp/xlir_launch_v137_initial/`，不覆盖失败历史。

根因是 XLIR 头只发射导出声明，额外 C 文件的 module.h 包装因而没有外部
函数原型可校验。修复使头与实现共同使用经验证 CallTarget 的物理 ABI
原型发射器，包含原生 handle、借用字符串复制结果、raw callback 类型。
外部全局声明也提取为共同发射器；运行时 bridge 仍由其组件持有声明，
不从语言归一化类型另造一个原型。没有新增 AST/语义回查或名称猜测。

正向固定用例涵盖 bool、外部函数值、跨两个 C 文件的外部变量、静态可变
字符串复制、原生 handle 析构一次，以及 C 源码包、类/virtual/byref/out
包、多 XL 文件包、includeDirs 包。修复后固定 11 组全部通过，2 个负向
用例在 run 和 relocate 两条入口均以明确 incompatible 声明诊断拒绝。
新增覆盖单独计 3 组：C 基础类型、raw callback 加 byref/out、错误全局
变量宽度拒绝，全部通过。新增用例初稿漏写调用点 byref/out，被语义层
正确拒绝；修正测试语法，没有为此调整语言规则。

14 组均重复发射 C/声明头/IR 并逐字节相等，头独立以 GCC C11 严格警告
检查。全部 11 个原用例修复前后 C 和 IR 字节完全不变，变化在声明头。
正向运行与负向拒绝分别记账，不把预期非零退出计成运行成功。

v138 的核心统一回归暴露 4 项新增失败，均为新增 extern 原型需要 xfuture
而独立头未声明该类型。保留 `xlir_launch_v138_batch.json` 与原始日志，
不将该中间状态记作通过。v139 补齐 opaque 类型声明，并将语言函数值的
XL6_Function 物理结构声明从运行时代码中提取为共用发射器，供头按 IR
参数 ABI 选择。另设 2 组 header-only 检查，覆盖 Future 返回值和语言
函数值参数，均经过公开发射和 GCC 严格独立编译；不冒称其为新增执行测试。

当前公开编译器构建为 `build/test/xlir_launch_v139_public/build.json`，
56 个翻译单元全部重建，仍依赖未修改的 release XRT/TCC 核心。
旧公开入口 10 组（含网络/async）以及同一上下文 10 轮错误恢复也通过。
核心批次、最终输入哈希与冻结结果见 `build/tmp/xlir_launch_v139_batch.json`
和 `build/tmp/xlir_launch_v139_evidence/`；未运行的门仍单独保留，不补写通过。
v139 原 24 门全部重跑，23 通过、对象逃逸自动保活 1 失败；v138 新增的
4 项类型声明退步全部消除。源码编码检查覆盖 6311 文件，通过。编译器
构建、最终测试输入与 release 三个产物哈希核对无意外变化。

边界：本批证明显式 C sidecar 对声明头的 ABI 校验，以及所列源码包正向
执行，不代表导入包 C 源文件的所有负向 ABI 校验或任意外部二进制准入。
原有逃逸对象自动保活问题不由声明头修复解决；长期目标未完成。

### v140–v142：源码包与显式 C 文件统一准入

上一批边界中的源码包负向 ABI 检查现已补充。复用公开入口批量验证器的
PackageBatch 独立账本：固定 6 组含普通 C 包、includeDirs 包、双 C 文件
加 byref/extern global 包，以及错误函数宽度、错误全局定义、错误析构
返回值三个反例。v140 初始状态 3 正向通过、3 反例未拒绝；原始结果和
生成物保存在 `build/tmp/xlir_launch_v140_initial/`。

原因不是 IR 缺少 ABI，而是 API 中 importDeps.cSources 直接 tcc_add_file，
绕过已经生成的声明头。现将其与 xlcAddCSourceFile 的文件列表交给同一个
xlCompileUnitCSourceFiles：后端选定的声明头只生成一次，逐 C 翻译单元
拼入包装文本，交由已有 xlTccCompileSourceText 选择磁盘编译字符串或
VFS 内存源传输。可选 saveTemps 保留 module.h/包装源码，不再要求普通
执行为了校验而先写磁盘头文件。包、显式 C、混合两组列表不再维护各自
的原型校验逻辑，额外 C 文件的原有数量上限仍保留。

删除了两处裸编译 import C 的路径、固定长度路径/包装缓冲区及重复的
磁盘/VFS包装函数；API 相对本轮基线新增 41 行、删除 209 行，净减 168。
共同路径位于 XL_ENABLE_EXE_COMPILER 门外，运行时-only 配置也不能跳过
包内 ABI 校验；该配置经 GCC 严格语法检查，尚未作为独立运行时产物执行。
包装构造或编译任一失败时，整组失败，只有全部文件完成才返回成功。

v142 固定 6 组全部通过，另增混合包与显式 C 正反两组，均通过；后者
确认前面的包内 C 编译成功后，错误 sidecar 仍会阻止执行/relocate。
反例在这两个入口均以指明符号的 incompatible redefinition 拒绝。
原 6 组 C/头/IR 修复前后逐字节不变，每组重复发射及独立头编译通过。

旧跨语言 14 组和 2 个 header-only 用例、旧公开入口 10 组及同上下文
10 轮恢复也重新验证通过。最终驱动为
`build/test/xlir_launch_v142_public/build.json`，原 24 门账本与冻结记录
见 `build/tmp/xlir_launch_v142_batch.json` 和
`build/tmp/xlir_launch_v142_evidence/`。源码编码检查 6330 文件通过。
原 24 门全部新跑，23 通过、对象逃逸自动保活 1 失败，与 v139 相同，
未新增退步；构建对象、部署声明头、所有最终批次输入均经哈希核对。

本轮未改 Cgen/IR 语义，未重建 release、VFS 或相邻 xrt。实际验证覆盖
Windows 动态 TCC 的公开 run/relocate；外部 EXE/DLL 输出虽调用同一路径，
仍不冒称完成外部二进制发布准入、运行时-only 动态执行或完整 VFS 准入。
对象逃逸自动保活与长期目标仍未完成。

### v143：关闭 EXE 编译功能后的真实运行时入口

将公开源码构建器增加 RuntimeOnly 模式，56 个翻译单元以
XL_ENABLE_EXE_COMPILER=0 重新构建，驱动只包含运行时 SDK，不包含 API
实现，不替换执行/查符号/删除钩子，也不预生成被测程序 C。
`build/test/xlir_launch_v143_runtime/build.json` 记录模式、输入、对象文件、
私有运行时声明头和二进制哈希。

新增 `check_xlir_public_runtime_batch.ps1`，通过 xlRunScriptFile 与
xlCompileScriptFileToModule → xlRunModuleMain → xlDestroyModule 独立运行
同一批用例。13 正向含基础表达式、源导入/公开类型、可空函数默认值、
async try/finally、五组网络以及普通/includeDirs/双 C 加 byref 包；
3 反例为包内错误函数宽度、全局定义、析构返回值。16 组两条入口全部
通过，反例必须非零退出且包含明确符号的 incompatible 诊断。

同一运行时上下文另执行 10 轮字符串脚本成功 → 语义失败 → 运行断言
失败 → 新模块成功执行/销毁，确认最后错误清除、退出码恢复。期望的
AssertionError 是负向注入，不忽略其他错误。每个进程有 45 秒期限，
独立日志和完成标记；源码包路径只临时设置在进程环境并正确恢复。

二进制符号证据确认 7 个运行时入口及 XLIR lowering/Cgen 符号在本地
代码段，未定义/导入 xlcCreateContext、xlcCompile、xlcRun、xlcRelocateModule，
未依赖 xlc.dll。这补齐 v142 中“运行时-only 仅语法检查”的证据缺口。
仍使用未修改的 release XRT/TCC 核心和磁盘部署声明头，不等同于同源
release/VFS 或所有公开 API 的发布准入。

本轮生产 src/lib/SDK/运行时依赖均未变。v142 核心账本旧输入中仅公开
驱动构建脚本改变，该脚本不被原 24 门调用；因此保留 23 通过、对象
逃逸自动保活 1 失败的结果，不重复运行。新增 C 驱动和批次另列，未
扩大原分母。编码检查 6332 文件通过，冻结证据位于
`build/tmp/xlir_launch_v143_evidence/`。长期目标未完成。

### v144–v147：EXE/DLL 产物 ABI 与未完成的 DLL 退役

新增公开 xlcEmitExe/xlcEmitSharedLibrary 批次，固定 15 组单独记账：
5 个 EXE 正向、3 个 DLL 正向、7 个错误 ABI 拒绝。每次输出先写入 KEEP，
正向必须生成 PE 并实际运行；负向必须在具体符号上报告 incompatible，
且 KEEP 未变。不能用旧产物或普通构建失败冒充通过。

v144 中 7 反例通过，8 正向因私有编译目录未部署源码而失败。按照既有
VFS manifest 的 src/include/xruntime 映射，在独立构建目录物化 377 个
源码/头文件并逐项核对哈希，供现有 compiler-relative 路径使用；未改
项目根查找逻辑、未生成 VFS、未重建 release，也未为递归 import xlang
提供完整内嵌编译器包。部署后 5 个 EXE 通过，3 个 DLL 因导出表为空失败。

实际修复：XLIR 现在从明确的 Export/GlobalExport 记录与固定模块 ABI
发射 XLIR_C_API，而不是靠 C 编译器猜测可见性。定义和声明头共用标记
规则：现有 XL6_BUILD_SHARED 控制 Windows dllexport / ELF default
visibility，XL6_USE_SHARED 控制 Windows dllimport，普通 JIT/EXE 无
附加导出属性。不扫描源名字，不扩大成 export-all-symbols。固定 main、
init、checked unit、错误报告及 code-owner 绑定入口也有正确公开链接。

v147 原 15 组已有 14 通过、1 失败；另增全局变量/常量导出 1 组通过。
新增组既经 GetProcAddress 检查，也使用新生成声明头加 XL6_USE_SHARED
直接编译/链接 DLL 消费者，确认函数调用与可变全局/常量读取 ABI 一致。
正向 EXE 覆盖 nullable function、async try/finally、双 C 包、混合 sidecar
和独立 resolver。外部产物不依赖 xl.dll 或 xlc.dll。

保留失败是字符串/容器 DLL 的真实卸载安全问题：函数返回 42、模块清理
成功、FreeLibrary 成功后，进程退出仍在 ntdll RtlProcessFlsData 回调
到已卸载的 DLL 地址（0x10004c44），退出 -1073741819。GDB 日志位于
`build/tmp/xlir_launch_v147_dll_debug.log`。不是把运行失败误计成编译失败，
也不是关闭 FLS/缓存、跳过 FreeLibrary 或仅检查 stdout 标记来改绿。

只读检查 XRT snapshot 可见 heap cache 和 error runtime 各自持有 FLS
回调；公开 xrtThreadKeysClear 只清当前线程的动态键值，不能代表这些
内部槽及其他线程的完整退役。需要有线程安全、失败原子性与代码保活
顺序的 XRT 公共退役合同；未经跨仓库授权，不修改 sibling XRT 或手工
补 vendored 单头私有字段。此问题与原有对象逃逸拥有图问题均未关闭。

首轮缺部署和空导出表原始证据分别保存在 v144_initial、v145_initial，
最终批次及冻结记录为 `build/test/xlir_public_artifact_batch.results.json`
与 `build/tmp/xlir_launch_v147_evidence/`。跨语言 14 组、2 个 header-only
用例和公开入口 10 组及 10 轮恢复另行回归；原核心 24 门另行记账。
源码编码检查 6343 文件通过。本证据不构成可卸载 DLL 的完整发布准入。
最终原 24 门全量回归为 23 通过、原有对象逃逸门 1 失败，无新增核心
退步；旧跨语言 14 组、2 个 header-only、公开入口 10 组及 10 轮恢复
全部通过。外部 DLL 的 FLS 退役失败另列，不合并或隐藏在原 24 门中。

### v148：获准修改 XRT，关闭 Windows 内部 FLS 卸载故障

本轮用户明确授权修改 sibling `D:/GIT/xrt` 并复测。XRT core 新增
`xrtRuntimeRetireThreadStorage` 公共终态接口：所有内部 Windows FLS/TLS
惰性分配登记到静态、无堆分配的注册表。按 payload → error → heap →
borrowed/guard 顺序释放，不依赖首次使用顺序，不在注册表锁内执行析构。
堆缓存仍正常启用。动态 thread key 保留既有调用方拥有及显式销毁合同。

宿主必须关闭所有入口，等待 XRT 工作完成，释放外部资源、清理各线程动态键、
解绑上下文；宿主线程/Fiber 可继续存活，但退役期间不得退出、删除或重新进入
该实例。在代码/分配器仍驻留且不持有加载器锁时调用。失败保持代码驻留，只允许
重试，不恢复业务，也不调用错误 TLS；此前已完成的清理不回滚。重入或并发退役
返回 false，成功幂等。非 Windows 返回 false，不虚报 POSIX 支持。

XLIR 的 DLL 固定宿主 ABI 增加 `xl_script_runtime_retire_checked`，它先执行
经验证的 module finalizer，再退役该 DLL 自有 XRT 线程存储。生成声明头在
shared 构建/消费条件下声明该接口，C/C++ 链接一致。**不能仅调用可复用的
`xl_script_module_unit_checked` 后卸载 DLL**；最终卸载需调用新退役入口并
检查返回值。失败后不能打印 XRT 错误或 FreeLibrary。普通 JIT 模块不能退役
共享宿主 XRT，因此不会发射/调用这一 Windows standalone-DLL 入口。

按 `tools/amalgamate.py`（XRT 与 xruntime）及 `tools/sync_xrt_snapshot.py`
生成/同步，不手改合并头。当前 sibling 源码较旧 x-lang 快照已前进，因此本次
同步除了退役接口还带入已有 Pattern、调度器、TLS 等 API 更新：产品 API 从
3307 增到 3401，其中本轮新增仅 1 个；实施头从 7712242 到 8226385 字节。
这些是源码快照大小，不是 DLL 增量。发布 xl.dll/xlc.dll/xl.exe 哈希均未改变。

公开编译器与 runtime-only 私有验证驱动改用当前生成的 XRT/xruntime 实现
（`tools/xrt_single_header_impl.c`），不让新 API 链到不含它的旧 release XRT。
TCC 核心仍取自旧 release，未重建发布物或 VFS。原 24 门仍保留其历史的
source-compiler + released-runtime lane，不能将其说成完整同源发布验收。

批量复测结果，固定分母独立记账：

- 原 EXE/DLL 15 组 **15 通过、0 失败**（上轮 14/15）；另增全局导出/import
  1 组通过。字符串/容器 DLL 的 FreeLibrary 后进程退出已恢复正常。
- XRT 新门 GCC/TinyCC 各 5 个 FLS/TLS 释放失败点及 1 个卸载压力用例，
  **12/12 通过**。共 192 次真实装卸、768 次停泊线程退出、576 次停泊 Fiber
  删除，逐次检查代码页 `MEM_FREE`；覆盖分配失败、析构顺序、中途失败重试、
  幂等、递归/并发退役拒绝。不是禁用 FLS 或保留 DLL 不卸载。
- XRT 相关模块严格重建的 25 个测试进程退出 0；其中 2 个非本机架构 ABI
  测试明确 skip，因此本机有效 23 个，不能声称 AArch64/LoongArch64 已验证。
- 公开入口 10 组、10 轮恢复；跨语言 14 组及 header-only 2 组；包 8 组通过。
- runtime-only 13 正向 + 3 负向及 10 轮恢复通过，确实以 EXE_COMPILER=0 构建。
- 原核心质量门重新收集 **23/24 通过**；唯一红项仍是对象逃逸自动保活，
  没有新增核心退步，也没有把新用例混入原分母。

XRT 可复现门为 `D:/GIT/xrt/tools/check_runtime_thread_storage_retirement.ps1`；
结果在其 `out/runtime_thread_storage_retirement/results.json`。本轮汇总冻结到
`build/tmp/xlir_launch_v148_evidence/`。本轮关闭的是**内部线程存储回调引用**，
不清空中央堆 span，不收集对象拥有图，不替代外部对象/代码租约，也不等于
长期任务或所有可卸载 DLL 的完整资源生命周期验收完成。

## v149：物理拥有图和 native 环收集

完成 sibling XRT 的物理图检查以及 `xrtObjectGraphCollectOwned`，把 Value
外壳、COW backing、callable 和显式 native/context 拥有边纳入同一次计数。
新增实际收集回归：宿主别名仍在时不终结，释放后内部环真正回收；共享边不
重复扣减；失败不改变图；域外弱引用不能观察到被部分终结的环。

本轮独立结果：XRT 新测试 8/8 个编译器/布局进程，原相关 XRT 回归 24/24；
同源公开编译链 10/10 组及恢复通过。生成快照已同步，XRT 2964、xruntime
3411 个公共 API，发布 xl.dll/xlc.dll/xl.exe 未改动，VFS 未重建。

语言对象逃逸门十种模式各 25 次已重跑：需要自动保活的八种仍失败，纯内部
和普通标量两种通过。该门继续保留测试专用 guard 和历史 released-runtime
lane，不把 native 收集器新增绿项记成语言门已通过。原语言 24 门本轮没有
整批复跑，23/24 仍只是 v148 基线。

语言侧尚需将类型/捕获/cell 和模块退役域连接到完整图，补齐相关复杂载荷，
保证检查到提交期间的静止，以及生成 Trace/Drop 返回之后的代码释放。
详见 `XLIR对象拥有图接入评估.md` 的 v149 节。本轮不是长期任务完成声明。

证据冻结：`build/tmp/xlir_launch_v149_evidence/results.json`，SHA256
`7DE3F6B3543877A7BDDF3A3E67417077CC221E8ABC02BC91C29AA881BFD0547E`。
包含 XRT 新/旧批次、公开入口和对象逃逸十模式的独立日志及哈希。
编码检查通过（6343 个文本文件）。

## v150：语言捕获拥有边接入，模块自动保活仍未关闭

真实生成对象对接 XRT 物理图：闭包捕获、共享 cell、绑定方法 receiver、
闭包装箱、callable checked cast/容器投影。使用既有 verified capture ABI，
不新增 AST 回溯或按样例猜测。闭包/cell 保留创建方 ops 身份；未知环境及
未完成的 Future capture 图适配 fail closed，不虚报图完整。

新门 `tools/check_xlir_capture_ownership.ps1`：GCC O0/O2 与 TinyCC 3/3，
每进程 700 个真实生成图事务；另有 22 个 IR 变异拒绝、C/H/JSON 确定性和
精确内存收支。结果在 `build/test/xlir_capture_ownership_v150_final/`。
十个既有 closure/cell/async-closure/callable/bound-method 用例的 emit 和
严格 C 语法检查另行通过，不计成十个运行时通过项。

当前源码重建 56 TU 公开驱动，公开入口 10/10 及恢复通过。首轮各执行阶段
通过，但新增测试文件触发输入变动检查，不作正式批次；稳定后已完整重收。
对象门升级为 source-XRT lane 后，十模式各 25 次仍是八个自动保活 exit 2、
内部和标量 exit 0。保留安全 guard，不将图检查改绿记成模块自动 pin。
原语言 24 门没有整批复跑，23/24 仍是 v148 基线，不是当前完整验收结论。

新增隐式 var-callable 初始化正例单列 pending，详见对象拥有图评估。
完整 Future/frame/module 图、检查到提交的静止、析构返回后的代码释放及
最终同源/跨平台验收仍未完成。发布二进制和 VFS 未重建，长期目标继续保持。

冻结报告：`build/tmp/xlir_launch_v150_evidence/results.json`，SHA256
`6F12ABB04FC12093251627BDA7739B35A79F3C0B579D9A3CFDC592F154F387AB`。
编码检查通过（6348 个文本文件）。

## v151：上下文函数转换与返回所有权统一，拥有图自动保活仍开放

关闭 v150 的合法 pending 正例：开放 var 到非空具体函数类型的初始化、赋值、
实参及返回现在建立显式 checked-cast、may-raise 和 owned closure 合同。
`contextualCastSource` 将转换绑定到 Typed IR 的真实源表达式与确切目标边界；
验证器独立核验。显式 as/intrinsic 不伪装成上下文转换。普通和 finally 延迟
单值返回共享 typed conversion → owned result → pending-return commit 路径，
避免按转换前的 var 物理表示处理已经转换出的 closure。finally 接纳范围使用
已有 storage-release ABI，不新增零散的资源类型白名单。

新门 `tools/check_xlir_callable_contextual.ps1` 验证三个语言正例的公开 emit/
重复生成/run/module，并实际执行 async 与 generator 两种生成模块各 100 次
初始化/清理循环。GCC O0/O2、独立 TinyCC 共六条运行路径全部通过，逐轮检查
内存计数、字节数、分配释放平衡及非法释放。正例包含八种 TypeError 失败、
失败赋值保持旧值、实参验证失败不进入 callee、finally 精确执行及 async 返回。
IR 合同保持单独分母：14 个上下文 cast，99 个畸形 IR 拒绝。

独立 TinyCC 暴露 async program label 后直接声明不合法；async 与 generator
均改为 `label:;`，与普通基本块一致，不按具体指令修改生成结果。原始失败
日志保留在 `xlir_callable_contextual_v151_final`。宿主对异步 executor 的 busy
退役保持代码驻留并重试；不把 Future 已就绪误认为最后 worker release 已返回。

返回相关独立回归：1,000 次提交事务、10,500 个所有权场景（含 3,000 次真实
诊断分配失败）通过。返回投影 IR 108 个拒绝加 2 个 join CFG 拒绝通过。
所有权分类 337 RuntimeOps、1,158 consuming inputs、470 个拒绝和 47 个 CFG
检查通过；code-owner/await 及 dynamic-result 合同另行通过。静态架构门更新
为检查共享返回 helper 的转换后所有权处理，不再绑定已删除的重复实现拼写。

当前源码 56 TU 公开驱动重建，捕获图 GCC O0/O2/TCC 每条 700 个事务再次通过。
对象门当前源码 XRT 的十模式各 25 次仍为八个自动保活 exit 2、内部/标量 exit 0。
没有修改测试 guard 或把 native 图通过计成生产模块 pin。Future/frame/module
完整图、检查到提交期间的静止、最终生成析构返回后的代码释放继续开放。
本批不声明 nullable callable 或所有上下文转换均已完整迁移。

原 24 门没有整批复跑，23/24 仍只是 v148 历史基线。发布二进制、VFS 未重建。
本轮证据冻结入口：`build/tmp/xlir_launch_v151_freeze.ps1`；结果目录：
`build/tmp/xlir_launch_v151_evidence/`。不构成长期目标完成或正式发布准入声明。

## v152：补充 Future/Promise 物理图基础合同

完成 XRT 的 Future/Promise 同节点视图、错误 cause/取消父链视图，以及
带显式载荷/上下文 Trace 的原子终态发布。保留 native Retain/ResolveOwned
原有语义；未知等待帧和未描述的拥有载荷拒绝检查，不假装是叶子。
具体边界见对象拥有图评估 v152。语言 Cgen 本轮没有新增语义推测或回溯。

XRT 新门四个编译器/布局进程通过，原相关回归 19/19 通过。生成头按流程
同步：XRT 2969 API、xruntime 3416 API（新增 5 个），发布二进制/VFS 未重建。
同源公开驱动重建 56 TU，10/10 公开用例和恢复通过；闭包拥有图 GCC O0/O2
与 TinyCC 每条 700 个事务通过。对象自动保活仍按原十模式独立验收，不能
把这轮运行时基础合同计成它已通过。原核心 24 门没有整批重跑。

下一阶段仍需把 traced publication 接入 verified AsyncResultAbi 的生产端，
连接 ValueFuture、frame/Watch 和模块全局拥有关系，并建立从图检查到最终
析构返回后的安全退役。公开新增接口不等于这些语言路径已经自动采用它。
本轮汇总冻结入口为 `build/tmp/xlir_launch_v152_freeze.ps1`。

## v153：语言 Future 结果拥有图与任务池原子发布

已把 verified AsyncResultAbi 对应的析构和 Trace 合同接到普通 ready 返回、
状态机完成、任务池 async 返回、task.spawn 和 Future continuation 的生产端。
resident 五类载荷由同一个 kind 选择实际驻留的 Drop/Trace 地址；closure 使用
生成的精确 closure view。Future capture 消费 xrtFutureOwnership，ValueFuture /
ValueFutureTake 包装绑定其实际 Future 拥有边，不再把这些可描述的终态图拒绝为未知。
已有的未知 waiter/payload 拒绝合同保持不变。

任务池增加 `xrtTaskSubmitTraced`，在入队前固定结果 Trace，并与 owned 结果原子
发布。只增加私有 job 字段，不改变 xtaskargs/xtaskvalue 的公开布局和旧接口语义；
提交失败不消费数据，任务失败/取消仍释放未发布结果。新增 private runtime 的
xl6PayloadResolveOwned / xl6PayloadTraceGet 已注册公开编译链、one-shot 宿主及
产物依赖扫描。`.then` 的任意指针型参数读取统一为 `%s const*`，避免把存储层
const 错加到 pointee，不引入按 closure 名称处理的特例。

本批固定分母：

- GCC O0/O2/TinyCC 各 700 个真实生成图，覆盖转发任务结果、Value/callable、
  字符串、Future capture、真实挂起恢复、then 和 task.spawn。逐例验证精确节点/
  边数、原生 Retain 外部根、调用结果和全部内存收支；每条另跑 1500 个 resident
  载荷成功/拒绝/空值场景，含嵌套 Future、非法 kind 原子性和三类裁剪入口。
- 新 Future IR 门为 4 targets、3 results、1 source、1 capture，25 个 C/header
  输出前拒绝；既有结果提交 15、回调 188、闭包捕获 22 个拒绝继续通过。
  code-owner/execution 的 21 + 14 await + 2 source linkage 检查通过。
- XRT GCC/TinyCC × 任务结果/既有 Future 图两布局，加 ValueFuture 模块化普通/
  OOM 与单头，共 14 个进程通过。任务结果每进程 700 个结果、300 个提交拒绝，
  其中 100 次实际 OOM。原 core/cancel/future/continue/combine/task/pool 26 个
  回归通过；task-pool 测试使用的 Atomic 依赖另有两个检查，单独记账。
- 当前源码 56 TU 公开编译驱动的 10/10 emit/确定性/run/module 和恢复通过；
  既有闭包图三条路径每条 700 个事务通过。对象逃逸原十模式各 25 次仍有八个
  自动保活 exit 2、internal-only/trivial-value exit 0，没有把测试 guard 当作 pin。

初轮测试基础设施问题已保留日志并修正：生成 closure 的宿主调用必须使用统一
bool + out ABI；单头 ValueFuture 文件没有注册在模块测试清单中，改为显式编译
执行；PowerShell 单元素 CRT 参数强制数组；既有 task-pool barrier 测试显式补齐
Atomic 编译依赖。没有通过放宽严格 C、图/析构断言或减少既有回归分母结项。

XRT 快照由生成工具同步为 2970 / xruntime 3417 API。证据入口：
`tools/check_xlir_future_ownership.ps1`、sibling `tools/check_task_result_ownership.ps1`；
汇总 `build/tmp/xlir_launch_v153_evidence_final/results.json`。

仍未完成：聚合和部分网络/原生适配器仍使用旧 owned 发布；挂起 frame/Watch、
模块全局内部环、检查至提交期间全图静止，以及最后生成 Trace/Drop 返回后的
自动模块退役。原 24 门未整批复跑，23/24 仍是 v148 历史基线。旧 release C-host
payload 脚本依赖已发布 runtime，不能用于本批新增 API；当前源运行时载荷证据
单列，不冒充其 JIT/AOT 历史门已重跑。CLI/DLL/VFS 未重建，Linux、同源发布及
legacy 物理退役继续开放；长期目标保持进行中。

## v154 / v155：聚合拥有图、事务式启动和语言终态收敛

v154 的显式 Watch、CancelWatch 和原生共享聚合节点已完成：新增四个编译器/
布局测试进程及既有八个图测试通过，原相关回归 26/26，Atomic 支撑 2/2。
公开源码驱动构建成功；语言侧后续复测由 v155 的最终源码批次覆盖。

v155 消除“先启动聚合，再分配 continuation/cancel-link”的半启动窗口：
XRT All/Any/RaceMapOwnedTraced 在接管前完成全部可失败准备。XLIR all/race
不再经过 legacy 元数据跟踪，whenAny 的映射、发布和析构不再复制到每个生成
模块。载荷通过 resident Drop/Trace 原子发布，源失败保持原始错误身份。

本轮批量发现并关闭两项语言/产物问题：

- 无返回值 task.all 原先直接消费原生 All 协调结果，吞掉失败源；持久正例
  `xlir_task_all_void_outcome.xl` 在修改前确实运行失败，修后字面量和数组均传播
  失败。它使用与拥有值 all 相同的终态观察合同，不新增 Cgen 语义猜测。
- whenAny 数组适配器的分配器声明补全；语言取消诊断统一到 resident outcome
  observer，同步 await/异步恢复共享。取消错误的分配失败保留 XERR_CANCELLED
  种类，不能把取消改成失败。架构门改为验证新边界及 resident 原始错误 retain，
  不再要求搬走的实现拼写仍出现在 Cgen 内。

最终独立分母：

- XRT 新映射四个进程，每进程 1,000 个生命周期场景、9,600 个启动探针及
  2,400 次实际准备失败；既有拥有图 12/12，原回归 26/26，Atomic 2/2。
- 语言聚合图 GCC O0/O2/TinyCC 各 3,500 个事务及 192 个桥接启动探针，
  每条 34 次准备失败和 8 次受理后的映射失败；精确图/析构/内存断言均保留。
  9 个 IR 操作、36 个 C/header 前拒绝。五个公开入口 emit/确定性/run/module
  均通过；相同五个程序 GCC O0/O2 的 10 条严格 C/实际运行路径通过。
- 原 Future 图三条各 700、resident payload 各 1,500、结果观察各 600 加 100 次
  真实诊断分配失败；原 capture 三条各 700；相关 25/22/15/188 和 owner/await
  21+14+2 的合同检查通过，保留各自分母。
- 通用异步终态在当前源 XRT 下 GCC O0/O2/TinyCC 每条 2,200 次执行、3,000 次
  真实挂起、16,549 次平衡分配，通过 catch/rethrow/finally 及嵌套挂起断言。
  这是新同源 native 证据，不冒充旧 release C-host JIT/AOT 门已经复跑。
- 公开源码入口原 10/10 加十轮错误恢复通过。对象逃逸十模式各 25 次仍是
  八个自动保活失败、两个内部/标量通过；测试 guard 没有升级为生产 pin。

证据目录：`build/tmp/xlir_launch_v155_evidence/`；原始失败批次和修后结果分别
保留在 `build/test/xlir_future_aggregate_ownership_v155` 与 `_v155_final`。
XRT 快照按生成工具同步为 2975 API / xruntime 3422 API。CLI/DLL/VFS 未重建，
完整原 24 门未复跑，23/24 仍仅是 v148 历史基线。完整 frame/continuation 图、
模块全局环与安全退役、Linux、同源发布及 legacy 物理退役继续开放，目标未完成。

### v156：async 帧的真实拥有槽、独立传输租约及同源批量复测

本轮继续 v155 未完成的挂起帧链，不重写已完成的聚合器。运行时新增
`xl6ContinuationTraceBind/Ownership`，ticket 明确记录帧/队列租约与 Watch
存储租约；注册失败还原，TransportReleased 归还后才允许队列发布，物理
释放前核对最终计数。前后帧继承执行容量和原有 code-owner lease 的机制不变。
Watch 到 ticket 的一条边只代表其确实归还的存储引用；队列执行根不能扣掉。

统一 frame slot 新增 trace ABI，经 loan 归一后赋值并在两种 machine 的
verifier 中独立重算。Cgen 只消费该字段及 init bit，输出确切 Value/Future/
closure/cell/error 边；没有 trace 合同的 generator/iterator 状态明确拒绝图
检查。43 个物理槽（async 37、generator 6）的 129 个变异均在 C/header 输出
前拒绝。此处 generator 的 verifier 覆盖不是其运行时图已经接通的声明。

发现批次完整保留：`xlir_frame_ownership_v156` 中两条 TinyCC 测试的结构体
数组初始化触发编译器内部错误，已用等价显式赋值修正；其余 12 条通过。
`xlir_terminal_source_v156` 暴露纯标量 async 缺失 ownership 声明依赖，已在
统一 `NeedsXrtDeclarations` 中补全，仍使用 declaration-only 头。
既有 Future 门原来要求这个挂起帧 opaque；现在精确验证 5 节点/6 边/3 根，
并新增真实 unknown native Watch 保留原子的拒绝断言，而不是删除负例。

最终独立分母：

- 新帧门 14/14：生成帧和原生图各 GCC O0/O2/TinyCC 三条；旧 queue/handoff/
  executor/progress 原断言各 GCC O0/O2 八条。每条生成帧路径 1,500 事务、
  3,000 次实际 pending 与 100 次 ready 恢复，逐轮验证精确节点/边/根、共享
  capture、cell 更新、原始错误身份、五种终态及内存平衡。每条原生图路径
  400 个 Watch 生命周期、2,400 个检查分配探针（1,600 次真实失败），另有
  100 个未知 code-owner 场景，挂起和排队时均拒绝，代码 lease 在 frame
  物理析构返回后归还。生成图使用宿主驱动队列，未实现生产全图暂停。
- 原通用异步终态三条均通过：每条 2,200 执行、3,000 实际挂起、16,549 平衡
  分配。正常 worker、catch/rethrow/finally、嵌套 await 断言不变。
- 原 Future 三条各 700 图、resident payload 各 1,500、outcome 各 600 加 100
  次诊断分配失败；capture 三条各 700 图及 22 个 IR 变异；全部通过。
- 原聚合三条各 3,500 图、192 启动探针、36 个 IR 变异；五个公开程序全部
  emit/确定性/run/module 通过，同五程序的 GCC O0/O2 严格 C 十条全部通过。
- 当前源码公开入口原 10/10 加十轮恢复通过。对象逃逸原十模式各 25 次仍为
  八个自动保活失败、两个内部/标量通过，没有把 guard 当成生产 pin。

最终当前源码编译器：`build/test/xlir_launch_v156_verified_public`（56 TU），
IR 驱动：`build/test/xlir_launch_v156_verified_contracts`（23 TU 共享、五驱动）。
最终帧/终态/Future 报告分别在 `xlir_frame_ownership_v156_final`、
`xlir_terminal_source_v156_final`、`xlir_future_ownership_v156_final`；失败批次
不覆盖。冻结证据在 `build/tmp/xlir_launch_v156_evidence/`。

完整 24 门本轮未重跑，23/24 仍仅为 v148 的历史基线。模块绑定的 code-owner
图、generator/iterator 及其它 native 载荷、模块全局环、从检查至提交的生产
静止、最终生成 Trace/Drop 返回后的自动代码退役、Linux、同源发布、legacy
物理清理继续开放。此次未修改 XRT 或重建发布 DLL/CLI/VFS，长期目标未完成。

### v157：完整 generator/iterator 拥有槽及真实消费路径

完成 XRT 只读迭代器视图，规范化到实际 retained backing；当前元素和键保持
借用。生成器分别描述控制块、唯一帧和当前 yield 的拥有载荷，保存创建模块
Ops 身份，运行/清理中拒绝检查。共享帧 trace ABI 新增 VALUE_ITERATOR 与
GENERATOR，generator 的 itemTraceAbi 按 yield ABI 独立验证；文本/JSON 均
可检查。async/generator 使用同一个帧槽发射器，未知拥有载荷仍拒绝检查。

本轮批量发现两项真实语言问题并修复根因：

- 合成 catch error 使用 ABI 0，而声明 error 使用 xlang.error；相同语言类型
  出现两个 TypeId，直接 yield 捕获错误失败。现在在类型池入口使用前端相同
  的 ABI，不在 yield/Cgen 添加同 C 类型强转或绕过精确 TypeId 检查。
- async/generator 原先未接通生成器消费指令，合法的跨 await 迭代无法发射。
  普通、async、generator 共用 NEXT/GET/TAKE 与成功/结束结果的机械发射器；
  使用既有已验证操作和错误 CFG，不新增一套按执行模式拼接的语义实现。

固定分母及当前源码复测：

- XRT 新门 GCC/TinyCC × 模块化/单头四个进程，每个 800 图、6,400 检查探针、
  3,200 次实际失败。四容器、双方向、栈/堆、重复边、Retain/COW、源释放、
  非法游标和逐例内存断言通过；原拥有图八进程及原相关回归 24/24 分开通过。
- 新语言图 GCC O0/O2/TinyCC 三条，每条 2,400 个生成器生命周期、1,000 个
  async 迭代事务、2,000 次真实挂起、1,800 次运行中拒绝、16,000 检查探针
  和 6,600 次实际失败。覆盖 Value/closure/Future/error/generator/string/
  scalar yield、嵌套消费、当前项转移与丢弃、提前关闭、错误身份及五种终态。
  精确图/根/内存断言保留；宿主队列提供全图静止，不冒充生产并发安全点。
- 八个 generator machine、23 个帧槽、16 个消费操作共 141 个 C/header 前
  变异拒绝；原 43 帧槽、129 变异及帧门 14/14 继续通过。
- 原终态三条每条 2,200 执行/3,000 实际挂起；Future 三条各 700 图/1,500
  resident 载荷，capture 三条各 700 图；聚合三条各 3,500 图/192 启动探针/
  36 IR 变异，五个公开入口与十条严格 C 路径继续通过，保留各自分母。
- 公开源码原 10/10 加十轮错误恢复通过；另有原 cgen_generator 和新组合
  程序的 emit/确定性/run/module 两个独立正例，验证声明依赖和默认入口。
- 原对象逃逸十模式各 25 次仍八个 exit 2、两个 exit 0；没有把新图门通过
  换算成自动对象保活完成。完整原 24 语言门没有重跑，23/24 仍仅为 v148 基线。

发现记录保留在 `xlir_generator_v157_probe`、`xlir_generator_v157_discovery`、
`xlir_generator_v157_runtime_probe` 和 `xlir_generator_public_v157`。测试初稿的
generator 原始 C export 及 C 风格逻辑符号属于测试入口/语法问题，未放宽语言
规则；改用测试绑定已验证内部 ABI，以及合法 xlang 语法。架构门不再要求已被
明确描述的 iterator/generator 仍是 OPAQUE，但保留未知载荷拒绝并要求共享发射器。

最终来源：`xlir_launch_v157_discovery_public` 的 56 TU 编译器通过输入哈希再次
确认仍是当前源码；`xlir_launch_v157_verified_contracts` 为 23 TU、六个驱动。
新运行图与公开入口最终报告为 `xlir_generator_ownership_v157_final`、
`xlir_generator_public_v157_final`；XRT 最终新门为 sibling
`out/value_iterator_ownership_v157_final`，汇总冻结至 `xlir_launch_v157_evidence`。
快照使用生成工具同步为 XRT 2976 / xruntime 3423 API，没有手改单头实现。

模块 code-owner 图/自动 pin、全局环、其它 native 载荷、生产检查至提交的
静止与析构返回后的安全卸载，及 Linux、同源发布、legacy 物理退役继续开放。
发布 CLI/DLL/VFS 未重建；标准库仍只保持最低必要基线，长期目标继续进行。

### v158：模块全局拥有槽、规范代码节点与旧前缀兼容

不再把所有绑定模块的 continuation 一律当作没有描述的 code-owner：新增
带标记的可选 CodeOwnerGraph，旧四字段 ABI 保持不变。CodeLease 的共享
引用数与它实际 acquire 的模块边分别描述；ticket 在 predecessor/successor
交接时继承同一条代码边。没有描述符的旧能力仍拒绝检查，未更改其获取、
关闭、继承或常驻尾部释放语义。

模块宿主新增规范物理节点及只读全局 Trace 边界。Lowered GLOBAL storage
固定 moduleTraceAbi，验证器按物理类型和 cell 归属独立重算；其他 storage
不得冒充模块拥有槽。Cgen 消费统一视图映射，并保护初始化/清理阶段；未知
全局表示和未描述的模块任务池/网络状态明确拒绝。全图静止仍由调用方提供。

批次发现和修复记录保持分离：

- 新 IR 测试初稿调用了不存在的 dump 名称，修正为既有 DumpModuleText。
- O0 白盒宿主原来依赖优化消除未链接的公开 API，已按最终源码构建记录提供
  完整依赖并使用一致的 compiler 配置，没有删掉 O0 路径或填充假实现。
- 首稿把闭包独占环境重复计成节点；根据既有实际槽合同修正为七节点/七边，
  没有改动生产 Count/Trace 去迎合测试数字。最终所有图断言均固定。
- 旧前缀复制反例在 `xlir_launch_v158_legacy_discovery.log` 中真实失败；
  宿主现在仅向具有新 Trace 入口的模块传递扩展，旧模块收到准确的前缀大小。

最终编译器为 `xlir_launch_v158_verified_public`（56 TU），合同驱动为
`xlir_launch_v158_verified_contracts`（23 个共享 TU、四个驱动）。固定分母：

- 新模块 IR：41 个全局槽、236 个其他槽、557 次 C/header 前拒绝，三个
  保留符号冲突变异在其中。原 code-owner/execution 的 21 + 14 + 2 保持通过。
- 模块图：GCC O0/O2 宿主、实际 TinyCC 生成模块各 50 次生命周期、25 次
  successor 交接、325 次原子拒绝、600 个检查探针及 200 次实际分配失败。
  保留原生 Retain 根、精确图、关闭后继承、最后释放顺序、实际 MEM_FREE
  和逐事务内存平衡断言。报告：`xlir_module_ownership_v158_verified`。
- 绑定原五模式加 traced、legacy-prefix 共七模式通过；原停用 400 次事务、
  3,600 次重入拒绝通过；原退休 400 次重试、100 次真实依赖库卸载、200 次
  ticket abort/successor 链通过。退休门的 TCC 删除仍是记录型，不冒充新门
  的实际代码页释放。它们均链接当前源码 XRT，不混用旧声明与发布运行时。
- 原 shared CodeLease 门 350 次真实 DLL 生命周期、200,000 对并发保留/释放、
  100 次实际分配失败通过；旧 C-host JIT/AOT 各十次 smoke 单列通过，不能
  将它们记为新完整源码发布验收。
- 原帧门 14/14、43 槽/129 变异；原生成器 GCC O0/O2/TinyCC 三条以及八机器/
  23 槽/141 变异继续通过，既有运行/故障注入分母不变。
- 原公开源码十程序全部 emit/确定性/run/module 通过，十轮错误恢复通过；
  原对象逃逸十模式各 25 次仍八红、两绿，没有转为 XFAIL 或缩小断言范围。

本轮未改 sibling XRT 或重生成其快照，未重建发布 CLI/DLL/VFS。原 24 个语言
门未整批复跑，23/24 仍只是 v148 历史基线；Future/capture/聚合独立专项的
上一批完整结果仍为 v157，不据本轮相关帧回归冒充它们全部重跑。
冻结证据为 `build/tmp/xlir_launch_v158_evidence_final/results.json`。首次封存目录
因尝试读取仍在写入的封存日志而中止，保留未覆盖，不作为完成的证据入口。

剩余主线是逃逸对象自动拥有代码租约、常驻回调退役边界、模块全局内部环、
其它未知 native 载荷和生产 Inspect 到 commit 的安全机制；随后还须完成
同源发布、Linux 和 legacy 物理退役。标准库仍只保最低必要基线，长期目标
保持进行中；只读模块图接通不等于自动对象生命周期已经完成。

### v159：补齐对象终结的物理生命周期，保留自动拥有图硬门

本批先用原 XRT 复现“先释放所有外壳、再结束最后游标”漏掉终结器的问题。
修复位于 XRT 原子 backing 释放、真实游标拥有槽及一次性终结状态，不在类
Cgen 增加按名称识别的补丁。新的拥有式 FinalizerBind 将上下文释放延后到
字段清理之后；常驻 xl6CodeFinalizerBind 保留代码直至最后生成回调返回。
生产类绑定尚未切换到这个有分配/失败路径的新合同，自动租约与内部环仍开放。

C sidecar 新语言回归先暴露完整声明头被提前裁剪 guard 阻断的问题。改用统一
声明头作为源包装器前缀后，正/反向、原壳/COW 的类游标场景 run/module 各
200 次析构通过，重复生成的 C/header/IR 完全一致。发现阶段的错误 include
尝试、错误的模块门 driver 路由，以及测试初稿误写 xrtValueHandle 的编译
失败均保留；分别修正为 canonical 声明前缀、真正的生成 driver 和 HandleTake，
未删除已有门或放宽断言。

最终源码编译器：`xlir_launch_v159_verified_public`，57 TU；最终合同驱动：
`xlir_launch_v159_final_contracts`，23 个共享 TU、四个驱动。早期 discovery
编译器与第一个 verified_contracts 已因后续源包装器修复过时，不混入最终输入。

同源固定分母：

- 新 XRT 终结门 GCC/TinyCC × 模块化/单头四进程，各 501 生命周期、100 次
  四线程释放、2400 绑定拒绝、4800 图探针/1600 实际失败；普通迭代器原四
  进程与拥有图原八进程，以及原 XRT 回归 24/24 分开通过。
- 新常驻终结门 GCC O0/O2 各 250 真实 DLL 生命周期、500 Finalize/context
  Release、250 字段 Drop、25 分配失败、25 接管拒绝、4900 图探针/1400 实际
  失败。真实代码页 MEM_FREE、关闭后继承、提取子对象、终结器新建游标、
  原生 Retain/COW 别名、未知 owner 拒绝和逐事务诊断/内存平衡均通过。
- 原帧 14/14、43 槽/129 IR 变异；原生成器三条、八机器/23 槽/141 变异通过，
  不缩小既有运行和失败注入分母。原模块图 O0/O2 各 50 真实 TCC 生命周期、
  25 successor、325 原子拒绝、600 图探针/200 实际失败通过；模块 IR 仍为
  41 全局/236 其他槽、557 个 C/header 前拒绝。
- 原 code-owner 七种绑定、停用 400 事务/3600 重入拒绝、退役 400 重试/
  100 真实依赖库卸载/200 ticket 链、CodeLease 350 DLL 生命周期/200000 对
  并发 retain-release/100 分配失败保持通过。旧 C-host JIT/AOT smoke 单列，
  不算新源码发布验收。
- 原公开源码十程序所有 emit/确定性/run/module 与十轮错误恢复通过；原
  对象门十模式各 25 次仍八个 exit 2、两个 exit 0，没有转为 XFAIL。

XRT 生成工具与同步工具已更新 2977 / 3424 API 快照；声明头、符号表、构建
源清单同步，未手改 amalgamated 实现。完整 24 语言门未重跑，Future/capture/
聚合独立专项的上一轮完整证明仍为 v157，不用相关帧回归冒充全部重跑。
发布 CLI/DLL/VFS、Linux、legacy 物理退役仍待；标准库仍维持最低基线。
冻结入口为 `build/tmp/xlir_launch_v159_evidence/results.json`。自动对象保活、
模块内部环与生产图安全点仍未达标，长期目标继续进行。

### v160：共享构建、隔离并行与一轮集中发现

本轮不改 IR/Cgen/XRT 的生产语义，先兑现批次加速的两个执行入口：按实际
预处理内容复用 GCC 对象文件，以及基于经审计资源声明的并发调度。公开驱动
临时目录和 module/object 的生成文件、声明头已隔离，原有断言及分母保持。

构建快检覆盖冷/热/强制重编译、与普通 GCC 直接编译的对象字节一致、头文件
保留时间戳的内容修改、新增高优先级同名头、included C、编译参数、工具链
标识变化、缓存损坏、编译失败和工具路径覆盖。初稿遗漏预处理注释导致 XRT
内 BBRE 的 fallthrough 注解失效，严格编译真实报错；已保留宏内外注释并补
普通 GCC 对照，没有关闭该警告。缓存候选识别也不能把 `config.include`
这样的普通成员访问误当作汇编外部输入。

同一工具链和同一源码下，57 TU 冷构建 38.932 秒；热构建 57/57 命中、0 个
重编译，含重新链接共 14.948 秒，构建阶段约减少 61.6%。这是本机两次构建
的观测，不代表测试或整体开发时间减少同样比例。合同驱动 23 个共享 TU、
四个驱动一次构建 14.594 秒。最终驱动目录为 `xlir_launch_v160_final_public`
和 `xlir_launch_v160_final_contracts`；冷构建目录单独保留。

调度器保留旧串行清单，新增命名参数按 JSON 数据传递、独立资源并行、共享
资源互斥、`*` 全局独占。快检实际验证双进程重叠、共享文件锁互斥、带空格/
引号/中文的参数、并发后完整收集失败及旧证据/输入指纹保护。已处理的原生
负例退出码不能覆盖 PowerShell 脚本的最终成功状态；显式失败仍保留原退出码。

一次集中发现于 2026-09-13 03:47:27 +08 完成，六门、峰值三进程、80.778 秒；
4,003 项整批输入指纹前后一致。五门通过，对象逃逸门保持失败：

- frame 原 14/14、43 槽/129 IR 变异通过；generator 原三编译器路径与八机器/
  23 槽/141 变异通过，运行和故障注入的原分母不变。
- module 原 O0/O2 实际 TCC 路径及独立模块 IR 验证通过，未把只执行断言的
  module driver 错当成生成 driver。
- 常驻 finalizer 原 O0/O2 两条与四个语言阶段通过；公开入口原 10/10 和
  十轮错误恢复通过。
- 原对象逃逸十模式各 25 次仍八个 exit 2、两个 exit 0。一次完整收集，未
  转 XFAIL、未删除守护及真实 TCC 页释放/内存平衡检查。

批次报告为 `build/tmp/xlir_launch_v160_ownership_discovery/results.json`。
此报告是集中发现和加速机制的运行验证，不是自动对象保活验收完成。下一步
按对象拥有图评估中的四个关联边界集中修复；完整 24 语言门、同源发布、Linux
和 legacy 退役仍未完成。本轮没有重跑 sibling XRT 专项或重建发布 CLI/DLL/VFS。

### v161：可失败的类发布与常驻回调作用域

本轮落实生产语义修复，不继续扩展构建缓存。类析构绑定原来在生成 helper
内直接 abort，现由 XLIR 明确为 MAY_RAISE / NULL_RESULT，立即检查结果并按
既有异常/清理 CFG 转移。完整 payload 在创建生产指令时即写入，不在错误
分支生成后修补“最后一条指令”。新 XRT `FinalizerBindTake` 消费失败输入，
保存绑定主错误直到字段/原有 finalizer 回滚结束；Cgen 只发出固定 ABI 调用。

常驻 CodeScope 使用调用方栈记录，Enter/Leave 无分配，真实租约引用使运行中
回调成为图的外部根。ScopedAcquire 可从同线程、相同完整 owner 身份的活动
回调继承已接受的能力，关闭后的普通 Acquire 仍拒绝。已接入常驻 CodeDrop
和拥有式 Finalizer 适配器，最后 Release 位于生成回调返回后的常驻尾部。
这不是闭包自动租约：生成闭包/xcall 外层回调、模块自持环、安全快照到提交
以及构造期间 self 提前发布的完整语义仍待完成，不能用当前安全拒绝当终点。

快检先验证新增实际失败、IR 变异及真实 DLL。XRT 原生测试初稿把借用的
handle ops 放在返回后失效的局部栈上，发生访问违例；改为静态 const 描述符后
修复，未放宽生产断言。生产编译器和运行时只构建一次：57 TU 公开驱动与
23 个共享 TU、五个合同驱动，目录分别为 `xlir_launch_v161_discovery_public`
及 `xlir_launch_v161_discovery_contracts`；在后续验证时逐项复核仍是同一输入，
不因目录名含 discovery 而重复重建。

随后一次七门集中回归，峰值三进程，100.642 秒，4,011 项输入前后一致：

- 原 frame 14/14、generator 三条、module 两条和独立 IR、常驻 finalizer
  两条及四语言阶段、public 十程序和十轮恢复全部通过；原分母不变。
- 新发布门：3 绑定/18 C-header 前变异拒绝；O0/O2 各 100 成功与 100 实际
  绑定拒绝，局部/子对象/析构和 allocator 平衡；emit/repeat/run/module
  通过，重复 C/header/IR 相同。Scope O0/O2 各 50 DLL 生命周期、25 图根
  变化、100000 无分配线程继承、100 跨线程和 100 身份拒绝通过。
- 原对象逃逸十模式各 25 次仍八个 exit 2、两个 exit 0；整批仍失败，不转
  XFAIL。相关底层通过不能算作自动对象拥有图已接通。

此外，XRT 新发布专项及原终结专项在 GCC/TinyCC × 模块化/单头四路径通过；
新专项每路径 500 生命周期、400 拒绝、100 无分配提交，原终结专项保持每
路径 501 生命周期/100 四线程释放/2400 拒绝/4800 图探针/1600 实际失败。
原 CodeLease 同源 O2 保持 350 DLL 生命周期、200000 并发引用对和 100 OOM；
原三组类析构 fixture 的同源 emit/repeat/run/module 全部通过，不冒充旧 CLI
质量门重跑。旧运行时的 JIT/AOT lease smoke 在快检时通过，仍单列为旧发布。

XRT 声明/实现/符号快照经原工具同步为 2978 / xruntime 3425 API。发布二进制、
VFS 未更新，完整原 24 门仍未整批复跑，23/24 仍只是 v148 的历史结果。
冻结入口：`build/tmp/xlir_launch_v161_evidence/results.json`。下一批直接推进
闭包控制块/回调尾部的常驻化和 IR 代码拥有合同，再接全局内部环及安全退役。

### v162：常驻闭包与真实双模块代码拥有边（整合仍失败）

普通闭包的 Create/Retain/Release/Ownership/调用作用域迁至 resident runtime，
物理结构只在私有 `xl6_closure.h` 定义。Typed IR 不得提前携带布局，Lowered
IR 必须选择 resident scoped lease V1；16 个变异均在 C/header 前拒绝。
动态 Box 不假定 invoke 和装箱 adapter 属于同一个模块：常驻记录分别持有
closure 与 adapter lease，常驻入口在最外层保护两个生成回调。环境释放、
Future closure payload Drop 和 XL6_Function 的 retain/release 返回真实常驻
地址，避免 PE thunk 回调地址落在生产者映像。按值 Function 的 invoke 包装和
线程句柄仍需独立迁移，不能把上述生命周期迁移说成所有回调路径已闭环。

先做原生快检，再复用预处理内容哈希缓存。冻结公开编译器 58 TU 全部复用，
13.750 秒；六个合同驱动共享 23 TU，20 复用/3 重编译，13.130 秒。随后一次
八门批次峰值三进程，4,018 项输入前后一致，六绿两红。新增闭包门的 O0/O2
各 50 双 DLL 生命周期、25 图根变化、10 万引用对及 150 次真实 OOM 拒绝通过；
五源程序四阶段共 20 次通过，重复输出稳定。原 frame/generator/finalizer/
public/publication 的分母保留并通过；完整 24 门并未因此视为已重跑。

本轮没有达到“红项单调减少”：新增自动闭包租约使模块内部环成为真实物理
结构，而生产退休尚不支持它。模块门原图计数断言失败；按实际新增一节点、
两边及一个内部租约修正测试后，仅重测模块的 O0/O2，两条均在等待实际卸载
时失败，证明不是修改计数就能通过。保留初次批次和其旧 host 的哈希副本，
不把定点复测冒充第二次完整批次。原对象门十模式全部断言退出，250 次目标
未完成；没有删模式、缩短事务、XFAIL 或放松 allocator/MEM_FREE 断言。

额外对原 C callback export 的原生宿主仅适配 XLIR 生命周期，保留其所有
断言和 1000 次 callback 替换；O0/O2 同源 XRT 通过。初始宿主仍使用旧的
XL_InitOptions，以及误重复链接已内嵌的 foreign-error TU，属于验证装配错误，
修正后才纳入有效结果；失败日志同时保留，不当作语言语义失败。

本轮未修改 XRT 源码或发布 DLL/CLI/VFS，私有 header/source 包装静态检查为
28/28。下一批处理类对象自动能力与内部环原子退休，并接通已接受 continuation
的运行时作用域；native Retain/COW/weak 及生成回调返回尾部不能漏入安全假设。
冻结入口：`build/tmp/xlir_launch_v162_evidence/results.json`。

### v163：continuation 共享租约、已接受作用域与一次九门验证

本批修复生产异步 transport：ticket 拥有实际共享 CodeLease，successor
无分配继承；run/release 由 resident CodeScope 包围，前驱回调和作用域结束
后才发布后继，最终 host drop 返回之前保留 queue 执行额度。关闭后原有
activation 可以恢复并创建子闭包，但不能把新 root admission 偷换为继承。
未给 Cgen 增加语义分支，未修改 XRT，也未启用不安全的模块环收集。

原生快检通过后只做一次冻结构建：公开驱动 58 TU，54 命中/4 重编译，
13.806 秒；六个合同驱动共享 23 TU，全部命中，7.231 秒。旧源码专项补齐
resident lease/closure 链接；原 progress 故障注入跳过新增 lease 分配，仍
命中原 worker-record/thread-record 两个真实失败点；根 lease OOM 独立覆盖。

`run_xlir_ownership_batch.ps1` 新增可选 `-ContinuationScopes`，与已有
publication/closure 选项一同追加专项，不改变原六门默认清单及分母。一次
九门 verification 峰值三进程、88.584 秒，4,026 项输入指纹前后一致：

- 原 frame 14/14、generator 3/3、finalizer、public 十程序及十轮恢复、
  publication 与 closure 全部通过。原 frame 中的 queue/handoff/executor/
  progress 同源 O0/O2 全数执行，不用新 happy-path 程序替换故障/并发断言。
- 新 continuation-scopes 原生 O0/O2 每条各 100 DLL 生命周期、350 子闭包、
  100 无分配后继 abort/recommit、25 四线程排空、100 根租约真实 OOM、
  100 容量回滚；检查生成回调返回尾部、重入退出、真实拥有边和内存平衡。
- 同一专项的实际语言/TCC O0/O2 每条各 100 Close/resume/closure/invoke/
  unmap 生命周期，Future 保有及抽离原生 retain 各 50，没有额外宿主 code
  guard；公开编译器重复 C/header/IR 一致。
- 原 module O0/O2 仍在实际退休等待处失败；原对象逃逸十模式均首事务
  exit 3，目标 250 次未完成。整批七通过、两失败；新增绿色没有使原红项
  减少，不能换成 XFAIL 或宣称对象拥有图已完成。

统一验证后只调整架构检查中失效的源码位置假设：环境字段和原子闭包 RC
查 resident 定义，生成侧必须调用版本化 ABI，禁止复制实现；另检查 ticket
实际租约边和回调/作用域/后继发布顺序。旧脚本按精确哈希保留，生产输入
未变，不为该只读检查调整重新跑整套重型批次。架构检查、28/28 私有包装
静态检查另列，未选旧专项的语法通过不冒充功能复测。

当前重点仍是类对象的自动代码拥有边与模块内部环的稳定快照/原子接管/
提交；native Retain/COW/weak 及所有生成回调的返回尾部必须有完整合同。
原 24 语言门、发布 CLI/DLL/VFS、Linux 和 legacy 退役本轮没有完成。
冻结入口：`build/tmp/xlir_launch_v163_evidence/results.json`。

### v164：构造期代码能力与复制语义必须分开验收

本轮实际修改 XRT、resident class storage ABI、Typed/Lowered XLIR 和机械
C emitter。类分配在字段/create 前准备代码租约，构造成功才提交析构；失败
发布的 self 可由原生别名/游标继续保有字段而不运行用户析构。隐式字段在
构造/析构体中也归一为确切 IR 接收者/字段身份，Cgen 不增加语义回查。

收敛过程与当前证据：

- 共享公开编译器 58 TU，57 缓存命中、1 实编，15.498 秒；最终七合同驱动
  共用 23 TU 全命中，6.272 秒。缓存键仍包括预处理内容、标志及工具链，
  新鲜链接并检查输入前后 SHA256；不是复用旧 release 语义冒充新编译器。
- 第一次十门集中验证 7/10，通过前后的 4,034 输入一致。两个测试装配
  遗漏集中修复：对象宿主补链接常驻 finalizer，发布门的提交人口纳入两处
  无析构 DynamicFields（3→5 提交，18→30 变异）。生产源码未继续变化。
- 再共享链接合同驱动，一次十门复验 8/10，84.962 秒，峰值三进程，输入
  仍 4,034 项一致。原 frame14/generator3/public10+recovery10 和全部新增门
  保留。原 module O0/O2 及 objects 十模式仍首事务失败；没有缩分母、XFAIL，
  也没有把新类专项变绿解释为原退休问题减少。
- 新 class-storage 门：四分配/四提交、两个 lifecycle 字段绑定、22 变异；
  O0/O2 每条 225 native DLL 生命周期和 100 OOM，175 实际 TCC 生命周期。
  覆盖 Retain/Clone/cursor、失败构造、无析构类、回调返回后实际卸载及内存。
- 源码邻接审计发现 finalizer 身份限制可能破坏普通类 copy/COW，立即追加
  五个既有复制程序的 15 阶段小批发现。三种派生复制的 run/module 共六项
  确认回归，两种自定义复制通过；深复制代码边也仍缺失。没有为了维持每轮
  更少失败的表面数字隐藏新增问题，当前批次仍属未完成的集成开发。
- 下一批应从 XRT/backing 的可复制代码能力合同解决 shallow/COW/deep 的
  统一接管，不能放宽唯一析构职责，不能在 Cgen 按源程序拼补丁。处理这一根因
  时继续保留本轮构造、析构、提前发布的固定专项和原模块/对象退休验收。

官方同步后 XRT 2980 / xruntime 3427 API。新 construction 与原 publication/
lifetime 在 GCC/TCC 模块化、单头四路径通过，成功标记短名误判单独按实际
日志精确审计；不为日志文本重编译。发布二进制未改、没有关闭 Defender、
没有自动切换后端；旧完整语言批次未重跑。
冻结入口：`build/tmp/xlir_launch_v164_evidence/results.json`，复制追加发现与
原十门各自保留报告，不将其混合成一个虚假完成百分比。

### v165：按共同根因修复复制，再做一次十一门验证

本轮处理 v164 暴露的普通类 copy/COW 回归、deep-clone 代码边遗漏与相邻
空对象合并的能力丢失。三者统一由 XRT/backing 的可复制 lifetime 合同解决：
新 backing 保有同一个不可变能力节点，真实析构身份保持独立且不可复制。
常驻 ClassRuntime ABI 转移真实 CodeLease，没有增加 Cgen 语义例外。

发现阶段先用窄 XRT 测试和五个既有语言程序确认修复，并完成新真实 DLL/TCC
复制生命周期专项。随后统一补入关闭后调用生成哈希的检查，冻结源码再做
一次完整 verification；早期 discovery 的宿主版本不冒充最终哈希验证版本。
本轮没有为了日志标记或小型静态检查重复跑重型完整批次。

- 共享公开编译器 58 TU，5 缓存命中、53 重编译，36.551 秒；本轮新 XRT
  声明改变预处理输入，不能误用旧对象。七个合同驱动共享 23 TU，1 命中、
  22 重编译，14.616 秒；新鲜链接，沿用工具链/预处理/标志键和前后输入核验。
- 可选 ClassCopyStorage 门追加为第十一门，原门与分母保留。一次 verification
  89.339 秒，峰值三进程，4,040 输入一致：9 通过、2 失败、0 未运行。
  原 frame14/generator3/public10+recovery10 不缩减。原模块两通道和对象
  十模式仍失败，不改成 XFAIL，也不拿新增专项数稀释原失败比例。
- 五个既有 copy/clone 程序不改源码，emit/repeat/run/module 共 20 阶段
  通过，原六个运行失败消除。新 native O0/O2 每条 175 真实 DLL 生命周期，
  actual TCC O0/O2 每条 150，其中 100 次 Close 后调用生成哈希；检查源
  对象先销毁、原生别名/游标、双模块空合并、多线程最后释放与实际卸载。
- 旧构造专项继续执行 native 每条 225 / TCC 每条 175；新增独立 lifetime
  分配使真实 Create OOM 点从 4 增到 5，每条从 100 增到 125 拒绝。四个
  IR 分配/提交、22 变异、发布五提交/30 变异保留，未修改语言允许复制的策略。
- XRT lifetime-copy 与原 construction/publication/finalizer-lifetime 共
  四套，在 GCC/TCC 模块化及单头四路径统一通过，1,918 输入一致；成功标记
  按实际 compiler 参数匹配。官方同步为 XRT 2983 / xruntime 3430 API，
  447 自有。静态架构、28/28 包装与快照检查单列，不声称发布或 Linux 已验收。

复制根因已关闭，但本轮仍是未完成的集成开发检查点。下一批集中处理模块
内部环的快照/接管/提交闭环，不把单次 ownership inspection 或重复计数
当作并发回收安全证明。默认后端、发布二进制、Defender 未改，完整旧语言
批次未重跑。冻结入口：`build/tmp/xlir_launch_v165_evidence/results.json`。

### v166：准入协议批量接入，性能发现触发必要的集中复验

本轮把全图静止的前置能力落实到 XRT 生产代码：实际引用 CAS 与 85 个 Value
完整操作参与 mutation/freeze 协作域，保留普通宿主强引用和原生 weak 提升
API，不新增 Cgen 语义补丁。此处仍不是完整收集器或模块自动退休完成。

- 第一组修改后，GCC/TCC 各 35 个 XRT 程序实际 exit 0；父 stdout 缓冲使
  四条标记粘连，先保留原日志做只读精确审计，不为标记问题重新编译。
  首次十一门集中验证 89.502 秒、9/2、4,040 项输入一致。
- 随后实际公开 RC 微基准发现单一全局锁造成约 97.75 倍的四线程独立计数
  退化。这个发现不能靠绿色功能测试掩盖：将准入改为 64 个线程散列分片，
  收集方才取得统一协调锁；同时补齐直接 Hash/ScalarEqual 回调外层边界。
- 最终窄测试加入 65 同时 reader 的强制碰撞和四收集方的竞争；保留原 weak、
  容器、字段析构、真实 OOM 等检查，再按这一组生产变更统一完整复验。
  新哈希测试修正包含 TypeId 的预期值，计数比较改为原 API 的 unsigned
  类型；发现日志与旧测试 bytes 保留，没有改生产哈希语义或降低编译警告。
- 最终 XRT 12 套、35 程序/编译器，GCC 45.542 秒、TCC 11.323 秒，全部
  精确执行且完成，1,983 输入不变。源码和单头路径均覆盖，不称 Linux 已测。
- 最终公开驱动 58 TU 中 57 命中/1 重编，22.710 秒；七合同驱动共用 23
  TU 全命中，6.910 秒。最终十一门 88.148 秒、三进程峰值、4,040 输入不变：
  9 通过、2 失败、0 未运行。旧 module 两通道和 objects 十模式仍红，未减少
  原分母；五个既有复制程序二十阶段及原构造/发布/闭包/continuation 门均绿。
- 两次完整验证来自不同生产输入，不能写成一次验证，也不能把两次通过数
  累加。分片前十个会变化的源码/快照输入按原 SHA 保存，最终结论只用最终
  源码和报告；探索时的微基准、标记误判与测试修正也分别保留。
- 五次交替 GCC O2 微基准表明最终串行 RC 仍为 v165 的 3.63 倍、独立
  四线程为 3.45 倍；这不是性能验收通过。批量流程不能保证每轮只发现更少
  问题，新增性能或安全问题必须显式记录并按共同根因处理。

后续继续以外层参与方 + 原子接管/提交为一个完整根因批次，不能在未参与的
Future/Cancel、native/callable、生成存储或回调旁直接启用 inspection 回收。
默认后端、发布二进制和 Defender 未改；24 门旧语言总验收本轮未重跑。
冻结入口：`build/tmp/xlir_launch_v166_evidence/results.json`。

### v167：异步拥有转换按共同入口修复，一次集中复验

本轮围绕同一并发缺口，统一接通 Future/Promise、Cancel 和共同的 combine /
continuation 工厂；保留原 body 和终态/失败受理语义，没有给 Cgen 新增例外。
28 个公开包装、3 个内部 waiter 边界和 2 个共同工厂使用同一私有返回宏，
枚举错误必须显式选 ERROR，不能以零值伪装 READY。阻塞等待/协程 park 的
完整 body 不持有长期 mutation，原来实际保有的 Future 引用仍计入外部根。

- 新测试在冻结 v166 实现上实际发现首个终态发布事务穿过 freeze；它单独
  记录为缺口证明，不算绿色验收或 XFAIL。当前实现的窄 GCC 模块化/单头
  验证通过后，源码不再修改，统一构建和复验，不反复重跑完整批次。
- 词法平衡审计保留 33 个原签名/操作体，四个异步文件其余 bytes 不变；
  原分片/RC/Value 与协程 Await 源码未变。新专项每程序 100 冻结转换、
  130 在途边界、20 并发注销和 10 真实 Pending 等待，检查图/内存平衡。
- XRT 23 套、58 个程序/编译器，保留上一轮 35 个程序，新增原并发/OOM/
  协程/bridge 回归及本轮专项。GCC 65.731 秒、TCC 15.611 秒均通过，1,987
  项输入一致。单头、源码结果不扩张成 Linux、完整拥有图或性能验收结论。
- 公开驱动共用 58 TU，5 命中、53 实编、38.378 秒；七合同共用 23 TU，
  1 命中、22 实编、16.041 秒。预处理输入变化时按完整缓存键重编，不以
  旧对象或 release 语义冒充当前构建。
- 一次十一门 verification 89.324 秒、峰值三进程、4,040 输入一致：9 通过、
  2 失败、0 未运行。原 frame14/generator3/public10+recovery10、copy20
  和各真实构造/回调/释放分母不变。module 两通道及 objects 十模式仍红，
  对象目标 250 事务仍未完成；底层新增专项通过不稀释这两个原失败门。

下一组工作是直接代码租约/活动作用域、生成存储以及剩余外层拥有边的参与
能力，之后才能实现安全的候选接管/提交和析构复活处理。v166 的引用性能
风险仍未验收，本轮没有报告新的倍率；发布/默认后端/Defender 没有变动，
24 门完整语言总验收未重跑。
冻结入口：`build/tmp/xlir_launch_v167_evidence/results.json`。

### v168：常驻拥有转换与队列分段准入，一次集中验证

本轮把租约、闭包/类工厂和 continuation 队列/transport 登记作为一组修复。
31 个原操作体保留；另明确重写 dispatcher 的取票/等待边界与物理退休尾部。
准入先于内部锁，等待/正常 run 不长期占用 mutation，仍保留真实活动代码根。
同一新检查在冻结 v167 上分别检出租约、工厂和队列缺口；修复后的 GCC O0/O2、
源码 TCC/UCRT 均通过，不改原失败断言、不使用 XFAIL、不强制原生根登记。

新增门逐程序覆盖 100 lease/stack/closure 冻结转换、50 回调尾部、50 真实
allocator-held 工厂、10 Pending invocation、100 queue/transport 冻结转换、
10 Pending dispatch 和 10 退休回调 Close 后子闭包逃逸；实际计数、图、credit
与逐事务 allocator 均检查。覆盖依旧不等于任意用户数据已能安全冻结。

公开构建 58 TU（51 hit/7 miss，13.280 秒），七合同共享 23 TU 全 hit（6.570
秒）。冻结生产输入后一次十二门 verification 92.396 秒，峰值三进程，4,043
输入一致：10 绿/2 红/0 未运行。原十一门仍是 9/2，加一新门，原 frame14、
generator3、public10+recovery10、copy20 等分母保留。module 两通道与 objects
十模式未关闭，250 目标事务仍未完成，不能通过新增绿色门稀释其状态。

XRT/bundled implementation 与 IR/Cgen 源码未改；上一轮 XRT 58/58 是历史
证据，不记本轮重跑。新私有头落实四个 VFS 清单与本地 staging，尚未生成
发布 VFS/CLI/DLL。性能、全部生成存储/用户数据参与合同、安全 claim/commit、
析构复活、完整 24 语言门、Linux/默认后端仍未验收。
质量检查另外发现能力文档矩阵过期，官方生成器已更新；延期清单未变。
此派生文档不改变执行输入，只重做质量检查，未增加第二次完整验证批次。
冻结入口：`build/tmp/xlir_launch_v168_evidence/results.json`。

### v169：cell 常驻 ABI 成组修复与一次十三门复验

先集中迁移 cell 工厂、初始化、拥有型交换、引用及释放/trace，IR 记录可验证
ABI 版本；Cgen 不再发射物理生命周期副本。分配失败、并发 reader、恢复程序
和捕获图沿用原语言 fixture；旧测试接入当前源码运行时，不把 release 验证
冒充为新实现验证。定向发现阶段修正旧 helper 拼写检查、精确图计数及闭包
提前拒绝合同，同时保留原 inspection 回滚、100 轮/7 模式和真实 OOM 分母。

新 cell 门为 5 组 × GCC O0/O2/TCC 共 15 通道，全部通过；原拥有图十二门
不删减。一轮 verification 13 门 11 绿/2 红/0 未运行，126.964 秒，峰值 3，
4048 输入不变。原十二门仍 10/2，原模块/对象失败状态不被新绿色门稀释。
共享构建公开 59 TU（58 hit/1 miss，13.873 秒）、11 合同共用 23 TU
（22 hit/1 miss，8.429 秒）。完整批次本轮只跑一次，修复期间只跑定向发现。

resident 每程序 400 生命周期、450 真实 OOM、250 冻结转换、50 析构尾部；
原分配场景每通道 260 基线/1800 真实 OOM，并发场景 800000 引用操作/
48240 reader/30 最后拥有者竞争，capture 700 生成图，均精确内存平衡。
阶段、分配、恢复、捕获 IR 反例与 C/header/JSON 确定性另行验证。

8 项质量检查通过，能力生成器区分公共静态内联与外部符号，保持未知引用
拒绝及空延期清单。未重建 release/VFS，未改变默认后端/Defender，XRT
实现未改且历史 58/58 没有重跑。外层 frame/global 槽和其余节点参与证明、
安全接管/提交/析构复活、原两类失败、性能及完整语言/发布验收继续开放。
冻结入口：`build/tmp/xlir_launch_v169_evidence/results.json`。

### v170：有序槽转移成组迁移，受影响门成组修复复验

同一根因批次把共享槽与显式 frame 转移统一为已验证的有序写入计划；不在
同步、async、generator 三处分别增加语义补丁。冻结 v169 producer 在同一
新测试中真实复现共享写入越过冻结，当前 producer 和常驻 ABI 三通道通过。
每程序保留 100 生成全局冻结、100 物理 handoff 冻结、100 真实 OOM 开关下
无分配提交及 20000 并发 handoff，并检查精确图与 allocator。

公开共享构建 60 TU（59 hit/1 miss，13.878 秒），12 合同共享 23 TU 全 hit
（7.601 秒）。一次十四门完整 verification 为 100.285 秒、8/6/0；收集完
才修复测试 TCC 宿主遗漏的新 getter 和 publication 原生链接遗漏，共两文件。
六个失败门和一个声明依赖受影响的 public 门组成一次七门修复复验，80.645
秒、5/2/0；两批都峰值三进程、4056 输入前后一致，生产源码和共享构建未改。

其他七个绿色门的全部声明依赖仍与其报告 SHA 一致，结果保留，不再启动
第二次十四门全跑。最终合并为 12 通过/2 失败，原十三门仍 11/2。合并报告
逐项校验 3883 个当前输入版本，保留两批完整报告及两个接线修复前的准确
历史文件版本；不同批次不累加通过数，也不伪称同一次完整执行。

原 frame14、generator3、public10+recovery10、cell15、copy20 和全部对象
模式保留；module 两通道仍是退休期限，objects 十模式仍是八期限/两自动
租约断言，250 目标事务尚未完成。8 项质量检查通过，XRT/发布实现未改变，
历史 XRT 专项没有重跑。整图节点参与、frame/core 生命周期、安全接管/提交、
析构复活、性能与完整语言/发布验收继续开放。
合并入口：`build/tmp/xlir_launch_v170_ownership_composite/results.json`；
冻结入口：`build/tmp/xlir_launch_v170_evidence/results.json`。

### v171：generator 生命周期整组迁移与一次十五门验证

将 generator 控制、frame 生灭、当前项、关闭和返回尾部整体移出 Cgen，
统一 IR 新增可验证的常驻 ABI，原 continuation/cleanup 状态仍由 IR 决定。
同一新测试在冻结 v170 producer 上真实检出生成 core 越过 freeze；新实现
具有真实代码租约、独立物理 frame busy 拒绝和 resident callback 返回尾部。

发现阶段先跑旧 generator 拥有图和新 resident 三通道，再集中发现全部
24 场景 × 3 编译通道。修正新测试入口签名、私有包含文件的脚本表达式，
并补强首例启用内存调试和非零分配计数的断言；没有调整原语言 fixture、
运行断言或真实 OOM 分母。当前 XRT 默认已开启调试，新增断言用于保证
测试前置条件，不是发现过追踪关闭故障；发现与最终验收记录分开保存。

公开共享构建 61 TU（54 hit/7 miss，20.576 秒），19 合同共享 23 TU
（20 hit/3 miss，15.677 秒）。生产实现没有因测试修正变化，最终复核构建
每项输入仍匹配 SHA 后直接复用；不同目录名字不作为重新编译的理由。
源码稳定后只跑一次十五门 verification：207.690 秒、峰值三进程，4065
输入前后一致，13 绿/2 红/0 未运行。原十四门仍 12/2，原硬门未关闭。

新增 72 通道包括新 resident 3 通道，以及全部原 payload 15 场景、close
5 场景、拥有项 cleanup、loan 优化/基线。每 resident 程序 200 生成冻结、
200 活动控制/frame 拒绝、200 真实 OOM、100 armed-OOM 转移、100 实际
DLL 生命周期与 Close 后子租约，3400 分配/零 live delta。关闭错误顺序、
重复关闭、自然/提前退出、借用与原始分配失败扫描不使用 smoke 缩减。

22 个原 XL 文件和 11 个 runtime 主体与 v170 完全一致，8 个 host 只加
私有 include 参数。Cgen 源码净减少 9880 字节，不声称二进制缩小。8 项
质量检查通过；仍不代表任意 native/task/transport 或 async frame 已有
全图合同，安全候选接管、模块环退休、析构复活、性能与完整语言验收继续。
冻结入口：`build/tmp/xlir_launch_v171_evidence/results.json`。

### v172：async 物理存储和 queue 退休分相，一次十六门验证

把 async machine/pool 的 frame 物理 Allocate/Free 收口到常驻 ABI，并由
统一 IR 显式记录与验证版本。现有 ticket 就是物理 frame 节点，不额外
添加计数、根或 trace 身份。queue 在 mutation 中 detach 后退出协调区
执行 release，保留真实代码与执行额度，最终 Free 单独协调，之后才发布
successor；不能由此认证任意原生子节点或普通 pool 的回调代码保活。

发现阶段完整保留两组新门失败：非法 export-future fixture 改为合法
void export；另一个消费者的短出队 mutation 改为有界等待后再检查冻结，
没有减少最终循环。最终新门每通道 200 个分配/释放冻结、100 个生成
launch/200 个 Watch 退休、100 个阻塞清理 handoff、100 真 OOM 和 100
armed-OOM free，3600 真分配且零 live delta。冻结 v171 producer 和 queue
分别在当前测试中实际 exit 3，确切失败原因及旧源码保持可复查。

公开构建 62 TU（52 hit/10 miss，21.198 秒），20 合同共享 23 TU（20 hit/
3 miss，16.461 秒）。修正测试后确认共享构建仍匹配当前输入，直接复用。
只进行一次完整十六门 verification：207.611 秒、峰值 3，4072 输入哈希
前后一致，14 绿/2 红/0 未运行；原十五门仍 13/2。全部 72 generator 通道
及原 frame14、generator3、public10+recovery10、cell15、slot3、copy20 保留。
module 两通道仍退休期限；objects 十模式仍首事务八期限/两自动租约断言。
两组原硬门没有因为新增专项通过而被关闭，250 目标事务未完成。

2709 个已有测试文件与 v171 完全一致，仅一个宿主绑定新 getter，以及
resident 清理阶段的 100 次观察改为“可冻结、frame 拒绝、真实代码/额度
仍存活”；剩余循环与主体保持。生产 10 修改/2 新增，Cgen 文本 +643 字节，
不作 DLL 体积结论。8 项质量检查通过，XRT/发布实现及默认后端没有改变。
发现、负例与 verification 分开保存，不合并重复通过数或缩减原失败分母。
冻结入口：`build/tmp/xlir_launch_v172_evidence/results.json`。

### v173：task 生命周期整体迁移，分批发现后一次十七门复验

本批统一处理普通 async pool / task.spawn 的 frame、代码租约、池准入与
收尾、结果通知桥，不在生成入口分别堆叠生命周期补丁。IR 固定存储版本
和精确 capture trace；常驻 task ABI 维护真实 job/submitter/Watch 引用、
独立并发锁和代码返回尾部。XRT 通过新显式协作 Watch API 分相通知，旧
回调继续保守保护，不能由 trace 的存在推断任意代码可并发检查。

发现阶段先完整跑新 task 十五通道，再跑原十六门，分别收集到六个通道
失败与四个失败门。统一修复 pool 最小声明缺失、通知 mutation 跨等待、
并发 frame 引用写，以及新测试错误检查已退休 frame 的假设；冻结发现
阶段 3948 个输入版本，不覆盖红灯日志。最终测试检查仍被拥有的 Future
和 pool join，保留完整循环，不借新测试通过关闭原有模块/对象失败。

最终共享公开 63 TU（5/58 hit/miss，37.981 秒）、24 合同共享 23 TU
（1/22 hit/miss，19.364 秒）。XRT 声明变化导致缓存失效，按实际依赖重编。
源码稳定后仅一次完整十七门 verification：215.924 秒、峰值 3，4080
输入前后一致，15 绿/2 红/0 未运行；原十六门仍 14/2，全部原分母保留。
新增 task15、原 generator72/async3 等全部通过。旧 XRT 的保守 Watch
通过 baseline-only API 适配在当前测试中 O0/O2 实际检出冻结失败，单独
保存为负例，不记绿色通道，也不计为第二次完整 verification。

每 resident 程序 280 真提交前故障、2000 即时退休竞争、26646 真分配且
零 live delta；另保留排队/执行/清理/通知窗口、取消、回滚和真实 DLL
Close 后执行/卸载。原 module pool、outcome、callback、future-only
循环不缩减，25 个旧通知白盒案例仅迁入常驻测试。XRT 原测试未改，本轮
23 组专项 GCC/TCC 各精确 58/58；8 项质量检查通过。

Cgen 文本比 v172 净少 6380 字节，常驻实现新增，不据此推断 DLL 体积。
module 两通道退休期限和 objects 十模式首事务失败仍保留；全图参与、
候选安全接管/析构复活、性能、完整语言/Linux/发布/默认后端继续验收。
XRT 单头和 bundled 源码已同步，但没有重建发布 CLI/DLL/VFS。
冻结入口：`build/tmp/xlir_launch_v173_evidence/results.json`。

### v174：Future 回调整体收口，十八门统一复验与三条受影响通道补验

Future then/catch/finally 共用常驻物理 frame、Watch 登记、真实拥有边和
通知/清理返回尾部，IR 显式选择存储 ABI。语言捕获、清理和结果优先级仍由
已验证计划决定，不增加 AST 追溯或复制状态管理。Close 后子 continuation
只能继承本线程同一拥有者的已受理能力，不重开根准入，task 规则不变。

先一次十八门 discovery，收集 15 通过/3 失败；新增门的错误是新测试漏计
九个 task、重复计算 Promise/Future 外根和模块清理入口错误。成组修正后
新专项九通道通过，再一次完整十八门 verification：208.356 秒、峰值 3，
4089 输入前后一致，16 通过/2 原失败/0 未运行。末尾审阅 TCC 日志发现新
测试计时函数无声明，改用已声明且溢出安全的经过时长计时，仅补跑三个
resident 通道；不重复整套，不把旧整树输入哈希冒称成最终测试修正后的哈希。
原始红灯、测试版本、完整批次和补验分别保存，补验只替代受影响的三个通道。

三原 fixture 的 15 Future/10 task、75 非法存储变异全部验证；resident
每程序 1000 竞争、100 真 DLL 卸载、80 真准备故障、11600 分配零 live delta，
另保留 pending/ready 四终态、取消、活动拒绝和失败清理。真实 DLL 父/子回调
与子闭包均实际调用 DLL 代码，最后常驻尾部卸载页。生成图精确 11/13/2、
100 事务及原十个语言 continuation 位点通过。冻结旧 producer 的 O0/O2
pending 图真实失败作为单独负例，不增加绿色分母。

公开构建 64 TU（57/7 hit/miss，20.412 秒）复用；最终 25 合同共享 23 TU
全命中，11.011 秒。2719 旧测试文件逐字一致，仅 one-shot 宿主绑定新 getter。
原 generator72/task15/async3 等原分母全部保留；module 两期限与 objects
十模式首事务失败依旧开放。Cgen 净少 1196 字节，不推断 DLL 体积。
8 项质量通过，矩阵引用更新；XRT/bundled/发布实现未变，未重跑历史 XRT58/58
或完整语言/Linux/发布验收。下一批接入安全候选接管、模块环退休与析构复活。
冻结入口：`build/tmp/xlir_launch_v174_evidence/results.json`。

### v175：接管事务集中验证，测试兼容修正仅补受影响组

新增同一物理图算法的快照/准入、常驻真实 pin/claim/finalize/revalidate/clear
事务引擎、普通 XRT Value/backing 接管及宿主 CodeLease release 外层尾部。
先集中验证新功能，修正新测试声明/严格编译和 GCC 完整依赖；原直接宿主
release 的忙态断言迁移为验证可冻结，保留该专项全部轮数和其他忙态断言。
冻结旧 lease 的 GCC O0/O2 负例实际失败，新 producer 的 freeze+真实 worker
join 通过。没有改模块/对象失败断言，也没有把未知析构策略视作可回收。

源码稳定后一次十九门 verification：217.969 秒、峰值3，4095 输入哈希一致，
17 通过/2 原失败/0 未运行。原十八门仍16/2；新增九通道验证400真实环、1600
四类Value/COW事务、40000并发隔离弱提升、真实准备/重验故障及释放平衡。
2725 旧测试文件字节不变，另一文件只迁移上述宿主尾部边界；原语言分母不减。

XRT 一次25组发现GCC62/62、TCC新测试初始化内部编译失败，保留原日志与版本。
只更换结构体数组等价初始化，不改生产源/测试断言；补GCC受影响两个程序与
TCC未完成组，结果2/2、62/62。没有重跑GCC另外60项，也没有第二次十九门
完整验证。8项质量通过，60私有头映射/56源码放置，发布二进制/默认后端不变。

本轮是接管机制与普通Value接入，不是模块环验收；原模块两通道、对象十模式
首事务失败仍开放，下一批接入真实闭包/类/模块及其语义析构、代码尾部。
冻结入口：`build/tmp/xlir_launch_v175_evidence/results.json`。

### v176：真实模块门转绿，格式问题只补受影响通道

把 closure/cell/code lease 与真实模块接入 v175 事务引擎；IR 给出 V2 不可变
捕获布局和独立 checked retirement 描述符。保留 V1 调用前缀，不猜 native
策略，不修改真实聚合租约计数。原模块 50 生命周期/通道硬门首次全部通过。

首轮十九门 discovery 一次收集到旧构造入口、原生负例入口和白盒宿主链接
遗漏；集中修复，并增加五通道 actual-module/closure-layout 专项。随后一次
二十门 verification 为 18/2/0，214.71 秒、峰值 3、4098 输入哈希一致。
剩下两个红灯分别是原对象门，以及 GCC 对一行生成 checked-cast 代码的
misleading-indentation 告警。仅拆行，不改变 CFG/调用/IR/header，并对原
700 capture 图的 GCC O0/O2/TCC 三通道补验，全部通过。不重跑其他十二个
cell 通道或十九门；证据合并为 19 通过/1 原对象门开放，不称第二次完整验证。

新增布局三通道各 400 环、400 隔离提升、96 准备/提交 probes（14 真故障）、
496 实际 owner 释放，另 500 Value bridge（200 真故障）。新增模块两通道
各 221 真 TCC 生命周期，活动线程检查在 Trace 前拒绝、旧合同拒绝、Finish
失败重试、忙态 abort、96 allocation probes（21 真故障），保持零 live/MEM_FREE。
原公开 raw callback 宿主 O0/O2 各 1000 次替换/跨 unit 调用也通过。

2725 原测试字节不变，四文件为明确的 ABI/接线迁移，两个原始模块/对象
主验收源码和分母未变。工程八项通过；构建缓存使最终公开 65 TU 只重编一项
（14.690 秒），25 合同共用 23 TU 只重编一项（12.097 秒）。主验证 producer、
拆行前后输入版本、主验证及补验独立保存，不把当前源码哈希冒称旧整批哈希；
早期探索日志和可用版本另存，不声称每个探索阶段的全部输入都已提前冻结。
相邻 XRT 的独立配置/生成头变化未同步，实际 bundled 快照和发布二进制未变。

下一批优先真实类的非终态析构/不可重复职责、native callable 的物理协议及
原对象门。标准库仍最低要求；语言拥有、外部根、代码驻留和析构顺序必须解决。
冻结入口：`build/tmp/xlir_launch_v176_evidence/results.json`。

### v177：对象十模式收敛，扩充覆盖不压缩分母

同一批处理 Class V2 的图/身份/代码生命周期、resident callable 的显式策略与
活动准入、实际 receiver 的唯一析构职责、BOOL 失败结果和不可变 Error/Cause。
发现批次一次收齐 21 门：20/1；对象八模式已经通过，另外两种仍被旧聚合
租约断言阻止。把观测迁移为完整已准入图中的外部可达性，内部真实代码引用
继续计数，恰好扣除一个仍存在的调用方 handle，不缩短循环或屏蔽失败。

原 10×25 guarded 全保留，再追加 10×25 automatic（无宿主保护租约）；同时
追加析构错误/无诊断错误/原错误优先四模式，每个宿主优化等级 4×25，及
resident 450 图×三通道。语义层失败全部有明确 bool，不能以 NULL 错误指针
推断成功；物理职责跨 Abort 不复活，已发布 child 使完整重验恢复外部根。

唯一完整 verification：21 门、20/1/0、216.356 秒、峰值 3，4101 输入前后
相同。失败仅是新增 O0 白盒宿主的 whole-program/编译服务依赖配置；原两组
各 250 次及 O2 四模式 100 次已经通过。定向补齐相同源码/生成 C/header/IR
的 O0 四模式 100 次后有效为 21 门通过；不再重跑另外 20 门或已通过通道。
保留最初依赖缺失、补验中缺少编译服务宏的日志，以及最后完整依赖的通过结果。
最后工具修复只影响 O0 链接：取消错误的 whole-program 内部化并匹配源码构建
的 EXE 编译服务定义。生产编译器/运行时未因该补验改变。

附加反序列化出生路径检查保留四个原程序；三个白盒宿主一度被历史 build/tmp
同名 C 文件遮蔽，改为绝对生成文件路径后，原 C/header/IR 字节一致，最终
GCC O0/O2 各 25 次/程序，共 200 次源级断言与零 live delta。原 raw callback
宿主也在 O0/O2 各保留 1000 次替换、unit/reinit、mixed/void 回调验收。

XRT 授权修改后统一复跑原 25 组，GCC/TCC 各 62/62，1991 输入前后相同；
快照同步和工程八项检查通过，独立 xllm 配置保持。2728 原测试字节不变，三个
迁移文件有明确审计，新增两个文件；原 generator72/task15/cell15 等分母不减。
发布 CLI/DLL/VFS 及默认后端不动，不把有效门全绿称为统一 IR 总目标完成。

下一批重点是完整 Future/task/native 传递拥有图与语言整体验收。标准库仍按
最低要求；整图准入、弱/raw/线程转换和代码最终卸载不能靠模糊白名单补齐。
冻结入口：`build/tmp/xlir_launch_v177_evidence/results.json`。

## v178：Future 与任务结果生命周期合同

先以原 v177 证据哈希为锚，保存 5940 份基线文本输入，再集中实现 Future/Promise
单节点接管、常驻结果策略、task 原生受理前选型及结果错配拒绝。新用例先发现
测试 API 用法、异步等待和模块资源追踪边界的问题；不修改语言的 async 返回
类型限制来迁就测试，嵌套结果通过合法的 native var 边界构造。

生产源码稳定后只跑一次完整 22 门 verification：16/6/0，221.215 秒，峰值 3，
4106 输入哈希前后一致。集中修正六项测试集成问题，再只补受影响门及相邻旧
capture/Future/aggregate 用例；结果以“完整批次 + 定向补验”合并，不重复完整
批次、不减少分母。六门补验中的 107 份既有 C/header/IR 均未改变，生产源码
也未在补验期间改动。追加检查暴露的历史脚本依赖遗漏保留失败证据再修正。

有效主门 22/22；GCC/TCC 各 66/66 XRT 执行，原 62 项保留；三个配置的新增
Future 物理计划、宿主 O0/O2 共 300 次真实 TCC 生命周期，以及原 Future 和
aggregate 固定分母全部通过。工程八项通过，发布物和默认后端保持不变。

本轮实际证明的是“结果合同完整 + 资源已关闭排空后的模块回收”，不是任意
活动任务图的自动收集。下一批需为退休前资源准备建立明确合同，继续认证
pending Watch/frame/native；不能靠提前清空全局或猜测 callback Trace 绕过。
冻结入口：`build/tmp/xlir_launch_v178_evidence/results.json`。

## v179：模块资源准备与活跃生产者负对照

以 v178 证据哈希为锚保存 5950 份基线文本。统一 IR 改为 PREPARED_GRAPH_V2，
生成共享资源准备程序，宿主在图检查前持真实 preparation 引用、解锁调用并
保护重入。V1 布局保留，畸形 V2 精确拒绝；不能提前清空 global 来消除内部环。

初次编译发现共享 ABI 声明中的 enum 逗号需要变参字符串化宏，集中修复后
重新构建；没有恢复手写第二份布局。新夹具曾在普通函数中使用非法 await，
改为语言本来允许的顶层 await，保留语言限制。负图检查的诊断和 worker 尾部
导致的合法 Freeze BUSY 分别正确消费/重试，不把它们当作析构失败或忽略边界。

首个完整批次 23/23，255.925 秒、峰值 3，4110 输入相同。随后审查活跃任务时
追加真实 TCC 负对照：任务需要调用方后续信号，阻塞式排空却使 Destroy 无法
返回。反例在解除控制事件并完成真实回收后仍明确失败，保留日志和当时版本。
因此不能仅凭已有全绿收口，也不能把该问题说成测试配置错误。

成组修正：新增常驻非阻塞 TryQuiesce，关闭受理后用既有 XRT 严格 idle 合同
探测已受理任务及 finalizer 尾部，BUSY 保留资源并允许后续驱动重试；保持旧
显式同步清理的等待行为。生成代码共享资源顺序，只按固定调用 ABI 选择模式。
新 gate 保留原 16 模式，再加外部信号依赖和自身退役，真实源宿主 O0/O2 共
900 生命周期、2700 次内部析构、MEM_FREE 及精确内存平衡全部通过。

这是影响公共任务准备入口和生成代码的实质修复，故必要地执行第二次完整
23 门批次，而非声称只有一次完整验证。最终 23/23、0 未运行、259.552 秒、
峰值 3，4110 输入哈希为 9CADD8C87C38E5AA924CCF50F31897269C3149EFA6D5AAB2A013ED6FD1A43C56，
验证期间不修改源码。新旧全部门保留，两个版本及新增负对照独立归档。

末尾工程检查发现能力矩阵因既有 XRT API 的新增引用而过期，只重生成矩阵并
补工程检查；延期 API 清单、生产源码及生成 C 不变。最终公开构建 65 TU：
49/16 hit/miss、17.861 秒；26 合同共享 23 TU：22/1、13.692 秒。XRT 本轮
未修改，原 GCC/TCC 各 66/66 在逐输入/日志哈希核对后复用，不计为本轮新跑。
三个旧测试有明确合同或符号接线迁移，2734 个不变、新增三个，不减少原分母。

后续是 pending Watch/frame/native 全图、其他必要资源合同和完整语言验收；
不以自动资源准备替代这些义务，不动发布物或默认后端。冻结入口：
`build/tmp/xlir_launch_v179_evidence/results.json`。

## v180：封闭捕获布局与保留的普通释放失败门

本批任务回调/Future 回调改为同一个常驻 capture 布局执行器，Lowered storage
使用各自 V2，原 V1 表保持兼容但旧 IR 值明确拒绝。捕获类型、偏移、宽度来自
已验证 ABI；生成代码不再复制这些帧的 Drop/Trace 或逐字段 cleanup。闭包内部
环境和其他 async frame 的进一步统一仍有独立义务，不能用本次布局替代全图。

先集中发现、实现并补永久负例，再进行一次完整 discovery：23 门中 21 通过、
2 失败、0 未运行，273.600 秒、峰值 3，4113 个输入哈希前后一致。随后仅补：

- 白盒模块宿主的 capture.o 重复链接排除，原 O0/O2 实际 TCC 两条通道通过。
- Future callback 原九条运行通道全部保留且通过；新增默认布局隔离与独立普通
  class/closure release 反例在 GCC O0/O2/TCC 仍失败。有效主门为 **22/23**。
- 新测试的 values-only 只用于定位：三个配置的数据/错误/反序清理、分配失败与
  回滚通过；默认硬门从未使用这个参数，也不据此关闭跨线程隔离问题。

确切缺口在常规末次释放：XRT Value 的 mutation 包住语义析构，closure release
又可能增加外层 mutation。独立最小反例不调用 capture/task/Watch，且在失败报告
前真实汇合线程、完成唯一析构及零 live delta。下一批须从普通释放协议统一修复，
不能仅为新测试对象增加特例，也不能丢掉 pending 回调/finally 的语义职责。

公开构建为 66 TU，60/6 cache hit/miss、21.261 秒；26 个合同共享 23 TU，
23/0、12.307 秒。工程八项通过，64 个私有头映射及 60 个源码放置检查通过。
同一标准库 Future fixture 的 C 从 271562 减到 259842 字节（UTF-8/LF）；这不是
二进制或全项目体积测量。标准库本身未扩张，XRT/发布 CLI/DLL/VFS 未改。

完整发现批次、必要的链接补验、仍失败的硬门及只用于定位的运行分别冻结在
`build/tmp/xlir_launch_v180_evidence/results.json`。这是开发检查点，长期目标未完成。

## v181：普通终态释放隔离，一次发现与一次统一验证

从普通 Value/backing、closure/cell、拥有式游标和已知 callable/Future 包装的
实际释放路径修复，不在 capture Cgen 中增加特例。短 scope 只完成引用归零/
节点隔离，已认证语义析构和子对象释放在该 scope 外运行，实际 receiver、字段、
弱引用、代码和 context/lifetime 尾部保留。未知策略及外部调用方 scope 不自动
解除；HandleOwnershipBindPhased 是独立的显式合同，不是图收集许可。

新增 21 类传递释放后跑完整 discovery：20/23、3 失败、0 未运行，270.247 秒、
峰值 3，4115 输入稳定。v180 的九条默认隔离反例已经在 GCC O0/O2/TCC 通过。
本次失败仍完整保留，随后集中修正的是：

- 旧 resident-scope 断言把已认证类析构仍当作不可冻结的修改过程；改为要求
  能冻结且真实终态 receiver 的 Count/adapter 必须拒绝。原全部样本和 legacy
  callback/factory 负例保留。脚本也修正空 stdout 导致提前退出的问题。
- 新夹具补 VALUE_GRAPH 声明选择及 const 正确性。首错误用语言实际 cleanup
  scope 验证，而非把无 scope 的原生旧诊断误当作已经逃逸的语言错误；仍断言
  原错误身份、失败状态、全部析构和内存平衡。生产错误协议未改。
- 模块退役偶发失败保留，并用受控外线程 mutation 给出 prepares=0 的确定性
  反例。测试先覆盖合法延期，再验证第一次实际 Prepare 的原 BUSY/FAILED/
  错误身份/无提前析构断言；原 18 模式不删，新增 150 次并发准入延期。

然后只跑一次完整 verification：23/23、0 未运行，265.672 秒，峰值 3；4115
输入哈希为 23CF78202CE1A9CDE7810D0C3CFBB481052FA9BDE726B4F56298EDB992279841，
前后一致。两轮间仅修正测试夹具/脚本，生产源码与编译器/运行时构建输入不变。Future callback
共 21 个编译/运行通道全部通过；新增夹具每配置 526 次 finalizer/528 跨线程探针。

公开构建 66 TU 为 5/61 cache hit/miss、39.912 秒；26 合同共享 23 TU 为 1/22、
20.610 秒，头文件变化导致必要重编。XRT 原 26 组 GCC/TCC 各 66/66 实际重跑，
分别 69.936/18.227 秒。快照为 3002/3453 API（451 自有）。发布物、默认后端与
标准库范围不变；Cgen 不新增回溯/析构拼接分支。

发现失败、诊断复现与确定性反例、集中测试修正及最终验证分别保存在
`build/tmp/xlir_launch_v181_evidence/results.json`；该专项完成不是长期目标结项。

## v182：生产者真实拥有边，同哈希复用已通过批次

输出 Future 现在通过一次性 ProducerBindTakeV1 实际拥有 Future callback
和 language-output task frame。终态发布摘边，锁/mutation 外释放，注册/job/
活动调用的真实引用仍保护尾部。AdapterV2 独立认证 producer 策略，不能把
Trace、策略 getter 或 V2 帧布局当作完整 pending 图回收许可；原生 task Job
尚未接入，Watch 关闭/通知与语义析构顺序仍开放。Cgen 没有新增语义分支。

新夹具在每种 GCC O0/O2/TCC 配置证明 200 个 output-only-root 图和 100 个
丢弃输出后的真实无根循环；四种终态仍各按原语言路径完成，不跳过已受理
回调。原 callback 21 个运行通道全部保留，增加三条至 24/24。物理拥有关系
变化使四个旧 C 测试的符号接线/精确计数同步更新，未缩减断言分母。

一次完整 discovery 为 23/23、0 未运行、274.043 秒、峰值 3，4116 输入
前后同哈希。工程八项通过。XRT 初测 GCC 严格编译因新增测试未用 testRequire
中止，2/68 执行；TCC 68/68。保留失败和全部未执行身份后，仅改测试使用
已有辅助函数，原 26 组加两个 modular/single producer 执行重新跑过，GCC/TCC
最终各 68/68，耗时 71.938/18.795 秒。每个新执行有 1300 次拒绝、500 次
终态释放、100 个真实循环及 700 次跨线程锁/freeze 探针、逐事务零 live delta。

生产编译器和运行时未随测试修正变化。修正后以原 InputSnapshot 函数重算
全部 4116 项仍为 95AC938CFEFFED80A07069C1B7723CEBBEEC5BC952938C96CF32D5090670C89A，
并独立校验子报告输入和原测试人群，因此复用完整已通过批次；本轮并没有
第二次全量 23 门 verification。初始重验记录的复用标志为空，仅是元数据
书写错误；最终重验另存新文件并固定 bool，不覆盖原记录。

公开构建 66 TU、5/61 cache hit/miss、38.313 秒；26 合同共享 23 TU、1/22、
19.641 秒。快照 3004/3455 API（451 自有），能力矩阵同步，延期范围未改变。
发布物、默认后端和完整语言/平台验收不在本轮通过声明中。证据入口：
`build/tmp/xlir_launch_v182_evidence/results.json`。

## v183：准备协议、通知顺序与错误分类，一次发现和一次完整复验

已认证 Watch 通过独立不可变策略报告实际拥有的回调帧，V3 Future 描述符返回
必须履行的准备职责。Plan V2 在每项准备和每次语义析构后重新构建物理图，
源码宿主直接接入；不能只移除 Watch，也不能在生成 C 中拼接关闭/析构特例。

完整发现 22/23、0 未运行，284.246 秒。新增生成语言两级 finally 用例的三个
配置都在完整图准入处失败：普通 Value 适配器把自有内联字符串误当成外部借用。
另补实际 TCC 宿主反例，证明重建图的临时分配失败会错误记为永久语义失败。
集中修复物理字节拥有识别及 ERROR/FAILED/BUSY/READY 状态区分，没有给 Cgen
增加回溯或私有语义。原始失败、诊断源和构建产物文本均保留。

完整复验 23/23、0 未运行，297.590 秒，两次均峰值 3、4120 输入前后同哈希。
Future callback 27 条和 Plan 12 条通道全部通过；新增通知中新建职责、复活、
通知后 OOM 与两个 finally 先于类析构的真实运行证据。原 TCC 221 个事务保留，
再加每配置 50 个快照 OOM 延期/恢复/真实页释放事务。

XRT 仍选择原 26 组 68 个程序。扩展 blob 测试先因把不可变 Clone 当成新物理
节点而在 5/68 停止，随后单头因比较 API 未选择而在 42/68 停止；两个编译器的
失败及未执行程序均保留。测试改为检查同一物理身份、精确双重拥有计数和基础
String/Bytes 读取，不增加模块依赖或改变生产代码。最终 GCC/TCC 各 68/68，
73.369/19.719 秒；每执行新增 1200 个物理 blob、2400 个实际拥有槽和 100 个
certified Watch 循环。重新计算 DEMO6 全部输入仍为
`C07A920F576A38C6A9B81A5DAEF221EB5E9CB649036E5CE58EEA5BF6C48D1D51`，
所以复用已经执行过的完整 verification，不再额外跑一次相同的 23 门。

这不完成完整 pending/原生任务/其他 async 拥有图，也不证明新 pending 语言
夹具的实际模块卸载。必要的原生/网络、完整语言与跨平台/发布验收继续开放。
发布物与默认后端未变；完整证据：`build/tmp/xlir_launch_v183_evidence/results.json`。

## v184：pending 混合模块真实卸载，一次发现和一次强化复验

新增门 `check_xlir_pending_module_preparation.ps1`，批次可用
`-PendingModulePreparation` 选择其私有输出资源；原 23 门保留，当前为 24 门
拥有图/生命周期专项，不是原完整语言回归分组数。生成两个 finally 捕获同一类
对象，原生 Promise 槽由 TCC 模块实际拥有并组合不可变 Trace/Finish/Prepare。
它是明确的生成/原生混合模块，不伪造语言 Promise 支持或隐式原生拥有登记。

首轮 discovery 24/24、0 未运行，306.965 秒。复核发现需把“确实卸载”进一步
限定到实际宿主的执行阶段，故加强 Prepare/Finalize/未 Clear 断言，并让测试
模块 unchecked Unit 使用与生成代码相同的显式错误转交。没有新增生产补丁，
也没有把全绿发现批次改写成失败。只再运行一次完整 verification：24/24、
0 未运行，308.984 秒；两批均峰值 3，4124 输入前后一致。最终指纹为
`3456530F37A77E1EC7187E5191BCA1753FDDB68761FD80B8DA9D772EFDF5F018`。

新门 O0/O2 各八种模式，共 400 次实际 TCC 生命周期、800 次有序 finally、
400 次类析构、1200 次跨线程锁/freeze/驻留探针和 350 次真实引用/复活延期。
其中 400 次通知直接验证宿主图准备阶段，150 次析构直接验证图 Finalize；
四种外部完成模式还在 MEM_FREE 后读取真实结果，所有事务内存平衡。

原 2748 个测试文件全部保持，新增三个持久用例；原 callback 27 通道、Plan
12 通道及模块循环/快照 OOM 样本不减。1314 项构建输入同哈希，复用 v183
最终公开源码与合同驱动，生产 src/lib/Cgen 和发布物未变化。文档更新后再以
原 InputSnapshot 函数重算完整批次输入，不另跑第三轮相同验证。

独立 XRT checkout 同期出现非本批的 JSONL/XSONL 改动，保持不动、不集成；
本批不宣称 XRT 各 68/68 已在新 checkout 重跑或全部同哈希复用。DEMO6 嵌入
运行时及源宿主是本批实际验证的固定输入，历史 XRT 报告单独保留其时间边界。
新增/变化输入保存新副本，其余按哈希引用 v183 已冻结版本，避免重复复制整棵
历史证据。工程检查、两轮所有日志（含空日志）、生成文本和阶段断言结果均保存。

该专项完成不缩减完整 native/async 参与、依赖环关停及语言/平台/发布验收要求。
完整证据：`build/tmp/xlir_launch_v184_evidence/results.json`。

## v185：Job 实际 producer 与封闭任务帧，集中修正后一次完整复验

Job 输出现在真实保有原生作业，执行器引用不由图检查估算或扣除。不可变数据
策略明确已转移的数据引用和 Proc/Drop/Ops；V2 任务帧及 forwarding Watch 的
认证接入源码宿主、TCC 接线和共享 Plan。准备等待正常工作与清理完成，不以
移除 Watch、摘队列或提前释放捕获来伪造 READY，Cgen 没有新增语义分支。

公开源码重建 66 TU（5/61 cache hit/miss，40.969 秒），26 合同驱动共享 23 TU
（1/22，20.235 秒）。完整 discovery 23/24、0 未运行、323.787 秒：任务存储门
六条失败通道分别暴露旧图的 opaque 提交接线和新用例遗漏类 backing/lifetime
节点；原程序及失败版本均保存。集中修正两份测试和架构守门接线，保留原 15 条
task-storage 通道并新增三条完整生命周期通道，不减少任何旧样本分母。

随后仅一次完整 verification，24/24、0 未运行、293.824 秒。两批峰值均为 3，
4125 输入各自前后一致；最终输入指纹为
`E692C6636AD4022ABFF6AC9845BAB23AC31B8C71C30993AA1C1E871E231A36EF`。
新三通道合计 600 个完整任务图、1200 次执行器/结果根延期、1200 次 BUSY 准备、
600 个原路径终态、1200 次类析构、450 次执行中拒绝及 1200 次析构跨线程探针。
原 callback 27 通道、Plan 12 通道和 pending 混合模块的 16 个程序继续通过。

原生验证使用明确固定的维护快照：从 v183 最终实测来源重新生成四份旧头，
逐字节匹配当前嵌入基线，再只加入 Job 改动，走原成熟度/生成检查与同步流程。
此前尝试使用 v184 开始时源码的复现失败也保留，因为其中已经混入并发 JSONL
工作而旧生成头尚未更新。未覆盖上游改动，也未直接导入无关 JSONL 功能。
34 组/89 个 modular/single 程序在 GCC/TCC 全部通过，原 26 组/68 个程序完整
保留，2001 个固定输入后续再核对；112.602/32.867 秒。不是整个 live upstream
checkout 的通过声明。上游无关 xllm-session EOF whitespace 失败单独保留；
本批八个 XRT 路径的检查通过，不能把局部通过写成上游全绿。

文档和证据整理后只重算输入与工程检查，不第三次运行同一批。Cgen 源/原 Future
生成样例、发布二进制和默认后端不变。其他异步/生成器帧、生产者依赖环关停、
必要 native/network、生成模块原生 Job 卸载联合证明及语言/平台/发布验收仍开放。
证据入口：`build/tmp/xlir_launch_v185_evidence/results.json`。

## v186：批量闭合捕获 ABI，保留完整迭代故障人口

普通 worker-pool async 的参数析构向量现在是显式 IR 合同，Lowered 固定封闭
V2 布局，Cgen 仅输出字段描述及共享清理调用。常驻 Capture 增至九类物理载体，
动态 iterator 的实际 cursor/借用视图从生成 C 移入运行时。原生 cursor 子节点
尚未获得生命周期准入，不能把 wrapper 的适配完成写成任意环回收完成。

原 24 门完整 discovery 收集到 21 通过/3 失败，323.765 秒。首次 generator
门在旧 IR 文本断言处停止，其原生子程序未运行、子报告未产生；保留这个事实，
不将门级 0 not-run 扩大解释为全部子程序已执行。冻结 discovery 的 3722 项
源码/测试/工具输入版本，与批前 6102 项快照共同保留修正前证据。

按根因集中修正旧 ABI 断言、源码清单漏接和生成 C 格式警告。迭代器测试原先
假定 pending continuation 同步完成且 Future 释放即代表执行器尾部已归还，
现等待真实 Future 终态及 executor live=0；原 5180 次人口、3620 次真实故障、
480 次恢复后故障、160 次关闭及逐例内存平衡全部保留。新增六类捕获用例补上
真实生成可达性检查，按既有 bool/输出参数 ABI 接线语言闭包，未修改 Cgen
调用语义去兼容错误宿主。必要的失败诊断探针独立保存，不冒充完整批次验收。

随后仅一次完整 verification，24/24、0 未运行、332.293 秒，峰值 3。
4131 输入前后相同，最终指纹
`9B74259006324B66284862BD499079C25DE9CC9CB421DE839D8334ACF17ED4BF`。
task-storage 原 18 通道保留并增至 27；七组生成各重复一次。新增三类程序在
GCC O0/O2/TCC 全通过，原 callback 27 通道、Plan 12 通道、pending 模块 16
程序及完整 Job 拥有图人口继续保留。发布产物未重建，工程检查与文档后只重算
输入指纹，不第三次运行整批。Cgen 源按统一 LF 口径净减少 1335 字节。

XRT 使用未变的 v185 固定 vendor，复核 2001 输入和四份嵌入头后复用原 89/89
证明，不引入并发 JSONL/XSONL/xllm-session 工作。独立上游 EOF 空行失败仍记录。
完整 async/generator 布局、原生 cursor 子节点、封闭 producer 依赖环及完整
语言/平台/发布验收继续开放；旧动态迭代 CLI C-host 门的同源码 JIT/AOT 接线
也未被本批独立 TCC 原生通道取代。
证据入口：`build/tmp/xlir_launch_v186_evidence/results.json`。

## v187：托管原生游标，定向发现后一次完整验证

旧 `xvalueiter` 的栈/独占堆式 ABI 保留；新增具有真实 Retain/Release 的不透明
`xvaluecursor`，让语言 wrapper 的原生子节点可独立 Hold/Claim/Clear/Finish。
它拥有内联快照的 backing 和实际析构 shell，借用 key/item 不重复计边。模块
resolver 独立认证子节点，不因 wrapper 已知而跳过未知 native 对象。

首先在固定 vendor 中保留旧迭代器程序，增加 modular/single 托管游标程序，
GCC/TCC 各四个程序通过。再定向验证实际六节点/七边的语言 wrapper/cursor/
析构对象拥有环，GCC O0/O2/TCC 全通过。初次原生诊断脚本的绝对路径拼接以及
循环诊断脚本的数组组装错误单独保留，修正脚本不改变运行语义或测试人口。
精确匹配旧 SHA256 的中间脚本版本亦保留；不伪称跑过一轮完整 24 门 discovery。

源码稳定后仅一次完整 verification：24/24、0 未运行、352.647 秒，峰值 3；
4132 项输入前后一致，指纹
`56A20DE92E97B23B287EF9C31A4DA39CE5976B61730C450F6B127CA99C4EEC8F`。
原 task-storage 27 通道、14 次生成保留，新三条循环通道合计 225 个完整图、
300 次外部根延期、225 次漏接子节点拒绝、5400 档 OOM（1575 次实际失败）、
225 次析构与 context 释放、450 次跨线程 freeze。原 5180 次动态迭代人口及
已有 Job/模块/回调/Plan 群组不减。IR/lowering/Cgen 及原 Future 生成样例未改。

独立执行固定 XRT 的完整原生回归：原 34 组/89 程序源文件保留，加两份新程序，
GCC/TCC 各 91/91，112.705/26.629 秒，2003 输入一致。新 API 通过原生成与
成熟度入口同步，快照 3016/3467。没有复制并发上游的整个 manifest，没有同步
无关 JSONL/XSONL/xllm-session 改动；七个相关 XRT 路径的独立检查通过，旧 EOF
告警仍保留。文档后只重算工程检查和原始输入指纹，不再重复同批测试。

本批关闭的是托管游标及明确 resident 对象策略的运行时拥有环。其他异步/生成器
帧、封闭 producer 依赖环关停、生成模块实际卸载联合证明、旧 CLI C-host 同源码
JIT/AOT 接线以及完整语言/平台/发布验收仍按原目标推进。
证据入口：`build/tmp/xlir_launch_v187_evidence/results.json`。

## v188：稀疏帧统一布局，跨门失败集中修正

async 和 generator 直接序列化既有 XLIR 的物理槽、初始化标记与 destroy action
序列，共用常驻 `XL6_ActivationCaptureRuntimeV1`。不在 Cgen 为不同帧重复生成
逐槽追踪和整帧销毁代码；局部释放/转移及 source finally 仍遵循原 CFG。旧 V1
task/callback 与新稀疏布局共享物理种类操作，不把借用 loan 当拥有边，也不把
payload 描述符当作任意外层控制/帧的 collector pin 证书。

先做三组定向 discovery，3/3、147.894 秒：新增穷举 256 种初始化组合、300 组
指定顺序和首次失败事务、1028 次析构回调跨线程 freeze 的测试，GCC O0/O2/TCC
均通过。原有 30 条 task-storage 通道、14 次生成保留，新三条达到 33。IR/layout
比对逐项核实真实槽与清理顺序；复核时修正测试辅助代码对空数组的指针运算。

首次完整 verification 为 22/24、0 未运行、326.364 秒，而不是全部通过。
generator-ownership 的旧逐槽 C 字符串断言在原生程序执行前失败；continuation
scope 的 native 两通道通过，language-TCC 两通道在新入口符号重定位时失败，
没有执行语言生命周期。两者输入和日志均保留：前者改为真实 IR/layout 对照，
后者补上独立手工宿主的符号注册，生产宿主原本已有注册；生产代码无需改动。
集中修正后完整复测 24/24、0 未运行、333.179 秒，峰值 3；4134 项输入前后一致，
指纹 `3C31EAFF31D541756AAE2C0E9F1588FD66AA00ECC335E6228459B1589834780F`。
恢复 generator 原每通道 2400 生命周期、1000 async 事务、2000 挂起和 16000 图
检查人口，并完成每优化级别 100 次真实 TCC 关闭/恢复/闭包/卸载。实际有两次完整
运行，不伪称只有一次，也不把全部专项通过解释为完整目标完成。原 2757 测试文件
中四个修改、2753 个字节未改，另增两份稀疏布局测试；没有删减原有语义断言。

固定 XRT 嵌入版本及 2003 项原生输入复核未变，复用 v187 91/91；不重新同步
并发上游，也不虚报本批重跑。文档后复算原始输入指纹，不追加第三次完整运行。
标准库重设计不纳入本批；帧实体生命周期、producer 依赖环、真实卸载联合证明、
同源码 CLI JIT/AOT 和完整语言/平台/发布验收继续按原目标推进。
证据入口：`build/tmp/xlir_launch_v188_evidence/results.json`。

## v189：生成器独立真实引用，集中修正后一次完整复验

以 v188 冻结结果 `06D01AA8E31E8D10C6478F0CFCDB393AACA76F3F2FFC7787A087A4218AD6F277`
为基线，保留全部 24 项专项及原测试人口。V2 generator 控制对象、帧、执行中
调用均有实际引用；帧另持真实代码租约。Lowered IR 切换封闭 V2 存储 ABI，Cgen
仅发出帧/item 布局和已验证 CFG，模块 resolver 分别认证 control/frame/children。
共享 Plan 准备阶段正常关闭生成器，先执行 finally，再清理存储及对象析构；
不以清空捕获绕过语义职责，不把旧唯一拥有计数伪装成 collector pin。

三项 discovery 为 2/3、131.915 秒。生成器原拥有图及 continuation scopes
通过，generator-storage 完整收集了新增 fixture 的 CFG 规划失败和错误采用
旧式不透明析构回调的测试契约。定位到规划只生成三个可达挂起点，而检查要求
四个（含一个不可达 finally 分支）；规划、程序覆盖和验证器现共同采用真实
CFG 可达性。保留未引用及已引用两种合法 fixture，并追加不可达身份拒绝检查。
native fixture 则接入完整显式 lifetime/finalizer policy，保持旧回调边界不变。
初期 API 缺少 generator 声明头、fixture 使用不受支持的类型拼写 `dynamic`
也在批前诊断中修正；正式动态类型用既有 `var`。不把这些失败计为验收通过。

集中修正后唯一一次完整 verification：24/24、0 未运行、345.231 秒、峰值三
进程，4141 项输入前后一致，指纹
`AA958FCC6F5C632BEBA172658F4D628432D1CE1A1FECAFFFC4B1768B44EF243B`。
新增每编译器 140 个真实六节点/七边生成器环，finally 中 yield 的 280 步职责
均执行，140 次析构/context 释放、560 次跨线程 freeze；180 次真实根延期、
140 次漏接 frame 拒绝、3360 档计划 OOM 中 980 次实际失败。40 次准备失败
保留显式 false/NULL，重试不重复职责；20 次析构失败安全撤销。另每配置 150
次最后拥有者重入关闭、100 次真实 DLL 描述符卸载、100 次实际分配失败、250
次 ABI 拒绝均通过。GCC O0/O2/TCC 的原 72 个 generator-storage 程序保留，
新增至 78，确定性生成 48 → 50。所有其他专项原分母逐项复核，不缩减为 smoke。

十个生产文件变化；原 2759 测试文件保留（2754 不变、五个合同/接线调整），
新增七个。源码构建 67 TU/17.942 秒/66 hit，29 合同共享 23 TU/15.582 秒/
22 hit。Cgen LF 字节减少 2175，frame-loan 生成 C 减少 3431；另两个既有样例
逐字节不变。发布 CLI/DLL/VFS、嵌入 XRT 及默认后端不变；固定 XRT 的 v187
91/91 证据重核 2003 输入后复用，不虚报新跑。

后续工程门中一条旧文本断言仍要求 Cgen 发出 generator cleanup 包装；改为
验证常驻 cleanup/错误边界后，七项 DEMO6 工程检查通过，保留不相关的相邻
XRT `xllm_session_core.c:646` EOF 空行失败。只有这一审计脚本改变；公开 API
专项把它列为输入，因此完整补验该单项，1/1、54.226 秒，原案例不缩减。
其余 23 项执行脚本/记录依赖无变化。以保存的唯一旧审计行重构原完整指纹，
严格证明没有其他运行输入漂移；补验当前 4141 行指纹为
`E3634C83C991A14CCC91D244AE5E93B12800E108D1EC00FB4B50DA0550C0FFD7`。
实际执行是一轮完整 24 门加一项受影响补验，不表述为零补验或两轮完整测试。

通用 async 帧/ticket、producer 依赖环关停、必要 native/network、生成模块联合
真实卸载、独立 CLI 同源码 JIT/AOT、完整语言/平台/发布验收和旧路径退役继续
开放。文档后复核原输入及工程门，不为相同输入重复整批。
证据入口：`build/tmp/xlir_launch_v189_evidence/results.json`。

## v190：通用 async 真实帧引用，集中发现后一次完整复验

以前轮冻结结果 `F50E458B20D558E3C1988F055FF086E657F1F91C4358D0D8E1F59FBBFFC25CC2`
为基线，完成 V2 async 稀疏帧、内嵌 ticket/Watch 的单一物理分配引用。Queue、
Watch、native 与 Plan 各取还实际引用；帧另保留代码租约，不把队列 credit
或地址登记伪装成 collector pin。Cgen 发出完整布局，常驻模块执行清理/退役；
独立源码列表完整接入，私有 ticket 布局要求同源码重编译。源语义和 accepted
work 继续由原执行流程承担，Prepare 不替它强行清空 pending 捕获。

三项集中 discovery 为 2/3、46.330 秒。原 async-storage 三通道及新增 TCC
生命周期通道通过；GCC 新宿主由于宏替换漏声明原 ReserveSuccessor，构建失败。
frame-ownership 与 continuation-scopes 全过。原始输入及失败日志保留，补齐
宿主声明并集中修正旧工程断言、源码注册列表后，再执行唯一一轮完整复验：
24/24、0 未运行、359.792 秒、峰值三，4146 项输入前后一致，指纹
`68F123CE688B8183DE6E23BA7491D814D7869B30039956CA851986C9DFEFC565`。
本批没有第二轮整批或受影响单项补验，不把未执行路径算成通过。

async-storage 保留原人口，三条运行增至六条、两次确定性生成保留；原五个
fixture 的 20 个负例保留，再加十个新旧 V2 边界负例。每编译器新增 240 个
native/Plan 生命周期事务、80 个真实 Watch 完成状态、50 条实际生成后继链、
580 次跨线程 freeze、820 次拒绝、120 个显式失败、10 次真实 OOM 和 20 次
真正 DLL 最后引用卸载。两次 await 的前后帧可各自保活，回调返回后队列继续
推进，而清空的旧帧可晚些释放。逐事务精确内存平衡，其他 23 专项人口逐项
复核。原 2766 测试文件保留：2762 不改、四个合同/接线更新、另增四个文件。

七项 DEMO6 工程门通过；保留相邻 XRT 不相关的 `xllm_session_core.c:646`
EOF 告警。生产改动为 12 个既有文件加一个常驻模块；68 TU 公开源码构建
15.266 秒（68 hit），29 合同/23 TU 构建 14.405 秒（23 hit）。Cgen LF 字节
减少 1248，async 样例减少 1825，Future callback 样例减少 1681；不是 DLL
体积测量。固定 XRT 2003 输入复核未变，复用 v187 91/91，不重新同步上游或
重建发布 CLI/DLL/VFS。默认 XLIR 后端不变。

通用 async 的实际保活已落地，但 pending producer/Watch graph policy 的
逐节点准入仍开放；同时继续 producer SCC、必要 native/network、生成模块
联合卸载、同源码 CLI JIT/AOT、全语言/平台/发布验收与旧路径退役。冻结前
再次按原 InputSnapshot 算法复算，不为文档变化重复相同运行输入的完整测试。
证据入口：`build/tmp/xlir_launch_v190_evidence/results.json`。

## v191：async producer/Watch 图准入，一轮完整验证加三项受影响补验

以 v190 冻结证据为基线，完成稳定 producer 的实际引用、当前帧交接及 certified
Watch；IR 显式携带并验证 ownership ABI，Cgen 只选择常驻入口。根 Abort 后的
私有帧可重新绑定；后继必须保留同一 output Promise 身份，回滚不改变旧 current。
执行队列依然构成真实外部根，没有用收集器提前取消 accepted work 或源 finally。

集中发现 5/5、72.887 秒。一次完整 verification 保留全部原 24 项，并追加
受本次链接变化影响的 capture 专项：24/25 通过、0 未运行、355.744 秒、峰值三。
4147 输入前后均为
`1F417B49F9DEE5EFB12E483A558BD6C9976F3321BCBCE2B274C5182FE578E2AF`。
完整失败记录保留，未在批次执行中修改源码。唯一失败是 generator 宿主的两个
async 图大小没有计入新增真实 producer：每图应增加一节点、三边，可达性和
执行根断言通过。批后仅修正该宿主精确预期，完整补验原生成器专项，1/1、
16.933 秒；未改生产编译器、运行时、生成代码或其他用例，未再跑第二轮完整。
补验指纹为
`BBACB58BC3A0D40A7DE91EDD68FA65F7160AA1C987BC30EFBE39EE3E625FF32F`。
记录依赖复核发现，公开源码与 generator-storage 两项也把该宿主列入输入；
追加完整补验，2/2、118.152 秒，保持原 generator-storage 的 78 条运行/50 次
生成，两个补验批次均为上述当前指纹。保留旧输入并用原算法重构完整旧指纹，
确认其余 22 项可以复用。实际为一轮完整加三项受影响补验（1 + 2），最终
25 项具有有效通过证据，但不把首轮 24/25 改写成 25/25。

async-storage 原六条运行与两次确定性生成保留，增加至九条。GCC O0/O2/TCC
每配置新增 100 个实际生成 pending/ready outcome 图、1040 次 policy/身份拒绝、
30 次根重试、640 次后继回滚/交接、200 次跨线程 freeze；330 档分配预算有
80 次真实 OOM。Plan 从 output 传递准入，前驱退役后刷新得到新帧，真实保活
不会形成旧帧链；十条 64 后继 native 链逐步核对恒定 live count/bytes。
原生成器 2400 生命周期、1000 async 事务、2000 挂起、16000 探针和 6600 真实
失败人口保留。所有旧专项分母逐项比对，原 2770 测试文件保留，新增一个文件。

十个生产文件变化，Cgen LF 字节增加 217；三个跟踪生成 C 样例均未增长。
源码驱动 68 TU/15.697 秒/64 hit；29 合同共享 23 TU/12.224 秒/23 hit。
七项 DEMO6 工程检查通过，相邻 XRT 不相关 EOF 告警保留；固定 XRT 91/91
重核 2003 输入后复用，发布 CLI/DLL/VFS、嵌入 XRT、默认后端不变。

完整目标继续开放：封闭 producer SCC 正常关停、必要 native/network 合同、
生成模块联合卸载、独立 CLI 同源码 JIT/AOT、全语言/平台/发布验收和旧路径退役。
本批证明传递准入与真实引用，不把 pending 图已准入等同于依赖环已能关停。
证据入口：`build/tmp/xlir_launch_v191_evidence/results.json`。

## v192：取消交接先集中发现，再一次完整验证

以 v191 冻结证据为基线，保留全部 25 门，新增 current-source cancel-wait 门。
实现常驻 producer 的一次性取消观察，IR 用 RESIDENT_AWAIT_ONCE_V2 明确约束，
Cgen 移除逐帧取消状态及后继复制，只发出已验证 Observe 调用。显式 CancelWait
保留真实尾部引用，只退回自己的 Watch，通过正常队列/resume CFG 执行取消，
不关闭共享源、不跳过 catch/finally、不将 pending 的执行根视作已经解除。

发现批次 0/5、58.032 秒，分别保留各失败日志与完整输入版本。共性问题是新增
代码的 GCC 严格排版告警；TCC 新增路径还检出两处宿主接线问题：launch 前有
活着的类对象却等待其代码租约归零，以及遗漏关闭已经空闲的私有队列。一次性
修正排版、准确队列/代码退役同步和队列 Close，未删除原 case 或降低断言。

随后只执行一轮完整 verification：26/26 通过、0 未运行、367.879 秒、峰值三。
4151 项输入前后均为
`2E78CB91FACA472F40D96A0C0B96F5D4AAA42AB42029F0DE2E4731A36BD2DCD0`。
没有补跑第二轮完整批次或受影响单项。原 async-storage 九条通道和两次生成、
generator-storage 78 条运行和 50 次生成，以及其他旧门记录的人口逐项保留。

新门在 GCC O0/O2/TCC 各保留原 34560 个取消执行案例、20520 次取消请求、
34560 个 publication observer；每配置 1289034 次分配平衡。追加 2106 个生成
中断、300 次三线程实验、2106 次零分配预算传输和同样数量的同帧原生重入，
验证共享源独立、finally 后续 await 不重复取消、精确失败 outcome 和真实尾部
引用。两个取消 IR fixture 各确定性生成两次，frame 合同负例从 139 增至 154。
历史 cell-cancel-race 只复验 IR 合同，不把未执行的旧 runtime/CLI 门计为通过。

九个既有生产文件变化，无新增；2771 个既有测试文件全部保留，五个合同/接线
更新、三个新测试文件。Cgen LF 减少 430 字节；两个 async 生成样例分别减少
661、336 字节，生成器样例逐字节不变。公开源码 68 TU/14.440 秒/67 hit；
31 合同共享 23 TU/12.679 秒/23 hit。七项 DEMO6 工程检查通过，仅保留相邻
XRT 无关 EOF 空行告警；固定 XRT 2003 输入复核后复用 v187 的 91/91。
发布 CLI/DLL/VFS、嵌入 XRT、默认后端均未改变。

本批是 SCC 正常关停的执行入口前提，不是 SCC 不可达证明或收集器授权；完整
目标中的依赖环关停、必要 native/network、生成模块联合卸载、独立 CLI 同源码
JIT/AOT、全语言/平台/发布验收及旧路径退役仍开放。
证据入口：`build/tmp/xlir_launch_v192_evidence/results.json`。

## v193：依赖环集中修正，保留一次新增回归揭示的测试缺口

以 v192 冻结证据为基线，保留原 26 项专项，增加真实宿主管理队列依赖环门。
队列拥有槽、物理帧、稳定 producer、Future 和 Watch 逐节点准入，共同计划
claim 及关闭准入是必要条件；不可达本身不是取消 accepted work 的充分条件。

发现批次 5/6、78.030 秒，保留 OOM 聚合断言失败记录；原 80 个生命周期均已
执行完成，真实 OOM 数为 20。将原注入预算覆盖到全部轮次，不降低 >20 断言。
第一轮完整 27/27、373.912 秒，4156 输入前后均为
`1649570EB35B6D403BEF3D4809F99A9C9BCAA3C90D66CA5E38A485BF0CC9F806`。
首轮结束后审查发现：初版 Prepare 把不可达的可运行无环链也取消了。新增对照
在未修正生产代码上 GCC O0/O2/TCC 全部运行失败；保存单独回归报告及输入，
不能因为原 27 项已绿就忽略这个语言行为问题。

集中修正为冻结、无分配的真实 wait-for 成环/入口判定。必须是同计划已认领
且在已关闭准入队列上的 pending 当前帧，source/output producer 身份准确；
只中断实际环内一员。无环链继续正常运行，非环 feeder 通过 outcome 传播完成，
finally await 外部清理时可重新 ROOTED，完成前不 finalizer/clear 或发布结果。
新增自环、无环及 feeder 后第二轮完整 27/27、0 未运行、372.470 秒、峰值三。
4156 项输入前后均为
`446F8EDFBF26CBA4694E35698A1E4423EEB4CB74D6B93A367F43046B6EDEA015`。
因此本批是两轮完整验证加一次三配置失败回归，不声称“一次全绿即完成”。

新门每编译器为 120 个一/二/三 producer 环、40 条可运行无环链、40 条 feeder；
800 次逐节点拒绝、160 次环内中断、2400 注入预算下 270 次真实 snapshot OOM。
每事务精确检查所有 outcome、catch/finally、清理重建根、物理退役及分配平衡。
原 async-storage 九条运行/两次生成、generator-storage 78 条运行/50 次生成、
cancel-wait 原 34560 执行及 2106 新中断/300 三线程人口和其他专项均完整保留。

六个既有生产文件变化，无新增；2774 个原测试文件全部不改，新增四个文件及
独立门。Cgen 和三个跟踪生成 C 样例逐字节不变。最终源码驱动 68 TU/15.301 秒/
67 hit，32 合同共享 23 TU/13.374 秒/23 hit。七项 DEMO6 工程门通过，仅保留
相邻 XRT 不相关 EOF 告警；固定 XRT 91/91 在复核 2003 输入后复用，不新跑。
发布 CLI/DLL/VFS、嵌入 XRT、默认后端未改；文档更新后用原 InputSnapshot
算法复算最终指纹，不为文档变化再重复运行相同源码的完整批次。

执行器/工作线程及混合 producer 依赖仍开放，原生执行/等待入口的真实根不能
猜减。必要 native/network、生成模块联合卸载、同源码 CLI JIT/AOT、全语言/
平台/发布验收与旧路径退役继续推进。本批不是完整长期目标达成。
证据入口：`build/tmp/xlir_launch_v193_evidence/results.json`。

## v194：真实执行器拥有关系集中发现，合并修正后一次完整验证

以 v193 冻结证据为基线，保留原 27 门，新增 executor ownership 门。实际
worker 的 wait.queue 持有和归还真实队列引用，休眠时按拥有槽追踪，不从 RC
猜减数量。executor 的实际 owner/plan/native 引用与已 join 的内部资源退役
分离，运行、原生入口、join 和退役转换仍拒绝检查；独立 queue 根阻止关停。

构建检查先发现 API 漏 include 和新测试排版告警。补上 include 后合同构建
输入记录已过期，首次发现批次 1/6、26.962 秒，五门被输入检查拦下，不能算作
已执行的语言失败。重建合同记录后新发现批次 5/6、143.470 秒：新增门的原
queue/executor 六通道全过，两个新宿主因重复 freeze 和漏声明原函数失败。
此外独立原生入口回归证实了生产缺口：宿主 acquire 重入 Destroy 能提前退役
外层准入仍在使用的资源（build=0/run=3）。合并修复要求 Destroy 是唯一原生
入口，acquire/drop 两个重入均拒绝；独立复验 run=0，回归并入永久测试。

合并修正后重建两个源码驱动，仅一次完整 verification：28/28、0 未运行、
380.706 秒、峰值三。4160 输入前后均为
`3B067F2D5F09D2C93B1DD7EFE4D9E76D195FBC49CC81081BA037426722016CC1`。
所有发现、独立失败回归及修正版本保留，不更改旧失败记录、不以 smoke 替代。

新门 GCC O0/O2/TCC 共 12 运行通道、两次确定性生成。原生 queue/executor
原程序不改；每配置新增 120 休眠图、60 外部 queue 根、120 policy 拒绝、20
已 join 本体保活、40 run/release 拒绝、40 acquire/drop 重入拒绝，4800 预算
下 420 真实 snapshot OOM。真实 worker 执行的生成用例每配置 60 环/20 feeder、
320 逐节点拒绝和 40 cleanup-await 重建根；逐项核对正常 CFG、join 和准确
分配平衡。原 27 门记录人口逐项不减少，包括 v193 的无环可运行链。

七个既有生产文件变化，无新增；2778 原测试文件不改，新增三个文件及一门。
公开源码 68 TU/15.881 秒/67 hit，32 合同共享 23 TU/14.030 秒/23 hit。Cgen
和三个跟踪生成 C 样例不变。七项 DEMO6 工程门通过，保留相邻 XRT 不相关 EOF
告警；固定 XRT 91/91 重核 2003 输入后复用，发布 CLI/DLL/VFS、嵌入 XRT 和
默认后端未改。文档完成后按原算法复核完整输入，不为文档变化再跑一轮。

下一批是 generated module 执行器拥有的显式 IR ABI/trace 及联合卸载，不是
取消现有 guard。混合 producer、必要 native/network、同源码 CLI JIT/AOT、
全语言/平台/发布验收和旧路径退役仍未完成。
证据入口：`build/tmp/xlir_launch_v194_evidence/results.json`。

## v195：生成模块拥有执行器，V3 预检与混合服务边界

以 v194 冻结证据为基线，保留全部 28 门，新增 module executor 门。lowering
和 verifier 以实际 lowered 操作确定 executorOwnershipAbi；V3 图预检与 V2
资源排空使用互斥且精确验证的协议，Cgen 按 IR 发出真实模块 executor 槽的
拥有边及普通 checked teardown 调用，不复制线程/等待环回收实现。

发现批次 7/7、168.709 秒。首轮完整 28/29、415.660 秒，唯一失败是旧模块
合同对 async frame fixture 的 V2 固定断言；该门两个原运行通道均通过。
原有 41 globals、236 other slots 和 587 变异保留，增加 11 个 V3 变异到
598。静态架构门的旧非空 executor 拒绝断言迁移为实际 V3/拥有边/混合池顺序
检查，原失败证据保留。

补查混合服务时先排除了被 eager 化的无任务池 leaf，改为真实 task.spawn。
随后分清 preflight 的资源行为与 Destroy 后真正新任务的 fresh code admission；
后者维持原有拒绝，不改成 scoped inheritance。以保存并重核输入的旧编译器
生成同一 fixture，证实 owner 开放时 preflight=READY 仍会把 pool 关闭，使
后续任务提交失败，build=0/run=3。修正为 accepted executor 工作未排空时
BUSY 且不碰 pool，独立 50 次同输入复验 build=0/run=0。全部设置错误、错误
假设和真实回归日志保留，没有只保留最后的通过记录。

合并修正并重建后最终完整 29/29、0 未运行、432.585 秒、峰值三。因此本批
实际为两轮完整验证，不宣称只跑了一轮。4167 项输入前后均为
`758A6DC34AF326E157C654B3FBE7348E34A36D6C54F4E7BDC379F82529437ADA`。
原 28 门全部记录人口逐项比较，不用新门代替旧语言、IR 或运行时测试。

新门共八运行通道、四次确定性生成。GCC O0/O2 宿主分别使用实际 TCC 模块和
实际 DLL；四个基本通道各 182 次生命周期，另四个混合通道各 50 次，共 928
次真实 MEM_FREE 与准确分配平衡。环、feeder、无环、外部根、finally await、
错误协议、退役失败重试、原全局对象析构均检查；每基本通道 32 个分配预算下
有 12 次真实 OOM。混合通道分别验证开放 owner 正常完成与关闭 owner 拒绝
真正新任务；它们不是混合等待环的回收证明，也不是公开 Future C 导出支持。

八个既有生产文件变化、无新增；2781 原测试保留，2779 不改、两个合同/宿主
接线更新，新增六个测试文件及一个门。Cgen LF 增加 1444 到 5086936；三个
跟踪样例分别增加 253、354、463 字节，变化来自显式边界声明及拥有关系。
最终源码驱动 68 TU/20.055 秒/49 hit，33 合同共享 23 TU/16.277 秒/22 hit。
八项工程检查和 6558 文本文件编码通过；固定 XRT 91/91 仅重核 2003 输入后
复用，发布 CLI/DLL/VFS、嵌入 XRT 和默认后端不变。文档更新不触发重复全跑，
仍以原 InputSnapshot 复核最终源码及测试输入。

后续继续真实 task pool/Job 图及混合 producer、必要 native/network、其他
组合模块卸载、同源码 CLI JIT/AOT、全语言/平台/发布验收和旧路径退役。
证据入口：`build/tmp/xlir_launch_v195_evidence/results.json`。

## v196：先补真实任务池/Job 和延迟资源，再接上层 IR

本批修改固定 v187 XRT 源树中的实际 task pool/Job 合同，并对接上游同一组选定
文件；不替换 DEMO6 嵌入头或发布文件。五个既有生产文件包括 task 公共/内部头、
Job、task pool 和 async file，实现实际 owner/plan/native/resource 引用、休眠
worker、精确 queued Job 边、原生入口重入保护和 join 后资源释放。未认证异步
文件的延迟 finalizer 必须由真实池引用形成外部根，不能以空闲状态推断无资源。
Prepare 不取消或跳过 accepted work；排空期间该工作仍可完成原生资源清理。

原 91 个原生程序全部保留，新纳入三个既有异步文件程序和六个新增程序。
发现阶段先暴露精简套件缺少 atomic 依赖，再完成 GCC/TCC 27/27；资源专项
GCC 32/32、TCC 20/32 在新增 fixture 编译时因结构体数组初始化内部错误停止。
改成等价逐项赋值而不删除断言。原生完整运行实际为 95/95、98/98、100/100
三轮（每轮 GCC 和 TCC），最后分别 110.058/28.413 秒、2005 输入前后不变。
另跑 GCC O2 六个新增模块化/单头程序，35.313 秒通过，不冒称原百程序全量 O2。

每个新拥有程序有 60 个终态外壳、60 个精确 queued 图、四组各 20 个边界
（运行拒绝、native entry、finalizer、真实文件根），96 snapshot 预算/16 次
真实失败及 48 create 预算/5 次失败。另两个程序为 100 次原生入口重入与后续
任务完成、40 次关准入后的真实资源清理。GCC O0/TCC/O2 均含模块化与单头。
Submit 重入和 accepted-resource 排空分别保留同用例修正前 run=3、修正后 run=0
的实际原生对照；构建均为 0，不以缺少新接口、mock 或错误夹具代替回归证据。

质量检查保留通用 public ABI 脚本的 xhttp 夹具缺失记录：固定 v187/native 源树
在改动前后都不含该扩展，核心两项 ABI 另行明确通过，扩展项不计通过。其余
单头一致性、范围 UTF-8/空白、脚本语法、6558 文本编码均通过。并行上游提交
改变了 git 基点，差异及输入证明以批前文件快照/哈希为准，不用 git diff 的
当前较小统计代替本批实际变化，也不修改不相关的上游工作。

DEMO6 原 29 门以原 InputSnapshot 算法重新核对 4167 输入，仍为
`758A6DC34AF326E157C654B3FBE7348E34A36D6C54F4E7BDC379F82529437ADA`，
故复用而非重复执行。Cgen 5086936 LF 字节、上层生产源码、测试和发布产物
均不变。下一批必须接入 XL6_TaskPool/显式 IR 拥有 ABI/生成模块，再推进混合
producer、必要 native/network、最终语言/平台/发布验收及旧路径退役；不能将
本次原生 foundation 或保守 BUSY 边界当作长期任务完成。
证据入口：`build/tmp/xlir_launch_v196_evidence/results.json`。

## v197：任务池拥有 ABI 上层接入与两代宿主协议完整验证

固定 v196 已验证 XRT 源树及其四份生成头，使用原同步器更新 DEMO6 嵌入头、
符号表和能力矩阵，不重建发布 CLI/DLL/VFS。XL6_TaskPool owner、accepted
frame、native entry 和 activity 改为实际持有引用；惰性 native 创建和等待不
包在 ownership scope 内。显式 IR 契约由 lowering/verifier 独立计算，生成
模块 V3 不再预先关闭/排空任务池，图中保留真实 wrapper/native/Job 边。

原 29 门全部保留，新增 task-pool-graph。发现批次 3/6 后按根因修正，首轮
完整 27/30、393.220 秒；三个失败是 Future 宿主旧 resolver、模块 V2 接线和
手工 frame 缺真实 pool 的 outcome 白盒崩溃。原失败及源版本保存后成组修正。
V2 的 18 模式乘两优化级别完整保留，并追加同人口 V3，共 72 通道、1800 次
真实 TCC 生命周期。test-only V2 adapter 不进入生产 Cgen。聚焦阶段另有参数
数组展开导致的编译失败，原始脚本通过已记录 SHA256 校验的精确版本恢复。

最终完整 30/30、0 未运行、440.800 秒、峰值三，4171 输入前后指纹一致：
`513EA602AD978B9A976E386838E69D98A223814CFD1D33B66D305FD5B7FA914A`。
本批实际两轮完整验证，不能报告一次全绿。33 条任务存储通道、全部旧门人口
逐项核对，原 200 活动事务保留并追加 200 真正休眠 queued 图；200 退休图采用
实际持有 bundle，不省略新 pool 边。原根延期、outcome、class finalizer、
queued BUSY 和跨线程 freeze 人口保持。新三通道各 280 生命周期/240 queued
图/520 Prepare，覆盖真实 native join 和两种 Finish 顺序。

源码 public 68 TU/16.076 秒/67 hit，34 合同共享 23 TU/14.707 秒/23 hit。
十个既有生产文件变化、无新增，九个旧测试修改、三个新增；Cgen LF -1008
到 5085928。初次工程检查的能力矩阵过期失败保留，同步后 12 项通过；6562
文本编码检查通过。v196 原生百程序只在 2005 原输入和 2804 副本重核后复用，
不重复声称新跑；最终文档后复核使用原 InputSnapshot 算法。

这关闭语言任务池 owning ABI 的接入缺口，不代表混合 producer/native/network
及所有联合卸载已闭环。后续继续这些生命周期和同源码 CLI JIT/AOT、完整语言/
平台/发布验收、旧路径退役，不用专项绿色替代长期目标。
证据入口：`build/tmp/xlir_launch_v197_evidence/results.json`。

## v198：混合等待证明移入共享运行时，保持全部旧验证人口

以冻结的 v197 为基线，提取只读、单输入的 output/source 依赖契约。
continuation/callback 各自验证实际布局、稳定状态和同一图的 claim；共享检测
核对真实 output 身份，不把 capture 当作 wait，不跳过正常回调，不取消 feeder
或就绪工作。Cgen 与 IR 完全不变，多输入 all/race/whenAny 另需明确完成规则。

新联合模块同时使用真实 native task pool、生成 async 和四级 Future callback。
修正前四个 TCC/DLL 与宿主 O0/O2 通道均 build=0/run=3，在实际卸载等待处失败；
修正后发现批次 5/5、176.671 秒。C/header/JSON 修正前后相同，保留原失败记录。
初次用例错误 throw error 和空日志导致的脚本异常单独保留，不混作产品回归。

首轮完整 27/31、418.138 秒，发现两个旧 Get-Item 列表分隔错误、一个新对象
重复链接和新增混合宿主对非阻塞冻结准入的过强假设。批次结束后统一修正。
新增可观测宿主包装只重试生成预检尚未被调用的准入竞争，实际 callback 必须
READY，BUSY/FAILED 均不重试；原宿主默认路径不变。最终实测一次入口延期、
528 个 READY callback。两个 producer 实现与共享检测在对应 O0/O2 单独编译。

最终完整 repaired 31/31、423.922 秒、0 未运行、峰值三。实际两轮完整验证，
原 30 门和全部旧人口逐项比较。4177 输入前后均为
`24F4E46C2458260CAB969046099792C5757514B8CA2CDB31CBCC27A9311F1A7A`。
新四通道各 132 生命周期/60 环/20 feeder/20 无环/112 根保留/40 cleanup-await
根/548 callback finally/32 OOM 预算，合计 528 真实卸载、2192 finally，最终
各有 16/14/16/14 次真实分配失败。catch/finally、全局析构和内存平衡未放宽。

三个旧生产文件变化、新增两文件；一个旧宿主增加可选钩子、新增三个测试及
一专项。源列表机械接入共享实现。public 69 TU/17.417 秒/65 hit、34 合同
共享 23 TU/14.529 秒/23 hit；IR/Cgen 哈希不变，Cgen 5085928 LF 字节。
13 项工程检查、6568 文本编码及最终文档后的输入重核；原生百程序只在 2005
原输入和 2804 副本重新校验后复用。嵌入 XRT、CLI/DLL/VFS 均未更新。

继续多输入/native/network 和其他联合卸载，再做同源码 CLI JIT/AOT、完整
语言/平台/发布验收和旧路径退役；本批专项闭合不等同长期目标完成。
证据入口：`build/tmp/xlir_launch_v198_evidence/results.json`。

## v199：原生拥有型取消观察器前置合同

多输入组合器尚有借用 CancelWatch 和 operation 外部根。本批推进其共享
前置合同：成功注册接管一个真实 Data 引用，明确 Notify/Drop、真实派发
pin、注销等待和 Future 取消入口的 mutation 边界；不在 Cgen 或单输入
Floyd 算法上追加多输入特例。新 Watch/Cancel 查询均强制返回语义准备，
Prepare 不取消或摘除 accepted observer，等实际 owner Unwatch 后再清理。

固定原生副本来自 v197；仅覆盖 cancel.h、cancel.c、future.c、新增测试及
单个测试模块，其余原生版本保留。发现批次 GCC 0/14（严格缩进告警）、
TCC 14/14；第一次完整 GCC 65/104（新增单头宏重定义）、TCC 104/104。
所有输出、对应源和已运行二进制保留，不能把未执行项当通过。修正测试
模块宏保护、补齐输入准入负例后，最后一次完整 GCC/TCC 都是 104/104，
分别 110.618/27.234 秒，2008 输入前后不变。原 100 程序逐项保留，不用
新专项替换。额外 GCC O2 四个新增程序 4/4、32.344 秒；实际两轮完整。

每个 ownership 进程：320 生命周期、192 真正拥有环事务、320 Future
取消/关闭/父取消入口、32 OOM 预算；每个 phases 进程：64 拥有 callback、
64 旧式 callback、64 Drop 尾部、256 父/子/注销竞争。图计数包括重复强
槽、真实 request/dispatch/plan pin；旧式 callback 仍拒绝 freeze，拥有型
callback/Drop 和真实等待中的 Unwatch 不持有 API 自己的 mutation。
自己持有的外层 scope 不被暂停，引用与准确内存平衡均有断言。

工程七检查通过，核心 ABI 两用例、构建/范围/文档 54 单测不减，取消文档
15 函数/3 类型零遗漏，2564 输入不变。初次隔离副本缺少完整扩展清单和
工作流的四个工具失败保留；同一完整工具人口在实际原生仓重跑通过。

DEMO6 所有源码/工具/原测试、IR/Cgen、嵌入头和发布文件保持原版本。
v198 的 31/31 是输入重新核对后的沿用结果，本批没有重跑语言专项，也没有
宣称组合器或语言模块已经采用新取消合同。下一批必须接通实际 multi-input
producer、all/any 完成条件、代码拥有与真实卸载，不能以本批底层通过结项。
证据入口：`build/tmp/xlir_launch_v199_evidence/results.json`。

## v200：原生多输入物理组与独立全人口批量验证

同一原生引擎覆盖 All/Any/Race、旧 traced mapper 及新 owned policy mapper。
输出 producer、源 Watch 投影、拥有型取消 Data 都是实际引用；组与所有输入
槽仍一次分配，公开 Watch 仍 64 字节。源重复槽保留，pending operation base
不再是外部根。原生结果实际持有组及源，终态取消 Watch 真正注销。

新 V4 身份表不扩张旧准入边界。组必须稳定且映射策略精确已知，数据和代码
独立拥有；未完成 Prepare 只 BUSY，不替源取消。等待视图保存真实 output、
有序全输入和 all/any 规则，既不挑一个 child，也不授予死锁取消权限。新认证
回调和 Drop 自行协调拥有转换，旧回调隔离保留。

发现阶段保留旧图预期失败：mapped pending 从 9/19/1 变为 9/21/0，raw
pending 从 8/17/1 变为 8/19/0，真实 producer/取消 Data 边替代 operation 根。
raw terminal 因真实注销 Watch 从 10/12 变为 9/10。测试按实际拥有图更新，
原功能、别名、重复输入、OOM 和析构人口保留。扩展 GCC/TCC 25/25。

验证器改为先编译整个人口，再独立执行每个程序收集全部失败，不因首个运行
断言中止。辅助脚本的 Windows PowerShell 5 空 ExitCode、并行链接输出拼接
解析错误均保留日志/源/已编译二进制；改用 PowerShell 7 和按目标提取后完成
全量收集。首轮 GCC/TCC 各实际运行 110/110、通过 108/110：新增并发测试
错误要求取消请求必成功。按实际 Future 契约修正为“已完成才可无错拒绝”，
保留全部竞争，第二轮两编译器 110/110，109.322/27.034 秒。

新六程序独立 O2 首次链接失败，发现新测试模块漏声明 atomic，不能依赖全套
其他模块偶然带入。补齐清单后 O2 6/6，38.749 秒；最后按相同修正输入完整
复验，GCC/TCC 均 110/110，分别 108.591/27.382 秒，2014 输入稳定。实际三轮
完整运行（108/110、110/110、110/110）；原 104 程序及原断言保留。

每个新 ownership 程序 576 生命周期、128 终态环事务、96 OOM 预算（24 实际
失败）；projected Watch 程序 256 接管/64 未知策略拒绝；phases 程序 96 阻塞
映射/96 Drop 尾部/384 完成取消竞争。全部检查准确内存数量、字节与引用平衡。
DEMO6 的 IR/Cgen、嵌入头及发布物未更新；语言结果在最终核对原 InputSnapshot
后沿用 v198 31/31，不能将原生组合器单测冒充语言多输入或发布验收。七项工程
检查全部通过，公开 ABI 两用例及原 54 工具单测不減；Future API 63 函数、
10 常量、23 类型零遗漏。下一阶段接入真实 mapper code owner、宿主准入和
共享 all/any 等待求解，保持核心语言/平台/发布验收及旧路径退役的完整目标。
证据入口：`build/tmp/xlir_launch_v200_evidence/results.json`。

## v201：聚合 mapper 代码拥有接入与 33 项统一复验

先验证 v200 原生证据输入和隔离单头产物，再用现有同步器嵌入 DEMO6。
发布 xl.dll/xlc.dll/xl.exe 保持不变。六种聚合 RuntimeOp 通过显式、独立验证
的模块 ABI 调用一个常驻桥接；实际 mapper context 拥有真实 code lease，
CodeScope 保护生成 boxer，宿主分别接纳物理组、投影 Watch、取消监听和代码。
数组拆包从生成 C 移出，不增加第二个原生聚合器或在 Cgen 推断等待语义。

开发期编译发现缺少 stdlib 声明和 xruntime Future 载体声明，一并修正为
声明头依赖。专项保留 3500 原事务、192 OOM 预算和 36 原 IR 变异；仅因
真实 mapper/取消节点更新 pending 图为 10/21/0，因三类桥接的实际上下文
分配更新准备失败次数 34→40。所有结果、别名、取消、错误与内存断言不减。
新增 9 个模块 ABI 变异、六操作独立派生和 text/JSON 检查。

新增代码拥有专项在 GCC O0/O2、TCC 各执行 100 真实 DLL 父/子生命周期、
100 Close 后跨线程接纳拒绝、500 次 scoped boxer、200 条精确物理代码链、
300 个完整准入图、100 个旧裸 boxer 拒绝和 128 owned mapper OOM 预算。
最后代码引用归还后核实实际解除映射；不把仍常驻的模拟地址当作卸载。

原 31 项保留，再加两项聚合门组成 33 项固定批次。首轮全执行 32/33，
对象逃逸宿主三个链接变体漏编新桥接，O0 依赖完整的四模式已通过；保留
失败日志与原 28 模式，不删除失败路径。工程检查另发现接入后的能力矩阵
过期，按生成器同步。集中修复后只做一轮完整复验：33/33，464.465 秒，
4182 输入前后相同；13 项工程检查也全部通过。源码构建 69 TU，契约驱动
从原 34 增为 35，共享 23 TU 对象缓存，验证构建缓存命中 23/23。

Cgen 减少 5789 LF 字节至 5080139。新私有头在 Windows/Linux 两产品的
72 映射与现有 72 源位置静态检查通过，未重建发布物，也未宣称 Linux 执行。
下一步仍是共享 all/any 等待求解及生成聚合模块联合卸载，再继续必要 native/
network、完整语言、CLI JIT/AOT、平台/发布与旧路径退役；不能以本批 33/33
替代这些目标。证据入口：`build/tmp/xlir_launch_v201_evidence/results.json`。

## v202：多输入等待证明、生成模块退休与下一批产物发现

原 33 项专项和 35 个契约驱动全部保留，增加共享 AND/OR 求解器独立对照与
真实生成聚合模块专项。三路算法测试各覆盖 66064 个图、197648 个根判定；
四路生成模块各 170 次真实卸载、704 次回调 finally，以及 18 次 Close 后的
成功/失败/取消完成。172 个原生视图逐项核对全部有序输入、重复/完成槽和
ALL/ANY/Race 规则。证明资源不足保留诊断，实际 producer 只 BUSY；完整图
不可达、独立准入和真实队列准备仍是前提，不能通过跳过回调或模拟引用验收。

发现期先运行独立算法及模块小批，保留新增样例的逻辑运算符、const、缩进
编译错误。模块探针还暴露测试宿主漏做实际队列准备，以及原生池停驻转换中
的暂时准入拒绝；核实真实协议后，前者调用实际已准入的准备入口，后者只
限时重建整个快照，不将未知节点当作叶子或许可取消。源码版本与失败日志
独立保存，生产求解器没有为测试降低判定标准。

完整批次实际是三轮，不写成一轮：首次 35/35、485.085 秒；补强原生输入事实
和关闭后完成路径后 35/35、488.631 秒；工程静态守卫仍要求旧单输入名字，
更新为多输入/只读/资源失败边界后，最终 35/35、489.185 秒，13 项工程检查
全通过。每次集中运行期间输入不改；最终 4188 输入前后指纹一致。后续应把
工程静态守卫也提前纳入轻量发现，以免语言复验后再因工具合同过期重验。

编译器构建 69 TU 只重编 3 个受影响对象，耗时 16.117 秒；35 个契约驱动
共享 23 个缓存命中对象，耗时 14.576 秒。Cgen 仍是 5080139 LF 字节；没有
增加生成端等待图分支。本批未修改独立 XRT、嵌入头或发布物，原生历史证据
经 2014 输入复核后沿用，不能冒充本轮原生全量执行。

等待复验期间已独立建立下一批源码 CLI 探针，未修改生产输入。五个样例在
明确 provider 和源码部署后 Cgen/JIT 共 10 项通过；AOT 五项暴露常驻运行时
选源缺口，五个 exe 未产生/未执行。按同一根因归为独立产物依赖合同，下一批
统一处理 Slot/Closure/CodeLease 及其真实传递依赖，不逐符号加特判。
此发现不改变本批专项验收范围，也不将开发目录 CLI 视为发布/平台验收。
证据入口：`build/tmp/xlir_launch_v202_evidence/results.json`；CLI 失败保留于
`build/tmp/xlir_launch_v202_cli_staged/results.json`。

### 2026-09-14 v203：独立产物依赖合同，集中发现后一次完整复测

保留 v202 的全部五个 AOT 失败样例、35 个门及原契约驱动人口。新增常驻
依赖计划从已验证 Lowered XLIR、物理 ABI 和精确链接身份产生，不输入 AST
或生成 C 文本。23 个组件闭包同时给出常驻源码和原生根模块，172 个导出
由真实对象核对。产物入口的错误作用域单独建模，UTF-8 的原生要求由明确
RuntimeOp 描述，未向 Cgen 增加补丁。

集中发现过程记录为：原五个 AOT 从 5 项失败收敛到 1 项；补齐入口合同后
原五个全部通过。扩展到 14 个语言程序又发现 1 项原生根缺项；修复后 56 项
Cgen/JIT/AOT/EXE 全通过。追加 529 个依赖双根对照、Typed/损坏 IR/入口拒绝、
伪前缀反例和 9 个分配预算（8 失败、1 成功，精确内存平衡）。工程静态检查
提前执行并已通过，未因旧工具合同而追加完整批次。

最终仅运行一次包含全部原 35 项的 36 项批次：36/36，0 失败，0 未运行，
495.194 秒。4193 输入前后相同，指纹为
`F90279AE54E01854E865CE126FD8BD90AF01086D540D84EBEC5028298FB70D14`。
五个原始样例的 C 逐字节不变；Cgen 仍为 5080139 LF 字节。发布 DLL/EXE、
嵌入头和独立 XRT 均未修改，原生历史证据只经输入重核沿用。

完整原生能力裁剪的验证后需求清单、native/network 混合生命周期、全语言、
共享发布/隔离部署/平台和旧路径退役仍开放。不能将 14 个开发目录程序或
本批 36 个门当作长期目标已经完成。设计见 `docs/XLIR独立产物依赖合同.md`，
证据入口 `build/tmp/xlir_launch_v203_evidence/verified_results.json`。

### 2026-09-14 v204：原生链接目录，保留原人口的一轮统一复测

先从全部公共声明和真实预处理定义集中发现原生模块边界。普通文件归属
不足以覆盖无独立源码的附加模块，单纯 XRT_API 正则也漏掉括号保护和不重复
注解的定义；最终用真实功能闭包、顶层定义及来源记录建立 3484 项目录。
402 个模块根、无选择核心及全产品共 404 个预处理记录全部保留并可复核。
这些是链接提供者证明，不代表额外实现了 3484 项语言 API。

新程序在原 v203 CLI 上 Cgen/JIT 通过但 AOT 缺 xrtEncodingUnitSize。按目录
将 CHARSET 根加入可达外部调用的产物需求，原生边界七项、全部实际对象导出、
精确/近似名字查询通过；原 14 个程序加新程序的 60 项 Cgen/JIT/AOT/EXE 全通过。
发现期的 PowerShell 单元素数组断言错误保留原日志和精确源版本后集中修复，
没有修改语言断言或删除旧用例。新程序生成 C 另通过 GCC -Werror 编译。

仅进行一次完整 37 项复测：37/37，0 失败，0 未运行，496.516 秒。全部原
36 个门和 35 个契约驱动保留；4196 输入前后指纹均为
`19F08044D6BA351BC6EC6AF8B21B14489BC1B571FC56EC0AB75C9DE037D71C81`。
Python 生成器、解析反例、原生清单和预处理缓存记录另有最终哈希复核，不能
因原批次扩展名过滤不包括 Python 而遗漏。工程检查 13/13；原 14 份生成 C
逐字节不变，Cgen 的 5080139 LF 字节也不变。开发 CLI 新增链接元数据使文件
增加 143521 字节，未更新发布物，不把本批描述成二进制减小。

后续仍须为全部 RuntimeOp/生成桥接补齐原生需求，移除 XLIR 产物路径的
历史 AST/生成 C 扫描；混合生命周期、完整语言、共享发布/隔离部署/平台、
旧路径退役继续开放。证据入口 `build/tmp/xlir_launch_v204_evidence/results.json`。

### 2026-09-14 v205：实现需求、C ABI 与求值生命周期的集中收敛

本批在修改前保存 v204 输入快照，发现和修复阶段另存精确输入版本。先将
244 个 C 实现需求接入 54 个原生依赖组，令 XLIR 的 C 输出与产物计划使用
同一验证后选择结果。产物公开 ABI、包级完整产品要求分别显式声明，XLIR
装配不再输入 AST/生成 C 文本，旧扫描仅留在 legacy 分支。

独立产物首次 16 程序、64 阶段发现 4 个 AOT 缺少数字格式化模块，另有 4 个
对应 EXE 未生成的执行失败。扩展严格 C/原生对象检查到 46 个适配器又发现
31 项失败；按实现依赖、原生 ABI 和生命周期三组共同根因处理，不逐条运行
完整总回归。修复后增为 47 个适配器、只剩持久化临时字符串装箱失败的清理
缺口。补齐统一求值事务并增加正反运行探针后，48 个适配器全部通过。
新增用例是增加覆盖，不能把跨不同规模发现批次的数字冒称同一失败率。

12 个路径函数的 char* 结果进入显式 IR ABI，32 项错误签名反例拒绝；共享
HTTP/WebSocket 体由模板参数选择正确后端返回 ABI。装箱源和后续参数求值
期间的临时结果进入已有所有权事务，未放宽 verifier 或修改语义断言。
真实运行 192 项：64 次分配失败、64 次后续参数抛错、64 次成功，逐项检查
活跃数量/字节及分配释放精确平衡。其余网络程序只编译，不计为网络运行验收。

13 项工程检查在完整批次前已通过。17 个独立程序的 68 项 Cgen/JIT/AOT/EXE
与所选原生对象闭包通过，真实 AOT 实现和计划逐字节一致。原 15 份生成 C
仍与 v204 完全相同。保留全部原 37 门、35 个契约驱动，增加原生适配器门后
仅启动一次 38 项完整复测：38/38，0 失败、0 未运行，823.12 秒。4206 输入
前后指纹均为 `EF315BD37818DBA098FD54459B537F58C2252B5BE7BBADAB8872D62B0BE73DE4`。
最终另核对原生目录的 Python 输入与 404 份预处理记录，以及全部旧门的用例
覆盖和循环预算；不只依据总通过数判断没有退化。

Cgen .c/.h 的 5075908 LF 字节小于旧值是因为字段移入 .inc；包含描述文件
后为 5096095 字节，实际增加 15956，不能宣传为缩减。开发 CLI 增加 37645
字节，发布物未改。当前源码构建 69 TU、68 命中，17.166 秒；契约驱动共享
23 TU、19 命中，255.32 秒。独立 XRT、嵌入/声明头和发布 DLL/EXE 未修改，
原生 2014 输入与旧 110/110 证据仅重核沿用，不冒称重新执行。

仍开放语言/IR 的完整边界与通用混合生命周期、更多 FFI 物理签名、共享发布、
隔离部署、平台及 legacy 退役；非核心专属协议功能按标准库最低基线单列。
证据入口 `build/tmp/xlir_launch_v205_evidence/results.json`。

### 2026-09-14 v206：网络命令生命周期与错误调用，统一一轮验收

保留原 5 个网络程序，先校正开发磁盘工具链的完整提供者配置，再运行实际
保存 AOT 的同进程循环。普通运行成功但 TLS 每轮 4 次非法释放的失败完整
保留，并用原生调用栈确认是内嵌事件误配 Destroy。共享 TCP/TLS 和两处
WebSocket 命令统一改用 Unit，补齐初始化后的失败清理；未修改原生 XRT。

加入 TCP peek/consume 正例后，集中发现该新探针误传 send 第二个参数，
以及语义层对未绑定 nominal class 调用的诊断缺口。保留错误探针作为发现
证据，将同类情况扩为 11 个语义负例；共同修复最终调用绑定边界，合法方法
调用组合保持可用。先完成两个轻量专项复验，期间没有重复运行整轮旧门。

最终网络 6 程序、30 项 JIT/AOT/EXE、30 次真实公开 main 生命周期、32 次
停止 worker 提交拒绝均通过，内存与事件句柄精确平衡。11 个负例在 sem
阶段拒绝且零 IR 节点，合法方法组合的严格 C 与实际运行通过。WebSocket
专属协议行为未执行，常驻代码循环不代替网络混合拥有图和动态卸载验收。

源码固定后，新增两个门与原 38 门组成唯一一轮完整 40 项复测，并行上限 3；
只读工程检查同时执行。结果 40/40、0 失败、0 未运行，635.858 秒，工程
13/13。4213 输入前后均为
`59CB9911F0FDFE077CC40D67B65593A0D52DE6756D68545F5D0EA73D6E2D75E3`。
原 35 契约驱动及旧用例覆盖保留，17 份既有独立程序生成 C 逐字节不变。

最终公共源码构建 69 TU、68 命中、16.92 秒；契约共享 23 TU、22 命中、
16.15 秒。只修改 3 个生产文件；Cgen 含 .inc 为 5096411 LF 字节，增加
316；开发 CLI 为 9639560 字节，增加 1536。发布物、4 个嵌入/声明头及
独立 XRT 不变，旧原生 110/110 仅重核 2014 输入后沿用。失败脚本、旧输入
版本和诊断栈保留；长期任务仍开放，不把本批结项等同于完整语言验收。
证据入口 `build/tmp/xlir_launch_v206_evidence/results.json`。

### 2026-09-16 v207：原生 Future 映射和代码驻留，统一一轮验收

先以 v206 真正生成的 TCP/resolver 适配器复现 pending 回调没有代码能力、
地址列表结果没有独立代码保活的问题。共同修复 verified nativeFutureMapAbi、
常驻 exclusive callback、resident payload 和 native class factory，移除
TLS 在 XLIR 路径中的 legacy result shim。保留实际失败探针、第一次严格 C
格式告警及工程检查的原版本，不将源码修复前的结果并入最终通过数。

稳定源码后保留原 40 个门，增加原生 Future 映射门，统一 41 项只运行一轮。
专项包含 25 个 IR 变异拒绝、三路取消竞争和 600 次实际 DLL 卸载；完整
批次 41/41、0 失败、0 未运行，671.113 秒，工程检查 13/13。4219 输入
前后均为 `9E7CE87321D1A16600AF5A66BB7A521DDF9A4BF894F6FE847559849BE2E7A458`。
原 35 个契约驱动和 17 份逐字节相同的生成 C 保留。工程检查修正旧身份绑定
断言、自动同步能力矩阵，并
单独登记与 v206 不同的发布 DLL 环境；不以替换历史哈希掩盖版本差异。
当前源编译器、契约驱动以及过去两份发布 DLL 都有独立可核对记录。

完整原生服务/混合拥有图、活动网络模块卸载、全语言/FFI、同源发布、隔离
部署、平台与 legacy 退役继续开放。原生 2014 输入及旧执行证据只重核沿用，
不冒称本批重新执行完整 XRT 测试。
证据入口 `build/tmp/xlir_launch_v207_evidence/results.json`。
