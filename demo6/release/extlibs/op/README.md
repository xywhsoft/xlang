# op

2026-10-24：更新后的 Windows 发布版执行默认 `test/check.ps1`，
包装层严格 C ABI、C/XLang 只读调用、错误参数、随机数与等待边界、
双语范例同输出以及核心/公开范例 AOT 均通过。旧的带输出 AOT
共享符号缺失属于先前发布物；本轮只签收公开调用层，没有运行
`-Stress`、`-RealGpu` 或替插件验证设备/驱动矩阵及长时 hook 稳定性。

普通扩展库任务只验收 XLang/C 声明、加载、调用、错误和结果所有权。
设备/驱动矩阵与 hook 长时稳定性由插件单独维护。下文保留既有验证记录，
不作为本任务继续扩大测试的门禁。
本机 `op.xl`/`op_c.c` 只读范例及带窗口夹具的 XLang 调用 smoke 已通过；
无输出核心及带输出双语范例 AOT 均已通过。2026-10-26 当前 Windows
默认门禁再次退出 0；设备/驱动、长时 hook 和真实 GPU 场景不属本任务的
公开调用层验收。

包清单声明包装层直接使用的 XRT buffer、文件、互斥、once、路径、字符转换、默认随机及时间模块。
同一屏幕尺寸查询和对象创建探针原在裁剪前后为 3,660,800 / 746,496 字节；
加入有界随机和等待模块后的当前裁剪产物为 752,640 字节并运行通过，
不把 hook 代码作为该可执行文件的运行依赖。

Windows 桌面自动化与图像捕获扩展库，提供窗口绑定、屏幕/窗口采集、输入模拟、颜色搜索和进程工具。D3D9、D3D11、WGL hook 还依赖各自的 DLL；C 源码及预编译 DLL 都在 `hooks/`。该包具有较强平台和桌面会话约束，不能把无界面的导入测试当成真实图形捕获验收。
`bindWindow(hwnd, display, mouse, keypad, mode)` 的显示模式只接受
`normal`、`gdi`、`dxgi`、`opengl`、`d3d9`、`d3d11`、`wgl`；
鼠标和键盘模式分别只接受 `normal`、`windows`。空字符串采用 `normal`，
`mode` 当前只能为 0。未知或过长模式会报错，且不会解除已有窗口绑定；
显示模式是否能成功捕获仍取决于对应的设备、驱动和目标窗口。
公开 C 接口的 64 位整数目前写作 `long long`，XLang 侧写作 `int`；本包仅支持 Windows，当前 MinGW 下两者与 `int64_t` 为同一 C 类型，默认 C/XLang 调用门禁通过。Linux LP64 下 `int64_t` 是另一 C 类型，但本包不提供 Linux 桌面实现，不能据此宣称跨平台 ABI 已验收。

公开 C 声明见 `include/op.h`，原生实现与 C 测试直接包含同一头文件。
XLang 公开接口把字符串交给零结尾 C 参数前统一拒绝内嵌 NUL，避免
窗口模式、标题、文件路径、颜色条件或文本只按有效前缀执行；C 调用方
仍需自行保证传入正常的零结尾字符串。包内 `test/nul_arguments.xl`
用自建窗口夹具验证拒绝发生在绑定和查找之前。
`xop_create` 返回的句柄由 `xop_destroy` 释放。C 头中已改为 `void *` 的文本
结果由 XRT 独立分配，C 调用方用 `xrtFree` 释放，XLang 由 `takeCstr` 接管；
窗口标题/坐标/枚举、进程/模块列表、屏幕尺寸、剪贴板读取，
以及 `Op` 的颜色/图片搜索、帧信息、图片哈希和 `captureToMem` 已走这条路径。
`getColors`、`findColorEx`、`findPicEx`、`captureToMem` 不再以 64 KiB 缓冲区
静默截断；结果上限为 16 MiB，超限返回空指针并设置错误。
C/XLang 门禁验证 69,999 字节颜色列表、80,008 字节像素串和连续调用独立性；
C 还用真实 BMP 验证 `findPic` / `findPicEx` / `findPicN` 的命中结果。
颜色搜索条件严格使用 `RRGGBB` 或 `RRGGBB-DRDGDB`，可加 `@` 取反，
多个条件用 `|` 分隔且最多 64 个。多点偏移为有符号 32 位整数
`x|y|RRGGBB`，逗号分隔且最多 128 个；图片搜索的 `deltaColor`
可为空或恰好六位十六进制数。错误字符、溢出、空项和超限都明确报错，
不会截断后按有效前缀搜索；极端偏移只会安全地返回未命中。
`readString`、`findMemory` 也返回独立结果：读取须指定 1 至 16 MiB 的完整
可读字节范围，遇到非 UTF-8 文本拒绝；内存模式支持十六进制字节与 `??` 通配字节，
搜索范围为 `[start, end)`，跨连续可读页匹配，不跨不可读区域拼接。
XLang 内存夹具还验证 `writeString` 写入后可读回；带内嵌 NUL 的写入在调用
C 接口前抛 `ValueError`，原目标内容保持不变。
读操作只申请读取/查询权限，写操作单独申请写入权限；部分读取/写入报告错误。
`writeInt` 的目标内存为 C 32 位 `int`，超范围整数会报错；`writeFloat`
的目标为 C 32 位 `float`，NaN、无穷大、溢出及非零值下溢为零时会报错，
两种拒绝均不会改动目标内存。C/XLang 内存门禁覆盖这些边界。
`findWindow`、`getWindowTitle`、`setWindowTitle`，以及
窗口枚举的标题/类名过滤，已通过 XRT UTF-8/UTF-16 转换使用 Win32 宽字符接口；
隐藏窗口的中文标题 C/XLang 双语回归通过。进程/模块枚举已改用
`Process32FirstW` / `Module32FirstW`，UTF-8 名称过滤与输出在中文进程、
中文 DLL 的独立子进程门禁中通过；Toolhelp、user32、OpenGL 的首次动态加载
由 XRT once 串行化。`keyPressStr` 和 `keyChar` 以 Unicode 字符发送，
补充平面字符拆成 UTF-16 代理对；无效 UTF-8/码点直接报错。
`dragPath` 接受逗号分隔的有符号 32 位整数坐标，至少两对、最多 256 对；
不完整坐标、空项、非整数、溢出及超限均在发送输入前报错。
延迟 0 使用默认 20 毫秒，负数及超过 32 位正整数范围会报错。
`keyCombo` 以 `+` 分隔 1 至 8 个键名，可在键名两侧使用空格或 tab；
空键、未知键、超长键名、超过 8 键和带尾随字符的功能键名均整体拒绝，
不会执行已解析的前缀。输入调用测试只向本进程隐藏窗口投递消息。
剪贴板文本改用 `CF_UNICODETEXT`，写入前完成 UTF-8 转码；读取超过
16 MiB 显式限额时报错而不静默截断。C/XLang 隐藏窗口输入测试和
独立 window station 的 XLang 剪贴板往返测试均进入普通门禁，
不触碰用户桌面的剪贴板。BMP 保存/加载改用 XRT 文件 API，
中文路径往返和畸形头/截断文件拒绝用例进入普通门禁；读取范围固定为
4096×4096 以内、24/32 位未压缩 BMP，不把其他 BMP 变体误判为可用。
`imageHash` 与 `imageCompare` 在像素索引前验证 64 位坐标并求帧交集；
帧外极值和负起点的 C 负例已覆盖，其他图像入口仍需单独审计。
颜色/模板搜索现共用闭区间求交函数，先判断 64 位坐标再转帧内整数；
纯 C 极值矩阵与公开入口空区域负例进入普通门禁。
`test/text_threads.c` 现在验证跨线程持有独立文本结果并进入普通门禁；
本进程受控内存测试覆盖跨保护区域命中、短范围不误报、70 KiB 文本、
错误模式与 C/XL 一致输出；尚未覆盖其他权限或位数的目标进程。
TCC `__thread` 的独立复现见 `demo6/dev/extlib_blockers/tcc_jit_tls.md`。

**插件本体未做完整发布认证；本任务仅验收调用层。** 此前统一 hook 生产端与消费端的帧容量检查。共享映射改为约 64 MiB，可容纳至 4096x4096 BGRA 帧；超限尺寸、异常帧头、无效 pitch 和分配失败均拒绝，WGL 仅为可容纳帧分配缓冲。映射容量属于资源上限，不保证任意分辨率或多屏可用。

共享协议升级为 `OPH2` / `op_hook_v2_<pid>`。生产者用奇偶帧序号标记写入中/已完成；消费端复制后复查序号，只返回稳定的整帧。三个 hook DLL 已从本包源码重新构建，与新协议同步；旧协议映射/DLL 不兼容。本轮的并发测试让读写线程交替运行一万次，并验证读取结果不会混合两帧。它不是实际游戏进程的注入与设备测试。

另按 Windows SDK 头文件核对并修正 DXGI/D3D11 桥接 ABI：`AcquireNextFrame` 的帧信息结构为完整 48 字节，`ID3D11DeviceContext.CopyResource` 为槽位 47，`GetImmediateContext` 为槽位 40；映射结构首字段是 `pData`，读访问位是 `0x20000`。设备经 `IDXGIDevice.GetAdapter` 获取适配器，再由输出接口查询 `IDXGIOutput1`。DXGI 捕获仅在等待超时复用旧帧；拒绝不支持的格式/尺寸，staging 创建、Map 或行跨度失败不再返回伪成功，新帧准备好之前保留旧缓冲。D3D9 的 `GetBackBuffer` 改为 SDK 槽位 18，表面尺寸来自 `GetDesc` 而非猜测。

D3D11 hook 初始化改在 DLL 加载完成后的线程进行，临时 swapchain 使用本进程隐藏窗口。捕获回调从当前 swapchain 获取真实设备与 context，按设备复用 staging，不再跨设备复制纹理。包内 `test/d3d11_capture.c` 用真实 D3D11 渲染红色帧，验证共享映射整帧尺寸、首尾像素与颜色；本机硬件在 64×64 和 1920×1080 两组均通过。hook 导出的状态/错误/调用次数/捕获阶段只用于诊断注入与设备路径，不属于 XLang 公开 API。

D3D9 后缓冲区读回使用 `GetRenderTargetData` + 系统内存 surface，探针设备创建在 DLL 加载后的线程中，按实际设备复用 staging。原先仅修改探针 vtable 时，本机稳定复现 `status=1, calls=0`；现改为对共享 `Present` 实现安装函数级 hook。跳板、指令边界和线程暂停复用 [MinHook 官方 v1.3.4](https://github.com/TsudaKageyu/minhook)（固定提交 `c3fcafdc10146beb5919319d0683e44e3c30d537`），包内仅包含 x64 所需源码与 `hooks/third_party/minhook/LICENSE.txt`（BSD-2-Clause/HDE 许可）。D3D9 hook 单独由 MinGW GCC 构建，D3D11/WGL 构建方式不变；发布 DLL 只依赖 Windows 系统 DLL。DLL 安装 hook 后固定到目标进程生命周期，不支持显式卸载。独立进程 `test/d3d9_capture.c` 在本机真实设备的 64×64 和 1920×1080 红色帧上均验证了完整尺寸与首尾 BGRA 像素；这证明本机 `IDirect3DDevice9::Present` 路径，不覆盖 D3D9Ex `PresentEx`。

包入口的 hook 状态已从全局单例收进每个 `op_context`。注入路径由 XRT 构建 UTF-8 路径并通过 `LoadLibraryW` 加载；远程写入、线程完成及加载结果必须检查，绑定目标退出后不再返回映射里遗留的旧帧。`test/d3d9_remote_capture.c` 同时启动红、蓝两个独立进程并以两个 context 分别抓帧，验证其中一个退出后另一个仍可读取；还会从中文路径下的运行时加载 DLL。此验证仅覆盖本机同权限、同位数 D3D9 目标，不等于支持任意进程注入。

远程 DLL 加载完成后，注入路径立即释放目标进程内的 UTF-16 路径缓冲；此前成功路径因转移进程句柄而漏掉了这一步。等待超时后，context 保留远程线程/进程句柄与路径状态，下一次捕获只轮询同一加载，不重复注入；完成或失败时释放路径。`test/hook_timeout.c` 用受事件阻塞的测试线程验证成功、失败、等待和解绑分支，真实 D3D9/WGL 注入回归保持通过。若在远程线程仍运行时解绑/销毁，包内代码可能随 JIT 上下文卸载，不能启动跨生命周期的本地回收线程；此时只关闭本地句柄，少量远程路径内存留待目标进程退出回收。这是明确的残余资源边界，长期服务中频繁超时又反复解绑的场景尚未签收。

WGL 原实现只改主模块 IAT 中的 `wglSwapBuffers`，对常见 `gdi32!SwapBuffers` 路径无效；包内真实 OpenGL 探针稳定复现“100 次交换仍无帧”。现用同一份 MinHook 固定 `gdi32!SwapBuffers` 入口，在交换前读取 `GL_BACK`，按行翻转为顶端起始的 BGRA，并恢复读取缓冲区与像素打包对齐设置。发布 WGL DLL 改由 MinGW GCC 构建；真实 OpenGL 探针在 64×64 和 1920×1080 两档验证首尾像素。另以两个独立 OpenGL 进程验证红/蓝帧隔离、单目标退出以及中文运行目录加载，均在本机同权限、同位数环境通过；多窗口目标选择和 FBO/PBO 绑定状态尚未验收。

普通/GDI 帧现先校验尺寸和 256 MiB 上限，再事务性创建 DC、位图与像素缓冲；失败不丢弃旧帧。GDI 模式按客户端区域调用 `PrintWindow(PW_CLIENTONLY)`，普通模式检查 `BitBlt` 返回值；读回前将位图从 DC 取消选入，并要求 `GetDIBits` 返回完整扫描行数。屏幕外自建 8×8 窗口的红色首尾像素由 C/XLang 双语测试验证，C 用例还覆盖过大尺寸拒绝及原帧保持（`test/gdi_frame.c`、`test/gdi_xlang_frame.xl`）。第三方窗口是否能响应 `PrintWindow` 仍由目标程序决定，返回成功也不能证明它实际绘制了预期内容。

`opengl` 直接模式与注入式 `wgl` 不同：调用线程必须已有绑定到目标窗口的当前 GL 上下文；缺少上下文时显式报错，不再降级为 GDI。该路径只分配像素帧，不创建 GDI DC/位图，读回后翻转为顶端起始的 BGRA，并恢复 read-buffer、打包对齐、行长度及跳过行/像素状态。已在本机 GL 4.6 的 8×8 真实上下双色帧验证，同时验证 PBO/FBO 非默认绑定会被拒绝（`test/opengl_direct.c`，纳入 `-RealGpu` 门禁）。其他 GL 版本/驱动、GL 错误队列与长期资源占用仍未签收。

Windows 默认门禁（在 `demo6` 目录执行）：`powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/op/test/check.ps1`。它验证 GCC 包装层 ABI、C/XLang 只读调用、范例同输出、无输出核心 AOT 以及带输出公开范例 AOT/C 对照。`-SkipAot` 只跑包侧 C/JIT 聚焦检查，不作为发布签收；当前默认路径在公开范例构建处因共享 `xl6ValueDataSnapshotV1` 缺符号准确失败。`-Stress` 保留帧尺寸/协议、SDK 槽位、hook 编译、窗口/进程/内存与 UTF-8 等插件专项测试；`-RealGpu` 还运行 D3D9、D3D11、WGL、OpenGL 和 DXGI 的真实图形测试，需要可用 Windows 图形设备。默认只读测试不移动鼠标、不改剪贴板；真实 GPU 测试会创建和移动自己的临时测试窗口。GCC 包源码检查仅屏蔽动态加载函数指针转换和遗留未使用参数告警。Linux 门禁 `sh release/extlibs/op/test/check.sh` 只跑平台无关帧尺寸测试和 ASan/UBSan，不声称支持 Linux 桌面自动化。

hook 帧边界和快照协议测试在 `-DNDEBUG` 下仍启用断言；Windows
`-Stress` 与 Linux 门禁均以发布宏编译相关测试。两份测试在 Windows
严格 GCC 发布宏下单独编译、运行通过；这不扩张为真实 GPU 或插件内部验收。
Windows/Linux 包门禁使用本次独占的 `build/test/op/run_*` 目录，退出时
只清理该目录，不覆盖旧的固定测试产物或另一份并行门禁的文件。

默认门禁的核心与公开范例 AOT 产物每次使用独占文件名，并在成功或失败后清理本次文件；并行运行不会复用旧产物。这只验证公开调用层，不扩大插件内部测试范围。

2026-10-18：XLang 包装层的零结尾 C 文本参数统一拒绝内嵌 NUL；
`bindWindow("normal\0ignored")` 原会按 `normal` 成功绑定，负例先红后绿。
公开方法不暴露新设的原生直通入口，默认门禁的 C/XLang 调用、
NUL 边界和无输出 AOT 均通过。插件内部算法与设备矩阵未扩测。

默认门禁的 GCC ABI 检查按 manifest 传入 XRT 模块宏，覆盖包装源码重复定义宏的构建错误。
默认 `release/examples/extlibs/op.xl` 与 `op_c.c` 只读取屏幕尺寸并展示未绑定的上下文。
XLang 范例可直接运行 `release/xl.exe run release/examples/extlibs/op.xl`；
C 范例使用 `tools/run_extlib_c.ps1 -Package op -Source release/examples/extlibs/op_c.c`，
由包清单提供 `op_native.c` 和 Win32 链接库。单独执行 `xl run op_c.c`
不会自动读取扩展库清单，因而不是这个 C 范例的运行命令。
`op.randomInt(min, max)` / `xop_random_int` 使用 XRT 线程独立的非密码学随机状态，
无偏返回完整 int64 闭区间 `[min, max]`；两端相等时返回该值，反向区间报告
`ValueError` / `XERR_VALUE`，不会再静默返回最小值。它不用于密钥、令牌等安全用途。
C/XLang 极大区间与失败后恢复回归已进入默认门禁，独立 XLang 用例也登记到目录 smoke。
`op.delay(ms)` / `xop_delay` 接受非负毫秒数，零值只让出当前时间片；
负数报告 `ValueError` / `XERR_VALUE`。超过可安全换算成 uint64 微秒的值
报告 `RangeError` / `XERR_RANGE`，不会再因 Win32 `DWORD` 收窄而变成短等待或无限等待。
实现直接调用 XRT 的分段睡眠接口；C/XLang 的无效值及失败后恢复进入默认门禁。
普通 Windows 门禁还用本进程隐藏窗口运行 C/XLang 双语 UTF-8 标题读写、查找及键盘字符回归，
不改动用户桌面的现有窗口。
它另用临时中文文件名子进程和 DLL 验证进程/模块名称及窗口筛选，
并让 12 个线程并发触发首次 Toolhelp 装载；子进程结束后退出。
原有交互演示移至 `op_interactive.xl`，须显式传入
`-- --allow-desktop-effects`；它会修改剪贴板、移动鼠标并暂时写入 BMP，
仅在可交互桌面中主动运行。

插件专项维护范围：多窗口选择和 WGL hook 的 FBO/PBO 状态、D3D9Ex 与更多设备矩阵、跨权限/位数的注入策略、多屏、DPI、中文目标进程路径、超时后解绑的残余远程路径与长时资源占用；远程进程权限/位数与内存布局矩阵，更完整的剪贴板容量/格式验证及 BMP 变体矩阵。DXGI 目前只选择设备适配器的第一个输出；本机 3840x2160 的单/双实例捕获已通过，进程内复用单个 duplication，并在等待超时时从共享最近帧向每个 context 复制独立帧。多显示器、桌面切换、设备丢失和长期并发仍待专项验证。`xl build examples/extlibs/op.xl` 能编译包源码，但链接仍被共享主线符号 `xl6ValueDataSnapshotV1` 缺失阻断，本任务不修改 xlang 主线。

2026-10-17：`randomInt` 的旧 `rand() % (int)range` 在 2^32 宽区间可除零，完整 int64 范围还会发生有符号溢出；反向区间原静默返回 `min`。C/XLang 最小负例先复现反向区间问题，现统一复用 XRT 无偏闭区间随机接口。Windows 默认包门禁的 GCC 语法、C/XLang 边界、只读调用、双语范例及核心 AOT 均通过；GPU 与完整带输出 AOT 状态不变。

同日补 `delay`：旧实现将 64 位毫秒数直接收窄为 Win32 `DWORD`，负数静默跳过，成功还保留旧 XRT 错误。C 红色复现旧错误污染；现用 XRT 分段等待并拒绝负数/微秒换算溢出。C/XLang 定向用例和 Windows 默认 GCC/C/XL/双语/核心 AOT 门禁通过；当前核心 AOT 为 752,640 字节，完整带输出 AOT 阻断不变。
