# 许可范围与第三方说明

xlang 原创代码和文档由项目权利人按根目录 [MIT License](LICENSE) 授权。该授权不覆盖项目无权重新授权的第三方代码、衍生部分、字体及其他素材，也不替换文件中已有的版权与许可声明。

## TCC 与工具链资源

TCC 不是整体采用普通 GPL 的项目。当前仓库的主要许可边界如下：

| 范围 | 当前来源声明 | 保留方式 |
| --- | --- | --- |
| `lib/tcc/` 的编译器主体 | 文件头通常指定 GNU LGPL version 2 or later；上游 COPYING 提供 LGPL 2.1 文本 | 保留 [上游 COPYING](lib/tcc/COPYING)和各文件版权声明；完整 LGPL 文本已由该 COPYING 提供 |
| `lib/tcc/lib/libtcc1.c` | GPL version 2 or later，附允许编译后链接组合的额外许可 | 保留文件中的额外许可和 [GPLv2 文本](LICENSES/GPL-2.0.txt)；不把它概括为无例外 GPL |
| `lib/tcc/include/stdatomic.h` | Apache-2.0 WITH LLVM-exception | 保留该文件的 SPDX 和来源声明，按实际交付内容附相应许可与例外文本 |
| `res/tcc/` 中的 Windows SDK 头文件 | 多数头文件声明来自 MinGW 并进入 Public Domain；具体文件可能另有声明 | 按文件来源保留，不能仅因放在 TCC SDK 目录就全部标为 GPL |

项目权利人已明确：TCC 衍生修改继续沿用原 LGPL，不主动改为普通 GPL。对 TCC 衍生文件的修改不能由根 MIT 文件覆盖；保留现有逐文件许可、版权和链接例外，参见 [TCC 本地修改许可说明](lib/tcc/XLANG-LICENSE-NOTICE.md)。

[TCC 官方许可说明](https://www.bellard.org/tcc/)确认其主体采用 LGPL。LGPL 2.1 第 3 节允许选择适用普通 GPL，但本项目没有作出该转换选择。xlang 原创源码采用 MIT，TCC 及其衍生修改遵守原许可；当前静态链接交付仍需满足 LGPL 的相关条件。

## 其他主要组件

| 范围 | 来源与许可依据 |
| --- | --- |
| `lib/xrt/`、`lib/xruntime/` | 原有 MIT 文件头及供应方版权声明；保留其原年份、权利人和声明 |
| `lib/lzma/` | 7-Zip/LZMA SDK 来源文件中的 Public Domain 声明；保留文件级说明 |
| 自动生成的扫描器／解析器文件中的第三方骨架 | 按对应生成器的许可和适用例外处理；根 MIT 适用于项目原创语法与逻辑，不替换骨架来源的许可 |
| `release/extlibs/` | 各包及其内嵌依赖的 LICENSE、COPYING、文件头和包内说明；根 MIT 不统一替换这些许可 |
| `ide/` 与发布目录中的第三方素材、工具、字体 | 各自已有版权、许可证、来源和交付清单；包括字体目录中的 LICENSE |

这份说明记录当前许可边界，不是一份已经验收完毕的所有第三方材料清单。打包时，应按实际载荷保留对应许可证、版权、例外、修改说明及适用的源码／重新链接材料。静态链接 LGPL 组件的交付还需满足相应重新链接条件，参见 [GNU 静态／动态链接说明](https://www.gnu.org/licenses/gpl-faq.html#LGPLStaticVsDynamic)。

根 MIT 授权不会自动改变第三方许可，也不能据此将整个带第三方依赖的二进制标成仅受 MIT 约束。
