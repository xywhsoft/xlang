# 极长单行的坐标精度修正

SDK62 私有候选，保留 SDK59 的光标、自动换行和缩放修正。布局、绘制及恢复检查点使用双精度累计坐标，内部光标停靠点保存精确位置；公开头、GPU 坐标和滚动 API 保持现有类型，在扣除可视区原点后换算。相对 SDK59 修改三个内部源码文件。

严格完整 SDK 构建通过。16 份引擎夹具保留全部覆盖和断言，独立旧算法的数值参照使用 long double，停靠点逐字段精确比较，避开 C 结构体未定义填充字节。当前完整 IDE 的相同 83 个应用对象连接匹配头、DLL 和导入库，在 C／xlang 的 18 个原生用例中完成 1560 项检查，包括 16 MiB 单行的高精度宽度、末尾字符点击定位、输入撤销、自动换行、三次窗口尺寸变化和显式滚动取消跟随。相同新增数值检查在 SDK59 的两种语言上均按预期失败；真实行末截图恢复了文字和闭合符号。

tools/prepare_precise_coordinates_sdk.py 与 tools/prepare_precise_coordinates_app.py 向新目录重现材料；tools/test_precise_coordinates_sdk.py 严格编译并运行完整引擎夹具。tests 中的 v10／v13／v14 原生应用夹具还需要完整应用对象。证据和范围见 patch-manifest.json 的 preciseCoordinateActualReport。

当前可运行包仍为 SDK52，后续继续完整 IDE 构建、回归和打包集成。本阶段完成所测行末画面的恢复及定位验证，不等于全部极端文本的独立逐像素验收，也未宣称极长行性能验收完成。
