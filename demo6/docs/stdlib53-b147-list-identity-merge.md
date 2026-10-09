# B147：List 结构化身份、checked 查询与事务合并

日期：2026-10-09。继续已批准 D-XRT-CONTAINER53，不新增语言公开面。
**本批完成私有原生合同，未完成语言 `list<T>` 载体切换或 53 模块总验收。**

## 实现与边界

- 在 B146 的实际 `xtypedlist` 底座上补齐 checked Equals/Contains/Hash/Merge。
  相等比较使用成对有序 iterator，完整稀疏 int64 键和元素共同决定结果，
  时间 O(n)；不使用 ordinal 搜索、临时装箱或连续 Array 替代稀疏 List。
- 完整 layout 与 resident admission policy 决定比较域。相同物理布局的
  u8/int64、bytes/var 不获得相互准入。显式方法拒绝不同策略；动态 Value
  相等对不同域返回正常 false，而不是把域不匹配当作错误或忽略策略。
- 标量和文本复用实际 typed descriptor；保留精确 NUL 长度、Unicode
  scalar 和 nullable 语义。引用槽使用真实 Value 结构化相等/哈希，规范化
  NULL 与 canonical null；Future 使用实际控制块引用身份，不比较状态或
  结果。没有给引用值伪造全序 comparator，无法哈希的子值仍报告 TypeError。
- 原生 Hash 属于 payload 域；XRT ValueHash 还会混合 HANDLE tag。等值对象
  在各自同一边界具有相等哈希，不要求不同边界的数值相同，也不是稳定 wire
  哈希。Hash 不是冻结接口；作为 Set/Dict key 存放时仍要求 key 内容稳定。
  Cursor 不是集合 key；重入和递归超限不回退为地址哈希。
- 每个引用槽安装独立 checked 错误边界，区分正常 false 与比较失败。
  即使子回调返回貌似合法的 true/hash，或只传播 failed 位而没有诊断对象，
  也会在第一槽停止，保留首错，失败不改变调用者输出。
  payload-only XRT Hash/Equal 回调在首错对象为 NULL 时发布不分配的次级
  State 诊断，防止 raw XRT 将其误认成功；外层 checked 结果仍保留 NULL 首错。
- Merge 直接复用 `xrtTypedListMerge` 的失败原子实现，完整同域检查后执行。
  保留/覆盖重复键两种策略和 self merge 均有明确合同；分配失败不改变源或
  目标原有内容，不用先修改再补救的方式实现。
- Array/Stack/List 共享原有每原生线程 64 层 identity 调用预算，避免每个
  容器家族独立计数而绕过限制。复用现有 64 lane 的短锁与 native stack
  frame，不增加 C TLS relocation、堆 key 或每家族一套 guard。
  空 frame、活动 frame 重复入栈正常拒绝；正常出栈清零后可复用。frame
  必须初始清零，成功 Enter/Leave 在同一同步 native 调用路径配对，不能
  复制、挂起或作为语言借用暴露。错误的私有 Leave 协议仍属于内部不变量。
- 元素回调在 graph mutation 临界区外运行，BUSY/pin 保护本次生命周期，
  不据此宣称 List 支持并发修改。Array/Stack 的正常结构身份域没有改变；
  本批不宣称其全部回调错误路径已重新审计。

没有新增产品构建源、原生组件、SDK/VFS、source interner 或 Cgen/IR
分支。List 的原生 handle 身份已具备，但仍不能据此将语言迁移标记完成。

## 同一最终源码的批量验收

暂存 index 隔离导出：

- `E:/xlang-validation/stdlib53-b147-source-final-dev1/frozen/demo6`
- Linux `/var/tmp/xlang-stdlib53-b147-final-dev1`，Ubuntu VHD 仍位于 E 盘。

`tools/check_stdlib_list_native.py` 编译实际选定 XRT/xruntime/resident
实现闭包，不使用旧 DLL、伪 storage 或 provider 覆盖。Windows/Linux 均
严格 C11/Wall/Wextra/Werror，O0/O2；Linux 对本次完整选定闭包启用
ASan/UBSan、leak、stack-use-after-return。**不是整个产品/runtime 的
sanitizer，也没有语言 C/JIT/AOT、独立产物或完整 53 模块通过的含义。**

15 类独立 vector 保留 B146 的槽/游标/拥有图/清理/重入测试，新增完整
稀疏身份、独立 bytes 与 NUL 内容、稳定集合 key、域拒绝、实际 Future
身份、两种合并、首槽错误停止、NULL 首错、List→实际 native Array 的
共享深度预算、frame 防误用与释放后重试。

15 类 caller-thread OOM 前缀全部从 0 到首次真实未触发点闭合。新增
merge-keep/merge-replace 每优化 lane 分别 7/8 个实际失败点；实际 48 层
Value 子图比较触发 pair memoization，每 lane 18 个失败点。核对源/目标
内容、输出哨兵及 allocation/free/live/invalid/double-free/UAF。零分配
路径的首次未触发不冒充 OOM 成功；没有用静态错误构造冒充堆分配失败。

独立父门 `tools/check_stdlib_sequence_identity.py` 未修改，验证实际
Array/Stack identity、完整比较 OOM、30 轮回收、64/65 深度边界和 80 个
同时 native thread 的 lane 冲突。后者验证父 guard，不宣称 List 并发
修改已验收；List 与实际 Array 共享预算另由本批 vector 验证。

| 最终报告 | 阶段 | 闭合前缀 | 前缀运行 | 实际失败命中 |
| --- | ---: | ---: | ---: | ---: |
| E:/xlang-validation/stdlib53-b147-list-win-final-dev1/results.json | 190 | 30 | 156 | 126 |
| /var/tmp/xlang-stdlib53-b147-list-linux-final-dev1/results.json | 190 | 30 | 156 | 126 |
| E:/xlang-validation/stdlib53-b147-sequence-win-final-dev1/results.json | 44 | 2 | 38 | 36 |
| /var/tmp/xlang-stdlib53-b147-sequence-linux-final-dev1/results.json | 44 | 2 | 38 | 36 |

全部 Complete=true、InputsUnchanged=true，所有阶段成功。合计 468 阶段，
64 个闭合前缀、388 次前缀运行，其中 324 次实际失败命中和 64 次首次
未触发。数字包含平台/优化等级重复，不是不同 API 数或不同缺陷数。
dev1 将原生 payload hash 与 XRT tagged ValueHash 错误地比较为同一数值，
修正该测试假设后统一复测；失败报告保留且不计入通过。

B146 的 cleanup 边界注入仍用 GNU ld `--wrap`，仅显式激活时拒绝，其他
调用委托真实实现，包括 NULL 首诊断；不宣称实际 OS/FLS 资源耗尽。
最终 Windows/Linux index audit 分别记录于
`E:/xlang-validation/stdlib53-b147-index-win-final.json` 与
`E:/xlang-validation/stdlib53-b147-index-linux-final.json`，串行核对暂存
生产/测试输入、隔离源码和报告输入哈希，不豁免生产源或测试差异。

## 下一步与未完成范围

继续 List 的 resident 动态值/精确稀疏 wire 桥接、嵌套 typed schema，
然后一次性切换字面量、默认值、索引、foreach、所有 producer、var 和
持久化边界。不能仅改入口、临时转 wire 或扩大为 ANY 丢失子类型。
真实产品 JIT/AOT/VFS/按需组件验收，Dict/Set、SlotMap、WaitQueue/Channel、
OrderedMap 与跨组总门仍待完成。

持锁失去语言引用继续遵循用户选择：保留原生退役记录，由宿主 checked
接口取得 owned handle，满足原线程约束后手动解锁并重试清理；不自动
解锁、abort 或丢弃资源。该决定不扩展 X15/X16 的未批准公开面。
19 个 XRT 未决策组不变，CRT 无未决策；对象池最后，长期目标保持 active。
