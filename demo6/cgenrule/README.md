# xlang demo6 cgen 规则对照表

本目录记录 demo6 当前代码生成器的生成规则。行号以 `demo6/src/cgen` 下各分片文件自身行号为准，不以 `xl6_cgen.c` 合并后的翻译单元行号为准。

## 文件说明

- `_function_index.generated.md`：机器扫描生成的 `xl6Cgen*` 函数索引，用来防止遗漏 cgen 分片中的函数。
- `00_文件覆盖索引.md`：按文件列出 cgen 分片职责，并标注哪些文件直接生成 C、哪些文件只做辅助决策。
- `01_入口与声明规则.md`：整体生成流程、函数、extern/export、main、class/custom/union 等规则。
- `02_表达式规则.md`：字面量、名称、调用、成员、索引、运算符、容器等表达式规则。
- `03_语句与控制流规则.md`：声明、赋值、return、if/while/for、异常、yield 等语句规则。
- `04_运行时与高级规则.md`：function 值、闭包、xcall、async、thread、channel、generator、标准库运行时注入等规则。
- `05_继续细化清单.md`：当前已细化区域、剩余高优先级拆解目标和验收推进方式。
- `06_P0_参数与动态值细则.md`：参数、命名参数、默认参数、`...`/`***`、xvalue 装箱、动态运算细则。
- `07_P0_索引与赋值细则.md`：索引读取/写入、typed container 赋值、动态成员赋值、复合赋值细则。
- `08_P0_控制流与循环细则.md`：if/while/do、range、动态容器遍历、typed container 遍历、generator 遍历、break/continue 清理细则。
- `09_P0_函数调用与callable细则.md`：普通调用、重载/泛型、function/var 调用、匿名函数、XCall、wrapper、签名和多返回值细则。

## 对照表字段

每条规则固定包含：

- xlang 写法
- 生成规则描述
- 生成器文件名和行号
- 对应生成的 C 代码
- 中间形态伪代码
- 实现同等功能手写的 C 代码
- 额外依赖的封装代码

## 覆盖口径

这里的“规则”指会改变生成 C 形态的分支，不把纯字符串编码、哈希、数组扩容、上下文清理这类内部工具函数单独当成语言规则。辅助函数仍在 `_function_index.generated.md` 和 `00_文件覆盖索引.md` 中保留索引。

当前已经完成主干规则与 P0 细化的第一轮覆盖，目录内已有 193 条 `CGEN-*` 规则。后续继续逐个文件补充更细的子分支，例如 async/channel/thread/generator 内部状态机、class/custom 生命周期、标准库 wrapper、header/XAPI 输出和 needs 扫描触发条件。
