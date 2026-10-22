# semver

`semver` 是严格的 SemVer 2.0.0 解析与优先级比较库，并提供常用的依赖范围匹配。
它不自动修正非法版本；需要去掉 `v`、`V`、`=` 前缀或首尾 ASCII 空白时显式调用 `clean`。

```xlang
import semver

assert semver.valid("1.2.3+build-5")
assert semver.compare("1.0.0-alpha", "1.0.0") < 0
assert semver.satisfies("1.5.0", "^1.2.0")
```

## 接口

| XLang | C | 结果 |
| --- | --- | --- |
| `valid(version)` | `xsemver_valid` | 严格格式检查 |
| `major/minor/patch(version)` | 同名 `xsemver_` 前缀 | 整数；非法返回 `-1` |
| `prerelease(version)` | `xsemver_prerelease` | 先行版本文本；无值或非法返回空字符串 |
| `build(version)` | `xsemver_build` | build 元数据；无值或非法返回空字符串 |
| `clean(version)` | `xsemver_clean` | 去前缀和首尾 ASCII 空白（空格、tab、换行、回车、换页、垂直 tab）；非法返回空字符串 |
| `compare(a, b)` | `xsemver_compare` | `-1/0/1`；非法输入返回 `-2` |
| `satisfies(version, range)` | `xsemver_satisfies` | 是否满足范围 |

C 字符串结果是独立分配的内存，由调用方使用 `xrtFree` 释放。XLang 接口自动转成
`string` 并释放原缓冲区。其余 C 接口返回值无需释放。`semver_native.h` 给出完整 C 声明。

四个整数结果使用 `int64_t`，与 XLang `int` 的 C ABI 一致；C 输出可使用 `<inttypes.h>` 的 `PRId64`。
九个 C 入口在调用开始时清除旧的 XRT 线程错误；正常返回和合法的不匹配不会遗留上次失败。
三个 C 字符串入口只有在分配失败时返回 `NULL`，同时保留 `XERR_MEMORY`；
XLang 的对应入口把该失败转换为 `MemoryError`，不会将分配失败误作空字符串。
XLang 在进入 C 字符串接口前检查内嵌 NUL，并按各函数原有失败值返回，
不会将带 NUL 的非法版本或范围误判为有效前缀。

范围语法支持精确/部分版本、`x`/`*` 通配、`<`/`<=`/`>`/`>=`、
`~`、`^`、空格交集、`||` 并集和 `A - B` 区间。比较符与版本号之间允许空格或 tab，
如 `>= 1.2.3 < 2.0.0`；区间的 `-` 两侧可使用多个空格或 tab。
区间也可与同一分支的其他比较条件取交集，例如
`1.2.3 - 2.3.4 >=2.0.0`；条件位于区间前后均可。
空范围（包括只含空格或 tab）匹配正式版本；
空的 `||` 分支、畸形数字和无效范围均拒绝。build 元数据不影响优先级。
只有运算符而没有版本号的范围（如 `>=`、`^`）属于无效范围；需要匹配任意正式版本时使用空范围或显式 `*`。

当前实现对每个 prerelease/build 字段限制为 255 字节，对范围表达式限制为
511 字节；每个 `||` 分支在展开后最多包含 32 个基础比较器，`^`、`~`
或部分版本的简写可能占用两个。超限直接拒绝，不做截断；版本号分量须能
放入非负 64 位整数。
这些是实现限制，不是 SemVer 规范限制。
严格版本解析和范围入口先在格式上限内查找终止符；超限输入不会为了寻找
远处的 NUL 而继续扫描。C 调用方仍应传入合法的 NUL 结尾字符串；
`clean` 为支持任意长度的首尾空白，仍须读取完整输入后才能裁去空白。
`clean` 在去掉首尾空白后先检查最大合法长度，再分配结果缓冲区；
超长非法输入不会产生与原文等大的临时副本。
完整版本的 `<=` 使用包含式上界；部分版本的 `<=1.2` 表示 `<1.3.0`。
最大整数可直接用于比较边界（如 `>=9223372036854775807`）；简写范围若必须
对该分量加一才能构造排他上界，则按实现限制拒绝，不进行整数回绕。

## 验证

包清单显式声明空的 `xrtModules`：原生实现只调用 XRT 基础分配/释放入口，没有额外模块依赖。无输出边界用例在裁剪 XRT 的 AOT 产物中运行通过；这不改变带输出范例的共享快照链接阻断。

在 `demo6` 目录运行：

```powershell
./release/xl.exe run release/extlibs/semver/examples/semver_c.c
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/semver/test/check.ps1 -SkipAot
bash release/extlibs/semver/test/check.sh
```

去掉 `-SkipAot` 可额外尝试独立可执行文件构建。脚本仅验证该包，不运行全仓回归。
Windows 日常门禁和可选的独立对照门禁各自使用单次独占的
`build/test/semver_guard_*` / `build/test/semver_oracle_*` 目录；XRT 对象
每次重新编译，结束或失败时仅清理本次目录，不复用固定缓存。
包内 `test/boundaries.xl` 和 `test/allocations.xl` 已登记到 `coverage.tests`，可用
`tools/smoke_extlib_catalog.ps1 -Package semver` 连同原主用例复验。
`examples/semver_c.c` 与 `examples/basic.xl` 覆盖九个公开入口，共用
`examples/basic.expected.txt` 逐行校验两种语言的输出。
C 范例包含包内原生实现，可由 `xl run` 单文件执行；正式包仍按清单
编译原生源文件。Windows/Linux 包门禁将直跑与严格 C 编译输出逐字比较。
Windows C/XLang 范围矩阵、无输出边界与分配故障 AOT、Linux ASan/UBSan 已通过；
可选 node-semver 对照只比较双方共同支持的范围语法：本包另允许
`A - B >=C` 将区间与其他比较条件取交集；若对照版本把它视为无效范围，
oracle 跳过此扩展写法，而 C/XLang 边界用例仍独立验证其正反结果。
2026-10-22 使用 node-semver 7.8.5（提交 `7257c1423947`）对照：
202,860 组共同语法全部一致，276 组上述扩展语法跳过；Windows 包级
`-SkipAot` 与 Linux C ASan/UBSan、XLang 门禁通过。完整带输出 AOT
仍受共享运行时缺符号阻断，不据此签收整包。
同日将独立对照扩到严格版本有效性与两两优先级：共同语义共
279,319 组，0 差异，仍仅跳过上述 276 组扩展范围；测试 C 查询程序
同时通过 Linux GCC `-Wall -Wextra -Werror` 语法检查。无效版本的
`compare` 在 node-semver 中会抛错，仍由本包 C/XLang 负例验证。
包内 C 测试还对五种合法版本种子逐位置截断和单字节替换，检查有效性、
自比较、精确范围匹配及 `clean` 的一致性；Linux 门禁以 ASan/UBSan 执行
同一矩阵。这是有界的确定性回归，不代替可选的独立 node-semver 对照。
Linux 包级脚本也运行中央、包内边界及分配故障 XLang 用例，并逐字节核对
C/XLang 范例与共同黄金输出；C 侧仍使用 ASan/UBSan。
带输出范例的 AOT 仍受主线 `xl6ValueDataSnapshotV1` 缺符号阻断。
超长 `clean` 输入的 C 回归还检查 XRT 请求分配量，防止恢复先复制后拒绝；
大量首尾空白的有效版本也有定向测试。统计模块只启用于测试程序。
分配故障注入覆盖三个 C 字符串入口的 `XERR_MEMORY` 与恢复，及三个 XLang
入口的 `MemoryError` 与恢复；C 测试也逐一验证九个入口不会带出陈旧错误。

可选的独立语义对照不进入日常包门禁。将上游
[`npm/node-semver`](https://github.com/npm/node-semver) 检出到临时目录后运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File release/extlibs/semver/test/check_oracle.ps1 -OracleDir build/test/semver_oracle
```

本轮以 node-semver 7.8.5（提交 `6e05b7637396ac66522cff8731f07cfe0ef49a29`）
对照 201,480 组版本与范围（包含 alpha、beta、rc 预发布比较器），结果一致。对照仅覆盖本包公开的范围子集；
本包有意拒绝空 `||` 分支，而上游将其当作通配，该分歧由普通 C/XL 负例
独立约束，不纳入“应相同”矩阵。对照脚本仅在维护范围语义时运行，
不要求正常使用者安装 Node.js。
传入目录须为带 `package.json` 的 node-semver 包；缺少该依赖会在编译
测试程序前报错，而不会伪装成范围比较失败。

2026-10-16：Linux 包级门禁新增语言用例和 C/XLang 范例同输出，严格 GCC、
C ASan/UBSan 与语言路径同次通过。当前 Windows 完整门禁的边界、分配故障
AOT 均构建运行通过，带输出范例构建仍缺共享 `xl6ValueDataSnapshotV1`，
未签收完整发布形态。

2026-10-17：补齐每个 `||` 分支展开后 32 个基础比较器的公开限制；
16 个 `^` 范围通过、17 个范围按超限拒绝的 C/XLang 边界已固定。
Linux 严格 GCC、C ASan/UBSan、XLang 及双语同输出门禁通过；完整
带输出 AOT 仍缺共享 `xl6ValueDataSnapshotV1`。

2026-10-17：可选 node-semver 对照加入四组核心版本的 alpha/beta/rc 预发布矩阵与组合范围，当前 201,480 组零差异；只扩展测试，公开实现未改。完整带输出 AOT 阻断不变。

2026-10-18：C 严格版本入口原在长度上限前调用 `strchr`，ASan 对达到上限且无终止符的缓冲区复现越界读；范围入口同样先使用无界 `strlen`。两条路径现先有界找 NUL，超限立即拒绝，原解析和范围语义不变。C/XLang 另覆盖 511/512 字节范围及两个恰好 255 字节的元数据字段；Windows 严格 C/XLang、双语黄金输出和 Linux ASan/UBSan 联合门禁通过。完整带输出 AOT 的共享缺符号阻断仍在。

2026-10-19：C 教学范例补单文件 `xl run`，Windows/Linux 门禁加入
直跑与严格 C 编译结果逐字对照。Windows 严格 C/XLang 边界及分配故障、
Linux C ASan/UBSan/XLang 双语门禁通过；算法未改。完整带输出 AOT
仍受共享缺符号阻断。

2026-10-20：两个 Windows 门禁改为独占构建目录并清理，不再争用同一
对象和可执行文件。日常 `-SkipAot` 严格 C、语言边界、分配故障与黄金
输出通过；默认门禁的边界和分配故障 AOT 也通过，带输出范例仍缺共享
`xl6ValueDataSnapshotV1`。随后从 npm registry 临时取得并校验
node-semver 7.8.5 包的 SHA-512 完整性，重新运行独立语义对照：
201,480 组版本/范围零差异。Windows `-SkipAot` 和 Linux C ASan/UBSan、
XLang 对照也在当前构建复验通过；这些证据仍不覆盖完整带输出 AOT。

2026-10-21：范围比较符后可分隔空格/tab，区间分隔符两侧可使用多个空格/tab；
C 与 XLang 边界及对照范例已覆盖。固定 node-semver 7.8.5 标签
`6e05b7637396ac66522cff8731f07cfe0ef49a29` 的 203,136 组范围对照零差异；
Windows `-SkipAot` 严格 C/语言门禁及 Linux C ASan/UBSan/语言门禁通过。
本次未运行完整带输出 AOT，不据此签收发布级。

同日对当前产物补跑完整 Windows 门禁：边界和分配故障 AOT 均构建运行通过，
带输出的 `examples/basic.xl` 在 AOT 构建阶段仍缺共享
`xl6ValueDataSnapshotV1`，门禁退出 1。该阻断不属于范围解析器；主线修复后
须重跑完整门禁，不能用 JIT 或无输出 AOT 代替发布签收。

同日对新增的比较符后空格/tab 范围用例补跑固定 node-semver 7.8.5
独立语义对照：203,136 组版本/范围零差异。该结果只证明公开范围子集的
语义一致，不解除带输出范例的 AOT 链接阻断。
