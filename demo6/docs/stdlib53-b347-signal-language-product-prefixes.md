# Signal 语言故障前缀的真实产品验收

八份现有 signal 语言夹具已使用真实编译器生成的 C，链接 Windows／Linux
实际运行库及编译库，在 O0／O2 下完成分配故障前缀验证。每端 32 个组合、
2,528 个实际故障点与九项反向控制通过。生产实现、公开 API 与 XRT 快照不变。

## 回调与资源合同

原语言断言保持不变，覆盖基础查询和错误、SignalSubscription／SignalEvent／
SignalHandle、注册线程显式派发、无参和带事件回调、once、保存事件的独立寿命、
回调中关闭、拒绝重入、原异常传播、通道背压及关闭、任务组取消路由。

测试宿主在模块初始化前配置当前线程的一次性逻辑故障，检查初始化结果、
错误类型、模块终结、重复终结及 signal dispatcher 清理。完整堆账本检查分配／
释放、live 数量和字节、invalid free、double free、use after free；预算零
允许在第一次资源取得前失败，但必须确实触发注入。成功运行必须发生实际分配。

每条前缀逐点执行，直到第一个未触发点，并另确认下一个未触发点。其他线程
不注入故障。此处是链接实际共享产品的生成 C 验收，普通 JIT 与独立裁剪 AOT
的运行结果仍由各自产品清单记录，不把本项扩大为其 OOM 或 OS join 证明。

## 原异常优先于诊断分配

回调寿命夹具故意抛出 RangeError，路由夹具故意抛出通道关闭 RuntimeError，
并由语言 catch 检查原消息。在已经确定回调失败后，xlang 可复制回调诊断，
XRT 可追加调用上下文；这两次可选分配失败都应保留原异常，而不是替换为
MemoryError。因此每份相关夹具各有两个故障点能够完成原有语言断言。

消费者对 `xl6ForeignCallbackFail` 和 `xrtValueInvoke` 的调用做透明观察，
每次只转发到实际产品一次，原样返回结果，不替换 provider 或分配器。
第一层要求故障在真实回调诊断入口内首次触发，并返回同一个原异常对象；
第二层要求回调返回时尚未触发、实际调用返回后才触发，返回 false 且保留
同一个原生回调诊断。类型、domain、operation、消息和源位置另有精确断言。

每个相关配置必须恰有上述两个独立见证，其他六份夹具不得出现这种降级。
每端共有 16 个原异常保留点；其余 2,512 个故障点必须以 MemoryError 失败。
不启用旧 Channel 测试的错误类型放宽宏，也不允许任意“触发后成功”。

## 编译与反向控制

消费者使用同次构建的声明头，不编入编译器、XRT 或驻留实现对象。原生成 C
字节在验收前后保持不变，导入表核对到选定的实际 DLL／SO。消费者对象的
外部定义检查覆盖强函数、弱函数、只读数据及 COFF 弱别名，禁止伪造 provider。

两种产品分别执行泄漏、关闭调试、缺失故障注入、缺失回调诊断见证的已编译
反向控制，均被真实断言拒绝；另有一个强／弱／数据 provider 对象被定义检查
拒绝，CRLF 对照同样有效。共九项，不把编译失败当成控制通过。

## 产品复测

```text
python tools/check_stdlib_signal_language_oom_shipping.py --root <frozen-demo6> --build-report <ordinary-build.json> --out <fresh-directory> --jobs 2
```

专项报告为 `D:/xlang-validation/stdlib53-b347r4-signal-win/results.json` 与
`/var/tmp/xlang-stdlib53-b347r4-signal-linux/results.json`。八份语言源与产品
冻结来源逐一比较，故障宿主及透明观察器是显式的后续测试输入。

`signal-language-oom` 已进入当前产品必需门。双端仍使用同一 328 项清单，
Windows 25／Linux 27 个必需子门通过，报告为
`D:/xlang-validation/stdlib53-b347-products-win/results.json` 与
`/var/tmp/xlang-stdlib53-b347-products-linux/results.json`。未变化的 AOT、严格 C
和裁剪证据按相同产品及断言来源复核复用，不冒称重新构建。

完整 53 模块结项审计继续。用户已决定 Windows DST 专用环境测试保留待验收，
不作为本轮环境阻塞，不修改当前宿主时区，也不将该项标成通过。旧 signal 白盒中的
饱和和精确 recorder 竞态仍是独立的源码证据，不由本项替代。CRT 无未决策项，
XRT 其余 19 组未决策及对象池最后的顺序不变；本批不提升发布目录或推送。
