# tictactoe - BUG 记录

工具：`tictactoe`（井字棋，计算机 vs 计算机，验证 class + minimax AI）

开发过程中发现 1 个高危编译器 BUG 与 1 个手感问题。BUG-T1 已在 `tictactoe.xl`
中绕过，工具可正常 `xl run` 与 `xl build`（均 EXIT=0，无需 companion `.c`）。

---

### BUG-T1: class 方法返回 typed 容器（array<int>）后，调用方使用即崩溃

- 工具：tictactoe
- 复现：
  ```xlang
  class B
      array<int> data
      create()
          self.data = array[0, 0, 0, 0]
      end
      func fill()
          self.data[0] = 1
          self.data[2] = 2
      end
      // 方法返回 typed 容器
      func empties() as array<int>
          array<int> result = array[]
          int i = 0
          while i < 4 do
              if self.data[i] == 0 then
                  result.push(i)
              end
              i += 1
          end
          return result
      end
  end

  B b = B()
  b.fill()
  auto e = b.empties()        // 编译通过
  println("len=", e.length)   // 运行期崩溃
  int k = 0
  while k < e.length do
      println("e[", k, "]=", e[k])   // 也会崩溃
      k += 1
  end
  ```
- 现象：
  - `auto e = b.empties()` 能通过编译，但运行期访问 `e.length` / `e[i]` 触发
    **访问违规 (0xC0000005 / EXIT=-1073741819)**。返回的容器内容已损坏（悬空指针式）。
  - 与之配套：类型化局部 `array<int> e = b.empties()` 在**编译期**就被拒：
    `[checked] 静态泛型容器目前只支持同类容器字面量初始化`。
    即 typed 局部写法直接报错，`auto` 写法放过编译却在运行期崩溃——
    两种写法都不可用，且 `auto` 形式极具误导性（看着正常，实则段错误）。
  - **关键不对称**：把同样的 `empties` 写成**顶层 function**（`func emptiesOf(B b) as array<int>`）
    则完全正常（`array<int> e = emptiesOf(b)` 后 `.length`/`[i]` 都正确）。
    即"function 返回 typed 容器" OK，"**method** 返回 typed 容器" 崩溃。
    这也与 `contacts` 工具一致——它的 `loadContacts(...) as array<Contact>` 是顶层 function，长期正常工作。
- 期望：class 方法返回 `array<T>`（及其它 typed 容器）应与顶层 function 行为一致；
  返回值在调用方应能正确访问；至少 `auto x = self.method()` 不应编译通过却在运行期段错误。
- 绕过（本工具采用）：**Board 不暴露任何返回 array 的方法**。
  - `randomMove()` / `minimax()` / `bestMove()` 全部直接在 `self.cells` 上**内联遍历 0..8**
    （计数+抽取 / make-undo），不经过 `emptyCells()`。
  - `explore()` 本可写成 Board 方法，但需要返回空格列表；改为**顶层 function**
    `func explore(Board b)`（function 返回 void，内部也内联遍历 0..8），
    顺带复用了"function 路径正常"这一事实。
  - 这样全文件没有任何"class 方法返回 typed 容器"的代码路径，BUG-T1 不再触发。
- 严重度：**高**（隐蔽；`auto` 形式能过编译，运行期才崩，且崩溃点在调用方而非方法内部，
  无对照写法时极难定位到"方法返回容器"这个根因。所有"想把空位列表/邻居列表/子节点列表
  从 class 方法返回"的工具都会踩到）

---

### NOTE-T2: `set` / `get` 是属性访问关键字，不能作为方法名

- 工具：tictactoe（开发中踩到）
- 复现：
  ```xlang
  class B
      func set(int i, int v)   // 或 func get(int i) as int
          ...
      end
  end
  ```
- 现象：`[parse] 语法错误`，报错位置指向方法名（如 `set`/`get` 的位置），
  而非清晰的"关键字冲突"。`set` / `get` 是属性访问器关键字（见
  `examples/oop_property_get_let.xl` 的 `get hp() as int`），用作普通方法名会触发解析失败。
- 期望：解析器对关键字冲突给出明确提示（如"`set` 是保留字，不能作方法名"）。
- 绕过：访问器叫 `cell(pos)`，清子叫 `unset(pos)`（不取 `set`/`get`/`clear` 等可能冲突的名字）。
- 严重度：低（易绕过，但报错信息不直观）

---

## 附：正向验证通过的特性（本工具确认可用）

- `class Board` + `array<int>` 字段：create 内 `array[0×9]` 字面量初始化、方法内
  `self.cells[i]` 读写（**含 if/elseif/while 控制流**）—— **完全可靠**。
  - 这正面确认 `examples/../projects/dev/sudoku/BUGS.md` 的 BUG-S1~S3
    （`print(self.method())` 读垃圾）是 **list<int> 专属**；`array<int>` 不受影响。
  - `render()`（方法体内 while+if，从 `self.cells` 读值拼字符串）+ `showBoard()`
    （方法内 `println(self.render())`）输出全部正确，无垃圾值。
- **方法返回自定义 class 类型**：`func clone() as Board` + `Board child = b.clone()` 正常
  （`explore` 据此克隆子局面，去重统计出正确的 5478）。
- **class 实例按值拷贝**：`Board c = b` 是独立副本（`c.place(...)` 不影响 `b`）；
  故必须用 `clone()` 取独立副本，不能靠别名。minimax 用 self 上 make/undo 规避拷贝。
- **顶层 function 返回 typed 容器**：正常（与 `contacts` 一致；本工具虽未用，smoke 已验证）。
- **顶层 function 读/写全局 `set<string>`**：`sig in visited` / `visited.add(sig)` /
  `visited.length` 全部正常（`explore` 据此去重）。
- **negamax（minimax）递归方法**：`self.minimax(opponent)` 最深约 9 层，
  空棋盘值 = 0（和棋），与博弈论结论一致；最优 AI 在 `smart`/`tournament` 中从不输。
- **`random.seed(uint64, uint64)` 固定种子**：`xl run` 与 built exe 输出逐字节一致（可复现）。
- **`xl build` 不再需要 companion `.c`**：BUG-19（argv 符号缺失）在当前 release 已修复；
  本工具仅一个 `.xl` 文件即通过 `xl build`（对比 `config/dupfind/psearch/sudoku` 仍带 shim）。
- `float` 运算 + `string(float)`：胜率百分比格式化正常（如 `81.2%`）。
- `func main(...) as int` + `arg`：退出码透传正确（有效命令 EXIT=0，无参/未知命令 EXIT=1）。
