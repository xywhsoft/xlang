# List 共享图复制与格式计划验收

日期：2026-10-09。属于已批准的容器接入任务。本批完成私有原生 List
与 Array、类字段和普通 wire 容器的共享图深复制及数据投影。递归恢复、
完整语言载体切换、真实产品与 53 模块总验收仍待完成；没有新增公开 API。

## 单次图遍历

`xl6_value_graph_plan.h` 定义同步借用的驻留 provider 与精确身份格式表。
`xl6_array_graph.c` 保留原有入口，新增显式计划入口；可选
`xl6_list_graph.c` 提供真实 typed List 适配。整个操作只调用一次
`xrtValueGraphCopyV1`，所有引用边通过同一个 `Next` 递归，没有另建 memo、
先复制成 wire 再遍历、全局注册或保存源模块回调。

List 只在 genuine handle ops 匹配后读取捕获的原生 payload，并取得
owner 操作 pin。引用槽借用真实 Value 边，不对正在遍历的祖先 GetHandle
或 Retain。深复制保留完整准入策略、驻留 codec 与全部 int64 稀疏键，
目标拥有独立 typed 存储。非引用深复制直接使用物理槽，不经过临时装箱、
解码及第二次字符串复制；Future 叶保留真实 consumer control block。

遍历沿用 XRT 的无环图合同：同一源 Value 在多个分支重复出现时共享复制
结果，循环和超深图被拒绝。不同 Value 外壳指向同一 raw owner 不因此被
视为同一 memo 身份。源 List 使用实际 typed 迭代器线性访问槽位，目标
IntMap 的插入复杂度不由此改为线性。

## 数据格式与类字段

无原生格式规则的 List 数据快照输出 IntMap，保留稀疏键。
精确 native sequence 规则只选择指定 provider，要求输出 ARRAY，并按
排序后的原生迭代顺序压紧元素；这承接既有语言 List 数组格式，而不是
将一层 IntMap 边界误当作语言持久化格式。身份、provider、数量、指针和
重复规则均校验；native 身份与普通 wire sequence 表冲突时拒绝。

`xl6PersistenceProjectNativeV1` 将现有对象字段计划接入同一 memo，保留
字段选择、omit、精确长度重命名与 open extras。旧 V1 字段计划布局与
既有入口不变，没有向 XRT 图 ABI 添加特殊 List 字段。

原生规则拒绝不匹配 handle、错误 provider 输出，以及根节点与被调用
对象回调的物理类型冲突。它不是所有嵌套 wire 标量的通用 TypeId 验证器；
语言层仍须提供闭合且准确的 IR 类型和格式计划。

## 错误与清理

每个 provider 都有 checked 错误边界。返回成功但同时有诊断或独立
NULL failed 位的结果不会发布；错误状态与输出组合被拒绝。未完成目标
会释放，入口与 cleanup 边界失败仍保留首错，不用 abort 处理可检查的
分配失败。测试边界注入通过 GNU ld wrap 真实入口，只有明确武装向量
才返回失败；它不是实际 OS 或 FLS 耗尽。

## 批量验收

两端使用严格 C11、Wall/Wextra/Werror，分别运行 O0/O2。Linux
ASan/UBSan、泄漏及栈生命周期检查覆盖被选择的原生闭包，不代表整套
产品或运行时全部插桩。最终八份报告 Complete/InputsUnchanged 均为
true，所有运行行成功；存在 Failures 字段的报告均为空。

| 专项与计数范围 | 阶段 | 闭合前缀 | 前缀运行 | 实际命中 |
| --- | ---: | ---: | ---: | ---: |
| List 共享图专项 每端 | 1362 | 66 | 1294 | 1228 |
| 不启用图组件的 List 父门 每端 | 1144 | 54 | 1090 | 1036 |
| 既有九类 Array 共享图父门 每端 | 5327 | 80 | 5226 | 5146 |
| 既有对象字段投影与 open extras 父门 每端 | 4671 | 22 | 4566 | 4544 |
| 双端总计 | 25008 | 444 | 24352 | 23908 |

阶段是平台、优化级别及不同操作的执行次数，不是独立 API 或缺陷数。
每条分配失败前缀从 0 连续执行到首次实际未命中，包含该未命中点。
新增六个图操作每 lane 的首次未命中分别为 deep 10、data 7、sequence 5、
project 21、mixed deep 32、mixed data 21；原有完整嵌套 schema 的
390 点前缀仍保留。各向量检查分配释放、live bytes、无效或重复释放
以及 UAF 计数。

新增向量覆盖七种真实叶配置，包括 char、内嵌 NUL 的 wstring/bytes、
nullable string/int64 和真实 Future；完整 MIN/MAX 稀疏键；
List/Array/List 混合、类字段及 wire IntMap 的四分支别名；70 个不同
子节点各重复两次以触发 memo 扩容；真实 JSON/XLON 写出；循环、深度、
退役、BUSY、Cursor、foreign 和格式碰撞拒绝；codec、入口、cleanup
以及有效输出附带 NULL failed 位的失败清理。

bare List O0/O2 符号门排除新的 native plan 入口、Array erased 入口及
可选 List provider；Array 图父门不链接 List。bare 专项的底层 XRT
依赖仍含 `xrtValueGraphCopyV1`，不能据此声称整个 XRT 图模块已被剔除。
该证据只证明新增原生适配的可选依赖，不替代真实 shipping 组件、
SDK/VFS 或最终独立裁剪验收。

## 最终输入与证据

List、bare List 和对象投影使用最终 Git index 冻结输入。Array 图父门
使用前一冻结目录，其报告中的 174 个目录内输入与最终目录逐文件
字节相同，两端均核对；两轮之间仅修正未进入该父门的 List 图夹具。
最终 index 审计另行比较当前暂存源码与最终目录，并重新哈希各报告
输入。闭包相同不被表述为两个整仓快照完全相同。

- Windows 最终源码 `E:/xlang-validation/stdlib53-b150-source-final-dev3/frozen/demo6`。
- Linux 最终源码 `/var/tmp/xlang-stdlib53-b150-final-dev3`。
- Windows 报告根目录 `E:/xlang-validation/` 下的
  `stdlib53-b150-list-win-final-dev3`、`stdlib53-b150-list-bare-win-final-dev3`、
  `stdlib53-b150-array-graph-win-final-dev2`、
  `stdlib53-b150-array-project-win-final-dev3`，各含 `results.json`。
- Linux 报告根目录 `/var/tmp/` 下的
  `xlang-stdlib53-b150-list-linux-final-dev3`、
  `xlang-stdlib53-b150-list-bare-linux-final-dev3`、
  `xlang-stdlib53-b150-array-graph-linux-final-dev2`、
  `xlang-stdlib53-b150-array-project-linux-final-dev3`，各含 `results.json`。
- 最终 index 审计 `E:/xlang-validation/stdlib53-b150-index-win-final-v2.json`
  与 `E:/xlang-validation/stdlib53-b150-index-linux-final-v2.json`。

早期失败证据保留，不计阳性：dev1 的 char 夹具错误调用 GetUInt；
final-dev2 的 nullable int64 夹具误用 transfer 2，实际应为 1。
修正夹具后 List 图专项两端完整重跑 final-dev3。没有用失败轮次的
List 图结果替代最终输入验收；Array 父门输入相同的核对范围如上。

## 后续整体迁移

先完成递归恢复与准入，再协调切换 List 语言工厂、所有 producer、
索引和迭代器、clone、var 分派、泛型、Future/类字段及持久化计划。
本批 provider 和显式计划尚未登记为语言或宿主公开入口；旧语言 List
仍不能仅凭本次原生通过被认定已经使用新载体。

整体切换同时处理按需组件、宿主导出、SDK/VFS，并执行真实产品
严格 C/JIT/AOT、Windows/Linux、裁剪和跨组总门。Dict/Set typed 迁移、
SlotMap、WaitQueue/Channel、OrderedMap 合同及 53 模块总验收继续。
CRT 无未决策；19 个 XRT 未决策组不变，对象池仍最后开发。
