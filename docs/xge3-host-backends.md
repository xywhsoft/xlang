# 宿主后端与离屏接入

本文件记录基础修复和剩余合同，不代表完整后端接入已经完成。

已把原生窗口状态与图形上下文就绪拆开。Texture、Font、Buffer、Shader、Mesh、
RenderTarget、Sprite、Shape、SVG、Particle 等 GPU 路径检查图形状态；原生窗口、
输入、IME、剪贴板、拖放和 Sokol 唤醒继续检查窗口状态。Sokol 的 GL 加载成功后
才开放图形状态，清理仍在有效上下文中完成，最后关闭状态。原生 EGL 初始化的
能力拒绝和实际失败都不会再改写既有后端表。

配套 native r23 通过两端各 96 项原生检查，包括实际 OpenGL、CPU/OOM/资源采用
与异步回归；语言 Windows 30 项、Linux 24 项 JIT/静态 AOT 检查通过，包含实际
渲染与像素读回。Linux 另以实际 Mesa EGL 1.5 验证 PBuffer 成功及七个原生失败
阶段的回滚和逆序清理；该项是带明确日志测试前置的独立 TU 验证，并非完整 EGL
SDK／渲染器验收。Linux g_xge 私有 BSS 对象在本工具链前后均为 6,984 字节。

这些改动在 XGE 原生层；xlang 不增加逐绘图操作的包装，也不修改 Cgen。本批候选
仍在隔离目录，未覆盖同时进行的 XUI／LSP 改动，正式 release／IDE 尚未替换。

后续接入必须解决的合同：

- 原生 PlatformBackend／GraphicsBackend 表目前只保存描述，正常运行循环不调用
  其中的函数指针。不能把这种表的复制包装成“已经支持可替换后端”。
- 宿主平台／图形上下文需要真实的拥有、代码寿命和创建线程合同，启动失败要逆序
  回滚；方法最终调用原生函数或函数指针，不增加语言端运行循环和操作码分发。
- 维持单实例上下文及现有 GPU 资源计数。显式关闭遇到仍被持有的资源必须 checked
  Busy；自动退役也不能提前销毁上下文、代码或释放仍在使用的资源。
- 离屏目标与创建 EGL 上下文是两个职责。已有 RenderTarget 的安全能力继续复用；
  不能让内部目标指针在父上下文释放后悬空，也不能盲目复制 GL 句柄制造多个 owner。
- EGL 构建开关、真实后端加载、拥有式会话和配套 SDK 都需完成。默认构建返回
  Unsupported 不足以证明可选配置已接入。
- 继续核对既有共享资源在原生 C 发布者持有期间的独立代码寿命，再开放新的上下文
  拥有关系；尚未运行的跨模块边界不能由普通绘图回归代替。

本批输入及报告见 `dev/release-materials/provenance/xge-context-foundation-20261110.json`。

可选 [EGL SDK](xge3-egl-sdk.md) 已在 native r25 补齐正式构建开关、功能位、
SDK 指纹、系统依赖来源和动态／静态包接线。默认 Windows/Linux 与启用 EGL 的
Linux 通过配套验证；实际 PBuffer、Surfaceless、X11 窗口、无显示服务器路径及
失败清理均有运行证据。211 项构建／运行检查通过，默认运行库不增加体积。

旧 Offscreen 尚未接通 XGE 渲染器的情况已用实际启用 EGL 的库复现，不再只依赖
源码推断。拥有式会话、GL 加载／激活、GPU 子资源关闭、真正的宿主调度仍需完成；
Windows EGL SDK 交付及 TCC ELF 按需系统依赖裁剪也继续保留。
