# Dict 和 PatternBuilder 表示边界修复

三组标准库的批量实际运行发现了两个旧表示假设：全局 `dict.remove`
只接受旧 Value OBJECT，PatternBuilder 私有状态却由普通语言类创建。
本批将两者改为明确的原生合同，保持公开 API 不变，未修改 XRT 或
xruntime 单头。WaitQueue 同时确定为 MPMC；其实现和压力验收仍继续。

## Dict 命名空间

`dict.remove(table, key)` 直接登记到已有 `xl6DictValueRemoveV1`，
借用动态值并传递双字段字符串，支持原生 Dict 和旧 Value OBJECT。
内嵌 NUL 不截断，缺失键返回 false，错误类型走 checked 错误合同。
Cgen 只选择 Dict 方法组件，不再生成旧 wire-only 删除算法；旧 IR
枚举数值保留为 tombstone，不再授权旧调用。

专项同时检查原生和 wire 对象、NUL 键、缺失键及类型错误。四种 IR
合同篡改均被验证器、C/header 输出和原生产物规划拒绝。

## PatternBuilder 私有状态

运行时创建私有 Value OBJECT，包含 identity 和 INTMAP tags。普通
语言类布局不再被当成旧 OBJECT，也不假定语言 list 的物理表示。
tags 的实际 Value 边仍对拥有图可见；builder、key 和 edit 原生句柄
不持有语言标签。现有编辑事务、快照、退役清理和线程检查算法保留。

状态工厂七个实际分配失败点均返回 checked 内存错误并恢复分配平衡。
现有编辑流程的 49 个实际失败点、十条首个无故障完成路径、身份、
失效键、快照、孤儿编辑及线程检查均通过。

## 验收结果

Windows 完整源码编译器由 324 个翻译单元构建，Linux 为 323 个。
两端专项分别通过 41 和 43 阶段：五个语言夹具实际执行 JIT、module
和 AOT，生成 C 分别以 O0/O2 严格编译；原生宿主也分别以 O0/O2
执行。Linux 额外对选定原生宿主及其全部供应方进行 ASan/UBSan
检查。此插桩不代表完整运行时或发布 DLL 已插桩。

两端各 298 个三组夹具完成 595 阶段的声明和严格 C 检查，全部通过。
实际运行清单分别完成 579 阶段，结果均为 288 个通过、一个预期
拒绝、一个失败、八个需要专用宿主或平台/产品环境的条目。
目录校验为 285 个原生组件、1460 个精确导出，13 个拒绝控制通过。

原先七个实际失败收缩为 `stdlib_string_bridge_boundaries` 一项。
其 Buffer、Dict 和六个 NUL 边界拒绝已由诊断探针确认；剩余失败是
嵌套 `xlang.run` 所建编译器找不到 `__xl_runtime/xruntime_decl.h`。
该项仍记为失败，没有从运行清单排除，也没有调整断言使其通过。

六个需要专用宿主的夹具涉及真实 Promise continuation、拥有图及
析构错误注入；产品条目检查 release CLI。Windows IANA DST 夹具
与 Linux 旧 Windows shell 夹具分别列为平台需求，不算执行通过。
Linux 的 IANA DST 用例已设置 TZ 后实际通过。

## 剩余工作

嵌套编译器 SDK、WaitQueue MPMC、独立 borrowed 原生 SSA spill、
工作线程和图 OOM、专用宿主汇总及发布产物/VFS 总验收继续。
本批不是三组 53 模块或全部公开函数的最终发布验收。CRT 未决策为
零，XRT 其余 19 组保持未决策，对象池仍放最后。

证据目录为 `E:/xlang-validation/stdlib53-b254-` 前缀下的
`native-{win,linux}-dev7`、`runtime-{win,linux}-dev5`、
`discovery-{win,linux}-dev7`、`resident-{win,linux}-dev6`。
提交前证书逐项核对冻结源、产物和日志哈希，保留原始失败证据。
