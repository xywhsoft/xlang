# 参数化场景的 TCC 精确参数与 UTF-8 路径

## 实现

复用已经实际验证的响应文件编码，将 GCC 专用主体提取为明确的 compiler
response 入口，并增加 `Invoke-XlirTccToLog`。两个完整驻留 worker 的三个
TCC 调用迁移至该入口；不删除源文件、不替换 provider、不改原测试人口。
真实退出码、stdout／stderr、完整逻辑 argv、响应文件和有限超时继续保留。

TCC 的小型 exe 会加载邻接 `libtcc.dll`，因此回执同时核对启动器、邻接 DLL
及存在的 manifest 的前后库存和字节。这不是对 GCC 全部子工具的哈希声明。

安装的 TCC 0.9.27 使用窄字符文件接口，首轮在实际中文输出目录上拒绝写入。
原失败证据保留；未将测试路径改为纯 ASCII。新增显式工具
`stage_xlir_tcc_utf8.ps1 -Compiler <tcc.exe> -Directory <新隔离目录>`：

- 不修改原安装；复制并核对全部发行文件，拒绝覆盖、安装内输出和重解析点。
- 隔离副本只增加进程级 UTF-8 sidecar manifest；不改 exe／DLL 字节、系统
  locale、注册表或全局环境。不自动替换用户传入的编译器。
- 必须实际通过中文／空格输出路径和 C 字节探针，才返回可传入 worker
  `-Tcc` 的路径。不能假定 sidecar 一定生效；嵌入 manifest 可优先于它。
- 保存完整来源／副本哈希、控制输入及探针回执。重复使用保留既有证据。

Windows 的 per-process UTF-8 机制见
[Microsoft 文档](https://learn.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page)。
本次实际核对的隔离 TCC 共 1,096 个原发行文件字节不变。

## 完成的验证

PS5／PS7 × GCC／隔离 TCC 共 40 次真实编译，其中 36 成功、4 为预期
`#error`，36 次程序核对精确字节、20 次非法参数／覆盖拒绝。包括实际中文
输出目录、空格、内部引号、反斜杠、NUL 和超长逻辑命令，未缩减原测试。
原 native 助手 24 项、74-worker 库存、每端 194 项角色与 70 项路径检查通过。

独立读取器复核全部 40 个启动器回执、编译器／DLL／manifest、控制输入、
日志和报告哈希；不是仅相信运行脚本的成功摘要。

## 完整 worker 的终态，不计为通过

原 16 项复测已终止：4 通过、12 失败、0 未运行；输入保持稳定。四项为
class_copy_storage、module_executor、module_retirement_v4、network_service_runtime。
旧报告不回写为通过，native_adapters 的中途语义诊断也不算完整测试完成。

修正 channel 两处严格警告后，用 10,261 文件冻结输入复测两项完整 worker：
0 通过、2 失败。六个实际主体编译回执保留原驻留源范围：aggregate 1,226 个；
oracle 直接编译 1,225 个，其 solver 源通过原 fixture include 参与编译。
GCC O0／O2 未改为缓存 O0 驻留对象；TCC 也没有删源来规避参数长度。

两个真实后续问题仍需处理：GCC 链接组成缺少 `xlDestroyContext` 的正式实现；
TCC 编译 `xl6_xacme.c:358` 的自动 struct 数组初始化触发编译器内部错误。
本批解决参数传输，不声称两个 worker、当前工作区产品或长期任务通过。

## 证据

根：`F:/xlang-validation/five-groups-20261026/p7-parameterized/`。
`verified-c-response-r2.json` 是四组合实际执行；`tcc-staged-r2/setup.json`
绑定完整隔离发行；`verified-tcc-transport-final-r1.json` 独立核对上述证据、
10,261 冻结文件以及 `response-batch-r1`／`tcc-full-workers-r1` 的失败终态。
首轮中文路径失败、先前发现批次及所有原日志保留，没有重新解释成成功。

目标 active，未决策 CRT 0／XRT 11，内存池最后；无网络／密码学安全专项。
