# sudoku - 9x9 数独求解器（经典回溯算法）

用 `class Sudoku`（`list<int>` 长度 81，`idx=row*9+col`）+ 递归回溯求解 9x9 数独。
输入 81 位数字串（0=空格），输出格式化 9x9 网格并校验解的合法性。

## 用法

```bat
cd /d D:\GIT\x-lang\demo6\release

xl run projects\dev\sudoku\sudoku.xl -- "530070000600195000098000060800060003400803001700020006060000280000419005000080079"
xl run projects\dev\sudoku\sudoku.xl -- solve projects\dev\sudoku\puzzle1.txt
```

两种调用：
- `sudoku "<81位数字串>"` —— 直接传谜题串
- `sudoku solve <input_file>` —— 从文件读 81 位数字串（自动去除空白/换行）

输出示例：
```
Puzzle loaded: 30 givens, 51 empty

Solved (verified valid):

5 3 4 | 6 7 8 | 9 1 2
6 7 2 | 1 9 5 | 3 4 8
1 9 8 | 3 4 2 | 5 6 7
------+-------+------
8 5 9 | 7 6 1 | 4 2 3
4 2 6 | 8 5 3 | 7 9 1
7 1 3 | 9 2 4 | 8 5 6
------+-------+------
9 6 1 | 5 3 7 | 2 8 4
2 8 7 | 4 1 9 | 6 3 5
3 4 5 | 2 8 6 | 1 7 9
```

## 设计要点

- **数据结构**：`class Sudoku` 持有 `list<int> data`（81 元素），`idx = row*9+col`。
- **算法**：经典回溯——递归方法 `solve(pos)` 从位置 0 扫到 81，遇空格依次试填 1..9，
  `isValid(pos,num)` 检查行/列/3x3 宫，失败回溯。最深约 60 层，"世界最难"数独瞬时解出。
- **校验**：`verify()` 对完整解逐格做冲突复查，确认是合法解。
- **输入解析**：`string.charAt` + `int(ch)` 逐字符转数字；文件输入用 `string.filter` 去空白。

## 绕过的 BUG（详见 BUGS.md）

| 编号 | 问题 | 绕过 |
|---|---|---|
| B6 | try/catch 内不能 return | 参数校验放 try 外 |
| B11 | 方法内 typed 读 list 字段返垃圾 | peek 助手 `var v=self.data[i]; return int(v)` |
| B12/B13/B19 | 不把容器/native class 传函数 | 回溯逻辑内联到递归方法 |
| BUG-S1 | peek 返回值传 print 变垃圾（同体 debugPeek 不变） | 打印路径用 debugPeek |
| BUG-S2 | 方法体内有 if 会让 print(self.X()) 读垃圾 | printRow 完全展开、无 if/循环 |
| BUG-S3 | class 方法间接调用 print(self.X()) 读垃圾 | printRow 只从顶层 main 调用 |
| BUG-S4 | `T x` 不触发无参 create() | 统一写 `Sudoku g = Sudoku()` |
| BUG-19 | xl build 缺 argv 符号 | companion `argv_runtime.c` shim（见下） |

## build 路径（BUG-19 绕过：companion .c shim）

`xl build` 对所有用到 `arg` 的程序链接失败（**BUG-19**，已由 config/dupfind/psearch 工具记录）：
生成代码调用 `xl6_command_argv_to_arg`，但 cgen 的 `xl6CgenEmitArgvArrayHelper` 是死代码
（定义了却未被管线调用），独立 exe 链接时该符号未解析。`xl run` 靠 xl.exe 进程内符号救场。

绕过：同目录放 `argv_runtime.c`（从 `../config/argv_runtime.c` 复制，用 XRT C API 提供
该符号的外部定义，含 Windows CommandLineToArgvW UTF-8 路径，顺带缓解 B5），build 时
显式列为参数并加链接项：

```bat
xl build projects\dev\sudoku\sudoku.xl ^
    projects\dev\sudoku\argv_runtime.c ^
    -o projects\dev\sudoku\sudoku.exe ^
    --link D:\GIT\x-lang\demo6\build\tmp\xrt.o ^
    --link D:\GIT\x-lang\demo6\res\tcc\win64\x86_64\lib\libtcc1.a ^
    --link Ws2_32 --link IPHLPAPI --link Shell32 ^
    --lib-path D:\GIT\x-lang\demo6\res\tcc\win64\x86_64\lib
```

built exe 经全命令验证与 `xl run` 输出一致。

## 注意：编译器会改写 include/xl_runtime.h

本工具开发中观察到：`xl`（run/build）在某些 codegen 路径上会**改写自身的
`include/xl_runtime.h`**（git diff 显示 ~1580 行变化）。正常情况下是良性重生成（功能正常）；
但若被处理的源码触发 **C 编译期错误**（如本工具调试时的一些写法），该重生成可能中途
中断，留下 `return ;` / `result = ;` 等空表达式，导致**之后所有程序都无法编译**。
发生时用 `git restore include/xl_runtime.h` 恢复即可。详见 BUGS.md "附记"。

## 验证

- 经典题（30 givens）：run + build 两条路径均输出唯一正确解并通过 verify。
- "世界最难"题（Arto Inkala，21 givens）：瞬时解出，正确。
- 已完成盘面（0 empty）：原样返回并通过 verify。
