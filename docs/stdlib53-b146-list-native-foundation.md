# B146：原生稀疏 List 底座

日期：2026-10-09。属于已批准 D-XRT-CONTAINER53 的实现步骤，不新增语言公开面。
**语言 `list<T>` 尚未迁移完成；此批不代表 shipping 或 53 模块总验收。**

## 本批实现

- 私有 `xl6_list.h/.c` 使用实际 `xtypedlist`，复用 XRT IntMap/节点池/类型槽，
  不重新实现存储或用连续 Array 冒充整数稀疏 List。键包含完整 int64，
  `count` 是元素数量；追加使用最大键 + 1，INT64_MAX 溢出不插入。
- 从新建空 Array 捕获独立拥有的 resident admission 元数据。List 不保留
  witness、Array 存储或生成模块代码。确切身份、物理 tuple、完整逻辑 policy
  均参与准入；同物理布局的 u8/int64、bytes/var 不混同。
- Set/Append/Get/Take/Remove/Clear/Count/Trim/Clone 是原生私有操作，不是
  本批新增的语言 API。checked 成功状态与 found 分离，失败不擅自发布结果键。
  输出为编译器私有已初始化结果槽，不引入 source byref 或元素借用。
- Cursor 独立拥有实际 typed-list 快照及元数据，复用 XRT 正向/反向/有界
  iterator；完整遍历 O(n)，不是每步调用 O(n) ordinal lookup。缓存槽指针
  只指向 Cursor 自身的不可变快照，不暴露到语言，也不依赖原 List 寿命。
- BUSY 与 operation pin 拒绝重入，元素回调不在 graph mutation 临界区执行。
  这是生命周期保护，不把 List 宣称为可并发修改的线程安全容器。
- Phased retirement 观察实际物理槽，重复拥有边保留重复。Future 槽追踪
  实际控制块，并要求确切 resident ops allowlist；V1 不静默忽略 Future 边。
  Claim 后、Clear 前可读；Finish 将物理清理完成与继承的失败分开，支持重试。
- 销毁保留首错，即使后续 finalizer 报另一错误。清理边界未能安装时仍释放
  已拥有的存储/元数据，并在元素回调前锁定首错；checked Finish 则在边界
  失败时保留待清理存储供宿主/图协议重试，不自动丢弃或 abort。
- 私有 handle **没有**伪造的结构化 Hash/Equal callback。结构化身份、contains、
  合并、var/持久化 wire bridge、IR producer/索引/迭代整体切换仍待完成。
  未登记产品构建源、原生组件、source interner、SDK/VFS 或 Cgen；不能将
  此底座提前作为语言载体或 Set/Dict key。普通产品没有新增 List provider。

## 同一源码的批量验证

使用选定 Git index 的隔离导出，不读取主工作区的其他修改。最终源码：

- `E:/xlang-validation/stdlib53-b146-source-final-dev3/frozen/demo6`
- Linux 副本 `/var/tmp/xlang-stdlib53-b146-final-dev3`，Ubuntu VHD 位于 E 盘。

`tools/check_stdlib_list_native.py` 编译完整本次选定原生源码闭包，包括实际
XRT/xruntime bodies；不链接旧 DLL，不替换 List 算法/存储/graph provider。
两端都执行严格 C11/Wall/Wextra/Werror 的 O0/O2；Linux 同时对本次闭包
启用 ASan/UBSan、leak 与 stack-use-after-return 检查。此处的 sanitizer
范围不是整个产品或整个 runtime。

十类独立 vector 模式：整数稀疏键、NUL/UTF-8 字符串副本、char/nullable
文本、快照游标、身份/策略拒绝、实际重复 Value 边、实际 Future/nullable
Future 边、真实自环 phased 清理、重入与释放最后 shell、清理边界失败。
后者使用 GNU ld `--wrap=xl6ErrorScopeBeginCleanup`：未激活时原样委托真实
边界，显式激活时返回 checked 失败，包括 NULL 诊断；**不是实际 OS/FLS
资源耗尽试验**。验证首错保留、仍然释放和 Finish 保留资源后重试。

12 类 caller-thread 分配失败前缀：create/insert/replace/append/clone/cursor/
get/take/cursor-read/cursor-advance/clear/trim。每类从点 0 遍历到首个真实
未触发点；每次核对源内容、计数、输出哨兵，以及分配/释放/live bytes、
invalid/double free/UAF。零分配方法首点未触发只证明该路径未分配，不将其
冒充一次 OOM 失败。NULL 首诊断使用明确的 checked 失败注入单独测试，
不会错误地声称静态状态错误发生了堆分配失败。

最终报告均 Complete=true、InputsUnchanged=true、Failures 为空：

| 报告 | 阶段 | 闭合前缀 | 前缀运行 |
| --- | ---: | ---: | ---: |
| E:/xlang-validation/stdlib53-b146-list-win-final-dev3/results.json | 108 | 24 | 84 |
| /var/tmp/xlang-stdlib53-b146-list-linux-final-dev3/results.json | 108 | 24 | 84 |

共 216 阶段、48 个闭合前缀、168 次前缀运行，其中 120 次实际触发失败、
48 次首次未触发。包含两端与优化等级重复，不是 120 个不同实现缺陷。
dev1 的测试 API 拼写错误、dev2 的静态诊断错误假设均保留，不计入通过；
dev3 通过后继续增加活动 Cursor tracing、NULL 继承和边界失败/重试覆盖，
最终按同一暂存版本统一复测。不用旧通过报告覆盖新输入。

提交前 `stdlib53-b146-index-win-final.json` 和 `index-linux-final.json` 重查
暂存生产/测试输入与隔离源码、测试及编译工具哈希；不豁免生产源或头。
新文档不参与 native 编译。发现遗留的零字节 Git index.lock，核对超过
15 分钟且无 git 进程后移为 `.git/index.lock.stale-b146`，未删除 Git 数据。

## 尚待完成

首先统一 List 的结构化身份/查询、事务合并与 wire 转换，再一次性迁移
字面量、默认值、索引、foreach、所有 producer 和 var/持久化边界，并
完成真实产品 JIT/AOT/VFS/组件裁剪验收。不能只改一个包装入口临时转换。
Dict/Set、SlotMap、WaitQueue/Channel、OrderedMap 合同及跨组总门继续；
未决策 19 组不变，对象池最后，长期目标保持 active。
