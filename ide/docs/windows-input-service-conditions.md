# Windows 输入服务的桌面条件核对

2026-10-12 已完成实际桌面条件对照。当前普通桌面的只读进程能取得已启用、活动的微软拼音配置；私有诊断桌面即使初始化本地文本服务，仍只能取得普通键盘布局。私有桌面的枚举结果不能代表日常 IDE 的输入法条件。

## 实际检查

私有桌面检查通过 Windows `InitLocalMsCtfMonitor(ILMCM_LANGUAGEBAROFF)` 初始化本地文本服务，初始化和相应释放均返回 0。程序明确核对自身桌面名称，使用工具新建的桌面，关闭本地语言栏；没有激活输入配置或切换桌面。

随后执行两种配置枚举和对微软拼音已知 CLSID／配置 GUID 的 `GetProfile` 查询。实际仍返回一个普通中文键盘布局、零个文本输入服务配置；直接查询返回 `0x80004005`。键盘布局及核对的当前用户键盘预加载、替代布局、拼音配置值均保持不变。原始结果见 [私有桌面检查](../build/tsf-local-monitor-v3/evidence.json)、[stdout](../build/tsf-local-monitor-v3/profiles.stdout) 和 [stderr](../build/tsf-local-monitor-v3/profiles.stderr)。

另一只读 CLI 进程在普通桌面查询相同接口，不创建测试窗口、不调用本地监控初始化、不激活输入配置。它实际枚举到微软拼音，并取得 `GetProfile=0`、`type=1`、`flags=3`；对应旧接口也返回 `active=true`、`enabled=true`。程序线程的键盘布局保持 `0x8040804`。原始记录见 [普通桌面检查](../build/tsf-current-desktop-readonly-v1/evidence.json) 和 [stdout](../build/tsf-current-desktop-readonly-v1/profiles.stdout)。

微软 API 合同见 [当前桌面文本服务初始化](https://learn.microsoft.com/en-us/windows/win32/api/msctfmonitorapi/nf-msctfmonitorapi-initlocalmsctfmonitor)、[对应释放](https://learn.microsoft.com/en-us/windows/win32/api/msctfmonitorapi/nf-msctfmonitorapi-uninitlocalmsctfmonitor)、[明确配置查询](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfinputprocessorprofilemgr-getprofile)。本次已实际执行这些调用；两种桌面结果不同，不以初始化成功代替输入法服务可用。

## 本机接口适配和保留的失败

本机 MinGW 头文件有两个监控函数声明，但缺少 `ILMCM_LANGUAGEBAROFF` 常量。第一次测试编译失败，源码、工具和日志保留在 `build/tsf-local-monitor-v1/`。诊断源码随后按微软 [SDK 头文件](https://raw.githubusercontent.com/microsoft/win32metadata/main/generation/WinSDK/RecompiledIdlHeaders/um/MsCtfMonitorApi.h) 的实际定义补充该常量，没有改写工具链安装目录或 IDE SDK。

第二次程序根据网页 DLL 字段在 `msctf.dll` 查找导出，实际缺少这两个符号，程序退出 22，未调用监控初始化；记录保留在 `build/tsf-local-monitor-v2/`。本机 PE 导出检查确认两个函数实际位于 `System32/MsCtfMonitor.dll`，第三次使用系统目录 API 构造明确路径，实际初始化、查询和释放成功。首次编译失败和导出缺失没有计作成功。

## 对 IDE 验收的影响

[TSF 上下文测试](tsf-context-verification.md) 已验证 C/xlang 编辑器的实际 Windows 文本存储读写、Unicode 选区、替换与撤销。本次对照进一步说明：继续重复私有桌面的配置枚举，无法补齐普通桌面中的微软拼音输入验收。

实际拼音组合、候选翻页和选择、重转换、系统快捷键，以及实体跨显示器 DPI 定位，继续使用普通桌面的真实 IDE 窗口进行设备验收。当前 [开发候选](../CURRENT-CANDIDATE.md) 可以作为该验收入口。两项自动检查均未驱动拼音引擎输入或候选窗口，`physicalImeVerified` 保持 false。

本次只有独立诊断程序、执行记录和说明新增；IDE 产品源码、固定 SDK、当前默认编译器、便携包和完整回归结果保持原样。正式首版未因此视为完成，长期目标保持 active。
