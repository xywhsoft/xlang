# stb_image_resize2

2026-10-30：补齐最小行缓冲与失败清理边界，并确认逐块分配的OOM泄漏。
ASan会自动启用该路径：info尚未创建时的暂存分配、未登记的局部采样器
指针没有被释放，失败清理还丢失原生分配上下文。现于该宏内统一失败出口，
只登记已经拥有的资源，保留原共享系数/临时区的别名判断。未修改缩放算法、
公开ABI、正常单块路径或XRT。补丁前后哈希与机制见
[补丁复核](../../../docs/stb_image_resize2补丁复核_2026-10-30.md)。

新增C的360组行布局对照、16组不透明常量像素（含单行INT_MAX步长），
检查末行无padding、短一字节拒绝、输入未改、源释放后的独立输出与恢复。
四种缩放布局×两颜色模式逐点扫描：普通分配24个故障点、逐块154个；
另有非空原生分配上下文21个。两平台同一最终回归，旧逐块40个预算不平衡，
原生上下文另5个预算失败/202次错误上下文，修复后均0。语言新增40组
布局与分类捕获，登记coverage；新双语padded_rows例对照同一六行golden。

Windows默认39项/53输入工具指纹、Linux原生快照默认完整门禁均通过，
含Linux ASan/UBSan、原有图片/分配边界与新旧双语JIT/AOT，各8份独立程序；
58复制源和两实际Linux工具指纹不变。三个实际生成原型对正式头通过，
错改resize返回两平台均拒绝。Windows真实入口静默编译器raw37/outer1，
Linux原始37/127均保留现场；不再失败即删除本次目录。门禁后仅补README/
官网结果说明与根目录文档，未重新实现运行时。依赖stb_image的M3仍开放，
本包继续R，不把Buffer路径通过升级成依赖已闭环。证据在
build/test/resize_audit_1793336849243、/var/tmp/xlang-resize-audit-tpr6If3F；
最终Windows报告build/test/stb_resize2_guard_0b20f95d6dde489cbbc4aa77002fc1d1.report.json。

2026-10-26 重建 Windows/Linux x64 发布物后，两平台默认包门禁
均退出 0：严格 C、Linux ASan/UBSan、尺寸/步长/内存故障边界、
黑白中点的 sRGB 188 与 linear 128 像素断言、C/XLang 固定输出，
以及核心和双语图片范例的独立 EXE AOT。C 范例现接受显式图片路径，
不再要求独立产物与发布目录共址。旧共享 AOT 缺符号不再复现。
本次仅签收有界 Buffer 缩放接口，不替代依赖包 `stb_image` 的
私有构造器访问控制修复，也不承诺全部格式/滤镜或其他架构。

2026-10-25 历史记录：当时在 WSL 中运行 `test/check.sh` 全通过：严格 GCC、
C 边界 ASan/UBSan、XLang 边界与分配故障、C/XLang 三路范例的
固定输出对照。本次语言用例使用现有 Linux `release/xl`；没有
重建 Linux 发布版或运行 Linux 独立 EXE，因此不计作 Linux AOT
验收。

2026-10-24 当前 Windows 发布版默认 `test/check.ps1` 全通过：严格
GCC、C/XLang 边界与分配故障、三路双语范例固定输出，以及中央
用例和公开图片范例的独立 EXE AOT 构建、执行及结果对照。
下文共享 AOT 缺符号记录属于旧发布物；本轮未用新 Linux 发布版
重跑独立 EXE，不能外推为 Linux AOT 验收。

`stb_image_resize2` 使用 stb_image_resize2 2.18 缩放 uint8 图片。输入、输出
均为独立的 XRT `Buffer`；不暴露无长度裸指针，也不要求调用者管理额外的
图片句柄。`resize` 使用 sRGB 颜色空间缩放，`resizeLinear` 使用线性空间，
两个函数的参数一致：源像素、源宽高、通道数、目标宽高，以及可选的输入
行步长（字节数，`0` 表示紧密排列）。
输入最小长度是 `(height - 1) * 实际步长 + width * channels`，最后一行
不要求额外 padding；缩放不修改源像素或 padding。包内
`examples/padded_rows.xl` / `padded_rows_c.c` 用同一个五字节灰度输入展示
最小长度、sRGB/linear 的 188/128 中点、短一字节拒绝及恢复，六行输出一致，
不依赖图片文件或解码对象。仓库根目录运行：

```text
release/xl.exe run release/extlibs/stb_image_resize2/examples/padded_rows.xl
release/xl.exe run release/extlibs/stb_image_resize2/examples/padded_rows_c.c
```

Linux 使用 `release/xl`。旧图片范例和解码依赖门禁仍保留，不由内存例替代。
2 通道像素按灰度+alpha 解释，4 通道按 RGBA 解释；透明像素的颜色不会
参与可见颜色的加权缩放。若第二或第四通道并非透明度，应先按实际像素
布局转换，再调用此接口。

```xlang
import stb_image
import stb_image_resize2

stb_image.StbImage image = stb_image.load("photo.png", 4)
Buffer small = stb_image_resize2.resize(image.pixels(),
    image.width(), image.height(), image.channels(), 320, 240)
```

宽高必须为正且各不超过 16384；通道数限 1 到 4；输入与输出各限
64 MiB。调用前检查输入有效长度、步长和整数乘法，失败在 XLang 中抛出
`ValueError`，原生接口通过 XRT 错误通道报告。XLang 入口在收窄为 C
`int` 前校验源/目标尺寸和步长，避免大整数回绕成有效参数。输入只借用；C 返回的
`xbuffer*` 要用 `xrtBufferDestroy` 释放。C 声明见
`include/stb_image_resize2_native.h`，许可证见 `LICENSE.stb.txt`。
原生缩放入口会清除本线程上一次的 XRT 错误；失败保留本次诊断，随后成功不再携带旧错误。

第三方头文件对 `STBIR__SEPARATE_ALLOCATIONS` 的未使用局部变量做条件
编译收紧，并修正逐块分配失败时释放未初始化指针的问题；不修改缩放算法。
stb 工作区使用 XRT 分配器，内存不足时返回 `XERR_MEMORY`，不误报为
普通缩放失败。正常发布构建的单块分配路径不增加清零成本。

包级验证：

- 从仓库根目录运行 `powershell -NoProfile -ExecutionPolicy Bypass -File
  release/extlibs/stb_image_resize2/test/check.ps1`，执行 Windows 严格 GCC、
  C 边界、中央与包内 XLang 用例、三种范例入口的固定输出对照，默认还
  构建并运行中央与 C/XLang 公开范例 AOT；`-SkipAot` 只做 C/JIT 聚焦快测。
- 在 `release` 目录运行 `xl run examples/extlibs/stb_image_resize2.xl` 或
  `xl run examples/extlibs/stb_image_resize2_c.c`；C 范例直接包含解码与
  缩放实现，两个范例不传路径时从 `xl.exe` 所在目录定位素材，并对照
  RGBA 与灰度+alpha 缩放结果。独立 AOT 可执行文件应显式传入图片路径；
  C 范例接收首个命令行参数，XLang 范例在 `--` 后传入。
- 从仓库根目录运行 `xl run release/extlibs/stb_image_resize2/test/boundaries.xl`
  和 `xl run release/extlibs/stb_image_resize2/test/allocations.xl`；两项已登记
  到包级测试套件，后者对 sRGB/linear 各扫描前 128 个分配点，确认故障在
  XLang 中仍是 `MemoryError`，正常调用和解除故障后的结果均可用。
- `tools/run_extlib_c.ps1 -Package stb_image_resize2,stb_image -Source release/examples/extlibs/stb_image_resize2_c.c`
- C 边界测试使用 `-Package stb_image_resize2` 和
  `release/extlibs/stb_image_resize2/test/test_stb_image_resize2.c`
- `wsl bash release/extlibs/stb_image_resize2/test/check.sh` 可复跑 Linux 严格编译、
  ASan/UBSan C 边界、中央与包内 XLang 用例，以及核心和双语范例独立
  EXE 的固定输出。
- 覆盖 sRGB/linear、同尺寸保持、带填充步长、短缓冲、逐点内存故障注入、
  C 整数收窄及参数/尺寸拒绝；
  C/XL 范例输出一致；Windows GCC `-Wall -Wextra -Werror` 与 Linux
  ASan/UBSan 已通过。

历史阻断：旧 Windows 发布物的中央核心 AOT 曾缺少
`xl6NativeHandleTraceLeaf`，见
`demo6/dev/extlib_blockers/native_handle_trace_aot.md`；带输出范例也曾缺少
`xl6ValueDataSnapshotV1`，见 `demo6/dev/extlib_blockers/text_snapshot_link.md`。
2026-10-26 两平台默认包门禁已验证不再复现。

2026-10-23：Windows 聚焦门禁、Linux C ASan/UBSan 与 XLang 双语门禁均
退出 0，Linux 范例已与固定黄金输出逐字节核对。Windows 默认门禁在中央
核心 AOT 构建处按上述共享缺符号非零退出，公开范例 AOT 尚未执行；独占
临时目录没有残留。此次只加强验收，不修改缩放算法或公开 API。

2026-10-15：C 入口失败后成功的 XRT 错误状态测试先红后绿，缩放成功不再保留旧诊断；源码模块宏改为条件定义。中央无输出 AOT 用例构建运行通过，C/XLang 边界、双语逐字同输出及新增 Linux ASan/UBSan 门禁通过；带输出完整 AOT 仍受共享快照符号阻断。

同日追加：sRGB/linear 两条路径的 XRT 分配故障扫描先复现工作区未纳入故障注入；改由 XRT 分配工作区后，Linux ASan/UBSan 进一步复现 stb 逐块分配失败时访问未初始化指针。包内 vendored 头文件仅修复失败清理所需的指针初始化与空表保护。Windows C/XLang 边界、双语范例、严格 GCC 语法检查和 Linux ASan/UBSan 复跑通过。

2026-10-18：修复 2 通道灰度+alpha 被当成两个普通颜色通道缩放。C 用例对照 stb 的 `STBIR_RA` 语义先红后绿，XLang 用透明白/黑对照固定 sRGB 与线性缩放行为。C/XLang 公开范例也展示这个边界；边界测试从 `xl.exe` 所在目录定位素材，不依赖调用方工作目录。

同日补全 Linux 包级脚本的语言路径：中央用例、包内边界与分配失败用例、
C/XLang 双语图片范例同次运行，严格 GCC、C ASan/UBSan 联合通过。
完整带输出 AOT 的共享主线阻断不变。

2026-10-19：新增 Windows 包级 `test/check.ps1`，将严格 GCC、C 边界、
中央及包内 XLang 用例、单文件 C 直跑、包运行器和 XLang 范例合并验证；
三份范例必须与固定的缩放字节数黄金输出一致。当前 Windows 门禁通过，
未修改缩放实现；完整带输出 AOT 仍未验收。

同日补灰度+alpha 的独立像素黄金值：透明白与不透明黑两像素缩为一像素，
sRGB 和 linear 两种模式均须得到灰度 0、alpha 128。C 不再只与同一份 stb
实现互比，XLang 也逐字节核对预期结果。Windows 严格 C/XLang 门禁与 Linux
C ASan/UBSan、XLang、双语范例门禁均通过。此向量只锁定透明度加权边界，
不代表已认证所有缩放滤镜；完整带输出 AOT 的共享阻断不变。

2026-10-20：96×56 灰度图缩为 32×32 时，Linux UBSan 复现 vendored
`stbir__pack_coefficients` 将 4 字节对齐的浮点系数强转为 8 字节整数指针
写入。三个非 SIMD 复制宏现改用 `memcpy`，避免未对齐访问和别名违规；
新尺寸向量已进入本包 C 边界用例，先红后绿。Windows C/XLang 门禁、
Linux ASan/UBSan 与上层 `imagehash` 双语门禁均通过。同步上游 stb 版本时
需保留或重新验证这处本地修复；完整带输出 AOT 阻断不变。
