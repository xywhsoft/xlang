# xlang demo6 cgen 规则对照表

本目录记录 demo6 当前代码生成器的生成规则。行号以 `demo6/src/cgen` 下各分片文件自身行号为准，不以 `xl6_cgen.c` 合并后的翻译单元行号为准。

## 文件说明

- `_function_index.generated.md`：机器扫描生成的 `xl6Cgen*` 函数索引，用来防止遗漏 cgen 分片中的函数。
- `00_文件覆盖索引.md`：按文件列出 cgen 分片职责，并标注哪些文件直接生成 C、哪些文件只做辅助决策。
- `01_入口与声明规则.md`：整体生成流程、函数、extern/export、main、class/custom/union 等规则。
- `02_表达式规则.md`：字面量、名称、调用、成员、索引、运算符、容器等表达式规则。
- `03_语句与控制流规则.md`：声明、赋值、return、if/while/for、异常、yield 等语句规则。
- `04_运行时与高级规则.md`：func 值、闭包、xcall、async、thread、channel、generator、标准库运行时注入等高级能力总览；不计入唯一 `CGEN-*` 规则数。
- `05_继续细化清单.md`：当前已完成区域、验证矩阵状态、剩余非阻断增强项和维护准则。
- `06_P0_参数与动态值细则.md`：参数、命名参数、默认参数、`...`/`***`、xvalue 装箱、动态运算细则。
- `07_P0_索引与赋值细则.md`：索引读取/写入、typed container 赋值、动态成员赋值、复合赋值细则。
- `08_P0_控制流与循环细则.md`：if/while/do、range、动态容器遍历、typed container 遍历、generator 遍历、break/continue 清理细则。
- `09_P0_函数调用与callable细则.md`：普通调用、重载/泛型、function/var 调用、匿名函数、XCall、wrapper、签名和多返回值细则。
- `10_P1_异步线程通道同步细则.md`：async/await、future、channel、mutex/sem/cond/rwlock、thread 生成和运行时 helper 细则。
- `11_P1_generator与yield细则.md`：generator runtime、frame、worker、`yield`、generator 函数生成和消费端细则。
- `12_P1_class_custom生命周期细则.md`：class/custom 声明布局、构造、默认初始化、成员访问、方法调用、继承路径、析构和 cleanup 细则。
- `13_P1_decl_proto与ABI元数据细则.md`：函数原型、extern/export C ABI、record/handle box runtime、函数签名 ID、多返回值结构、运行时 type desc 和 `type(expr)` 细则。
- `14_P2_标准库wrapper细则.md`：crypto/url/query/regex/process/dir/string/path/http.server 等标准库 wrapper 的触发和 C helper 生成细则。
- `15_P2_header与needs细则.md`：导出头文件生成、C ABI 类型映射、class/union 依赖排序和 runtime needs 扫描细则。
- `16_P1_声明return多返回细则.md`：声明分支优先级、typed container/xvalue/普通托管声明、语句分派、return 与多返回值声明/赋值细则。
- `17_P2_module输出顺序细则.md`：module 总装配顺序、runtime 注入、声明顺序、函数体顺序和 main 包装规则。
- `18_P2_runtime_helper细则.md`：runtime/lifecycle/main/xvalue helper 输出细则。
- `19_P2_符号命名类型映射细则.md`：C 符号 mangling、类型映射、查找和 function value 命名细则。
- `20_P2_泛型推断与运行时状态细则.md`：泛型实例收集、typed container 状态、runtime type desc 和 auto/调用类型推断细则。
- `21_P2_调用解析与默认值细则.md`：函数/方法/构造调用解析、重载匹配 cost 和默认值生成细则。
- `22_P2_类型克隆与表达式推断细则.md`：类型节点克隆、隐性 toString、await/async、容器字面量、并发构造/方法和表达式类型推断细则。
- `23_P2_cleanup与借用生命周期细则.md`：cleanup 栈、function/class/custom/xvalue/typed container/并发对象释放、借用判定和 loop target 细则。
- `24_P2_上下文状态表细则.md`：变量表、typed container 标记、上下文状态、class/union 去重、多返回签名表和 try target 细则。
- `25_P2_入口判定细则.md`：顶层运行语句筛选、xlang main 识别和入口扫描细则。
- `26_P2_XAPI基础输出与doc细则.md`：XAPI JSON 转义、类型字符串、入口元数据、基础 export/import 输出、`#doc` 解析和导出身份构造细则。
- `27_P2_XAPI模块构建与JSON输出细则.md`：XAPI module 模型、class/union 字段、export/type/import 构建和完整 JSON 输出细则。
- `28_P2_XAPI读取与模型恢复细则.md`：XAPI JSON 读取、字符串/布尔/整数解析、param/return/field/doc 恢复、模块模型重建和文件读取细则。
- `29_P2_import依赖与命名空间细则.md`：import 依赖收集、package 版本解析、私有命名空间、符号表构建和类型 token 重写细则。
- `30_P2_import声明生成与AST重写细则.md`：XAPI 声明注入、extern 函数/变量/type 声明生成、import AST 合并、命名空间访问重写和缺失诊断细则。
- `31_P2_package解析与resolver细则.md`：package manifest JSON 读取、默认身份补齐、路径/裸包名搜索、XAPI interfacePath 归一和 resolver 状态细则。
- `32_P2_CLI构建运行与产物路径细则.md`：CLI/backend 命令分派、cgen/header/xapi/dump、build/run 临时产物、TCC 链接和 C 混编 wrapper 细则。
- `33_P2_DLL_API动态执行细则.md`：xl.dll/API 上下文、libtcc 动态加载、XRT 符号注入、脚本内存运行和编译到 exe 细则。
- `34_规则测试范例交叉索引.md`：规则文档与 `demo6/test/cases`、feature suite、tutorial、DLL/CLI 测试入口的交叉覆盖索引；不计入 `CGEN-*` 规则数。
- `35_主干P0规则验证矩阵.md`：主干规则与 P0 规则共 175 条的验证矩阵，记录直接/间接证据和非阻断增强项；不计入 `CGEN-*` 规则数。
- `36_P1核心规则验证矩阵.md`：async/thread/channel/sync/task、generator、class/custom、ABI 元数据、声明/return/多返回值共 127 条 P1 规则的验证矩阵；不计入 `CGEN-*` 规则数。
- `37_P2运行时HeaderModule标准库验证矩阵.md`：标准库 wrapper、header/needs、module 总装配、runtime/main/xvalue helper 共 94 条 P2 规则的验证矩阵；不计入 `CGEN-*` 规则数。
- `38_P2符号泛型调用类型推断验证矩阵.md`：符号命名与类型映射、泛型推断与运行时状态、调用解析与默认值、类型克隆与表达式推断共 136 条 P2 规则的验证矩阵；不计入 `CGEN-*` 规则数。
- `39_P2清理上下文入口验证矩阵.md`：cleanup 与借用生命周期、上下文状态表、入口判定共 82 条 P2 规则的验证矩阵；不计入 `CGEN-*` 规则数。
- `40_P2_XAPI元数据验证矩阵.md`：XAPI 基础输出与 doc、XAPI module 构建与 JSON 输出、XAPI 读取与模型恢复共 87 条 P2 规则的验证矩阵；不计入 `CGEN-*` 规则数。
- `41_P2_import_package验证矩阵.md`：import 依赖与命名空间、import 声明生成与 AST 重写、package 解析与 resolver 共 74 条 P2 规则的验证矩阵；不计入 `CGEN-*` 规则数。
- `42_P2_CLI_DLL验证矩阵.md`：CLI 构建运行与产物路径、DLL/API 动态执行共 63 条 P2 规则的验证矩阵；不计入 `CGEN-*` 规则数。
- `43_文档整理收口.md`：记录本轮文档整理后的稳定口径、剩余非阻断增强项和后续维护要求；不计入 `CGEN-*` 规则数。
- `44_文档整理验收报告.md`：记录本轮文档整理的最终验收口径、计数校验、重复 ID 校验和行号引用校验；不计入 `CGEN-*` 规则数。

## 对照表字段

每条规则固定包含：

- xlang 写法
- 生成规则描述
- 生成器文件名和行号
- 对应生成的 C 代码
- 中间形态伪代码
- 实现同等功能手写的 C 代码
- 额外依赖的封装代码

## 覆盖口径

这里的“规则”指会改变生成 C 形态的分支，不把纯字符串编码、哈希、数组扩容、上下文清理这类内部工具函数单独当成语言规则。辅助函数仍在 `_function_index.generated.md` 和 `00_文件覆盖索引.md` 中保留索引。

当前已经完成主干规则、P0 细化、async/generator/class/custom/ABI 元数据/声明与多返回值 P1 细化，以及标准库 wrapper、header/needs/module runtime helper/符号命名与类型映射/泛型推断与运行时状态/调用解析/类型克隆与表达式推断/cleanup 与借用生命周期/上下文状态表/入口判定/XAPI 输出/XAPI 读取/import 展开/package resolver/CLI 工具链/xl.dll API P2 细化。目录内已有 841 条 `CGEN-*` 规则；同时已完成主干/P0、P1 核心、P2 运行时/Header/Module/标准库、P2 符号/泛型/调用/类型推断、P2 cleanup/上下文/入口、P2 XAPI 元数据、P2 import/package、P2 CLI/DLL 验证矩阵。

剩余内容不再属于规则文档缺失，而是非阻断增强：把人工矩阵机器化、为内部状态补更细生成快照、扩展资源巡检和错误路径测试。新增或修改 cgen 行为时，应同步更新对应规则表、验证矩阵和 `05_继续细化清单.md`。

文档守门脚本为 `tools\check_cgenrule_docs.ps1`，已接入 `test.bat`。它负责校验唯一规则数、重复 ID、矩阵/索引引用、占位残留和源码行号引用。
