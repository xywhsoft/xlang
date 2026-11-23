# 拥有式离屏渲染上下文

在 `XGE_RUN_MANUAL` 模式初始化引擎后，调用
`xge.createOffscreenContext(width, height)` 可以在不显示窗口的情况下渲染。
Windows 使用隐藏的 WGL drawable，需要 OpenGL 3.3 Core 驱动。
启用 EGL 的 Linux SDK 使用 GLES 3 PBuffer，默认 Linux SDK 返回 checked
`XGE_ERROR_UNSUPPORTED`。Linux 仍可用 `createEglContext(width, height, true)`
显式选择无 surface 模式。

`context.info()` 提供尺寸、GPU 后端、当前绑定状态和渲染就绪状态。
`xge.platformCapsGet()` 对这两种离屏后端报告 offscreen 能力。
同一个引擎一次只允许一个拥有式渲染上下文，整个上下文由创建线程管理。
上下文关闭后，仍由外部持有的 RenderTarget 可以继续回读。引擎关闭会报告
Busy，直到剩余的 GPU 子资源释放。打开下一套上下文前，需要关闭并重新初始化引擎。

`offscreen_context.xl` 演示创建上下文、清除 2×2 RenderTarget、回读四个 RGBA
像素并按顺序释放。Windows 使用默认 SDK，Linux 无显示服务器时使用下述命令和
启用了 EGL 的 SDK。

```sh
LIBGL_ALWAYS_SOFTWARE=1 EGL_PLATFORM=surfaceless xl run offscreen_context.xl
```

Windows WGL 和 Linux Mesa EGL 的示例及生命周期夹具通过了 JIT、动态 AOT 和静态
AOT。Linux 默认 SDK 验证了 unsupported 路径。Windows 原生测试覆盖了 OOM、
错误线程、重新绑定和失败后的物理清理重试。Linux EGL 还通过原有的上下文及
故障注入测试。

相同 4×4 上下文的创建与关闭在 C 中各有 1 次 XRT 分配和释放，在 xlang 中各有
4 次，额外操作来自语言拥有对象和代码驻留管理。预热后的 1,000 次 `info()`
查询在两边均无 XRT 堆操作。Windows 样本为 C 0.023–0.073 ms、xlang
0.587–2.448 ms，Linux Mesa EGL 样本为 C 0.345–0.400 ms、xlang
0.910–1.710 ms。计时包含循环、状态检查和语言视图管理。创建与关闭的耗时
另行记录，其中包括驱动与系统窗口操作。

[跨平台离屏渲染验收](../dev/release-materials/provenance/xge-offscreen-context-20261124.json)

## 显式 EGL 模式

`xge.GraphicsContext` 支持宿主在语言模块关闭后继续使用自己持有的上下文。
Linux 的 Mesa EGL 配套已通过模块全局上下文的退役与原生库卸载测试。
正式 SDK／IDE 配套和 Windows 显式 EGL 配置仍待验收。

启用 EGL 的配套 SDK 可以在 `XGE_RUN_MANUAL` 引擎上调用
`xge.createEglContext(width,height,surfaceless=false)`，创建真正接入 XGE 渲染器的
上下文。默认 SDK 会 checked 拒绝。Linux 可用显式 EGL SDK 和 Mesa surfaceless
平台进行无显示服务器绘制；Windows EGL SDK 仍未验收。旧 raw EGL／Offscreen
记录不作为这个安全类的替代。

`context.info()` 返回精确的 `GraphicsContextInfo`，编译器初始化记录 `size`，
直接调用 `xgeGraphicsContextInfoGet`。`share()`、`makeCurrent()` 同样直接调用
相应原生符号，`close()` 使用通用受管资源控制。创建时只有一项冷代码寿命适配；
没有逐方法 C 转发、操作码分发、动态装箱或额外 XRT 副本。

上下文由创建线程操作和最终清理。关闭最后一个上下文视图会启动逻辑退役并取消
私有绘图／上传队列；外部仍持有的 GPU 子对象可以继续使用，物理上下文和代码
保留到子对象释放。新 GPU 根对象会被拒绝。清理失败保留义务并允许重试，活动
Pass／回调不能提前销毁上下文。完成后需 Unit／Init 才能开始下一图形周期。
全局 `unit()` 持有活 GPU 资源时报 Busy 的规则没有修改。

原生构造的失败输出可能仍有非空清理义务。现有 Cgen 的通用 owning-factory 合同
在调用前预留语言控制块，成功与失败都观察原生输出，失败时把剩余义务交给通用
退役队列，不在已经创建原生资源后再依赖一次可能失败的交接分配。本批无需修改
Cgen 来支持这个合同。

EGL 使用是独占的。原始 C 调用若替换当前上下文，必须先 checked `makeCurrent()`
恢复；失败时关闭渲染就绪状态，拒绝 GPU 更新及假截图，不把过期 CPU shadow
作为 RenderTarget 的实际内容。普通 CPU 预载 Texture 的读写能力保留。

示例 `lib/bindings/xge/examples/egl_context.xl` 用局部对象完成 2×2 RenderTarget、
Pass 清屏、精确像素读回及显式清理。Linux 无显示服务器命令：

```sh
LIBGL_ALWAYS_SOFTWARE=1 EGL_PLATFORM=surfaceless xl run egl_context.xl
```

## 像素回读

`target.readPixels()` 返回自上而下、紧密排列的 RGBA 字节。每像素四字节，RGB
为非预乘颜色。结果持有独立内存，关闭目标或上下文后仍可读取。已有的
`target.readPixels(buffer, stride)` 适合复用输出缓冲区或指定行间距。

拥有式入口同步等待 GPU，复用回读分配，在原地交换行并恢复非预乘颜色。
交换行使用 256 字节栈空间。生成 C 直接调用 `xgeRenderTargetReadPixelsOwned`，
并由通用拥有字节输出合同接管同一指针。32 次预热回读的堆计数为 C 32 次分配、
xlang 64 次分配，两边均成对释放且没有重分配。计数包含语言字节值的拥有包装。

[像素回读验收](../dev/release-materials/provenance/xge-target-readback-20261123.json)

## 验收

Windows 默认、Linux 默认与 EGL 配置的语言 JIT／静态 AOT 共 65 项检查通过，
其中启用 EGL 的夹具和示例实际读取像素；生成头和原生 policy/domain 的 C11、
C++11、C++17 检查另有 16 项通过。原生独立完成 263 项检查，包含真实 XGE／
Sokol 绘制、静态／动态、创建失败、清理重试、OOM、线程及拥有图。它们分别
记录在 `dev/release-materials/provenance/xge-owning-context-20261110.json`。

`tools/check_xge3_context_module.py` 用实际宿主关闭模块、使用外部持有的原生
上下文，再释放引用并检查共享库卸载。O0／O2 均覆盖局部上下文、模块全局上下文，
以及原生回调继续访问全局上下文三种情形。移除回调后，模块先清理全局变量，
引擎凭独立的二进制驻留引用继续存活。最后一次 `unit()` 释放该引用，宿主再确认
物理卸载。

创建、分享、访问保护和退役仍需要分配、引用计数、线程检查与拥有图操作。
绘图继续沿用既有原生调用。Windows OpenGL 和 Linux Mesa EGL 的像素读回
通过 JIT、动态 AOT 和静态 AOT 复测。

[模块退役和读回验收](../dev/release-materials/provenance/xge-egl-image-retirement-20261123.json)
