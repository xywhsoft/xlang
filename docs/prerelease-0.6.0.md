# xlang 0.6.0 发布前检查

版本：`0.6.0`；版本代号：`0x00000600`；宿主 C ABI：`1`。

## 发布判定

**暂不通过正式发布。** 已修复本轮发现的多项编译、运行时与交付问题，但原生拥有合同和宿主退出仍有实际阻塞。版本修改和本轮修复已落地，以下保留最终成功与失败结果；本轮不签收正式发布。

## 已完成修复

- 统一产品版本头、CLI 查询、双 DLL/SO 查询接口及 Windows EXE/DLL 版本资源，版本查询不要求初始化运行时。
- 按职责拆分超出维护尺寸的 Cgen 源文件；修复整数除零、余数和位移错误的源位置。
- 将 PathParts、PathIterator、SipHasher64 的析构迁入驻留运行时，补齐精确原生拥有策略；补齐异步文件及底层文件节点的图合同。
- 独立 AOT 退出先静止服务再进行最终图清理；失败保留 checked 结果，不进入无限等待。
- 修复 Windows TCC SDK 的 C99 `vsnprintf` 合同；长格式化、零容量及截断行为有独立回归。
- 补齐原先声明却不能链接的 `os.args()`，覆盖 Unicode、空参数、独立副本和真实分配失败前缀。
- floatEnv 快照改用普通语言对象持有 ForeignResource，移除裸指针接管构造入口，保证 hold 之前先建立完整拥有关系。
- 修复 Windows DLL 的六个漏导出驻留入口；双 DLL／双 SO 的全部 1,469 个驻留登记符号均可查得。
- 去重预编译对象目录，清退三个未使用的私有桥接登记；修复根目录迁移后失效的门禁路径、符号核对、测试清单和文档索引。
- 从实际声明登记表生成标准库函数／常量参考；生成的调用形式夹具只代表类型检查，不能解释为全部函数运行覆盖。

## 最终验证结果

| 检查 | 结果 |
| --- | --- |
| Windows / Linux 普通构建 | 均成功；各自冻结输入在构建期间未变 |
| 版本与公开 C SDK | Windows 12 项、Linux 9 项通过；含初始化前查询、Windows VERSIONINFO、双库独立 C 宿主 |
| 普通产品批量回归 | 两端各 303 阶段，301 通过、2 失败：`stdlib_file_async` 的 JIT 与 AOT 退出 |
| 驻留导出 | 四个库各 1,469 个符号全部存在 |
| 定向格式化／进程参数 | Windows 10 阶段、Linux 12 阶段通过；每库 80 个分配失败前缀，Linux 含非法 UTF-8 |
| 隔离交付 | 两端各 17 阶段通过；空工作目录、复制的产品与 SDK、C／嵌套 xlang 的 JIT/AOT、双库宿主 |
| 静态门禁 | 33 项通过；文档索引首轮缺少本报告的状态登记，补齐后复验通过 |
| VFS／文件系统 | 38 项严格 C 与确定性检查通过，37 项 JIT/AOT 全阶段通过，复合异步文件仍失败；10 个 ABI 负例和 6 个公开示例通过 |
| floatEnv | 两端完整包门禁通过；Linux 包括 C ASan/UBSan，均含完整边界与范例 AOT |
| 原生拥有发布门禁 | 两端失败：12 个析构合同缺失、4 类全局对象及复合异步文件退出失败；Windows 另复现 SDK 清理未完成 |

批量回归包括每端 90 个正例的 JIT／构建／独立 AOT，以及 33 个拒绝用例。Linux 首轮有 5 个文件用例缺少 `build/test` 前置目录；补齐后 15 个阶段全部复验通过，原始日志保留。表内 2 个剩余失败均在修正测试环境后仍可复现。

另有 254 个补充编译／拒绝用例通过，使用同源 TCC 快速驱动，只计静态合同验证。VFS 前 30 项使用冻结 TCC 驱动与最终 `xl.dll`，后 8 项及全部负例、示例使用普通产物；未将它们混报为全套普通产物验收。完整 VFS 门禁保持失败；脚本已改为收集各用例失败，并支持 `-OnlyFixture` 定点复验。

## 剩余阻塞

1. **12 个析构入口缺少拥有图合同，涉及 13 种类。** ConsoleSession、ConsoleEvent、JsonWriter、XlonWriter、XLangHandler、TreeStats、WalkStats、LogRecord、LogFileStats、LogQueueStats、LogStats、LogSink、Logger。JsonWriter、XlonWriter、Logger、LogRecord 的全局对象已复现“主体完成，AOT 退出报 STATE”。需要逐类确认真实持有边、活动调用、静止与 checked 退役；日志回调和嵌套模块不可直接登记为叶对象。
2. **SDK 异步资源清理存在时序缺口。** 全局 AsyncFile／Future 在脚本完成后，checked drain 曾在 5 秒内未完成；自动与显式关闭对照均出现过失败，不能把 close 当作完整解决方案。需要完成模块预清理、任务池、文件与 Future 节点之间的退出顺序，不能用无限等待掩盖。
授权政策已确定： 项目权利人已指定 xlang 原创部分采用 MIT，已增加根 LICENSE 和第三方许可说明。TCC 主体及其衍生修改继续沿用原 LGPL，不主动转换为普通 GPL。现有逐文件许可与链接例外保留，第三方交付材料仍按实际载荷整理。

`stdlib_file_async.xl` 已同时复现主体成功、JIT 收尾失败与 AOT STATE；它会使用未封闭的统计快照等对象，属于真实发布阻塞。最终 Windows SDK 复测中 `xlc.dll` 自动关闭返回 `{run:1, ready:0, ok:0}`，显式关闭通过；Linux 本轮双库对照均通过。此前 Windows 候选的显式关闭也出现过失败，因此没有把这条路径宣告修复。

上述两类技术问题已进入独立的 `check_release_native_owners.py` 发布门禁；失败保持失败。

## 验证范围

Windows x64 与 WSL Ubuntu x86-64；正常离线用例、类型拒绝、内存故障、生成 C 与 SDK 交付。不运行攻击、模糊测试、外部网络或 TLS 握手测试，不重复修改本机时区。IDE 的并行修改不纳入本轮提交。

XRT 固定为最新核验的 `9b5d5269`，使用毫秒时间合同；未修改独立 XRT 工作区。

## 复验入口

```powershell
python tools/check_release_version.py
python tools/check_release_runtime.py
python tools/check_release_delivery.py
python tools/check_release_native_owners.py
```

前三项覆盖版本、定向回归与隔离交付；最后一项当前预期拒绝发布。`rc0_guard.ps1` 接入版本、隔离交付和原生拥有门禁。完整 test_all 未执行，本轮结果不等同于全部平台和全部标准库逐入口签收。

## 本地交付产物

产品已同步到仓库 `release/`，这些二进制按现有规则由 Git 忽略，源码、头文件、构建与验收工具参与提交。没有创建 tag、推送或对外发布。IDE 产物和原有安装回执不在本轮重签范围。

| 产物 | 字节 |
| --- | ---: |
| `xl.exe` | 49152 |
| `xl.dll` | 14268928 |
| `xlc.dll` | 19621888 |
| `xl` | 43136 |
| `libxl.so` | 15980080 |
| `libxlc.so` | 21268336 |

[机器可读摘要、产物 SHA-256 与证据索引](reports/prerelease-0.6.0.json)。完整日志位于 `F:\xlang-validation\prerelease-060-20261030`；成功与首次失败日志均保留。第三方 TCC 构建仍有既存的 `fread` 返回值警告，本轮未改动其算法源码。

后续封板应先补齐上述原生类的实际持有边与 checked 退役，再修复宿主异步退出顺序；随后合并执行同一批回归和独立发布门禁。
