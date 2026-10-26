# ui.auto

2026-10-26 当前 Windows 发布版默认包门禁复跑通过：严格 C 范例检查、
自建窗口的 C/XLang 同输出、定位边界、输入读取，以及 JIT/AOT
两条路径的真实截图和文字像素验证；具名类型与公开范例 AOT 也
构建运行成功。旧版 `xl6ValueDataSnapshotV1` 缺符号不再是当前阻断。
这只验收自建窗口和公开组合层，不推断真实第三方应用的权限、
DPI/多屏或远程会话稳定性。

Windows 桌面自动化组合层，底层为 `uia`，截图依赖 `opencv`。通过
`import ui.auto` 使用。定位器可按 automation ID、控件类、名称、
控件类型或进程 ID 建立，再用 `within(locator, hwnd)` 限定窗口，
`with_timeout(locator, ms)` 设置查询等待时间。

`wait_exists`、`click`、`type_text`、`read` 提供常见交互；
`screenshot` 将目标区域保存为图像。查不到目标时，
`wait_exists/click/type_text` 返回 false，`read` 返回空串。
测试窗口由 `uia.TestWindow` 管理，不再另建重复句柄类型。

定位器中非空的 automation ID、控件类、名称、控件类型与进程 ID
会组成交集，不会因前一个条件命中而忽略后面的条件。
文本定位条件与 `type_text` 经 `uia` 进入 C 前拒绝内嵌 NUL，
不会把前缀误当作完整条件或静默截断输入。
`locator_from_json` 接受这些字段及 UIA 生成的定位信息，但
`strategy` 若存在，只允许 `automationId`、`classNameName`、
`processClassPath`；未知值会报 `ValueError`，不会静默改用其他字段定位。
`processClassPath` 会沿 UIA Control View 的类名、控件类型及同签名兄弟序号
逐段定位；可与进程、名称、类名、自动化 ID、控件类型一并取交集，
不会退化为宽泛的进程匹配。路径不在 `within` 的根下、目标消失或
进程不匹配时视为未找到。显式 `index` 必须与路径末段一致。
JSON 字段类型必须正确，且至少有
一个搜索条件；负等待时间报 `ValueError`。
`within` 和 `with_timeout` 修改传入的 Locator 并返回同一实例。
`Locator` 现作为具名类导出，参数与变量可写为 `uiauto.Locator`；
多段包名用于类型声明时请用导入别名：`import ui.auto as uiauto`。
定位器只保存筛选条件，不持有 UIA 原生句柄；`uia` 的生命周期规则不变。

从 `demo6/release` 运行：

```powershell
./xl.exe run examples/extlibs/ui_auto.xl
powershell -NoProfile -ExecutionPolicy Bypass -File extlibs/ui.auto/test/check.ps1 -SkipAot
```

默认不带 `-SkipAot` 的门禁还构建运行具名类型、公开范例、定位边界
和截图四份 AOT；范例与 C/XLang JIT 对照，截图复用同一像素断言。
只有显式 `-SkipAot` 才跳过 AOT。当前发布版结果见本页开头。

对照 C 范例为 `examples/extlibs/ui_auto_c.c`，直接使用 `uia` 的 C API
组合控件类、类型与进程条件，并与 XLang 范例一样完成输入、读取和按钮点击。
包级门禁以严格 GCC 检查 C 范例，并逐行比较两份范例的输出。
包级门禁覆盖自建窗口上的成功/缺失/错误进程定位、生成的定位器
JSON 往返、类路径往返与相互矛盾的筛选条件、输入读取与含固定文字
像素的真实 PNG 截图。截图测试还把
目标 Edit 控件与父窗口的图像尺寸对照，防止目标截图退化为整窗截图。
截图写入门禁的独占临时目录，像素验证后自动清理；不会读取上次运行留下的图片。

2026-10-14 自建窗口包级门禁再次通过：XLang 定位与输入读取、截图像素、
C 对照范例均可调用。OpenCV 在本包只验收公开调用链，其独立桥接源码与内部
测试不作为 `ui.auto` 的发布条件。
2026-10-17 自建窗口截图进一步核对目标区域小于父窗口，Windows 包级门禁
通过；当前 Edit 控件返回自身 HWND，既有截图路径未发现整窗误存。

平台范围仍有限：`uia` 明确限定在创建线程使用，不提供并行 UIA 上下文；
真实桌面权限、DPI/多屏、远程会话、取消和长期事件处理仍需验证。

`test/boundaries.xl` 已登记为 `coverage.tests`；`ui.auto` 属于平台套件，通用目录门禁默认跳过，明确选择本包时才运行自建窗口边界。
