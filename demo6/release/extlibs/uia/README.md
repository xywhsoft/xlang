# uia

Windows UI Automation 扩展库。C 桥接源代码和公开头文件分别为
`x_uia_artifact.c`、`include/uia.h`；XLang 通过 `import uia` 使用。
非 Windows 平台不提供此包。

## 使用

- `uia.test_window()` 创建仅供测试的窗口；真实桌面入口为 `uia.desktop()`、`uia.from_hwnd(hwnd)` 和 `uia.from_point(x, y)`。
- `Element` 支持属性读取、快照、条件查询、Value/Invoke/Toggle/Text 等 UIA 协议。目标控件不支持的协议会报告错误，不能把失败当作普通布尔值忽略。
- `uia.events()` 提供焦点与属性事件。无事件时 `poll_json(0)` 返回 `{}`，不设置 XRT 错误。
- `poll_json` 等待事件时会处理当前线程的窗口消息；收到宿主的 `WM_QUIT` 时返回 `{}`，并将原退出码重新投递给宿主，不吞掉退出请求。
- `Events.poll_callback(handler, timeout_ms)` 从 UIA 事件队列取出 JSON，再调用 XLang 函数；回调返回值原样传回。`test/test_uia_focus_callback.xl` 演示具名函数处理真实焦点变化。
- `Process`、`TestWindow`、`Condition`、`Element`、`Events` 是拥有型句柄；XLang 自动析构，`release()` 可提前释放且再次调用返回 false。C 调用方必须使用对应的 `x_uia_*_release`；`xvalue` 元素数组中的每个原生句柄也须单独释放。
- UIA 上下文绑定首次使用它的线程；`Element`、`Condition`、`Events` 句柄及所有 UIA 操作只能在该线程使用。其他线程的调用会拒绝；如需交接，请在原线程释放所有句柄并调用 `uia.shutdown()`，再由新线程初始化。
- `shutdown()` 在仍有 UIA 句柄时拒绝卸载。C 返回的文本借用进程内缓冲，下次 UIA 调用前复制；`last_error()` 也是进程全局状态，不要跨线程持有。
- 对应的 `_owned` C 文本接口返回独立的 XRT 分配，调用方用 `xrtFree` 释放；XLang 的属性文本、快照、缓存子树、定位器和事件 JSON 直接接管这份分配。原 C 借用接口和调用约定不变；版本号、错误消息及固定短矩形 JSON 仍为借用接口。
- UIA 的 BSTR 文本转换按需扩容，`shutdown()` 释放该缓冲；不再因旧 8 KiB 槽而返回空文本。内嵌 NUL 或非法 UTF-16 明确报错，内存不足保留内存错误。文本仍只在当前 UIA 线程、下一次相关调用前借用。
- 快照、缓存快照和事件 JSON 按需扩容，完整输出上限为 16 MiB；超限或分配失败返回 `{}` 并设置错误，不返回截断的 JSON。快照中的名称、自动化 ID 和类名不再受旧 1 KiB 属性缓冲限制。
- `launch`、三个文本定位条件和 `Element.set_value` 在 XLang 侧拒绝内嵌 NUL，避免 C 字符串接口只使用前缀；拒绝的输入不会修改控件。
- 组合查询条件由系统 UIA 持有子条件；构造后可以先释放子条件。`find_all(..., timeout_ms)` 等待首次非空结果，`0` 表示即时快照；无匹配返回空数组，不设置错误。
- `Element.find_class_path(path, process_id, timeout_ms)` 与 C 的 `x_uia_element_find_class_path` 沿生成定位器所用的 Control View 树逐段查找，不混用原始 `children()` 树；每层只扫描一次同级元素，并在等待期限内检查超时。路径段包含类名、控件类型和同签名兄弟序号，类名中的 `/[]%#` 会转义。路径必须处于接收元素的子树内；目标或进程不匹配时返回空元素且不设置错误，畸形路径或 UIA 调用失败仍报错。

从 `demo6/release` 运行：

```powershell
./xl.exe run examples/extlibs/uia_quick.xl
./xl.exe run examples/extlibs/uia_quick_c.c extlibs/uia/x_uia_artifact.c --link user32 --link ole32 --link oleaut32
powershell -NoProfile -ExecutionPolicy Bypass -File extlibs/uia/test/check.ps1 -StrictC
```

默认门禁同时构建并运行隐式句柄清理、焦点事件回调、核心、完整公开范例和配对范例 AOT，并与 JIT/C 输出对照；当前 Windows 发布版 `-StrictC` 门禁已通过。开发时可加 `-SkipAot` 只检查 C/JIT，不能据此签收完整门禁。门禁把 AOT 与独立 GCC C 测试产物写入本次独占的 `demo6/build/test` 子目录，结束时仅清理该目录；不会再向 `release/build/test/uia` 写入新产物。

## 验收状态

已通过完整 XLang 范例、C/XLang 同输出范例、空事件轮询、整数 ABI 边界、自动/显式析构、TCC C 冒烟和独立 GCC/XRT C 构建。另覆盖 Win64 `VARIANT` ABI、严格 UTF-8、五个文本入口的 NUL 拒绝、子条件提前释放、跨线程拒绝、活跃句柄阻止卸载、真实焦点回调及短轮次事件订阅/释放。测试只操作包自己创建的窗口。

2026-10-26：焦点事件补 XLang 具名回调用例，确认事件 JSON 和回调返回值均能穿过 `poll_callback`；空队列不调用回调，回调抛错后事件句柄仍可释放。JIT 与独立 AOT 运行结果一致，完整 `-StrictC` 包门禁通过。这里验证的是“UIA 原生事件入队，再由 XLang 主动轮询并调用函数”，不是 UIA 在事件线程直接调用 XLang。

尚未覆盖真实桌面应用的权限隔离、DPI、远程会话和不同 Windows 版本，以及事件长期压力；单线程是明确的 API 限制，不支持同一 UIA 上下文的并行调用。旧构建曾缺 `xl6ValueDataSnapshotV1`，当前完整 XLang AOT 门禁已通过。

定位器仍使用定长字段和最多 16 层的类路径启发式；超长 `name`、`automationId`、`className`、路径段或深层树会返回 `{}` 并设置错误，不再静默丢失定位信息。调用方应检查错误；不要将定位器视为任意桌面结构下的无损表示。

2026-10-18：构造 9,001 字符 BSTR 的 C 回归先复现旧 8 KiB 转换失败，改为 XRT 可增长缓冲后通过；额外拒绝内嵌 NUL 和孤立代理项。TCC C、XLang 包内边界、自建窗口、双语范例、核心 AOT 与独立 GCC `-StrictC` 门禁全部通过，TCC 缺少的 Win32 原型只在 TCC 编译路径声明。自建控件的 UIA `Name`/`Value` 均只返回 4,096 字节，因此这份夹具不能证明真实第三方控件的超长属性端到端读取；该平台边界仍待专门验收。

同日补测快照和 JSON 输出：伪 UIA getter 返回 9,000 字符 BSTR 时属性完整保留；70,000 字符 JSON 超过旧 64 KiB 槽仍完整输出；16 MiB 上限明确返回错误而非半截 JSON。完整包门禁 `-StrictC` 通过。真实第三方控件的长属性和定位器无损性仍未验收。

定位器的固定字段、类路径限制和关键 UIA 遍历失败也改为显式错误；文本与数值属性读取失败不再生成带空值或零值的伪定位器。伪 getter 的超长属性与失败返回、普通定位器成功以及依赖它的 `ui.auto` 包门禁均通过。大规模真实桌面的定位器稳定性仍待验证。

原生层仅使用 XRT 值容器和基础错误接口，已删除源码中的全量 XRT 宏并在包清单声明 `XRT_MODULE_VALUE_CONTAINER`。同一最小 AOT 从 3,843,584 降至 929,280 字节，构建和运行通过；`test/aot_core.xl` 已纳入包级门禁。Windows `-StrictC` 下的 C smoke、自建窗口、XLang 边界和双语范例也通过。该裁剪不解除完整范例 AOT 的共享主线阻断。

`test/test_uia_boundaries.xl`、`test/test_uia_implicit_handles.xl` 和 `test/test_uia_focus_callback.xl` 已登记为 `coverage.tests`。本包属于平台套件，通用目录门禁默认跳过，只有明确选择 Windows UIA 包时才运行自建窗口用例。

2026-10-24：长 BSTR 文本和动态 JSON 返回改为所有权转移，移除 `extern as string` 对整段输出的重复复制。C 侧验证 70,002 字节 JSON 指针原样转移、后续缓存复用不改写旧结果、错误路径不转移；XLang 侧验证长文本和快照跨调用仍有效。C/JIT 与核心 AOT 检查通过；完整范例 AOT 尚未签收。

同日当前发布物复核：`TestWindow`、`Process`、`Element`、`Condition`、
`Events` 五种有效句柄在自然析构时先后因写退役字段报 `RuntimeError`；
聚焦用例逐段红绿验证，析构现只释放资源，显式 `release()` 保持原语义。
TCC OLE 头缺少的 `CoInitializeEx` 原型仅在 `__TINYC__` 下声明，
消除内嵌编译警告。Windows `-StrictC` 完整门禁通过，含五类对象 JIT/AOT、
自建窗口、公开范例与 C/XLang 对照；旧完整 AOT 缺符号结论不再适用。
