# Plist 冷启动与 binary 错误分类审查

## 修复范围

接续 XML 三包的每线程状态审查，在普通队列中检查相同机制。TOML、
netrc 的主要入口已有状态失败前置检查，本轮未改两包或据此重新签收。
plist 的五个基础入口尚会清掉该错误并继续操作；11 个 XML/binary
调用分支确认了这一问题。只在创建、解析、加载、序列化、保存前
拒绝不可用的线程状态，保留原 XRT 内存错误，不交出半成功资源。

故障扫描又定位到三个相关问题：

- binary 的节点、字节副本、UTF-16 转换或容器扩容失败，原统一落成
  FORMAT。现在仅在已有结构化错误为空时检查本次 XRT MEMORY；已有
  LIMIT 等首因不被覆盖，正常解析和编码不增加分配或扫描。
- binary 数组/字典超过 maxEntries 原走格式错误，现报告 LIMIT。
- XML 字典用 `uint64上限 * 2` 比较子元素，超大合法上限会回绕。
  改成比较实际条目数；uint32 子项数先拓宽后向上取整，零上限仍不限。

生产改动限 `plist_native.c`、`xrt_plist_binary.c`、`xrt_plist_xml.c`，
没有公开 API、ABI、格式或限额语义变更。没有替换 XRT、XML 依赖、
Expat 或编译器。保留 PlistError 与 MemoryError 的原分工，不把
所有失败改成统一运行时错误，也不把原生 NULL 改成可用结果。

## 验证

`test_cold_state.c` 每个预算一个新进程，不能预热后再声称覆盖首次
线程槽创建。11 条路径为 create、XML memory/text/load、XML
buffer/text/save、binary memory/load/buffer/save。检查结果、错误
类别、保存目标未变、故障解除后同一输入恢复及活动分配/字节平衡，
同时检查非法释放、双重释放和 UAF 计数。文件只使用门禁提供的
独占且原先不存在的路径，正常结束删除。

`cold_start.xl` 的公开 loads 在每个进程首次调用时注入故障；
`binary_errors.xl` 检查 ASCII/非 ASCII binary 解码故障必须是
MemoryError，而非 PlistError，并验证下一次正常解析。

`test_binary_boundaries.c` 包含全部十种值类型，另分 ASCII/Unicode
字符串与嵌套容器，共 12 份完整数据：null、bool、int、real、两种
string、data、date、uint64 最大 UID、array、dict、嵌套 dict。
142 个实际 OOM 点均要求正确分类和内存平衡、立即恢复；48 个
XML/binary 容器边界覆盖小上限及 `UINT64_MAX/2` 两侧、最大值和零。
36 个未支持标记/陈旧 IO、MEMORY 对照确保格式错误不会被旧原因误标。
其中的“不产生额外 ARGUMENT”断言在旧代码也通过，不把它冒充新修复。

Python 标准库 plistlib 独立读取 C 生成的 12 份 binary 文件，逐个核对
完整值与类型，包括字节、日期和最大 UID；原有 C/JIT/AOT 的 XML/
binary 互通仍保留。不把写出器自己读回成功当作唯一的正确性证据。

| 最终回归 | Windows | Linux 原生文件系统 |
| --- | --- | --- |
| C 冷启动逐进程扫描 | 356 场景通过 | 348 场景通过，ASan/UBSan |
| 公开 XLang 冷启动 | JIT/AOT 各 87 预算通过 | JIT/AOT 各 87 预算通过 |
| 同一最终 C 夹具只换旧三文件 | 50 个冷启动失败场景、64 条 binary/限额断言 | 同样 50 场景、64 条断言 |
| 新线程错误隔离 | 4 线程/64 诊断，GCC/TCC JIT/AOT | 相同三条路径通过 |
| 生成 C 与原生定义 | 41 条原型一致，错改 owned 返回值被拒绝 | 相同对照通过 |
| 正常独立 AOT | 13 份构建运行通过 | 13 份构建运行通过 |

旧 64 条断言包含 58 条 OOM 分类和 6 条容器上限错误，不是 64 个
互不相关的 BUG。冷启动的 50 个失败场景包含初始化及其派生 binary
路径；不把两种统计简单相加。最初探测的旧程序为 358 场景，最终
测试宿主加入 THREAD 后按相同输入/工具重新编译，正式对照采用
表中的 356/348；不混用不同宿主的结果。

Windows 默认完整门禁 587 项均符合预期，保存 125 项输入/实际工具
指纹（含 GCC、cc1、Python、PowerShell）；Linux 快照 131 源文件及
GCC/cc1/Python 三工具前后不变，控制阶段复用的 runtime 对象亦核对。
两门禁的编译/执行有界，失败保留命令、输出、原始状态及输入指纹；
Windows 静默编译器 raw 37/外层 1、Linux 37/缺失编译器 127 的真实
入口控制通过。成功仅删除自己创建的工作目录，保留汇总记录。

本轮曾把新 XLang 用例的 break 紧接在内联分号后，触发语法错误；
按当前普通多行 if 写法修正后验证，未改语言语法。其余正式对照
没有靠放宽断言、替换编译器或捕获错误当成功换绿灯。

## 现场与边界

- 本地：`build/test/plist_state_audit_1793350140`。
- Windows 完整报告：
  `build/test/plist_guard_ecc79469a8a84c26a5ee4edeb2db5863.report.json`。
- Linux：`/var/tmp/xlang-plist-state-audit-7cc7BS8N`。
- 执行完门禁后仅补说明、台账和官网源，隔离生成官网不部署。
- 八个冻结产物/单头库未变，未合并滚动主线或新 XRT。

这是当前明确的 XML 1.0/bplist00 合同，不是全部 Apple 私有扩展、
任意文件规模或任意线程调度的认证。没有吞吐基准，不把错误路径
守门称为性能优化或完整桥接零开销。普通队列仍 100 包，其他已登记
主线/平台风险不因本包通过而关闭。
