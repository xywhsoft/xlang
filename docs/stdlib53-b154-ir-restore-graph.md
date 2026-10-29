# XLIR 数据恢复构造图

日期：2026-10-09。数据恢复的根入口、节点、字段边和声明默认工厂现已
成为真实 lowered XLIR 的显式记录，随既有恢复表一起构建和验证。
本批完成的是编译器中的静态构造配方；尚未把生成式递归恢复切换到
共享原生事务，不代表 List 语言载体或 53 模块总验收完成。

## 构造配方

每个实际可达的 JSON 或 XLON parse/read 目标产生一个根记录。同一
格式和目标 TypeId 的重复调用去重，不保留不可达函数中的恢复目标。
typed 阶段不携带这些记录。根拥有连续节点、边切片，内部引用使用
局部索引，避免让 Cgen 从 AST、类名或生成函数名重新推断语义。

节点明确区分 scalar、Array、class、List、Set、Dict；nullable 标志、
真实 payload TypeId、名义身份、标量值域、当前物理载体及 Array seed
槽位分别记录。class 节点引用格式对应的规范字段恢复表，声明默认
工厂保存函数索引、symbol 和 may-raise ABI，不借用用户 create 方法。
字段边保留精确 RestoreField 身份，因此 NUL 键、required、missing/null
和替换或原位赋值合同继续来自同一份规范元数据。

非 nullable class 字段的节点带精确 ClassField 上下文，不与 declaration
默认工厂节点合并。同类型的两个私有默认字段保持不同静态配方；nullable
替换使用独立声明默认配方。宽字段夹具在每种格式中包含 8 个节点、
77 条边、两个 Array seed，以及同一叶类的两个字段上下文和一个 nullable
声明上下文。不可达类不进入图。

构图使用迭代式节点队列，不以编译器 C 栈递归展开。这个有限静态图
只描述构造配方，不能证明不同父对象中的实际默认子对象具有相同
别名身份。运行时实例上下文、共享 memo 和模块 owner 的协调仍是
后续切换条件；不能仅凭相同 ClassField 或零 pending 标记完成认证。

## 明确的迁移边界

根的 pending 位记录开放 DynamicFields、旧 Array Value 载体、List、
Set、Dict、缺少声明默认工厂，以及 JSON bytes/time 域等当前边界。
List/Set/Dict 的组合夹具每种格式的 pending 都是 28，即三项并集。
这些位由规范合同重建并比对，不能通过修改 IR 把未迁移能力标成已支持。

pending 为零也不表示 Cgen 已切换。当前仍使用原有生成式恢复路径，
本批没有新增公开 API、原生符号或独立产物组件，也没有把私有恢复
provider 登记为默认语言载体。新图为下一批提供确切输入，不提供
未经验证的兼容分支或动态类型猜测。

## 失败原子性和验证

旧 plan/schema/field 与新 root/node/edge 六表在同一临时 arena 中构建，
全部成功才替换模块记录。任何失败保留原有六表的指针、容量、数量
和字节内容；临时记录全部释放。模块销毁同步释放新增三表。

验证从真实调用、类型、类字段和默认函数合同重建期望图，再逐字段
比对，不对含 padding 的 struct 做整体相等猜测。表计数、切片、节点、
边、上下文、标量规则、物理载体、seed、工厂 symbol/效果及 pending
的损坏必须在生成 C 或头文件前被拒绝，输出保持为空。

真实 parse/semantic/lowering 夹具覆盖格式分区、继承、NUL 键、私有和
跳过字段、nullable、不同默认上下文、宽字段、嵌套 Array、开放对象及
List/Set/Dict 边界。每个成功重建的实际分配点均注入持续分配失败，
检查六表不变及底层 allocator 存活分配账本；这属于编译器恢复表
重建 OOM，不是整个语言程序或异步线程的 OOM 认证。

## 源码和语言验收

Windows 编译器由 251 个实际翻译单元构建，Linux 为 250 个；真实
XRT/xruntime 和当前编译器源码均参与，仅 TCC 核心复用既有真实候选。
验证构建器现处理 manifest 的运行时头目录映射，两端各暂存并逐字节
核对 142 个头文件，重复目标拒绝。不改变发布 VFS 清单。

宽字段和完整声明构造两个实际语言程序在两端共 44 个阶段通过：
typed/lowered IR、确定生成 C、O0/O2 C 执行及 genuine TCC JIT。
执行后的语言分配账本平衡。生成 C 使用 C11/Wall/Wextra/Werror，
保留既有生成代码的 unused 类警告抑制；编译器全源码也沿用既有
兼容警告配置。没有把这些配置宣称为无抑制的全仓严格编译。

最终六份报告均 Complete/InputsUnchanged，所有执行行成功。

| 双端合计的实际范围 | 计数 |
| --- | ---: |
| 16 个源程序完整 IR 矩阵 | 92 阶段 |
| List Set Dict 补充 IR 矩阵 | 32 阶段 |
| 两个实际语言程序 | 44 阶段 |
| 上述全部阶段 | 168 |
| 新图记录变异拒绝 | 11264 次 |
| 含既有合同的全部变异拒绝 | 53988 次 |
| 六表重建的分配失败点注入 | 136 次 |

计数包含平台和 O0/O2 重复，不是独立 API 或缺陷数量。生成 C、
头文件的拒绝输出均为空；同平台两种优化的 IR dump 字节一致。

源码快照为 `E:/xlang-validation/stdlib53-b154-source-dev8/frozen/demo6`。
实际源码编译器证据为 Windows `stdlib53-b154-build-win-dev5/build.json`
及 Linux `/var/tmp/xlang-stdlib53-b154-build-linux-dev5/build.json`；编译
输入不变，后续快照只补充测试和文档。语言报告位于 Windows
`stdlib53-b154-language-win-dev6/results.json` 和 Linux
`/var/tmp/xlang-stdlib53-b154-language-linux-dev6/results.json`。
完整 IR 报告为 Windows `stdlib53-b154-graphs-win-dev8/results.json`、
Linux `/var/tmp/xlang-stdlib53-b154-graphs-linux-dev8/results.json`；
边界补测为 Windows `stdlib53-b154-pending-win-dev8/results.json` 和
Linux `/var/tmp/xlang-stdlib53-b154-pending-linux-dev8/results.json`。
Windows 相对证据目录均位于 `E:/xlang-validation/`。最终索引审计为
`E:/xlang-validation/stdlib53-b154-index-win-final.json` 和
`E:/xlang-validation/stdlib53-b154-index-linux-final.json`。

本批没有 AOT、shipping DLL/SO/VFS 或 sanitizer 全产品结论。早期
dev1 的选择性暂存定位错误、dev3 的头目录遗漏，以及 dev5/dev6 的
夹具或检查脚本失败保留在原目录，不能替代最终验收报告。

## 后续工作

接下来由 Cgen 直接消费验证后的构造图，将实际默认实例上下文、
checked nullable 输出和模块 owner 接入共享原生恢复事务，并协调
开放对象与 List 的语言载体切换，随后移除旧逐类型递归生成实现。
Set/Dict typed 迁移、SlotMap、WaitQueue/Channel、OrderedMap 公开合同
和完整 53 模块产品门继续。CRT 无未决策；19 个 XRT 未决策组保持
不变，对象池最后。
