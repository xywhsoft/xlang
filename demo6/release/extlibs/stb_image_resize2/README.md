# stb_image_resize2

2026-10-25 在 WSL 中运行现有 `test/check.sh` 全通过：严格 GCC、
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

- 从 `demo6` 目录运行 `powershell -NoProfile -ExecutionPolicy Bypass -File
  release/extlibs/stb_image_resize2/test/check.ps1`，执行 Windows 严格 GCC、
  C 边界、中央与包内 XLang 用例、三种范例入口的固定输出对照，默认还
  构建并运行中央及公开范例 AOT；`-SkipAot` 只做 C/JIT 聚焦快测。
- 在 `release` 目录运行 `xl run examples/extlibs/stb_image_resize2.xl` 或
  `xl run examples/extlibs/stb_image_resize2_c.c`；C 范例直接包含解码与
  缩放实现，两个范例都从 `xl.exe` 所在目录定位素材，并对照 RGBA 与
  灰度+alpha 缩放结果。XLang 范例可在 `--` 后传入图片绝对路径，供
  独立 AOT 可执行文件使用。
- 从 `demo6` 目录运行 `xl run release/extlibs/stb_image_resize2/test/boundaries.xl`
  和 `xl run release/extlibs/stb_image_resize2/test/allocations.xl`；两项已登记
  到包级测试套件，后者对 sRGB/linear 各扫描前 128 个分配点，确认故障在
  XLang 中仍是 `MemoryError`，正常调用和解除故障后的结果均可用。
- `tools/run_extlib_c.ps1 -Package stb_image_resize2,stb_image -Source release/examples/extlibs/stb_image_resize2_c.c`
- C 边界测试使用 `-Package stb_image_resize2` 和
  `release/extlibs/stb_image_resize2/test/test_stb_image_resize2.c`
- `wsl bash release/extlibs/stb_image_resize2/test/check.sh` 可复跑 Linux 严格编译、
  ASan/UBSan C 边界、中央与包内 XLang 用例及 C/XLang 图片范例固定输出。
- 覆盖 sRGB/linear、同尺寸保持、带填充步长、短缓冲、逐点内存故障注入、
  C 整数收窄及参数/尺寸拒绝；
  C/XL 范例输出一致；Windows GCC `-Wall -Wextra -Werror` 与 Linux
  ASan/UBSan 已通过。

当前 Windows 默认门禁在中央核心 AOT 构建处因主线缺少
`xl6NativeHandleTraceLeaf` 而非零退出，见
`demo6/dev/extlib_blockers/native_handle_trace_aot.md`；带输出的完整 XL AOT
还可能遇到共享 `xl6ValueDataSnapshotV1` 缺符号，见
`demo6/dev/extlib_blockers/text_snapshot_link.md`。暂不标记发布验收完成。

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
