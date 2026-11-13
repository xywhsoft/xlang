# 字体面、字体族与查询接入

本批把原生 FontFace／FontFamily 映射为 `xge.FontFace`、`xge.FontFamily`，补入
Font 的度量、缓存、字形索引／码点查询、图集查询、测量／矩形绘制和 XRF 操作。
它是阶段 3 的一部分，不代表 GlyphRun／字形位图、完整文本排版及音频已经完成。
上一批上下文与模块驻留的发布门仍未通过，正式 release／IDE 不替换。

`loadFontFace(path|bytes|Buffer,faceIndex=0,flags=0)` 保持原生 face 语义：内存输入
由 XGE 复制一次，语言投影把原始字节指针和精确长度直接交给 C；文件读取及其
原生拷贝也没有在语言侧再包一层。Face 不是给定尺寸的 Font，`face.createFont`
用调用期栈描述创建独立原生拥有记录。正常字宽比例为 100，不是 OS/2 表中的 5。

Family 的 `addFace` 保留原生引用；`resolve(weight=400,stretch=100,slant=0)` 直接
调用 ResolveEx 并取得新的原生引用。正常 stretch 的调用覆盖旧 Resolve 的功能。
关闭输入 Face、Family 或已解析 Face，都不会提前释放 Font 仍在使用的数据。
原生 add-ref/free 由资源合同承接，不额外公开重复的手动引用计数方法。

Font 增加 `metrics/cacheStats/clearCache/glyph/glyphByIndex/atlasGlyph/atlasGlyphByIndex`，
查询结果是独立的精确值记录。`measureText/drawTextRect` 沿用原生 C 字符串语义；
不把它们冒充具备显式文本长度的完整 shaping 接口。后者仍须通过 GlyphRun 接入。
`saveXRF`、`loadFontXRF`、`loadFontCached` 和 `fontFallbackGet` 复用 XGE 实现。
`fontFallbackSetMemory` 升级为 checked 的 bytes／Buffer 重载，失败直接报告；
它不再返回供调用者比较的原始 c_int，调用处改为普通语句并按需捕获异常。

## 原生修复与验证边界

族匹配改为 64 位差值／算分，容量增长检查溢出；空族和失败输出有明确结果。
字体尺寸拒绝 NaN／Infinity；字形索引及光栅边界在不安全的数值转换前校验。
后备字体替换先创建候选，成功才替换旧值，OOM 不丢掉旧字体。

可选缓存文件及后备查找使用独立诊断范围。普通缺失／不可用缓存可以回退，OOM、
provider 回调失败、状态错误及非预期诊断不能变成成功；调用者原有诊断不被探测
覆盖。XRF 烘焙不再忽略字形准备失败，失败不交付半成品；保存检查最终 fclose。
这些修复在 XGE 中完成，不在 Cgen 中增加补丁。

实际 EGL 绘制还复现了图集内嵌纹理在缓存清理／Font 释放后被排队命令继续访问
的问题。纹理改为独立稳定记录，命令保留真实引用；内部纹理不另占公开资源准入
计数，外部 Texture／Font 的 Busy 规则保持不变。最终测试在清缓存和释放 Font
后提交两次文本绘制，读回 462 个着色像素，内存和资源计数均归零。

原生边界／OOM、JIT／静态 AOT、精确头及调用证据由
`tools/check_xge3_font_resources.py` 与 XGE 的 `tools/test/check_font_resources.py`
记录。测试只使用自行生成的有效小字体，不运行畸形字体模糊测试或网络测试。
字体文件完整解析器的安全边界、CFF／可变字体、GlyphRun／Bitmap、跨模块原生
发布者及音频仍要各自验收；本批实际 GPU 验证仅覆盖所列有效字体与绘制路径。

## 成本与示例

业务方法直接调用相应原生函数；描述符为调用期栈值，无逐方法 C 转发或装箱。
原生字体实例创建使用同一拥有记录分配方式；Face 的输入复制、族数组和字体／
图集缓存的分配是原生算法成本。稳定纹理增加每个已上传图集页一次原生记录
分配和访问纹理时的指针读取，CPU 页则省去未使用的内嵌纹理。语言侧另有通用
视图控制、线程／调用期保护、代码寿命及 C 字符串投影检查。本批不声称端到端
零开销或硬件性能达标；无分配热查询也不能代替完整耗时对比。

`lib/bindings/xge/examples/font_faces.xl` 用线性代码演示引用接力与查询，附带 2,408
字节的自行生成字体，无系统字体依赖。它从示例目录运行，观察输出并显式清理。
验收夹具还在禁止新 XRT 分配的条件下重复进行度量和字形查询。
最终源码和构建／运行证据见 `xge-font-resources-20261111.json` 小索引。

## 单字形矢量轮廓

`Font.appendGlyphOutline(glyph, shape, penX=0, baselineY=0)` 直接调用
`xgeFontGlyphOutlineAppendShapeEx`，将指定字形的轮廓追加到现有 Shape。它不重置
Shape；需要替换路径时先调用 `Shape.reset()`。坐标沿用字体实例的像素缩放与向下
的 Y 方向，`baselineY` 表示基线。Shape 保存原生复制的路径命令，Font 关闭后仍可
查询或使用已有几何。原生分配失败可能留下部分追加内容，不承诺原子替换。

`glyph` 是当前 Font 的字形索引，不是 Unicode 码点。特别注意 `Font.glyph(cp)`
可以经过 fallback 返回其他字体的索引，不能把该索引再交给错误的 Font。已排版
文本优先使用现有 `GlyphRun.appendOutline()`；需要逐字形处理时，将只读 glyph
记录与同位置 `GlyphRun.font(index)` 返回的独立字体 owner 配对。单文件范例
`glyph_outline.xl` 使用自带字体已包含的 A，避免隐藏 fallback 条件。

业务调用不增加包装函数。语言仍执行 Font／Shape 的有效期和线程检查；冷轮廓
缓存与 Shape 容量增长是原生分配。热循环对比复用同一缓存与各自的 Shape 容量，
不代表冷路径或 CPU 时间相同。故障注入使用独立冷 Face，并要求同时观察到失败
和成功，避免热缓存掩盖分配失败。

Windows／Linux 完整 O2 编译器各通过 24 项消费检查，包括 JIT、独立动态／静态
程序、公共 C/C++ 头及上述冷分配故障。每种模式下，语言和 C 的 1000 轮热
reset／append 均为零 XRT 堆操作。匹配包、源码、程序和日志的哈希索引为
`xge-glyph-outline-20261114.json`；最终正式 SDK／运行库／IDE 安装另行完成。

## XRF 内存导出

`Font.encodeXRF(firstCodepoint, count)` 直接调用 `xgeFontBuildXRFMemory`，生成
当前字号的烘焙字形图集与度量，返回独立 `bytes`。它沿用原生 Unicode 范围、
非零数量和失败状态规则；原生会准备字体缓存，并将所选图集页面复制到输出块。
失败不交付半成品，语言赋值目标保持原值；已经生成的原生缓存可以保留。

原生使用 `void**`／`int*` 输出。通用拥有字节声明保持精确 C ABI，接管与当前
驻留 XRT 兼容的完整块，接纳时不再复制 XRF 载荷。语言 `xvalue` 外壳仍分配，
Font 的线程／有效期和 checked 调用作用域仍执行。`int` 长度规则另见
`foreign-owned-bytes-output.md`；没有新增逐方法转发函数或自定义释放适配器。

`font_xrf_memory.xl` 用自带字体烘焙 A，在关闭源 Font 后通过现有
`xge.loadFontXRF(bytes)` 重载加载，输出编码长度和字形字宽。`Buffer` 输入也
沿用现有重载。范例从其所在目录运行，不创建输出文件或图形设备。

`tools/check_xge3_font_xrf_bytes.py` 验证有效字体往返、值复制隔离、源 Font
关闭后的输出寿命、64 档独立冷字体分配故障及清理；热循环与 C 对照均显式
包含一个 XRF 输出块和一个字节值外壳。源码预检与完整 O2／嵌套编译消费门
分别记录，正式配套交付仍属全量接入任务。

两端完整 O2 编译器各通过 24 项消费门，JIT／动态／静态路径的 100 次热编码
均为 200 次分配／释放、零重分配，与 C 对照一致。另用同一编译器与静态 SDK，
仅移除未使用的 encodeXRF 声明对照字形轮廓范例，两端文件和机器码均增加
0 字节；这只证明该样例的增量，不代表全部原生未用代码均已删除。原静态 SDK
在对照后逐项复核未变。证据索引为 xge-font-xrf-memory-20261114.json。
