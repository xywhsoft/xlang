# Shader、RenderTarget 与 Pass

本批按已批准的规则处理资源：显式关闭引擎遇到仍然存活的原生拥有资源时，
checked 返回 Busy，保持引擎和回调注册；释放资源/保留槽后重试。用户已决定继续
由主代理单独实施。这里记录本批实现与证据，不是全量 XGE/XUI/Document 结项。

| 语言类型/入口 | 原生操作 | 拥有与失败规则 |
| --- | --- | --- |
| Shader / createShader | CreateOwned、Uniform1i/1f/2f/3f/4f | 原生最后引用释放外壳；uniform 名使用 checked C 字符串 |
| ShaderDefine / ShaderVariants | 固定 C 记录、SetInit、GetOwned | 数组投影保留准确布局；返回 Shader 可独立活过变体集 |
| Material.shader、setShader | 保留字段读取、SetShaderChecked | setter 先 retain 后替换；可置空；三个纹理槽也有拥有读取 |
| RenderTarget / createRenderTarget / createWindowTarget | 原生拥有创建、Resize、Capture、ReadPixelsSized | 返回纹理保留目标；活跃 Pass/外部纹理使 resize Busy；输出 Buffer 按实际容量校验 |
| Pass / createPass | CreateOwned、Begin、End、IsCurrent、ReleaseOwnerChecked | 保留目标；关闭当前 Pass 恢复 LIFO；非当前活跃 Pass 关闭 Busy、可重试 |
| graphicsResourceCount / passesIdle | 原生只读查询 | 包括创建中的原生拥有义务，不是显存计数 |

业务方法保持直接声明，生成 C 直接调用真实 XGE。头文件仅承接冷路径生命周期，
不增加逐方法转发、操作码分发或另一份渲染实现。目标/Pass 的 close 预检先拒绝
不能消费的拥有者，再交给通用视图关闭，避免把 Busy 的对象先标成不可用。

原生底座在 `D:/GIT/xge` 独立实现并提交：`cf559794`，最终修订 `58f7264e`；
验收工具及退出后 Pass 清理检查为 `668316a`、`3f7b254c`。
初始化使用独立准入状态，拒绝非创建线程的初始化/资源创建，并保留初始化前已
排队的纹理；分配失败可重试，不留下未排队的“成功”纹理。
Texture/Font、Buffer/Mesh 的创建改用 Native Owned 工厂，语言失败回滚能够区分
原生拥有外壳与 caller storage。RenderTarget 的事务式 resize 同时迁移上传队列的
地址，不留下临时纹理地址。默认 XUI Proxy 的引擎义务持续到物理 storage 释放，
包括通过 Font/Surface/保留 paint 延长的寿命。XGE 自身的 XRT 快照未被替换；
隔离构建显式选择 xlang 的当前配套 XRT，SDK/二进制/指纹一起暂存。

成本没有被隐藏：

- 原生拥有创建/最终释放有一次冷准入自旋锁和计数；不在普通 draw/update 增加
  全局检查、映射表或 epoch 查询。
- Shader/Texture 获取保留结果仍有原生 retain、受管视图与关闭 bookkeeping；
  Material 和目标子资源使用真实原生引用，不复制 GPU 数据。
- Buffer span 仍有调用期保护；定长记录数组仍走既有 O(n) 快照/复制。
  `ShaderVariants.get` 通常还会编译 GPU 程序，这条路径不宣称零分配。
- uniform/source 的 C 字符串准入继续检查 NUL；变体集按原生语义复制源文本。
- 既有预热更新/入批循环测得两种链接方式均无 XRT 分配。这不等于整个接入零开销。

验收工具：`tools/check_xge3_render_resources.py` 默认 CPU；只有显式
`--graphics-case` 才打开短暂窗口。Windows 的本批 40 项进程检查包含八个用例的
C/头生成、JIT、静态 AOT 构建/运行及导入表检查。新的 GPU 夹具实际读回 Pass
清屏、ShapeBatch、Mesh、SpriteBatch 和自定义 Shader/Material 的像素；原生
Windows/Linux O0/O2 另测嵌套 Pass、退出后保留资源、Busy 以及释放后重新初始化。
Linux 图形环境使用 Xvfb，不等同于物理 GPU 性能认证。

OOM 夹具扩到八种工厂各 32 个分配前缀（预热后统一复测），检查实际 XRT 的
live count/bytes 与非法/重复释放计数；热路径分配计数保持零。并发准入门在两端
O0/O2 通过，Linux 同时通过 ASan/UBSan。新增 `offscreen_pass.xl` 示例绘制一次并
打印四个像素，显式清理 Pass、目标与回调。

外部证据保存在 `F:/xlang-validation/xge-full-20261110/`；小索引位于
`dev/release-materials/provenance/xge-render-pipeline-20261110.json`。本批首先使用
显式标注为诊断替换的 O2 编译器发现问题，随后完成正常 `build.bat` 的完整 O2
编译器、TinyCC、VFS 和 SDK 重建（851.438 秒，构建输入无漂移）。最终完整候选
通过 40 项渲染检查、33 项输入合同检查、23 项 CRT 权威头检查、6 项嵌套编译
检查，以及生成头的 C11/C++11/C++17 严格编译。原生 Windows/Linux 各通过
20 项检查，并发准入门两端各 4 项通过，Linux 包含 ASan/UBSan。

最终编译器是 `compiler-full-r7/`，原生源为 `native-source-r7/`，
Windows 动态/静态包为 `packages-graphics-host-r6/`、`packages-graphics-static-r6/`。
`native-freeze-r7.json` 冻结时记录了上一个提交与工作区差异，最终原生提交
`58f7264e` 收录这些实现。证据哈希固定各自输入，不把后续其他工作区修改算入验收。
尚未更新发布目录、IDE 或最终 Linux xlang 交付，阶段 2—8 继续保留。
