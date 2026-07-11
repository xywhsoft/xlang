# RC 三轮问题统一修复任务

本文是 demo6 发布候选阶段的新问题总账，只记录本轮三次审计确认的 24 项工作。
历史阶段文档不作为这些问题的实现口径。

## 固定原则

- 普通 `class` 保持 C 结构体值语义，不恢复隐式 `Derived -> Base` 按值转换。
- 继承多态统一使用显式 `ref<T>`；`ref<Base>` 保存完整动态对象，不产生基类切片。
- 同一根因必须在公共语义、公共 lowering 或公共运行时边界一次修复，不为单个范例增加兼容分支。
- XRT 能承担的通用能力优先在 XRT 实现，并保持 C 与 xlang 都可直接使用。
- 每个确认缺陷至少有行为回归；cgen 缺陷还必须检查生成 C，并覆盖 `run/build`。
- 不存在的旧语法、旧行为和临时绕过不保留兼容入口。

## 第一轮：代码审计 18 项

| 编号 | 问题 | 解决目标 | 状态 |
| --- | --- | --- | --- |
| F01 | 不存在的 `import` 被静默忽略 | 所有未解析 import 必须产生稳定诊断；区分可选内部探测与用户 import | 已关闭：用户 import 缺失必报错，并覆盖相对作用域、包与内置能力模块 |
| F02 | 中文安装路径下 package 导入失败 | 全链路使用 XRT UTF-8 文件/路径能力，移除 Win32 ANSI 路径入口 | 已关闭：导入、package 与编译路径统一使用 XRT UTF-8 路径能力 |
| F03 | 超长标识符被固定缓冲区截断并生成非法 C | 名称生成改为动态缓冲或显式长度诊断，禁止静默截断与符号碰撞 | 已关闭：名称和路径改为动态缓冲，超长标识符具备 run/build 回归 |
| F04 | `check` 接受但 cgen 拒绝部分写法 | 语义能力矩阵与后端保持一致；合法写法可生成，未支持写法在 check 阶段拒绝 | 已关闭：能力检查前移到语义层，负例诊断和 cgen 可生成集合已对齐 |
| F05 | package/XLP 读取器接受非法 JSON、损坏 Unicode | 统一使用 XRT JSON 解析器并严格验证 schema | 已关闭：改用 XRT JSON，严格验证 schema、类型、Unicode 与尾随内容 |
| F06 | Windows 异常 TLS 首次初始化竞态且未释放 slot | 使用线程安全初始化与模块卸载清理；无异常需求时不生成死重 | 已关闭：线程安全 TLS 初始化、退出清理和按需生成均已落地 |
| F07 | DLL context/module 执行与销毁存在并发生命周期竞态 | 引入执行租约、关闭状态和等待协议；错误文本使用线程稳定快照 | 已关闭：执行租约、关闭状态、延迟销毁和线程稳定错误快照已完成 |
| F08 | `task.all/race` 对 var、容器、资源和 class 结果存在类型断层 | 所有已支持的一等公民类型具备一致聚合、await、取消和所有权语义 | 已关闭：聚合结果类型具体化、所有权、失败、取消与空集合边界已统一 |
| F09 | DAP 当前仅为协议 smoke，缺少真实断点、栈帧和变量 | 完成真实调试执行模型，或在发布前明确移出正式能力口径 | 已关闭：合成 DAP 原型、命令和构建入口已移出首发版本，并有反向守门 |
| F10 | `finally` 禁止 `return/break/continue` | 实现 pending-control lowering，保证 finally 必定执行后恢复控制流 | 已关闭：同步与 async 均通过 pending-control 恢复 return/break/continue |
| F11 | 用户可见诊断包含合法 UTF-8 乱码 | 修复全部乱码并增加中文诊断内容守门，不只验证 UTF-8 字节合法性 | 已关闭：源文件与用户诊断增加 UTF-8 内容守门，已清理已知乱码 |
| F12 | 函数值命名参数、属性维度参数和部分复合赋值能力不统一 | 已设计能力走统一调用/赋值解析；明确暂缓能力保留精确诊断，不做半实现 | 已关闭：函数值命名参数明确拒绝一次，属性可变参数精确拒绝，复合赋值统一派发协议 |
| F13 | package 依赖追加失败会泄漏，路径/命名空间存在静默截断 | 事务式追加与统一动态路径缓冲，失败路径完整回滚 | 已关闭：依赖追加事务化，路径、命名空间和解析器缓冲均改为动态存储 |
| F14 | 已删除的 `custom` 与旧字面量/返回箭头仍有死重口径 | 删除不可达 IR/cgen/测试/文档残留，不保留兼容提示 | 已关闭：用户语法和实现分支已删除，历史阶段文档仅以明确历史标记保留 |
| F15 | `xl6_sem.c`、`xl_api.c` 等核心模块重新形成单体 | 按稳定所有权边界拆分独立编译单元，保持行为不变 | 已关闭：API path/symbol 与 sem docs/field attrs 已成为独立编译单元，并有尺寸/接线守门 |
| F16 | BAT 测试体积过大，并发 soak 默认强度不足 | 提取数据化守门与公共 helper，建立 fast/normal/heavy 分层及稳定 soak 基线 | 已关闭：核心、OOP、语义、async、task、generator 套件均已分片，新增 heavy soak |
| F17 | demo6 缺少 CI、Sanitizer、竞态、模糊测试和覆盖率门槛 | 建立 Windows/Linux 可执行 CI 与内存、UB、解析模糊测试守门 | 已关闭：Windows 全回归与 Linux ASan/UBSan、fuzz、覆盖率 CI 已落地；本机执行确定性 fuzz |
| F18 | 发布临时目录和发布树存在历史污染，守门只能发现新增数量 | 发布目录零临时文件，测试前后检查内容与来源，明确二进制/日志跟踪策略 | 已关闭：CLI 内存运行不落临时 exe，测试前后按内容检查并要求发布临时目录为空 |

## 第二轮：确认需要收口的 3 项

| 编号 | 问题 | 解决目标 | 状态 |
| --- | --- | --- | --- |
| S01 | `var` 与 string 拼接进入静态 string 时产生乱码 | 所有动态字符串结果在进入 string 所有者前统一检查、解箱和复制 | 已关闭：统一动态 string 解箱/复制，run/build 和所有权回归通过 |
| S02 | `encoding.toBytes(..., encoding.bom)` 曾导致堆损坏 | 保持 BOM flag 拆分和独立缓冲构造，补齐编码矩阵与 DLL/run/build 回归 | 已关闭：BOM 与编码 flag 分离，独立缓冲和编码矩阵回归通过 |
| S03 | class 继承缺少无切片多态模型 | 实现 `ref<T>` 多态引用；普通 class 继续按 C 结构体值语义工作 | 已关闭：`ref<T>` 语义、运行时、泛型 class、容器、XAPI 和 C ABI 全链路完成 |

## 第三轮：确认需要修复的 3 项

| 编号 | 问题 | 解决目标 | 状态 |
| --- | --- | --- | --- |
| T01 | func 字面量跨函数传递并循环调用会破坏 xvalue 所有权 | direct invoke 与普通调用共用参数移交规则，覆盖跨帧和复合捕获 | 已关闭：参数移交规则统一，跨函数、循环和复合捕获压力回归通过 |
| T02 | 泛型函数/class 方法的多返回值没有完整替换 `T` | 调用签名统一具体化，覆盖显式/推断泛型和全部返回项 | 已关闭：完整返回元组执行泛型替换，函数与泛型 class 方法均覆盖 |
| T03 | class 字段初始化器引用模块 const 时生成未声明 C 标识符 | 模块声明先于 class helper，初始化顺序稳定，覆盖不同 const 类型 | 已关闭：模块 const 声明先于 class helper，字段初始化回归通过 |

## ref<T> 工作包边界

`ref<T>` 的目标是提供显式、无切片、可跨函数和容器保存的多态引用，同时不改变普通 class：

- `ref<Derived>` 可以安全拓宽为 `ref<Base>`，共享同一个完整动态对象。
- `ref<Base>` 的成员访问和 virtual 调用基于动态对象，不复制 `.base` 前缀。
- ref 对象具有独立引用计数、动态类型描述和析构入口；引用本身可复制，底层对象只析构一次。
- C ABI 公开稳定的引用句柄和查询/retain/release API；C 可以读取动态类型并取得合法基类视图。
- `Base value = Derived()` 继续拒绝；`array<Base>` 继续保存 Base 值，不承担多态。
- 多态容器使用 `array<ref<Base>>`，返回值和参数同理。
- `ref<Base> as ref<Derived>` 进行运行时检查；不匹配时直接产生可捕获的运行时类型错误，不返回伪造的空引用。

## 衍生问题闭环

修复主问题时额外发现并关闭了两个同根因的 XRT 网络并发边界：

- async wait future 曾在回调释放内部异步持有引用前唤醒调用方，严格销毁会与回调并发；现在调用方只释放 owner ref，由内部持有引用完成延迟释放。
- Windows listener 关闭时曾遗留 IOCP bound socket 记录；系统复用同一 SOCKET 值后，新 listener 会被误判为已绑定。关闭路径现在先提交 `XNET_PORT_OP_CLOSE` 再关闭 socket。

XRT 新增 C 级 async TCP/UDP 压力测试，并已同步单头文件到 demo6。该修复不是 xlang 侧绕过。

## 最终验证证据

- `test.bat` 从清理旧产物开始连续完整通过，最终检查 429 个负例诊断产物和 1029 个文本测试产物。
- 最终 `rc0_guard.ps1 -SkipFullTests` 在重新构建全部发布产物后通过；守门当次检查 1053 个文本产物、429 个精确负例和 21 个已关闭风险。
- `03_cgen_runtime_core.bat`、`06_task_api_spawn.bat`、`07_async_generator_channel_docs.bat`、`test_dll.bat` 与 tutorial/feature suite 均通过。
- async/task heavy soak 完成 3 轮，覆盖 7 个 run 场景及 2 个 build 后执行场景。
- frontend 确定性 fuzz 完成 10000 次；Linux ASan/UBSan 与覆盖率阈值由 `.github/workflows/demo6-rc.yml` 持续守门。
- XRT async 网络独立压力测试完成 1000 轮；xlang `stdlib_net_tcp_udp.xl` 完成 300 个独立进程回归。
- cgen 共有 123 个分片，最大文件 1190 行，无文件超过 1200 行；cgenrule 索引覆盖 756 条唯一规则。
- package/XRT 符号闭包检查覆盖 182 个 package C/H 文件、100 个使用中的公开 XRT 符号和 928 个运行时导出。
- CLI 临时目录检查、发布临时目录零文件检查、源码 UTF-8、模块接线、负例诊断质量和调试器未发布守门均通过。

本机 Windows MinGW 不提供完整 ASan/UBSan runtime，因此本地没有伪造 Sanitizer 通过结论；该项以 Linux CI 为权威门禁。

## 验收顺序

1. 先关闭内存破坏、静默数据损坏和前后端不一致问题。
2. 再完成 `ref<T>`、泛型多返回和 finally/async 类型闭包。
3. 随后收口 package、DLL/TLS、调试器和旧代码。
4. 最后执行模块拆分、测试工程化、CI/Sanitizer/模糊测试和发布目录清理。
5. 全部 24 项现均具有明确的“已修复、设计行为或移出发布能力”结论，不保留模糊状态。
