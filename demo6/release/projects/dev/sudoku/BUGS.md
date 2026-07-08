# sudoku - BUG 记录

工具：`sudoku`（9x9 数独求解器）

开发过程中发现 4 个 xlang 编译器 BUG（demo6 release）。前 3 个围绕 **class 方法返回 int 后传入 print 的值被破坏**，第 4 个是无参构造未触发。均已在 `sudoku.xl` 中绕过，工具可正常 `xl run` 与 `xl build`。

---

### BUG-S1: 同体方法名不同，返回值传入 print 的正确性不同

- 工具：sudoku
- 复现：class 内定义两个方法体完全相同、仅名字不同的方法：
  ```xlang
  func peek(int i) as int
      var v = self.data[i]
      return int(v)
  end
  func debugPeek(int i) as int
      var v = self.data[i]
      return int(v)
  end
  ```
  `self.data` 为 `list<int>`，已正确写入 1..9。
- 现象：
  - `self.peek(i) == num`（用于比较）→ **正确**（isValid/solve/countEmpty 全部依赖它，求解结果正确）。
  - `print(self.peek(i))` 或 `string(self.peek(i))` → **垃圾值**（类似 39672984 的指针式整数）。
  - `print(self.debugPeek(i))` → **正确**（值 1..9）。
  - 即：两个同体方法，一个的返回值能被 print 正确消费，另一个不能。
- 期望：同体方法返回值在所有消费场景下行为一致。
- 绕过：打印路径（`printRow`）一律改用 `debugPeek`；比较路径继续用 `peek`。
- 严重度：高（极隐蔽；若无对照方法几乎无法定位）。

---

### BUG-S2: 方法体内出现 if/elseif 会让 print(self.method()) 读到垃圾值

- 工具：sudoku
- 复现：一个 class 方法内同时存在 `if`/`elseif` 控制流 与 `print(self.peek(...))`：
  ```xlang
  func printRow(int r)
      int c = 0
      while c < 9 do
          int idx = r * 9 + c
          print(self.debugPeek(idx))   // 即使是 debugPeek 也救不了
          if c == 2 or c == 5 then      // <-- 这条 if 一出现，上面的 print 就变垃圾
              print(" | ")
          elseif c < 8 then
              print(" ")
          end
          c += 1
      end
      println()
  end
  ```
- 现象：只要方法体里有任何 `if`，`print(self.X(...))` 的返回值就被读成垃圾；把 if 删掉（仅留 while + print）即恢复正常。把分隔符逻辑移到 print 之前、或在 print 与 if 之间插无参 print 均无效——只要同方法体内有 if 就坏。
- 期望：控制流不应破坏已有的方法返回值。
- 绕过：`printRow` 完全展开（unroll），无 if、无循环，仅交替 `print(self.debugPeek(var))` / `print(字面量)`。
- 严重度：高。

---

### BUG-S3: class 方法间接调用（A 调 B，B 内 print(self.method())）读到垃圾值

- 工具：sudoku
- 复现：
  ```xlang
  func printRow(int r) ... print(self.debugPeek(...)) ... end
  func display()
      int r = 0
      while r < 9 do
          self.printRow(r)   // <-- 从另一个方法调用 printRow
          ...
      end
  end
  ```
  `printRow` 从顶层 `main` 直接调用 `g.printRow(r)` → **正确**；
  `printRow` 从 `display()` 内 `self.printRow(r)` 调用 → **垃圾值**。
- 现象：一个"会 print(self.method())"的方法，被顶层直接调用时正常，被另一个 class 方法间接调用时其内部 print 读到垃圾。
- 期望：调用来源不应影响方法内部行为。
- 绕过：取消 `display()` 方法；改为在顶层 `main` 里写 while 循环直接调用 `g.printRow(rr)`，由 main 负责打印水平分隔线。
- 严重度：高。

---

### BUG-S4: `T x`（无括号声明）不调用无参 create()

- 工具：sudoku
- 复现：
  ```xlang
  class Probe
      list<int> data
      create()
          self.data = list[0: 0]
          ... // 扩容到 81
      end
  end
  Probe pa          // 无括号
  println(pa.data.length)   // 0  -> create() 未执行
  Probe pb = Probe()        // 有括号
  println(pb.data.length)   // 81 -> create() 已执行
  ```
- 现象：`T x`（无括号）不触发无参 `create()`，字段保持默认/未初始化；`T x = T()` 才会触发。
- 期望：`T x` 与 `T x = T()` 等价（均调用无参 create），或在语法上拒绝 `T x`（要求显式 `T()`）。
- 绕过：所有实例化统一写 `Sudoku g = Sudoku()`。
- 严重度：中（极易踩坑，但有明确的写法绕过）。

---

## 附记：编译器会改写自身的 include/xl_runtime.h（运行时头文件非只读）

- 工具：sudoku（开发中观察到，影响整个 release）
- 现象：`xl`（run/build）在某些 codegen 路径上会**覆写 `demo6/release/include/xl_runtime.h`**
  （`git diff` 显示约 1580 行新增 / 778 行删除）。正常情况下是良性重生成——头文件仍可正常编译。
- 严重并发症：若被处理的源码触发 **C 编译期错误**（本工具调试 `bugs_repro.xl` 时复现），
  该重生成会**中途中断**，留下 `return ;`、`result = ;` 等空表达式（见 xl_runtime.h 第
  784/789/792... 行）。此时**之后所有 xlang 程序都无法编译**，报
  `tcc: ... xl_runtime.h:NNN: error: expression expected before ';'`。
- 恢复：`git restore include/xl_runtime.h`（从已提交版本还原）。还原后 `xl run` 立即恢复；
  `xl build` 因 BUG-19 仍需 companion shim。
- 期望：编译器不应改写自带的运行时头文件；或至少在 codegen 失败时不留下半成品。
- 严重度：高（一旦触发，整个 release 编译能力瘫痪，且不易定位到是这个文件）。

---

## 附：build 路径命中已知 BUG-19（与 config/dupfind/psearch 同范式）

`xl build projects\dev\sudoku\sudoku.xl -o sudoku.exe` 链接失败：
`unresolved reference to 'xl6_command_argv_to_arg'`。根因是 cgen 的
`xl6CgenEmitArgvArrayHelper` 是死代码（详见 `../config/README.md`）。
绕过：同目录 `argv_runtime.c` shim + `--link` 标准链接项（见本工具 README.md 的 build 命令）。

---

## 附：正向验证通过的特性（本工具确认可用）

- 经典回溯（递归方法 `solve(pos)`，最深约 60 层）求解正确且迅速（"世界最难"数独瞬时解出）。
- `list<int>` 作为 class 字段：create 内循环写入、顶层/方法内 peek 读取（`var v=self.data[i]; return int(v)`）—— **用于 == 比较时完全可靠**。
- `string.charAt` + `int(ch)` 解析数字串。
- `string.filter` 去除文件中的空白/换行。
- `file.readText` / `file.exists`。
- class 递归方法、`while` 循环、`try/catch`（无 return 穿越边界）。
- `xl run` 与 `xl build` 两条路径输出一致。
