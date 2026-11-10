# 可选 EGL SDK 与标准声明

原生构建增加了正式的 `XGE_ENABLE_EGL=0/1` 开关，默认关闭。配置、功能位 10、
公开 SDK 指纹、动态库和静态链接输入一致；功能位不同的 SDK 会被原生兼容检查
拒绝。显式要求 EGL 而缺少系统 SDK 时，构建会明确失败，不静默降级。

Linux 可选配置使用 `pkg-config egl` 的系统开发库，记录实际头、链接库和构建
助手的哈希。动态、静态包均通过普通配套工具生成，继续复用唯一的 XRT。系统
EGL dispatcher 和驱动不进入包，需要由目标系统提供。正常 Windows OpenGL
配置不增加 EGL 依赖；Windows EGL 的 SDK／运行时交付仍待单独完成。

语言入口统一为 `xge.eglCapsGet()`，返回精确的 `EglCaps` 值记录。名称替换原先
不一致的 `eGLCapsGet` 拼写。生成 C 直接调用 `xgeEGLCapsGet`；没有增加函数转发
包装、动态装箱或新的编译器合同。`bCompiled` 等字段表示编译了创建路径，并不
承诺当前机器一定能创建该上下文。

示例位于 `lib/bindings/xge/examples/backend_capabilities.xl`，可在未初始化引擎时
查询能力。默认 Windows、默认 Linux、启用 EGL 的 Linux 均通过 JIT、独立静态
AOT 和严格 C/C++ 公开头检查。原生动态库和静态库验证实际 PBuffer、Surfaceless、
X11 窗口与 GLES 像素读回、表面交换，以及八类创建失败的逆序清理。另有无
DISPLAY／WAYLAND_DISPLAY、显式 Mesa surfaceless 平台的原生和语言实测。

这些是 raw EGL／SDK 验收。原生旧 `Offscreen` 尚未把 EGL 接入 XGE 的 GL 函数表、
渲染状态和拥有图；实际对照中 EGL 创建成功，但目标 GPU texture/FBO ID 都为 0。
下一步需要拥有式上下文／离屏、真正的宿主回调调度与 GPU 子资源退役合同，不能
直接将此裸对象作为安全类导出。完整接入仍在进行，正式 release/IDE 尚未替换。

性能边界也保留证据：本批默认运行库体积不变，Linux EGL 共享库增加 128 字节、
静态库增加 6,416 字节。TCC 静态能力查询范例在启用的 SDK 下仍列出 `libEGL.so.1`
依赖；当前 TCC ELF 的 `as-needed` 选项被忽略。最终按需依赖裁剪还需处理，不能
把函数节裁剪等同于所有系统库依赖也已去掉。

本批共 211 项通过的构建／运行检查，包含构建命令、负例拒绝、实际运行、头文件
和导入验证，并非 211 个独立业务场景。原始证据及输入哈希位于
`dev/release-materials/provenance/xge-egl-sdk-20261110.json` 指向的隔离目录。
