# 标准库模块定位与当前产品裁剪

系统、文本和容器共 53 个模块的原生定位，已在当前固定产品上集中复核。
48 个独立配置覆盖这些模块的公开能力、typed 等价实现、聚合接口、内部依赖
和不重复公开的旧原始容器。该结果补齐早期裁剪证据与当前产品之间的空档，
不等于全部功能边界或整个长期任务已结项。

## 模块定位

| 定位 | 模块数 | 合同 |
| --- | ---: | --- |
| 直接公开能力 | 34 | 有对应驻留实现、原生 feature 和实际符号 |
| typed 等价能力 | 11 | 复用 xruntime typed 存储，不要求再带入旧指针容器 |
| 类型安全格式化 | 1 | string.format 与插值复用数字格式化；不导出 C printf 形参模型 |
| 聚合入口 | 1 | regex 由被使用的 core／match／replace／split／set 提供方组成 |
| 内部依赖 | 3 | ptr_array、avl、int_map 不重复公开为语言类型 |
| 不重复公开的旧存储 | 3 | ptr_fixed_stack、block_stack、ptr_stack 不充当语言 Stack 的存储 |

逐模块映射和独立期望在 `tools/stdlib53_pruning_contract.py`。检查器核对
映射名称与原任务的 53 项完全一致，不以少量常用模块替代完整范围。
WaitQueue 另有已批准的 MPMC 配置。一个模块可以对应多个配置，一个配置
也可以证明多个共用底座的模块，不能把配置数误读成模块数。

## 当前产品与检查对象

每个配置通过真正的 compiler DLL／SO、公开 XLC API 和内嵌 VFS 生成独立
程序，并保存编译器实际使用的原生 C。目录没有 src／lib／res／include
兜底，也没有替换 provider 或拼装编译器源码对象。

对应源码必须与同一普通构建的完整产品、独立 AOT 报告一致。检查同时核对
原输入与命令实际编译的 staged 源文件字节，以及已执行用例的退出、日志和
AOT 程序。冻结路径和更新断言的路径可以不同，源字节不能不同。
这些执行回执证明用例行为；新编译的检查对象不冒充运行中的产品。

保存的原生输入经 GCC O0／O2 编译，以预处理后的 feature 和实际对象定义
核对需要的实现与无关家族的排除。Linux 另从真实 SO 读取 lowering 后的
驻留组件计划，核对必须存在的源组件。Windows 不用源码替代这个内部观察。
相同保存输入可复用同轮检查对象，检查对象不回注实际程序。

可执行入口的共同错误／值转换依赖仍可能包含 time_text、XLON、console
或内部容器；不能把这些真实依赖当成可任意删除的库。检查的是既定模块与
组件合同，不是声称函数级最小化，也不从已剥离的 TCC 程序推断完整符号表。

## 反例与结果

五个反例实际加入并编译 ptr_stack、block_stack、ptr_fixed_stack、regex_match
或 process_run。对象中须有相应定义，再由同一判定拒绝，不能以编译失败或
源码字符串充当反例。另用缺少 regex_core 的有效基线对象检查必需实现缺失。

两端各 48 个正向配置、6 个反例通过。每端 53 份不同原生输入分别生成 O0／O2
对象，共 106 个实际对象；其中包含五份保留代码的反例输入，不是 53 模块各
生成一个对象。Windows 315、Linux 364 个阶段通过，全部输入指纹保持一致。

独立验收入口为 `check_stdlib53_pruning_shipping.py --root <frozen-demo6>
--build-report <ordinary-build.json> --inventory-report <complete-products.json>
--aot-report <complete-aot.json> --out <fresh-directory>`。它是完整完成审计中
位于产品／AOT 执行之后的裁剪阶段，不与尚未完成的产品汇总形成循环依赖。

报告在 `D:/xlang-validation/stdlib53-b339-pruning-win/results.json` 和
`/var/tmp/xlang-stdlib53-b339-pruning-linux/results.json`；运行产品与源码回执
来自相同 B336 普通构建及 B338 完整执行报告，没有升级 release 或改变生产实现。

最终只读复核证书为 `/var/tmp/xlang-stdlib53-b339-certificate.json`，
重新核对两端裁剪与完整产品／AOT 报告关联的 34,324 项文件指纹。
证书明确记录原生模块定位通过、完整 53 模块功能结项未宣告，二者不可混用。

## 仍需保留的边界

Windows 当前宿主为无 DST 的 China Standard Time。当地时间使用系统 WinAPI，
不会读取 Linux 式进程 IANA TZ；gap／fold 的 Windows 原生实测仍需合适环境，
不能把 Linux 的 America/New_York 通过结果算作 Windows 通过。

本轮使用仓内清单固定的 XRT／xruntime 快照及已批准的微秒时间合同。外部
XRT 仓库已改变时间单位和表示；同步该版本会涉及语言 time、动态值和 ABI，
不能混入裁剪验收或自动改成整数强转。当前生产实现与外部 HEAD 不是同一版本。

完整功能和平台完成审计继续；CRT 未决策 0、XRT 未决策 19 组、对象池最后
不变。没有新增公开函数、参数修饰或未批准的库。
