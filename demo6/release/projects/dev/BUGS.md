# dev 工具群 BUG 与手感问题记录

格式见 `README.md` 末尾 "BUG 记录" 一节。

---

### BUG-S1: string.splitLines 把末尾换行当分隔符，多出尾部空串
- 工具：sorter
- 复现：`array a = string.splitLines("a\nb\n")`
- 现象：`a.length == 3`，元素为 `["a", "b", ""]`。`\r\n` 同理：`"a\r\nb\r\n"` → `["a","b",""]`。
- 期望：与 Python `str.splitlines()` / Unix 行工具一致，末尾换行作为"行终结符"而非"分隔符"，应得到 `["a","b"]` (2 行)。无末尾换行时 (`"a\nb"`) 当前行为正确 (2 行)。
- 绕过：在 `loadLines` 中，若结果非空且最后一个元素为 `""`，`remove(length-1)` 丢掉恰好一个尾部空串。该规则对 `"a\nb\n"`→2 行、`"a\nb"`→2 行、`""`→0 行、`"\n"`→1 行 (空行) 均符合行工具惯例。
- 严重度：中 (所有基于 `splitLines` 的行式工具都会踩到，不绕过会多吐/多算一行空行)

### BUG-G1: 带自定义 create(args) 的 class 实例不能直接装箱进 var
- 工具：geo
- 复现：
  ```xlang
  class Point
      float x = 0.0
      float y = 0.0
      create(float x, float y)
          self.x = x
          self.y = y
          return
      end
  end

  var pts = []
  pts.push(Point(1.0, 2.0))   // 编译期失败
  ```
- 现象：`<file>:<line>:<col>: [checked] class constructor with create/arguments cannot be boxed directly as var`，编译阶段直接失败，`xl run` / `xl build` 均无法通过。同样写法 `var p = Point(1.0, 2.0)` 也触发同一错误。
- 期望：普通（非 native-backed）class 实例应能装入 `var` 动态容器或 `var` 变量，以便把同类对象集合统一处理（例如多边形顶点列表）。当前对「带 create/arguments 的 class」做了保守拒绝，且错误信息未指引「改用 typed 局部变量 / typed `array<T>`」。
- 绕过：把对象字段拆成基础类型分别存进多个 `var` 动态数组；或把 class 实例保留在 typed（非 var）局部变量中直接使用。`geo` 的 `polygon` 子命令采用前者（坐标拆进 `xs` / `ys` 两个 `float` 动态数组）；`distance` / `angle` / `selftest` 用 typed `Point` 局部变量，完全正常。
- 严重度：中 (影响「想把若干 class 实例塞进动态容器」的工具；不绕过则无法编译)

### BUG-M1: int() 对非数字字符串静默返回 0 而不抛异常
- 工具：mdtoc
- 复现：
  ```xlang
  func main(...) as int
      var v = int("abc")
      println("int(abc)=", v)   // 输出 int(abc)= 0
      return 0
  end
  ```
- 现象：`int("abc")` 返回 `0` 且不抛异常，程序正常退出 EXIT=0。`int("2")`、`int("2.5")`（截断为 2）等正常；唯独纯非数字串被静默当成 0。
- 期望：对无法解析的字符串应抛运行时异常（类似 `int("abc")` 在多数语言里抛 `ValueError`），或至少提供一个可用的 `tryParse`/可判空返回。否则基于 `try/catch` 的命令行数值参数校验完全失效。
- 绕过：当前无法用 try/catch 捕获；mdtoc 对 `--min-level/--max-level` 的非法值不报错，而是被当成 0 再被边界归一化（钳到 1）。如需严格校验，只能自行逐字符 `string.chars` + `string.byteAt` 检查是否全为 ASCII 数字。
- 严重度：低 (mdtoc 不崩溃；但削弱了 CLI 参数校验。影响所有想把字符串数字安全转成 int 的工具)

### BUG-T1: Template.render() 方法返回类型未被标注为 string，无法内联字符串拼接
- 工具：tpl
- 复现：
  ```xlang
  func main(...) as int
      var data = {"title": "x"}
      Template tpl = template.compile("{$title}")
      string b = "cls-inline: " + tpl.render(data)   // [sem] 失败
      println(b)
      return 0
  end
  ```
- 现象：`xl run` 报 `<file>:<line>:<col>: [sem] 字符串拼接两侧都必须是 string`，编译期直接失败。对比：命名空间便捷函数 `template.render(text, data)` 返回值被正确标注为 `string`，`"x: " + template.render("{$title}", data)` 可正常拼接；`Template` 类的 `tpl.render(data)` 方法却不行。注意 `string r = tpl.render(data)`（赋值给 string 局部变量）是可以的——返回值能隐式转 string，只是 `+` 拼接的语义检查不认它为 string。
- 期望：`Template.render(data)` 与 `template.render(text, data)` 返回类型应一致地标为 `string`，允许 `"..." + tpl.render(data)` 内联拼接。当前该 native-backed 类方法的返回类型被当成 `var`/未知。
- 绕过：把方法返回值先赋给一个 `string` 局部变量再拼接（`string r = tpl.render(data); println("x: " + r)`），或用 `string(tpl.render(data))` 包一层。
- 严重度：低 (不影响功能，仅影响写法手感；命名空间 `template.render` 不受影响，可优先使用)

### BUG-B1: `~=` 是「近似相等」而非「不等于」，易被误用为不等判断
- 工具：bigint
- 复现：
  ```xlang
  string a = "0"
  string b = "0"
  if a ~= b then
      println("误判为不相等")   // 实际进入分支
  end
  ```
- 现象：`~=` 在 xlang 是 `string.approx` 的运算符形式（近似相等），不是 Python/Lua 风格的「不等于」。`"0" ~= "0"` 因近似相等成立，返回 `true`。本工具 `assertEq` 初版写成 `if got ~= want then FAIL`，导致 `assertEq("0","0")` 误报 `FAIL: got 0 want 0`。
- 期望：`~=` 语义与多数主流语言差异大，建议文档/错误信息显著提示「`~=` = approx，`!=` 才是不等」；或在字符串上下文对 `~=` 给出语义提示。
- 绕过：不相等一律用 `!=`（`!=` 才是严格不等）。本工具所有「不等」判断已改为 `!=`。
- 严重度：低 (不影响功能，纯属手感陷阱；但初学者极易踩坑，特别是从 Python/Lua 迁移时)

### BUG-C1: `file.readText` 不剥离 UTF-8 BOM
- 工具：crawl
- 复现：
  ```xlang
  // 准备一个 UTF-8 + BOM 的文件（PowerShell `Set-Content -Encoding UTF8` 或 .NET `UTF8Encoding(true)` 写出）：
  //   字节: EF BB BF 23 63 6F 6D 6D 65 6E 74 0A ...
  //   内容: "\uFEFF#comment\nhttp://x/y\n"
  func main(...) as int
      string text = file.readText("bom.txt")        // charset 默认 65001 (UTF-8)
      println(string.byteAt(text, 0))               // 239  (= 0xEF，BOM 第一字节还在)
      array lines = string.splitLines(text)
      println(string.startsWith(string(lines[0]), "#"))   // false —— 首行是 "\uFEFF#comment"
      return 0
  end
  ```
- 现象：`file.readText(path)`（默认 `charset=65001` UTF-8）读出来的字符串**保留** UTF-8 BOM（首 3 字节 `0xEF 0xBB 0xBF`，即 1 个 `U+FEFF` 字符）。BOM 粘连在首行头部，导致：(1) `string.startsWith(line, "#")` 之类的注释/前缀判断失效；(2) 把 `"\uFEFF#comment"` 当成一行内容/无效 URL 装进数据。
- 期望：`file.readText` 以 UTF-8 读取时应自动剥离起始 UTF-8 BOM（与 Python `open(...,encoding="utf-8-sig")` / 多数编辑器/工具一致）；或至少提供一个显式 `utf-8-sig`/`stripBom` 选项。`encoding` 常量里有 `encoding.bom`，但 `file.readText` 没有接它。
- 绕过：读完后用 `string.byteAt` 检测 `0xEF 0xBB 0xBF`，命中则 `string.slice(text, 1)` 去掉那 **1 个** BOM 字符（注意 `string.slice` 的 `start` 是字符下标，见 手感-C1；不是 `slice(text, 3)`）。crawl 的 `loadUrlsFromFile` 已用此绕过。
- 严重度：低 (只影响「读取被 Windows 工具写出、带 UTF-8 BOM 的文本文件」的工具；不绕过会污染首行)

### 手感-C1: `string.slice` 的 `start` 是字符下标，与 `string.byteAt` 的字节下标不一致
- 工具：crawl
- 复现：
  ```xlang
  string s = "\uFEFF#comment"          // 1 个 BOM 字符 + "#comment"，byteLen = 11
  string r = string.slice(s, 3)        // 想去掉 3 个 BOM 字节
  println(string.byteLen(r))           // 6 —— 实际只剩 "mment"，删了 5 字节
  // string.slice(s, 1) 才是正确去掉 BOM 的写法（去掉 1 个字符 = 3 字节）
  ```
- 现象：`string.slice(text, start, count=-1)` 的 `start` 按 **Unicode 字符**计数，而 `string.byteAt(text, i)` / `string.byteLen(text)` 按 **UTF-8 字节**计数。两套下标混在同一 namespace 的字符串 API 里，处理多字节字符（含 BOM）时极易错位：用 `byteAt` 检测到 BOM 是 3 字节后，自然想用 `slice(text, 3)` 去掉，结果多删了 2 个字符。
- 期望：文档/IDE 显著标注 `slice/mid/charAt/len` 是字符维度，`byteAt/byteLen/bytes` 是字节维度；或提供成对的 byte-indexed 切片函数（如 `string.sliceBytes`）。
- 绕过：明确用 `slice(text, 1)` 去掉单个 BOM 字符；多字节计算时先 `string.chars` 转字符数组。
- 严重度：低 (不影响功能；纯手感/认知陷阱，但写 UTF-8 处理代码时几乎必踩)

### 手感-C2: `string.trim` 不剥离 U+FEFF BOM 字符
- 工具：crawl
- 复现：
  ```xlang
  string s = "\uFEFF#comment"
  println(string.startsWith(string.trim(s), "#"))   // false
  ```
- 现象：`string.trim(text)` 不把 `U+FEFF`（ZERO WIDTH NO-BREAK SPACE / BOM）当作可裁剪空白，首部的 BOM 在 trim 后仍然保留。
- 期望：`string.trim` 默认裁剪集合里包含 `U+FEFF`，或文档说明它只裁剪 ASCII 空白。
- 绕过：先按 BUG-C1 的方式显式去掉 BOM，再 trim。
- 严重度：低 (与 BUG-C1 联动；单独影响很小)

### BUG-C2: `file.readText` 对不存在文件静默返回 "" 而不抛异常
- 工具：crawl
- 复现：
  ```xlang
  func main(...) as int
      bool threw = false
      string text
      try
          text = file.readText("definitely_missing_file.xyz")
      catch e
          threw = true
      end
      println(threw)                          // false —— 没抛
      println(file.exists("..._missing..."))  // false
      println(string.byteLen(text))           // 0 —— 静默返回空串
      return 0
  end
  ```
- 现象：`file.readText(不存在的路径)` 不抛异常，静默返回 `""`。与 BUG-M1（`int("abc")` 静默返回 0）同属「读路径对无效输入静默失败」一族，导致无法用 try/catch 区分「文件不存在」与「文件存在但为空」。
- 期望：读文件失败（路径不存在 / 无权限 / 编码错误）应抛运行时异常，让 try/catch 可用；或至少提供单独的 `file.tryReadText`。
- 绕过：调用 `file.readText` 前先 `file.exists(path)` 判定；用 `file.info(path)` 可进一步区分文件/目录/权限。crawl 的 `loadUrlsFromFile` 已用此绕过并区分「不存在 / 读失败 / 无有效 URL」三种返回。
- 严重度：低 (不绕过只是错误提示不精确；不影响核心功能)

### 手感-C3: `catch e` 的异常变量不能传给 `print`/`println` 可变参数
- 工具：crawl
- 复现：
  ```xlang
  try
      ...
  catch e
      println("caught:", e)   // [checked] 当前 C 生成暂不支持将该表达式打包为 var
  end
  ```
- 现象：catch 子句的异常变量 `e`（动态异常对象）传给 `print`/`println` 等 `...` 可变参数函数时，codegen 报 `当前 C 生成暂不支持将该表达式打包为 var`，编译期失败。不使用 `e`（如 `catch e ... ok = false ... end`）则正常。
- 期望：异常对象能像普通值一样传入 varadic 输出函数，便于直接 `println("err:", e)`。
- 绕过：catch 内不要把 `e` 传给 varadic；改用 `f.error()`（对 future 失败）或在 catch 内只设标志位，把错误信息另外取。
- 严重度：低 (仅影响「想在 catch 里直接打印异常」的写法；不影响功能)

### BUG-F1: class 方法体内「裸字段名」作方法/索引接收方或赋值目标不被解析为 self.field，须显式写 self.
- 工具：fwatch
- 复现（最小）：
  ```xlang
  class C
      array<string> a = array[]
      func push(string v)
          a.append(v)          // [checked] 找不到 class 方法
      end
  end
  func main(...) as int
      C x = C()
      x.push("z")
      println(x.a.length)
      return 0
  end
  ```
- 现象：在 class 方法体里直接用裸字段名 `a` 当**接收方**或**赋值目标**时，编译期（`xl run` / `xl build` 的实际编译阶段，并非 `xl check`）一律失败，错误信息随用法不同：
  - `a.append(v)`、`a.remove(...)` 等方法调用 → `[checked] 找不到 class 方法`（定位到方法名）。
  - `a.length`（属性形式）→ `[checked] class field receiver is not a class value`。
  - `a[0]`（索引读）/ `a[i] = v`（索引写）→ `[checked] index receiver must be array, list, dict or var`。
  - `a = local`（整字段赋值）→ `[sem] 赋值目标未声明`。
  
  把同一个字段改成 `self.a.xxx` / `self.a[i]` / `self.a = local` 则**全部正常**。验证用例（r7a/r7c/r8）：`self.a.append`、`self.a.length`、`self.a[i]` 读、`self.a[i]=v` 写均通过 `xl run` 与 `xl build`，EXIT=0。
- 期望：方法体内裸字段名应等价解析为 `self.<field>`（多数带 class 的语言默认行为）；或至少在 `xl check` 阶段就报清楚错并提示「请写 self.」。当前既不解析为 self，`xl check` 又静默通过（见下条），非常误导。
- 附带：**`xl check` 与 `xl run`/`xl build` 不一致**。对同一份会触发本 BUG 的源码，`xl check` 返回 `ok` (EXIT=0)，而 `xl run` 必现失败 (EXIT=1，6/6 次)。即 check 的语义分析没发现该问题，只有真正下到 cgen/编译阶段才报错。
- 绕过：class 方法体内**一律**用显式 `self.<field>` 访问容器字段（读/写/索引/方法调用全都要）。fwatch 的 `Snap` 类（`self.paths.append` / `self.paths[i]` / `self.paths.length`）即用此绕过，`xl run` + `xl build` 双路径 EXIT=0。需要把对象集合传进/传出函数时仍可正常返回/接收 class 实例（`func buildSnap(string) as Snap` 已验证）。
- 严重度：中（写带状态 class 的工具几乎必踩；不绕过则 `xl run`/`xl build` 直接编译失败；且 `xl check` 绿灯让人误以为没问题）

### BUG-K1: var dict 迭代/json.stringify 键序不保插入序（常见键表现为逆序）
- 工具：kvdb
- 复现：
  ```xlang
  var d = {}
  d["user:1"] = "alice"
  d["count"]  = 5
  d["pi"]     = 3.14
  d["flag"]   = true
  d["msg"]    = "hello"
  string out = ""
  for key, item in d
      out = out + string(key) + " "
  end
  println(out)                                  // -> "msg count pi flag user:1 "
  println(json.stringify(d))                    // -> {"msg":...,"count":...,"pi":...,"flag":...,"user:1":...}

  var p = json.parse('{"user:1":"alice","count":5,"pi":3.14,"flag":true,"msg":"hello"}')
  println(json.stringify(p))                    // -> {"msg":"hello","count":5,...,"user:1":"alice"}  键序被反过来
  ```
- 现象：`var` dict（`dict{...}` 显式构造、`d[key]=val` 逐个插入、以及 `json.parse` 还原）的迭代顺序与插入/文档顺序不一致。对上面这组真实键，迭代与 compact `json.stringify` 输出的都是 `msg,count,pi,flag,user:1`，即插入序 `user:1,count,pi,flag,msg` 的**逆序**。对单字符键 `a,b,c,d,e` 得到 `b,a,d,c,e`（按内部哈希桶序）。`dict<int>` 显式构造同理。
- 期望：dev/README「已修复 BUG」与「仍需注意的限制」中明确写着 `json.parse compact 模式保留键序 ✅`、`json key order in compact stringify ✅`，且「仍需注意」只点名 pretty 模式可能重排。但实测 compact 模式下 `json.parse → json.stringify` 会把 JSON 文档键序反过来，与文档描述不符。期望 var dict 至少在 compact 模式下保留插入/文档键序。
- 绕过：dict 本身语义上无序，kvdb 功能正确性不受影响，仅 `list`/`keys`/`export` 的展示顺序与写入顺序不同。若强需固定顺序，可另存一个 `array<string>` 键序索引并在展示时按它遍历。
- 严重度：中 (所有依赖 dict 键序的工具——配置表、清单生成、有序 KV——都会踩到；compact 保序是文档承诺的能力，当前对 var dict 不成立)


