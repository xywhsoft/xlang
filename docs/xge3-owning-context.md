# 拥有式 EGL 渲染上下文候选

本批补入 `xge.GraphicsContext` 的声明和配套原生合同，仍有跨模块全局退役阻塞，
不能视为完整交付。候选在隔离目录，正式 release／IDE 未替换。

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

## 验收与当前阻塞

Windows 默认、Linux 默认与 EGL 配置的语言 JIT／静态 AOT 共 65 项检查通过，
其中启用 EGL 的夹具和示例实际读取像素；生成头和原生 policy/domain 的 C11、
C++11、C++17 检查另有 16 项通过。原生独立完成 263 项检查，包含真实 XGE／
Sokol 绘制、静态／动态、创建失败、清理重试、OOM、线程及拥有图。它们分别
记录在 `dev/release-materials/provenance/xge-owning-context-20261110.json`。

`tools/check_xge3_context_module.py` 是发布门，不能跳过：实际宿主先关闭模块，
继续使用自己持有的原生 context 引用，再释放并检查 DLL／SO 物理卸载。局部
context 的模式通过；模块全局 context 的模式复现了代码驻留连带全局的相互
保留，最终 `unit()` 仍 Busy。代码驻留需要补充正确的原生依赖能力，不能把任意
代码所有者伪装成无子节点叶对象，也不能以提前卸载或吞掉 Busy 解决。

这项发布门未通过前，拥有式上下文接入不结项，正式交付索引不更新。可替换宿主
调度、拥有式 WGL、Windows EGL 交付、TCC ELF 按需系统依赖裁剪等仍在长期任务
范围内。这里没有宣称硬件性能验收或端到端零成本：创建、分享、访问保护和退役
需要冷路径分配、引用计数、线程与拥有图工作，绘图继续使用既有直接调用。
