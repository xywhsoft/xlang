# geohash

2026-10-27 所有权收口：编码、边界框与四个邻格查询的 C 结果原型
明确为拥有型 `char*`；XLang 使用 `@owned string?` 直接接管
XRT 分配，删去 `point` / `takeCstr` 中转。生成 C 门禁固定六条
返回路径均不复制文本或二次接管；接管仍需一次 `strlen`。
极点、非法输入和分配故障仍按原有边界用例验证。

2026-10-26 发布物复验：Windows 默认 `test/check.ps1` 和 Linux 默认
`test/check.sh` 均退出 0。两平台验证严格 C、线程、语言边界、
分配故障、固定双语输出，以及中央用例、边界、分配和公开范例的
独立 EXE；C 单文件范例也由 `xl build` 生成 EXE，输出与 GCC、
XLang 范例及固定黄金文件一致。Linux C 边界与线程继续使用
ASan/UBSan。本轮只完善门禁，算法、API 与主线未改。
验收范围是本文所述 1..12 字符网格编码、解码和四邻格规则，
不代表完整地理信息系统或全部编码空间的穷举证明。
以下早期 AOT 缺符号记录均为历史状态。

2026-10-25 所有权复核：`encode`、`bbox` 和四个邻居查询返回
普通 `string`，由 `takeCstr` 直接移交 C 端 XRT 分配，不复制文本。
先前 `cstring` 返回转为 `string` 时反而复制，现已修正；Windows 包门禁
核对生成 C 的直接移交及 C/JIT/AOT 行为。Linux C ASan/UBSan、
线程、XLang JIT 与双语门禁已通过；独立 AOT 复验见上。

2026-10-24 当前发布物复验：Windows 默认 `test/check.ps1` 的严格 GCC、
C/XLang 边界、分配故障、固定黄金输出以及核心、范例、边界三项 AOT
全部通过。边界程序原本会打印 `geohash boundaries: PASS`，门禁却要求
无输出，导致成功执行被误判失败；现在要求 JIT 与 AOT 输出逐字一致。
Linux `test/check.sh` 的 C ASan/UBSan、线程、XLang JIT 与双语黄金输出
也通过。旧版 `xl6ValueDataSnapshotV1` 缺符号不再是当前 Windows 阻断；
本轮未用新 Linux 编译器构建独立 EXE。以下旧记录的“当前缺符号”
均指当时的发布物。

`geohash` 将经纬度编码为 1..12 字符的标准小写 Geohash Base32 文本。经度位先于纬度位交错；处于分割中点的坐标归入上半格。仅接受有限范围内的纬度 `[-90, 90]`、经度 `[-180, 180]`；最大端点归入最后一格。解码严格校验长度和字符，不跳过错误输入，也不静默修改精度。

## XLang API

- `encode(lat, lon, precision = 6)`：创建编码；精度必须为 1..12。
- `decodeLat(hash)` / `decodeLon(hash)`：返回该格中心坐标。
- `bbox(hash)`：返回 `minLat,minLon,maxLat,maxLon`，使用足以往返双精度的十进制表示；字段分隔符固定为逗号，小数点固定为 `.`，不受进程区域设置影响。
- `north/south/east/west(hash)`：返回同精度的相邻格；经度在 ±180 度处环绕，极点外没有北/南邻格，会抛 `ValueError`。
- `precision(hash)`：严格校验后返回字符数。

## C 接口与所有权

公开声明在 `include/geohash_native.h`。文本结果为独立 XRT 分配，C 调用方使用 `xrtFree`；错误返回 `NULL`，通过 XRT 错误对象读取原因。`xgeohash_decode_bounds` 以结构体输出精确格边界，失败返回 0 且不修改输出；`xgeohash_precision` 失败返回 -1。单独的 C `decode_lat` / `decode_lon` 失败时返回 0.0，因此需要可靠诊断时应使用 `decode_bounds`，不可仅凭数值判断成功。XLang 封装已做严格校验并自动接管字符串所有权。

每次公开 C 调用开始时会清除本线程上次遗留的 XRT 错误；成功返回不会让调用方读到前次失败的诊断。失败后仍可直接从 `xrtGetError()` 读取本次原因。

邻格在经纬网格的整数索引上计算，避免在高精度下由浮点中心加减造成跳格；东/西环绕，北/南在极点报错。C 输入是 NUL 结尾字符串，不能表达嵌入 NUL 的 hash；XLang 在调用 C 前检查完整字符串，发现内嵌 NUL 则抛 `ValueError`，不会按有效前缀查询。

## 范例与验证

- XLang：`release/examples/extlibs/geohash.xl`
- 同输出 C：`release/extlibs/geohash/examples/geohash_c.c`

在 `release` 可直接运行 `xl run extlibs/geohash/examples/geohash_c.c`；
普通 C 编译仍按包清单单独链接 `geohash_native.c`。
- 边界：包内 `test/test_geohash.c`、`test/test_threads.c`、`test/boundaries.xl`（含内嵌 NUL 前缀负例），以及 `test/cases/extlib_geohash.xl`

在 `release` 运行 `xl run extlibs/geohash/test/boundaries.xl`。在 `demo6` 运行 `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package geohash -Source release/extlibs/geohash/test/test_geohash.c`。
`pwsh -NoProfile -File release/extlibs/geohash/test/check.ps1 -SkipAot` 汇总严格 GCC manifest 宏、C/XLang 边界和三路范例黄金输出检查。去掉 `-SkipAot` 会继续构建并运行核心 AOT、带输出范例 AOT 和边界 AOT；当前 Windows 发布版均已通过。固定输出见 `examples/geohash.expected.txt`，不只依赖两种语言互相一致。

2026-10-12 已通过 Windows 严格 GCC、Linux ASan/UBSan、C/XL 边界与线程测试、同输出范例及 `test/cases/extlib_geohash.xl` 无输出核心 AOT。当时完整 AOT 缺少共享 `xl6ValueDataSnapshotV1`；现行复验结果见本页开头。

Linux 可在 `demo6` 目录运行 `bash release/extlibs/geohash/test/check.sh`，同次验证严格 GCC/ASan/UBSan 的 C 边界、中央与包内 XLang 用例，以及 C/XLang 范例与固定黄金文件的逐字对照。2026-10-14 新增全部 1、2 字符格子（1056 格）的中心点重新编码与邻格互逆验证，并在 C/XLang 测试最高 12 字符精度的经线环绕和极点终止；实现未改。此测试不是全部 12 字符编码空间的穷举证明。

2026-10-22：补上固定的北京坐标、格边界与四邻格范例输出；Windows 快测和 Linux C ASan/UBSan、XLang 双语门禁均通过。Windows 默认门禁核心 AOT 通过，新增带输出范例 AOT 构建仍缺共享 `xl6ValueDataSnapshotV1`，其运行比较以及后续边界 AOT 尚未执行；独占测试目录已清理。包算法/API 未改，整包仍不能签收发布级。

2026-10-15：修复 `bbox` 在逗号小数点区域设置下产生歧义的输出，改用 XRT 数字写出函数；Windows 区域设置负例先红后绿。Windows C/XLang 同输出、线程边界、无输出核心 AOT 与 Linux ASan/UBSan 复验通过；完整带输出 AOT 仍受上述主线缺符号阻断。

2026-10-18：C 边界测试增加固定种子的 512 组跨经纬度坐标，逐组覆盖 1..12 位精度，检查格子包含原坐标、精度增加时前缀稳定、格子中心可重新编码以及东西邻格往返。Windows 严格 GCC/C、XLang 边界、双语范例与 Linux C ASan/UBSan/XLang 联合门禁通过；实现未改，完整带输出 AOT 仍待共享主线修复。

2026-10-19：`encode`、`bbox`、邻格三条文本结果路径增加 C/XLang 逐点分配故障测试；C 侧验证 `XERR_MEMORY` 与下一次调用恢复，语言侧验证 `MemoryError` 不会被误判为输入错误。Windows 包级及 Linux C ASan/UBSan、XLang、双语门禁通过；原生实现无需修改。完整带输出 AOT 的共享缺符号阻断不变。

2026-10-20：Windows 包门禁增加显式 `-SkipAot` 与独占 AOT 构建目录，快速模式输出明确注明跳过 AOT；默认模式验证中央核心 AOT，再验证包内边界 AOT。当前严格 C/JIT、双语同输出和核心 AOT 均通过，边界 AOT 链接仍缺共享 `xl6ValueDataSnapshotV1`，默认门禁正确以非零状态退出，成功或失败均清理本次构建目录。原生算法未改，整包仍未签收发布级。

2026-10-21：C/XLang 边界新增 NaN、正负无穷坐标拒绝断言；C 还覆盖空输出指针和空 hash 的错误类别、失败后原输出不变。Windows `test/check.ps1 -SkipAot` 与 Linux `test/check.sh` 的 C ASan/UBSan、语言边界及双语对照通过。实现未改；完整 AOT 共享缺符号阻断仍在，不签收整包发布级。
