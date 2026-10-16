# 线程绑定资源身份与进程别名合同

B344 修复 StringIterator、UnicodeValidator、BufferIterator 的托管创建者身份，
以及 `os.exec/run` 的过时默认超时和缺失长度参数。实现复用既有身份组件与
process 声明，不增加 Cgen 库函数体或新的 XRT 模块依赖。

## 创建者身份

旧实现仅保存操作系统线程 ID；线程结束后，该 ID 可能被复用，不能长期作为
可变资源的创建者身份。三类资源现在与 Regex 共用 `XL6_TextThreadOwner`：
创建于 XRT 托管线程时保留真实 `xthread` 引用，操作比较托管对象身份，
销毁释放引用。构造的后续分配失败也释放身份引用，不改变首次错误。

原生线程的 `Data` 是借用的回调参数，不形成拥有语言对象的传递边；因此
这些资源仍是拥有图叶节点，BufferIterator 也仍持有字节快照而非原 Buffer。
身份保留不提供跨线程修改、互斥或重入保证；最后的资源销毁可在其他线程执行。
非托管宿主线程仍须在使用期间存活，不能把该修复扩大为任意宿主线程 ID
复用安全或实际线程 join／动态库卸载保证。

新增 `check_stdlib_resource_identity_shipping.py` 使用真实 DLL／SO，不链接
运行时或编译器实现对象。每种资源由一个真实托管线程创建，OS TLS 终结回调
见证 XRT 线程入口已经返回，再释放宿主所持线程引用。检查内存调试器的
分配元数据确认身份仍存活；不解引用已释放线程，也不强制伪造线程 ID 重用。

每端十二个正向配置覆盖三种资源、两种产品和 O0／O2：创建线程正常操作、
其他线程分别拒绝 3／5／4 个入口、远程销毁保留环境错误并释放身份。
构造完整分配前缀分别为 3／1／2，均要求首个及第二个未触发点确认，
并核对完整堆账本和调用线程隔离。真实旧产品另有十二个已编译对照，
均因缺少身份保留返回专用失败码，清理后账本仍平衡。

这些对照证明旧产品缺少身份保留，不声称已经实测出 OS ID 重用攻击。
专项作为 `resource-identity` 进入产品必需门；复用报告必须重新核对同次
构建、当前产品及旧产品对照的输入和产物指纹。

复测入口（旧产品参数成对提供）：

```text
python tools/check_stdlib_resource_identity_shipping.py --root <frozen-demo6> --build-report <ordinary-build.json> --baseline-root <old-frozen-demo6> --baseline-build-report <old-ordinary-build.json> --out <fresh-directory>
```

Linux 另对实际选取的 XRT 实现、三份驻留单元及其真实原生句柄／错误域
依赖运行 ASan、UBSan 和泄漏检查，三类完整构造前缀通过。
这不是未插桩共享库、JIT、全部 XRT 或完整 53 模块的 sanitizer 证明。
两次初始测试装配的链接失败记录保留：第一次错误地将声明预包含用于
实现单元，第二次遗漏真实 ONCE 依赖；最终没有使用测试替身绕过依赖。

## 进程别名

`os.exec/run(command, timeoutMs=-1, size=-1)` 现在把三个参数直接交给既有
`process.exec/run`；不再用过时的默认零超时。`size` 遵循已批准的字节前缀
规则，超出实际长度截断，小于 -1 拒绝；有界前缀可避开尾部 NUL，未经
选择的完整含 NUL 命令仍被 checked 拒绝。语义不引入第二套进程实现。

新增语言用例验证默认等待、显式无限等待、零超时、非法参数及 NUL 前缀。
旧产品单独对照验证默认零超时缺陷，不作为新实现验收。
默认 exec 的子进程延迟一秒才输出，超过既有 250 ms 停止宽限期，避免
错误地假定“超时必定没有输出”。早期短延迟对照中输出在宽限期内被收集，
不能据此推断 Windows 进程实现错误；原始日志保留。

## 现行说明与证据边界

主标准库说明同步已经落地的双字段 string、长度选择、拥有返回、
array<char>／bytes、越界错误、ASCII 大小写以及 iterator 合同。
不把历史临时文本区域、错误数组类型和旧桥接名继续写作当前接口。
说明检查器按实际声明核对 string 方法清单，并新增八种错误文档反例，
合计十五项控制通过。历史批次文档仍作为当时事实保留，不改写旧结论。

最终产品由固定生产输入分别完成 Windows／Linux 的普通完整构建，未同步
外部 XRT 的新时间模型，也没有吸收工作区中的无关编译器、扩展库或 IDE 变更。
别名用例在构建冻结后只调整等待时间，通过显式 fixture-source 记录其确切
断言字节；生产源没有随之变化。全产品、独立 AOT、严格 C 与裁剪使用同批产品。

两端完整清单均为 328 项。JIT／模块每端 319 项执行通过、一项预期拒绝，
七项宿主条件与一项平台条件保留；AOT 每端 318 项执行通过、一项预期拒绝，
八项宿主条件与一项平台条件保留。产品必需子门 Windows 22／Linux 24 项
全部通过。严格 C 包括条件夹具，每端生成 654 个 O0／O2 对象；六项已编译
错误控制按预期拒绝。没有把条件夹具的编译通过冒称为相应运行验收。

主报告目录为 `D:/xlang-validation/stdlib53-b344r3-` 和
`/var/tmp/xlang-stdlib53-b344r3-`，分别追加 `products-win/results.json`、
`aot-win/results.json`、`strict-win/results.json` 与对应 `-linux` 路径。
身份专项为 `identity-win/results.json`、`identity-linux/results.json`；
源码 sanitizer 为 `/var/tmp/xlang-stdlib53-b344r3-identity-sanitizer-r4/results.json`。
专项报告保留准确输入、实际产品、命令、日志和反例，不以旧产品证明新实现。

两端原生裁剪均通过：全部 53 模块映射到 48 个正向配置，每端 106 个
O0／O2 真实原生对象及六项拒绝控制通过。报告为上述目录下的
`pruning-win/results.json` 和 `pruning-linux/results.json`，不复用 B339 旧产品结果。

最终只读证书 `/var/tmp/xlang-stdlib53-b344-certificate.json` 已通过：复核
62,262 项关联文件指纹，并将 699 项公开签名（142 原生、228 静态、329 类
成员）关联到同一断言源码的双端 JIT／模块及独立 AOT 通过记录。
十一份进程语言故障夹具在新产品上重新运行，每端 44 配置、七项拒绝控制
通过；本次实际完整前缀计数为 Windows 5,864／Linux 5,644，不挪用 B343
旧产品的计数或报告。全部证据保留各自的插桩、平台和生命周期证明范围。

Windows DST 专用环境及完整 53 模块逐项完成审计继续；CRT 0、XRT 19 组
未决策和对象池最后不变。本批验证未执行发布目录提升或推送。
