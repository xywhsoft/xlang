# dinov3

DinoV3 图片嵌入扩展库使用 OpenCV 预处理和 ONNX Runtime 执行模型。创建模型必须传入实际权重文件路径：`dinov3.DinoV3(modelPath)`。本包不携带权重，也不提供假默认模型。输入/输出名称、尺寸和归一化参数必须与所用模型匹配。

**尚未达到完整发布级验收。** 本机已有兼容的外部模型，可做真实图像推理；权重不属于本包，也不随发布物分发。C/XLang 同图输出及两条独立线程各一次推理已核对，完整 AOT 仍未签收。ONNX 桥接 DLL 只验调用链，其内部算法由插件自身维护。`release/examples/extlibs/dinov3.xl` 是缺模型负例和 ABI smoke；真实推理范例在 `examples/live_model.xl` 和 `examples/live_model_c.c`。

## 接口和所有权

`DinoV3Model.op_call(imagePath)` 返回 `DinoV3Output`。输入/输出名称配置拒绝内嵌 NUL，避免传给 ONNX C 接口时静默截断；空名称仍返回 `false`。输出对象独占 ONNX 结果句柄，`release()` 可提前释放，析构时自动释放；`shape_json()`、`count()`、`at()` 只借用句柄，不额外释放。不要将已释放输出的裸 `handle` 传给 C。C 公开接口在 `include/dinov3.h`；`embedding_json` 和 `output_json` 返回独立持有的 XRT 字符串，调用方用 `xrtFree` 释放，XLang 包装层自动接管。`last_error` 借用当前线程错误槽。

包内 C 结果处理直接复用 `onnxruntime` 的公开 C ABI，不再额外加载一份桥接 DLL。结果 shape 必须为非空维度数组，元素乘积与结果数一致，最多 100000 个浮点值；非有限值拒绝输出。JSON 使用 XRT 结构化写入，路径不受旧固定缓冲区限制。

`docs/site.html` 是官网详情页的可维护源文件，当前页面公开使用方法和待验收边界，不代表包已签收生产发布。

## 当前测试

Windows：`powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/dinov3/test/check.ps1`

Linux：`sh release/extlibs/dinov3/test/check.sh`

两套门禁以链接期五函数测试桩验证 C 结果处理：嵌入向量、shape/count、NaN、长度错误、长路径 JSON 和独立所有权；Linux C 使用 ASan/UBSan。两套门禁均运行 XLang 输入/输出名称 NUL 负例。Windows 另以发布目录的真实 `onnxruntime_xlang.dll` 验证 C 桥接装载，并运行 XLang 导入与缺模型负例。设置 `DINOV3_MODEL` 和 `DINOV3_TEST_IMAGE` 后，Windows 门禁额外运行 C/XLang 同图真实模型范例，要求 384 维输出且全部元素逐项误差不超过 `1e-5`；模型与图片路径均由调用方提供。加 `-ParallelLive` 时，再让两条线程各自创建会话、推理并对照串行结果。测试桩和两线程 smoke 均不代替后端长期稳定性认证；完整 AOT 仍待复核。

门禁每次使用独占临时构建目录并在退出时清理，不复用上次构建的可执行文件。

2026-10-15：manifest 声明原生结果处理实际使用的 XRT 值容器、JSON、
Buffer、once 和线程键模块。无模型 `test/aot_core.xl` 调用公开
`available()`，同一 Windows AOT 从 4,213,760 降至 1,351,168 字节，
构建和运行均通过，已纳入包级门禁；严格 C、真实桥接加载与缺模型
XLang 负例复跑通过。随后用本机兼容模型验证了真实 384 维推理和
C/XLang 同图 384 个元素对照及双线程各一次真实推理；未验证完整范例 AOT。

2026-10-18：使用本机外部兼容权重和
`release/extlibs/xfeat/test/assets/target.png`，重跑
`test/check.ps1 -ParallelLive`。严格 C 结果处理、真实桥接装载、
XLang 缺模型与名称边界、核心 AOT、C/XLang 384 维逐项对照及
两线程独立推理全部通过；权重仍由调用方提供，完整范例 AOT 未签收。
