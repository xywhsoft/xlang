# 大源文件编辑：SVG 外层裁剪实验

这是 2026-10-10 的第四版私有 SDK 补丁，实际构建为 SDK15。相对 SDK13，仅修改 `src/xge_emoji.c` 和 `src/xge_svg.c`。它保留前三版的文档、Unicode、Emoji 查找及可见范围优化，共七个完整源码材料；公开头保持不变，新旧 DLL 的 5,949 个导出名称和序号一致。新 SDK 尚未安装到 IDE 的发布目录。

长行含大量 Emoji 时，原有屏幕外判断仍遍历每个 SVG 的子图形来求绘制边界。本版先计算渲染器实际采用的、经四层可变根画笔变换调整的外层裁剪框；只有该框也确定在像素裁剪范围外才跳过递归边界遍历。不能只用名义目标矩形，因为运行时根变换可能把图形移入视口。保留原有两像素保守余量、引用释放和完整边界／绘制后备路径，世界坐标绘制保持原路径。没有新增文本、字形或可变画笔状态缓存。

## 准备源码与复查

在 `D:\GIT\x-lang\ide` 执行，输出必须是尚不存在的目录：

~~~powershell
& 'C:\Program Files\Python312\python.exe' .\tools\prepare_outer_clip_sdk.py --source-root 'D:\GIT\auto_test_ide\build\sdk-candidates\theme-20260916\xge-source' --output '.\build\outer-clip-sdk-review'
~~~

入口复用原始来源、完整材料和新目录检查，仅准备 237 个源材料。本轮实际复现的材料逐个匹配 SDK15；已有目标被拒绝且内容不变，变更来源在创建输出前被拒绝。准备成功不是另一次 DLL 构建。

已有成功的 SDK15 私有构建报告时，可严格编译并运行保存的绘制比较夹具：

~~~powershell
& 'C:\Program Files\Python312\python.exe' .\tools\test_outer_clip_sdk.py --build-report '.\build\editor-sdk-viewport-v15-build.json' --output '.\build\outer-clip-equivalence-review'
~~~

入口复核全部 237 个材料、两份绘制测试材料及构建产物，在新目录生成单一真实引擎翻译单元，加入 SDK13 的原始 Emoji 绘制函数进行比较。比较可执行文件提供唯一 XRT，不链接 SDK DLL；SDK 本身没有测试钩子。程序在从未切换到前台的私有 Windows 桌面上执行，并保存严格编译日志、原始测试日志、两张实际像素截图和 `result.json`。

原始比较运行已通过 35,263 项断言、17,274 项几何裁剪证明和 768 组新旧像素对照；上述落盘入口本轮实际重跑也通过同样的检查。六类 SVG 包含超出 viewBox 的图形、描边、组变换、渐变、偏移 viewBox 与 clipPath；还覆盖四层根画笔变换、极端位置、无效／关闭裁剪，以及名义矩形在屏幕外但根变换后可见的控制用例。有限夹具不代表所有 SVG 滤镜、内嵌图像、文本、透明度、GPU 或分配失败路径的完整验收。

SDK15 全套冷构建实际使用 `-Werror` 并退出 0，仍保留既有三项警告排除。三个原始 SDK 测试、六项现有 IDE 回归和 16 组原生文件对照通过。应用回归复用现有可执行文件，在独立目录替换 DLL；本轮没有重新编译应用、执行完整标准回归或验收新便携包。

超长行编辑仍有数秒延迟，性能未验收。时间、内存、截图差异与剩余工作见[本轮记录](../../../docs/large-source-outer-clip.md)。
